// BoardView — a vanilla port of GP2040-th's BoardSVG component.
//
// Loads the board's SVG (served as /board.svg), styles it dark, labels each
// button element (identified by "pin"/"key" in its id or inkscape:label) with
// its key assignment (key + modifiers), colors the #led-N slots that are
// mapped in pinLedIndices, and wires pin / LED / test clicks.

const SHAPE_TAGS = ['path', 'rect', 'circle', 'ellipse', 'polygon', 'polyline', 'line'];
const SHAPE_SEL = 'path, rect, circle, ellipse, polygon, polyline, line';
const LABEL_RE = /-label$/i;

// Returns { pinNumber, name } if el is a button: the element id or its
// inkscape:label starts with "pin"/"key" followed by digits (matrix boards
// use "keyNN", direct-pin boards "pinNN"). Skips "-label" guide elements
// (e.g. "pin27-label"), which are label-positioning paths, not buttons.
// name is the matched id or inkscape:label (used to find the "-label" guide).
function matchButtonIndex(el, isMatrix) {
  const re = isMatrix ? /^key(\d+)/i : /^pin(\d+)/i;
  for (const name of [el.id, el.getAttribute('inkscape:label')]) {
    if (!name || LABEL_RE.test(name)) continue;
    const m = name.match(re);
    if (m) return { pinNumber: parseInt(m[1], 10), name };
  }
  return null;
}

const MODIFIER_SHORT = {
  0xe0: 'Ctrl', 0xe1: 'Shift', 0xe2: 'Alt', 0xe3: 'Win',
  0xe4: 'Ctrl', 0xe5: 'Shift', 0xe6: 'Alt', 0xe7: 'Win',
};

const KEY_LABELS = {
  0x28: 'Enter', 0x29: 'Esc', 0x2a: 'Bksp', 0x2b: 'Tab', 0x2c: 'Space',
  0x2d: '-', 0x2e: '=', 0x2f: '[', 0x30: ']', 0x31: '\\', 0x32: '\\',
  0x33: ';', 0x34: "'", 0x35: '`', 0x36: ',', 0x37: '.', 0x38: '/', 0x39: 'Caps',
  0x46: 'PrtSc', 0x47: 'ScrLk', 0x48: 'Pause', 0x49: 'Ins', 0x4a: 'Home',
  0x4b: 'PgUp', 0x4c: 'Del', 0x4d: 'End', 0x4e: 'PgDn',
  0x4f: '\u2192', 0x50: '\u2190', 0x51: '\u2193', 0x52: '\u2191',
  0x53: 'NumLk', 0x58: 'KPEnt', 0x63: 'KP.', 0x65: 'Menu', 0x66: 'Power', 0x67: 'KP=',
  0x7f: 'Mute', 0x80: 'Vol+', 0x81: 'Vol-',
  0xe8: 'Next', 0xe9: 'Prev', 0xf0: 'Stop', 0xf1: 'Play',
  0xf2: 'Mute', 0xf3: 'Vol+', 0xf4: 'Vol-',
};

function keyLabel(code) {
  if (code >= 0x04 && code <= 0x1d) return String.fromCharCode(0x41 + code - 0x04); // A-Z
  if (code === 0x27) return '0';
  if (code >= 0x1e && code <= 0x26) return String.fromCharCode(0x31 + code - 0x1e); // 1-9
  if (code >= 0x3a && code <= 0x45) return 'F' + (code - 0x3a + 1); // F1-F12
  if (code >= 0x68 && code <= 0x73) return 'F' + (code - 0x68 + 13); // F13-F24
  if (code >= 0x59 && code <= 0x62) return 'KP' + (code - 0x59 + 1); // KP1-KP9
  return KEY_LABELS[code] || '';
}

const MIDI_NOTE_NAMES = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B'];
function midiNoteName(note) {
  if (note <= 0) return '';
  const octave = Math.floor(note / 12) - 1;
  return `${MIDI_NOTE_NAMES[note % 12]}${octave}`;
}

function isShape(el) {
  return SHAPE_TAGS.includes(el.tagName.toLowerCase());
}

function shapesOf(el) {
  return isShape(el) ? [el] : Array.from(el.querySelectorAll(SHAPE_SEL));
}

// Find an element by its id OR inkscape:label (Inkscape exports often put the
// meaningful name in inkscape:label and use a generic id like "rect88").
function findByRef(root, ref) {
  const els = root.querySelectorAll('[id]');
  for (const el of els) {
    if (el.id === ref || el.getAttribute('inkscape:label') === ref) return el;
  }
  return null;
}

// True if el or an ancestor matches any of the given refs (id or label).
function matchesRef(el, refs) {
  for (let n = el; n; n = n.parentElement) {
    if (n.getAttribute) {
      if (refs.includes(n.id) || refs.includes(n.getAttribute('inkscape:label'))) return true;
    }
  }
  return false;
}

// All per-key LED elements: ids like "led-0" (GP2040) or "led0" (Inkscape).
function ledElements(root) {
  const leds = [];
  root.querySelectorAll('[id]').forEach((el) => {
    if (/^led-?\d+$/.test(el.id)) leds.push(el);
  });
  return leds;
}

function intToCss(value) {
  return '#' + value.toString(16).padStart(6, '0');
}

function prepareSvg(svg) {
  return svg.replace(/<svg([^>]*)>/, (match, attrs) => {
    let cleaned = attrs
      .replace(/\s+width="[^"]*"/g, '')
      .replace(/\s+height="[^"]*"/g, '');
    if (!cleaned.includes('viewBox')) {
      cleaned += ' viewBox="0 0 100 100"';
    }
    return `<svg${cleaned}>`;
  });
}

function parsePathEndpoints(d) {
  const numRe = /-?\d+(?:\.\d*)?(?:e[+-]?\d+)?/gi;
  const nums = [...d.matchAll(numRe)].map((m) => parseFloat(m[0]));
  const cmds = d.match(/[mMlLhHvV]/g) || [];
  if (nums.length < 2 || cmds.length < 1) return null;

  let x = 0, y = 0;
  const points = [];
  let ni = 0;

  const takePair = (rel) => {
    if (ni + 1 >= nums.length) return;
    x = rel ? x + nums[ni] : nums[ni]; ni++;
    y = rel ? y + nums[ni] : nums[ni]; ni++;
  };
  const takeSingle = (rel, axis) => {
    if (ni >= nums.length) return;
    if (axis === 'x') x = rel ? x + nums[ni] : nums[ni];
    else y = rel ? y + nums[ni] : nums[ni];
    ni++;
  };

  for (let ci = 0; ci < cmds.length && ni < nums.length; ci++) {
    const cmd = cmds[ci];
    const rel = cmd === cmd.toLowerCase();
    switch (cmd.toLowerCase()) {
      case 'm':
        takePair(rel);
        points.push({ x, y });
        while (ni + 1 < nums.length) {
          takePair(rel);
          points.push({ x, y });
        }
        break;
      case 'l':
        takePair(rel);
        points.push({ x, y });
        break;
      case 'h':
        takeSingle(rel, 'x');
        points.push({ x, y });
        break;
      case 'v':
        takeSingle(rel, 'y');
        points.push({ x, y });
        break;
    }
  }

  if (points.length < 2) return null;
  return { x1: points[0].x, y1: points[0].y, x2: points[points.length - 1].x, y2: points[points.length - 1].y };
}

class BoardView {
  constructor(container, callbacks) {
    this.container = container;
    this.callbacks = callbacks || {};
    this.options = null;
    this.svgRoot = null;
    this.pinElements = [];
    this.heldPins = new Set();
    this.wired = false;
    this.ledSim = null;
    this.ledFrame = null;
    document.addEventListener('visibilitychange', () => {
      if (document.hidden) this.stopLedSim();
      else if (this.ledSim) this.startLedSim();
    });
    this.load();
  }

  async load() {
    try {
      const resp = await fetch('/board.svg');
      if (!resp.ok) throw new Error(`HTTP ${resp.status}`);
      const text = await resp.text();
      this.render(text);
    } catch (e) {
      this.container.innerHTML =
        '<div class="board-placeholder">No board graphic for this board.</div>';
    }
  }

  setOptions(options) {
    this.options = options;
    if (this.svgRoot) {
      this.updateLabels();
      this.applyLeds();
      this.applyPins();
    }
    this.updateLedSim();
  }

  setHeldPins(pins) {
    this.heldPins = new Set(pins);
    if (this.ledSim) this.ledSim.setHeld(pins || []);
    this.applyPins();
  }

  // Refresh button labels after a config change that doesn't affect the LED
  // sim (e.g. input mode toggle). Cheaper than setOptions: leaves the running
  // LED preview untouched.
  refresh() {
    if (!this.svgRoot) return;
    this.updateLabels();
    this.applyPins();
  }

  // Push live LED control values into the simulation (mirrors /api/setLedPreview).
  setLedParams(led) {
    if (!this.ledSim || !led) return;
    // Merge with the full led options so board properties (pinLedIndices,
    // ledsPerKey) survive preview pushes that only carry the control fields.
    this.ledSim.setParams({ ...this.options?.led, ...led });
  }

  // ---- render -----------------------------------------------------------

  render(svgText) {
    const parser = new DOMParser();
    const doc = parser.parseFromString(svgText, 'image/svg+xml');
    // Matrix boards index keys by linear matrix position (keyNN names) rather
    // than GPIO pins (pinNN names); either way the array index is the key
    // index. Buttons are found by "pin"/"key" in their id or inkscape:label,
    // skipping the "*-label" guide elements.
    const isMatrix = !!this.options?.matrix?.enabled;
    this.pinElements = [];
    doc.querySelectorAll('[id]').forEach((el) => {
      const match = matchButtonIndex(el, isMatrix);
      if (match) this.pinElements.push({ id: el.id, ...match });
    });
    this.pinElements.sort((a, b) => a.pinNumber - b.pinNumber);

    this.container.innerHTML = prepareSvg(svgText);
    this.svgRoot = this.container.querySelector('svg');
    if (!this.svgRoot) return;

    this.themeStyle();
    this.updateLabels();
    this.applyLeds();
    this.applyPins();
    this.styleTestButton();
    this.wireEvents();
    this.buildLedSim();

    if (typeof ResizeObserver !== 'undefined') {
      new ResizeObserver(() => {
        this.updateLabels();
        this.applyLeds();
        this.applyPins();
      }).observe(this.container);
    }
  }

  themeStyle() {
    this.container.querySelectorAll(SHAPE_SEL).forEach((el) => {
      el.setAttribute('vector-effect', 'non-scaling-stroke');
      if (matchesRef(el, ['logo'])) return;
      if (matchesRef(el, ['board-led'])) return;
      el.style.fill = 'var(--bg-1)';
      if (matchesRef(el, ['oled'])) {
        el.style.stroke = 'none';
      } else {
        el.style.stroke = 'var(--bg-4)';
        el.style.setProperty('stroke-width', '2', 'important');
      }
    });

    const caseEl = findByRef(this.container, 'case');
    if (caseEl) {
      caseEl.style.setProperty('fill', 'var(--bg-1)', 'important');
      caseEl.style.setProperty('stroke-width', '1', 'important');
    }
    const oledEl = findByRef(this.container, 'oled');
    if (oledEl) {
      oledEl.style.setProperty('fill', '#000000', 'important');
    }
  }

  // ---- labels -----------------------------------------------------------

  updateLabels() {
    if (!this.svgRoot) return;

    for (const { id, pinNumber, name } of this.pinElements) {
      const el = this.container.querySelector(`#${CSS.escape(id)}`);
      if (!el) continue;

      let labelEl = el.querySelector('.pin-action-label');
      if (!labelEl) {
        const ns = 'http://www.w3.org/2000/svg';
        labelEl = document.createElementNS(ns, 'text');
        labelEl.setAttribute('class', 'pin-action-label');
        labelEl.setAttribute('text-anchor', 'middle');
        labelEl.setAttribute('dominant-baseline', 'central');
        labelEl.setAttribute('font-family', 'Nunito');
        labelEl.setAttribute('font-size', '11');
        labelEl.setAttribute('font-weight', 'bold');
        labelEl.setAttribute('fill', 'currentColor');

        if (isShape(el)) {
          const g = document.createElementNS(ns, 'g');
          g.setAttribute('id', id);
          el.parentNode.insertBefore(g, el);
          g.appendChild(el);
          el.removeAttribute('id');
          g.appendChild(labelEl);
        } else {
          el.appendChild(labelEl);
        }
      }

      const keycode = Number(this.options?.keycodes?.[pinNumber] || 0);
      const mask = Number(this.options?.modifierMasks?.[pinNumber] || 0);
      const midiMode = Number(this.options?.defaultInputMode || 1) === 2;
      const midiNote = Number(this.options?.midiNotes?.[pinNumber] || 0);

      let lines = [];
      if (midiMode) {
        if (midiNote > 0) lines.push(midiNoteName(midiNote));
      } else if (keycode > 0) {
        for (let i = 0; i < 8; i++) {
          if (mask & (1 << i)) lines.push(MODIFIER_SHORT[0xe0 + i] || '');
        }
        lines.push(keycode < 0xe0 ? keyLabel(keycode) : (MODIFIER_SHORT[keycode] || keyLabel(keycode)));
      }

      // Position: use the <name>-label guide path if present, else shape
      // center. Looked up by id or inkscape:label so buttons named either way
      // still land their label on the guide.
      const guidePath = findByRef(this.container, `${name}-label`);
      let cx, cy, rotation = null;
      if (guidePath) {
        const ep = parsePathEndpoints(guidePath.getAttribute('d') || '');
        if (ep) {
          cx = (ep.x1 + ep.x2) / 2;
          cy = (ep.y1 + ep.y2) / 2;
          const dx = ep.x2 - ep.x1;
          const dy = ep.y2 - ep.y1;
          if (dx !== 0 || dy !== 0) {
            rotation = Math.atan2(dy, dx) * (180 / Math.PI);
            if (rotation > 90 || rotation < -90) rotation += 180;
          }
        }
      } else {
        const shape = shapesOf(el)[0];
        if (shape) {
          const rect = shape.getBoundingClientRect();
          cx = rect.left + rect.width / 2;
          cy = rect.top + rect.height / 2;
          const ctm = this.svgRoot.getScreenCTM();
          if (ctm) {
            const pt = new DOMPoint(cx, cy).matrixTransform(ctm.inverse());
            cx = pt.x;
            cy = pt.y;
          }
        }
      }
      if (cx === undefined || cy === undefined) continue;

      const ns = 'http://www.w3.org/2000/svg';
      while (labelEl.firstChild) labelEl.removeChild(labelEl.firstChild);
      labelEl.removeAttribute('x');
      labelEl.removeAttribute('y');
      if (lines.length > 0) {
        const lineHeight = 14;
        lines.forEach((line, idx) => {
          const tspan = document.createElementNS(ns, 'tspan');
          tspan.textContent = line;
          tspan.setAttribute('x', String(cx));
          tspan.setAttribute('y', String(cy + (idx - (lines.length - 1) / 2) * lineHeight));
          tspan.setAttribute('text-anchor', 'middle');
          labelEl.appendChild(tspan);
        });
      } else {
        labelEl.setAttribute('x', String(cx));
        labelEl.setAttribute('y', String(cy));
      }

      if (rotation !== null) {
        labelEl.setAttribute('transform', `rotate(${rotation}, ${cx}, ${cy})`);
      } else if (labelEl.hasAttribute('transform')) {
        labelEl.removeAttribute('transform');
      }
    }
  }

  // ---- LED slots --------------------------------------------------------

  applyLeds() {
    // The live simulation paints per-LED colors every frame; keep this static
    // fill only as the fallback / pre-sim baseline.
    if (this.ledSim) return;

    const color = this.options?.led?.colorNormal != null
      ? intToCss(this.options.led.colorNormal) : '#4caf50';

    ledElements(this.container).forEach((led) => {
      shapesOf(led).forEach((s) => {
        s.style.setProperty('fill', color, 'important');
        s.style.setProperty('opacity', '0.85', 'important');
        s.style.setProperty('stroke', 'var(--bg-4)', 'important');
        s.style.setProperty('stroke-width', '1.5', 'important');
      });
    });
  }

  // ---- LED simulation ---------------------------------------------------

  // Rebuild the LedSim from the rendered SVG (LED centers + led options).
  buildLedSim() {
    this.stopLedSim();
    this.ledSim = null;
    if (!this.svgRoot || !this.options?.led) return;

    const leds = ledElements(this.container);
    if (!leds.length) return;

    const positions = [];
    leds.forEach((el) => {
      const idx = parseInt(el.id.replace(/^led-?/, ''), 10);
      const shape = shapesOf(el)[0];
      if (!shape) return;
      const rect = shape.getBoundingClientRect();
      if (rect.width === 0 && rect.height === 0) return;
      const ctm = this.svgRoot.getScreenCTM();
      if (!ctm) return;
      const pt = new DOMPoint(rect.left + rect.width / 2, rect.top + rect.height / 2)
        .matrixTransform(ctm.inverse());
      positions[idx] = { x: pt.x, y: pt.y };
    });

    if (!positions.some((p) => p)) return;
    for (let i = 0; i < positions.length; i++) {
      if (positions[i] === undefined) positions[i] = null;
    }

    this.ledSim = new LedSim(positions);
    this.ledSim.setParams(this.options.led);
    this.ledSim.setHeld([...this.heldPins]);
    this.startLedSim();
  }

  // (Re)build the sim if it hasn't been built yet (e.g. setOptions before the
  // SVG finished loading), otherwise just refresh its parameters.
  updateLedSim() {
    if (this.ledSim) this.setLedParams(this.options?.led);
    else if (this.svgRoot) this.buildLedSim();
  }

  startLedSim() {
    if (this.ledFrame || !this.ledSim || document.hidden) return;
    this.ledSim.resync();
    const loop = () => {
      this.ledFrame = requestAnimationFrame(loop);
      this.paintLedSim();
    };
    this.ledFrame = requestAnimationFrame(loop);
  }

  stopLedSim() {
    if (this.ledFrame) {
      cancelAnimationFrame(this.ledFrame);
      this.ledFrame = null;
    }
  }

  paintLedSim() {
    if (!this.ledSim) return;
    const colors = this.ledSim.tick();
    ledElements(this.container).forEach((el) => {
      const idx = parseInt(el.id.replace(/^led-?/, ''), 10);
      const c = colors[idx];
      if (!c) return;
      const rgb = `rgb(${c[0]}, ${c[1]}, ${c[2]})`;
      shapesOf(el).forEach((s) => {
        s.style.setProperty('fill', rgb, 'important');
      });
    });
  }

  // ---- test button ------------------------------------------------------

  // Represent the test button the way GP2040 does: a green shape with a
  // "Test" label. If the SVG has it as a bare shape, wrap it in a <g> so it
  // carries the #test-btn id for click handling.
  styleTestButton() {
    let btn = findByRef(this.container, 'test-btn');
    if (!btn) return;

    const isShape = SHAPE_TAGS.includes(btn.tagName.toLowerCase());
    let target = btn;
    if (isShape) {
      if (!btn.querySelector('text')) {
        const ns = 'http://www.w3.org/2000/svg';
        const g = document.createElementNS(ns, 'g');
        g.setAttribute('id', 'test-btn');
        const parent = btn.parentNode;
        if (!parent) return;
        const x = parseFloat(btn.getAttribute('x') || '0');
        const y = parseFloat(btn.getAttribute('y') || '0');
        const w = parseFloat(btn.getAttribute('width') || '60');
        const h = parseFloat(btn.getAttribute('height') || '30');
        const label = document.createElementNS(ns, 'text');
        label.setAttribute('text-anchor', 'middle');
        label.setAttribute('dominant-baseline', 'central');
        label.setAttribute('font-family', 'Nunito');
        label.setAttribute('font-size', '12');
        label.setAttribute('font-weight', 'bold');
        label.setAttribute('fill', 'currentColor');
        label.setAttribute('stroke', 'var(--bg-1)');
        label.setAttribute('stroke-width', '2');
        label.setAttribute('stroke-linejoin', 'round');
        label.setAttribute('paint-order', 'stroke fill');
        label.setAttribute('x', String(btn.hasAttribute('transform') ? -(x + w / 2) : x + w / 2));
        label.setAttribute('y', String(y + h / 2));
        label.textContent = 'Test';
        parent.insertBefore(g, btn);
        g.appendChild(btn);
        g.appendChild(label);
      }
      target = btn;
    } else {
      target = btn.querySelector(SHAPE_SEL);
    }

    if (target) {
      target.style.setProperty('fill', '#a3be8c', 'important');
      target.style.setProperty('stroke', '#b5d9a5', 'important');
      target.style.removeProperty('opacity');
    }
  }

  // ---- pin styling / highlight ------------------------------------------

  applyPins() {
    this.pinElements.forEach(({ id, pinNumber }) => {
      const el = this.container.querySelector(`#${CSS.escape(id)}`);
      if (!el) return;

      const isHeld = this.heldPins.has(pinNumber);

      // All buttons get the same base fill (mapping is shown by the label
      // text); only held pins are highlighted differently.
      shapesOf(el).forEach((shape) => {
        const s = shape;
        if (isHeld) {
          s.style.setProperty('fill', '#3d3d00', 'important');
          s.style.setProperty('stroke', '#ffff00', 'important');
          s.style.setProperty('stroke-width', '3', 'important');
          s.style.removeProperty('fill-opacity');
        } else {
          s.style.setProperty('fill', 'var(--bg-2)', 'important');
          s.style.removeProperty('fill-opacity');
          s.style.setProperty('stroke', 'var(--bg-4)', 'important');
          s.style.setProperty('stroke-width', '2', 'important');
        }
      });
    });
  }

  // ---- events -----------------------------------------------------------

  wireEvents() {
    if (this.wired) return;
    this.wired = true;

    this.pinElements.forEach(({ id, pinNumber }) => {
      const el = this.container.querySelector(`#${CSS.escape(id)}`);
      if (!el) return;

      el.addEventListener('click', () => this.callbacks.onPinClick?.(pinNumber));
      el.style.setProperty('cursor', 'pointer');

      el.addEventListener('mouseenter', () => {
        shapesOf(el).forEach((s) => {
          s.style.setProperty('fill-opacity', '0.6', 'important');
        });
      });
      el.addEventListener('mouseleave', () => this.applyPins());
    });

    this.container.querySelectorAll('[id]').forEach((el) => {
      const m = el.id.match(/^led-?\d+$/);
      if (!m) return;
      el.addEventListener('click', () => {
        const idx = parseInt(el.id.replace(/^led-?/, ''), 10);
        this.callbacks.onLedClick?.(idx);
      });
    });

    const testBtn = findByRef(this.container, 'test-btn');
    if (testBtn) {
      testBtn.addEventListener('click', () => this.callbacks.onTest?.());
      testBtn.style.setProperty('cursor', 'pointer');
    }
  }
}
