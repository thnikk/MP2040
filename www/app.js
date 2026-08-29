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

const GAMEPAD_MULTISELECT_OPTIONS = [
  { group: 'gamepad', label: 'Up', value: 0x0001 },
  { group: 'gamepad', label: 'Down', value: 0x0002 },
  { group: 'gamepad', label: 'Left', value: 0x0004 },
  { group: 'gamepad', label: 'Right', value: 0x0008 },
  { group: 'gamepad', label: 'B1', value: 0x0010 },
  { group: 'gamepad', label: 'B2', value: 0x0020 },
  { group: 'gamepad', label: 'B3', value: 0x0040 },
  { group: 'gamepad', label: 'B4', value: 0x0080 },
  { group: 'gamepad', label: 'L1', value: 0x0100 },
  { group: 'gamepad', label: 'R1', value: 0x0200 },
  { group: 'gamepad', label: 'L2', value: 0x0400 },
  { group: 'gamepad', label: 'R2', value: 0x0800 },
  { group: 'gamepad', label: 'S1', value: 0x1000 },
  { group: 'gamepad', label: 'S2', value: 0x2000 },
  { group: 'gamepad', label: 'L3', value: 0x4000 },
  { group: 'gamepad', label: 'R3', value: 0x8000 },
  { group: 'gamepad', label: 'A1', value: 0x10000 },
  { group: 'gamepad', label: 'A2', value: 0x20000 },
];

// Working copy of the config from /api/getOptions, edited via the modal
let currentOptions = null;

// Profile support (see proto/config.proto): all four profiles live in
// `profiles`; `currentOptions` is the working copy of the profile currently
// being edited (its per-profile fields are mirrored into the full options
// shape the rest of the UI expects). Switching takes effect at boot.
const PROFILE_COUNT = 4;
let profiles = [];
let currentProfileIndex = 0;
let activeProfile = 0;

// Board SVG view (see boardview.js), initialized by load()
let boardView = null;

// MultiSelect used in the key modal
let modalSelect = null;

// Visual keyboard picker (keyboardwidget.js) used in the key modal
let keyboardWidget = null;

// MIDI note picker (midikeyboard.js) used in the key modal in MIDI mode
let midiKeyboard = null;

// Gamepad control multi-select (MultiSelect) used in the key modal in
// gamepad modes
let gamepadSelect = null;

// Visual macro editor (macrobuilder.js) used on the Settings page
let macroBuilder = null;

// Configurable hotkeys editor (hotkeys.js) used on the Settings page
let hotkeysPanel = null;

// Configurable boot keys editor (bootkeys.js) used on the Settings page
let bootKeysPanel = null;

// Pin currently being edited in the modal
let editingPin = -1;

// LED brightness / speed pill sliders (see pillslider.js) and the LED timeout
// spinner (see spinner.js, in the Settings card)
let brightnessSlider = null;
let speedSlider = null;
let timeoutSpinner = null;

// LED color pickers (see createColorPicker below)
let colorNormalPicker = null;
let colorPressedPicker = null;

// Per-LED color popover (custom LED mode): the currently-open popover state,
// the LED element it's anchored to, and the pin whose colors it edits.
let ledColorPopover = null; // { ledIdx, pin, element } or null
let ledPopoverEl = null;
let ledPopoverNormalDot = null;
let ledPopoverPressedDot = null;
let ledPopoverNormalInput = null;
let ledPopoverPressedInput = null;
let ledPopoverUnsetBtn = null;

// Color picker pill button: a pill with a colored dot inside, with a hidden
// native <input type="color"> overlaid so clicking opens the OS picker
// (port of GP2040-th's LedColorPopover .led-color-btn).
function createColorPicker(container, { label, value, onChange }) {
  const wrap = document.createElement('div');
  wrap.className = 'color-picker';

  const btn = document.createElement('button');
  btn.type = 'button';
  btn.className = 'led-color-btn';

  const dot = document.createElement('span');
  dot.className = 'led-color-circle';
  dot.style.backgroundColor = value;

  const lbl = document.createElement('span');
  lbl.textContent = label || '';

  btn.appendChild(dot);
  btn.appendChild(lbl);

  const input = document.createElement('input');
  input.type = 'color';
  input.value = value;
  input.addEventListener('input', () => {
    dot.style.backgroundColor = input.value;
    if (onChange) onChange(input.value);
  });

  wrap.appendChild(btn);
  wrap.appendChild(input);
  container.classList.add('color-picker');
  container.appendChild(wrap);

  return {
    setValue(v) {
      input.value = v;
      dot.style.backgroundColor = v;
    },
    getValue() {
      return input.value;
    },
    setDisabled(disabled) {
      input.disabled = disabled;
      btn.classList.toggle('disabled', disabled);
    },
  };
}

function colorToInt(hex) {
  return parseInt(hex.replace('#', ''), 16);
}

function intToColor(value) {
  return '#' + value.toString(16).padStart(6, '0');
}

// ---- profile helpers ----------------------------------------------------

// Deep-ish copy of a profile's editable fields.
function cloneProfile(p) {
  p = p || {};
  return {
    keycodes: (p.keycodes || []).slice(),
    modifierMasks: (p.modifierMasks || []).slice(),
    midiNotes: (p.midiNotes || new Array(30).fill(0)).slice(),
    midiVelocities: (p.midiVelocities || new Array(30).fill(0)).slice(),
    midi: { channel: p.midi?.channel ?? 0, velocity: p.midi?.velocity ?? 127 },
    led: {
      ledMode: p.led?.ledMode ?? 0,
      ledNormalColors: (p.led?.ledNormalColors || []).slice(),
      ledPressedColors: (p.led?.ledPressedColors || []).slice(),
    },
  };
}

// Mirror a profile's per-profile fields into `options`, preserving the
// full-options shape (matrix, led.pinLedIndices, board properties, ...).
function applyProfileToOptions(profile, options) {
  options.keycodes = profile.keycodes.slice();
  options.modifierMasks = profile.modifierMasks.slice();
  options.midiNotes = profile.midiNotes.slice();
  options.midiVelocities = profile.midiVelocities.slice();
  options.midi = { ...(options.midi || {}), ...profile.midi };
  // Speed, brightness and per-mode colors are global; profiles only carry the
  // per-profile LED scalars (mode, per-key colors).
  const { ledSpeeds: _ledSpeeds, colorNormalByMode: _colorNormal, colorPressedByMode: _colorPressed, brightnessByMode: _brightness, ...profileLed } = profile.led || {};
  options.led = { ...(options.led || {}), ...profileLed };
}

// Copy `options`' per-profile fields back into a profile (opposite of above).
function applyOptionsToProfile(options, profile) {
  profile.keycodes = options.keycodes.slice();
  profile.modifierMasks = options.modifierMasks.slice();
  profile.midiNotes = options.midiNotes.slice();
  profile.midiVelocities = options.midiVelocities.slice();
  profile.midi = { ...(profile.midi || {}), ...(options.midi || {}) };
  const { ledSpeeds: _ledSpeeds, colorNormalByMode: _colorNormal, colorPressedByMode: _colorPressed, brightnessByMode: _brightness, ...optionsLed } = options.led || {};
  profile.led = { ...(profile.led || {}), ...optionsLed };
}

// Save any unsaved edits of the current tab back into its profile slot.
function syncCurrentToProfile() {
  if (!profiles[currentProfileIndex]) return;
  applyOptionsToProfile(currentOptions, profiles[currentProfileIndex]);
}

// Make the per-key color arrays dense so they can round-trip through JSON and
// flash: fill any missing entries with 0 (off). Keys with no per-key color
// stay dark in custom mode; a no-op once the arrays are populated.
// Returns the (now populated) led options object.
function materializeLedColors() {
  const led = currentOptions.led || {};
  const keyCount = (currentOptions.keycodes || []).length;
  if (!Array.isArray(led.ledNormalColors) || led.ledNormalColors.length < keyCount)
    led.ledNormalColors = new Array(keyCount).fill(0);
  if (!Array.isArray(led.ledPressedColors) || led.ledPressedColors.length < keyCount)
    led.ledPressedColors = new Array(keyCount).fill(0);
  return led;
}

// Refresh the LED/MIDI controls from the current profile's values.
function refreshPerProfileControls() {
  const midi = currentOptions.midi || {};
  if (midiChannelSpinner) midiChannelSpinner.setValue(midi.channel ?? 0);
  if (midiVelocitySpinner) midiVelocitySpinner.setValue(midi.velocity ?? 127);
  const led = currentOptions.led || {};
  const ledModeEl = document.getElementById('led-mode');
  if (ledModeEl) ledModeEl.value = led.ledMode ?? 0;
  syncBrightnessSliderToMode();
  syncSpeedSliderToMode();
  syncColorPickersToMode();
  if (timeoutSpinner) timeoutSpinner.setValue(led.ledTimeout ?? 0);
}

function updateProfileTabs() {
  const tabs = document.querySelectorAll('#profile-tabs .profile-tab');
  tabs.forEach((btn, i) => {
    btn.classList.toggle('active', i === currentProfileIndex);
    btn.classList.toggle('boot', i === activeProfile);
  });
}

function buildProfileTabs() {
  const tabs = document.getElementById('profile-tabs');
  if (!tabs) return;
  tabs.innerHTML = '';
  for (let i = 0; i < PROFILE_COUNT; i++) {
    const btn = document.createElement('button');
    btn.type = 'button';
    btn.className = 'profile-tab';
    btn.appendChild(document.createTextNode(`Profile ${i + 1}`));
    btn.addEventListener('click', () => switchProfile(i));
    tabs.appendChild(btn);
  }
  updateProfileTabs();
}

function switchProfile(i) {
  if (i === currentProfileIndex || !currentOptions) return;
  syncCurrentToProfile();
  currentProfileIndex = i;
  loadProfileIntoUi();
  updateProfileTabs();
}

function loadProfileIntoUi() {
  if (!currentOptions) return;
  const profile = profiles[currentProfileIndex] || cloneProfile(profiles[0]);
  applyProfileToOptions(profile, currentOptions);
  refreshPerProfileControls();
  if (boardView) boardView.setOptions(currentOptions);
}

async function api(path, options) {
  const res = await fetch(path, options);
  return res.json();
}

// Debounce a function by `ms`; trailing edge fires the last call.
function debounce(fn, ms) {
  let timer = null;
  return (...args) => {
    clearTimeout(timer);
    timer = setTimeout(() => fn(...args), ms);
  };
}

// Per-mode speeds are global (not per-profile): a 6-element array indexed by
// LED mode, seeded from the legacy scalar ledSpeed when the firmware hasn't
// sent per-mode values yet.
function getModeLedSpeeds() {
  const led = currentOptions.led || {};
  let speeds = Array.isArray(led.ledSpeeds) ? led.ledSpeeds.slice() : null;
  if (!speeds || speeds.length < 7) {
    const fill = Number.isFinite(led.ledSpeed) ? led.ledSpeed : 50;
    speeds = new Array(7).fill(fill);
  }
  return speeds;
}

function currentLedMode() {
  return parseInt(document.getElementById('led-mode').value, 10) || 0;
}

// Modes that actually render the normal/pressed colors: Custom (per-key
// fallback), Ripple, Rain, Fire. Cycle/Reactive/BPS are hue-based and ignore
// them.
function ledModeUsesColors(mode) {
  return mode === 0 || mode === 4 || mode === 5 || mode === 6;
}

// Per-mode normal/pressed colors (global, not per-profile): 6-element arrays
// indexed by LED mode, seeded from the legacy scalars when the firmware hasn't
// sent per-mode values yet.
function getModeLedColors() {
  const led = currentOptions.led || {};
  let normal = Array.isArray(led.colorNormalByMode) && led.colorNormalByMode.length >= 7
    ? led.colorNormalByMode.slice() : null;
  if (!normal) normal = new Array(7).fill(Number.isFinite(led.colorNormal) ? led.colorNormal : 0x00ff00);
  let pressed = Array.isArray(led.colorPressedByMode) && led.colorPressedByMode.length >= 7
    ? led.colorPressedByMode.slice() : null;
  if (!pressed) pressed = new Array(7).fill(Number.isFinite(led.colorPressed) ? led.colorPressed : 0xffffff);
  return { normal, pressed };
}

// Per-mode brightness (global, not per-profile): a 6-element array indexed by
// LED mode, seeded from the legacy scalar brightnessMaximum when the firmware
// hasn't sent per-mode values yet.
function getModeLedBrightnesses() {
  const led = currentOptions.led || {};
  let brightness = Array.isArray(led.brightnessByMode) && led.brightnessByMode.length >= 7
    ? led.brightnessByMode.slice() : null;
  if (!brightness) {
    const fill = Number.isFinite(led.brightnessMaximum) ? led.brightnessMaximum : 255;
    brightness = new Array(7).fill(fill);
  }
  return brightness;
}

// The brightness slider edits the currently-selected mode's brightness; reload
// it from the per-mode array when the mode changes.
function syncBrightnessSliderToMode() {
  if (!brightnessSlider) return;
  const mode = currentLedMode();
  const brightness = getModeLedBrightnesses();
  brightnessSlider.setValue(brightness[mode] ?? 255);
}

// The color pickers edit the currently-selected mode's colors; reload them
// when the mode changes and grey them out for modes that don't render colors.
function syncColorPickersToMode() {
  if (!colorNormalPicker || !colorPressedPicker) return;
  const mode = currentLedMode();
  const colors = getModeLedColors();
  colorNormalPicker.setValue(intToColor(colors.normal[mode] ?? 0x00ff00));
  colorPressedPicker.setValue(intToColor(colors.pressed[mode] ?? 0xffffff));
  const disabled = !ledModeUsesColors(mode);
  colorNormalPicker.setDisabled(disabled);
  colorPressedPicker.setDisabled(disabled);
}

// The speed slider edits the currently-selected mode's speed; reload it from
// the per-mode array when the mode changes, and grey it out in Custom (the
// only mode with no animation speed).
function syncSpeedSliderToMode() {
  if (!speedSlider) return;
  const mode = currentLedMode();
  const speeds = getModeLedSpeeds();
  speedSlider.setValue(speeds[mode] ?? 50);
  speedSlider.setDisabled(mode === 0);
}

// Read the current LED controls and push them to the board for a live preview.
// The control values are also written back into currentOptions.led so the sim
// and any later setOptions stay in sync (the led-mode dropdown change used to
// only fire a preview, leaving currentOptions.led.ledMode stale).
async function previewLed() {
  if (!currentOptions.led) currentOptions.led = {};
  const led = currentOptions.led;
  led.ledMode = parseInt(document.getElementById('led-mode').value, 10);
  const speeds = getModeLedSpeeds();
  speeds[led.ledMode] = speedSlider ? speedSlider.getValue() : 50;
  led.ledSpeeds = speeds;
  const brightness = getModeLedBrightnesses();
  brightness[led.ledMode] = brightnessSlider ? brightnessSlider.getValue() : 255;
  led.brightnessByMode = brightness;
  led.ledTimeout = timeoutSpinner ? timeoutSpinner.getValue() : 0;
  const statusLedEl = document.getElementById('status-led');
  led.statusLedEnabled = statusLedEl ? statusLedEl.checked : true;
  const colors = getModeLedColors();
  colors.normal[led.ledMode] = colorToInt(colorNormalPicker ? colorNormalPicker.getValue() : '#00ff00');
  colors.pressed[led.ledMode] = colorToInt(colorPressedPicker ? colorPressedPicker.getValue() : '#ffffff');
  led.colorNormalByMode = colors.normal;
  led.colorPressedByMode = colors.pressed;
  const preview = {
    ledMode: led.ledMode,
    ledSpeeds: led.ledSpeeds,
    brightnessByMode: led.brightnessByMode,
    ledTimeout: led.ledTimeout,
    statusLedEnabled: led.statusLedEnabled,
    colorNormalByMode: led.colorNormalByMode,
    colorPressedByMode: led.colorPressedByMode,
    ledNormalColors: led.ledNormalColors || [],
    ledPressedColors: led.ledPressedColors || [],
  };
  try {
    await api('/api/setLedPreview', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ led: preview }),
    });
    if (boardView) boardView.setLedParams(preview);
  } catch (e) {
    Toast.show('Preview failed: ' + e, 'error');
  }
}

const previewLedDebounced = debounce(previewLed, 150);

// Global MIDI Channel / Velocity spinners (visible only in MIDI mode)
let midiChannelSpinner = null;
let midiVelocitySpinner = null;
let debounceSpinner = null;
let touchMarginSpinner = null;
let touchReleaseSpinner = null;
let displaySplashDurationSpinner = null;
let displaySaverTimeoutSpinner = null;
let displayHistoryTimeoutSpinner = null;

// Human label for a menu-combo option, mirroring the board view: the pin's
// mapping in the current input mode (key assignment, macro, MIDI note or
// gamepad controls) when set, otherwise the bare pin index.
function comboPinLabel(options, index) {
  const mode = Number(options.defaultInputMode || 1);
  const midiMode = mode === 2;
  const gamepadMode = mode === 3 || mode === 4;
  const macroIndex = Number(options.macroIndices?.[index] || 0);
  const midiNote = Number(options.midiNotes?.[index] || 0);
  const gamepadMask = Number(options.gamepadMasks?.[index] || 0);

  let base = '';
  if (midiMode) {
    if (midiNote > 0) base = midiNoteName(midiNote);
  } else if (gamepadMode) {
    if (gamepadMask > 0) base = gamepadMaskLabels(gamepadMask).join('+');
  } else if (macroIndex > 0) {
    base = 'M' + macroIndex;
  } else {
    const mods = [];
    const mask = Number(options.modifierMasks?.[index] || 0);
    for (let i = 0; i < 8; i++) {
      if (mask & (1 << i)) mods.push(MODIFIER_SHORT[0xe0 + i] || '');
    }
    const code = Number(options.keycodes?.[index] || 0);
    const key = code ? keyLabel(code) : '';
    base = mods.length ? mods.join('+') + '+' + key : key;
  }
  return base ? `${base} (Pin ${index})` : `Pin ${index}`;
}

// Options for the menu-combo / hotkey multi-selects: one entry per key,
// labeled with the pin's mapping in the current input mode. group is the
// MultiSelect group id to attach them to.
function buildComboOptions(group = 'combo') {
  const count = (currentOptions.keycodes || []).length;
  return Array.from({ length: count }, (_, i) => ({
    group,
    label: comboPinLabel(currentOptions, i),
    value: i,
  }));
}

// Options for the boot-key dropdowns: "None" (disabled) plus one entry per key,
// labeled with the pin's mapping in the current input mode.
function buildBootPinOptions() {
  const count = (currentOptions.keycodes || []).length;
  return [{ value: -1, label: 'None' }].concat(
    Array.from({ length: count }, (_, i) => ({
      value: i,
      label: comboPinLabel(currentOptions, i),
    }))
  );
}

// Gather the current controls into a full config payload for /api/setOptions.
// Includes the profile being edited (profileIndex) and the boot profile.
function buildOptionsBody() {
  const colors = getModeLedColors();
  const mode = currentLedMode();
  colors.normal[mode] = colorToInt(colorNormalPicker ? colorNormalPicker.getValue() : '#00ff00');
  colors.pressed[mode] = colorToInt(colorPressedPicker ? colorPressedPicker.getValue() : '#ffffff');
  const brightness = getModeLedBrightnesses();
  brightness[mode] = brightnessSlider ? brightnessSlider.getValue() : 255;
  return {
    keycodes: currentOptions.keycodes,
    modifierMasks: currentOptions.modifierMasks,
    midiNotes: currentOptions.midiNotes,
    midiVelocities: currentOptions.midiVelocities,
    gamepadMasks: currentOptions.gamepadMasks || [],
    macroIndices: currentOptions.macroIndices,
    macros: currentOptions.macros || [],
    hotkeys: hotkeysPanel ? hotkeysPanel.getValue() : (currentOptions.hotkeys || []),
    bootKeys: bootKeysPanel ? bootKeysPanel.getValue() : (currentOptions.bootKeys || []),
    defaultInputMode: parseInt(document.getElementById('default-input-mode').value, 10),
    debounceInterval: debounceSpinner ? debounceSpinner.getValue() : 5,
    touchMargin: touchMarginSpinner ? touchMarginSpinner.getValue() : 15,
    touchRelease: touchReleaseSpinner ? touchReleaseSpinner.getValue() : 10,
    serialConfigEnabled: document.getElementById('serial-config').checked,
    midi: {
      channel: midiChannelSpinner ? midiChannelSpinner.getValue() : 0,
      velocity: midiVelocitySpinner ? midiVelocitySpinner.getValue() : 127,
    },
    gamepad: {
      socdMode: document.getElementById('socd-mode')
        ? parseInt(document.getElementById('socd-mode').value, 10)
        : 0,
      useNintendoLayout: document.getElementById('nintendo-layout')
        ? document.getElementById('nintendo-layout').checked
        : false,
    },
    ring: {
      ringStickTarget: currentOptions.ring?.ringStickTarget ?? 1,
      ringKeyboardMode: currentOptions.ring?.ringKeyboardMode ?? 2,
      ringScrollAxis: currentOptions.ring?.ringScrollAxis ?? 0,
      ringMidiBehavior: currentOptions.ring?.ringMidiBehavior ?? 1,
    },
    led: {
      ledMode: mode,
      ledSpeeds: getModeLedSpeeds(),
      brightnessByMode: brightness,
      ledTimeout: timeoutSpinner ? timeoutSpinner.getValue() : 0,
      statusLedEnabled: document.getElementById('status-led').checked,
      colorNormalByMode: colors.normal,
      colorPressedByMode: colors.pressed,
      ledNormalColors: currentOptions.led?.ledNormalColors || [],
      ledPressedColors: currentOptions.led?.ledPressedColors || [],
    },
    display: {
      size: currentOptions.display?.size ?? 3,
      flip: currentOptions.display?.flip ?? 0,
      invert: currentOptions.display?.invert ?? false,
      splashDuration: displaySplashDurationSpinner ? displaySplashDurationSpinner.getValue() : 3,
      displaySaverTimeout: displaySaverTimeoutSpinner ? displaySaverTimeoutSpinner.getValue() : 0,
      displaySaverMode: parseInt(document.getElementById('display-saver-mode').value, 10),
      inputHistoryEnabled: document.getElementById('display-input-history').checked,
      inputHistoryTimeout: displayHistoryTimeoutSpinner ? displayHistoryTimeoutSpinner.getValue() : 3,
    },
    profileIndex: currentProfileIndex,
    activeProfile,
  };
}

// ---- routing ----------------------------------------------------------
// Single HTML file, three routes: the landing page (/), the layout editor
// (/layout) and the settings page (/settings). The firmware httpd serves
// index.html for /layout and /settings (fs_open_custom in webconfig.cpp);
// here we switch which page is visible and keep the URL in sync via
// history.pushState.
const ROUTES = ['/', '/layout', '/settings'];

function currentRoute() {
  return ROUTES.includes(location.pathname) ? location.pathname : '/';
}

function renderRoute() {
  const route = currentRoute();
  document.getElementById('page-home').hidden = route !== '/';
  document.getElementById('page-layout').hidden = route !== '/layout';
  document.getElementById('page-settings').hidden = route !== '/settings';
  document.querySelectorAll('.nav-link').forEach((el) => {
    el.classList.toggle('active', el.dataset.route === route);
  });
  // The board SVG is laid out while hidden; re-fit it when the layout page
  // becomes visible, and only long-poll pin state there. updateLedSim()
  // (re)builds the LED simulation now that the page has real dimensions.
  if (route === '/layout') {
    if (boardView) {
      boardView.refresh();
      boardView.updateLedSim();
    }
    pollPinState();
  } else {
    closeLedColorPopover();
    stopPinState();
  }
}

function navigate(path, event) {
  if (event) event.preventDefault();
  if (location.pathname === path) return;
  history.pushState({}, '', path);
  renderRoute();
  window.scrollTo(0, 0);
}

window.addEventListener('popstate', renderRoute);

// Live pin state: highlight buttons yellow while their physical switch is
// held. The board answers /api/getPinState only when a button actually
// changes (long-poll), so this is event-driven rather than polled. A small
// gap before each reconnect keeps the mock server (which answers instantly)
// from being hammered, and the whole loop pauses while the tab is hidden or
// the layout page isn't shown.
let pinStateTimer = null;
function pollPinState() {
  if (document.hidden) return;
  pinStateTimer = setTimeout(async () => {
    try {
      const res = await api('/api/getPinState');
      if (boardView) boardView.setHeldPins(res.heldPins || []);
    } catch (e) {
      // Server closed the parked request (idle timeout); just retry.
    }
    pollPinState();
  }, 20);
}
function stopPinState() {
  clearTimeout(pinStateTimer);
}
document.addEventListener('visibilitychange', () => {
  if (document.hidden) stopPinState();
  else if (currentRoute() === '/layout') pollPinState();
});

// Parse "v1.2.3"-style versions; returns [major, minor, patch] or null.
function parseVersion(str) {
  if (typeof str !== 'string') return null;
  const m = str.trim().match(/^v?(\d+)\.(\d+)\.(\d+)/);
  return m ? m.slice(1).map(Number) : null;
}

// Compare [major, minor, patch] arrays: negative if a < b, 0 if equal.
function compareVersions(a, b) {
  for (let i = 0; i < 3; i++) {
    if (a[i] !== b[i]) return a[i] - b[i];
  }
  return 0;
}

// Compare the board's firmware version against the latest GitHub release and
// show an update card on the welcome page when a newer release exists. Runs
// client-side: the browser has internet even though the board does not. The
// mock server can inject `fakeLatestVersion` (via VITE_FAKE_UPDATE) to test
// the card without any network access.
async function checkForUpdates(version) {
  const card = document.getElementById('update-card');
  if (!card) return;
  const current = parseVersion(version.firmwareVersion);
  if (!current) return;

  // Mock-only: use the injected fake release instead of hitting GitHub.
  let latest = version.fakeLatestVersion ? parseVersion(version.fakeLatestVersion) : null;
  if (!latest) {
    // Skip mock/dev servers and untagged (bare-SHA) dev builds.
    if (version.mock || !/^v?\d+\.\d+\.\d+/.test(version.gitCommit || '')) return;
    try {
      const controller = new AbortController();
      const timer = setTimeout(() => controller.abort(), 5000);
      const res = await fetch('https://api.github.com/repos/thnikk/MP2040/releases/latest', {
        signal: controller.signal,
      });
      clearTimeout(timer);
      if (!res.ok) return;
      latest = parseVersion((await res.json()).tag_name);
    } catch (e) {
      // Offline or GitHub unreachable: leave the card hidden.
      return;
    }
  }
  if (!latest || compareVersions(latest, current) <= 0) return;
  document.getElementById('update-latest').textContent = 'v' + latest.join('.');
  document.getElementById('update-current').textContent = 'v' + current.join('.');
  card.hidden = false;
}

async function load() {
  const [options, version] = await Promise.all([
    api('/api/getOptions'),
    api('/api/getFirmwareVersion'),
  ]);
  currentOptions = options;
  // Global macros: per-key triggers and the M1-M8 definitions. Default to
  // empty for old firmware responses that don't carry the fields.
  currentOptions.macroIndices = Array.isArray(options.macroIndices)
    ? options.macroIndices.slice()
    : new Array(128).fill(0);
  currentOptions.macros = Array.isArray(options.macros) ? options.macros : [];
  document.getElementById('board-label-hero').textContent = version.boardLabel || '';
  document.getElementById('footer-version').textContent = version.firmwareVersion
    ? `${version.firmwareVersion}${version.gitCommit ? ` · ${version.gitCommit}` : ''}`
    : '';
  document.getElementById('landing-year').textContent = new Date().getFullYear();
  checkForUpdates(version);

  // Mock-server board switcher (dev only). The Development section is injected
  // into the page only by the mock server, and the real board never returns
  // `mock`, so neither exists when proxied to hardware. Switching boards
  // reloads the page so the whole config (options, board view, board svg)
  // re-initializes from the new board config.
  if (version.mock) {
    const mockSection = document.getElementById('mock-board-section');
    const mockBoardEl = document.getElementById('mock-board');
    if (mockSection && mockBoardEl) {
      const [boards, current] = await Promise.all([
        api('/api/boards'),
        api('/api/board'),
      ]);
      for (const b of Array.isArray(boards) ? boards : []) {
        const opt = document.createElement('option');
        opt.value = b.id;
        opt.textContent = b.label;
        mockBoardEl.appendChild(opt);
      }
      if (current && typeof current.board === 'string') mockBoardEl.value = current.board;
      mockBoardEl.addEventListener('change', async () => {
        const res = await api('/api/board', {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify({ board: mockBoardEl.value }),
        });
        if (res && res.error) {
          Toast.show('Failed to switch board: ' + res.error, 'error');
          return;
        }
        window.location.reload();
      });
    }
  }

  const led = options.led || {};
  const midi = options.midi || {};

  document.getElementById('default-input-mode').value = options.defaultInputMode ?? 1;
  document.getElementById('default-input-mode').addEventListener('change', () => {
    currentOptions.defaultInputMode = parseInt(document.getElementById('default-input-mode').value, 10);
    updateModalMode();
    if (boardView) boardView.refresh();
    if (hotkeysPanel) hotkeysPanel.setKeyOptions(buildComboOptions('hotkey'));
    if (bootKeysPanel) bootKeysPanel.setKeyOptions(buildBootPinOptions());
  });

  // Gamepad settings (XInput / Switch Pro modes)
  const gamepad = options.gamepad || {};
  const socdEl = document.getElementById('socd-mode');
  if (socdEl) {
    socdEl.value = gamepad.socdMode ?? 0;
    socdEl.addEventListener('change', () => {
      if (!currentOptions.gamepad) currentOptions.gamepad = {};
      currentOptions.gamepad.socdMode = parseInt(socdEl.value, 10);
    });
  }
  const nintendoEl = document.getElementById('nintendo-layout');
  if (nintendoEl) {
    nintendoEl.checked = gamepad.useNintendoLayout === true;
    nintendoEl.addEventListener('change', () => {
      if (!currentOptions.gamepad) currentOptions.gamepad = {};
      currentOptions.gamepad.useNintendoLayout = nintendoEl.checked;
    });
  }

  document.getElementById('serial-config').checked = options.serialConfigEnabled === true;
  document.getElementById('serial-config').addEventListener('change', () => {
    currentOptions.serialConfigEnabled = document.getElementById('serial-config').checked;
  });

  const statusLedEl = document.getElementById('status-led');
  if (statusLedEl) {
    // Boards without a status LED don't show the toggle at all.
    statusLedEl.closest('label').hidden = options.led?.hasStatusLed === false;
    statusLedEl.checked = Boolean(options.led?.statusLedEnabled);
    statusLedEl.addEventListener('change', () => {
      if (!currentOptions.led) currentOptions.led = {};
      currentOptions.led.statusLedEnabled = statusLedEl.checked;
      previewLed();
    });
  }

  // Display settings (SSD1306 OLED). Only shown when the board has display
  // wiring (enabled and wiring are board-fixed, not editable).
  const display = options.display || {};
  const displaySection = document.getElementById('display-settings');
  if (displaySection) {
    displaySection.hidden = display.hasDisplay !== true;
  }
  const bindSelect = (id, key) => {
    const el = document.getElementById(id);
    if (!el) return;
    el.value = display[key] ?? 0;
    el.addEventListener('change', () => {
      if (!currentOptions.display) currentOptions.display = {};
      currentOptions.display[key] = parseInt(el.value, 10);
    });
  };
  // buttonLayout / orientation / splashMode are board-fixed (not editable).
  bindSelect('display-saver-mode', 'displaySaverMode');
  const displayHistoryEl = document.getElementById('display-input-history');
  if (displayHistoryEl) {
    displayHistoryEl.checked = display.inputHistoryEnabled !== false;
    displayHistoryEl.addEventListener('change', () => {
      if (!currentOptions.display) currentOptions.display = {};
      currentOptions.display.inputHistoryEnabled = displayHistoryEl.checked;
    });
  }
  displaySplashDurationSpinner = new Spinner({
    container: document.getElementById('display-splash-duration-spinner'),
    min: 0,
    max: 60,
    step: 1,
    value: display.splashDuration ?? 3,
    onChange: () => {},
  });
  displaySaverTimeoutSpinner = new Spinner({
    container: document.getElementById('display-saver-timeout-spinner'),
    min: 0,
    max: 3600,
    step: 5,
    value: display.displaySaverTimeout ?? 0,
    onChange: () => {},
  });
  displayHistoryTimeoutSpinner = new Spinner({
    container: document.getElementById('display-history-timeout-spinner'),
    min: 0,
    max: 300,
    step: 1,
    value: display.inputHistoryTimeout ?? 3,
    onChange: () => {},
  });

  midiChannelSpinner = new Spinner({
    container: document.getElementById('midi-channel-spinner'),
    min: 0,
    max: 15,
    value: midi.channel ?? 0,
    onChange: () => {},
  });
  midiVelocitySpinner = new Spinner({
    container: document.getElementById('midi-velocity-spinner'),
    min: 1,
    max: 127,
    value: midi.velocity ?? 127,
    onChange: () => {},
  });

  debounceSpinner = new Spinner({
    container: document.getElementById('debounce-spinner'),
    min: 0,
    max: 100,
    value: options.debounceInterval ?? 5,
    onChange: () => {},
  });

  touchMarginSpinner = new Spinner({
    container: document.getElementById('touch-margin-spinner'),
    min: 0,
    max: 100,
    value: options.touchMargin ?? 15,
    onChange: () => {},
  });

  touchReleaseSpinner = new Spinner({
    container: document.getElementById('touch-release-spinner'),
    min: 0,
    max: 100,
    value: options.touchRelease ?? 10,
    onChange: () => {},
  });

  // Boards without capacitive touch pads don't show the touch settings row.
  const touchSettingsEl = document.getElementById('touch-settings');
  if (touchSettingsEl) {
    touchSettingsEl.hidden = options.touch?.hasTouchPads === false;
  }

  document.getElementById('led-mode').value = led.ledMode ?? 0;
  document.getElementById('led-mode').addEventListener('change', () => {
    syncBrightnessSliderToMode();
    syncSpeedSliderToMode();
    syncColorPickersToMode();
    previewLed();
  });

  brightnessSlider = new PillSlider({
    container: document.getElementById('led-brightness'),
    min: 0,
    max: 255,
    label: 'Brightness',
    value: led.brightnessByMode?.[led.ledMode ?? 0] ?? led.brightnessMaximum ?? 255,
    padLength: 3,
    onChange: previewLed,
  });

  speedSlider = new PillSlider({
    container: document.getElementById('led-speed'),
    min: 0,
    max: 100,
    label: 'Speed',
    value: led.ledSpeeds?.[led.ledMode ?? 0] ?? led.ledSpeed ?? 50,
    padLength: 3,
    onChange: previewLed,
  });
  syncSpeedSliderToMode();

  timeoutSpinner = new Spinner({
    container: document.getElementById('led-timeout-spinner'),
    min: 0,
    max: 600,
    value: led.ledTimeout ?? 0,
    onChange: previewLed,
  });

  colorNormalPicker = createColorPicker(document.getElementById('led-colorNormal'), {
    label: 'Normal',
    value: intToColor(led.colorNormalByMode?.[led.ledMode ?? 0] ?? led.colorNormal ?? 0x00ff00),
    onChange: previewLedDebounced,
  });
  colorPressedPicker = createColorPicker(document.getElementById('led-colorPressed'), {
    label: 'Pressed',
    value: intToColor(led.colorPressedByMode?.[led.ledMode ?? 0] ?? led.colorPressed ?? 0xffffff),
    onChange: previewLedDebounced,
  });
  syncColorPickersToMode();

  buildLedColorPopover();

  const macrosPanel = document.getElementById('macros-panel');
  if (macrosPanel) {
    macroBuilder = new MacroBuilder({
      container: macrosPanel,
      macros: currentOptions.macros,
      onChange: (macros) => { currentOptions.macros = macros; },
    });
  }

  const hotkeysPanelEl = document.getElementById('hotkeys-panel');
  if (hotkeysPanelEl) {
    hotkeysPanel = new HotkeysPanel({
      container: hotkeysPanelEl,
      hotkeys: Array.isArray(options.hotkeys) ? options.hotkeys : [],
      keyOptions: buildComboOptions('hotkey'),
      // Only display boards can use the mini-menu toggle action.
      menuToggle: display.hasDisplay === true,
      onChange: (hotkeys) => { currentOptions.hotkeys = hotkeys; },
    });
  }

  const bootKeysPanelEl = document.getElementById('boot-keys-panel');
  if (bootKeysPanelEl) {
    bootKeysPanel = new BootKeysPanel({
      container: bootKeysPanelEl,
      bootKeys: Array.isArray(options.bootKeys) ? options.bootKeys : [],
      keyOptions: buildBootPinOptions(),
      // Board-fixed boot pins shown for reference (greyed out, not editable);
      // rows with an undefined pin are hidden.
      fixedKeys: [
        { label: 'Web Config', pin: options.webConfigPin ?? -1 },
        { label: 'USB Bootloader', pin: options.bootPin ?? -1 },
      ],
      pinLabel: (pin) => (pin < 0 ? '' : comboPinLabel(currentOptions, pin)),
      onChange: (bootKeys) => { currentOptions.bootKeys = bootKeys; },
    });
  }

  modalSelect = new MultiSelect({
    container: document.getElementById('key-modal-select'),
    options: MULTISELECT_OPTIONS,
    groups: MULTISELECT_GROUPS,
    onChange: () => {
      const { keycode, mask, macroIndex } = modalSelect.getValue();
      keyboardWidget.setValue(keycode, mask, macroIndex);
    },
  });

  keyboardWidget = new KeyboardWidget({
    container: document.getElementById('key-modal-keyboard'),
    keycode: 0,
    mask: 0,
    onChange: (keycode, mask, macroIndex) => {
      modalSelect.setValue(keycode, mask, macroIndex);
    },
  });

  midiKeyboard = new MidiKeyboard({
    container: document.getElementById('key-modal-midi'),
    value: 0,
    onChange: () => {},
  });

  gamepadSelect = new MultiSelect({
    container: document.getElementById('key-modal-gamepad'),
    options: GAMEPAD_MULTISELECT_OPTIONS,
    groups: GAMEPAD_MULTISELECT_GROUPS,
    onChange: () => {},
  });

  initBoard(options);
  updateModalMode();

  // Profiles: default the editor to the active profile so what the user sees
  // matches what boots. Tabs and the "Set as Default" header button are wired
  // below.
  profiles = Array.isArray(options.profiles) && options.profiles.length >= PROFILE_COUNT
    ? options.profiles.map(cloneProfile)
    : [options, options, options, options].map(cloneProfile);
  activeProfile = Number(options.activeProfile ?? 0);
  if (activeProfile < 0 || activeProfile >= PROFILE_COUNT) activeProfile = 0;
  currentProfileIndex = activeProfile;

  buildProfileTabs();

  loadProfileIntoUi();

  renderRoute();

  const loading = document.getElementById('loading');
  if (loading) loading.hidden = true;
}

function loadError() {
  const loading = document.getElementById('loading');
  if (loading) loading.hidden = true;
}

// Show either the key/modifier pickers (keyboard mode), the MIDI note picker
// (MIDI mode) or the gamepad control multi-select (gamepad modes) in the modal,
// based on the current default input mode. Also reveals the MIDI / gamepad
// settings and swaps the Board card description to match the mode.
function updateModalMode() {
  const mode = Number(currentOptions.defaultInputMode || 1);
  const midiMode = mode === 2;
  const gamepadMode = mode === 3 || mode === 4;
  const keyboardMode = !midiMode && !gamepadMode;
  document.getElementById('key-modal-select').hidden = !keyboardMode;
  document.getElementById('key-modal-keyboard').hidden = !keyboardMode;
  document.getElementById('key-modal-midi').hidden = !midiMode;
  document.getElementById('key-modal-gamepad').hidden = !gamepadMode;
  document.getElementById('midi-settings').hidden = !midiMode;
  document.getElementById('gamepad-settings').hidden = !gamepadMode;
  // The Nintendo layout toggle only applies to Switch Pro.
  document.getElementById('nintendo-layout-wrap').hidden = mode !== 4;
  document.getElementById('board-hint').textContent = midiMode
    ? 'Click a button on the board to set its MIDI note and velocity.'
    : gamepadMode
      ? 'Click a button on the board to set its gamepad button or direction.'
      : 'Click a button on the board to set its key and modifiers.';
}

function initBoard(options) {
  const panel = document.getElementById('board-panel');
  if (!panel) return;

  boardView = new BoardView(panel, {
    onPinClick: (pin) => openKeyModal(pin),
    onLedClick: (ledIdx, el) => openLedColorPopover(ledIdx, el),
    onRingClick: () => openRingModal(),
  });
  boardView.setOptions(options);
}

// ---- LED color popover (custom mode) -------------------------------------

// Build the popover DOM once. It's positioned next to whichever LED is
// clicked (GP2040-th style) and edits that LED's per-key colors.
function buildLedColorPopover() {
  ledPopoverEl = document.createElement('div');
  ledPopoverEl.className = 'led-popover';
  ledPopoverEl.hidden = true;

  const body = document.createElement('div');
  body.className = 'led-popover-body';

  const normalWrap = document.createElement('div');
  normalWrap.className = 'led-popover-color';
  const normalBtn = document.createElement('button');
  normalBtn.type = 'button';
  normalBtn.className = 'led-color-btn';
  ledPopoverNormalDot = document.createElement('span');
  ledPopoverNormalDot.className = 'led-color-circle';
  const normalLbl = document.createElement('span');
  normalLbl.textContent = 'Normal';
  normalBtn.appendChild(ledPopoverNormalDot);
  normalBtn.appendChild(normalLbl);
  ledPopoverNormalInput = document.createElement('input');
  ledPopoverNormalInput.type = 'color';
  ledPopoverNormalInput.addEventListener('input', () => {
    ledPopoverNormalDot.style.backgroundColor = ledPopoverNormalInput.value;
    if (!ledColorPopover) return;
    const ledOpts = materializeLedColors();
    ledOpts.ledNormalColors[ledColorPopover.pin] = colorToInt(ledPopoverNormalInput.value);
    updateLedPopoverUnsetState();
    previewLedDebounced();
  });
  normalWrap.appendChild(normalBtn);
  normalWrap.appendChild(ledPopoverNormalInput);

  const pressedWrap = document.createElement('div');
  pressedWrap.className = 'led-popover-color';
  const pressedBtn = document.createElement('button');
  pressedBtn.type = 'button';
  pressedBtn.className = 'led-color-btn';
  ledPopoverPressedDot = document.createElement('span');
  ledPopoverPressedDot.className = 'led-color-circle';
  const pressedLbl = document.createElement('span');
  pressedLbl.textContent = 'Pressed';
  pressedBtn.appendChild(ledPopoverPressedDot);
  pressedBtn.appendChild(pressedLbl);
  ledPopoverPressedInput = document.createElement('input');
  ledPopoverPressedInput.type = 'color';
  ledPopoverPressedInput.addEventListener('input', () => {
    ledPopoverPressedDot.style.backgroundColor = ledPopoverPressedInput.value;
    if (!ledColorPopover) return;
    const ledOpts = materializeLedColors();
    ledOpts.ledPressedColors[ledColorPopover.pin] = colorToInt(ledPopoverPressedInput.value);
    updateLedPopoverUnsetState();
    previewLedDebounced();
  });
  pressedWrap.appendChild(pressedBtn);
  pressedWrap.appendChild(ledPopoverPressedInput);

  body.appendChild(normalWrap);
  body.appendChild(pressedWrap);

  // Unset: clear this key's per-key colors back to off (0 = off). Unlike the
  // color pickers, which only ever assign a value, this also lets you remove
  // an existing per-key color so the key goes dark again.
  const unsetBtn = document.createElement('button');
  unsetBtn.type = 'button';
  unsetBtn.className = 'led-popover-unset';
  unsetBtn.title = 'Unset';
  const unsetIcon = document.createElement('span');
  unsetIcon.className = 'icon icon-trash';
  unsetIcon.setAttribute('aria-hidden', 'true');
  unsetBtn.appendChild(unsetIcon);
  unsetBtn.addEventListener('click', () => {
    if (!ledColorPopover) return;
    const ledOpts = materializeLedColors();
    ledOpts.ledNormalColors[ledColorPopover.pin] = 0;
    ledOpts.ledPressedColors[ledColorPopover.pin] = 0;
    previewLedDebounced();
    closeLedColorPopover();
  });
  body.appendChild(unsetBtn);
  ledPopoverUnsetBtn = unsetBtn;

  ledPopoverEl.appendChild(body);
  document.body.appendChild(ledPopoverEl);

  // Close on a click outside the popover (except on the LEDs themselves,
  // which toggle/move it via their own click handler).
  document.addEventListener('mousedown', (e) => {
    if (!ledColorPopover || !ledPopoverEl) return;
    if (ledPopoverEl.contains(e.target)) return;
    if (e.target.closest && e.target.closest('[id^="led"]')) return;
    closeLedColorPopover();
  });

  window.addEventListener('scroll', positionLedColorPopover, true);
  window.addEventListener('resize', positionLedColorPopover);
}

// Reverse LED strip index -> key index from pinLedIndices (+ ledsPerKey range).
function ledKeyForIndex(ledIdx) {
  const indices = currentOptions.led?.pinLedIndices || [];
  const perKey = Math.max(1, currentOptions.led?.ledsPerKey || 1);
  for (let pin = 0; pin < indices.length; pin++) {
    const start = indices[pin];
    if (start === undefined || start < 0) continue;
    if (ledIdx >= start && ledIdx < start + perKey) return pin;
  }
  return -1;
}

function isCustomLedMode() {
  return parseInt(document.getElementById('led-mode')?.value, 10) === 0;
}

function openLedColorPopover(ledIdx, el) {
  if (!isCustomLedMode() || !ledPopoverEl) return;
  if (ledColorPopover && ledColorPopover.ledIdx === ledIdx) {
    closeLedColorPopover();
    return;
  }
  const pin = ledKeyForIndex(ledIdx);
  if (pin < 0) return;

  ledColorPopover = { ledIdx, pin, element: el };
  const led = currentOptions.led || {};
  // Unset keys (value 0) show Custom mode's default color so assigning a first
  // color is easy; set keys show their current value.
  const rawNormal = led.ledNormalColors?.[pin] ?? 0;
  const rawPressed = led.ledPressedColors?.[pin] ?? 0;
  const normal = rawNormal > 0 ? rawNormal : (led.colorNormalByMode?.[0] ?? led.colorNormal ?? 0x00ff00);
  const pressed = rawPressed > 0 ? rawPressed : (led.colorPressedByMode?.[0] ?? led.colorPressed ?? 0xffffff);
  ledPopoverNormalInput.value = intToColor(normal);
  ledPopoverNormalDot.style.backgroundColor = intToColor(normal);
  ledPopoverPressedInput.value = intToColor(pressed);
  ledPopoverPressedDot.style.backgroundColor = intToColor(pressed);
  updateLedPopoverUnsetState();

  ledPopoverEl.hidden = false;
  positionLedColorPopover();
}

// Enable the Unset button only once this key actually has a per-key color to
// remove; re-checked whenever the popover's colors change.
function updateLedPopoverUnsetState() {
  if (!ledColorPopover || !ledPopoverUnsetBtn) return;
  const led = currentOptions.led || {};
  const pin = ledColorPopover.pin;
  const rawNormal = led.ledNormalColors?.[pin] ?? 0;
  const rawPressed = led.ledPressedColors?.[pin] ?? 0;
  ledPopoverUnsetBtn.disabled = rawNormal === 0 && rawPressed === 0;
}

function closeLedColorPopover() {
  ledColorPopover = null;
  if (ledPopoverEl) ledPopoverEl.hidden = true;
}

function positionLedColorPopover() {
  if (!ledColorPopover || !ledPopoverEl || ledPopoverEl.hidden) return;
  const rect = ledColorPopover.element.getBoundingClientRect();
  const pw = ledPopoverEl.offsetWidth;
  const ph = ledPopoverEl.offsetHeight;
  const vw = window.innerWidth;
  const GAP = 10;

  let left = rect.left + rect.width / 2 - pw / 2;
  left = Math.max(GAP, Math.min(left, vw - pw - GAP));
  let top = rect.top - ph - GAP;
  const flip = top < GAP;
  if (flip) top = rect.bottom + GAP;

  ledPopoverEl.classList.toggle('arrow-up', flip);
  const arrowOffset = rect.left + rect.width / 2 - left;
  ledPopoverEl.style.setProperty('--arrow-left', `${arrowOffset}px`);
  ledPopoverEl.style.left = left + 'px';
  ledPopoverEl.style.top = top + 'px';
}

function openKeyModal(pin) {
  editingPin = pin;
  document.getElementById('key-modal-title').textContent =
    `GP${pin.toString().padStart(2, '0')}`;
  const keycode = Number(currentOptions.keycodes[pin] || 0);
  const mask = Number(currentOptions.modifierMasks[pin] || 0);
  const macroIndex = Number(currentOptions.macroIndices?.[pin] || 0);
  const midiNote = Number(currentOptions.midiNotes?.[pin] || 0);
  const gamepadMask = Number(currentOptions.gamepadMasks?.[pin] || 0);
  modalSelect.setValue(keycode, mask, macroIndex);
  keyboardWidget.setValue(keycode, mask, macroIndex);
  midiKeyboard.setValue(midiNote);
  midiKeyboard.setVelocity(Number(currentOptions.midiVelocities?.[pin] || 0));
  gamepadSelect.setGroupMask('gamepad', gamepadMask);
  closeLedColorPopover();
  updateModalMode();
  document.getElementById('key-modal').hidden = false;
  // The widget may have been built while the modal was hidden, so re-fit the
  // octave window now that it has a real width.
  requestAnimationFrame(() => midiKeyboard.refresh());
}

function closeKeyModal() {
  document.getElementById('key-modal').hidden = true;
  editingPin = -1;
}

function saveKeyModal() {
  if (editingPin < 0) return;

  // Gamepad mapping is separate from the keyboard / MIDI mapping (each lives
  // in its own per-pin array, and they coexist the way a pin can hold both a
  // key and a MIDI note). Which picker saves what is decided by the active
  // mode; neither branch clears the other's data.
  const perKey = (key) => {
    if (!currentOptions[key]) currentOptions[key] = new Array(128).fill(0);
    return currentOptions[key];
  };

  currentOptions.keycodes = perKey('keycodes');
  currentOptions.modifierMasks = perKey('modifierMasks');
  currentOptions.midiNotes = perKey('midiNotes');
  currentOptions.midiVelocities = perKey('midiVelocities');
  currentOptions.gamepadMasks = perKey('gamepadMasks');

  const mode = Number(currentOptions.defaultInputMode || 1);
  const gamepadMode = mode === 3 || mode === 4;

  if (gamepadMode) {
    // A pin maps to zero or more gamepad controls, packed into one mask.
    currentOptions.gamepadMasks[editingPin] = gamepadSelect.getGroupMask('gamepad');
  } else {
    const { keycode, mask, macroIndex } = keyboardWidget.getValue();
    // A pin is either a plain key or a macro trigger, never both.
    currentOptions.keycodes[editingPin] = macroIndex ? 0 : keycode;
    currentOptions.modifierMasks[editingPin] = macroIndex ? 0 : mask;
    if (!currentOptions.macroIndices) currentOptions.macroIndices = new Array(128).fill(0);
    currentOptions.macroIndices[editingPin] = macroIndex;
    currentOptions.midiNotes[editingPin] = midiKeyboard.getValue();
    currentOptions.midiVelocities[editingPin] = midiKeyboard.getVelocity();
  }

  if (boardView) boardView.setOptions(currentOptions);
  closeKeyModal();
}

// ---- touch ring modal ----------------------------------------------------

// Populate the ring modal from currentOptions.ring. Only the control for the
// current input mode is shown (gamepad = stick, keyboard = behavior + axis,
// MIDI = behavior).
function openRingModal() {
  if (!currentOptions.ring) currentOptions.ring = {};
  const r = currentOptions.ring;
  const mode = Number(currentOptions.defaultInputMode || 1);
  const gamepadMode = mode === 3 || mode === 4;
  const midiMode = mode === 2;

  document.getElementById('ring-modal-stick-wrap').hidden = !gamepadMode;
  document.getElementById('ring-modal-keyboard-wrap').hidden = gamepadMode || midiMode;
  document.getElementById('ring-modal-midi-wrap').hidden = !midiMode;

  document.getElementById('ring-modal-stick').value = r.ringStickTarget ?? 1;
  const kbEl = document.getElementById('ring-modal-keyboard');
  kbEl.value = r.ringKeyboardMode ?? 2;
  document.getElementById('ring-modal-scroll-wrap').hidden = Number(kbEl.value) !== 1;
  document.getElementById('ring-modal-axis').value = r.ringScrollAxis ?? 0;
  document.getElementById('ring-modal-midi').value = r.ringMidiBehavior ?? 1;

  closeLedColorPopover();
  document.getElementById('ring-modal').hidden = false;
}

function closeRingModal() {
  document.getElementById('ring-modal').hidden = true;
}

function saveRingModal() {
  if (!currentOptions.ring) currentOptions.ring = {};
  const mode = Number(currentOptions.defaultInputMode || 1);
  const gamepadMode = mode === 3 || mode === 4;
  const midiMode = mode === 2;
  // Save only the control shown for the current mode; the others are left
  // unchanged (they're configured when that mode is active).
  if (gamepadMode)
    currentOptions.ring.ringStickTarget = parseInt(document.getElementById('ring-modal-stick').value, 10);
  else if (midiMode)
    currentOptions.ring.ringMidiBehavior = parseInt(document.getElementById('ring-modal-midi').value, 10);
  else {
    currentOptions.ring.ringKeyboardMode = parseInt(document.getElementById('ring-modal-keyboard').value, 10);
    currentOptions.ring.ringScrollAxis = parseInt(document.getElementById('ring-modal-axis').value, 10);
  }
  closeRingModal();
}

async function save() {
  const saveBtn = document.getElementById('save');
  saveBtn.disabled = true;
  try {
    syncCurrentToProfile();
    const res = await api('/api/setOptions', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(buildOptionsBody()),
    });
    // Refresh the local profile pool from the server response so switching
    // tabs never shows stale data, and mirror the active profile back into
    // the working copy (the board stays in config mode until reboot).
    if (res && Array.isArray(res.profiles)) {
      profiles = res.profiles.map(cloneProfile);
      activeProfile = Number(res.activeProfile ?? activeProfile);
      if (activeProfile < 0 || activeProfile >= PROFILE_COUNT) activeProfile = 0;
      const edited = profiles[currentProfileIndex] || cloneProfile();
      applyProfileToOptions(edited, currentOptions);
      refreshPerProfileControls();
      updateProfileTabs();
    }
    Toast.show('Saved.', 'success');
  } catch (e) {
    Toast.show('Save failed: ' + e, 'error');
  }
  saveBtn.disabled = false;
}

async function reboot(bootMode) {
  const rebootBtn = document.getElementById('reboot');
  rebootBtn.disabled = true;
  try {
    await api('/api/reboot', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ bootMode }),
    });
    Toast.show('Rebooting...', 'info');
  } catch (e) {
    Toast.show('Reboot failed: ' + e, 'error');
    rebootBtn.disabled = false;
  }
}

function openRebootModal() {
  document.getElementById('reboot-modal').hidden = false;
}

function closeRebootModal() {
  document.getElementById('reboot-modal').hidden = true;
}

async function rebootTo(bootMode) {
  closeRebootModal();
  await reboot(bootMode);
}

async function resetSettings() {
  if (!confirm('Reset all settings to defaults and reboot?')) return;
  await api('/api/resetSettings', { method: 'POST' });
  Toast.show('Settings reset. Rebooting...', 'info');
}

// ---- import / export ------------------------------------------------------

// Download all settings (profiles + globals) as a portable JSON file. Board
// properties are excluded: they're fixed per board and re-enforced on import.
function exportSettings() {
  syncCurrentToProfile();
  const payload = {
    type: 'mp2040-config',
    version: 1,
    activeProfile,
    defaultInputMode: currentOptions.defaultInputMode ?? 1,
    serialConfigEnabled: currentOptions.serialConfigEnabled === true,
    macroIndices: currentOptions.macroIndices || [],
    macros: currentOptions.macros || [],
    gamepad: {
      socdMode: currentOptions.gamepad?.socdMode ?? 0,
      useNintendoLayout: currentOptions.gamepad?.useNintendoLayout === true,
    },
    ring: {
      ringStickTarget: currentOptions.ring?.ringStickTarget ?? 1,
      ringKeyboardMode: currentOptions.ring?.ringKeyboardMode ?? 2,
      ringScrollAxis: currentOptions.ring?.ringScrollAxis ?? 0,
      ringMidiBehavior: currentOptions.ring?.ringMidiBehavior ?? 1,
    },
    gamepadMasks: currentOptions.gamepadMasks || [],
    led: {
      ledTimeout: currentOptions.led?.ledTimeout ?? 0,
      statusLedEnabled: currentOptions.led?.statusLedEnabled ?? true,
      ledSpeeds: getModeLedSpeeds(),
      colorNormalByMode: getModeLedColors().normal,
      colorPressedByMode: getModeLedColors().pressed,
    },
    profiles: profiles.map((p) => cloneProfile(p)),
  };
  const blob = new Blob([JSON.stringify(payload, null, 2)], { type: 'application/json' });
  const url = URL.createObjectURL(blob);
  const a = document.createElement('a');
  a.href = url;
  a.download = 'mp2040-config.json';
  document.body.appendChild(a);
  a.click();
  a.remove();
  URL.revokeObjectURL(url);
  Toast.show('Settings exported.', 'success');
}

// Restore settings from an exported JSON file. The board's setOptions only
// edits one profile per call, so each profile is applied in turn; global
// fields (macros, input mode, active profile) ride along on the last call to
// stay under the firmware's 16KB POST limit.
async function importSettings(file) {
  let data;
  try {
    data = JSON.parse(await file.text());
  } catch (e) {
    Toast.show('Import failed: not a valid JSON file', 'error');
    return;
  }
  if (!data || data.type !== 'mp2040-config' || !Array.isArray(data.profiles) || !data.profiles.length) {
    Toast.show('Import failed: not an MP2040 settings file', 'error');
    return;
  }
  const profiles = data.profiles.slice(0, 4);
  const led = data.led || {};
  const globals = {
    macroIndices: data.macroIndices || [],
    macros: data.macros || [],
    defaultInputMode: data.defaultInputMode ?? 1,
    serialConfigEnabled: data.serialConfigEnabled === true,
    activeProfile: Number.isInteger(data.activeProfile) ? data.activeProfile : 0,
    gamepad: {
      socdMode: Number.isInteger(data.gamepad?.socdMode) ? data.gamepad.socdMode : 0,
      useNintendoLayout: data.gamepad?.useNintendoLayout === true,
    },
    ring: {
      ringStickTarget: Number.isInteger(data.ring?.ringStickTarget) ? data.ring.ringStickTarget : 1,
      ringKeyboardMode: Number.isInteger(data.ring?.ringKeyboardMode) ? data.ring.ringKeyboardMode : 2,
      ringScrollAxis: Number.isInteger(data.ring?.ringScrollAxis) ? data.ring.ringScrollAxis : 0,
      ringMidiBehavior: Number.isInteger(data.ring?.ringMidiBehavior) ? data.ring.ringMidiBehavior : 1,
    },
    gamepadMasks: data.gamepadMasks || [],
  };
  try {
    for (let i = 0; i < profiles.length; i++) {
      const p = profiles[i];
      const body = {
        profileIndex: i,
        keycodes: p.keycodes || [],
        modifierMasks: p.modifierMasks || [],
        midiNotes: p.midiNotes || [],
        midiVelocities: p.midiVelocities || [],
        midi: p.midi || {},
        led: {
          ...(p.led || {}),
          ledSpeeds: led.ledSpeeds || [],
          colorNormalByMode: led.colorNormalByMode || [],
          colorPressedByMode: led.colorPressedByMode || [],
          ledTimeout: led.ledTimeout ?? 0,
          statusLedEnabled: led.statusLedEnabled ?? true,
        },
        // Globals only on the last call.
        ...(i === profiles.length - 1 ? globals : {}),
      };
      await api('/api/setOptions', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(body),
      });
    }
    Toast.show('Settings imported.', 'success');
    location.reload();
  } catch (e) {
    Toast.show('Import failed: ' + e, 'error');
  }
}

document.getElementById('export-settings').addEventListener('click', exportSettings);
document.getElementById('import-settings').addEventListener('click', () => {
  document.getElementById('import-file').click();
});
document.getElementById('import-file').addEventListener('change', (e) => {
  const file = e.target.files && e.target.files[0];
  if (!file) return;
  importSettings(file);
  e.target.value = '';
});

document.getElementById('save').addEventListener('click', save);
document.getElementById('save-settings').addEventListener('click', save);
document.querySelectorAll('[data-route]').forEach((el) => {
  el.addEventListener('click', (e) => {
    if (e.button !== 0 || e.metaKey || e.ctrlKey || e.shiftKey || e.altKey) return;
    navigate(el.dataset.route, e);
  });
});
document.getElementById('reboot').addEventListener('click', openRebootModal);
document.getElementById('reset').addEventListener('click', resetSettings);
document.getElementById('set-boot-profile').addEventListener('click', () => {
  activeProfile = currentProfileIndex;
  updateProfileTabs();
});
document.getElementById('key-modal-save').addEventListener('click', saveKeyModal);
document.getElementById('key-modal-close').addEventListener('click', closeKeyModal);
document.getElementById('ring-modal-save').addEventListener('click', saveRingModal);
document.getElementById('ring-modal-close').addEventListener('click', closeRingModal);

// Toggle the ring modal's scroll-axis field based on the keyboard-mode select.
document.getElementById('ring-modal-keyboard').addEventListener('change', (e) => {
  document.getElementById('ring-modal-scroll-wrap').hidden = Number(e.target.value) !== 1;
});

document.getElementById('reboot-modal-close').addEventListener('click', closeRebootModal);
document.getElementById('reboot-normal').addEventListener('click', () => rebootTo(0));
document.getElementById('reboot-bootloader').addEventListener('click', () => rebootTo(2));
document.getElementById('reboot-webconfig').addEventListener('click', () => rebootTo(1));

// Close the modal when clicking the overlay backdrop or pressing Escape.
document.getElementById('key-modal').addEventListener('click', (e) => {
  if (e.target === e.currentTarget) closeKeyModal();
});
document.getElementById('ring-modal').addEventListener('click', (e) => {
  if (e.target === e.currentTarget) closeRingModal();
});
document.getElementById('reboot-modal').addEventListener('click', (e) => {
  if (e.target === e.currentTarget) closeRebootModal();
});
document.addEventListener('keydown', (e) => {
  if (e.key === 'Escape' && !document.getElementById('key-modal').hidden) closeKeyModal();
  if (e.key === 'Escape' && !document.getElementById('ring-modal').hidden) closeRingModal();
  if (e.key === 'Escape' && !document.getElementById('reboot-modal').hidden) closeRebootModal();
  if (e.key === 'Escape' && ledColorPopover) closeLedColorPopover();
});

// Theme toggle (light / dark / auto). The initial theme is applied in the head
// to avoid a flash; here we wire up the buttons and the auto-follow behavior.
function applyTheme(theme) {
  const t = theme === 'auto'
    ? (matchMedia('(prefers-color-scheme: light)').matches ? 'light' : 'dark')
    : theme;
  document.documentElement.setAttribute('data-theme', t);
}

matchMedia('(prefers-color-scheme: light)').addEventListener('change', () => {
  if (localStorage.getItem('theme') === 'auto') applyTheme('auto');
});

const savedTheme = localStorage.getItem('theme') || 'auto';
document.querySelectorAll('.theme-btn').forEach((btn) => {
  if (btn.dataset.theme === savedTheme) btn.classList.add('active');
  btn.addEventListener('click', () => {
    localStorage.setItem('theme', btn.dataset.theme);
    applyTheme(btn.dataset.theme);
    document.querySelectorAll('.theme-btn').forEach((b) => {
      b.classList.toggle('active', b === btn);
    });
  });
});

load().catch(loadError);
