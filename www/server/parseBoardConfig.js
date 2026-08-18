// Parse MP2040 board configs (configs/<Board>/BoardConfig.h) so the mock dev
// server can serve board-specific options, mirroring what the firmware's
// getOptions returns.

import fs from 'fs';
import path from 'path';

// ---- HID keycode map (matches tinyusb's class/hid/hid.h) -----------------

const HID = {};
for (let i = 0; i < 26; i++) HID[`HID_KEY_${String.fromCharCode(0x41 + i)}`] = 0x04 + i;
for (let i = 1; i <= 9; i++) HID[`HID_KEY_${i}`] = 0x1e + i - 1;
HID.HID_KEY_0 = 0x27;
for (let i = 1; i <= 24; i++) HID[`HID_KEY_F${i}`] = i <= 12 ? 0x3a + i - 1 : 0x68 + i - 13;
for (let i = 1; i <= 9; i++) HID[`HID_KEY_KP_${i}`] = 0x59 + i - 1;
HID.HID_KEY_KP_0 = 0x62;
Object.assign(HID, {
  HID_KEY_ENTER: 0x28, HID_KEY_ESCAPE: 0x29, HID_KEY_BACKSPACE: 0x2a, HID_KEY_TAB: 0x2b,
  HID_KEY_SPACE: 0x2c, HID_KEY_MINUS: 0x2d, HID_KEY_EQUAL: 0x2e, HID_KEY_BRACKET_LEFT: 0x2f,
  HID_KEY_BRACKET_RIGHT: 0x30, HID_KEY_BACKSLASH: 0x31, HID_KEY_INT_HASH: 0x32,
  HID_KEY_SEMICOLON: 0x33, HID_KEY_QUOTE: 0x34, HID_KEY_GRAVE: 0x35, HID_KEY_COMMA: 0x36,
  HID_KEY_PERIOD: 0x37, HID_KEY_SLASH: 0x38, HID_KEY_CAPS_LOCK: 0x39,
  HID_KEY_PRINT_SCREEN: 0x46, HID_KEY_SCROLL_LOCK: 0x47, HID_KEY_PAUSE: 0x48,
  HID_KEY_INSERT: 0x49, HID_KEY_HOME: 0x4a, HID_KEY_PAGE_UP: 0x4b, HID_KEY_DELETE: 0x4c,
  HID_KEY_END: 0x4d, HID_KEY_PAGE_DOWN: 0x4e,
  HID_KEY_RIGHT: 0x4f, HID_KEY_LEFT: 0x50, HID_KEY_DOWN: 0x51, HID_KEY_UP: 0x52,
  HID_KEY_ARROW_RIGHT: 0x4f, HID_KEY_ARROW_LEFT: 0x50, HID_KEY_ARROW_DOWN: 0x51,
  HID_KEY_ARROW_UP: 0x52,
  HID_KEY_NUM_LOCK: 0x53, HID_KEY_KP_DIVIDE: 0x54, HID_KEY_KP_MULTIPLY: 0x55,
  HID_KEY_KP_SUBTRACT: 0x56, HID_KEY_KP_ADD: 0x57, HID_KEY_KP_ENTER: 0x58,
  HID_KEY_KP_PERIOD: 0x63, HID_KEY_INT_BACKSLASH: 0x64, HID_KEY_APPLICATION: 0x65,
  HID_KEY_POWER: 0x66, HID_KEY_KP_EQUAL: 0x67,
  HID_KEY_CONTROL_LEFT: 0xe0, HID_KEY_SHIFT_LEFT: 0xe1, HID_KEY_ALT_LEFT: 0xe2,
  HID_KEY_GUI_LEFT: 0xe3, HID_KEY_CONTROL_RIGHT: 0xe4, HID_KEY_SHIFT_RIGHT: 0xe5,
  HID_KEY_ALT_RIGHT: 0xe6, HID_KEY_GUI_RIGHT: 0xe7,
});

// Media keys (consumer usage), reported on the multimedia report
const MEDIA = {
  KEYBOARD_MULTIMEDIA_NEXT_TRACK: 0xe8,
  KEYBOARD_MULTIMEDIA_PREV_TRACK: 0xe9,
  KEYBOARD_MULTIMEDIA_STOP: 0xf0,
  KEYBOARD_MULTIMEDIA_PLAY_PAUSE: 0xf1,
  KEYBOARD_MULTIMEDIA_MUTE: 0xf2,
  KEYBOARD_MULTIMEDIA_VOLUME_UP: 0xf3,
  KEYBOARD_MULTIMEDIA_VOLUME_DOWN: 0xf4,
};

// Mouse buttons (custom keycodes, reported on the mouse report)
const MOUSE = {
  MOUSE_BUTTON_LEFT: 0xf5,
  MOUSE_BUTTON_RIGHT: 0xf6,
  MOUSE_BUTTON_MIDDLE: 0xf7,
  MOUSE_BUTTON_BACK: 0xf8,
  MOUSE_BUTTON_FORWARD: 0xf9,
};

// Modifier mask bits (KEYBOARD_MODIFIER_* from tinyusb)
const MODIFIER_MASK = {
  KEYBOARD_MODIFIER_LEFTCTRL: 0x01,
  KEYBOARD_MODIFIER_LEFTSHIFT: 0x02,
  KEYBOARD_MODIFIER_LEFTALT: 0x04,
  KEYBOARD_MODIFIER_LEFTGUI: 0x08,
  KEYBOARD_MODIFIER_RIGHTCTRL: 0x10,
  KEYBOARD_MODIFIER_RIGHTSHIFT: 0x20,
  KEYBOARD_MODIFIER_RIGHTALT: 0x40,
  KEYBOARD_MODIFIER_RIGHTGUI: 0x80,
};

// Gamepad per-pin control mask bits (GAMEPAD_PIN_MASK_* from
// headers/gamepadmapping.h). A BoardConfig.h GAMEPAD_GPxx / GAMEPAD_IDXxx
// define can be a single constant or an OR'd combination of several.
const GAMEPAD_PIN_MASK = {
  GAMEPAD_PIN_MASK_UP: 1 << 0,
  GAMEPAD_PIN_MASK_DOWN: 1 << 1,
  GAMEPAD_PIN_MASK_LEFT: 1 << 2,
  GAMEPAD_PIN_MASK_RIGHT: 1 << 3,
  GAMEPAD_PIN_MASK_B1: 1 << 4,
  GAMEPAD_PIN_MASK_B2: 1 << 5,
  GAMEPAD_PIN_MASK_B3: 1 << 6,
  GAMEPAD_PIN_MASK_B4: 1 << 7,
  GAMEPAD_PIN_MASK_L1: 1 << 8,
  GAMEPAD_PIN_MASK_R1: 1 << 9,
  GAMEPAD_PIN_MASK_L2: 1 << 10,
  GAMEPAD_PIN_MASK_R2: 1 << 11,
  GAMEPAD_PIN_MASK_S1: 1 << 12,
  GAMEPAD_PIN_MASK_S2: 1 << 13,
  GAMEPAD_PIN_MASK_L3: 1 << 14,
  GAMEPAD_PIN_MASK_R3: 1 << 15,
  GAMEPAD_PIN_MASK_A1: 1 << 16,
  GAMEPAD_PIN_MASK_A2: 1 << 17,
};

// MP2040's enums.proto LEDFormat values
const LED_FORMAT_MAP = {
  LED_FORMAT_RGB: 0,
  LED_FORMAT_GRB: 1,
  LED_FORMAT_GRBW: 2,
  LED_FORMAT_RGBW: 3,
};

export function findBoardConfigDir(boardId, rootDir) {
  const boardIdLower = boardId.toLowerCase();
  const envConfig = process.env.MP2040_BOARDCONFIG;
  if (envConfig && envConfig.toLowerCase() === boardIdLower) {
    const dir = path.join(rootDir, 'configs', envConfig);
    if (fs.existsSync(dir)) return envConfig;
  }
  const configsDir = path.join(rootDir, 'configs');
  if (!fs.existsSync(configsDir)) return null;
  for (const entry of fs.readdirSync(configsDir, { withFileTypes: true })) {
    if (entry.isDirectory() && entry.name.toLowerCase() === boardIdLower)
      return entry.name;
  }
  return null;
}

// Every board available to the mock: the configs/<Board> directories that
// contain a BoardConfig.h. Mirrors docker-build.py's board list.
export function listBoardConfigs(rootDir) {
  const configsDir = path.join(rootDir, 'configs');
  if (!fs.existsSync(configsDir)) return [];
  const boards = [];
  for (const entry of fs.readdirSync(configsDir, { withFileTypes: true })) {
    if (!entry.isDirectory()) continue;
    const boardConfigPath = path.join(configsDir, entry.name, 'BoardConfig.h');
    if (!fs.existsSync(boardConfigPath)) continue;
    let label = entry.name;
    try {
      const m = fs.readFileSync(boardConfigPath, 'utf8').match(/BOARD_CONFIG_LABEL\s+"([^"]*)"/);
      if (m) label = m[1];
    } catch {
      // fall through to the directory name
    }
    boards.push({ id: entry.name, label });
  }
  boards.sort((a, b) => a.id.localeCompare(b.id));
  return boards;
}

function extractDefines(content) {
  const defines = {};
  const cleaned = content.replace(/\/\*[\s\S]*?\*\//g, '');
  const lines = cleaned.split('\n').filter((l) => !l.trim().startsWith('//'));
  const joined = lines.join('\n').replace(/\\\n\s*/g, '');
  for (const line of joined.split('\n')) {
    const match = line.match(/^#define\s+(\w+)(?:\s+(.*))?$/);
    if (match) {
      let val = (match[2] || '').trim();
      const commentIdx = val.indexOf('//');
      if (commentIdx >= 0) val = val.substring(0, commentIdx).trim();
      defines[match[1]] = val || true;
    }
  }
  return defines;
}

function parseNum(raw) {
  if (raw === undefined || raw === true) return undefined;
  const n = parseInt(String(raw).trim(), 10);
  return Number.isNaN(n) ? undefined : n;
}

function parseColor(raw) {
  const val = String(raw || '').trim();
  if (/^0x[0-9a-fA-F]+$/.test(val)) return parseInt(val, 16);
  const n = parseInt(val, 10);
  return Number.isNaN(n) ? undefined : n;
}

function parseKeycode(raw) {
  const val = String(raw || '').trim().replace(/;$/, '');
  if (/^-?\d+$/.test(val)) return parseInt(val, 10);
  if (HID[val] !== undefined) return HID[val];
  if (MEDIA[val] !== undefined) return MEDIA[val];
  if (MOUSE[val] !== undefined) return MOUSE[val];
  return 0;
}

function parseModifier(raw) {
  const val = String(raw || '').trim().replace(/;$/, '');
  if (/^\d+$/.test(val)) return parseInt(val, 10);
  if (MODIFIER_MASK[val] !== undefined) return MODIFIER_MASK[val];
  return 0;
}

// Parse a GAMEPAD_GPxx / GAMEPAD_IDXxx define into a control mask. The value
// is a GAMEPAD_PIN_MASK_* constant or an OR'd combination of several (e.g.
// "GAMEPAD_PIN_MASK_B1 | GAMEPAD_PIN_MASK_B2"); GAMEPAD_UNMAPPED (-1) and
// unknown values map to 0 (unmapped in the web config's representation).
function parseGamepadMask(raw) {
  const val = String(raw || '').trim().replace(/;$/, '').trim();
  if (val === '' || val === 'GAMEPAD_UNMAPPED') return 0;
  if (/^-?\d+$/.test(val)) return Math.max(0, parseInt(val, 10));
  let mask = 0;
  // Split on the OR operator, resolving each GAMEPAD_PIN_MASK_* token.
  for (const part of val.split('|')) {
    const token = part.trim();
    if (GAMEPAD_PIN_MASK[token] !== undefined) mask |= GAMEPAD_PIN_MASK[token];
  }
  return mask;
}

// Parse a brace-list define like { 26, 27, 28, 29 } into an array of numbers.
function parsePinArray(raw) {
  const val = String(raw || '').trim().replace(/;$/, '').trim();
  const m = val.match(/^\{(.*)\}$/s);
  if (!m) return [];
  return m[1]
    .split(',')
    .map((s) => s.trim())
    .filter((s) => s.length > 0)
    .map((s) => parseInt(s, 10))
    .filter((n) => !Number.isNaN(n));
}

// Per-mode LED defaults mirroring firmware: LED_<SETTING>_MODE_<NAME> for each
// of the 7 modes, falling back to the single global default per entry.
const LED_MODE_NAMES = ['CUSTOM', 'CYCLE', 'REACTIVE', 'BPS', 'RIPPLE', 'RAIN', 'FIRE'];

function perModeDefaults(defines, setting, parse, fallback) {
  return LED_MODE_NAMES.map((m) => {
    const raw = defines[`${setting}_MODE_${m}`];
    const val = raw !== undefined ? parse(raw) : undefined;
    return val ?? fallback;
  });
}

export function parseBoardConfig(configDir, rootDir) {
  const boardConfigPath = path.join(rootDir, 'configs', configDir, 'BoardConfig.h');
  if (!fs.existsSync(boardConfigPath)) return null;

  const content = fs.readFileSync(boardConfigPath, 'utf8');
  const d = extractDefines(content);

  const keycodes = [];
  const modifierMasks = [];
  const pinLedIndices = [];
  const gamepadMasks = [];
  // Key index arrays are MAX_KEYS (128) long; matrix boards can use indices
  // beyond the GPIO count. Direct boards only use the first 30.
  for (let i = 0; i < 128; i++) {
    const n = i.toString().padStart(2, '0');
    // Matrix boards define keys by linear index (KEYCODE_IDXxx); direct boards
    // by GPIO (KEYCODE_GPxx). Prefer IDX when present.
    keycodes.push(parseKeycode(d[`KEYCODE_IDX${n}`] ?? d[`KEYCODE_GP${n}`]));
    modifierMasks.push(parseModifier(d[`MODIFIER_IDX${n}`] ?? d[`MODIFIER_GP${n}`]));
    const ledIdx = parseNum(d[`LED_INDEX_IDX${n}`] ?? d[`LED_INDEX_GP${n}`]);
    pinLedIndices.push(ledIdx === undefined ? -1 : ledIdx);
    // Gamepad default mapping, like the keyboard defaults: GAMEPAD_IDXxx
    // (matrix) or GAMEPAD_GPxx (direct), IDX preferred when present.
    gamepadMasks.push(parseGamepadMask(d[`GAMEPAD_IDX${n}`] ?? d[`GAMEPAD_GP${n}`]));
  }

  let boardConfigLabel = configDir;
  if (d.BOARD_CONFIG_LABEL !== undefined) {
    const m = String(d.BOARD_CONFIG_LABEL).match(/"([^"]*)"/);
    if (m) boardConfigLabel = m[1];
  }

  const svgPath = path.join(rootDir, 'configs', configDir, 'board.svg');

  // Matrix input mode (rows/cols > 0). In matrix mode the keycode / LED index
  // arrays are indexed by linear matrix key (row * MATRIX_COLS + col), not GPIO.
  const matrixRows = parseNum(d.MATRIX_ROWS) ?? 0;
  const matrixCols = parseNum(d.MATRIX_COLS) ?? 0;

  return {
    boardConfigLabel,
    configDir,
    svgPath: fs.existsSync(svgPath) ? svgPath : null,
    keycodes,
    modifierMasks,
    pinLedIndices,
    gamepadMasks,
    matrix: {
      enabled: matrixRows > 0 && matrixCols > 0,
      rows: matrixRows,
      cols: matrixCols,
      rowPins: parsePinArray(d.MATRIX_ROW_PINS),
      colPins: parsePinArray(d.MATRIX_COL_PINS),
      activeHigh: (parseNum(d.MATRIX_ACTIVE_HIGH) ?? 0) === 1,
    },
    led: {
      dataPin: parseNum(d.LED_PIN) ?? -1,
      ledFormat: LED_FORMAT_MAP[d.LED_FORMAT] ?? 0,
      ledsPerKey: parseNum(d.LEDS_PER_KEY) ?? 1,
      ledCount: parseNum(d.LED_COUNT) ?? 0,
      ledMode: parseNum(d.LED_MODE) ?? 0,
      ledSpeed: parseNum(d.LED_SPEED) ?? 50,
      ledSpeeds: perModeDefaults(d, 'LED_SPEED', parseNum, parseNum(d.LED_SPEED) ?? 50),
      ledTimeout: parseNum(d.LED_TIMEOUT) ?? 0,
      hasStatusLed: (() => {
        const pin = parseNum(d.STATUS_LED_PIN);
        return pin !== undefined && pin >= 0;
      })(),
      brightnessMaximum: parseNum(d.LED_BRIGHTNESS_DEFAULT) ?? 255,
      brightnessByMode: perModeDefaults(d, 'LED_BRIGHTNESS', parseNum, parseNum(d.LED_BRIGHTNESS_DEFAULT) ?? 255),
      colorNormal: parseColor(d.LED_COLOR_NORMAL) ?? 0x00ff00,
      colorPressed: parseColor(d.LED_COLOR_PRESSED) ?? 0xffffff,
      colorNormalByMode: perModeDefaults(d, 'LED_COLOR_NORMAL', parseColor, parseColor(d.LED_COLOR_NORMAL) ?? 0x00ff00),
      colorPressedByMode: perModeDefaults(d, 'LED_COLOR_PRESSED', parseColor, parseColor(d.LED_COLOR_PRESSED) ?? 0xffffff),
    },
    webConfigPin: parseNum(d.PIN_WEBCONFIG) ?? -1,
    // Number of keys the board can report, mirroring firmware getKeyCount():
    // matrix boards report rows*cols, direct boards report all bank-0 GPIOs.
    keyCount: matrixRows > 0 && matrixCols > 0 ? matrixRows * matrixCols : 30,
  };
}
