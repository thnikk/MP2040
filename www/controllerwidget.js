// ControllerWidget — vanilla port of GP2040-th's gamepad picker
// (www/src/components/widgets/ControllerWidget.tsx). Renders an SVG gamepad;
// clicking a button or dpad direction toggles that control's bit in the pin's
// gamepad mask. The mask layout matches GAMEPAD_PIN_MASK_* in gamepadhelper.h:
// dpad in bits 0-3, buttons B1-A2 in bits 4-17.

// Control id → label key and mask bit. The ids are the ones baked into
// controller.svg; labelKey is the default (GP2040-style) control name, and
// labels are swapped per input mode via CTRL_LABEL_SETS below.
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
};

// Stick wells are just visual (the L3/R3 sticks sit on top); they're not
// clickable and get a recessed fill.
const STICK_WELLS = ['circle5', 'circle6', 'circle13'];

// Loaded glyphs, keyed by id: { viewBox: [x, y, w, h], nodes: [{ tag, attrs }] }.
// A null entry means the icon is missing or still loading.
const glyphCache = new Map();

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

// Ensure a glyph is loaded (cached); re-renders `widget` when it arrives.
async function ensureGlyph(widget, id) {
  if (glyphCache.has(id)) return;
  glyphCache.set(id, null); // mark in-flight / missing
  const glyph = await loadGlyph(id);
  glyphCache.set(id, glyph);
  if (widget) widget.render();
}

const CTRL_VIEWBOX_RE = /viewBox="([^"]+)"/;
const SELECTED_STROKE = '#00ff00';

class ControllerWidget {
  constructor({ container, mask, labels, glyphs, onChange }) {
    this.mask = mask || 0;
    this.labels = labels || CTRL_LABEL_SETS.gp2040;
    this.glyphs = glyphs || CTRL_GLYPH_SETS.gp2040;
    this.onChange = onChange || (() => {});
    this.markup = '';
    this.viewBox = '0 0 434.5 366';
    this.loaded = false;
    this.buildDom(container);
    this.preloadGlyphs();
    this.loadSvg();
  }

  setMask(mask) {
    this.mask = mask || 0;
    if (this.loaded) this.render();
  }

  getMask() {
    return this.mask;
  }

  // Swap the per-layout label set (e.g. Xbox vs Nintendo names) live.
  setLabels(labels, glyphs) {
    this.labels = labels || CTRL_LABEL_SETS.gp2040;
    this.glyphs = glyphs || CTRL_GLYPH_SETS.gp2040;
    this.preloadGlyphs();
    if (this.loaded) this.render();
  }

  // Kick off fetches for the active layout's glyphs so they're ready before
  // the modal opens (avoids a text→icon flash).
  preloadGlyphs() {
    for (const id of new Set(Object.values(this.glyphs))) {
      if (!glyphCache.has(id)) ensureGlyph(this, id);
    }
  }

  buildDom(container) {
    this.root = document.createElement('div');
    this.root.className = 'controller-widget';
    this.svg = document.createElementNS('http://www.w3.org/2000/svg', 'svg');
    this.svg.classList.add('cgp-svg');
    this.svg.setAttribute('viewBox', this.viewBox);
    this.svg.addEventListener('click', (e) => this.handleClick(e));
    this.root.appendChild(this.svg);
    container.appendChild(this.root);
  }

  async loadSvg() {
    try {
      const res = await fetch('/controller.svg');
      const text = await res.text();
      const match = text.match(CTRL_VIEWBOX_RE);
      if (match) this.viewBox = match[1];
      this.markup = text
        .replace(/<\?xml[^>]*\?>\s*/i, '')
        .replace(/<svg[^>]*>/, '')
        .replace(/<\/svg>\s*$/, '');
      this.svg.innerHTML = this.markup;
      this.svg.setAttribute('viewBox', this.viewBox);
      this.loaded = true;
      this.styleSvg();
      this.render();
    } catch (e) {
      // SVG missing (e.g. dev without the file): leave the container empty.
    }
  }

  // Neutralize the source SVG's hardcoded fills/strokes so it follows the
  // theme, and make every control clickable.
  styleSvg() {
    const drawable = 'path, rect, circle, ellipse, polygon, polyline, line';
    for (const el of this.svg.querySelectorAll(drawable)) {
      el.setAttribute('vector-effect', 'non-scaling-stroke');
      el.style.stroke = 'var(--bg-4)';
      el.style.setProperty('stroke-width', '2', 'important');
      const isBtn = this.isBtnId(el.id);
      if (isBtn) {
        el.style.setProperty('cursor', 'pointer', 'important');
        el.style.setProperty('pointer-events', 'all', 'important');
        el.style.fill = 'var(--bg-3)';
      } else if (el.id && STICK_WELLS.includes(el.id)) {
        el.style.fill = 'var(--bg-1)';
      } else {
        // The controller body (#case) and anything else left over.
        el.style.fill = 'var(--bg-1)';
      }
    }
  }

  isBtnId(id) {
    return id != null && CTRL_ELS.some((el) => el.id === id);
  }

  // Overlay a label centered on each control: a glyph icon when the active
  // layout has one loaded, otherwise a text label. Labels live on the svg (not
  // the control) and are pointer-events:none, so clicks pass through to the
  // shape underneath.
  updateLabels() {
    this.svg.querySelectorAll('.cgp-label').forEach((el) => el.remove());
    // getBBox() returns all zeros while the widget is inside a display:none
    // subtree (e.g. before the key modal opens), which would pile every label
    // at the origin. Skip placement until the widget is actually laid out.
    if (this.svg.getClientRects().length === 0) return;
    const ns = 'http://www.w3.org/2000/svg';
    for (const def of CTRL_ELS) {
      const node = this.svg.getElementById(def.id);
      if (!node) continue;
      const bbox = node.getBBox();
      const cx = bbox.x + bbox.width / 2;
      const cy = bbox.y + bbox.height / 2;
      const glyphId = this.glyphs[def.labelKey];
      const glyph = glyphId && glyphCache.get(glyphId);
      if (glyph) {
        this.appendGlyph(def.id, cx, cy, bbox, glyph);
      } else {
        if (glyphId) ensureGlyph(this, glyphId);
        const text = document.createElementNS(ns, 'text');
        text.setAttribute('x', String(cx));
        text.setAttribute('y', String(cy + 1));
        text.setAttribute('text-anchor', 'middle');
        text.setAttribute('dominant-baseline', 'central');
        text.setAttribute('id', 'label-' + def.id);
        text.classList.add('cgp-label');
        text.textContent = this.labels[def.labelKey] ?? def.labelKey;
        this.svg.appendChild(text);
      }
    }
  }

  // Place a glyph's shapes as a group centered on the control, scaled to fit
  // the control's smaller dimension.
  appendGlyph(defId, cx, cy, bbox, glyph) {
    const [vx, vy, vw, vh] = glyph.viewBox;
    const fit = Math.min(bbox.width, bbox.height) * 0.58;
    const scale = vh > 0 ? fit / vh : 1;
    const ns = 'http://www.w3.org/2000/svg';
    const g = document.createElementNS(ns, 'g');
    g.setAttribute('id', 'glyph-' + defId);
    g.classList.add('cgp-label');
    g.setAttribute('transform',
      `translate(${cx} ${cy}) scale(${scale}) translate(${-vx - vw / 2} ${-vy - vh / 2})`);
    for (const n of glyph.nodes) {
      const el = document.createElementNS(ns, n.tag);
      for (const [name, value] of n.attrs) el.setAttribute(name, value);
      g.appendChild(el);
    }
    this.svg.appendChild(g);
  }

  render() {
    if (!this.loaded) return;
    for (const def of CTRL_ELS) {
      const node = this.svg.getElementById(def.id);
      if (!node) continue;
      const selected = (this.mask & def.mask) !== 0;
      node.style.stroke = selected ? SELECTED_STROKE : 'var(--bg-4)';
      if (selected) {
        // Re-append to the end so a selected control draws on top of its
        // neighbors (e.g. the sticks over the stick wells).
        node.parentNode.appendChild(node);
        node.style.setProperty('stroke-width', '3', 'important');
      } else {
        node.style.setProperty('stroke-width', '2', 'important');
      }
    }
    this.updateLabels();
    for (const def of CTRL_ELS) {
      const label = this.svg.getElementById('label-' + def.id);
      if (label) label.style.setProperty('fill', 'currentColor', 'important');
    }
  }

  handleClick(e) {
    let el = e.target;
    while (el && el !== this.svg) {
      const id = el.getAttribute && el.getAttribute('id');
      const def = id && CTRL_ELS.find((d) => d.id === id);
      if (def) {
        this.setMask(this.mask ^ def.mask);
        this.onChange(this.mask);
        return;
      }
      el = el.parentElement;
    }
  }
}

// Per-layout label sets, shared with the gamepad multi-select in app.js.
ControllerWidget.LABELS = CTRL_LABEL_SETS;
// Per-layout glyph sets, used by the widget (and swapped by app.js for the
// Nintendo-layout toggle).
ControllerWidget.GLYPHS = CTRL_GLYPH_SETS;