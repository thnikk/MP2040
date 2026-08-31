// gamepadlabels.js — shared gamepad control/label/glyph module.
// The gamepad counterpart to kblayout.js: control definitions, per-layout
// label + glyph sets, the mode→set lookup (with the Nintendo-layout maskMap),
// and the glyph icon cache/loader. Used by the controller widget
// (controllerwidget.js), the board view labels (boardview.js) and the gamepad
// picker (app.js). Loaded as a plain script before those, so the top-level
// bindings are shared globals.

// Control id → label key and mask bit. The ids are the ones baked into
// controller.svg; labelKey is the default (GP2040-style) control name, and
// labels are swapped per input mode via CTRL_LABEL_SETS below. The mask layout
// matches GAMEPAD_PIN_MASK_* in gamepadhelper.h: dpad in bits 0-3, buttons
// B1-A2 in bits 4-17.
const CTRL_ELS = [
  { id: 'btn-l2', labelKey: 'L2', mask: 0x0400 },
  { id: 'btn-r2', labelKey: 'R2', mask: 0x0800 },
  { id: 'btn-l1', labelKey: 'L1', mask: 0x0100 },
  { id: 'btn-r1', labelKey: 'R1', mask: 0x0200 },
  { id: 'btn-a1', labelKey: 'A1', mask: 0x10000 },
  { id: 'btn-a2', labelKey: 'A2', mask: 0x20000 },
  { id: 'btn-s1', labelKey: 'S1', mask: 0x1000 },
  { id: 'btn-s2', labelKey: 'S2', mask: 0x2000 },
  { id: 'btn-b4', labelKey: 'B4', mask: 0x0080 },
  { id: 'btn-b3', labelKey: 'B3', mask: 0x0040 },
  { id: 'btn-b2', labelKey: 'B2', mask: 0x0020 },
  { id: 'btn-b1', labelKey: 'B1', mask: 0x0010 },
  { id: 'btn-up', labelKey: 'Up', mask: 0x0001 },
  { id: 'btn-down', labelKey: 'Down', mask: 0x0002 },
  { id: 'btn-left', labelKey: 'Left', mask: 0x0004 },
  { id: 'btn-right', labelKey: 'Right', mask: 0x0008 },
  { id: 'btn-l3', labelKey: 'L3', mask: 0x4000 },
  { id: 'btn-r3', labelKey: 'R3', mask: 0x8000 },
];

// Per-layout label sets. Keys are the GP2040 control names; anything not in a
// set falls back to the key itself (the dpad directions, which are the same
// everywhere). 'switch' labels a Nintendo-laid-out pad; when such a pad is
// wired Xbox-layout the face buttons are swapped (done by the caller).
const CTRL_LABEL_SETS = {
  gp2040: {},
  xbox: {
    B1: 'A', B2: 'B', B3: 'X', B4: 'Y',
    L1: 'LB', R1: 'RB', L2: 'LT', R2: 'RT',
    S1: 'Back', S2: 'Start', L3: 'LS', R3: 'RS',
    A1: 'Guide', A2: '-',
  },
  switch: {
    B1: 'B', B2: 'A', B3: 'Y', B4: 'X',
    L1: 'L', R1: 'R', L2: 'ZL', R2: 'ZR',
    S1: 'Minus', S2: 'Plus', L3: 'LS', R3: 'RS',
    A1: 'Home', A2: 'Capture',
  },
  xbone: {
    B1: 'A', B2: 'B', B3: 'X', B4: 'Y',
    L1: 'LB', R1: 'RB', L2: 'LT', R2: 'RT',
    S1: 'View', S2: 'Menu', L3: 'LS', R3: 'RS',
    A1: 'Guide', A2: 'Share',
  },
};

// Per-layout glyph sets. Keys are the same label keys as the label sets; the
// value is the id of an icon file in www/icons/gamepad/<id>.svg. Controls
// without a glyph (or whose icon hasn't loaded) fall back to the text label.
const CTRL_GLYPH_SETS = {
  gp2040: {},
  xbox: {
    S1: 'xbox-back', S2: 'xbox-start', A1: 'xbox-guide', A2: 'xbox-share',
    Up: 'dpad-up', Down: 'dpad-down', Left: 'dpad-left', Right: 'dpad-right',
  },
  switch: {
    S1: 'switch-minus', S2: 'switch-plus', A1: 'switch-home', A2: 'switch-capture',
    Up: 'dpad-up', Down: 'dpad-down', Left: 'dpad-left', Right: 'dpad-right',
  },
  xbone: {
    S1: 'xbox-back', S2: 'xbox-start', A1: 'xbox-guide', A2: 'xbox-share',
    Up: 'dpad-up', Down: 'dpad-down', Left: 'dpad-left', Right: 'dpad-right',
  },
};

// Label + glyph + bit-mask sets for a given input mode: XInput (3) shows Xbox
// names; Switch Pro (4) shows Nintendo names, always laid out like a real
// Switch Pro controller (A right, B bottom, Y left, X top). The Nintendo-layout
// toggle only swaps which stored position-bit each letter maps to (maskMap),
// so clicking a letter always maps the pin to that Switch button. Shared by the
// widget, the gamepad multi-select (app.js) and the board view.
function labelSet(mode, nintendoLayout) {
  if (mode === 3) {
    return { labels: CTRL_LABEL_SETS.xbox, glyphs: CTRL_GLYPH_SETS.xbox };
  }
  if (mode === 4) {
    // The letters sit at real Switch Pro positions (B1 bottom, B2 right, B3
    // left, B4 top). The toggle swaps which stored bit produces each letter:
    // ON → bottom=B, right=A, left=Y, top=X; OFF → bottom=A, right=B, left=X,
    // top=Y. maskMap gives each position the bit that sends its displayed
    // letter under the current toggle.
    const maskMap = nintendoLayout
      ? { B1: 0x0010, B2: 0x0020, B3: 0x0040, B4: 0x0080 }
      : { B1: 0x0020, B2: 0x0010, B3: 0x0080, B4: 0x0040 };
    return {
      labels: CTRL_LABEL_SETS.switch,
      glyphs: CTRL_GLYPH_SETS.switch,
      maskMap,
    };
  }
  if (mode === 5) {
    // Xbox One shares the Xbox face-button layout; the capture button (A2)
    // is the Share button. S1/S2 are View/Menu.
    return { labels: CTRL_LABEL_SETS.xbone, glyphs: CTRL_GLYPH_SETS.xbone };
  }
  return { labels: CTRL_LABEL_SETS.gp2040, glyphs: CTRL_GLYPH_SETS.gp2040 };
}

// The controls (in display order) whose stored bit is set in `mask`, each with
// the active layout's label text and glyph id ('' = text-only control). Inverts
// the widget's bitFor(): a stored bit is shown as whatever control position
// drives it under the current maskMap (Switch layout toggle). Shared with the
// board view so it labels pins exactly like the widget.
function controlsForMask(mask, set) {
  const labels = set.labels || {};
  const glyphs = set.glyphs || {};
  const maskMap = set.maskMap || null;
  const out = [];
  for (const def of CTRL_ELS) {
    const bit = (maskMap && maskMap[def.labelKey]) || def.mask;
    if (!(mask & bit)) continue;
    out.push({
      text: labels[def.labelKey] ?? def.labelKey,
      glyphId: glyphs[def.labelKey] || '',
    });
  }
  return out;
}

// ---- glyph icon cache / loader -------------------------------------------

// Loaded glyphs, keyed by id: { viewBox: [x, y, w, h], nodes: [{ tag, attrs }] }.
// A null entry means the icon is missing or failed to load.
const glyphCache = new Map();

// Sentinel stored in glyphCache while a glyph fetch is in flight. Lets a second
// consumer (e.g. the board view) tell "still loading" from "failed" (null) so
// it can wait on the shared load instead of giving up or re-fetching forever.
const GLYPH_LOADING = {};

// A fully-loaded glyph from the shared cache, or null while it's still loading
// or was missing (GLYPH_LOADING sentinel / null failed marker).
function loadedGlyph(id) {
  const g = glyphCache.get(id);
  return g && g !== GLYPH_LOADING ? g : null;
}

// Fetch and parse a glyph icon from /icons/gamepad/<id>.svg. Only the drawable
// shapes are kept, and their fill/stroke are dropped so the glyph renders in
// the theme's currentColor (same convention as the other /icons files).
async function loadGlyph(id) {
  try {
    const res = await fetch(`/icons/gamepad/${id}.svg`);
    const doc = new DOMParser().parseFromString(await res.text(), 'image/svg+xml');
    const root = doc.documentElement;
    if (!root || root.tagName.toLowerCase() !== 'svg') return null;
    const viewBox = (root.getAttribute('viewBox') || '0 0 24 24').split(/\s+/).map(Number);
    const nodes = [...root.querySelectorAll('path, circle, rect, ellipse, line, polygon, polyline')]
      .map((n) => ({
        tag: n.tagName,
        attrs: [...n.attributes]
          .filter((a) => a.name !== 'fill' && a.name !== 'stroke' && a.name !== 'style')
          .map((a) => [a.name, a.value]),
      }));
    if (!nodes.length) return null;
    return { viewBox, nodes };
  } catch (e) {
    return null;
  }
}

// Ensure a glyph is loaded (cached); calls `onReady` when it arrives. If a load
// is already in flight (GLYPH_LOADING sentinel, e.g. started by the controller
// widget's preload), wait on it and call onReady anyway so every consumer shows
// the icon. Already-loaded or failed glyphs return without calling onReady.
function ensureGlyph(id, onReady) {
  const cached = glyphCache.get(id);
  if (glyphCache.has(id) && cached !== GLYPH_LOADING) return; // loaded or failed
  if (!glyphCache.has(id)) glyphCache.set(id, GLYPH_LOADING); // mark in-flight
  loadGlyph(id).then((glyph) => {
    if (glyphCache.get(id) === GLYPH_LOADING) glyphCache.set(id, glyph);
    if (onReady) onReady();
  });
}