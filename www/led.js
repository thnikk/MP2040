// led.js — LED mode helpers, live preview and the per-LED color popover
// (custom mode). Plain script: top-level bindings are shared globals.

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
  led.statusLedEnabled = statusLedPill ? statusLedPill.checked : true;
  led.statusLedBrightnessMinimum = statusLedMinSlider ? statusLedMinSlider.getValue() : 0;
  led.statusLedBrightnessMaximum = statusLedMaxSlider ? statusLedMaxSlider.getValue() : 255;
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
    statusLedBrightnessMinimum: led.statusLedBrightnessMinimum,
    statusLedBrightnessMaximum: led.statusLedBrightnessMaximum,
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
