#!/usr/bin/env python3
import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parent
WWW_DIR = REPO_ROOT / "www"
NPM = shutil.which("npm")


def ensure_dependencies():
    if (WWW_DIR / "node_modules").exists():
        return
    print("node_modules not found, running npm install...")
    subprocess.run([NPM, "install"], cwd=WWW_DIR, check=True)


def main():
    parser = argparse.ArgumentParser(
        description="Initialize (if needed) and run the MP2040 web configurator dev server.",
    )
    parser.add_argument("-b", "--board", metavar="BOARD",
                        help="Initial board for the mock server (VITE_MP2040_BOARD)")
    parser.add_argument("-u", "--fake-update", metavar="VERSION",
                        help="Show a fake update on the welcome page (VITE_FAKE_UPDATE), e.g. v9.9.9")
    parser.add_argument("-p", "--port", type=int,
                        help="Port for the Vite dev server")
    parser.add_argument("--dev-board", action="store_true",
                        help="Proxy to a real board instead of the mock (npm run dev-board)")
    args = parser.parse_args()

    if NPM is None:
        print("Error: npm not found on PATH", file=sys.stderr)
        sys.exit(1)

    if args.dev_board and args.board:
        print("Warning: --board is ignored in --dev-board mode "
              "(the board comes from the real device)", file=sys.stderr)

    ensure_dependencies()

    env = os.environ.copy()
    if args.board:
        env["VITE_MP2040_BOARD"] = args.board
    if args.fake_update:
        env["VITE_FAKE_UPDATE"] = args.fake_update

    script = "dev-board" if args.dev_board else "dev"
    cmd = [NPM, "run", script]
    if args.port:
        cmd.extend(["--", "--port", str(args.port)])

    sys.exit(subprocess.run(cmd, cwd=WWW_DIR, env=env).returncode)


if __name__ == "__main__":
    main()
