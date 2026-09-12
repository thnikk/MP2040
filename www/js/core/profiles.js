// profiles.js — profile slots (see proto/config.proto) plus the small UI
// helpers the profile/LED controls use (icon spans, color pickers, color
// conversions). Plain script: top-level bindings are shared globals.

// Icon span (see the icon conventions in AGENTS.md): paints an icon from
// www/icons/ via mask-image in the current text color. Needs an explicit
// width/height from a per-context CSS rule to be visible.
function iconSpan(name) {
  const span = document.createElement('span');
  span.className = 'icon icon-' + name;
  span.setAttribute('aria-hidden', 'true');
  return span;
}

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
  // The spread above shares array references: per-key color edits must stay in
  // the working copy until synced, so copy the arrays (keycodes etc. already
  // slice above for the same reason).
  if (Array.isArray(profile.led?.ledNormalColors)) {
    options.led.ledNormalColors = profile.led.ledNormalColors.slice();
  }
  if (Array.isArray(profile.led?.ledPressedColors)) {
    options.led.ledPressedColors = profile.led.ledPressedColors.slice();
  }
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
  // Same aliasing hazard as above, in reverse: syncing must not link the slot
  // to the working copy's arrays.
  if (Array.isArray(options.led?.ledNormalColors)) {
    profile.led.ledNormalColors = options.led.ledNormalColors.slice();
  }
  if (Array.isArray(options.led?.ledPressedColors)) {
    profile.led.ledPressedColors = options.led.ledPressedColors.slice();
  }
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
  if (statusLedMinSlider) statusLedMinSlider.setValue(led.statusLedBrightnessMinimum ?? 0);
  if (statusLedMaxSlider) statusLedMaxSlider.setValue(led.statusLedBrightnessMaximum ?? 255);
}

function updateProfileTabs() {
  const tabs = document.querySelectorAll('#profile-tabs .profile-tab');
  tabs.forEach((btn, i) => {
    btn.classList.toggle('active', i === currentProfileIndex);
    btn.classList.toggle('boot', i === activeProfile);
  });
}

// List the other profiles in the "Copy from" select, excluding the one being
// edited (copying into itself is a no-op). The disabled placeholder is what
// the select shows between copies.
function refreshCopyProfileSelect() {
  const el = document.getElementById('copy-profile-src');
  if (!el) return;
  el.innerHTML = '';
  const placeholder = document.createElement('option');
  placeholder.value = '';
  placeholder.textContent = 'Copy from…';
  placeholder.disabled = true;
  placeholder.selected = true;
  el.appendChild(placeholder);
  for (let i = 0; i < PROFILE_COUNT; i++) {
    if (i === currentProfileIndex) continue;
    const opt = document.createElement('option');
    opt.value = String(i);
    opt.textContent = `Profile ${i + 1}`;
    el.appendChild(opt);
  }
}

// Whether the current profile has edits that haven't been saved yet, compared
// against its profile slot (the last state synced to it). Field-by-field so
// array length differences between the options and profile shapes don't count.
function profileEdited() {
  if (!currentOptions) return false;
  const a = cloneProfile(currentOptions);
  const b = profiles[currentProfileIndex] || cloneProfile();
  const len = Math.max(
    a.keycodes.length, b.keycodes.length,
    a.modifierMasks.length, b.modifierMasks.length,
    a.midiNotes.length, b.midiNotes.length,
    a.midiVelocities.length, b.midiVelocities.length,
    a.led.ledNormalColors.length, b.led.ledNormalColors.length,
    a.led.ledPressedColors.length, b.led.ledPressedColors.length,
  );
  const pad = (arr) => {
    const out = new Array(len).fill(0);
    for (let i = 0; i < arr.length; i++) out[i] = arr[i] || 0;
    return out;
  };
  const same = (x, y) => JSON.stringify(pad(x)) === JSON.stringify(pad(y));
  return !(
    same(a.keycodes, b.keycodes) &&
    same(a.modifierMasks, b.modifierMasks) &&
    same(a.midiNotes, b.midiNotes) &&
    same(a.midiVelocities, b.midiVelocities) &&
    a.midi.channel === b.midi.channel &&
    a.midi.velocity === b.midi.velocity &&
    a.led.ledMode === b.led.ledMode &&
    same(a.led.ledNormalColors, b.led.ledNormalColors) &&
    same(a.led.ledPressedColors, b.led.ledPressedColors)
  );
}

// Seed the current profile with another profile's mappings and per-key colors.
async function copyProfileFrom(src) {
  src = Number(src);
  if (src === currentProfileIndex) return;
  if (profileEdited()) {
    const choice = await confirmDialog({
      title: 'Copy profile',
      message: 'This profile has unsaved changes. Copying will replace them.',
      buttons: [
        { value: 'copy', label: 'Copy', kind: 'danger' },
        { value: 'cancel', label: 'Cancel' },
      ],
    });
    if (choice !== 'copy') return;
  }
  profiles[currentProfileIndex] = cloneProfile(profiles[src]);
  loadProfileIntoUi();
  updateProfileTabs();
  refreshCopyProfileSelect();
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
  refreshCopyProfileSelect();
}

function switchProfile(i) {
  if (i === currentProfileIndex || !currentOptions) return;
  syncCurrentToProfile();
  currentProfileIndex = i;
  loadProfileIntoUi();
  updateProfileTabs();
  refreshCopyProfileSelect();
}

function loadProfileIntoUi() {
  if (!currentOptions) return;
  const profile = profiles[currentProfileIndex] || cloneProfile(profiles[0]);
  applyProfileToOptions(profile, currentOptions);
  refreshPerProfileControls();
  if (boardView) boardView.setOptions(currentOptions);
}
