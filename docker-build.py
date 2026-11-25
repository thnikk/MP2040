#!/usr/bin/env python3
"""Build MP2040 firmware via Docker.

Runs the CMake build inside the gp2040-ce-builder Docker image (no local ARM
toolchain needed) and optionally flashes a connected board. Pipeline:

    nuke → ensure builder image → clean → fetch tags → build → flash

Examples:
    python3 docker-build.py -b 2k
    python3 docker-build.py -b MacroPad --nuke --flash
    python3 docker-build.py -b 2k --clean --verbose --output build.log
    python3 docker-build.py -b 2k --skip --flash
"""
import argparse
import glob
import os
import re
import shutil
import struct
import subprocess
import sys
import time
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parent
DEFAULT_IMAGE = "mp2040-builder"
DEFAULT_FLASH_PATH = os.path.expandvars("/run/media/$USER/RPI-RP2")
NUKE_FILE = REPO_ROOT / "tools" / "flash_nuke.uf2"


#  Board / filesystem helpers

def get_valid_boards():
    """Board config dirs: configs/<Board>/BoardConfig.h."""
    configs = REPO_ROOT / "configs"
    return sorted(
        d.name for d in configs.iterdir()
        if d.is_dir() and (d / "BoardConfig.h").exists()
    )


def board_uses_rp2350(board):
    """True when the board's optional board.cmake selects an RP2350 platform.

    Used only to avoid flashing the bundled RP2040 nuke image to an RP2350
    board, which the bootrom rejects.
    """
    path = REPO_ROOT / "configs" / board / "board.cmake"
    try:
        return path.exists() and "rp2350" in path.read_text().lower()
    except OSError:
        return False


FLASH_MOUNT_NAMES = ("RPI-RP2", "RP2350")


def is_flash_mount(path):
    """True for a BOOTSEL drive (either chip name, or a custom mount with a
    UF2 info file)."""
    if not path.is_dir():
        return False
    return path.name in FLASH_MOUNT_NAMES or (path / "INFO_UF2.TXT").is_file()


def resolve_flash_path(path_str):
    """Expand $VAR references and find the current BOOTSEL mount.

    The RP2350 BOOTSEL drive can appear as "RP2350" instead of "RPI-RP2"
    (some boards/OTP configs keep the RP2040 name). Accept either an exact
    mount, a mount that changed names, or a parent directory to search.
    """
    path = Path(os.path.expandvars(path_str))

    # An exact/custom mount wins.
    if is_flash_mount(path):
        return path

    # A parent directory (e.g. -p /run/media/$USER): look inside it. If it is
    # just an ordinary directory, keep polling under the default name instead
    # of treating the parent as the drive.
    if path.is_dir():
        for name in FLASH_MOUNT_NAMES:
            candidate = path / name
            if is_flash_mount(candidate):
                return candidate
        return path / FLASH_MOUNT_NAMES[0]

    # The usual case: the configured path is stale/missing because the drive
    # mounted under the other chip's name.
    for name in FLASH_MOUNT_NAMES:
        candidate = path.parent / name
        if is_flash_mount(candidate):
            return candidate
    return path


def wait_for_flash_dir(args):
    """Poll until a BOOTSEL mount appears, resolving either chip's name.

    Resolution happens on every poll (not once at startup), so a board that is
    plugged in after the build starts is still found.
    """
    deadline = time.monotonic() + max(0, args.timeout)
    while True:
        path = resolve_flash_path(args.path)
        if path.is_dir():
            return path
        if time.monotonic() >= deadline:
            return None
        time.sleep(1)


def log_msg(msg, log_file=None):
    """Print to the terminal and append to the --output log file if given."""
    print(msg)
    if log_file:
        with open(log_file, "a") as f:
            f.write(msg + "\n")


#  Docker helpers

def image_exists(image):
    """True if the builder image is already present locally."""
    try:
        subprocess.run(["docker", "image", "inspect", image],
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                       check=True)
        return True
    except (subprocess.CalledProcessError, OSError):
        return False


def build_image(image, log_file=None):
    """Build the Docker builder image (Dockerfile at repo root)."""
    cmd = ["docker", "build", "-t", image, "."]
    with subprocess.Popen(
        cmd, cwd=REPO_ROOT, stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT, text=True
    ) as proc:
        for line in proc.stdout:
            if log_file:
                with open(log_file, "a") as f:
                    f.write(line)
            print(line, end="", flush=True)
        proc.wait()
        return proc.returncode


def run_docker(image, command, extra_args=None, log_file=None, verbose=False):
    """Run a command in the builder container, streaming its output.

    Every line is written to --output when given. On the terminal, non-verbose
    mode collapses CMake's "[N%]" lines into a single-line progress bar while
    still printing error lines; --verbose prints everything.
    """
    cmd = [
        "docker", "run", "--rm",
        "-v", f"{REPO_ROOT}:/build",
    ]
    if extra_args:
        cmd.extend(extra_args)
    cmd.extend([image, "bash", "-c", command])

    log_fh = None
    if log_file:
        log_fh = open(log_file, "a")

    try:
        with subprocess.Popen(
            cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True
        ) as proc:
            _build_started = False
            _output_shown = False

            for line in proc.stdout:
                if log_fh:
                    log_fh.write(line)
                if verbose:
                    _output_shown = True
                    print(line, end="", flush=True)
                else:
                    m = re.match(r'^\[(\s*\d+)%\]', line)
                    if m:
                        _output_shown = True
                        if not _build_started:
                            _build_started = True
                        pct = int(m.group(1))
                        bar = '\u2588' * (40 * pct // 100) + '\u2591' * (40 - 40 * pct // 100)
                        print(f'\r  Building: |{bar}| {pct:3d}%', end='', flush=True)
                    elif re.search(r'error:', line, re.IGNORECASE):
                        _output_shown = True
                        if _build_started:
                            print()
                        print(line, end='', flush=True)
            proc.wait()
            if not verbose and _output_shown:
                print()
            return proc.returncode
    finally:
        if log_fh:
            log_fh.close()


#  Build pipeline steps

def nuke_flash(args):
    """Flash the nuke UF2 to wipe flash, or warn."""
    if not args.nuke:
        return
    if board_uses_rp2350(args.board):
        log_msg("RP2350 board: the bundled nuke image targets RP2040, skipping "
                "--nuke. Hold BOOT while resetting and use an RP2350 flash nuke "
                "(or picotool erase) if a full wipe is needed.", args.output)
        return
    if not NUKE_FILE.exists():
        log_msg(f"Warning: nuke file not found at {NUKE_FILE}, skipping",
                args.output)
        return
    log_msg(f"Waiting {args.timeout}s for a BOOTSEL mount...", args.output)
    flash_dir = wait_for_flash_dir(args)
    if flash_dir is None:
        log_msg(f"Warning: no BOOTSEL mount found after {args.timeout}s, "
                f"skipping nuke", args.output)
        return
    dst = flash_dir / "flash_nuke.uf2"
    log_msg(f"Nuking board: {NUKE_FILE} -> {dst}", args.output)
    try:
        shutil.copy2(NUKE_FILE, dst)
        log_msg("Nuke sent", args.output)
    except Exception as e:
        log_msg(f"Warning: nuke failed: {e}", args.output)


def ensure_builder_image(args):
    """Build the Docker image if it's missing or --rebuild was given."""
    if args.rebuild or not image_exists(args.image):
        log_msg("Building Docker image...", args.output)
        ret = build_image(args.image, args.output)
        if ret != 0:
            sys.exit(ret)


def clean_build_dir(args):
    """Fix file ownership in the mounted repo and optionally wipe the build dir.

    The build container runs as root, so its output files land owned by root;
    the chown hands them back to the host user. --clean also removes the build
    dir for a from-scratch build.
    """
    log_msg("Cleaning...", args.output)
    cleanup_cmd = (
        'chown -R 1000:1000 '
        '/build/.git/modules /build/lib/pico_pio_usb /build/lib/tinyusb '
        '/build/www /build/build 2>/dev/null || true'
    )
    if args.clean:
        cleanup_cmd += '; rm -rf /build/build 2>/dev/null || true'
    run_docker(args.image, cleanup_cmd,
               extra_args=["--user", "0:0"],
               log_file=args.output, verbose=args.verbose)


def fetch_tags(args):
    """Fetch tags from origin so git describe can version the build."""
    log_msg("Fetching tags...", args.output)
    try:
        subprocess.run(["git", "fetch", "--tags"], cwd=REPO_ROOT,
                       check=True, stdout=subprocess.DEVNULL,
                       stderr=subprocess.DEVNULL)
    except (subprocess.CalledProcessError, OSError):
        log_msg("Warning: could not fetch tags, using fallback version",
                args.output)


# One-time boot flag baked into the flashed UF2 to boot once into web config
# mode. Must match BOOT_FLAG_ADDR / BOOT_FLAG_MAGIC in src/system.cpp.
BOOT_FLAG_ADDR = 0x101EF000
BOOT_FLAG_MAGIC = 0x57424743
UF2_MAGIC0 = 0x0A324655
UF2_MAGIC1 = 0x9E5D5157
UF2_MAGIC_END = 0x0AB16F30
UF2_FLAGS = 0x2000


def mark_webconfig_uf2(uf2_path):
    """Return a patched UF2 that boots once into web config mode.

    Appends one 256-byte page block at BOOT_FLAG_ADDR containing the magic
    dword; the firmware reads it on first boot, enters web config mode, and
    erases the sector. All blocks get their block count bumped so the file
    remains a valid UF2.
    """
    data = uf2_path.read_bytes()
    if len(data) % 512:
        raise ValueError(f"{uf2_path} is not a valid UF2 (size not a multiple of 512)")
    num_blocks = len(data) // 512
    blocks = [data[i * 512:(i + 1) * 512] for i in range(num_blocks)]

    # Every block must end with the magic_end field or the bootrom rejects it
    # and never flashes (and never reboots).
    for i, blk in enumerate(blocks):
        if struct.unpack('<I', blk[508:512])[0] != UF2_MAGIC_END:
            raise ValueError(f"{uf2_path} block {i} missing magic_end field")

    # Use the family the linker/picotool stamped into the built UF2 (RP2040,
    # RP2350 arm-s, ...). Hardcoding RP2040 makes the bootrom reject the
    # appended block on RP2350 boards.
    family = struct.unpack('<I', blocks[0][28:32])[0]

    # Page payload: magic dword + erased (0xFF) fill for the rest of the page.
    payload = bytearray(b'\xff' * 256)
    payload[0:4] = struct.pack('<I', BOOT_FLAG_MAGIC)
    marker = struct.pack('<8I', UF2_MAGIC0, UF2_MAGIC1, UF2_FLAGS,
                         BOOT_FLAG_ADDR, 256, num_blocks, num_blocks + 1,
                         family) + bytes(payload) + bytes(220) \
        + struct.pack('<I', UF2_MAGIC_END)

    out = []
    for blk in blocks:
        header = list(struct.unpack('<8I', blk[:32]))
        header[6] = num_blocks + 1  # new total block count
        out.append(struct.pack('<8I', *header) + blk[32:])
    out.append(marker)

    patched = uf2_path.with_name(uf2_path.stem + "_webconfig.uf2")
    patched.write_bytes(b"".join(out))
    return patched


def find_existing_uf2(board):
    """Return the most recently built UF2 for `board`, or None."""
    # Prefer the flat build/ artifact (the documented output path); per-board
    # CMake trees are only a fallback for builds that predate the copy step.
    flat = glob.glob(str(REPO_ROOT / "build" / f"MP2040_*_{board}.uf2"))
    if flat:
        flat.sort(key=os.path.getmtime)
        return Path(flat[-1])
    matches = glob.glob(str(REPO_ROOT / "build" / "*" / f"MP2040_*_{board}.uf2"))
    matches.sort(key=os.path.getmtime)
    return Path(matches[-1]) if matches else None


def build_firmware(args):
    """Configure and build the firmware in the container. Returns the built
    UF2 path, or None if the build failed / produced no UF2."""
    # fsdata.c is regenerated from www/ by makefsdata.py at build time, so a
    # stale copy from a previous build must not be compiled in.
    fsdata = REPO_ROOT / "lib" / "httpd" / "fsdata.c"
    if fsdata.exists():
        fsdata.unlink()

    # One CMake build tree per board. RP2040 and RP2350 boards select different
    # platforms/toolchains, and the SDK cannot switch PICO_PLATFORM in an
    # existing build directory. The finished UF2 is copied up to build/ so the
    # flat artifact path stays the same.
    build_dir = f"build/{args.board}"
    log_msg("Configuring...", args.output)
    build_cmd = (
        f'cmake -B {build_dir} -DCMAKE_BUILD_TYPE=Release '
        '-DMP2040_BOARDCONFIG=$MP2040_BOARDCONFIG '
        f'&& cmake --build {build_dir} --parallel'
    )
    ret = run_docker(args.image, build_cmd,
                     extra_args=["-e", f"MP2040_BOARDCONFIG={args.board}"],
                     log_file=args.output, verbose=args.verbose)

    if ret != 0:
        if args.output:
            log_msg(f"Build failed (exit {ret}). Log: {args.output}")
        sys.exit(ret)

    build_dir = REPO_ROOT / "build" / args.board
    matches = sorted(glob.glob(str(build_dir / f"MP2040_*_{args.board}.uf2")),
                     key=os.path.getmtime)
    uf2 = Path(matches[-1]) if matches else None
    if uf2:
        # Keep the documented flat output path (build/MP2040_....uf2) while
        # leaving the per-board CMake tree and its other artifacts in place.
        flat = REPO_ROOT / "build" / uf2.name
        shutil.copy2(uf2, flat)
        uf2 = flat
        log_msg(f"Build complete! → {uf2.name}", args.output)
    else:
        log_msg("Build complete!", args.output)
    if uf2:
        remove_stale_artifacts(args.board, uf2, args.output)
    return uf2


def remove_stale_artifacts(board, uf2, log_file=None):
    """Delete older build artifacts for `board`, keeping only the current
    build's outputs (the UF2 plus its bin/hex/elf/dis/elf.map siblings)."""
    patterns = [f"MP2040_*_{board}.*", f"MP2040_*_{board}_webconfig.uf2"]
    removed = []
    for base in (REPO_ROOT / "build", REPO_ROOT / "build" / board):
        for pattern in patterns:
            for path in glob.glob(str(base / pattern)):
                p = Path(path)
                if p.resolve() == Path(uf2).resolve():
                    continue
                # Keep the current build's siblings (elf/bin/hex/...) in the
                # per-board tree; only the UF2 is copied to build/.
                if p.parent == base and p.name.startswith(Path(uf2).stem + "."):
                    continue
                if p.parent == base and p.name == Path(uf2).name:
                    continue
                p.unlink()
                removed.append(str(p.relative_to(REPO_ROOT / "build")))
    if removed:
        log_msg(f"Removed {len(removed)} stale artifact(s) for {board}:",
                log_file)
        for name in sorted(removed):
            log_msg(f"  {name}", log_file)


def flash_firmware(uf2, args):
    """Wait for the board mount and copy the built UF2 onto it."""
    if not args.flash:
        return
    if uf2 is None:
        log_msg("Warning: no UF2 found, skipping flash", args.output)
        return
    log_msg(f"Waiting {args.timeout}s for a BOOTSEL mount...", args.output)
    flash_dir = wait_for_flash_dir(args)
    if flash_dir is None:
        log_msg(f"Warning: no BOOTSEL mount found after {args.timeout}s, "
                f"skipping flash", args.output)
        return
    dst = flash_dir / uf2.name
    log_msg(f"Flashing: {uf2.name} -> {dst}", args.output)
    try:
        shutil.copy2(uf2, dst)
        log_msg("Flash complete!", args.output)
    except Exception as e:
        log_msg(f"Warning: flash failed: {e}", args.output)


#  CLI

def parse_args(valid_boards):
    parser = argparse.ArgumentParser(
        description="Build MP2040 firmware via Docker.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=f"Available boards: {', '.join(valid_boards)}",
    )
    parser.add_argument("-b", "--board", default="MacroPad",
                        help=f"Board config (default: MacroPad)")
    parser.add_argument("-o", "--output", metavar="FILE",
                        help="Save stdout+stderr to file")
    parser.add_argument("-i", "--image", default=DEFAULT_IMAGE,
                        help=f"Docker image tag (default: {DEFAULT_IMAGE})")
    parser.add_argument("-r", "--rebuild", action="store_true",
                        help="Force rebuilding the Docker image")
    parser.add_argument("-s", "--skip", action="store_true",
                        help="Skip Docker build; use existing UF2 for the "
                             "board (builds only if none exists)")
    parser.add_argument("-c", "--clean", action="store_true",
                        help="Force cleanup step")
    parser.add_argument("-v", "--verbose", action="store_true",
                        help="Print Docker output to terminal when using --output")
    parser.add_argument("-n", "--nuke", action="store_true",
                        help="Flash nuke UF2 to board before build")
    parser.add_argument("-f", "--flash", action="store_true",
                        help="Copy built UF2 to board after build")
    parser.add_argument("-w", "--webconfig", action="store_true",
                        help="Flash a UF2 that boots once into web config mode")
    parser.add_argument("-p", "--path", default=DEFAULT_FLASH_PATH,
                        help="RPI-RP2/RP2350 mount point or parent directory "
                             f"(default: {DEFAULT_FLASH_PATH})")
    parser.add_argument("-t", "--timeout", type=int, default=30,
                        help="Seconds to wait for mount (default: 30)")
    return parser.parse_args()


def validate_board(board, valid_boards):
    if board not in valid_boards:
        print(f"Error: Unknown board '{board}'. Available boards:",
              file=sys.stderr)
        for b in valid_boards:
            print(f"  {b}", file=sys.stderr)
        sys.exit(1)


def main():
    valid_boards = get_valid_boards()
    args = parse_args(valid_boards)

    # Fresh log file when --output is given.
    if args.output:
        open(args.output, "w").close()

    validate_board(args.board, valid_boards)

    # 1. Wipe flash (optional)
    nuke_flash(args)

    # 2. Use an existing UF2 instead of building (optional)
    skip_build = False
    if args.skip:
        uf2 = find_existing_uf2(args.board)
        if uf2:
            log_msg(f"Using existing UF2 → {uf2.name}", args.output)
            skip_build = True
        else:
            log_msg("No existing UF2 found, building anyway", args.output)

    # 3. Build the builder image (once)
    if not skip_build:
        ensure_builder_image(args)
        # 4. Fix ownership / clean build dir
        clean_build_dir(args)
        # 5. Fetch tags so git describe can version the build
        fetch_tags(args)
        # 6. Configure + build firmware
        uf2 = build_firmware(args)
    # 7. Optionally bake a one-time web config boot flag into the UF2
    if args.webconfig:
        if uf2 is None:
            log_msg("Warning: --webconfig skipped (no UF2 produced)",
                    args.output)
        else:
            uf2 = mark_webconfig_uf2(uf2)
            log_msg(f"Web config boot flag added → {uf2.name}", args.output)
    # 8. Copy the UF2 to the board (optional)
    flash_firmware(uf2, args)


if __name__ == "__main__":
    main()
