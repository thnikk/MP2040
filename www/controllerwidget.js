// ControllerWidget — vanilla port of GP2040-th's gamepad picker
// (www/src/components/widgets/ControllerWidget.tsx). Renders an SVG gamepad;
// clicking a button or dpad direction toggles that control's bit in the pin's
// gamepad mask. The mask layout matches GAMEPAD_PIN_MASK_* in gamepadhelper.h:
// dpad in bits 0-3, buttons B1-A2 in bits 4-17.

// Control id → label and mask bit. The ids are the ones baked into
// controller.svg.
const CTRL_ELS = [
  { id: 'btn-l2', label: 'L2', mask: 0x0400 },
  { id: 'btn-r2', label: 'R2', mask: 0x0800 },
  { id: 'btn-l1', label: 'L1', mask: 0x0100 },
  { id: 'btn-r1', label: 'R1', mask: 0x0200 },
  { id: 'btn-a1', label: 'A1', mask: 0x10000 },
  { id: 'btn-a2', label: 'A2', mask: 0x20000 },
  { id: 'btn-s1', label: 'S1', mask: 0x1000 },
  { id: 'btn-s2', label: 'S2', mask: 0x2000 },
  { id: 'btn-b4', label: 'B4', mask: 0x0080 },
  { id: 'btn-b3', label: 'B3', mask: 0x0040 },
  { id: 'btn-b2', label: 'B2', mask: 0x0020 },
  { id: 'btn-b1', label: 'B1', mask: 0x0010 },
  { id: 'btn-up', label: 'Up', mask: 0x0001 },
  { id: 'btn-down', label: 'Down', mask: 0x0002 },
  { id: 'btn-left', label: 'Left', mask: 0x0004 },
  { id: 'btn-right', label: 'Right', mask: 0x0008 },
  { id: 'btn-l3', label: 'L3', mask: 0x4000 },
  { id: 'btn-r3', label: 'R3', mask: 0x8000 },
];

// Stick wells are just visual (the L3/R3 sticks sit on top); they're not
// clickable and get a recessed fill.
const STICK_WELLS = ['circle5', 'circle6', 'circle13'];

const CTRL_VIEWBOX_RE = /viewBox="([^"]+)"/;
const SELECTED_STROKE = '#00ff00';

class ControllerWidget {
  constructor({ container, mask, onChange }) {
    this.mask = mask || 0;
    this.onChange = onChange || (() => {});
    this.markup = '';
    this.viewBox = '0 0 434.5 366';
    this.loaded = false;
    this.buildDom(container);
    this.loadSvg();
  }

  setMask(mask) {
    this.mask = mask || 0;
    if (this.loaded) this.render();
  }

  getMask() {
    return this.mask;
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
      } else if (el.id !== 'path1') {
        el.style.fill = 'var(--bg-1)';
      }
    }
  }

  isBtnId(id) {
    return id != null && CTRL_ELS.some((el) => el.id === id);
  }

  // Overlay a label centered on each control. Labels live on the svg (not the
  // control) and are pointer-events:none, so clicks pass through to the shape.
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
      const text = document.createElementNS(ns, 'text');
      text.setAttribute('x', String(bbox.x + bbox.width / 2));
      text.setAttribute('y', String(bbox.y + bbox.height / 2 + 1));
      text.setAttribute('text-anchor', 'middle');
      text.setAttribute('dominant-baseline', 'central');
      text.setAttribute('id', 'label-' + def.id);
      text.classList.add('cgp-label');
      text.textContent = def.label;
      this.svg.appendChild(text);
    }
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