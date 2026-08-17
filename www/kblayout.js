// kblayout.js — shared QMK-style keyboard layout data and label helpers.
// Used by the key picker (keyboardwidget.js), the macro builder
// (macrobuilder.js) and the board view labels (boardview.js). Loaded as a
// plain script before those, so the top-level bindings are shared globals.

const KB_MODIFIER_MIN = 0xe0;
const KB_MODIFIER_MAX = 0xe7;

const kbIsModifier = (v) => v >= KB_MODIFIER_MIN && v <= KB_MODIFIER_MAX;

const KB_MAIN_ROWS = [
  [
    { label: '', value: 0, size: '1u', spacer: true },
    { label: '', value: 0, flex: true, spacer: true },
    { label: 'F13', value: 0x68 },
    { label: 'F14', value: 0x69 },
    { label: 'F15', value: 0x6a },
    { label: 'F16', value: 0x6b },
    { label: '', value: 0, flex: true, spacer: true },
    { label: 'F17', value: 0x6c },
    { label: 'F18', value: 0x6d },
    { label: 'F19', value: 0x6e },
    { label: 'F20', value: 0x6f },
    { label: '', value: 0, flex: true, spacer: true },
    { label: 'F21', value: 0x70 },
    { label: 'F22', value: 0x71 },
    { label: 'F23', value: 0x72 },
    { label: 'F24', value: 0x73 },
  ],
  [
    { label: 'Esc', value: 0x29 },
    { label: '', value: 0, flex: true, spacer: true },
    { label: 'F1', value: 0x3a },
    { label: 'F2', value: 0x3b },
    { label: 'F3', value: 0x3c },
    { label: 'F4', value: 0x3d },
    { label: '', value: 0, flex: true, spacer: true },
    { label: 'F5', value: 0x3e },
    { label: 'F6', value: 0x3f },
    { label: 'F7', value: 0x40 },
    { label: 'F8', value: 0x41 },
    { label: '', value: 0, flex: true, spacer: true },
    { label: 'F9', value: 0x42 },
    { label: 'F10', value: 0x43 },
    { label: 'F11', value: 0x44 },
    { label: 'F12', value: 0x45 },
  ],
  [
    { label: '`', value: 0x35, sub: '~' },
    { label: '1', value: 0x1e, sub: '!' },
    { label: '2', value: 0x1f, sub: '@' },
    { label: '3', value: 0x20, sub: '#' },
    { label: '4', value: 0x21, sub: '$' },
    { label: '5', value: 0x22, sub: '%' },
    { label: '6', value: 0x23, sub: '^' },
    { label: '7', value: 0x24, sub: '&' },
    { label: '8', value: 0x25, sub: '*' },
    { label: '9', value: 0x26, sub: '(' },
    { label: '0', value: 0x27, sub: ')' },
    { label: '-', value: 0x2d, sub: '_' },
    { label: '=', value: 0x2e, sub: '+' },
    { label: 'Backspace', value: 0x2a, size: '2u' },
  ],
  [
    { label: 'Tab', value: 0x2b, size: '1.5u' },
    { label: 'Q', value: 0x14 },
    { label: 'W', value: 0x1a },
    { label: 'E', value: 0x08 },
    { label: 'R', value: 0x15 },
    { label: 'T', value: 0x17 },
    { label: 'Y', value: 0x1c },
    { label: 'U', value: 0x18 },
    { label: 'I', value: 0x0c },
    { label: 'O', value: 0x12 },
    { label: 'P', value: 0x13 },
    { label: '[', value: 0x2f, sub: '{' },
    { label: ']', value: 0x30, sub: '}' },
    { label: '\\', value: 0x31, size: '1.5u', sub: '|' },
  ],
  [
    { label: 'CapsLock', value: 0x39, size: '1.75u' },
    { label: 'A', value: 0x04 },
    { label: 'S', value: 0x16 },
    { label: 'D', value: 0x07 },
    { label: 'F', value: 0x09 },
    { label: 'G', value: 0x0a },
    { label: 'H', value: 0x0b },
    { label: 'J', value: 0x0d },
    { label: 'K', value: 0x0e },
    { label: 'L', value: 0x0f },
    { label: ';', value: 0x33, sub: ':' },
    { label: "'", value: 0x34, sub: '"' },
    { label: 'Enter', value: 0x28, size: '2.25u' },
  ],
  [
    { label: 'Shift', value: 0xe1, size: '2.25u' },
    { label: 'Z', value: 0x1d },
    { label: 'X', value: 0x1b },
    { label: 'C', value: 0x06 },
    { label: 'V', value: 0x19 },
    { label: 'B', value: 0x05 },
    { label: 'N', value: 0x11 },
    { label: 'M', value: 0x10 },
    { label: ',', value: 0x36, sub: '<' },
    { label: '.', value: 0x37, sub: '>' },
    { label: '/', value: 0x38, sub: '?' },
    { label: 'Shift', value: 0xe5, size: '2.75u' },
  ],
  [
    { label: 'Ctrl', value: 0xe0, size: '1.75u' },
    { label: 'Win', value: 0xe3, size: '1u' },
    { label: 'Alt', value: 0xe2, size: '1u' },
    { label: 'Space', value: 0x2c, size: '6.25u' },
    { label: 'Alt', value: 0xe6, size: '1.5u' },
    { label: 'Win', value: 0xe7, size: '1u' },
    { label: 'Menu', value: 0x76, size: '1u' },
    { label: 'Ctrl', value: 0xe4, size: '1.75u' },
  ],
];

const KB_EXTRA_CLUSTERS = [
  {
    label: 'Navigation',
    keys: [
      [
        { label: 'Ins', value: 0x49 },
        { label: 'Home', value: 0x4a },
        { label: 'PgUp', value: 0x4b },
      ],
      [
        { label: 'Del', value: 0x4c },
        { label: 'End', value: 0x4d },
        { label: 'PgDn', value: 0x4e },
      ],
    ],
  },
  {
    label: 'Arrows',
    keys: [
      [
        { label: '', value: 0, spacer: true },
        { label: '\u2191', value: 0x52 },
        { label: '', value: 0, spacer: true },
      ],
      [
        { label: '\u2190', value: 0x50 },
        { label: '\u2193', value: 0x51 },
        { label: '\u2192', value: 0x4f },
      ],
    ],
  },
  {
    label: 'Media',
    keys: [
      [
        { label: 'Mute', value: 0xf2 },
        { label: 'Vol-', value: 0xf4 },
        { label: 'Vol+', value: 0xf3 },
        { label: '', value: 0, spacer: true },
      ],
      [
        { label: 'Prev', value: 0xe9 },
        { label: 'Play', value: 0xf1 },
        { label: 'Next', value: 0xe8 },
        { label: 'Stop', value: 0xf0 },
      ],
    ],
  },
  {
    label: 'Mouse',
    keys: [
      [
        { label: 'LMB', value: 0xf5 },
        { label: 'MMB', value: 0xf7 },
        { label: 'RMB', value: 0xf6 },
      ],
      [
        { label: 'Back', value: 0xf8 },
        { label: '', value: 0, spacer: true },
        { label: 'Fwd', value: 0xf9 },
      ],
    ],
  },
];

const kbSizeClass = (s) => (s ? 'sz-' + s.replace('.', '_') : 'sz-1u');

// Modifier mask bit -> short label (bits are KEYBOARD_MODIFIER_* order).
const MODIFIER_SHORT = {
  0xe0: 'Ctrl', 0xe1: 'Shift', 0xe2: 'Alt', 0xe3: 'Win',
  0xe4: 'Ctrl', 0xe5: 'Shift', 0xe6: 'Alt', 0xe7: 'Win',
};

const KEY_LABELS = {
  0x28: 'Enter', 0x29: 'Esc', 0x2a: 'Bksp', 0x2b: 'Tab', 0x2c: 'Space',
  0x2d: '-', 0x2e: '=', 0x2f: '[', 0x30: ']', 0x31: '\\', 0x32: '\\',
  0x33: ';', 0x34: "'", 0x35: '`', 0x36: ',', 0x37: '.', 0x38: '/', 0x39: 'Caps',
  0x46: 'PrtSc', 0x47: 'ScrLk', 0x48: 'Pause', 0x49: 'Ins', 0x4a: 'Home',
  0x4b: 'PgUp', 0x4c: 'Del', 0x4d: 'End', 0x4e: 'PgDn',
  0x4f: '\u2192', 0x50: '\u2190', 0x51: '\u2193', 0x52: '\u2191',
  0x53: 'NumLk', 0x54: 'KP/', 0x55: 'KP*', 0x56: 'KP-', 0x57: 'KP+', 0x58: 'KPEnt',
  0x62: 'KP0', 0x63: 'KP.', 0x64: 'Intl\\', 0x65: 'Menu', 0x66: 'Power', 0x67: 'KP=',
  0x74: 'Execute', 0x75: 'Help', 0x76: 'Menu', 0x77: 'Select', 0x78: 'Stop',
  0x79: 'Again', 0x7a: 'Undo', 0x7b: 'Cut', 0x7c: 'Copy', 0x7d: 'Paste', 0x7e: 'Find',
  0x86: 'Compose',
  0x7f: 'Mute', 0x80: 'Vol+', 0x81: 'Vol-',
  0xe8: 'Next', 0xe9: 'Prev', 0xf0: 'Stop', 0xf1: 'Play',
  0xf2: 'Mute', 0xf3: 'Vol+', 0xf4: 'Vol-',
  0xf5: 'LMB', 0xf6: 'RMB', 0xf7: 'MMB', 0xf8: 'Back', 0xf9: 'Fwd',
};

// Labels for the per-pin gamepad control mask (KeyMapping.gamepadMasks).
// Bit values match GAMEPAD_PIN_MASK_* in gamepadhelper.h: dpad in bits 0-3,
// buttons B1-A2 in bits 4-17.
const GAMEPAD_MASK_LABELS = {
  0x0001: '\u2191', 0x0002: '\u2193', 0x0004: '\u2190', 0x0008: '\u2192',
  0x0010: 'B1', 0x0020: 'B2', 0x0040: 'B3', 0x0080: 'B4',
  0x0100: 'L1', 0x0200: 'R1', 0x0400: 'L2', 0x0800: 'R2',
  0x1000: 'S1', 0x2000: 'S2', 0x4000: 'L3', 0x8000: 'R3',
  0x10000: 'A1', 0x20000: 'A2',
};

// Labels for the controls set in a per-pin gamepad mask, in bit order.
function gamepadMaskLabels(mask) {
  return Object.entries(GAMEPAD_MASK_LABELS)
    .filter(([bit]) => mask & Number(bit))
    .map(([, label]) => label);
}

// Human label for a keycode (single key). '' for unassigned/unknown codes.
function keyLabel(code) {
  if (code >= 0x04 && code <= 0x1d) return String.fromCharCode(0x41 + code - 0x04); // A-Z
  if (code === 0x27) return '0';
  if (code >= 0x1e && code <= 0x26) return String.fromCharCode(0x31 + code - 0x1e); // 1-9
  if (code >= 0x3a && code <= 0x45) return 'F' + (code - 0x3a + 1); // F1-F12
  if (code >= 0x68 && code <= 0x73) return 'F' + (code - 0x68 + 13); // F13-F24
  if (code >= 0x59 && code <= 0x61) return 'KP' + (code - 0x59 + 1); // KP1-KP9
  return KEY_LABELS[code] || '';
}

// Compact label for a step: modifiers (+/- for clarity) then the keycode,
// e.g. "Ctrl+A". keycode 0 is just the modifiers (a bare modifier hold).
function stepLabel(step) {
  const mods = [];
  for (let i = 0; i < 8; i++) {
    if (step.modifiers & (1 << i)) mods.push(MODIFIER_SHORT[0xe0 + i] || '');
  }
  const key = step.keycode ? keyLabel(step.keycode) : '';
  if (!key) return mods.join('+');
  return mods.length ? mods.join('+') + '+' + key : key;
}
