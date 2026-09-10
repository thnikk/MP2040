// gp2040.js — one-way GP2040-th (.gp2040 backup) → MP2040 config converter.
//
// A .gp2040 backup is the JSON blob GP2040-th's BackupPage downloads: one key
// per API section ({display, splash, gamepad, led, ledTheme, macros, pins,
// profiles, addons}), each holding that section's GET response. Only the
// sections with an MP2040 equivalent are converted (pins/profiles → key maps,
// gamepad → input/global options, led/ledTheme → LED colors); everything else
// (macros, hotkeys, display, splash, add-ons) is reported as skipped.
//
// Pure functions, no DOM access: the summary is returned for app.js to toast.
// Plain script: top-level bindings are shared globals (loaded before app.js).

// GP2040-th GpioAction values (proto/enums.proto). Only the gamepad-button
// actions map to an MP2040 gamepad mask; the rest (menu nav, sustain, turbo,
// macros, analog, E-buttons...) have no MP2040 equivalent and map to 0.
const GP2040_CUSTOM_COMBO = 40;

// GP2040-th action → MP2040 GAMEPAD_PIN_MASK_* bit. MP2040 packs dpad (bits
// 0-3) and buttons B1-A2 (bits 4-17) into one mask; GP2040 splits them across
// the action enum plus customButtonMask/customDpadMask (dpad bits 0-3,
// button bits B1=0..A2=13).
const GP2040_ACTION_BITS = {
  1: 1 << 0, // up
  2: 1 << 1, // down
  3: 1 << 2, // left
  4: 1 << 3, // right
  5: 1 << 4, // B1
  6: 1 << 5, // B2
  7: 1 << 6, // B3
  8: 1 << 7, // B4
  9: 1 << 8, // L1
  10: 1 << 9, // R1
  11: 1 << 10, // L2
  12: 1 << 11, // R2
  13: 1 << 12, // S1
  14: 1 << 13, // S2
  15: 1 << 16, // A1
  16: 1 << 17, // A2
  17: 1 << 14, // L3
  18: 1 << 15, // R3
};

// GP2040-th customButtonMask bit (B1=0..A2=13) → MP2040 mask bit.
const GP2040_BUTTON_BITS = [
  1 << 4, 1 << 5, 1 << 6, 1 << 7, 1 << 8, 1 << 9, 1 << 10, 1 << 11,
  1 << 12, 1 << 13, 1 << 14, 1 << 15, 1 << 16, 1 << 17,
];

// GP2040-th InputMode (proto/enums.proto) → MP2040 InputMode. Modes MP2040
// doesn't implement (Switch, legacy consoles, ...) fall back to keyboard.
const GP2040_INPUT_MODES = {
  3: 1, // keyboard
  0: 3, // XInput
  15: 4, // Switch Pro
  5: 5, // XBONE → Xbox One
  2: 6, // PS3
  4: 7, // PS4
  13: 8, // PS5
};

// GP2040-th baseAnimationIndex → MP2040 ledMode (approximate: different
// effect sets on each side).
const GP2040_ANIMATIONS = {
  0: 0, // static → custom
  3: 0, // static theme → custom
  5: 0, // custom theme → custom
  1: 5, // rainbow → rain
  2: 1, // chase → cycle
  4: 4, // ripple → ripple
};

// GP2040-th GpioAction → ledTheme custom-color key ({u, d} pair).
const GP2040_ACTION_THEMES = {
  1: 'Up', 2: 'Down', 3: 'Left', 4: 'Right',
  5: 'B1', 6: 'B2', 7: 'B3', 8: 'B4',
  9: 'L1', 10: 'R1', 11: 'L2', 12: 'R2',
  13: 'S1', 14: 'S2', 15: 'A1', 16: 'A2',
  17: 'L3', 18: 'R3',
};

function gpNum(v, fallback = 0) {
  return Number.isInteger(v) ? v : fallback;
}

function gpClamp(v, lo, hi, fallback = 0) {
  if (!Number.isFinite(Number(v))) return fallback;
  return Math.max(lo, Math.min(hi, Number(v)));
}

function gpPinName(i) {
  return 'pin' + String(i).padStart(2, '0');
}

// Gamepad mask for one GP2040-th pin mapping. Returns {mask, mapped} where
// mapped is false when the action has no MP2040 equivalent.
function gpGamepadMask(pin) {
  const action = gpNum(pin?.action, -10);
  // No gamepad function assigned (or reserved): neutral, not a dropped mapping.
  if (action === -10 || action === -5 || action === 0) {
    return { mask: 0, mapped: true };
  }
  if (action in GP2040_ACTION_BITS) {
    return { mask: GP2040_ACTION_BITS[action], mapped: true };
  }
  if (action === GP2040_CUSTOM_COMBO) {
    let mask = gpNum(pin?.customDpadMask, 0) & 0xf;
    const buttons = gpNum(pin?.customButtonMask, 0);
    for (let b = 0; b < GP2040_BUTTON_BITS.length; b++) {
      if (buttons & (1 << b)) mask |= GP2040_BUTTON_BITS[b];
    }
    return { mask, mapped: true };
  }
  return { mask: 0, mapped: false };
}

// One GP2040-th pin-mapping section (pins or an alternativePinMappings entry)
// → MP2040 profile body. Arrays are sized to the target board's key count;
// GP2040-th pins are GPIO-indexed (0-29), so entries past pin 29 are zero.
function gpConvertProfile(section, ledTheme, keyCount) {
  const kc = Array.isArray(section?.keyboardKeycodes) ? section.keyboardKeycodes : [];
  const km = Array.isArray(section?.keyboardModifierMasks) ? section.keyboardModifierMasks : [];
  const keycodes = [];
  const modifierMasks = [];
  const gamepadMasks = [];
  const ledNormalColors = [];
  const ledPressedColors = [];
  let keys = 0;
  let gamepadPins = 0;
  let unmappedGamepadPins = 0;
  // Per-key custom colors only when the source used a custom theme; the
  // theme is keyed by gamepad button, resolved through the pin's action.
  const custom = ledTheme?.enabled === true ? ledTheme : null;
  const staticNormal = gpClamp(ledTheme?.staticColorNormal, 0, 0xffffff, 0);
  const staticPressed = gpClamp(ledTheme?.staticColorPressed, 0, 0xffffff, 0);
  for (let i = 0; i < keyCount; i++) {
    const code = i < 30 ? gpClamp(kc[i], 0, 255, 0) : 0;
    const mods = i < 30 ? gpClamp(km[i], 0, 255, 0) : 0;
    keycodes.push(code);
    modifierMasks.push(mods);
    if (code !== 0 || mods !== 0) keys++;
    let mask = 0;
    if (i < 30 && section) {
      const r = gpGamepadMask(section[gpPinName(i)]);
      mask = r.mask;
      if (mask !== 0) {
        gamepadPins++;
      } else if (!r.mapped) {
        unmappedGamepadPins++;
      }
    }
    gamepadMasks.push(mask);
    if (custom && i < 30 && section) {
      const themeKey = GP2040_ACTION_THEMES[gpNum(section[gpPinName(i)]?.action, -10)];
      const entry = themeKey ? custom[themeKey] : null;
      ledNormalColors.push(
        entry && Number.isFinite(Number(entry.u))
          ? gpClamp(entry.u, 0, 0xffffff, 0)
          : staticNormal
      );
      ledPressedColors.push(
        entry && Number.isFinite(Number(entry.d))
          ? gpClamp(entry.d, 0, 0xffffff, 0)
          : staticPressed
      );
    } else {
      ledNormalColors.push(0);
      ledPressedColors.push(0);
    }
  }
  return {
    profile: {
      keycodes,
      modifierMasks,
      midiNotes: [],
      midiVelocities: [],
      midi: {},
      led: {
        ledNormalColors,
        ledPressedColors,
      },
    },
    gamepadMasks,
    keys,
    gamepadPins,
    unmappedGamepadPins,
  };
}

// A .gp2040 backup holds one key per API section; an MP2040 export is
// {type: 'mp2040-config', profiles: [...]}.
function isGp2040Backup(data) {
  if (!data || typeof data !== 'object' || Array.isArray(data)) return false;
  if (data.type === 'mp2040-config') return false;
  return (
    (data.pins && typeof data.pins === 'object') ||
    (data.profiles && typeof data.profiles === 'object') ||
    (data.gamepad && typeof data.gamepad === 'object')
  );
}

// Convert a parsed .gp2040 backup into an mp2040-config-shaped payload ready
// for the per-profile setOptions loop. keyCount is the target board's key
// count; fallbackProfile supplies the key map when the backup holds no pin
// data (partial export), so globals-only backups never wipe the key map.
function convertGp2040Backup(data, keyCount, fallbackProfile) {
  const kc = Math.max(1, keyCount | 0);
  const ledTheme = data.ledTheme && typeof data.ledTheme === 'object' ? data.ledTheme : null;
  const sections = [];
  if (data.pins && typeof data.pins === 'object') {
    sections.push(data.pins);
  } else if (fallbackProfile) {
    sections.push(null);
  }
  const alts = data.profiles?.alternativePinMappings;
  if (Array.isArray(alts)) {
    for (const alt of alts.slice(0, 3)) {
      sections.push(alt && typeof alt === 'object' ? alt : null);
    }
  }
  if (sections.length === 0) {
    throw new Error('no pin mappings (pins/profiles) in backup');
  }
  const profiles = [];
  // The base gamepad mapping is global in MP2040 (shared by all profiles),
  // so only the base section's contributes.
  let gamepadMasks = new Array(kc).fill(0);
  let totalKeys = 0;
  let totalGamepadPins = 0;
  let unmappedGamepadPins = 0;
  let customColors = false;
  for (let i = 0; i < Math.min(sections.length, 4); i++) {
    const section = sections[i] || fallbackProfile;
    const r = gpConvertProfile(section, ledTheme, kc);
    if (i === 0) gamepadMasks = r.gamepadMasks;
    totalKeys += r.keys;
    totalGamepadPins += r.gamepadPins;
    unmappedGamepadPins += r.unmappedGamepadPins;
    if (ledTheme?.enabled === true) customColors = true;
    profiles.push(r.profile);
  }
  const gamepad = data.gamepad && typeof data.gamepad === 'object' ? data.gamepad : {};
  const gpInput = gpNum(gamepad.inputMode, 3);
  const inputFallback = !(gpInput in GP2040_INPUT_MODES);
  const defaultInputMode = inputFallback ? 1 : GP2040_INPUT_MODES[gpInput];
  const profileNumber = gpNum(gamepad.profileNumber, 1);
  const activeProfile = Math.max(0, Math.min(profiles.length - 1, profileNumber - 1));
  // LED globals: static theme colors seed every mode; the animation picks the
  // closest MP2040 effect. Wiring (dataPin/format/count/indices) is
  // board-fixed on MP2040 and intentionally not carried over.
  const led = {};
  const brightness = gpClamp(data.led?.brightnessMaximum, 0, 255, null);
  if (brightness !== null) led.brightnessByMode = Array(7).fill(brightness);
  if (ledTheme && (Number.isFinite(Number(ledTheme.staticColorNormal)) || Number.isFinite(Number(ledTheme.staticColorPressed)))) {
    led.colorNormalByMode = Array(7).fill(gpClamp(ledTheme.staticColorNormal, 0, 0xffffff, 0));
    led.colorPressedByMode = Array(7).fill(gpClamp(ledTheme.staticColorPressed, 0, 0xffffff, 0));
  }
  const anim = gpNum(ledTheme?.animationMode, -1);
  const ledMode = anim in GP2040_ANIMATIONS ? GP2040_ANIMATIONS[anim] : null;
  if (ledMode !== null) {
    for (const p of profiles) p.led.ledMode = ledMode;
  }
  const skipped = ['macros'];
  if (gamepad.hotkey01 !== undefined) skipped.push('hotkeys');
  if (data.display !== undefined || data.splash !== undefined) skipped.push('display/splash');
  if (data.addons !== undefined) skipped.push('add-ons');
  const summary = {
    profiles: profiles.length,
    keys: totalKeys,
    gamepadPins: totalGamepadPins,
    customColors,
    ledMode,
    inputMode: defaultInputMode,
    inputFallback,
    unmappedGamepadPins,
    skipped: skipped.filter((s) =>
      s === 'macros' ? data.macros !== undefined : true
    ),
  };
  const payload = {
    type: 'mp2040-config',
    version: 1,
    activeProfile,
    defaultInputMode,
    serialConfigEnabled: false,
    // Keyboard macros have no GP2040-th equivalent (gamepad button-mask
    // sequences): omit both keys so the board's macros are left untouched.
    gamepad: {
      socdMode: gpClamp(gamepad.socdMode, 0, 4, 0),
      dpadMode: gpClamp(gamepad.dpadMode, 0, 2, 0),
      useNintendoLayout: gamepad.useNintendoLayout === true || gamepad.useNintendoLayout === 1,
    },
    ring: {
      ringStickTarget: 1,
      ringKeyboardMode: 2,
      ringScrollAxis: 0,
      ringMidiBehavior: 1,
    },
    gamepadMasks,
    led,
    profiles,
  };
  const debounce = gpClamp(gamepad.debounceDelay, 0, 100, null);
  if (debounce !== null) payload.debounceInterval = debounce;
  return { payload, summary };
}

// One-line human summary for the import toast.
function gp2040SummaryText(summary) {
  const bits = [
    `GP2040-th import: ${summary.keys} keys across ${summary.profiles} profile${summary.profiles === 1 ? '' : 's'}`,
  ];
  if (summary.gamepadPins > 0) {
    bits.push(`${summary.gamepadPins} gamepad mapping${summary.gamepadPins === 1 ? '' : 's'}`);
  }
  if (summary.customColors) bits.push('custom LED colors');
  if (summary.inputFallback) bits.push('input mode not supported, using keyboard');
  if (summary.unmappedGamepadPins > 0) {
    bits.push(`${summary.unmappedGamepadPins} gamepad action${summary.unmappedGamepadPins === 1 ? '' : 's'} dropped`);
  }
  if (summary.skipped.length > 0) bits.push(`skipped: ${summary.skipped.join(', ')}`);
  return bits.join('; ') + '.';
}
