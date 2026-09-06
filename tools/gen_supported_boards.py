#!/usr/bin/env python3
"""Generate the supported-boards preview image from configs/*/board.svg.

Reads each board's BoardConfig.h to determine its category (touch keypad,
controller, or plain mechanical keypad) and display label, extracts the
"case" and "pin"/"key" shapes from its board.svg, and lays out a two-tone
preview grid grouped by category. Writes a light-mode and dark-mode SVG to
assets/ and rewrites the generated block in README.md.

Run this after adding, removing, or renaming a board under configs/:

    python3 tools/gen_supported_boards.py
"""

import argparse
import os
import re
import xml.etree.ElementTree as ET
from xml.sax.saxutils import escape

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CONFIGS = os.path.join(ROOT, "configs")
ASSETS = os.path.join(ROOT, "assets")
README = os.path.join(ROOT, "README.md")

INKSCAPE_LABEL = "{http://www.inkscape.org/namespaces/inkscape}label"
SHAPE_TAGS = {"rect", "circle", "ellipse", "path", "polygon", "polyline",
              "line"}
GEOM_ATTRS = ("x", "y", "x1", "y1", "x2", "y2", "width", "height",
              "rx", "ry", "cx", "cy", "r", "d", "points", "transform")
PIN_RE = re.compile(r"^(pin|key)\d")
LED_RE = re.compile(r"^led-?\d+")

# Category order and display headings.
CATEGORIES = [
    ("keypad", "Mechanical Keypads"),
    ("touch", "Touch Keypads"),
    ("controller", "Controllers"),
]

# Two-tone color scheme, matching assets/mp2040-logo(-light).svg. The case
# (background shape) uses whichever color contrasts with the page behind
# it; the pins use the opposite color for contrast against the case.
COLOR_SCHEMES = {
    "light": {"case": "#2b303b", "pin": "#d8dee9"},
    "dark": {"case": "#d8dee9", "pin": "#2b303b"},
}

# The OLED screen is always rendered black, regardless of color mode.
OLED_COLOR = "#000000"

# Per-key LEDs are rendered solid red regardless of color mode.
LED_COLOR = "#bf616a"

# Layout constants (SVG units).
ICON_SIZE = 160
CELL_WIDTH = 180
LABEL_GAP = 8
LABEL_HEIGHT = 20
HEADING_HEIGHT = 80
SECTION_GAP = 50
TOP_PAD = 10
FONT_FAMILY = "Nunito, sans-serif"
HEADING_FONT_SIZE = 40
LABEL_FONT_SIZE = 20

MARKER_START = "<!-- SUPPORTED-BOARDS:START -->"
MARKER_END = "<!-- SUPPORTED-BOARDS:END -->"


def local_tag(tag):
    """Strip the XML namespace from an ElementTree tag."""
    return tag.split("}")[-1]


def is_hidden(elem):
    """Return True if an element's own style hides it."""
    style = elem.get("style", "").replace(" ", "")
    return "display:none" in style or elem.get("display") == "none"


def classify(elem, has_oled, has_leds):
    """Return 'case', 'pin', 'oled', 'led', or None for a shape element."""
    elem_id = elem.get("id", "")
    label = elem.get(INKSCAPE_LABEL, "")
    if elem_id == "case" or label == "case":
        return "case"
    if has_oled and (elem_id == "oled" or label == "oled"):
        return "oled"
    if elem_id == "ring" or label == "ring":
        return "pin"
    if PIN_RE.match(elem_id) and not elem_id.endswith("-label"):
        return "pin"
    if has_leds and (LED_RE.match(elem_id) or LED_RE.match(label)):
        return "led"
    return None


def filter_tree(elem, colors, has_oled, has_leds):
    """Recursively rebuild elem keeping only case/pin/oled/led shapes, colored.

    Groups are kept only to preserve their transform when they contain a
    kept descendant. Everything else (logo, alignment guides, hidden
    layers) is dropped.
    """
    if is_hidden(elem):
        return None
    tag = local_tag(elem.tag)
    if tag in SHAPE_TAGS:
        role = classify(elem, has_oled, has_leds)
        if role is None:
            return None
        new = ET.Element(tag)
        for key in GEOM_ATTRS:
            if key in elem.attrib:
                new.set(key, elem.attrib[key])
        if role == "oled":
            fill = OLED_COLOR
        elif role == "led":
            fill = LED_COLOR
        else:
            fill = colors[role]
        new.set("style", "fill:%s;stroke:none" % fill)
        return new
    if tag == "g":
        kept = [c for c in (filter_tree(child, colors, has_oled, has_leds)
                            for child in elem) if c is not None]
        if not kept:
            return None
        new = ET.Element("g")
        if "transform" in elem.attrib:
            new.set("transform", elem.attrib["transform"])
        for child in kept:
            new.append(child)
        return new
    return None


def parse_view_box(root):
    """Return (min_x, min_y, width, height) from an svg's viewBox."""
    parts = root.get("viewBox", "0 0 1 1").split()
    return tuple(float(p) for p in parts)


def load_board_icon(svg_path, colors, has_oled, has_leds):
    """Return (icon_g_xml, width, height) for one board's board.svg."""
    tree = ET.parse(svg_path)
    root = tree.getroot()
    min_x, min_y, width, height = parse_view_box(root)
    kept = [c for c in (filter_tree(child, colors, has_oled, has_leds)
                        for child in root) if c is not None]
    inner = "".join(ET.tostring(c, encoding="unicode") for c in kept)
    group = '<g transform="translate(%g,%g)">%s</g>' % (
        -min_x, -min_y, inner)
    return group, width, height


def read_text(path):
    """Read a text file as UTF-8, replacing undecodable bytes."""
    with open(path, "r", encoding="utf-8", errors="replace") as fh:
        return fh.read()


def board_category(config_text):
    """Classify a board from its BoardConfig.h. Touch beats controller."""
    if re.search(r"#define\s+TOUCH_GP\d+\s+1", config_text):
        return "touch"
    if re.search(r"#define\s+DEFAULT_INPUT_MODE\b", config_text):
        return "controller"
    return "keypad"


def board_label(config_text, fallback):
    """Return BOARD_CONFIG_LABEL, or fallback (the directory name)."""
    match = re.search(
        r'#define\s+BOARD_CONFIG_LABEL\s+"([^"]+)"', config_text)
    return match.group(1) if match else fallback


def board_has_oled(config_text):
    """Return True if the board defines HAS_I2C_DISPLAY as 1."""
    return bool(re.search(r"#define\s+HAS_I2C_DISPLAY\s+1", config_text))


def board_has_leds(config_text):
    """Return True if the board defines per-key LEDs (LED_PIN)."""
    return bool(re.search(r"#define\s+LED_PIN\b", config_text))


def find_boards():
    """Return {category: [(label, svg_path, has_oled, has_leds), ...]}, sorted."""
    boards = {key: [] for key, _ in CATEGORIES}
    for name in sorted(os.listdir(CONFIGS)):
        config_path = os.path.join(CONFIGS, name, "BoardConfig.h")
        svg_path = os.path.join(CONFIGS, name, "board.svg")
        if not os.path.isfile(config_path) or not os.path.isfile(svg_path):
            continue
        text = read_text(config_path)
        category = board_category(text)
        label = board_label(text, name)
        has_oled = board_has_oled(text)
        has_leds = board_has_leds(text)
        boards[category].append((label, svg_path, has_oled, has_leds))
    for boards_list in boards.values():
        boards_list.sort(key=lambda item: item[0].casefold())
    return boards


def render_variant(boards, mode):
    """Render the full preview SVG for one color mode."""
    colors = COLOR_SCHEMES[mode]
    text_fill = colors["case"]
    active = [(key, title) for key, title in CATEGORIES if boards[key]]
    max_count = max(len(boards[key]) for key, _ in active)
    canvas_width = max_count * CELL_WIDTH

    body = []
    y = TOP_PAD
    for index, (key, title) in enumerate(active):
        if index:
            y += SECTION_GAP
        body.append(
            '<text x="%g" y="%g" font-family="%s" font-size="%g" '
            'fill="%s" text-anchor="middle">%s</text>'
            % (canvas_width / 2, y + HEADING_FONT_SIZE, FONT_FAMILY,
               HEADING_FONT_SIZE, text_fill, escape(title)))
        y += HEADING_HEIGHT

        row = boards[key]
        row_width = len(row) * CELL_WIDTH
        x_start = (canvas_width - row_width) / 2
        icons = []
        for i, (label, svg_path, has_oled, has_leds) in enumerate(row):
            cx = x_start + i * CELL_WIDTH + CELL_WIDTH / 2
            icon, vb_w, vb_h = load_board_icon(svg_path, colors, has_oled, has_leds)
            scale = ICON_SIZE / max(vb_w, vb_h)
            icons.append((cx, icon, scale, vb_w * scale, vb_h * scale,
                          label))
        row_height = max(render_h for *_, render_h, _ in icons)
        for cx, icon, scale, render_w, render_h, label in icons:
            icon_x = cx - render_w / 2
            icon_y = y + (row_height - render_h) / 2
            body.append(
                '<g transform="translate(%g,%g) scale(%g)">%s</g>'
                % (icon_x, icon_y, scale, icon))
            label_y = y + row_height + LABEL_GAP + LABEL_FONT_SIZE * 0.8
            body.append(
                '<text x="%g" y="%g" font-family="%s" font-size="%g" '
                'fill="%s" text-anchor="middle">%s</text>'
                % (cx, label_y, FONT_FAMILY, LABEL_FONT_SIZE, text_fill,
                   escape(label)))
        y += row_height + LABEL_GAP + LABEL_HEIGHT
    canvas_height = y + TOP_PAD

    return (
        '<?xml version="1.0" encoding="UTF-8" standalone="no"?>\n'
        '<svg viewBox="0 0 %g %g" width="%g" height="%g" '
        'xmlns="http://www.w3.org/2000/svg">\n%s\n</svg>\n'
        % (canvas_width, canvas_height, canvas_width, canvas_height,
           "\n".join(body)))


def build_readme_block(light_rel, dark_rel):
    """Return the <picture> markup pointing at the two rendered SVGs."""
    return (
        '<picture>\n'
        '  <source media="(prefers-color-scheme: dark)" '
        'srcset="%s">\n'
        '  <source media="(prefers-color-scheme: light)" '
        'srcset="%s">\n'
        '  <img alt="Supported boards" src="%s">\n'
        '</picture>' % (dark_rel, light_rel, light_rel))


def update_readme(block):
    """Replace the generated block in README.md, in place.

    On the first run (no markers yet) this replaces the existing manual
    tables, from "### Mechanical Keypads" up to the next "## " heading.
    Later runs just replace the content between the markers.
    """
    text = read_text(README)
    wrapped = "%s\n%s\n%s" % (MARKER_START, block, MARKER_END)
    if MARKER_START in text and MARKER_END in text:
        pattern = re.compile(
            re.escape(MARKER_START) + r".*?" + re.escape(MARKER_END), re.S)
        new_text = pattern.sub(lambda _m: wrapped, text, count=1)
    else:
        start = re.search(r"^### Mechanical Keypads", text, re.M)
        if not start:
            raise SystemExit(
                "could not find the Supported Boards tables in README.md")
        rest = text[start.start():]
        end = re.search(r"^## ", rest, re.M)
        end_pos = start.start() + (end.start() if end else len(rest))
        new_text = text[:start.start()] + wrapped + "\n\n" + text[end_pos:]
    with open(README, "w", encoding="utf-8") as fh:
        fh.write(new_text)


def main():
    parser = argparse.ArgumentParser(
        description="Regenerate the supported-boards preview image.")
    parser.parse_args()

    boards = find_boards()
    if not any(boards.values()):
        raise SystemExit("no boards found under %s" % CONFIGS)

    light_path = os.path.join(ASSETS, "supported-boards-light.svg")
    dark_path = os.path.join(ASSETS, "supported-boards.svg")
    with open(light_path, "w", encoding="utf-8") as fh:
        fh.write(render_variant(boards, "light"))
    with open(dark_path, "w", encoding="utf-8") as fh:
        fh.write(render_variant(boards, "dark"))

    light_rel = os.path.relpath(light_path, ROOT)
    dark_rel = os.path.relpath(dark_path, ROOT)
    update_readme(build_readme_block(light_rel, dark_rel))

    total = sum(len(v) for v in boards.values())
    print("wrote %s and %s (%d boards)" % (light_rel, dark_rel, total))


if __name__ == "__main__":
    main()
