// ControllerWidget — vanilla port of GP2040-th's gamepad picker
// (www/src/components/widgets/ControllerWidget.tsx). Renders an SVG gamepad;
// clicking a button or dpad direction toggles that control's bit in the pin's
// gamepad mask. The control definitions, per-layout label/glyph sets, mode→set
// lookup and glyph icon cache all live in gamepadlabels.js.

// Stick wells are just visual (the L3/R3 sticks sit on top); they're not
// clickable and get a recessed fill.
const STICK_WELLS = ['circle5', 'circle6', 'circle13'];

const CTRL_VIEWBOX_RE = /viewBox="([^"]+)"/;
const SELECTED_STROKE = '#00ff00';

class ControllerWidget {
  constructor({ container, mask, labels, glyphs, maskMap, onChange }) {
    this.mask = mask || 0;
    this.labels = labels || CTRL_LABEL_SETS.gp2040;
    this.glyphs = glyphs || CTRL_GLYPH_SETS.gp2040;
    this.maskMap = maskMap || null;
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
  setLabels(labels, glyphs, maskMap) {
    this.labels = labels || CTRL_LABEL_SETS.gp2040;
    this.glyphs = glyphs || CTRL_GLYPH_SETS.gp2040;
    this.maskMap = maskMap || null;
    this.preloadGlyphs();
    if (this.loaded) this.render();
  }

  // The raw mask bit for a control. maskMap lets a layout remap a control to a
  // different stored bit (Switch face buttons follow the Nintendo-layout
  // toggle, while the widget keeps showing the real Switch Pro arrangement).
  bitFor(def) {
    return (this.maskMap && this.maskMap[def.labelKey]) || def.mask;
  }

  // Kick off fetches for the active layout's glyphs so they're ready before
  // the modal opens (avoids a text→icon flash).
  preloadGlyphs() {
    for (const id of new Set(Object.values(this.glyphs))) {
      if (!glyphCache.has(id)) ensureGlyph(id, () => this.render());
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
        el.classList.add('cgp-btn');
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
      const glyph = glyphId ? loadedGlyph(glyphId) : null;
      if (glyph) {
        this.appendGlyph(def.id, cx, cy, bbox, glyph);
      } else {
        if (glyphId) ensureGlyph(glyphId, () => this.render());
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
      const selected = (this.mask & this.bitFor(def)) !== 0;
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
        this.setMask(this.mask ^ this.bitFor(def));
        this.onChange(this.mask);
        return;
      }
      el = el.parentElement;
    }
  }
}