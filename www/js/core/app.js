// app.js — configurator orchestrator: API, pin/combo helpers, save
// payload, dirty tracking, routing, version check, page init (load()),
// persistence actions and global event wiring. Domain chunks live in
// state.js, keycodes.js, profiles.js, led.js and modals.js (plain
// scripts loaded before this one, sharing globals).

async function api(path, options) {
  const res = await fetch(path, options);
  return res.json();
}

// Debounce a function by `ms`; trailing edge fires the last call. The
// returned function carries .cancel() so pending writes can be revoked
// (e.g. a draft persist must not land after the draft was cleared).
function debounce(fn, ms) {
  let timer = null;
  const debounced = (...args) => {
    clearTimeout(timer);
    timer = setTimeout(() => fn(...args), ms);
  };
  debounced.cancel = () => clearTimeout(timer);
  return debounced;
}

// Live LED preview, debounced (previewLed itself lives in led.js).
const previewLedDebounced = debounce(previewLed, 150);

// Per-pin mapping for a menu-combo / boot-key option, mirroring the board
// view: the pin's action in the current input mode (key assignment, macro,
// MIDI note or gamepad controls) when set, otherwise no action. Returns the
// pin label and the action separately so the UI can render the action in a
// pill next to the pin.
function pinAction(options, index) {
  const mode = Number(options.defaultInputMode || 1);
  const midiMode = mode === 2;
  const gamepadMode = mode === 3 || mode === 4 || mode === 5 || mode === 6 || mode === 7 || mode === 8;
  const macroIndex = Number(options.macroIndices?.[index] || 0);
  const midiNote = Number(options.midiNotes?.[index] || 0);
  const gamepadMask = Number(options.gamepadMasks?.[index] || 0);

  let action = '';
  if (midiMode) {
    if (midiNote > 0) action = midiNoteName(midiNote);
  } else if (gamepadMode) {
    // Label the controls per the active layout (Xbox / Switch names, honoring
    // the Nintendo-layout toggle), matching the board view and controller widget.
    if (gamepadMask > 0) {
      const set = labelSet(mode, options.gamepad?.useNintendoLayout === true);
      action = controlsForMask(gamepadMask, set).map((c) => c.text).join('+');
    }
  } else if (macroIndex > 0) {
    action = 'M' + macroIndex;
  } else {
    const mods = [];
    const mask = Number(options.modifierMasks?.[index] || 0);
    for (let i = 0; i < 8; i++) {
      if (mask & (1 << i)) mods.push(MODIFIER_SHORT[0xe0 + i] || '');
    }
    const code = Number(options.keycodes?.[index] || 0);
    const key = code ? keyLabel(code) : '';
    action = mods.length ? mods.join('+') + '+' + key : key;
  }
  return { pin: `Pin ${index}`, action };
}

// Plain-text label for a pin ("Pin 5" or "Pin 5 (A)"), used for search / a11y
// text and any place that can't render the action pill.
function comboPinLabel(options, index) {
  const { pin, action } = pinAction(options, index);
  return action ? `${pin} (${action})` : pin;
}

function getMappablePins(options) {
  if (Array.isArray(options?.mappablePins) && options.mappablePins.length > 0) {
    return options.mappablePins;
  }
  const count = (options?.keycodes || []).length;
  if (options?.matrix?.enabled) {
    return Array.from({ length: count }, (_, i) => i);
  }
  const mappable = [];
  for (let i = 0; i < count; i++) {
    if (
      (options?.keycodes && options.keycodes[i] !== 0) ||
      (options?.modifierMasks && options.modifierMasks[i] !== 0) ||
      (options?.gamepadMasks && options.gamepadMasks[i] !== 0) ||
      (options?.midiNotes && options.midiNotes[i] !== 0) ||
      (options?.macroIndices && options.macroIndices[i] !== 0)
    ) {
      mappable.push(i);
    }
  }
  return mappable.length > 0 ? mappable : Array.from({ length: count }, (_, i) => i);
}

// Options for the menu-combo / hotkey / boot-key multi-selects: one entry per
// mappable key, labeled with the pin's mapping in the current input mode. group is the
// MultiSelect group id to attach them to. Each option carries the pin label
// and action separately so the widget can render the action in a pill.
function buildComboOptions(group = 'combo') {
  const pins = getMappablePins(currentOptions);
  return pins.map((i) => ({
    group,
    value: i,
    ...pinAction(currentOptions, i),
    label: comboPinLabel(currentOptions, i),
  }));
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
    // Incomplete hotkey rows (keys without an action, or vice versa) are
    // inert: never sent to the firmware. This is also the dirty-tracking
    // baseline, so half-filled rows neither mark dirty nor stick around.
    hotkeys: hotkeysPanel ? hotkeysPanel.getActiveValue() : (currentOptions.hotkeys || []),
    bootKeys: bootKeysPanel ? bootKeysPanel.getValue() : (currentOptions.bootKeys || []),
    defaultInputMode: parseInt(document.getElementById('default-input-mode').value, 10),
    debounceInterval: debounceSpinner ? debounceSpinner.getValue() : 5,
    touchMargin: touchMarginSpinner ? touchMarginSpinner.getValue() : 15,
    touchRelease: touchReleaseSpinner ? touchReleaseSpinner.getValue() : 10,
    serialConfigEnabled: serialPill ? serialPill.checked : false,
    midi: {
      channel: midiChannelSpinner ? midiChannelSpinner.getValue() : 0,
      velocity: midiVelocitySpinner ? midiVelocitySpinner.getValue() : 127,
    },
    gamepad: {
      socdMode: document.getElementById('socd-mode')
        ? parseInt(document.getElementById('socd-mode').value, 10)
        : 0,
      dpadMode: document.getElementById('dpad-mode')
        ? parseInt(document.getElementById('dpad-mode').value, 10)
        : 0,
      useNintendoLayout: nintendoPill ? nintendoPill.checked : false,
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
      statusLedEnabled: statusLedPill ? statusLedPill.checked : false,
      statusLedBrightnessMinimum: statusLedMinSlider ? statusLedMinSlider.getValue() : 0,
      statusLedBrightnessMaximum: statusLedMaxSlider ? statusLedMaxSlider.getValue() : 255,
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
      inputHistoryEnabled: displayHistoryPill ? displayHistoryPill.checked : false,
      inputHistoryTimeout: displayHistoryTimeoutSpinner ? displayHistoryTimeoutSpinner.getValue() : 3,
    },
    profileIndex: currentProfileIndex,
    activeProfile,
  };
}

// ---- dirty-state tracking -------------------------------------------------
// A config has unsaved changes when (a) the active working copy differs from
// its in-memory profile slot (profileEdited, e.g. an edit not yet synced by a
// tab switch), (b) any in-memory slot has drifted from the board snapshot
// (profilesDirty, e.g. edits synced when switching away), or (c) a global
// setting differs from the board snapshot (globalsDirty). profileIndex is
// selection state, not a setting, so it's excluded; activeProfile ("Set as
// Default") is a real global setting and is included.

// Global-only view of the save payload: everything except per-profile fields
// (key mappings, MIDI channel/velocity, per-key LED colors) and profileIndex.
// Deep-copied so the snapshot is independent of the live arrays: saveKeyModal
// and the macro editor mutate currentOptions' arrays in place, which would
// otherwise change the snapshot too and hide the edit.
function buildGlobalState() {
  const body = buildOptionsBody();
  const { ledMode: _ledMode, ledNormalColors: _ln, ledPressedColors: _lp, ...led } = body.led || {};
  return JSON.parse(JSON.stringify({
    macros: body.macros,
    macroIndices: body.macroIndices,
    gamepadMasks: body.gamepadMasks,
    defaultInputMode: body.defaultInputMode,
    debounceInterval: body.debounceInterval,
    touchMargin: body.touchMargin,
    touchRelease: body.touchRelease,
    serialConfigEnabled: body.serialConfigEnabled,
    gamepad: body.gamepad,
    ring: body.ring,
    display: body.display,
    hotkeys: body.hotkeys,
    bootKeys: body.bootKeys,
    activeProfile: body.activeProfile,
    led,
  }));
}

function profilesDirty() {
  if (!savedProfiles) return false;
  return JSON.stringify(profiles.map(cloneProfile)) !== JSON.stringify(savedProfiles);
}

function globalsDirty() {
  if (!savedGlobals) return false;
  return JSON.stringify(buildGlobalState()) !== JSON.stringify(savedGlobals);
}

// Any unsaved changes to the config (working copy, profile slots or globals).
function isDirty() {
  if (!currentOptions) return false;
  return profileEdited() || profilesDirty() || globalsDirty();
}

// ---- per-section dirty tracking -------------------------------------------
// The Save buttons show the global state; section headings and profile tabs
// each get their own dot so the edit's location is visible without hunting.
// Every check diffs against the board snapshots (savedProfiles/savedGlobals),
// split by where the field lives in the UI rather than where it lives in the
// payload: per-profile MIDI channel/velocity counts as Input, per-key LED
// colors and LED mode count as LEDs, macro assignment counts as Board while
// macro definitions count as Macros.

function subsetEq(g, sg, keys) {
  if (!g || !sg) return true;
  return keys.every((k) => JSON.stringify(g[k]) === JSON.stringify(sg[k]));
}

function ledSubsetEq(g, sg, keys) {
  if (!g || !sg) return true;
  return keys.every((k) => JSON.stringify(g.led?.[k]) === JSON.stringify(sg.led?.[k]));
}

// Full-profile slot diff (any per-profile field), for the tab dots.
function slotDirty(i) {
  if (!savedProfiles || !profiles[i] || !savedProfiles[i]) return false;
  return JSON.stringify(cloneProfile(profiles[i])) !== JSON.stringify(savedProfiles[i]);
}

// Board-mapping part of a profile (keycodes, modifiers, MIDI notes,
// velocities): the working copy vs its slot, and slots vs the snapshot.
function boardFieldsOf(p) {
  return [p.keycodes || [], p.modifierMasks || [], p.midiNotes || [], p.midiVelocities || []];
}

function workingBoardEdited() {
  if (!currentOptions || !profiles[currentProfileIndex]) return false;
  const a = boardFieldsOf(cloneProfile(currentOptions));
  const b = boardFieldsOf(profiles[currentProfileIndex]);
  const len = Math.max(...a.concat(b).map((arr) => arr.length), 0);
  const pad = (arr) => {
    const out = new Array(len).fill(0);
    for (let i = 0; i < arr.length; i++) out[i] = arr[i] || 0;
    return out;
  };
  return a.some((arr, i) => JSON.stringify(pad(arr)) !== JSON.stringify(pad(b[i])));
}

function anySlotBoardDirty() {
  if (!savedProfiles) return false;
  return profiles.some((p, i) => {
    if (!savedProfiles[i]) return false;
    const a = boardFieldsOf(cloneProfile(p));
    const b = boardFieldsOf(savedProfiles[i]);
    const len = Math.max(...a.concat(b).map((arr) => arr.length), 0);
    const pad = (arr) => {
      const out = new Array(len).fill(0);
      for (let j = 0; j < arr.length; j++) out[j] = arr[j] || 0;
      return out;
    };
    return a.some((arr, j) => JSON.stringify(pad(arr)) !== JSON.stringify(pad(b[j])));
  });
}

// Per-profile MIDI channel/velocity (edited in Input, stored per-profile).
function midiOf(p) {
  return p.midi || { channel: 0, velocity: 127 };
}

function workingMidiEdited() {
  if (!currentOptions || !profiles[currentProfileIndex]) return false;
  const a = midiOf(cloneProfile(currentOptions));
  const b = midiOf(profiles[currentProfileIndex]);
  return a.channel !== b.channel || a.velocity !== b.velocity;
}

function anySlotMidiDirty() {
  if (!savedProfiles) return false;
  return profiles.some((p, i) => {
    if (!savedProfiles[i]) return false;
    const a = midiOf(cloneProfile(p));
    const b = midiOf(savedProfiles[i]);
    return a.channel !== b.channel || a.velocity !== b.velocity;
  });
}

// Per-profile LED fields (mode + per-key colors, edited in the LEDs card).
function ledProfileOf(p) {
  return {
    ledMode: p.led?.ledMode ?? 0,
    ledNormalColors: p.led?.ledNormalColors || [],
    ledPressedColors: p.led?.ledPressedColors || [],
  };
}

function workingLedProfileEdited() {
  if (!currentOptions || !profiles[currentProfileIndex]) return false;
  return JSON.stringify(ledProfileOf(cloneProfile(currentOptions))) !==
    JSON.stringify(ledProfileOf(profiles[currentProfileIndex]));
}

function anySlotLedProfileDirty() {
  if (!savedProfiles) return false;
  return profiles.some((p, i) => {
    if (!savedProfiles[i]) return false;
    return JSON.stringify(ledProfileOf(cloneProfile(p))) !==
      JSON.stringify(ledProfileOf(savedProfiles[i]));
  });
}

const INPUT_GLOBAL_KEYS = ['defaultInputMode', 'debounceInterval', 'touchMargin', 'touchRelease', 'serialConfigEnabled', 'gamepad', 'ring'];
const LED_GLOBAL_KEYS = ['ledSpeeds', 'brightnessByMode', 'colorNormalByMode', 'colorPressedByMode'];
const LED_SETTINGS_KEYS = ['ledTimeout', 'statusLedEnabled', 'statusLedBrightnessMinimum', 'statusLedBrightnessMaximum'];

function sectionDirty(id) {
  if (!currentOptions || !savedGlobals) return false;
  let g = null;
  try {
    g = buildGlobalState();
  } catch (e) {
    return false;
  }
  const sg = savedGlobals;
  switch (id) {
    case 'profile-section':
      return !subsetEq(g, sg, ['activeProfile']);
    case 'board-section':
      return workingBoardEdited() || anySlotBoardDirty() ||
        !subsetEq(g, sg, ['macroIndices', 'gamepadMasks']);
    case 'led-section':
      return workingLedProfileEdited() || anySlotLedProfileDirty() ||
        !ledSubsetEq(g, sg, LED_GLOBAL_KEYS);
    case 'input-section':
      return workingMidiEdited() || anySlotMidiDirty() ||
        !subsetEq(g, sg, INPUT_GLOBAL_KEYS);
    case 'led-settings-section':
      return !ledSubsetEq(g, sg, LED_SETTINGS_KEYS);
    case 'display-settings':
      return !subsetEq(g, sg, ['display']);
    case 'macros-section':
      return !subsetEq(g, sg, ['macros']);
    case 'hotkeys-section':
      return !subsetEq(g, sg, ['hotkeys']);
    case 'bootkeys-section':
      return !subsetEq(g, sg, ['bootKeys']);
    default:
      return false;
  }
}

// Toggle the unsaved-changes indicator on the Save buttons (both pages),
// plus per-section heading dots and per-profile tab dots. Save/Discard are
// truly disabled while clean so the dim state means "nothing to do".
function updateDirtyUi() {
  if (saving) return;
  const dirty = isDirty();
  const blocked = typeof hotkeysPanel !== 'undefined' && hotkeysPanel && hotkeysPanel.hasConflicts();
  document.querySelectorAll('#save, #save-settings').forEach((btn) => {
    btn.classList.toggle('dirty', dirty);
    btn.classList.toggle('blocked', !!blocked);
    btn.disabled = !dirty;
    btn.title = blocked ? 'Fix the conflicting hotkeys before saving'
      : dirty ? 'Unsaved changes (Ctrl+S)' : 'Save (Ctrl+S)';
  });
  document.querySelectorAll('#discard, #discard-settings').forEach((btn) => {
    btn.classList.toggle('dirty', dirty);
    btn.disabled = !dirty;
    btn.title = dirty ? 'Discard unsaved changes' : 'No unsaved changes';
  });
  for (const id of ['profile-section', 'board-section', 'led-section', 'input-section',
    'led-settings-section', 'display-settings', 'macros-section', 'hotkeys-section',
    'bootkeys-section']) {
    const el = document.getElementById(id);
    if (el) el.classList.toggle('section-dirty', sectionDirty(id));
  }
  document.querySelectorAll('#profile-tabs .profile-tab').forEach((btn, i) => {
    const dot = slotDirty(i) || (i === currentProfileIndex && profileEdited());
    btn.classList.toggle('dirty', !!dot);
  });
  refreshBoardEmptyHint();
  schedulePersistDraft();
}

// Empty-board guidance (not a warning): shown when no inputs are mapped in
// the working copy or any profile slot.
function anyMappedInputs() {
  if (!currentOptions) return true;
  for (const p of [currentOptions, ...profiles]) {
    const arrs = [p.keycodes, p.modifierMasks, p.midiNotes, p.midiVelocities,
      p.gamepadMasks, p.macroIndices];
    if (arrs.some((a) => Array.isArray(a) && a.some((v) => Number(v) !== 0))) return true;
  }
  return false;
}

function refreshBoardEmptyHint() {
  const el = document.getElementById('board-empty-hint');
  if (el) el.hidden = anyMappedInputs();
}

const refreshDirtyUi = debounce(updateDirtyUi, 80);
// Covers every edit path: native selects/inputs fire `change`/`input`, and the
// custom pill/slider widgets and modal Save buttons fire `click` but no DOM
// event, so all three are needed.
document.addEventListener('change', refreshDirtyUi);
document.addEventListener('input', refreshDirtyUi);
document.addEventListener('click', refreshDirtyUi);

// Warn before losing unsaved changes: browser back/forward (popstate), SPA
// route changes (navigate) and tab close/refresh. Intentional reloads (import,
// mock board switch) set `allowUnload` so they don't trigger the prompt.
window.addEventListener('beforeunload', (e) => {
  if (allowUnload || !isDirty()) return;
  // Auto mode keeps a draft backstop, so refresh/close needs no prompt: force
  // the (debounced) write through synchronously and reload silently. Ask/off
  // keep the native dialog since nothing restores the edits.
  if (getRestoreMode() === 'auto' && !saving) {
    persistDraft();
    return;
  }
  e.preventDefault();
  e.returnValue = '';
});

// ---- draft persistence (localStorage backstop) ------------------------------
// Unsaved edits are stashed per board + firmware version, so a refresh the
// page can't intercept (toolbar button, address bar, tab close) still offers
// them back on load. Writes happen only while dirty and are cleared on save;
// mismatched board/version keys are ignored, never restored.
const DRAFT_MODE_KEY = 'mp2040-restore-mode';
let draftBoard = '';
let draftFirmware = '';

// Restore behavior: 'auto' (default), 'ask' or 'off'. Browser-local, never
// sent to the board.
function getRestoreMode() {
  try {
    return localStorage.getItem(DRAFT_MODE_KEY) || 'auto';
  } catch (e) {
    return 'auto';
  }
}

function setRestoreMode(mode) {
  try {
    localStorage.setItem(DRAFT_MODE_KEY, mode);
  } catch (e) {
    // Private mode / quota: drafts just stay unavailable.
  }
}

function draftKey() {
  return `mp2040-draft:${draftBoard}:${draftFirmware}`;
}

function clearDraft() {
  // Revoke a pending debounced write first: location.reload() doesn't preempt
  // already-scheduled timers, so without this a persist landing between the
  // clear and the reload would resurrect the draft (discard appearing to do
  // nothing). Safe to call before persistDraftDebounced exists (post-load
  // callers only run after full init).
  try {
    if (typeof persistDraftDebounced === 'function') persistDraftDebounced.cancel();
  } catch (e) {
    // Ignore (temporal dead zone before init; nothing scheduled yet).
  }
  try {
    localStorage.removeItem(draftKey());
  } catch (e) {
    // Ignore.
  }
}

function persistDraft() {
  // The page is intentionally going away (discard/import/board-switch accepted
  // a reload): never write then. The modal confirm click bubbles to document
  // and schedules a fresh debounced persist *after* clearDraft() already ran;
  // without this, that write landing before a slow reload commits would
  // resurrect the draft.
  if (allowUnload) return;
  // Off means no drafts at all: drop any lingering one, even when switched
  // programmatically rather than through the select.
  if (getRestoreMode() === 'off') {
    clearDraft();
    return;
  }
  if (!currentOptions || saving) return;
  if (!savedGlobals || !profiles.length) return;
  try {
    // Clean means the board holds everything: no draft needed. This also
    // clears after a successful save (updateDirtyUi runs there).
    if (!isDirty()) {
      clearDraft();
      return;
    }
    localStorage.setItem(draftKey(), JSON.stringify({
      savedAt: Date.now(),
      slots: profiles.map(cloneProfile),
      globals: buildGlobalState(),
      working: cloneProfile(currentOptions),
      activeProfile,
      currentProfileIndex,
    }));
  } catch (e) {
    // Private mode / quota: the beforeunload prompt remains the backstop.
  }
}

const persistDraftDebounced = debounce(persistDraft, 500);

function schedulePersistDraft() {
  if (!currentOptions || !savedGlobals || !profiles.length) return;
  persistDraftDebounced();
}

// A stored draft, or null when missing, malformed, disabled, or for another
// board/version (the key already namespaces board + firmware).
function readDraft() {
  try {
    if (getRestoreMode() === 'off') {
      clearDraft();
      return null;
    }
    const raw = localStorage.getItem(draftKey());
    if (!raw) return null;
    const d = JSON.parse(raw);
    if (!d || !Array.isArray(d.slots) || !d.globals || !d.working) {
      clearDraft();
      return null;
    }
    return d;
  } catch (e) {
    return null;
  }
}

// Board-state view of the globals buildGlobalState() captures, derived purely
// from a getOptions response (no DOM): mirrors control initialization plus
// payload fallbacks, with canonical key orders so JSON comparison is exact.
// Used both to diff a draft and as the dirty baseline after a restore.
function buildBoardGlobals(options) {
  const led = options.led || {};
  const fill7 = (arr, fb) => (Array.isArray(arr) && arr.length >= 7) ? arr.slice() : new Array(7).fill(fb);
  const gamepad = options.gamepad || {};
  const display = options.display || {};
  return {
    macros: options.macros || [],
    macroIndices: Array.isArray(options.macroIndices)
      ? options.macroIndices.slice()
      : new Array(128).fill(0),
    gamepadMasks: options.gamepadMasks || [],
    defaultInputMode: parseInt(options.defaultInputMode ?? 1, 10),
    debounceInterval: options.debounceInterval ?? 5,
    touchMargin: options.touchMargin ?? 15,
    touchRelease: options.touchRelease ?? 10,
    serialConfigEnabled: options.serialConfigEnabled === true,
    gamepad: {
      socdMode: parseInt(gamepad.socdMode ?? 0, 10),
      dpadMode: parseInt(gamepad.dpadMode ?? 0, 10),
      useNintendoLayout: gamepad.useNintendoLayout === true,
    },
    ring: {
      ringStickTarget: options.ring?.ringStickTarget ?? 1,
      ringKeyboardMode: options.ring?.ringKeyboardMode ?? 2,
      ringScrollAxis: options.ring?.ringScrollAxis ?? 0,
      ringMidiBehavior: options.ring?.ringMidiBehavior ?? 1,
    },
    display: {
      size: display.size ?? 3,
      flip: display.flip ?? 0,
      invert: display.invert ?? false,
      splashDuration: display.splashDuration ?? 3,
      displaySaverTimeout: display.displaySaverTimeout ?? 0,
      displaySaverMode: parseInt(display.displaySaverMode ?? 0, 10),
      inputHistoryEnabled: display.inputHistoryEnabled !== false,
      inputHistoryTimeout: display.inputHistoryTimeout ?? 3,
    },
    // Same completeness rules as the panels: incomplete rows never reach the
    // payload, so they count as board state here too.
    hotkeys: (options.hotkeys || [])
      .filter((h) => (h.keys || []).length > 0 && Number(h.action) !== 0)
      .map((h) => ({ keys: h.keys.slice(0, 8), action: Number(h.action) || 0 })),
    bootKeys: (options.bootKeys || [])
      .filter((bk) => Number(bk.pin) >= 0)
      .map((bk) => ({ pin: Number(bk.pin), mode: Number(bk.mode) })),
    activeProfile: Number(options.activeProfile ?? 0),
    led: {
      ledSpeeds: fill7(led.ledSpeeds, Number.isFinite(led.ledSpeed) ? led.ledSpeed : 50),
      brightnessByMode: fill7(led.brightnessByMode,
        Number.isFinite(led.brightnessMaximum) ? led.brightnessMaximum : 255),
      ledTimeout: led.ledTimeout ?? 0,
      statusLedEnabled: Boolean(led.statusLedEnabled),
      statusLedBrightnessMinimum: led.statusLedBrightnessMinimum ?? 0,
      statusLedBrightnessMaximum: led.statusLedBrightnessMaximum ?? 255,
      colorNormalByMode: fill7(led.colorNormalByMode,
        Number.isFinite(led.colorNormal) ? led.colorNormal : 0x00ff00),
      colorPressedByMode: fill7(led.colorPressedByMode,
        Number.isFinite(led.colorPressed) ? led.colorPressed : 0xffffff),
    },
  };
}

// Fresh profile slots exactly as load() builds them.
function boardSlotsOf(options) {
  return (Array.isArray(options.profiles) && options.profiles.length >= PROFILE_COUNT)
    ? options.profiles.map(cloneProfile)
    : [options, options, options, options].map(cloneProfile);
}

// Whether applying the draft would change anything versus the fresh board
// state, so an already-saved draft clears silently instead of prompting.
function draftDiffersFromOptions(d, options) {
  const eq = (a, b) => JSON.stringify(a) === JSON.stringify(b);
  const freshSlots = boardSlotsOf(options);
  if (!eq(freshSlots, (d.slots || []).map(cloneProfile))) return true;
  if (!eq(buildBoardGlobals(options), d.globals || {})) return true;
  // Unsynced working-copy edits (no tab switch yet) live outside the slots.
  const idx = Number.isInteger(d.currentProfileIndex) &&
    d.currentProfileIndex >= 0 && d.currentProfileIndex < PROFILE_COUNT
    ? d.currentProfileIndex
    : Number(options.activeProfile ?? 0);
  if (!eq(cloneProfile(d.working), freshSlots[idx] || cloneProfile())) return true;
  return false;
}

// Merge a draft over fresh options in place, before UI init flows from it.
function applyDraftToOptions(options, d) {
  const g = d.globals || {};
  options.profiles = (d.slots || []).map(cloneProfile);
  options.activeProfile = (Number.isInteger(d.activeProfile) &&
    d.activeProfile >= 0 && d.activeProfile < PROFILE_COUNT) ? d.activeProfile : 0;
  options.macroIndices = g.macroIndices || [];
  options.macros = g.macros || [];
  options.gamepadMasks = g.gamepadMasks || [];
  options.defaultInputMode = g.defaultInputMode ?? 1;
  options.debounceInterval = g.debounceInterval ?? 5;
  options.touchMargin = g.touchMargin ?? 15;
  options.touchRelease = g.touchRelease ?? 10;
  options.serialConfigEnabled = g.serialConfigEnabled === true;
  options.gamepad = g.gamepad || {};
  options.ring = g.ring || {};
  options.display = g.display || {};
  options.hotkeys = g.hotkeys || [];
  options.bootKeys = g.bootKeys || [];
  options.led = { ...(options.led || {}), ...(g.led || {}) };
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

// In-app route switches (link navigation and browser back/forward) never
// prompt: both pages share one SPA state with a Save control each, so there
// is nothing to lose by switching. Prompts remain for paths that actually
// leave the state behind (reboot, reload, copy-profile, reset, tab close).
function navigate(path, event) {
  if (event) event.preventDefault();
  if (location.pathname === path) return;
  lastRoute = path;
  history.pushState({}, '', path);
  renderRoute();
  window.scrollTo(0, 0);
}

// Keyboard refresh (F5 / Ctrl+R). Auto mode reloads silently: the draft
// backstop restores the edits, so no prompt is needed. Ask/off keep the
// custom prompt (and beforeunload keeps the native dialog for refresh paths
// keyboard interception can't reach: toolbar button, address bar, tab close).
async function handleRefreshRequest() {
  if (rebooting || disconnected) return;
  if (allowUnload || !isDirty()) return;
  if (getRestoreMode() === 'auto' && !saving) {
    persistDraft();
    allowUnload = true;
    location.reload();
    return;
  }
  const choice = await confirmDialog({
    title: 'Unsaved changes',
    message: 'You have unsaved changes. Save them before reloading?',
    buttons: [
      { value: 'save', label: 'Save & Reload', kind: 'primary' },
      { value: 'discard', label: 'Discard & Reload' },
      { value: 'cancel', label: 'Cancel' },
    ],
  });
  if (choice === 'save') {
    if (!await save()) return;
    allowUnload = true;
    location.reload();
  } else if (choice === 'discard') {
    clearDraft();
    allowUnload = true;
    location.reload();
  }
}

let lastRoute = currentRoute();
window.addEventListener('popstate', () => {
  lastRoute = currentRoute();
  renderRoute();
});

// Live pin state: highlight buttons yellow while their physical switch is
// held. The board answers /api/getPinState only when a button actually
// changes (long-poll), so this is event-driven rather than polled. A small
// gap before each reconnect keeps the mock server (which answers instantly)
// from being hammered, and the whole loop pauses while the tab is hidden or
// the layout page isn't shown.
// True once a reboot has been accepted: the board is going away, so the
// pin-state long-poll must stay stopped and api errors must not surface.
let rebooting = false;
// True once an unexpected disconnect is confirmed (physical reset/unplug):
// same UI posture as a reboot, but entered from the heartbeat watcher below.
let disconnected = false;
let disconnectFailures = 0;
const DISCONNECT_THRESHOLD = 3;
let disconnectTimer = null;
let pinStateTimer = null;
function pollPinState() {
  if (document.hidden || rebooting || disconnected) return;
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
  else if (!rebooting && !disconnected && currentRoute() === '/layout') pollPinState();
});

// Unexpected-disconnect heartbeat: the pin-state long-poll can't tell an
// idle timeout from a dead board, so probe /api/getFirmwareVersion on a
// timer instead. Three consecutive failures (~6s) means a physical
// reset/unplug; a single blip just resets the counter on the next success.
function startDisconnectWatch() {
  if (disconnectTimer !== null) return;
  disconnectTimer = setInterval(checkBoardAlive, 2000);
}
async function checkBoardAlive() {
  if (document.hidden || rebooting || disconnected) return;
  try {
    await probeBoard(5000);
    disconnectFailures = 0;
  } catch (e) {
    disconnectFailures++;
    if (disconnectFailures >= DISCONNECT_THRESHOLD) enterDisconnectedState();
  }
}
// Physical reset/unplug: same posture as leaving web config — block the UI
// with the reboot overlay and reload when the board returns. Unsaved edits
// can't survive the reload, so say so instead of failing silently on save.
function enterDisconnectedState() {
  if (disconnected || rebooting) return;
  disconnected = true;
  stopPinState();
  allowUnload = true;
  const dirty = (() => {
    try {
      return isDirty();
    } catch (e) {
      return false;
    }
  })();
  showRebootedOverlay({
    title: 'Board disconnected',
    message: dirty
      ? 'The board stopped responding. Your unsaved changes are kept and will be offered back on reload.'
      : 'The board stopped responding.',
    hint: webConfigReturnHint(true),
    spinning: false,
    showBoard: true,
  });
  watchForBoardReturn(true);
}

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
    // Skip mock/dev servers and untagged (bare-SHA) dev builds. The firmware
    // version string is "dev" for untagged builds and "vX.Y.Z[+N]" otherwise;
    // gitCommit is always a bare SHA, so it can't gate this.
    if (version.mock || !/^v?\d+\.\d+\.\d+/.test(version.firmwareVersion || '')) return;
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
  // Draft restore runs before any UI init so restored values flow through
  // the normal path. Keyed by board + firmware: other boards/versions never
  // match, and an already-saved draft clears silently.
  draftBoard = version.boardLabel || '';
  draftFirmware = version.firmwareVersion || '';
  let draftData = readDraft();
  let useDraft = false;
  let draftMode = getRestoreMode();
  if (draftData && draftDiffersFromOptions(draftData, options)) {
    if (draftMode === 'ask') {
      const when = draftData.savedAt ? new Date(draftData.savedAt).toLocaleString() : '';
      useDraft = await confirmDialog({
        title: 'Unsaved changes',
        message: `Found unsaved changes${when ? ` from ${when}` : ''}. Restore them?`,
        buttons: [
          { value: 'restore', label: 'Restore', kind: 'primary' },
          { value: 'discard', label: 'Discard' },
        ],
      }) === 'restore';
      if (!useDraft) clearDraft();
    } else {
      useDraft = true;
    }
  } else if (draftData) {
    clearDraft();
    draftData = null;
  }
  // Board baselines for dirty tracking: captured before the draft merge so a
  // restore compares against the board, not against itself.
  let boardSlots = null;
  let boardGlobals = null;
  if (useDraft && draftData) {
    boardSlots = boardSlotsOf(options);
    boardGlobals = buildBoardGlobals(options);
    applyDraftToOptions(options, draftData);
  }
  // Key runtime asset fetches (board/controller graphics, gamepad glyphs)
  // by firmware: static files are served immutable, so the version query
  // keeps a firmware update from reusing stale cached copies. Set before
  // anything below fetches (board view, prefetch).
  assetVersion = [version.firmwareVersion, version.gitCommit].filter(Boolean).join('+');
  // Global macros: per-key triggers and the M1-M8 definitions. Default to
  // empty for old firmware responses that don't carry the fields.
  currentOptions.macroIndices = Array.isArray(options.macroIndices)
    ? options.macroIndices.slice()
    : new Array(128).fill(0);
  currentOptions.macros = Array.isArray(options.macros) ? options.macros : [];
  // Cache the board graphic now, while the board is definitely up. The
  // post-reboot overlay renders from this cache after the board disconnects.
  prefetchRebootBoard();
  document.getElementById('board-label-hero').textContent = version.boardLabel || '';
  document.getElementById('footer-version').textContent = version.firmwareVersion
    ? `${version.firmwareVersion}${version.gitCommit ? ` · ${version.gitCommit}` : ''}`
    : '';
  document.getElementById('landing-year').textContent =
    version.buildYear || new Date().getFullYear();
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
        // The new board resolves a different draft key, so this board's
        // draft (if any) is left alone and restores when switching back.
        allowUnload = true;
        window.location.reload();
      });
    }
  }

  const led = options.led || {};
  const midi = options.midi || {};

  document.getElementById('default-input-mode').value = options.defaultInputMode ?? 1;
  // PS4/PS5 are always listed: without a USB host port (see hasUsbHostPort
  // in webconfig.cpp) they run unauthenticated — fine on PC, ~8-minute
  // timeout on console. PS3 needs no host port.
  updatePsAuthHint();
  document.getElementById('default-input-mode').addEventListener('change', () => {
    currentOptions.defaultInputMode = parseInt(document.getElementById('default-input-mode').value, 10);
    updatePsAuthHint();
    updateModalMode();
    syncGamepadLabels();
    if (boardView) boardView.refresh();
    if (hotkeysPanel) hotkeysPanel.setKeyOptions(buildComboOptions('hotkey'));
    if (bootKeysPanel) bootKeysPanel.setKeyOptions(buildComboOptions('bootkey'));
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
  const dpadModeEl = document.getElementById('dpad-mode');
  if (dpadModeEl) {
    dpadModeEl.value = gamepad.dpadMode ?? 0;
    dpadModeEl.addEventListener('change', () => {
      if (!currentOptions.gamepad) currentOptions.gamepad = {};
      currentOptions.gamepad.dpadMode = parseInt(dpadModeEl.value, 10);
    });
  }
  const nintendoEl = document.getElementById('nintendo-layout-wrap');
  if (nintendoEl) {
    nintendoPill = new PillToggle(nintendoEl, {
      checked: gamepad.useNintendoLayout === true,
      onChange: (checked) => {
        if (!currentOptions.gamepad) currentOptions.gamepad = {};
        currentOptions.gamepad.useNintendoLayout = checked;
        syncGamepadLabels();
      },
    });
  }

  const serialEl = document.getElementById('serial-config');
  if (serialEl) {
    serialPill = new PillToggle(serialEl, {
      checked: options.serialConfigEnabled === true,
      onChange: (checked) => {
        currentOptions.serialConfigEnabled = checked;
      },
    });
  }

  const statusLedRowEl = document.getElementById('status-led-row');
  if (statusLedRowEl) {
    // Boards without a status LED don't show the toggle / min / max row.
    statusLedRowEl.hidden = options.led?.hasStatusLed === false;
  }
  const statusLedEl = document.getElementById('status-led');
  if (statusLedEl) {
    statusLedPill = new PillToggle(statusLedEl, {
      checked: Boolean(options.led?.statusLedEnabled),
      onChange: (checked) => {
        if (!currentOptions.led) currentOptions.led = {};
        currentOptions.led.statusLedEnabled = checked;
        previewLed();
      },
    });
  }

  // Status LED min/max brightness (the "device is powered" floor and the
  // runtime cap). The label lives inside each pill slider.
  const statusLedMinEl = document.getElementById('status-led-min');
  if (statusLedMinEl) {
    statusLedMinSlider = new PillSlider({
      container: statusLedMinEl,
      min: 0,
      max: 255,
      label: 'Minimum',
      value: options.led?.statusLedBrightnessMinimum ?? 0,
      padLength: 3,
      onChange: previewLed,
    });
  }
  const statusLedMaxEl = document.getElementById('status-led-max');
  if (statusLedMaxEl) {
    statusLedMaxSlider = new PillSlider({
      container: statusLedMaxEl,
      min: 0,
      max: 255,
      label: 'Maximum',
      value: options.led?.statusLedBrightnessMaximum ?? 255,
      padLength: 3,
      onChange: previewLed,
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
    displayHistoryPill = new PillToggle(displayHistoryEl, {
      checked: display.inputHistoryEnabled !== false,
      onChange: (checked) => {
        if (!currentOptions.display) currentOptions.display = {};
        currentOptions.display.inputHistoryEnabled = checked;
      },
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
    onChange: (v) => {
      if (!currentOptions.midi) currentOptions.midi = {};
      currentOptions.midi.channel = v;
    },
  });
  midiVelocitySpinner = new Spinner({
    container: document.getElementById('midi-velocity-spinner'),
    min: 1,
    max: 127,
    value: midi.velocity ?? 127,
    onChange: (v) => {
      if (!currentOptions.midi) currentOptions.midi = {};
      currentOptions.midi.velocity = v;
    },
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

  // Boards without an addressable LED strip (dataPin < 0, e.g. Fightboard-b)
  // hide the whole LEDs section. The widgets below still initialize while
  // hidden so save payloads, dirty tracking and profiles behave unchanged.
  // The Settings page timeout control stays visible; only its status LED row
  // is conditional (on hasStatusLed, handled above).
  const ledSectionEl = document.getElementById('led-section');
  if (ledSectionEl) {
    ledSectionEl.hidden = (options.led?.dataPin ?? -1) < 0;
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
      // Refresh immediately: MultiSelect picks fire no DOM change/input/click
      // of their own (the widget stops propagation), so the debounced global
      // listeners would otherwise leave dots/button state lagging a beat.
      onChange: (hotkeys) => { currentOptions.hotkeys = hotkeys; refreshDirtyUi(); },
    });
  }

  const bootKeysPanelEl = document.getElementById('boot-keys-panel');
  if (bootKeysPanelEl) {
    bootKeysPanel = new BootKeysPanel({
      container: bootKeysPanelEl,
      bootKeys: Array.isArray(options.bootKeys) ? options.bootKeys : [],
      keyOptions: buildComboOptions('bootkey'),
      // Board-fixed boot pins shown for reference (greyed out, not editable);
      // rows with an undefined pin are hidden.
      fixedKeys: [
        { label: 'Web Config', pin: options.webConfigPin ?? -1 },
        { label: 'USB Bootloader', pin: options.bootPin ?? -1 },
      ],
      onChange: (bootKeys) => { currentOptions.bootKeys = bootKeys; refreshDirtyUi(); },
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

  const { labels: gamepadLabels, glyphs: gamepadGlyphs, maskMap: gamepadMaskMap } =
    gamepadLabelSet(currentOptions.defaultInputMode ?? 1, currentOptions.gamepad?.useNintendoLayout === true);

  gamepadSelect = new MultiSelect({
    container: document.getElementById('key-modal-gamepad-select'),
    options: gamepadMultiOptions(gamepadLabels, gamepadMaskMap),
    groups: GAMEPAD_MULTISELECT_GROUPS,
    onChange: () => {
      if (gamepadWidget) gamepadWidget.setMask(gamepadSelect.getGroupMask('gamepad'));
    },
  });

  // The controller widget is the visual twin of the multi-select above; each
  // reflects the other (their set* methods don't fire onChange, so no loop).
  gamepadWidget = new ControllerWidget({
    container: document.getElementById('key-modal-gamepad-widget'),
    mask: 0,
    labels: gamepadLabels,
    glyphs: gamepadGlyphs,
    maskMap: gamepadMaskMap,
    onChange: (mask) => {
      gamepadSelect.setGroupMask('gamepad', mask);
    },
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

  // Draft working-copy edits (made without a tab switch) live outside the
  // slots: re-apply them over the mirrored slot and re-reflect the controls.
  // Globals and slots are already merged into `options` above, so panels
  // constructed them directly.
  if (useDraft && draftData) {
    if (Number.isInteger(draftData.currentProfileIndex) &&
        draftData.currentProfileIndex >= 0 && draftData.currentProfileIndex < PROFILE_COUNT) {
      currentProfileIndex = draftData.currentProfileIndex;
    }
    applyProfileToOptions(cloneProfile(draftData.working), currentOptions);
    refreshPerProfileControls();
    if (boardView) boardView.setOptions(currentOptions);
    updateProfileTabs();
    refreshCopyProfileSelect();
  }

  // Baseline for dirty tracking: the state exactly as loaded from the board
  // (after a restore, the pre-merge board snapshots, not the draft).
  savedProfiles = (useDraft && boardSlots) ? boardSlots : profiles.map(cloneProfile);
  savedGlobals = (useDraft && boardGlobals) ? boardGlobals : buildGlobalState();
  updateDirtyUi();

  renderRoute();

  const loading = document.getElementById('loading');
  if (loading) loading.hidden = true;
  if (useDraft && draftMode !== 'ask') Toast.show('Restored unsaved changes.', 'info');
  startDisconnectWatch();
}

function loadError() {
  const loading = document.getElementById('loading');
  if (loading) loading.hidden = true;
  // Board unreachable at startup (unplugged, wrong mode): block with the
  // same overlay rather than a blank page, and reload when it appears.
  disconnected = true;
  allowUnload = true;
  showRebootedOverlay({
    title: 'Board disconnected',
    message: 'The board stopped responding.',
    hint: webConfigReturnHint(true),
    spinning: false,
    showBoard: true,
  });
  watchForBoardReturn(true);
}

async function save() {
  // Conflicting hotkeys are never written: fix them first. All save paths
  // (buttons, Ctrl+S, Save & Leave/Reboot/Reload) funnel through here.
  if (typeof hotkeysPanel !== 'undefined' && hotkeysPanel && hotkeysPanel.hasConflicts()) {
    Toast.show('Fix the conflicting hotkeys before saving.', 'error');
    return false;
  }
  const saveBtns = [
    document.getElementById('save'), document.getElementById('save-settings'),
    document.getElementById('discard'), document.getElementById('discard-settings'),
  ].filter(Boolean);
  saveBtns.forEach((b) => { b.disabled = true; });
  saving = true;
  let ok = false;
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
    // The board now holds what we sent; the working copy, slots and globals
    // are the new baseline for dirty tracking.
    savedProfiles = profiles.map(cloneProfile);
    savedGlobals = buildGlobalState();
    // The board holds everything now: drop any draft synchronously (the
    // debounced persist may not fire before a save-and-leave reload).
    clearDraft();
    Toast.show('Saved.', 'success');
    ok = true;
  } catch (e) {
    Toast.show('Save failed: ' + e, 'error');
  }
  saving = false;
  saveBtns.forEach((b) => { b.disabled = false; });
  updateDirtyUi();
  refreshRebootDirtyUi();
  return ok;
}

// Cached /board.svg text for the reboot overlay. Fetched while the board is
// still up (when the reboot picker opens): after /api/reboot is accepted the
// firmware reboots ~500ms later, so fetching the graphic after the reboot
// races the disconnect and usually loses.
let rebootBoardSvg = null;

async function prefetchRebootBoard() {
  if (rebootBoardSvg !== null) return;
  try {
    const resp = await fetch(assetUrl('/board.svg'));
    if (!resp.ok) return;
    rebootBoardSvg = await resp.text();
  } catch (e) {
    // Board graphic unavailable; the overlay falls back to generic wording.
  }
}

// Show the full-page post-reboot overlay. The board is gone, so there is
// nothing to interact with: no buttons, no dismiss. `showBoard` renders the
// board graphic with the web config button highlighted (when the board has a
// web config pin and a board graphic is available).
async function showRebootedOverlay({ title, message, hint, spinning, showBoard }) {
  document.getElementById('rebooted-title').textContent = title;
  document.getElementById('rebooted-message').textContent = message || '';
  document.getElementById('rebooted-hint').textContent = hint || '';
  document.getElementById('rebooted-spinner').hidden = !spinning;
  document.getElementById('rebooted-board').hidden = true;
  document.getElementById('rebooted-status').textContent = '';
  document.getElementById('rebooted-status').hidden = true;
  document.getElementById('rebooted-overlay').hidden = false;
  if (showBoard && !(await renderRebootBoard())) {
    document.getElementById('rebooted-hint').textContent = webConfigReturnHint(false);
  }
}

// How to get back into the configurator after leaving web config. Touch
// boards can't hold a pad from power-on: the pad is touched after boot,
// inside the boot window. `withBoard` selects the highlighted-pad wording
// (board graphic shown) or the generic fallback. Assumes a touch board's web
// config pin is a touch pad, true for all shipping touch boards (the
// firmware arms the window when the web config or boot pin is touch, which
// the frontend can't distinguish from hasTouchPads alone).
function webConfigReturnHint(withBoard) {
  if (currentOptions?.touch?.hasTouchPads === true) {
    const target = withBoard ? 'the highlighted pad' : 'the web config pad';
    return `To open the configurator again, plug the board in, then touch ${target}.`;
  }
  const target = withBoard ? 'the highlighted button' : 'the web config button';
  return `To open the configurator again, hold ${target} while plugging the board in.`;
}

// Simplified board graphic for the reboot overlay: the served /board.svg with
// LEDs and label guides stripped out, all buttons dimmed except the web
// config button, which gets the same highlight as a held pin on the layout
// page. On matrix boards the web config pin is a linear key index (keyNN),
// on direct-pin boards a GPIO (pinNN), mirroring matchButtonIndex.
async function renderRebootBoard() {
  const pin = Number(currentOptions?.webConfigPin);
  const container = document.getElementById('rebooted-board');
  if (!Number.isInteger(pin) || pin < 0) return false;
  // Prefer the prefetched graphic; a live fetch is only a best-effort
  // fallback (the board is usually already gone by now).
  let text = rebootBoardSvg;
  if (text === null) {
    try {
      const resp = await fetch(assetUrl('/board.svg'));
      if (!resp.ok) return false;
      text = await resp.text();
    } catch (e) {
      return false;
    }
  }
  const doc = new DOMParser().parseFromString(text, 'image/svg+xml');
  if (!doc.querySelector('svg')) return false;
  // Strip LEDs, the status LED, and label-positioning guides: this graphic
  // carries no labels or live state. The OLED screen and splash logo follow
  // the board view: hidden unless the board physically has a display
  // (setDisplayElementsVisible lives in boardview.js, loaded before this
  // script).
  const hasDisplay = currentOptions?.display?.hasDisplay === true;
  doc.querySelectorAll('[id]').forEach((el) => {
    const names = [el.id, el.getAttribute('inkscape:label')].filter(Boolean);
    if (names.some((n) => /^led-?\d+$/i.test(n) || /-label$/i.test(n) || n === 'board-led' || n === 'led-alignment')) {
      el.remove();
    }
  });
  setDisplayElementsVisible(doc, hasDisplay);
  const isMatrix = !!currentOptions?.matrix?.enabled;
  // Compare numerically: board graphics mix padded (pin08) and unpadded
  // (pin8) ids, so an exact string match misses most boards.
  const numOf = (name, matrix) => {
    const m = (name || '').match(matrix ? /^key(\d+)$/i : /^pin(\d+)$/i);
    return m ? parseInt(m[1], 10) : null;
  };
  let target = null;
  const buttons = [];
  doc.querySelectorAll('[id]').forEach((el) => {
    for (const name of [el.id, el.getAttribute('inkscape:label')]) {
      if (!name || /-label$/i.test(name)) continue;
      if (numOf(name, isMatrix) === null) continue;
      buttons.push(el);
      if (numOf(name, isMatrix) === pin) target = el;
      break;
    }
  });
  // Fall back to the other naming scheme if the preferred one finds nothing
  // (some board graphics mix conventions).
  if (!target) {
    buttons.forEach((el) => {
      const names = [el.id, el.getAttribute('inkscape:label')];
      if (names.some((n) => numOf(n, !isMatrix) === pin)) target = el;
    });
  }
  if (!target) return false;
  const SHAPES = 'path, rect, circle, ellipse, polygon, polyline, line';
  const shapesOf = (el) => {
    if (/^(path|rect|circle|ellipse|polygon|polyline|line)$/i.test(el.tagName)) return [el];
    return Array.from(el.querySelectorAll(SHAPES));
  };
  // Base theme mirroring BoardView.themeStyle (matchesRef/findByRef live in
  // boardview.js, loaded before this script), one step up the palette: the
  // case takes --bg-2, buttons take --bg-3, no strokes.
  doc.querySelectorAll(SHAPES).forEach((s) => {
    s.removeAttribute('vector-effect');
    if (matchesRef(s, ['logo', 'ignore'])) return;
    s.style.setProperty('fill', 'var(--bg-2)', 'important');
    s.style.setProperty('stroke', 'none', 'important');
    s.style.removeProperty('stroke-width');
  });
  const oledEl = findByRef(doc, 'oled');
  if (oledEl) shapesOf(oledEl).forEach((s) => s.style.setProperty('fill', '#000000', 'important'));
  doc.querySelectorAll(SHAPES).forEach((s) => {
    if (matchesRef(s, ['boot', 'reset'])) {
      s.style.setProperty('fill', 'var(--bg-3)', 'important');
      s.style.setProperty('opacity', '0.5', 'important');
    }
  });
  // Buttons take --bg-3; the web config button keeps the held-pin highlight
  // and the rest are dimmed.
  buttons.forEach((el) => {
    if (el === target) {
      shapesOf(el).forEach((s) => {
        s.style.setProperty('fill', 'var(--bg-3)', 'important');
        s.style.setProperty('stroke', 'var(--nord13)', 'important');
        s.style.setProperty('stroke-width', '3', 'important');
      });
    } else {
      shapesOf(el).forEach((s) => s.style.setProperty('fill', 'var(--bg-3)', 'important'));
      el.style.setProperty('opacity', '0.35');
    }
  });
  const svg = doc.querySelector('svg');
  svg.removeAttribute('width');
  svg.removeAttribute('height');
  if (!svg.getAttribute('viewBox')) svg.setAttribute('viewBox', '0 0 100 100');
  container.innerHTML = '';
  container.appendChild(document.importNode(svg, true));
  container.hidden = false;
  return true;
}

// Probe the board with a timeout so a hung connection (half-dead RNDIS,
// hanging proxy) fails instead of stalling the reboot watchers forever.
// Cache is bypassed so a stale cached response can neither arm early nor
// mask the return.
async function probeBoard(timeoutMs = 10000) {
  const controller = new AbortController();
  const timer = setTimeout(() => controller.abort(), timeoutMs);
  try {
    const resp = await fetch('/api/getFirmwareVersion', {
      cache: 'no-store',
      signal: controller.signal,
    });
    if (!resp.ok) throw new Error(`HTTP ${resp.status}`);
    await resp.json();
  } finally {
    clearTimeout(timer);
  }
}

// Poll for the board to come back in web config mode after a reboot.
// Resolves true on success, false on timeout (caller shows the failure).
async function waitForWebconfig(timeoutMs = 30000) {
  const deadline = Date.now() + timeoutMs;
  while (Date.now() < deadline) {
    if (!rebooting) return false;
    try {
      await probeBoard();
      return true;
    } catch (e) {
      // Board not back yet (or RNDIS down during reboot); keep waiting.
    }
    await new Promise((r) => setTimeout(r, 1000));
  }
  return false;
}

// Watch for the board to return after leaving web config (controller /
// bootloader reboot) and reload into the configurator when it does. Arms
// only after the board is observed gone: a fetch that never fails means the
// board never left (e.g. the mock server, where reboot is a no-op), so the
// overlay stays put instead of reloading. Once armed, a status line shows
// the watcher is waiting, so a stall is distinguishable from a disconnect.
// `startDown` skips the arming step when the disconnect is already confirmed
// (unexpected reset/unplug, failed initial load): the overlay message covers
// it, so the status line stays hidden.
async function watchForBoardReturn(startDown = false) {
  let down = startDown;
  for (;;) {
    try {
      await probeBoard();
      if (down) {
        location.reload();
        return;
      }
    } catch (e) {
      if (!down) {
        down = true;
        const status = document.getElementById('rebooted-status');
        status.textContent = 'Board disconnected. Waiting for it to return.';
        status.hidden = false;
      }
    }
    await new Promise((r) => setTimeout(r, 2000));
  }
}

async function reboot(bootMode) {
  const rebootBtn = document.getElementById('reboot');
  if (isDirty()) {
    const choice = await confirmDialog({
      title: 'Unsaved changes',
      message: 'You have unsaved changes. Save them before rebooting?',
      buttons: [
        { value: 'save', label: 'Save & Reboot', kind: 'primary' },
        { value: 'discard', label: 'Reboot without saving' },
        { value: 'cancel', label: 'Cancel' },
      ],
    });
    if (choice === 'save') {
      if (!await save()) return;
    } else if (choice === 'discard') {
      clearDraft();
    } else {
      return;
    }
  }
  rebootBtn.disabled = true;
  try {
    await api('/api/reboot', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ bootMode }),
    });
  } catch (e) {
    Toast.show('Reboot failed: ' + e, 'error');
    rebootBtn.disabled = false;
    return;
  }
  // The reboot was accepted: the board drops RNDIS ~500ms later. Stop the
  // pin-state loop, silence beforeunload (the disconnect is intentional),
  // and show a persistent state instead of a transient toast.
  rebooting = true;
  stopPinState();
  allowUnload = true;
  // Hint shown under the board graphic. Names no pins: the highlighted
  // button is the thing to hold (or touch, on touch boards). Falls back to
  // generic wording when the board has no web config pin or no board graphic.
  const backHint = webConfigReturnHint(true);
  if (bootMode === 1) {
    showRebootedOverlay({
      title: 'Rebooting',
      message: 'Waiting for the board to come back.',
      hint: '',
      spinning: true,
      showBoard: false,
    });
    const back = await waitForWebconfig();
    if (!back) {
      showRebootedOverlay({
        title: 'Reboot timed out',
        message: 'The board did not come back in web config mode.',
        hint: backHint,
        spinning: false,
        showBoard: true,
      });
      // Keep watching: a later return still reloads into the configurator.
      watchForBoardReturn();
      return;
    }
    location.reload();
  } else if (bootMode === 2) {
    showRebootedOverlay({
      title: 'Rebooted into bootloader mode',
      message: 'Drag a UF2 file onto the board drive to flash new firmware.',
      hint: backHint,
      spinning: false,
      showBoard: true,
    });
    watchForBoardReturn();
  } else {
    showRebootedOverlay({
      title: 'Rebooted into controller mode',
      message: 'The configurator is now disconnected.',
      hint: backHint,
      spinning: false,
      showBoard: true,
    });
    watchForBoardReturn();
  }
}

// Show the "Save changes" row in the reboot picker when there are unsaved
// edits, so Save & Reboot is one modal instead of a discard confirm.
function refreshRebootDirtyUi() {
  const hint = document.getElementById('reboot-dirty-hint');
  const saveBtn = document.getElementById('reboot-save');
  if (!hint || !saveBtn) return;
  let dirty = false;
  try {
    dirty = isDirty();
  } catch (e) {
    dirty = false;
  }
  hint.hidden = !dirty;
  saveBtn.hidden = !dirty;
}

function openRebootModal() {
  document.getElementById('reboot-modal').hidden = false;
  refreshRebootDirtyUi();
  // Cache the board graphic now, while the board is still up. The reboot
  // overlay renders from this cache after the board disconnects.
  prefetchRebootBoard();
}

function closeRebootModal() {
  document.getElementById('reboot-modal').hidden = true;
}

async function rebootTo(bootMode) {
  closeRebootModal();
  await reboot(bootMode);
}

// ---- generic confirm modal ------------------------------------------------
// Promise-based replacement for native confirm(): resolves to the chosen
// button's value, or null when dismissed (×, backdrop or Escape). Buttons are
// [{ value, label, kind }], kind one of '' (default), 'primary' or 'danger'.
// Only one prompt at a time; a concurrent call reuses the open prompt.
let confirmResolvers = null;

function confirmDialog({ title, message, buttons }) {
  const overlay = document.getElementById('confirm-modal');
  if (confirmResolvers) {
    // A prompt is already open; share its outcome rather than stacking.
    return new Promise((resolve) => { confirmResolvers.push(resolve); });
  }
  document.getElementById('confirm-title').textContent = title || '';
  document.getElementById('confirm-message').textContent = message || '';
  const box = document.getElementById('confirm-options');
  box.innerHTML = '';
  for (const b of buttons || []) {
    const btn = document.createElement('button');
    btn.type = 'button';
    btn.className = 'modal-option' + (b.kind ? ' ' + b.kind : '');
    btn.textContent = b.label;
    btn.addEventListener('click', () => closeConfirmModal(b.value));
    box.appendChild(btn);
  }
  overlay.hidden = false;
  const primary = box.querySelector('.modal-option.primary') || box.querySelector('.modal-option');
  if (primary) primary.focus();
  return new Promise((resolve) => { confirmResolvers = [resolve]; });
}

function closeConfirmModal(value = null) {
  document.getElementById('confirm-modal').hidden = true;
  if (confirmResolvers) {
    const resolvers = confirmResolvers;
    confirmResolvers = null;
    resolvers.forEach((resolve) => resolve(value));
  }
}

async function resetSettings() {
  const choice = await confirmDialog({
    title: 'Reset settings',
    message: 'Reset all settings to defaults and reboot?',
    buttons: [
      { value: 'reset', label: 'Reset', kind: 'danger' },
      { value: 'cancel', label: 'Cancel' },
    ],
  });
  if (choice !== 'reset') return;
  clearDraft();
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
      dpadMode: currentOptions.gamepad?.dpadMode ?? 0,
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
  // GP2040-th backup (.gp2040): auto-convert to an mp2040-config payload.
  // Keys the backup doesn't cover (macros, LED timeout, ...) are omitted so
  // the board's existing values are left untouched.
  let summaryText = null;
  if (data && data.type !== 'mp2040-config' && isGp2040Backup(data)) {
    try {
      const keyCount = (currentOptions?.keycodes || []).length || 30;
      const fallbackProfile = {
        keyboardKeycodes: [...(profiles[0]?.keycodes ?? currentOptions?.keycodes ?? [])],
        keyboardModifierMasks: [...(profiles[0]?.modifierMasks ?? currentOptions?.modifierMasks ?? [])],
      };
      const converted = convertGp2040Backup(data, keyCount, fallbackProfile);
      if (!data.pins && !Array.isArray(data.profiles?.alternativePinMappings)) {
        converted.payload.gamepadMasks = Array.from(
          { length: keyCount }, (_, i) => currentOptions?.gamepadMasks?.[i] ?? 0);
      }
      data = converted.payload;
      summaryText = gp2040SummaryText(converted.summary);
    } catch (e) {
      Toast.show('Import failed: ' + e.message, 'error');
      return;
    }
  }
  if (!data || data.type !== 'mp2040-config' || !Array.isArray(data.profiles) || !data.profiles.length) {
    Toast.show('Import failed: not an MP2040 settings file or GP2040-th backup', 'error');
    return;
  }
  // Named to avoid shadowing the global `profiles` (state.js), which the
  // GP2040-th fallback above reads.
  const importProfiles = data.profiles.slice(0, 4);
  const led = data.led || {};
  const globals = {
    // Converted GP2040-th payloads omit macros (no equivalent), leaving the
    // board's macros untouched; MP2040 exports always carry both keys.
    ...(data.macros !== undefined || data.macroIndices !== undefined
      ? { macroIndices: data.macroIndices || [], macros: data.macros || [] }
      : {}),
    defaultInputMode: data.defaultInputMode ?? 1,
    serialConfigEnabled: data.serialConfigEnabled === true,
    activeProfile: Number.isInteger(data.activeProfile) ? data.activeProfile : 0,
    gamepad: {
      socdMode: Number.isInteger(data.gamepad?.socdMode) ? data.gamepad.socdMode : 0,
      dpadMode: Number.isInteger(data.gamepad?.dpadMode) ? data.gamepad.dpadMode : 0,
      useNintendoLayout: data.gamepad?.useNintendoLayout === true,
    },
    ring: {
      ringStickTarget: Number.isInteger(data.ring?.ringStickTarget) ? data.ring.ringStickTarget : 1,
      ringKeyboardMode: Number.isInteger(data.ring?.ringKeyboardMode) ? data.ring.ringKeyboardMode : 2,
      ringScrollAxis: Number.isInteger(data.ring?.ringScrollAxis) ? data.ring.ringScrollAxis : 0,
      ringMidiBehavior: Number.isInteger(data.ring?.ringMidiBehavior) ? data.ring.ringMidiBehavior : 1,
    },
    gamepadMasks: data.gamepadMasks || [],
    // Converted GP2040-th payloads carry the debounce delay; MP2040 exports
    // don't (web UI edits it separately). Both firmware and mock honor it.
    ...(Number.isInteger(data.debounceInterval) ? { debounceInterval: data.debounceInterval } : {}),
  };
  try {
    for (let i = 0; i < importProfiles.length; i++) {
      const p = importProfiles[i];
      const body = {
        profileIndex: i,
        keycodes: p.keycodes || [],
        modifierMasks: p.modifierMasks || [],
        midiNotes: p.midiNotes || [],
        midiVelocities: p.midiVelocities || [],
        midi: p.midi || {},
        led: {
          ...(p.led || {}),
          ...(Array.isArray(led.ledSpeeds) && led.ledSpeeds.length ? { ledSpeeds: led.ledSpeeds } : {}),
          ...(Array.isArray(led.colorNormalByMode) && led.colorNormalByMode.length
            ? { colorNormalByMode: led.colorNormalByMode } : {}),
          ...(Array.isArray(led.colorPressedByMode) && led.colorPressedByMode.length
            ? { colorPressedByMode: led.colorPressedByMode } : {}),
          // Converted GP2040-th profiles without a usable custom theme carry
          // no per-key colors: re-send the board's live ones so the import
          // doesn't wipe them. Native exports always carry both arrays, and
          // themed conversions carry their own, so neither path changes.
          ...(!Array.isArray(p.led?.ledNormalColors) && Array.isArray(profiles[i]?.led?.ledNormalColors)
            ? { ledNormalColors: profiles[i].led.ledNormalColors } : {}),
          ...(!Array.isArray(p.led?.ledPressedColors) && Array.isArray(profiles[i]?.led?.ledPressedColors)
            ? { ledPressedColors: profiles[i].led.ledPressedColors } : {}),
          // Absent keys leave the board's values untouched (converted
          // GP2040-th payloads omit what has no equivalent). MP2040 exports
          // always carry both, so their behavior is unchanged.
          ...(led.ledTimeout !== undefined ? { ledTimeout: led.ledTimeout } : {}),
          ...(led.statusLedEnabled !== undefined ? { statusLedEnabled: led.statusLedEnabled } : {}),
        },
        // Globals only on the last call.
        ...(i === importProfiles.length - 1 ? globals : {}),
      };
      await api('/api/setOptions', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(body),
      });
    }
    Toast.show(summaryText ?? 'Settings imported.', 'success');
    // The board now holds the import: a pre-import draft must not haunt it.
    clearDraft();
    allowUnload = true;
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
// Discard all unsaved changes: confirm, drop the draft backstop, and reload
// into board truth (same tradeoff as import: always correct, ~1s flash).
// Works while Save is conflict-blocked, since reverting resolves conflicts.
async function discardChanges() {
  if (!isDirty()) {
    Toast.show('No unsaved changes.', 'info');
    return;
  }
  const choice = await confirmDialog({
    title: 'Discard changes',
    message: 'Discard all unsaved changes?',
    buttons: [
      { value: 'discard', label: 'Discard', kind: 'danger' },
      { value: 'cancel', label: 'Cancel' },
    ],
  });
  if (choice !== 'discard') return;
  clearDraft();
  allowUnload = true;
  location.reload();
}
document.getElementById('discard').addEventListener('click', discardChanges);
document.getElementById('discard-settings').addEventListener('click', discardChanges);
// Browser-local restore setting: lives outside the board payload and dirty
// tracking (its own element ID is never read by either).
document.getElementById('restore-mode').value = getRestoreMode();
document.getElementById('restore-mode').addEventListener('change', (e) => {
  setRestoreMode(e.target.value);
  if (e.target.value === 'off') clearDraft();
});
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
// The header select is an action menu: picking a source profile copies it into
// the current one, then the select snaps back to its placeholder.
document.getElementById('copy-profile-src').addEventListener('change', (e) => {
  const src = e.target.value;
  if (src === '') return;
  copyProfileFrom(Number(src));
  e.target.value = '';
});
document.getElementById('key-modal-save').addEventListener('click', saveKeyModal);
document.getElementById('key-modal-cancel').addEventListener('click', closeKeyModal);
document.querySelectorAll('#key-modal-tabs .modal-tab').forEach((btn) => {
  btn.addEventListener('click', () => setModalTab(Number(btn.dataset.mode)));
});
document.getElementById('ring-modal-save').addEventListener('click', saveRingModal);
document.getElementById('ring-modal-close').addEventListener('click', closeRingModal);

// Toggle the ring modal's scroll-axis field based on the keyboard-mode select.
document.getElementById('ring-modal-keyboard').addEventListener('change', (e) => {
  document.getElementById('ring-modal-scroll-wrap').hidden = Number(e.target.value) !== 1;
});

document.getElementById('reboot-modal-close').addEventListener('click', closeRebootModal);
document.getElementById('reboot-save').addEventListener('click', async (e) => {
  const btn = e.currentTarget;
  btn.disabled = true;
  await save();
  btn.disabled = false;
});
document.getElementById('reboot-normal').addEventListener('click', () => rebootTo(0));
document.getElementById('reboot-bootloader').addEventListener('click', () => rebootTo(2));
document.getElementById('reboot-webconfig').addEventListener('click', () => rebootTo(1));
// Close the modal when clicking the overlay backdrop or pressing Escape.
// (The post-reboot overlay has no dismiss affordance: the board is already
// gone, so there is nothing to go back to.)
document.getElementById('key-modal').addEventListener('click', (e) => {
  if (e.target === e.currentTarget) closeKeyModal();
});
document.getElementById('ring-modal').addEventListener('click', (e) => {
  if (e.target === e.currentTarget) closeRingModal();
});
document.getElementById('reboot-modal').addEventListener('click', (e) => {
  if (e.target === e.currentTarget) closeRebootModal();
});
document.getElementById('confirm-close').addEventListener('click', () => closeConfirmModal());
document.getElementById('confirm-modal').addEventListener('click', (e) => {
  if (e.target === e.currentTarget) closeConfirmModal();
});
document.addEventListener('keydown', (e) => {
  if (e.key === 'F5' || ((e.ctrlKey || e.metaKey) && e.key.toLowerCase() === 'r')) {
    if (!allowUnload && !rebooting && !disconnected && isDirty()) {
      e.preventDefault();
      handleRefreshRequest();
    }
    return;
  }
  if ((e.ctrlKey || e.metaKey) && e.key.toLowerCase() === 's') {
    e.preventDefault();
    if (isDirty() && !saving) save();
    return;
  }
  if (e.key === 'Escape' && !document.getElementById('confirm-modal').hidden) closeConfirmModal();
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
