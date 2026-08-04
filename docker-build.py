#!/usr/bin/env python3
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
DEFAULT_IMAGE = "gp2040-ce-builder"
DEFAULT_FLASH_PATH = os.path.expandvars("/run/media/$USER/RPI-RP2")
NUKE_FILE = REPO_ROOT / "tools" / "flash_nuke.uf2"


def get_valid_boards():
    configs = REPO_ROOT / "configs"
    return sorted(
        d.name for d in configs.iterdir()
        if d.is_dir() and (d / "BoardConfig.h").exists()
    )


def resolve_flash_path(path_str):
    return os.path.expandvars(path_str)


def wait_for_mount(path, timeout, log_file=None):
    for _ in range(timeout):
        if path.is_dir():
            return True
        time.sleep(1)
    return path.is_dir()


def log_msg(msg, log_file=None):
    print(msg)
    if log_file:
        with open(log_file, "a") as f:
            f.write(msg + "\n")


def run_docker(image, command, extra_args=None, log_file=None, verbose=False):
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


def main():
    valid_boards = get_valid_boards()

    parser = argparse.ArgumentParser(
        description="Build MP2040 firmware via Docker.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=f"Available boards: {', '.join(valid_boards)}",
    )
    parser.add_argument("-b", "--board", default="Pico",
                        help=f"Board config (default: Pico)")
    parser.add_argument("-o", "--output", metavar="FILE",
                        help="Save stdout+stderr to file")
    parser.add_argument("-i", "--image", default=DEFAULT_IMAGE,
                        help=f"Docker image tag (default: {DEFAULT_IMAGE})")
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

    args = parser.parse_args()

    if args.output:
        open(args.output, "w").close()

    if args.board not in valid_boards:
        print(f"Error: Unknown board '{args.board}'. Available boards:",
              file=sys.stderr)
        for b in valid_boards:
            print(f"  {b}", file=sys.stderr)
        sys.exit(1)

    flash_path = resolve_flash_path(args.path)
    flash_dir = Path(flash_path)

    # --- Nuke ---
    if args.nuke:
        if not NUKE_FILE.exists():
            log_msg(f"Warning: nuke file not found at {NUKE_FILE}, skipping",
                    args.output)
        elif not flash_dir.is_dir():
            log_msg(f"Waiting {args.timeout}s for {flash_path} to mount...",
                    args.output)
            if not wait_for_mount(flash_dir, args.timeout, args.output):
                log_msg(f"Warning: flash path {flash_path} not found "
                        f"after {args.timeout}s, skipping nuke",
                        args.output)
                flash_dir = None
        if flash_dir and flash_dir.is_dir():
            dst = flash_dir / "flash_nuke.uf2"
            log_msg(f"Nuking board: {NUKE_FILE} -> {dst}", args.output)
            try:
                shutil.copy2(NUKE_FILE, dst)
                log_msg("Nuke sent", args.output)
            except Exception as e:
                log_msg(f"Warning: nuke failed: {e}", args.output)

    # --- Cleanup ---
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

    # --- Build ---
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

    uf2_matches = sorted(glob.glob(str(REPO_ROOT / "build" / f"MP2040_*_{args.board}.uf2")))
    if uf2_matches:
        log_msg(f"Build complete! → {Path(uf2_matches[-1]).name}", args.output)
    else:
        log_msg("Build complete!", args.output)

    # --- Flash ---
    if args.flash:
        if not flash_dir.is_dir():
            log_msg(f"Waiting {args.timeout}s for {flash_path} to mount...",
                    args.output)
            wait_for_mount(flash_dir, args.timeout, args.output)

        if not flash_dir.is_dir():
            log_msg(f"Warning: flash path {flash_path} not found "
                    f"after {args.timeout}s, skipping flash",
                    args.output)
        else:
            pattern = f"MP2040_*_{args.board}.uf2"
            matches = sorted(glob.glob(str(REPO_ROOT / "build" / pattern)))
            if not matches:
                log_msg(f"Warning: no UF2 found matching '{pattern}', "
                        "skipping flash", args.output)
            else:
                src = Path(matches[-1])
                dst = flash_dir / src.name
                log_msg(f"Flashing: {src.name} -> {dst}", args.output)
                try:
                    shutil.copy2(src, dst)
                    log_msg("Flash complete!", args.output)
                except Exception as e:
                    log_msg(f"Warning: flash failed: {e}", args.output)


if __name__ == "__main__":
    main()
