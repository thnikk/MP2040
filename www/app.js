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
};

// Modifier mask bits (KEYBOARD_MODIFIER_*). A pin can hold several at once.
const MODIFIERS = {
  leftctrl: 1, leftshift: 2, leftalt: 4, leftgui: 8,
  rightctrl: 16, rightshift: 32, rightalt: 64, rightgui: 128,
};

// MultiSelect options: one Modifiers group (multiple allowed) + one Keys group
// (at most one). Modifier keycodes (0xE0-0xE7) are handled by the Modifiers
// group, so they're excluded from Keys; "none" (0) is implicit as empty.
const MULTISELECT_GROUPS = [
  { id: 'modifiers', label: 'Modifiers' },
  { id: 'keys', label: 'Keys' },
];

const MULTISELECT_OPTIONS = [
  ...Object.entries(MODIFIERS).map(([label, value]) => ({ group: 'modifiers', label, value })),
  ...Object.entries(KEYCODES)
    .filter(([, value]) => value !== 0 && (value < 0xe0 || value > 0xe7))
    .map(([label, value]) => ({ group: 'keys', label, value })),
];

// Working copy of the config from /api/getOptions, edited via the modal
let currentOptions = null;

// Board SVG view (see boardview.js), initialized by load()
let boardView = null;

// MultiSelect used in the key modal
let modalSelect = null;

// Pin currently being edited in the modal
let editingPin = -1;

function colorToInt(hex) {
  return parseInt(hex.replace('#', ''), 16);
}

function intToColor(value) {
  return '#' + value.toString(16).padStart(6, '0');
}

async function api(path, options) {
  const res = await fetch(path, options);
  return res.json();
}

function setStatus(msg, ok = true) {
  const el = document.getElementById('status');
  el.textContent = msg;
  el.style.color = ok ? '#4caf50' : '#e57373';
}

async function load() {
  const [options, version] = await Promise.all([
    api('/api/getOptions'),
    api('/api/getFirmwareVersion'),
  ]);
  currentOptions = options;
  document.getElementById('board-label').textContent = version.boardLabel || '';
  document.getElementById('fw-version').textContent =
    `v${version.firmwareVersion || ''} (${version.gitCommit || ''})`;

  const led = options.led || {};
  document.getElementById('led-dataPin').value = led.dataPin ?? -1;
  document.getElementById('led-ledCount').value = led.ledCount ?? 0;
  document.getElementById('led-ledFormat').value = led.ledFormat ?? 0;
  document.getElementById('led-ledsPerKey').value = led.ledsPerKey ?? 1;
  document.getElementById('led-brightnessMaximum').value = led.brightnessMaximum ?? 255;
  document.getElementById('led-colorNormal').value = intToColor(led.colorNormal ?? 0x00ff00);
  document.getElementById('led-colorPressed').value = intToColor(led.colorPressed ?? 0xffffff);

  modalSelect = new MultiSelect({
    container: document.getElementById('key-modal-select'),
    options: MULTISELECT_OPTIONS,
    groups: MULTISELECT_GROUPS,
  });

  initBoard(options);
}

function initBoard(options) {
  const panel = document.getElementById('board-panel');
  if (!panel) return;

  boardView = new BoardView(panel, {
    onPinClick: (pin) => openKeyModal(pin),
    onTest: () => testLed(),
  });
  boardView.setOptions(options);
}

function openKeyModal(pin) {
  editingPin = pin;
  document.getElementById('key-modal-title').textContent =
    `GP${pin.toString().padStart(2, '0')}`;
  modalSelect.setValue(
    Number(currentOptions.keycodes[pin] || 0),
    Number(currentOptions.modifierMasks[pin] || 0),
  );
  document.getElementById('key-modal').hidden = false;
  if (boardView) boardView.highlightPin(pin);
}

function closeKeyModal() {
  document.getElementById('key-modal').hidden = true;
  editingPin = -1;
  if (boardView) boardView.clearHighlight();
}

function saveKeyModal() {
  if (editingPin < 0) return;
  const { keycode, mask } = modalSelect.getValue();
  currentOptions.keycodes[editingPin] = keycode;
  currentOptions.modifierMasks[editingPin] = mask;
  if (boardView) boardView.setOptions(currentOptions);
  closeKeyModal();
}

async function save() {
  const body = {
    keycodes: currentOptions.keycodes,
    modifierMasks: currentOptions.modifierMasks,
    led: {
      dataPin: parseInt(document.getElementById('led-dataPin').value, 10),
      ledCount: parseInt(document.getElementById('led-ledCount').value, 10),
      ledFormat: parseInt(document.getElementById('led-ledFormat').value, 10),
      ledsPerKey: parseInt(document.getElementById('led-ledsPerKey').value, 10),
      brightnessMaximum: parseInt(document.getElementById('led-brightnessMaximum').value, 10),
      colorNormal: colorToInt(document.getElementById('led-colorNormal').value),
      colorPressed: colorToInt(document.getElementById('led-colorPressed').value),
    },
  };

  setStatus('Saving...', true);
  try {
    await api('/api/setOptions', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(body),
    });
    setStatus('Saved! Rebooting...', true);
  } catch (e) {
    setStatus('Save failed: ' + e, false);
  }
}

async function testLed() {
  const color = colorToInt(document.getElementById('led-colorNormal').value);
  setStatus('Testing LEDs...', true);
  try {
    await api('/api/testLed', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ color, duration: 2000 }),
    });
    setStatus('Test sent!', true);
  } catch (e) {
    setStatus('Test failed: ' + e, false);
  }
}

async function resetSettings() {
  if (!confirm('Reset all settings to defaults and reboot?')) return;
  await api('/api/resetSettings', { method: 'POST' });
  setStatus('Settings reset. Rebooting...', true);
}

document.getElementById('save').addEventListener('click', save);
document.getElementById('test-led').addEventListener('click', testLed);
document.getElementById('reset').addEventListener('click', resetSettings);
document.getElementById('key-modal-save').addEventListener('click', saveKeyModal);
document.getElementById('key-modal-cancel').addEventListener('click', closeKeyModal);

// Close the modal when clicking the overlay backdrop or pressing Escape.
document.getElementById('key-modal').addEventListener('click', (e) => {
  if (e.target === e.currentTarget) closeKeyModal();
});
document.addEventListener('keydown', (e) => {
  if (e.key === 'Escape' && !document.getElementById('key-modal').hidden) closeKeyModal();
});

load();
