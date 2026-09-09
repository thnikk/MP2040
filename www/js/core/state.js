// state.js — shared mutable UI state for the configurator.
// Every script below reads/writes these; loaded first (after the widget
// libraries) so all bindings exist before any function runs. Plain
// script: top-level lets are shared globals.

// Working copy of the config from /api/getOptions, edited via the modal
let currentOptions = null;

// Dirty-state tracking: a config has unsaved changes when the working copy,
// an in-memory profile slot or a global setting differs from what the board
// last loaded/saved. `savedProfiles` and `savedGlobals` are snapshots taken
// on load and refreshed on save; see isDirty() below.
let savedProfiles = null;
let savedGlobals = null;
let saving = false;
let allowUnload = false;

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

// Asset cache-buster: the firmware serves static files with immutable
// caching, so runtime fetches (board/controller graphics, gamepad glyphs)
// carry the firmware version in the query string. Set once in load() from
// /api/getFirmwareVersion; empty until then (dev server, early fetches).
let assetVersion = '';
function assetUrl(path) {
  return assetVersion ? `${path}?v=${assetVersion}` : path;
}

// MultiSelect used in the key modal
let modalSelect = null;

// Visual keyboard picker (keyboardwidget.js) used in the key modal
let keyboardWidget = null;

// MIDI note picker (midikeyboard.js) used in the key modal in MIDI mode
let midiKeyboard = null;

// Gamepad control multi-select (MultiSelect) used in the key modal in
// gamepad modes
let gamepadSelect = null;

// Visual gamepad picker (controllerwidget.js) used in the key modal in
// gamepad modes
let gamepadWidget = null;

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
let statusLedMinSlider = null;
let statusLedMaxSlider = null;

// Pill toggles (see pilltoggle.js): Serial control, Status LED, Input History
// and the Nintendo layout toggle.
let serialPill = null;
let statusLedPill = null;
let displayHistoryPill = null;
let nintendoPill = null;

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

// Global MIDI Channel / Velocity spinners (visible only in MIDI mode)
let midiChannelSpinner = null;
let midiVelocitySpinner = null;
let debounceSpinner = null;
let touchMarginSpinner = null;
let touchReleaseSpinner = null;
let displaySplashDurationSpinner = null;
let displaySaverTimeoutSpinner = null;
let displayHistoryTimeoutSpinner = null;
