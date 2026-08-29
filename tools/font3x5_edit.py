#!/usr/bin/env python3
"""Edit the MP2040 3x5 splash font as an image.

The font is headers/display/fonts/GP_Font_3x5.h: 59 glyphs covering ASCII
32-90 (space..Z), each 3x5 pixels, stored column-major (one byte per
column, LSB = top row).

The edit image lays the glyphs out in a 10-column grid, one 3x5 pixel block
per glyph, black = pixel on, white = off. Glyphs run left-to-right,
top-to-bottom in ASCII order:

  Row 1:  ! " # $ % & ' ( )
  Row 2:  * + , - . / 0 1 2 3
  Row 3:  4 5 6 7 8 9 : ; < =
  Row 4:  > ? @ A B C D E F G
  Row 5:  H I J K L M N O P Q
  Row 6:  R S T U V W X Y Z

(leading space is the blank first cell of Row 1)

Usage:
  python3 tools/font3x5_edit.py --render [-o out.png]   # write edit image
  python3 tools/font3x5_edit.py --parse [-i in.png]     # regenerate header
"""

import argparse
import os
import re
import sys

from PIL import Image, ImageDraw

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FONT_PATH = os.path.join(ROOT, "headers", "display", "fonts", "GP_Font_3x5.h")

FIRST = 32          # ASCII of space
LAST = 90           # ASCII of Z
COUNT = LAST - FIRST + 1

GLYPH_W, GLYPH_H = 3, 5
GRID_COLS = 10
GRID_ROWS = (COUNT + GRID_COLS - 1) // GRID_COLS   # 6
GAP = 2             # logical px between cells
MARGIN = 2          # logical px outer margin
ZOOM = 1            # image px per logical px (1:1)

LOGICAL_W = MARGIN + GRID_COLS * (GLYPH_W + GAP) - GAP + MARGIN
LOGICAL_H = MARGIN + GRID_ROWS * (GLYPH_H + GAP) - GAP + MARGIN
IMG_W = LOGICAL_W * ZOOM
IMG_H = LOGICAL_H * ZOOM

GUIDE = 200         # light gray guide lines (parsed as "off")
THRESHOLD = 128     # block mean below this => pixel on


def load_glyphs():
    text = open(FONT_PATH, "r", encoding="utf-8").read()
    body = re.search(r"GP_Font_3x5\[\]\s*=\s*\{(.*?)\};", text, re.S).group(1)
    data = [int(n, 0) for n in re.findall(r"0x[0-9a-fA-F]+|\b\d+\b", body)]
    if len(data) != COUNT * 3:
        raise SystemExit(f"error: expected {COUNT*3} font bytes, found {len(data)}")
    return [tuple(data[i*3:i*3+3]) for i in range(COUNT)]


def cell_origin(index):
    col = index % GRID_COLS
    row = index // GRID_COLS
    x = (MARGIN + col * (GLYPH_W + GAP)) * ZOOM
    y = (MARGIN + row * (GLYPH_H + GAP)) * ZOOM
    return x, y


def render(glyphs, out_path):
    img = Image.new("L", (IMG_W, IMG_H), 255)
    draw = ImageDraw.Draw(img)

    for col in range(GRID_COLS):
        gx = (MARGIN + col * (GLYPH_W + GAP) + GLYPH_W) * ZOOM
        draw.rectangle([gx, 0, gx, IMG_H - 1], fill=GUIDE)
    for row in range(GRID_ROWS):
        gy = (MARGIN + row * (GLYPH_H + GAP) + GLYPH_H) * ZOOM
        draw.rectangle([0, gy, IMG_W - 1, gy], fill=GUIDE)

    for index, glyph in enumerate(glyphs):
        x0, y0 = cell_origin(index)
        for c in range(GLYPH_W):
            for r in range(GLYPH_H):
                if glyph[c] & (1 << r):
                    draw.rectangle(
                        [x0 + c * ZOOM, y0 + r * ZOOM,
                         x0 + (c + 1) * ZOOM - 1, y0 + (r + 1) * ZOOM - 1],
                        fill=0)

    img.save(out_path, "PNG")
    print(f"wrote {out_path} ({IMG_W}x{IMG_H})")
    print("glyph grid (ASCII code -> character):")
    for row in range(GRID_ROWS):
        cells = []
        for col in range(GRID_COLS):
            index = row * GRID_COLS + col
            if index >= COUNT:
                break
            code = FIRST + index
            label = repr(chr(code))
            cells.append(f"{code:3d}:{label}")
        print("  " + "  ".join(cells))


def parse(in_path):
    img = Image.open(in_path).convert("L")
    if img.size != (IMG_W, IMG_H):
        raise SystemExit(
            f"error: image is {img.size[0]}x{img.size[1]}, expected {IMG_W}x{IMG_H}. "
            "Please edit without resizing/cropping.")

    px = img.load()
    glyphs = []
    for index in range(COUNT):
        x0, y0 = cell_origin(index)
        glyph = []
        for c in range(GLYPH_W):
            b = 0
            for r in range(GLYPH_H):
                total = 0
                count = 0
                for iy in range(y0 + r * ZOOM, y0 + (r + 1) * ZOOM):
                    for ix in range(x0 + c * ZOOM, x0 + (c + 1) * ZOOM):
                        total += px[ix, iy]
                        count += 1
                if total / count < THRESHOLD:
                    b |= 1 << r
            if b & ~0x1F:
                raise SystemExit(f"error: glyph {index} (chr {chr(FIRST+index)!r}) "
                                 f"has bits above row 4 (0x{b:02x})")
            glyph.append(b)
        glyphs.append(tuple(glyph))

    ascii_art(glyphs)
    write_header(glyphs)
    print(f"wrote {FONT_PATH}")


def ascii_art(glyphs):
    for row in range(GRID_ROWS):
        lines = [""] * GLYPH_H
        for col in range(GRID_COLS):
            index = row * GRID_COLS + col
            if index >= COUNT:
                break
            glyph = glyphs[index]
            for r in range(GLYPH_H):
                lines[r] += "".join("#" if glyph[c] & (1 << r) else "." for c in range(GLYPH_W)) + " "
        print(f"  {FIRST + row * GRID_COLS:3d}-{FIRST + row * GRID_COLS + GRID_COLS - 1:3d}")
        for line in lines:
            print("   " + line)


def write_header(glyphs):
    lines = []
    lines.append("#ifndef _GP_FONT_3X5_H_")
    lines.append("#define _GP_FONT_3X5_H_")
    lines.append("")
    lines.append("// 3x5 font, ASCII 32-90, column-major (one byte per column,")
    lines.append("// LSB = top row). 3 bytes per glyph.")
    lines.append("#define GP_FONT_3x5_WIDTH 3")
    lines.append("#define GP_FONT_3x5_HEIGHT 5")
    lines.append("#define GP_FONT_3x5_COUNT %d" % COUNT)
    lines.append("")
    lines.append("const uint8_t GP_Font_3x5[] = {")
    row = []
    for glyph in glyphs:
        for b in glyph:
            row.append("0x%02x" % b)
            if len(row) == 12:
                lines.append("\t" + ", ".join(row) + ",")
                row = []
    if row:
        lines.append("\t" + ", ".join(row) + ",")
    lines.append("};")
    lines.append("")
    lines.append("#endif")
    with open(FONT_PATH, "w", encoding="utf-8") as fh:
        fh.write("\n".join(lines))


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--render", action="store_true", help="write the editable PNG")
    mode.add_argument("--parse", action="store_true", help="regenerate the header from a PNG")
    parser.add_argument("-o", "--output", default=os.path.join(ROOT, "GP_Font_3x5_edit.png"),
                        help="render output path (default: ./GP_Font_3x5_edit.png)")
    parser.add_argument("-i", "--input", default=os.path.join(ROOT, "GP_Font_3x5_edit.png"),
                        help="parse input path (default: ./GP_Font_3x5_edit.png)")
    args = parser.parse_args()

    if args.render:
        render(load_glyphs(), args.output)
    else:
        parse(args.input)


if __name__ == "__main__":
    main()