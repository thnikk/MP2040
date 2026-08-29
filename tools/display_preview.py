#!/usr/bin/env python3
"""Preview an OLED display layout (BOARD_DISPLAY_LAYOUT) without flashing.

Renders the 128x64 (or 128x32) monochrome panel pixel-for-pixel the way the
SSD1306 driver does, from a board's BoardConfig.h or the built-in button
layouts in headers/display/ui/buttonlayouts.h.

Two modes:
  python3 tools/display_preview.py --render out.png --board Fightboard
  python3 tools/display_preview.py --board Fightboard --port 8090

The serve mode runs a tiny web server (default port 8090) that shows the
layout on a canvas, scaled up with crisp pixels. The page polls every 500ms,
so editing the board's BoardConfig.h and saving updates the image live.
Placeholder status-bar and input-history text (XINPUT / SOCD-U / A+B) is drawn
in the reserved rows 0-7 and 56-63, matching the on-screen interface.

Rendering is a faithful port of the firmware primitives in
src/display/tiny_ssd1306.cpp (ellipse, rectangle, line, polygon, arc) and the
element drawing in GPButton.cpp / GPShape.cpp / GPLever.cpp. Layout resolution
mirrors src/display/ui/layoutmanager.cpp (builtin groups, southpaw mirror).
"""

import argparse
import json
import math
import os
import re
import struct
import sys
import time
import zlib
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import parse_qs, urlparse

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CONFIGS = os.path.join(ROOT, "configs")
BUTTONLAYOUTS_H = os.path.join(ROOT, "headers", "display", "ui", "buttonlayouts.h")

ELEMENT_VALUES = {
    "GP_ELEMENT_WIDGET": 0,
    "GP_ELEMENT_SCREEN": 1,
    "GP_ELEMENT_BTN_BUTTON": 2,
    "GP_ELEMENT_DIR_BUTTON": 3,
    "GP_ELEMENT_PIN_BUTTON": 4,
    "GP_ELEMENT_LEVER": 5,
    "GP_ELEMENT_LABEL": 6,
    "GP_ELEMENT_SPRITE": 7,
    "GP_ELEMENT_SHAPE": 8,
}

SHAPE_VALUES = {
    "GP_SHAPE_ELLIPSE": 0,
    "GP_SHAPE_SQUARE": 1,
    "GP_SHAPE_LINE": 2,
    "GP_SHAPE_POLYGON": 3,
    "GP_SHAPE_ARC": 4,
}

GAMEPAD_MASK_VALUES = {
    "GAMEPAD_MASK_UP": 1 << 0,
    "GAMEPAD_MASK_DOWN": 1 << 1,
    "GAMEPAD_MASK_LEFT": 1 << 2,
    "GAMEPAD_MASK_RIGHT": 1 << 3,
    "GAMEPAD_MASK_B1": 1 << 0,
    "GAMEPAD_MASK_B2": 1 << 1,
    "GAMEPAD_MASK_B3": 1 << 2,
    "GAMEPAD_MASK_B4": 1 << 3,
    "GAMEPAD_MASK_L1": 1 << 4,
    "GAMEPAD_MASK_R1": 1 << 5,
    "GAMEPAD_MASK_L2": 1 << 6,
    "GAMEPAD_MASK_R2": 1 << 7,
    "GAMEPAD_MASK_S1": 1 << 8,
    "GAMEPAD_MASK_S2": 1 << 9,
    "GAMEPAD_MASK_L3": 1 << 10,
    "GAMEPAD_MASK_R3": 1 << 11,
    "GAMEPAD_MASK_A1": 1 << 12,
    "GAMEPAD_MASK_A2": 1 << 13,
}

BTN_NAMES = {1: "B1", 2: "B2", 4: "B3", 8: "B4", 16: "L1", 32: "R1",
             64: "L2", 128: "R2", 256: "S1", 512: "S2", 1024: "L3",
             2048: "R3", 4096: "A1", 8192: "A2"}
DIR_NAMES = {1: "UP", 2: "DOWN", 4: "LEFT", 8: "RIGHT"}

BUILTIN_GROUPS = {
    "arcade_stick": "BUTTON_GROUP_ARCADE_STICK",
    "stickless": "BUTTON_GROUP_STICKLESS",
    "arcade_buttons": "BUTTON_GROUP_ARCADE_BUTTONS",
    "vewlix": "BUTTON_GROUP_VEWLIX",
    "fightboard": "BUTTON_GROUP_FIGHTBOARD",
}

BUILTIN_LAYOUTS = {
    "stick": ["arcade_stick", "arcade_buttons"],
    "stickless": ["stickless", "arcade_buttons"],
    "vlx": ["arcade_stick", "vewlix"],
    "fightboard": ["stickless", "fightboard"],
}

LAYOUT_DISPLAY_NAMES = {
    "board": "BOARD",
    "stick": "STICK",
    "stickless": "STICKLESS",
    "vlx": "VLX",
    "fightboard": "FIGHTBOARD",
}

BUILTIN_BY_BUTTON_LAYOUT = {0: "stick", 1: "stickless", 2: "vlx", 3: "fightboard"}

FONT_PATH = os.path.join(ROOT, "headers", "display", "fonts", "GP_Font_Standard.h")

STATUS_MODE_TEXT = "XINPUT"
STATUS_RIGHT_TEXT = "SOCD-U"
FOOTER_HISTORY_TEXT = "A+B LB+RB A+B LB+RB A+B"[-21:]

_font_cache = None


def load_font():
    global _font_cache
    if _font_cache is not None:
        return _font_cache
    with open(FONT_PATH, "r", encoding="utf-8") as fh:
        text = fh.read()
    body = re.search(r"GP_Font_Standard\[\]\s*=\s*\{(.*?)\};", text, re.S).group(1)
    data = [int(n, 0) for n in re.findall(r"0x[0-9a-fA-F]+|\b\d+\b", body)]
    glyphs = {}
    for i in range(0, len(data) - 4, 5):
        glyphs[32 + i // 5] = data[i:i + 5]
    _font_cache = glyphs
    return glyphs


def draw_text(fb, col, row, text, invert=False):
    glyphs = load_font()
    x = col * 6
    y = row * 8
    fallback = glyphs.get(ord("?"), [0] * 5)
    for ch in text:
        glyph = glyphs.get(ord(ch), fallback)
        for sprite_y in range(8):
            for sprite_x in range(5):
                color = (glyph[sprite_x] >> sprite_y) & 1
                if invert:
                    color ^= 1
                fb.set(x + sprite_x, y + sprite_y, color)
        x += 6


def cround(v):
    return math.floor(v + 0.5)


def parse_int(text):
    text = text.strip()
    m = re.fullmatch(r"-?\d+", text)
    if m:
        return int(text)
    m = re.fullmatch(r"\(1\s*<<\s*(\d+)\)", text)
    if m:
        return 1 << int(m.group(1))
    return None


def extract_defines(text):
    joined = re.sub(r"\\\r?\n", " ", text)
    out = {}
    for m in re.finditer(r"^\s*#define\s+([A-Z0-9_]+)\s+([^\r\n]+)$", joined, re.M):
        name, value = m.group(1), m.group(2).split("//")[0].strip()
        parsed = parse_int(value)
        if parsed is not None:
            out[name] = parsed
    return out


def strip_comments(text):
    text = re.sub(r"/\*.*?\*/", " ", text, flags=re.S)
    text = re.sub(r"//[^\r\n]*", " ", text)
    return text


def get_define_body(text, name):
    m = re.search(r"#define\s+" + name + r"\b", text)
    if not m:
        return None
    body = re.sub(r"\\\r?\n", "", text[m.end():])
    body = strip_comments(body)
    start = body.find("{")
    if start == -1:
        return None
    depth = 0
    for i in range(start, len(body)):
        if body[i] == "{":
            depth += 1
        elif body[i] == "}":
            depth -= 1
            if depth == 0:
                return body[start:i + 1]
    return None


def resolve_token(token):
    token = token.strip()
    if token in GAMEPAD_MASK_VALUES:
        return GAMEPAD_MASK_VALUES[token]
    if token in ELEMENT_VALUES:
        return ELEMENT_VALUES[token]
    if token in SHAPE_VALUES:
        return SHAPE_VALUES[token]
    parsed = parse_int(token)
    if parsed is not None:
        return parsed
    raise ValueError("unknown token %r" % token)


def parse_element_list(body):
    if not body:
        return []
    elements = []
    for m in re.finditer(r"\{([A-Z0-9_]+)\s*,\s*\{([^}]*)\}\}", body):
        elem_name = m.group(1)
        if elem_name not in ELEMENT_VALUES:
            raise ValueError("unknown element type %r" % elem_name)
        values = [resolve_token(t) for t in m.group(2).split(",")]
        if len(values) < 7:
            raise ValueError("element %s needs at least 7 parameters" % elem_name)
        keys = ["x1", "y1", "x2", "y2", "stroke", "fill", "value",
                "shape", "angleStart", "angleEnd", "closed"]
        params = dict(zip(keys, values))
        for key in keys[len(values):]:
            params[key] = 0
        elements.append({"type": elem_name, "params": params})
    return elements


def element_label(element):
    kind = element["type"]
    value = element["params"]["value"]
    if kind == "GP_ELEMENT_PIN_BUTTON":
        return str(value)
    if kind == "GP_ELEMENT_BTN_BUTTON":
        return BTN_NAMES.get(value, str(value))
    if kind == "GP_ELEMENT_DIR_BUTTON":
        return DIR_NAMES.get(value, str(value))
    if kind == "GP_ELEMENT_LEVER":
        return "STICK"
    return ""


def element_bounds(params, shape):
    if shape == SHAPE_VALUES["GP_SHAPE_SQUARE"] or shape == SHAPE_VALUES["GP_SHAPE_LINE"]:
        x1, y1, x2, y2 = params["x1"], params["y1"], params["x2"], params["y2"]
        return (min(x1, x2), min(y1, y2), max(x1, x2), max(y1, y2))
    r = params["x2"]
    return (params["x1"] - r, params["y1"] - r, params["x1"] + r, params["y1"] + r)


class BoardConfig:
    def __init__(self, text):
        self.text = text
        self.defines = extract_defines(text)
        self.layout_body = get_define_body(text, "BOARD_DISPLAY_LAYOUT")

    @property
    def has_display(self):
        return bool(self.defines.get("HAS_I2C_DISPLAY", 0))

    @property
    def size(self):
        return (128, 32) if self.defines.get("DISPLAY_SIZE", 3) == 2 else (128, 64)

    def flip(self):
        return bool(self.defines.get("DISPLAY_FLIP", 0))

    def invert(self):
        return bool(self.defines.get("DISPLAY_INVERT", 0))

    def default_layout(self):
        layout = self.defines.get("DISPLAY_BUTTON_LAYOUT")
        if layout in BUILTIN_BY_BUTTON_LAYOUT:
            return BUILTIN_BY_BUTTON_LAYOUT[layout]
        return "board"


def load_config(board_name):
    path = os.path.join(CONFIGS, board_name, "BoardConfig.h")
    if not os.path.isfile(path):
        raise FileNotFoundError("no board config at %s" % path)
    with open(path, "r", encoding="utf-8", errors="replace") as fh:
        return path, BoardConfig(fh.read())


def resolve_layout(cfg, layout_name):
    if layout_name is None:
        layout_name = cfg.default_layout()
    if layout_name == "board":
        elements = parse_element_list(cfg.layout_body)
        if not cfg.layout_body and cfg.has_display:
            return elements, "board", ["board defines no BOARD_DISPLAY_LAYOUT; nothing is drawn"]
        return elements, "board", []
    if layout_name == "custom":
        elements = parse_element_list(
            get_define_body(open(BUTTONLAYOUTS_H, encoding="utf-8").read(),
                            BUILTIN_GROUPS["stickless"]))
        return elements, "custom", ["CUSTOM layout is generated at runtime and not previewable; showing STICKLESS base"]
    groups = BUILTIN_LAYOUTS.get(layout_name)
    if groups is None:
        raise ValueError("unknown layout %r" % layout_name)
    with open(BUTTONLAYOUTS_H, "r", encoding="utf-8") as fh:
        text = fh.read()
    elements = []
    for group in groups:
        body = get_define_body(text, BUILTIN_GROUPS[group])
        elements.extend(parse_element_list(body))
    return elements, layout_name, []


def mirror_horizontally(elements, width):
    for element in elements:
        element["params"]["x1"] = (width - 1) - element["params"]["x1"]
        if element["params"]["shape"] == SHAPE_VALUES["GP_SHAPE_SQUARE"]:
            element["params"]["x2"] = (width - 1) - element["params"]["x2"]


class Framebuffer:
    def __init__(self, w, h):
        self.w = w
        self.h = h
        self.buf = bytearray(w * h)

    def set(self, x, y, color):
        if 0 <= x < self.w and 0 <= y < self.h and color:
            self.buf[y * self.w + x] = 1

    def line(self, x1, y1, x2, y2, color):
        dx = abs(x2 - x1)
        dy = abs(y2 - y1)
        step_x = 1 if x1 < x2 else -1
        step_y = 1 if y1 < y2 else -1
        err = dx - dy
        while True:
            self.set(x1, y1, color)
            if x1 == x2 and y1 == y2:
                break
            e2 = 2 * err
            if e2 > -dy:
                err -= dy
                x1 += step_x
            if e2 < dx:
                err += dx
                y1 += step_y

    def ellipse(self, x, y, rx, ry, color, filled):
        x1 = -rx
        y1 = 0
        e2 = ry
        dx = (1 + 2 * x1) * e2 * e2
        dy = x1 * x1
        err = dx + dy
        while x1 <= 0:
            self.set(x - x1, y + y1, color)
            self.set(x + x1, y + y1, color)
            self.set(x + x1, y - y1, color)
            self.set(x - x1, y - y1, color)
            if filled:
                for i in range(-x1):
                    self.set(x - i, y + y1, color)
                    self.set(x + i, y + y1, color)
                    self.set(x + i, y - y1, color)
                    self.set(x - i, y - y1, color)
            e2 = 2 * err
            if e2 >= dx:
                x1 += 1
                dx += 2 * ry * ry
                err += dx
            if e2 <= dy:
                y1 += 1
                dy += 2 * rx * rx
                err += dy
        y1 += 1
        while y1 < ry:
            self.set(x, y + y1, color)
            self.set(x, y - y1, color)
            y1 += 1

    def rectangle(self, x, y, x2, y2, color, filled, angle):
        cx = (x + x2) / 2.0
        cy = (y + y2) / 2.0
        hw = (x2 - x) / 2.0
        hh = (y2 - y) / 2.0
        rad = math.radians(angle)
        cos_a = math.cos(rad)
        sin_a = math.sin(rad)
        corners = []
        for wx, wy in ((-hw, -hh), (hw, -hh), (hw, hh), (-hw, hh)):
            corners.append((cround(cx + cos_a * wx - sin_a * wy),
                            cround(cy + sin_a * wx + cos_a * wy)))
        for i in range(4):
            c0 = corners[i]
            c1 = corners[(i + 1) % 4]
            self.line(c0[0], c0[1], c1[0], c1[1], color)
        if filled:
            num_lines = cround(math.sqrt(hw * hw + hh * hh) * 2)
            for i in range(num_lines + 1):
                t = i / num_lines
                x_start = (1 - t) * corners[0][0] + t * corners[3][0]
                y_start = (1 - t) * corners[0][1] + t * corners[3][1]
                x_end = (1 - t) * corners[1][0] + t * corners[2][0]
                y_end = (1 - t) * corners[1][1] + t * corners[2][1]
                self.line(cround(x_start), cround(y_start),
                          cround(x_end), cround(y_end), color)

    def polygon(self, x, y, radius, sides, color, filled, rotation):
        increment = 2 * math.pi / sides
        xs = []
        ys = []
        for i in range(sides):
            angle = i * increment + rotation
            xs.append(x + cround(radius * math.cos(angle)))
            ys.append(y + cround(radius * math.sin(angle)))
        for i in range(sides):
            j = (i + 1) % sides
            self.line(xs[i], ys[i], xs[j], ys[j], color)
        if filled:
            for scan_y in range(min(ys) + 1, max(ys)):
                hits = []
                for i in range(sides):
                    j = (i + 1) % sides
                    if (ys[i] < scan_y and ys[j] >= scan_y) or (ys[j] < scan_y and ys[i] >= scan_y):
                        hits.append(xs[i] + (scan_y - ys[i]) * (xs[j] - xs[i]) / (ys[j] - ys[i]))
                hits.sort()
                for i in range(0, len(hits) - 1, 2):
                    self.line(int(hits[i]), scan_y, int(hits[i + 1]), scan_y, color)

    def arc(self, x, y, radius, color, filled, start_angle, end_angle, closed):
        step = 0.01
        start = math.radians(start_angle)
        end = math.radians(end_angle)
        angle = start
        while angle < end:
            self.set(x + int(radius * math.cos(angle)),
                     y + int(radius * math.sin(angle)), color)
            angle += step
        self.set(x + int(radius * math.cos(end)),
                 y + int(radius * math.sin(end)), color)
        if closed:
            self.line(x, y, x + int(radius * math.cos(start)),
                      y + int(radius * math.sin(start)), color)
            self.line(x, y, x + int(radius * math.cos(end)),
                      y + int(radius * math.sin(end)), color)
        if filled:
            angle = start
            while angle <= end:
                self.line(x, y, x + int(radius * math.cos(angle)),
                          y + int(radius * math.sin(angle)), color)
                angle += step


def draw_element(fb, element, shape):
    p = element["params"]
    if element["type"] == "GP_ELEMENT_LEVER":
        radius = p["x2"]
        fb.ellipse(p["x1"], p["y1"], radius, radius, p["stroke"], 0)
        tick = 3
        fb.line(p["x1"], max(0, p["y1"] - radius - tick), p["x1"],
                max(0, p["y1"] - radius - tick) + tick, p["stroke"])
        fb.line(p["x1"], min(p["y1"] + radius, 64), p["x1"],
                min(p["y1"] + radius, 64) + tick, p["stroke"])
        fb.line(max(0, p["x1"] - radius - tick), p["y1"],
                max(0, p["x1"] - radius - tick) + tick, p["y1"], p["stroke"])
        fb.line(min(p["x1"] + radius, 128), p["y1"],
                min(p["x1"] + radius, 128) + tick, p["y1"], p["stroke"])
        fb.ellipse(p["x1"], p["y1"], int(radius * 0.75), int(radius * 0.75),
                   p["stroke"], 1)
        return
    is_button = element["type"] in ("GP_ELEMENT_BTN_BUTTON", "GP_ELEMENT_DIR_BUTTON",
                                    "GP_ELEMENT_PIN_BUTTON")
    filled = 0 if is_button else p["fill"]
    if shape == SHAPE_VALUES["GP_SHAPE_ELLIPSE"]:
        fb.ellipse(p["x1"], p["y1"], p["x2"], p["x2"], p["stroke"], filled)
    elif shape == SHAPE_VALUES["GP_SHAPE_SQUARE"]:
        fb.rectangle(p["x1"], p["y1"], p["x2"], p["y2"], p["stroke"],
                     filled, p["angleStart"])
    elif shape == SHAPE_VALUES["GP_SHAPE_LINE"]:
        fb.line(p["x1"], p["y1"], p["x2"], p["y2"], p["stroke"])
    elif shape == SHAPE_VALUES["GP_SHAPE_POLYGON"]:
        fb.polygon(p["x1"], p["y1"], p["x2"], p["y2"], p["stroke"],
                   filled, p["angleStart"])
    elif shape == SHAPE_VALUES["GP_SHAPE_ARC"]:
        fb.arc(p["x1"], p["y1"], p["x2"], p["stroke"], filled,
               p["angleStart"], p["angleEnd"], p["closed"])


def render(elements, w, h, flip, invert, show_footer=True):
    fb = Framebuffer(w, h)
    for element in elements:
        draw_element(fb, element, element["params"]["shape"])
    draw_text(fb, 0, 0, STATUS_MODE_TEXT)
    right_col = 21 - len(STATUS_RIGHT_TEXT)
    draw_text(fb, right_col, 0, STATUS_RIGHT_TEXT)
    if show_footer and h >= 64:
        draw_text(fb, 0, 7, FOOTER_HISTORY_TEXT)
    if flip:
        rotated = bytearray(w * h)
        for y in range(h):
            for x in range(w):
                rotated[y * w + x] = fb.buf[(h - 1 - y) * w + (w - 1 - x)]
        fb.buf = rotated
    if invert:
        for i in range(len(fb.buf)):
            fb.buf[i] ^= 1
    return bytes(v * 255 for v in fb.buf)


def check_layout(w, h, elements):
    warnings = []
    reserved_top = 8
    reserved_bottom = max(0, h - 8)
    for i, element in enumerate(elements):
        params = element["params"]
        shape = params["shape"]
        left, top, right, bottom = element_bounds(params, shape)
        if bottom < 0 or top >= h or right < 0 or left >= w:
            warnings.append("element %d (%s) is outside the %dx%d panel" %
                            (i, element["type"], w, h))
        if bottom >= 0 and top < reserved_top:
            warnings.append("element %d (%s) overlaps the status bar (rows 0-%d)" %
                            (i, element["type"], reserved_top - 1))
        if bottom >= reserved_bottom and top < h:
            warnings.append("element %d (%s) overlaps the input-history footer (rows %d-%d)" %
                            (i, element["type"], reserved_bottom, h - 1))
    return warnings


def build_entry(board_name, layout_name, flip, invert, southpaw):
    path, cfg = load_config(board_name)
    if not cfg.has_display:
        return {"error": "board '%s' does not define HAS_I2C_DISPLAY" % board_name}
    w, h = cfg.size
    if layout_name is None:
        layout_name = cfg.default_layout()
    elements, resolved, layout_warnings = resolve_layout(cfg, layout_name)
    if southpaw:
        mirror_horizontally(elements, w)
    warnings = layout_warnings + check_layout(w, h, elements)
    eff_flip = flip if flip is not None else cfg.flip()
    eff_invert = invert if invert is not None else cfg.invert()
    show_footer = bool(cfg.defines.get("DISPLAY_INPUT_HISTORY", 1)) and h >= 64
    raw = render(elements, w, h, eff_flip, eff_invert, show_footer)
    entry = {
        "board": board_name,
        "boardPath": os.path.relpath(path, ROOT),
        "revision": int(os.stat(path).st_mtime_ns),
        "renderedAt": time.time(),
        "size": [w, h],
        "layout": layout_name,
        "layoutName": LAYOUT_DISPLAY_NAMES.get(resolved, resolved),
        "flip": eff_flip,
        "invert": eff_invert,
        "southpaw": bool(southpaw),
        "boardFlip": cfg.flip(),
        "boardInvert": cfg.invert(),
        "elements": [{
            "type": e["type"],
            "label": element_label(e),
            "params": dict(e["params"]),
        } for e in elements],
        "warnings": warnings,
        "error": None,
        "raw": raw,
    }
    return entry


class Preview:
    def __init__(self, default_board):
        self.default_board = default_board
        self.cache = {}

    def get(self, board_name, layout_name, flip, invert, southpaw):
        if not board_name:
            board_name = self.default_board
        try:
            path, cfg = load_config(board_name)
        except FileNotFoundError as exc:
            return {"error": str(exc)}
        mtime = int(os.stat(path).st_mtime_ns)
        key = (board_name, layout_name, flip, invert, southpaw, mtime)
        if key not in self.cache:
            if len(self.cache) > 200:
                self.cache.clear()
            try:
                self.cache[key] = build_entry(board_name, layout_name,
                                              flip, invert, southpaw)
            except (ValueError, OSError) as exc:
                self.cache[key] = {"board": board_name, "error": str(exc),
                                   "revision": mtime, "renderedAt": time.time()}
        return self.cache[key]

    def state(self, board_name, layout_name, flip, invert, southpaw):
        entry = dict(self.get(board_name, layout_name, flip, invert, southpaw))
        entry.pop("raw", None)
        return entry

    def raw(self, board_name, layout_name, flip, invert, southpaw):
        return self.get(board_name, layout_name, flip, invert, southpaw).get("raw")


def parse_bool(value):
    return value in ("1", "true", "on", "yes")


PAGE = r"""<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<title>MP2040 Display Layout Preview</title>
<style>
  body { font-family: system-ui, sans-serif; background: #15151a; color: #ddd; margin: 0; }
  header { display: flex; gap: 14px; align-items: center; padding: 10px 16px; border-bottom: 1px solid #333; flex-wrap: wrap; }
  header strong { color: #fff; }
  select, input[type=range] { background: #23232a; color: #ddd; border: 1px solid #555; padding: 4px 8px; border-radius: 4px; }
  label { display: inline-flex; align-items: center; gap: 5px; font-size: 13px; }
  #stage { position: relative; display: inline-block; margin: 18px 16px 8px; background: #000; border: 1px solid #555; }
  #screen { display: block; image-rendering: pixelated; }
  #overlay { position: absolute; left: 0; top: 0; }
  #status { margin: 4px 16px 20px; color: #9aa; font: 12px/1.6 monospace; white-space: pre-wrap; }
  .warn { color: #ffb34d; }
  .err { color: #ff6b6b; }
</style>
</head>
<body>
<header>
  <strong>Display layout preview</strong>
  <label>Board <select id="board"></select></label>
  <label>Layout <select id="layout">
    <option value="board">board (default)</option>
    <option value="stick">STICK</option>
    <option value="stickless">STICKLESS</option>
    <option value="vlx">VLX</option>
    <option value="fightboard">FIGHTBOARD</option>
  </select></label>
  <label><input type="checkbox" id="flip"> flip (180&deg;)</label>
  <label><input type="checkbox" id="invert"> invert</label>
  <label><input type="checkbox" id="southpaw"> southpaw</label>
  <label>scale <input type="range" id="scale" min="2" max="16" step="1" value="8"></label>
  <label><input type="checkbox" id="grid" checked> grid</label>
  <label><input type="checkbox" id="labels" checked> labels</label>
</header>
<div id="stage"><canvas id="screen"></canvas><canvas id="overlay"></canvas></div>
<div id="status">loading&hellip;</div>
<script>
const $ = (id) => document.getElementById(id);
let state = null;
let lastSig = "";
let lastRaw = null;

function qp() {
  const p = new URLSearchParams();
  p.set("board", $("board").value);
  p.set("layout", $("layout").value);
  p.set("flip", $("flip").checked ? 1 : 0);
  p.set("invert", $("invert").checked ? 1 : 0);
  p.set("southpaw", $("southpaw").checked ? 1 : 0);
  return p;
}

async function getJSON(q) {
  const r = await fetch("/api/state?" + q.toString());
  return r.json();
}

function drawScreen(buf, w, h, scale) {
  const c = $("screen");
  c.width = w;
  c.height = h;
  c.style.width = (w * scale) + "px";
  c.style.height = (h * scale) + "px";
  const ctx = c.getContext("2d");
  const img = ctx.createImageData(w, h);
  for (let i = 0, n = w * h; i < n; i++) {
    const v = buf[i];
    img.data[i * 4] = img.data[i * 4 + 1] = img.data[i * 4 + 2] = v;
    img.data[i * 4 + 3] = 255;
  }
  ctx.putImageData(img, 0, 0);
}

function drawOverlay() {
  const ov = $("overlay");
  const s = state;
  const scale = parseInt($("scale").value);
  ov.width = s.size[0] * scale;
  ov.height = s.size[1] * scale;
  const ctx = ov.getContext("2d");
  ctx.clearRect(0, 0, ov.width, ov.height);
  if ($("grid").checked) {
    ctx.strokeStyle = "rgba(255,255,255,0.07)";
    ctx.lineWidth = 1;
    ctx.beginPath();
    for (let x = 0; x <= s.size[0]; x++) { ctx.moveTo(x * scale, 0); ctx.lineTo(x * scale, ov.height); }
    for (let y = 0; y <= s.size[1]; y++) { ctx.moveTo(0, y * scale); ctx.lineTo(ov.width, y * scale); }
    ctx.stroke();
  }
  if ($("labels").checked) {
    ctx.font = "10px monospace";
    ctx.textAlign = "center";
    ctx.textBaseline = "middle";
    for (const el of s.elements) {
      const p = el.params;
      let cx = p.x1, cy = p.y1;
      if (el.params.shape === 1) { cx = (p.x1 + p.x2) / 2; cy = (p.y1 + p.y2) / 2; }
      if (!el.label) continue;
      ctx.strokeStyle = "rgba(64,224,208,0.4)";
      ctx.beginPath();
      ctx.arc(cx * scale, cy * scale, 2, 0, 7);
      ctx.stroke();
      const tw = ctx.measureText(el.label).width;
      ctx.fillStyle = "rgba(0,0,0,0.85)";
      ctx.fillRect(cx * scale - tw / 2 - 2, cy * scale - 6, tw + 4, 12);
      ctx.fillStyle = "#40e0d0";
      ctx.fillText(el.label, cx * scale, cy * scale + 0.5);
    }
  }
}

async function refresh() {
  const q = qp();
  const s = await getJSON(q);
  if (s.error) {
    $("status").innerHTML = '<span class="err">ERROR: ' + s.error + "</span>";
    return;
  }
  state = s;
  const sig = q.toString() + "|" + s.revision;
  if (sig !== lastSig) {
    const r = await fetch("/preview.raw?" + q.toString());
    lastRaw = new Uint8Array(await r.arrayBuffer());
    lastSig = sig;
  }
  const scale = parseInt($("scale").value);
  drawScreen(lastRaw, s.size[0], s.size[1], scale);
  drawOverlay();
  const stamp = new Date(s.renderedAt * 1000).toLocaleTimeString();
  const head = s.board + " \u00b7 " + s.layoutName + " \u00b7 " + s.size[0] + "\u00d7" + s.size[1] +
    " \u00b7 rendered " + stamp + " \u00b7 rev " + s.revision;
  const warns = s.warnings.map((w) => '<span class="warn">\u26a0 ' + w + "</span>").join("\n");
  $("status").innerHTML = head + (warns ? "\n" + warns : "");
}

async function init() {
  const boards = await (await fetch("/api/boards")).json();
  const sel = $("board");
  for (const b of boards) {
    const o = document.createElement("option");
    o.value = b.name;
    o.textContent = b.name + (b.hasDisplay ? "" : " (no display)");
    sel.appendChild(o);
  }
  const s0 = await getJSON(new URLSearchParams());
  if (s0 && s0.board && sel.querySelector('option[value="' + s0.board + '"]')) {
    sel.value = s0.board;
    $("flip").checked = !!s0.boardFlip;
    $("invert").checked = !!s0.boardInvert;
  }
  $("board").addEventListener("change", async () => {
    const s = await getJSON(qp());
    if (s.board) { $("flip").checked = !!s.boardFlip; $("invert").checked = !!s.boardInvert; }
    refresh();
  });
  $("layout").addEventListener("change", refresh);
  $("flip").addEventListener("change", refresh);
  $("invert").addEventListener("change", refresh);
  $("southpaw").addEventListener("change", refresh);
  $("scale").addEventListener("input", () => { drawScreen(lastRaw, state.size[0], state.size[1], parseInt($("scale").value)); drawOverlay(); });
  $("grid").addEventListener("change", drawOverlay);
  $("labels").addEventListener("change", drawOverlay);
  await refresh();
  setInterval(refresh, 500);
}
init();
</script>
</body>
</html>
"""


def find_boards():
    boards = []
    if not os.path.isdir(CONFIGS):
        return boards
    for name in sorted(os.listdir(CONFIGS)):
        path = os.path.join(CONFIGS, name, "BoardConfig.h")
        if not os.path.isfile(path):
            continue
        try:
            with open(path, "r", encoding="utf-8", errors="replace") as fh:
                text = fh.read()
        except OSError:
            continue
        has_display = bool(extract_defines(text).get("HAS_I2C_DISPLAY", 0))
        boards.append({"name": name, "hasDisplay": has_display})
    return boards


class Handler(BaseHTTPRequestHandler):
    preview = None

    def log_message(self, fmt, *args):
        return

    def _json(self, obj, status=200):
        body = json.dumps(obj).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        parsed = urlparse(self.path)
        q = {k: v[0] for k, v in parse_qs(parsed.query).items()}
        board = q.get("board")
        layout = q.get("layout") or None
        flip = parse_bool(q["flip"]) if "flip" in q else None
        invert = parse_bool(q["invert"]) if "invert" in q else None
        southpaw = parse_bool(q.get("southpaw", ""))
        if parsed.path == "/":
            body = PAGE.encode("utf-8")
            self.send_response(200)
            self.send_header("Content-Type", "text/html; charset=utf-8")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)
        elif parsed.path == "/api/boards":
            self._json(find_boards())
        elif parsed.path == "/api/state":
            self._json(self.preview.state(board, layout, flip, invert, southpaw))
        elif parsed.path == "/preview.raw":
            raw = self.preview.raw(board, layout, flip, invert, southpaw)
            if raw is None:
                self._json({"error": "no render"}, status=400)
                return
            self.send_response(200)
            self.send_header("Content-Type", "application/octet-stream")
            self.send_header("Content-Length", str(len(raw)))
            self.send_header("Cache-Control", "no-store")
            self.end_headers()
            self.wfile.write(raw)
        else:
            self.send_error(404)


def write_png(path, w, h, pixels):
    raw = bytearray()
    for y in range(h):
        raw.append(0)
        raw += pixels[y * w:(y + 1) * w]

    def chunk(typ, data):
        out = struct.pack(">I", len(data)) + typ + data
        return out + struct.pack(">I", zlib.crc32(typ + data) & 0xffffffff)

    ihdr = struct.pack(">IIBBBBB", w, h, 8, 0, 0, 0, 0)
    png = (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", ihdr) +
           chunk(b"IDAT", zlib.compress(bytes(raw), 9)) + chunk(b"IEND", b""))
    with open(path, "wb") as fh:
        fh.write(png)


def render_one(board_name, layout_name, flip, invert, southpaw, size_override, out_path):
    _, cfg = load_config(board_name)
    if not cfg.has_display:
        sys.exit("board '%s' does not define HAS_I2C_DISPLAY" % board_name)
    w, h = cfg.size
    if size_override:
        w, h = (128, 32) if size_override == 2 else (128, 64)
    elements, resolved, layout_warnings = resolve_layout(cfg, layout_name)
    if southpaw:
        mirror_horizontally(elements, w)
    warnings = layout_warnings + check_layout(w, h, elements)
    show_footer = bool(cfg.defines.get("DISPLAY_INPUT_HISTORY", 1)) and h >= 64
    raw = render(elements, w, h, bool(flip), bool(invert), show_footer)
    write_png(out_path, w, h, raw)
    print("wrote %s (%dx%d, %s, %d elements)" % (out_path, w, h,
                                                 LAYOUT_DISPLAY_NAMES.get(resolved, resolved),
                                                 len(elements)))
    for warning in warnings:
        print("warning: %s" % warning)


def main():
    parser = argparse.ArgumentParser(
        description="Preview an OLED display layout without flashing.")
    parser.add_argument("board", nargs="?", default=None,
                        help="board directory under configs/ (default: first board with a display)")
    parser.add_argument("--board", dest="board_flag", default=None,
                        help="same as the positional board argument")
    parser.add_argument("--port", type=int, default=8090,
                        help="port for the preview server (default: 8090)")
    parser.add_argument("--render", metavar="PNG",
                        help="render one frame to a PNG file and exit")
    parser.add_argument("--layout", default=None,
                        choices=["board", "stick", "stickless", "vlx", "fightboard"],
                        help="button layout to preview (default: from DISPLAY_BUTTON_LAYOUT)")
    parser.add_argument("--flip", action="store_true",
                        help="rotate the render 180 degrees (DISPLAY_FLIP)")
    parser.add_argument("--invert", action="store_true",
                        help="invert the monochrome colors (DISPLAY_INVERT)")
    parser.add_argument("--southpaw", action="store_true",
                        help="mirror the layout horizontally")
    parser.add_argument("--size", type=int, choices=[2, 3], default=None,
                        help="override panel size (2=128x32, 3=128x64)")
    args = parser.parse_args()

    board_name = args.board_flag or args.board
    boards = find_boards()
    if board_name is None:
        board_name = next((b["name"] for b in boards if b["hasDisplay"]),
                          boards[0]["name"] if boards else None)
    if board_name is None:
        sys.exit("no boards found under %s" % CONFIGS)
    if not os.path.isfile(os.path.join(CONFIGS, board_name, "BoardConfig.h")):
        sys.exit("no configs/%s/BoardConfig.h" % board_name)

    if args.render:
        render_one(board_name, args.layout, args.flip, args.invert,
                   args.southpaw, args.size, args.render)
        return

    preview = Preview(board_name)
    Handler.preview = preview
    server = ThreadingHTTPServer(("127.0.0.1", args.port), Handler)
    print("display preview: http://localhost:%d (board: %s)" % (args.port, board_name))
    print("edit configs/%s/BoardConfig.h and save to update live" % board_name)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        server.shutdown()


if __name__ == "__main__":
    main()