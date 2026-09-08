// modals.js — key modal, touch-ring modal and input-mode UI switching.
// Plain script: top-level bindings are shared globals.

// Swap the Layout/Board icons between keyboard and gamepad, and the landing
// hero text, based on the current input mode. Gamepad modes (XInput / Switch
// Pro / Xbox One) get the controller icon and wording; keyboard and MIDI keep
// the keyboard icon and keypad wording.
function syncModeIcons(gamepadMode) {
  document.querySelectorAll('#layout-nav-icon, #layout-card-icon, #board-heading-icon')
    .forEach((el) => {
      el.classList.toggle('icon-keyboard', !gamepadMode);
      el.classList.toggle('icon-gamepad', gamepadMode);
    });
  const hero = document.getElementById('hero-hint');
  if (hero) hero.textContent = gamepadMode
    ? 'Configure your controller over USB'
    : 'Configure your keypad over USB';
}

// PS4/PS5 auth hint: shown when the board has no USB host port and a Sony
// auth mode is selected. Unauthenticated play works on PC with no timeout;
// consoles drop the controller after ~8 minutes until replugged.
function updatePsAuthHint() {
  const hint = document.getElementById('ps-auth-hint');
  if (!hint) return;
  const mode = Number(
    (currentOptions && currentOptions.defaultInputMode)
    ?? document.getElementById('default-input-mode').value ?? 1,
  );
  const show = currentOptions.hasUsbHostPort !== true && (mode === 7 || mode === 8);
  hint.hidden = !show;
  if (show) {
    hint.textContent = 'This board has no USB host port, so PS4/PS5 run unauthenticated: '
      + 'no timeout on PC, ~8-minute timeout on console.';
  }
}

// Show either the key/modifier pickers (keyboard mode), the MIDI note picker
// (MIDI mode) or the gamepad control multi-select (gamepad modes) in the modal,
// based on the current default input mode. Also reveals the MIDI / gamepad
// settings and swaps the Board card description to match the mode.
function updateModalMode() {
  const mode = Number(currentOptions.defaultInputMode || 1);
  const midiMode = mode === 2;
  const gamepadMode = mode === 3 || mode === 4 || mode === 5 || mode === 6 || mode === 7 || mode === 8;
  document.getElementById('midi-settings').hidden = !midiMode;
  document.getElementById('gamepad-settings').hidden = !gamepadMode;
  // The Nintendo layout toggle only applies to Switch Pro.
  document.getElementById('nintendo-layout-wrap').hidden = mode !== 4;
  document.getElementById('board-hint').textContent = midiMode
    ? 'Click a button on the board to set its MIDI note and velocity.'
    : gamepadMode
      ? 'Click a button on the board to set its gamepad button or direction.'
      : 'Click a button on the board to set its key and modifiers.';
  syncModeIcons(gamepadMode);
}

// Show the picker group for a modal tab (Keyboard / MIDI / Gamepad). Tabs are
// modal-local: they pick which per-pin mapping is edited without changing the
// board's active input mode. Defaults to the active input mode on open.
function setModalTab(mode) {
  const m = Number(mode || 1);
  const gamepadMode = m === 3 || m === 4 || m === 5 || m === 6 || m === 7 || m === 8;
  const midiMode = m === 2;
  const keyboardMode = !midiMode && !gamepadMode;
  document.getElementById('key-modal-group-keyboard').hidden = !keyboardMode;
  document.getElementById('key-modal-group-midi').hidden = !midiMode;
  document.getElementById('key-modal-group-gamepad').hidden = !gamepadMode;
  // Tabs map the three picker groups; the active tab gets the highlight.
  const tab = gamepadMode ? 3 : (midiMode ? 2 : 1);
  document.querySelectorAll('#key-modal-tabs .modal-tab').forEach((btn) => {
    btn.classList.toggle('active', Number(btn.dataset.mode) === tab);
  });
  // Widgets skip rendering while hidden (labels need layout), so re-fit the
  // one revealed by this tab on the next frame.
  if (midiMode) midiKeyboard.refresh();
  if (gamepadMode && gamepadWidget) {
    requestAnimationFrame(() => gamepadWidget.setMask(gamepadWidget.getMask()));
  }
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

function openKeyModal(pin) {
  editingPin = pin;
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
  if (gamepadWidget) gamepadWidget.setMask(gamepadMask);
  closeLedColorPopover();
  setModalTab(currentOptions.defaultInputMode ?? 1);
  document.getElementById('key-modal').hidden = false;
  // The widgets may have been built while the modal was hidden, so re-fit them
  // now that it has a real width: the MIDI octave window, and the gamepad
  // controller widget whose labels are placed from getBBox() (all zeros while
  // the modal is hidden).
  requestAnimationFrame(() => {
    midiKeyboard.refresh();
    if (gamepadWidget) gamepadWidget.setMask(gamepadWidget.getMask());
  });
}

function closeKeyModal() {
  document.getElementById('key-modal').hidden = true;
  editingPin = -1;
}

function saveKeyModal() {
  if (editingPin < 0) return;

  // Keyboard, MIDI and gamepad mappings are independent per-pin arrays that
  // coexist (a pin can hold a key, a MIDI note and a gamepad mask at once).
  // The modal's tabs let the user edit any of them; persist each from its
  // picker so edits stick regardless of which tab is active on Save.
  const perKey = (key) => {
    if (!currentOptions[key]) currentOptions[key] = new Array(128).fill(0);
    return currentOptions[key];
  };

  currentOptions.keycodes = perKey('keycodes');
  currentOptions.modifierMasks = perKey('modifierMasks');
  currentOptions.midiNotes = perKey('midiNotes');
  currentOptions.midiVelocities = perKey('midiVelocities');
  currentOptions.gamepadMasks = perKey('gamepadMasks');

  const { keycode, mask, macroIndex } = keyboardWidget.getValue();
  // A pin is either a plain key or a macro trigger, never both.
  currentOptions.keycodes[editingPin] = macroIndex ? 0 : keycode;
  currentOptions.modifierMasks[editingPin] = macroIndex ? 0 : mask;
  if (!currentOptions.macroIndices) currentOptions.macroIndices = new Array(128).fill(0);
  currentOptions.macroIndices[editingPin] = macroIndex;
  currentOptions.midiNotes[editingPin] = midiKeyboard.getValue();
  currentOptions.midiVelocities[editingPin] = midiKeyboard.getVelocity();
  // A pin maps to zero or more gamepad controls, packed into one mask.
  currentOptions.gamepadMasks[editingPin] = gamepadWidget
    ? gamepadWidget.getMask()
    : gamepadSelect.getGroupMask('gamepad');

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
  const gamepadMode = mode === 3 || mode === 4 || mode === 5 || mode === 6 || mode === 7 || mode === 8;
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
  const gamepadMode = mode === 3 || mode === 4 || mode === 5 || mode === 6 || mode === 7 || mode === 8;
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
