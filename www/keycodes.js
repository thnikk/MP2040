// keycodes.js — key/modifier/gamepad option data and picker builders.
// Pure data plus small functions shared by the key modal and the gamepad
// widget (see gamepadlabels.js, loaded before this). Plain script:
// top-level bindings are shared globals.

// Minimal USB HID keycode table for the configurator.
// Values are tinyusb HID_KEY_* codes (0x00 - 0xFF).
const KEYCODES = {
  none: 0,
  a: 0x04, b: 0x05, c: 0x06, d: 0x07, e: 0x08, f: 0x09, g: 0x0a, h: 0x0b,
  i: 0x0c, j: 0x0d, k: 0x0e, l: 0x0f, m: 0x10, n: 0x11, o: 0x12, p: 0x13,
  q: 0x14, r: 0x15, s: 0x16, t: 0x17, u: 0x18, v: 0x19, w: 0x1a, x: 0x1b,
  y: 0x1c, z: 0x1d,
  1: 0x1e, 2: 0x1f, 3: 0x20, 4: 0x21, 5: 0x22, 6: 0x23, 7: 0x24, 8: 0x25,
  9: 0x26, 0: 0x27,
  enter: 0x28, escape: 0x29, backspace: 0x2a, tab: 0x2b, space: 0x2c,
  minus: 0x2d, equal: 0x2e, bracketleft: 0x2f, bracketright: 0x30,
  backslash: 0x31, intlro: 0x32, semicolon: 0x33, quote: 0x34, grave: 0x35,
  comma: 0x36, period: 0x37, slash: 0x38, capslock: 0x39,
  f1: 0x3a, f2: 0x3b, f3: 0x3c, f4: 0x3d, f5: 0x3e, f6: 0x3f,
  f7: 0x40, f8: 0x41, f9: 0x42, f10: 0x43, f11: 0x44, f12: 0x45,
  printscreen: 0x46, scrolllock: 0x47, pause: 0x48, insert: 0x49, home: 0x4a,
  pageup: 0x4b, delete: 0x4c, end: 0x4d, pagedown: 0x4e, right: 0x4f,
  left: 0x50, down: 0x51, up: 0x52,
  numlock: 0x53, kpdivide: 0x54, kpmultiply: 0x55, kpminus: 0x56,
  kpplus: 0x57, kpenter: 0x58, kp1: 0x59, kp2: 0x5a, kp3: 0x5b,
  kp4: 0x5c, kp5: 0x5d, kp6: 0x5e, kp7: 0x5f, kp8: 0x60, kp9: 0x61,
  kp0: 0x62, kpperiod: 0x63, intlbackslash: 0x64, application: 0x65,
  power: 0x66, kpequal: 0x67, f13: 0x68, f14: 0x69, f15: 0x6a, f16: 0x6b,
  f17: 0x6c, f18: 0x6d, f19: 0x6e, f20: 0x6f, f21: 0x70, f22: 0x71,
  f23: 0x72, f24: 0x73,
  execute: 0x74, help: 0x75, menu: 0x76, select: 0x77, stop: 0x78,
  again: 0x79, undo: 0x7a, cut: 0x7b, copy: 0x7c, paste: 0x7d, find: 0x7e,
  mute: 0x7f, volumeup: 0x80, volumedown: 0x81,
  capscompose: 0x86,
  leftctrl: 0xe0, leftshift: 0xe1, leftalt: 0xe2, leftgui: 0xe3,
  rightctrl: 0xe4, rightshift: 0xe5, rightalt: 0xe6, rightgui: 0xe7,
  // Multimedia (report 2)
  media_next_track: 0xe8, media_prev_track: 0xe9, media_stop: 0xf0,
  media_play_pause: 0xf1, media_mute: 0xf2, media_volume_up: 0xf3,
  media_volume_down: 0xf4,
  // Mouse buttons (report 3, sent alongside the keyboard)
  mouse_left: 0xf5, mouse_right: 0xf6, mouse_middle: 0xf7,
  mouse_back: 0xf8, mouse_forward: 0xf9,
};

// Modifier mask bits (KEYBOARD_MODIFIER_*). A pin can hold several at once.
const MODIFIERS = {
  leftctrl: 1, leftshift: 2, leftalt: 4, leftgui: 8,
  rightctrl: 16, rightshift: 32, rightalt: 64, rightgui: 128,
};

// MultiSelect options: Modifiers group (multiple allowed), Keys group (at
// most one) and Macros group (at most one, mutually exclusive with Keys).
// Modifier keycodes (0xE0-0xE7) are handled by the Modifiers group, so
// they're excluded from Keys; "none" (0) is implicit as empty.
const MULTISELECT_GROUPS = [
  { id: 'modifiers', label: 'Modifiers' },
  { id: 'keys', label: 'Keys' },
  { id: 'macros', label: 'Macros' },
];

const MULTISELECT_OPTIONS = [
  ...Object.entries(MODIFIERS).map(([label, value]) => ({ group: 'modifiers', label, value })),
  ...Object.entries(KEYCODES)
    .filter(([, value]) => value !== 0 && (value < 0xe0 || value > 0xe7))
    .map(([label, value]) => ({ group: 'keys', label, value })),
  ...Array.from({ length: 8 }, (_, i) => ({ group: 'macros', label: 'M' + (i + 1), value: i + 1 })),
];

// Gamepad multi-select: every control is its own bit in a per-pin mask, so
// several can be picked at once (a pin fires them together). Values match the
// GAMEPAD_PIN_MASK_* defines in gamepadhelper.h.
const GAMEPAD_MULTISELECT_GROUPS = [
  { id: 'gamepad', label: 'Gamepad' },
];

// Gamepad controls in bit order. Labels are swapped per input mode (XInput →
// Xbox names, Switch Pro → Nintendo names) via labelSet in gamepadlabels.js.
const GAMEPAD_CONTROLS = [
  { label: 'Up', value: 0x0001 },
  { label: 'Down', value: 0x0002 },
  { label: 'Left', value: 0x0004 },
  { label: 'Right', value: 0x0008 },
  { label: 'B1', value: 0x0010 },
  { label: 'B2', value: 0x0020 },
  { label: 'B3', value: 0x0040 },
  { label: 'B4', value: 0x0080 },
  { label: 'L1', value: 0x0100 },
  { label: 'R1', value: 0x0200 },
  { label: 'L2', value: 0x0400 },
  { label: 'R2', value: 0x0800 },
  { label: 'S1', value: 0x1000 },
  { label: 'S2', value: 0x2000 },
  { label: 'L3', value: 0x4000 },
  { label: 'R3', value: 0x8000 },
  { label: 'A1', value: 0x10000 },
  { label: 'A2', value: 0x20000 },
];

// MultiSelect options for the gamepad picker, labeled per the active layout.
// `maskMap` (optional) remaps a control to a different stored bit so the
// displayed label still maps to the right function (see gamepadLabelSet).
function gamepadMultiOptions(labels, maskMap) {
  return GAMEPAD_CONTROLS.map(({ label, value }) => ({
    group: 'gamepad',
    label: labels[label] || label,
    value: (maskMap && maskMap[label]) || value,
  }));
}

// Label + glyph + bit-mask sets for a given input mode: XInput (3) shows Xbox
// names; Switch Pro (4) shows Nintendo names, always laid out like a real
// Switch Pro controller (A right, B bottom, Y left, X top). The Nintendo-layout
// toggle only swaps which stored position-bit each letter maps to (maskMap),
// so clicking a letter always maps the pin to that Switch button. Glyphs are
// the icon files the controller widget renders. Shared with the board view
// (see labelSet in gamepadlabels.js).
function gamepadLabelSet(mode, nintendoLayout) {
  return labelSet(mode, nintendoLayout);
}

// Re-label the gamepad pickers (multi-select + controller widget) for the
// current input mode and Nintendo-layout toggle.
function syncGamepadLabels() {
  const mode = Number(currentOptions.defaultInputMode || 1);
  const nintendo = currentOptions.gamepad?.useNintendoLayout === true;
  const { labels, glyphs, maskMap } = gamepadLabelSet(mode, nintendo);
  if (gamepadSelect) gamepadSelect.setOptions(gamepadMultiOptions(labels, maskMap));
  if (gamepadWidget) gamepadWidget.setLabels(labels, glyphs, maskMap);
}
