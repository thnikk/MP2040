#!/usr/bin/env python3
"""Build MP2040 firmware via Docker.

Runs the CMake build inside the gp2040-ce-builder Docker image (no local ARM
toolchain needed) and optionally flashes a connected board. Pipeline:

    nuke → ensure builder image → clean → fetch tags → build → flash

Examples:
    python3 docker-build.py -b 2k
    python3 docker-build.py -b MacroPad --nuke --flash
    python3 docker-build.py -b 2k --clean --verbose --output build.log
"""
import argparse
import glob
import os
import re
import shutil
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


def resolve_flash_path(path_str):
    """Expand $VAR references in the flash mount path."""
    return os.path.expandvars(path_str)


def wait_for_mount(path, timeout, log_file=None):
    """Poll for up to `timeout` seconds until the flash drive appears."""
    for _ in range(timeout):
        if path.is_dir():
            return True
        time.sleep(1)
    return path.is_dir()


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

def nuke_flash(args, flash_dir):
    """Flash the nuke UF2 to wipe flash, or warn. Returns the flash dir
    (or None if the drive never mounted, so the later flash step can skip)."""
    if not args.nuke:
        return flash_dir
    if not NUKE_FILE.exists():
        log_msg(f"Warning: nuke file not found at {NUKE_FILE}, skipping",
                args.output)
        return flash_dir
    if not flash_dir.is_dir():
        log_msg(f"Waiting {args.timeout}s for {flash_dir} to mount...",
                args.output)
        if not wait_for_mount(flash_dir, args.timeout, args.output):
            log_msg(f"Warning: flash path {flash_dir} not found "
                    f"after {args.timeout}s, skipping nuke",
                    args.output)
            return None
    dst = flash_dir / "flash_nuke.uf2"
    log_msg(f"Nuking board: {NUKE_FILE} -> {dst}", args.output)
    try:
        shutil.copy2(NUKE_FILE, dst)
        log_msg("Nuke sent", args.output)
    except Exception as e:
        log_msg(f"Warning: nuke failed: {e}", args.output)
    return flash_dir


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


def build_firmware(args):
    """Configure and build the firmware in the container. Returns the built
    UF2 path, or None if the build failed / produced no UF2."""
    # fsdata.c is regenerated from www/ by makefsdata.py at build time, so a
    # stale copy from a previous build must not be compiled in.
    fsdata = REPO_ROOT / "lib" / "httpd" / "fsdata.c"
    if fsdata.exists():
        fsdata.unlink()

    log_msg("Configuring...", args.output)
    build_cmd = (
        'cmake -B build -DCMAKE_BUILD_TYPE=Release '
        '-DMP2040_BOARDCONFIG=$MP2040_BOARDCONFIG '
        '&& cmake --build build --parallel'
    )
    ret = run_docker(args.image, build_cmd,
                     extra_args=["-e", f"MP2040_BOARDCONFIG={args.board}"],
                     log_file=args.output, verbose=args.verbose)

    if ret != 0:
        if args.output:
            log_msg(f"Build failed (exit {ret}). Log: {args.output}")
        sys.exit(ret)

    matches = sorted(glob.glob(str(REPO_ROOT / "build" / f"MP2040_*_{args.board}.uf2")))
    uf2 = Path(matches[-1]) if matches else None
    if uf2:
        log_msg(f"Build complete! → {uf2.name}", args.output)
    else:
        log_msg("Build complete!", args.output)
    return uf2


def flash_firmware(uf2, args, flash_dir):
    """Wait for the board mount and copy the built UF2 onto it."""
    if not args.flash:
        return
    if uf2 is None:
        log_msg("Warning: no UF2 found, skipping flash", args.output)
        return
    if flash_dir is None:
        log_msg(f"Warning: flash path not available, skipping flash",
                args.output)
        return
    if not flash_dir.is_dir():
        log_msg(f"Waiting {args.timeout}s for {flash_dir} to mount...",
                args.output)
        wait_for_mount(flash_dir, args.timeout, args.output)
    if not flash_dir.is_dir():
        log_msg(f"Warning: flash path {flash_dir} not found "
                f"after {args.timeout}s, skipping flash",
                args.output)
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
    parser.add_argument("-c", "--clean", action="store_true",
                        help="Force cleanup step")
    parser.add_argument("-v", "--verbose", action="store_true",
                        help="Print Docker output to terminal when using --output")
    parser.add_argument("-n", "--nuke", action="store_true",
                        help="Flash nuke UF2 to board before build")
    parser.add_argument("-f", "--flash", action="store_true",
                        help="Copy built UF2 to board after build")
    parser.add_argument("-p", "--path", default=DEFAULT_FLASH_PATH,
                        help=f"RPI-RP2 mount point (default: {DEFAULT_FLASH_PATH})")
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

    flash_dir = Path(resolve_flash_path(args.path))

    # 1. Wipe flash (optional)
    flash_dir = nuke_flash(args, flash_dir)
    # 2. Build the builder image (once)
    ensure_builder_image(args)
    # 3. Fix ownership / clean build dir
    clean_build_dir(args)
    # 4. Fetch tags so git describe can version the build
    fetch_tags(args)
    # 5. Configure + build firmware
    uf2 = build_firmware(args)
    # 6. Copy the UF2 to the board (optional)
    flash_firmware(uf2, args, flash_dir)


if __name__ == "__main__":
    main()
