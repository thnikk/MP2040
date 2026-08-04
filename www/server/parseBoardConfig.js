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
  return 0;
}

function parseModifier(raw) {
  const val = String(raw || '').trim().replace(/;$/, '');
  if (/^\d+$/.test(val)) return parseInt(val, 10);
  if (MODIFIER_MASK[val] !== undefined) return MODIFIER_MASK[val];
  return 0;
}

export function parseBoardConfig(configDir, rootDir) {
  const boardConfigPath = path.join(rootDir, 'configs', configDir, 'BoardConfig.h');
  if (!fs.existsSync(boardConfigPath)) return null;

  const content = fs.readFileSync(boardConfigPath, 'utf8');
  const d = extractDefines(content);

  const keycodes = [];
  const modifierMasks = [];
  const pinLedIndices = [];
  for (let i = 0; i < 30; i++) {
    const n = i.toString().padStart(2, '0');
    keycodes.push(parseKeycode(d[`KEYCODE_GP${n}`]));
    modifierMasks.push(parseModifier(d[`MODIFIER_GP${n}`]));
    const ledIdx = parseNum(d[`LED_INDEX_GP${n}`]);
    pinLedIndices.push(ledIdx === undefined ? -1 : ledIdx);
  }

  let boardConfigLabel = configDir;
  if (d.BOARD_CONFIG_LABEL !== undefined) {
    const m = String(d.BOARD_CONFIG_LABEL).match(/"([^"]*)"/);
    if (m) boardConfigLabel = m[1];
  }

  const svgPath = path.join(rootDir, 'configs', configDir, 'board.svg');

  return {
    boardConfigLabel,
    configDir,
    svgPath: fs.existsSync(svgPath) ? svgPath : null,
    keycodes,
    modifierMasks,
    pinLedIndices,
    led: {
      dataPin: parseNum(d.LED_PIN) ?? -1,
      ledFormat: LED_FORMAT_MAP[d.LED_FORMAT] ?? 0,
      ledsPerKey: parseNum(d.LEDS_PER_KEY) ?? 1,
      ledCount: parseNum(d.LED_COUNT) ?? 0,
      brightnessMaximum: parseNum(d.LED_BRIGHTNESS_MAX) ?? 255,
      brightnessSteps: parseNum(d.LED_BRIGHTNESS_STEPS) ?? 1,
      colorNormal: parseColor(d.LED_COLOR_NORMAL) ?? 0x00ff00,
      colorPressed: parseColor(d.LED_COLOR_PRESSED) ?? 0xffffff,
    },
    webConfigPin: parseNum(d.PIN_WEBCONFIG) ?? -1,
  };
}
