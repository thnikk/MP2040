#!/usr/bin/env python3
"""Generate a board-specific copy of proto/config.proto.

The ScreenPad build enables a per-key Font Awesome icon field. Putting it in the
generated proto only for that board keeps every other board's Config struct and
stored config byte-for-byte unchanged.
"""

import sys
from pathlib import Path

MARKER = "    // @MP2040_BOARD_ICONS@\n"

ICON_FIELD = """    // Per-key Font Awesome solid icon codepoint (0 = use the text label).
    // ScreenPad-only: injected into this board's generated config schema.
    repeated uint32 iconIds = 100 [(nanopb).max_count = 16];
"""


def main():
    if len(sys.argv) not in (3, 4):
        raise SystemExit("usage: gen_board_proto.py <src> <dst> [enabled]")
    src, dst = Path(sys.argv[1]), Path(sys.argv[2])
    enabled = sys.argv[3] if len(sys.argv) > 3 else "0"
    text = src.read_text(encoding="utf-8")
    if MARKER not in text:
        raise SystemExit(f"marker missing from {src}")
    text = text.replace(MARKER, ICON_FIELD if enabled == "1" else "")
    dst.parent.mkdir(parents=True, exist_ok=True)
    dst.write_text(text, encoding="utf-8")


if __name__ == "__main__":
    main()
