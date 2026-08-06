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
      ledSpeed: p.led?.ledSpeed ?? 50,
      brightnessMaximum: p.led?.brightnessMaximum ?? 255,
      brightnessSteps: p.led?.brightnessSteps ?? 1,
      colorNormal: p.led?.colorNormal ?? 0x00ff00,
      colorPressed: p.led?.colorPressed ?? 0xffffff,
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
  options.led = { ...(options.led || {}), ...profile.led };
}

// Copy `options`' per-profile fields back into a profile (opposite of above).
function applyOptionsToProfile(options, profile) {
  profile.keycodes = options.keycodes.slice();
  profile.modifierMasks = options.modifierMasks.slice();
  profile.midiNotes = options.midiNotes.slice();
  profile.midiVelocities = options.midiVelocities.slice();
  profile.midi = { ...(profile.midi || {}), ...(options.midi || {}) };
  profile.led = { ...(profile.led || {}), ...(options.led || {}) };
}

// Save any unsaved edits of the current tab back into its profile slot.
function syncCurrentToProfile() {
  if (!profiles[currentProfileIndex]) return;
  applyOptionsToProfile(currentOptions, profiles[currentProfileIndex]);
}

// Refresh the LED/MIDI controls from the current profile's values.
function refreshPerProfileControls() {
  const midi = currentOptions.midi || {};
  if (midiChannelSpinner) midiChannelSpinner.setValue(midi.channel ?? 0);
  if (midiVelocitySpinner) midiVelocitySpinner.setValue(midi.velocity ?? 127);
  const led = currentOptions.led || {};
  const ledModeEl = document.getElementById('led-mode');
  if (ledModeEl) ledModeEl.value = led.ledMode ?? 0;
  if (brightnessSlider) brightnessSlider.setValue(led.brightnessMaximum ?? 255);
  if (speedSlider) speedSlider.setValue(led.ledSpeed ?? 50);
  if (timeoutSpinner) timeoutSpinner.setValue(led.ledTimeout ?? 0);
  if (colorNormalPicker) colorNormalPicker.setValue(intToColor(led.colorNormal ?? 0x00ff00));
  if (colorPressedPicker) colorPressedPicker.setValue(intToColor(led.colorPressed ?? 0xffffff));
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
    const dot = document.createElement('span');
    dot.className = 'profile-tab-dot';
    btn.appendChild(dot);
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

// Read the current LED controls and push them to the board for a live preview.
async function previewLed() {
  const led = {
    ledMode: parseInt(document.getElementById('led-mode').value, 10),
    ledSpeed: speedSlider ? speedSlider.getValue() : 50,
    brightnessMaximum: brightnessSlider ? brightnessSlider.getValue() : 255,
    ledTimeout: timeoutSpinner ? timeoutSpinner.getValue() : 0,
    colorNormal: colorToInt(colorNormalPicker ? colorNormalPicker.getValue() : '#00ff00'),
    colorPressed: colorToInt(colorPressedPicker ? colorPressedPicker.getValue() : '#ffffff'),
  };
  try {
    await api('/api/setLedPreview', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ led }),
    });
    if (boardView) boardView.setLedParams(led);
  } catch (e) {
    Toast.show('Preview failed: ' + e, 'error');
  }
}

const previewLedDebounced = debounce(previewLed, 150);

// Global MIDI Channel / Velocity spinners (visible only in MIDI mode)
let midiChannelSpinner = null;
let midiVelocitySpinner = null;

// Gather the current controls into a full config payload for /api/setOptions.
// Includes the profile being edited (profileIndex) and the boot profile.
function buildOptionsBody() {
  return {
    keycodes: currentOptions.keycodes,
    modifierMasks: currentOptions.modifierMasks,
    midiNotes: currentOptions.midiNotes,
    midiVelocities: currentOptions.midiVelocities,
    defaultInputMode: parseInt(document.getElementById('default-input-mode').value, 10),
    midi: {
      channel: midiChannelSpinner ? midiChannelSpinner.getValue() : 0,
      velocity: midiVelocitySpinner ? midiVelocitySpinner.getValue() : 127,
    },
    led: {
      ledMode: parseInt(document.getElementById('led-mode').value, 10),
      ledSpeed: speedSlider ? speedSlider.getValue() : 50,
      brightnessMaximum: brightnessSlider ? brightnessSlider.getValue() : 255,
      ledTimeout: timeoutSpinner ? timeoutSpinner.getValue() : 0,
      colorNormal: colorToInt(colorNormalPicker ? colorNormalPicker.getValue() : '#00ff00'),
      colorPressed: colorToInt(colorPressedPicker ? colorPressedPicker.getValue() : '#ffffff'),
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
  // becomes visible, and only long-poll pin state there.
  if (route === '/layout') {
    if (boardView) boardView.refresh();
    pollPinState();
  } else {
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

async function load() {
  const [options, version] = await Promise.all([
    api('/api/getOptions'),
    api('/api/getFirmwareVersion'),
  ]);
  currentOptions = options;
  document.getElementById('board-label').textContent = version.boardLabel || '';
  document.getElementById('board-label-hero').textContent = version.boardLabel || '';
  document.getElementById('landing-version').textContent = version.firmwareVersion
    ? `Firmware ${version.firmwareVersion} · ${version.gitCommit || ''}`.trim()
    : '';

  const led = options.led || {};
  const midi = options.midi || {};

  document.getElementById('default-input-mode').value = options.defaultInputMode ?? 1;
  document.getElementById('default-input-mode').addEventListener('change', () => {
    currentOptions.defaultInputMode = parseInt(document.getElementById('default-input-mode').value, 10);
    updateModalMode();
    if (boardView) boardView.refresh();
  });

  midiChannelSpinner = new Spinner({
    container: document.getElementById('midi-channel'),
    min: 0,
    max: 15,
    value: midi.channel ?? 0,
    onChange: () => {},
  });
  midiVelocitySpinner = new Spinner({
    container: document.getElementById('midi-velocity'),
    min: 1,
    max: 127,
    value: midi.velocity ?? 127,
    onChange: () => {},
  });

  document.getElementById('led-mode').value = led.ledMode ?? 0;
  document.getElementById('led-mode').addEventListener('change', previewLed);

  brightnessSlider = new PillSlider({
    container: document.getElementById('led-brightness'),
    min: 0,
    max: 255,
    label: 'Brightness',
    value: led.brightnessMaximum ?? 255,
    padLength: 3,
    onChange: previewLed,
  });

  speedSlider = new PillSlider({
    container: document.getElementById('led-speed'),
    min: 0,
    max: 100,
    label: 'Speed',
    value: led.ledSpeed ?? 50,
    padLength: 3,
    onChange: previewLed,
  });

  timeoutSpinner = new Spinner({
    container: document.getElementById('led-timeout'),
    min: 0,
    max: 600,
    value: led.ledTimeout ?? 0,
    onChange: previewLed,
  });

  colorNormalPicker = createColorPicker(document.getElementById('led-colorNormal'), {
    label: 'Normal',
    value: intToColor(led.colorNormal ?? 0x00ff00),
    onChange: previewLedDebounced,
  });
  colorPressedPicker = createColorPicker(document.getElementById('led-colorPressed'), {
    label: 'Pressed',
    value: intToColor(led.colorPressed ?? 0xffffff),
    onChange: previewLedDebounced,
  });

  modalSelect = new MultiSelect({
    container: document.getElementById('key-modal-select'),
    options: MULTISELECT_OPTIONS,
    groups: MULTISELECT_GROUPS,
    onChange: () => {
      const { keycode, mask } = modalSelect.getValue();
      keyboardWidget.setValue(keycode, mask);
    },
  });

  keyboardWidget = new KeyboardWidget({
    container: document.getElementById('key-modal-keyboard'),
    keycode: 0,
    mask: 0,
    onChange: (keycode, mask) => {
      modalSelect.setValue(keycode, mask);
    },
  });

  midiKeyboard = new MidiKeyboard({
    container: document.getElementById('key-modal-midi'),
    value: 0,
    onChange: () => {},
  });

  initBoard(options);
  updateModalMode();

  // Profiles: default the editor to the active profile so what the user sees
  // matches what boots. Tabs and the boot-profile selector are wired below.
  profiles = Array.isArray(options.profiles) && options.profiles.length >= PROFILE_COUNT
    ? options.profiles.map(cloneProfile)
    : [options, options, options, options].map(cloneProfile);
  activeProfile = Number(options.activeProfile ?? 0);
  if (activeProfile < 0 || activeProfile >= PROFILE_COUNT) activeProfile = 0;
  currentProfileIndex = activeProfile;

  buildProfileTabs();

  const activeSelect = document.getElementById('active-profile');
  if (activeSelect) {
    activeSelect.value = String(activeProfile);
    activeSelect.addEventListener('change', () => {
      activeProfile = parseInt(activeSelect.value, 10) || 0;
      updateProfileTabs();
    });
  }

  loadProfileIntoUi();

  renderRoute();
}

// Show either the key/modifier pickers (keyboard mode) or the MIDI note picker
// (MIDI mode) in the modal, based on the current default input mode. Also
// reveals the MIDI Channel / Velocity controls and swaps the Board card
// description to match the mode.
function updateModalMode() {
  const midiMode = Number(currentOptions.defaultInputMode || 1) === 2;
  document.getElementById('key-modal-select').hidden = midiMode;
  document.getElementById('key-modal-keyboard').hidden = midiMode;
  document.getElementById('key-modal-midi').hidden = !midiMode;
  document.getElementById('midi-settings').hidden = !midiMode;
  document.getElementById('key-modal-hint').textContent = midiMode
    ? 'Pick a MIDI note for this button (0 = no note).'
    : 'Pick a key and any number of modifiers from the Modifiers group, or click a key on the keyboard below.';
  document.getElementById('board-hint').textContent = midiMode
    ? 'Click a button on the board to set its MIDI note and velocity.'
    : 'Click a button on the board to set its key and modifiers.';
}

function initBoard(options) {
  const panel = document.getElementById('board-panel');
  if (!panel) return;

  boardView = new BoardView(panel, {
    onPinClick: (pin) => openKeyModal(pin),
  });
  boardView.setOptions(options);
}

function openKeyModal(pin) {
  editingPin = pin;
  document.getElementById('key-modal-title').textContent =
    `GP${pin.toString().padStart(2, '0')}`;
  const keycode = Number(currentOptions.keycodes[pin] || 0);
  const mask = Number(currentOptions.modifierMasks[pin] || 0);
  const midiNote = Number(currentOptions.midiNotes?.[pin] || 0);
  modalSelect.setValue(keycode, mask);
  keyboardWidget.setValue(keycode, mask);
  midiKeyboard.setValue(midiNote);
  midiKeyboard.setVelocity(Number(currentOptions.midiVelocities?.[pin] || 0));
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
  const { keycode, mask } = modalSelect.getValue();
  currentOptions.keycodes[editingPin] = keycode;
  currentOptions.modifierMasks[editingPin] = mask;
  if (!currentOptions.midiNotes) currentOptions.midiNotes = new Array(30).fill(0);
  if (!currentOptions.midiVelocities) currentOptions.midiVelocities = new Array(30).fill(0);
  currentOptions.midiNotes[editingPin] = midiKeyboard.getValue();
  currentOptions.midiVelocities[editingPin] = midiKeyboard.getVelocity();
  if (boardView) boardView.setOptions(currentOptions);
  closeKeyModal();
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
document.getElementById('key-modal-save').addEventListener('click', saveKeyModal);
document.getElementById('key-modal-close').addEventListener('click', closeKeyModal);

document.getElementById('reboot-modal-close').addEventListener('click', closeRebootModal);
document.getElementById('reboot-normal').addEventListener('click', () => rebootTo(0));
document.getElementById('reboot-bootloader').addEventListener('click', () => rebootTo(2));
document.getElementById('reboot-webconfig').addEventListener('click', () => rebootTo(1));

// Close the modal when clicking the overlay backdrop or pressing Escape.
document.getElementById('key-modal').addEventListener('click', (e) => {
  if (e.target === e.currentTarget) closeKeyModal();
});
document.getElementById('reboot-modal').addEventListener('click', (e) => {
  if (e.target === e.currentTarget) closeRebootModal();
});
document.addEventListener('keydown', (e) => {
  if (e.key === 'Escape' && !document.getElementById('key-modal').hidden) closeKeyModal();
  if (e.key === 'Escape' && !document.getElementById('reboot-modal').hidden) closeRebootModal();
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

load();
