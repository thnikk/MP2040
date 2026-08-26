// BoardView — a vanilla port of GP2040-th's BoardSVG component.
//
// Loads the board's SVG (served as /board.svg), styles it dark, labels each
// button element (identified by "pin"/"key" in its id or inkscape:label) with
// its key assignment (key + modifiers), colors the #led-N slots that are
// mapped in pinLedIndices, and wires pin / LED / test clicks.

const SHAPE_TAGS = ['path', 'rect', 'circle', 'ellipse', 'polygon', 'polyline', 'line'];
const SHAPE_SEL = 'path, rect, circle, ellipse, polygon, polyline, line';
const LABEL_RE = /-label$/i;
// Display multiplier for the board's natural physical size: boards render at
// 1.5× their real footprint (mm) so they read better in the panel while
// staying proportional to each other and clamped to the container.
const BOARD_VIEW_SCALE = 1.5;

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

// Modifier mask / keycode label helpers (MODIFIER_SHORT, KEY_LABELS,
// keyLabel) come from kblayout.js.

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

// Parse the authored <svg> width/height into physical millimetres. The board
// SVGs are drawn to scale but mix two unit systems: some declare mm units
// (e.g. 3x3), the rest are Inkscape px at 96dpi (1 unit = 1/96 inch). Convert
// px to mm (× 25.4/96) so every board's rendered size reflects its real size.
function parseSvgSizeMm(svgText) {
  const parse = (attr) => {
    const m = svgText.match(new RegExp(`\\b${attr}="([0-9.]+)(mm|cm|in|pt|pc|px)?"`, 'i'));
    if (!m) return null;
    const value = parseFloat(m[1]);
    switch ((m[2] || 'px').toLowerCase()) {
      case 'mm': return value;
      case 'cm': return value * 10;
      case 'in': return value * 25.4;
      case 'pt': return value * 25.4 / 72;
      case 'pc': return value * 25.4 / 6;
      default: return value * 25.4 / 96; // px
    }
  };
  const w = parse('width');
  const h = parse('height');
  if (w == null && h == null) return null;
  return { w, h };
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
    this.ringElement = null;
    this.ringShapes = [];
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
      this.applyRing();
      this.applyLedCursors();
    }
    this.updateLedSim();
  }

  setHeldPins(pins) {
    this.heldPins = new Set(pins);
    if (this.ledSim) this.ledSim.setHeld(pins || []);
    this.applyPins();
  }

  // Refresh button labels after a config change that doesn't affect the LED
  // sim (e.g. input mode toggle). Cheaper than setOptions: leaves a running
  // LED preview untouched, but builds the sim if it was skipped because the
  // layout page was hidden when the SVG rendered (getBoundingClientRect
  // returns zero-size boxes for display:none elements).
  refresh() {
    if (!this.svgRoot) return;
    this.updateLabels();
    this.applyPins();
    this.applyLedCursors();
    if (!this.ledSim) this.updateLedSim();
  }

  // Push live LED control values into the simulation (mirrors /api/setLedPreview).
  setLedParams(led) {
    if (!led) return;
    // Merge with the full led options so board properties (pinLedIndices,
    // ledsPerKey) survive preview pushes that only carry the control fields.
    if (this.options) this.options.led = { ...(this.options.led || {}), ...led };
    if (this.ledSim) this.ledSim.setParams(this.options?.led);
    this.applyLedCursors();
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
    // A touch ring is a single clickable object (id/inkscape:label "ring"),
    // not a set of buttons. Its descendant/self shapes must not be treated as
    // individual pins.
    const ringEl = findByRef(doc, 'ring');
    doc.querySelectorAll('[id]').forEach((el) => {
      const match = matchButtonIndex(el, isMatrix);
      if (match) this.pinElements.push({ id: el.id, ...match });
    });
    // Drop anything that is the ring or lives inside it.
    if (ringEl) this.pinElements = this.pinElements.filter((p) => p.id !== ringEl.id && !ringEl.contains(
      doc.getElementById(p.id)));
    this.pinElements.sort((a, b) => a.pinNumber - b.pinNumber);

    this.container.innerHTML = prepareSvg(svgText);
    this.svgRoot = this.container.querySelector('svg');
    if (!this.svgRoot) return;

    // Size the board by its real physical footprint (mm) × BOARD_VIEW_SCALE,
    // clamped to the panel by CSS. Browsers render mm at 96dpi, so the SVG
    // scales naturally and the container's max-width/max-height clamp it while
    // preserving aspect ratio.
    const sizeMm = parseSvgSizeMm(svgText);
    if (sizeMm) {
      if (sizeMm.w != null) this.svgRoot.setAttribute('width', `${sizeMm.w * BOARD_VIEW_SCALE}mm`);
      if (sizeMm.h != null) this.svgRoot.setAttribute('height', `${sizeMm.h * BOARD_VIEW_SCALE}mm`);
    } else {
      // No parsable size: fall back to filling the panel width.
      this.svgRoot.style.width = '100%';
    }

    this.ringElement = findByRef(this.svgRoot, 'ring');
    this.ringShapes = this.ringElement ? shapesOf(this.ringElement) : [];

    this.themeStyle();
    this.updateLabels();
    this.applyLeds();
    this.applyPins();
    this.applyRing();
    this.applyLedCursors();
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
      // Elements inside an "ignore" group keep their authored fill/stroke.
      if (matchesRef(el, ['ignore'])) return;
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
      caseEl.style.setProperty('stroke-width', '2', 'important');
    }
    const oledEl = findByRef(this.container, 'oled');
    if (oledEl) {
      oledEl.style.setProperty('fill', '#000000', 'important');
    }
  }

  // ---- labels -----------------------------------------------------------

  updateLabels() {
    if (!this.svgRoot) return;

    // Scale factor from SVG user units to screen pixels (horizontal). The
    // board SVG is scaled responsively, so SVG text (which uses user units)
    // would grow/shrink with it. Counter-scale the label so it stays a
    // constant pixel size on screen regardless of the board's render width.
    let labelScale = 1;
    const ctm = this.svgRoot.getScreenCTM();
    if (ctm && ctm.a > 0) labelScale = ctm.a;

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
        labelEl.setAttribute('font-size', '24');
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
      // Counter-scale the font so the label stays ~24px on screen regardless
      // of the board's CSS scale factor (labelScale = screen px / user unit).
      labelEl.setAttribute('font-size', String(24 / labelScale));

      const keycode = Number(this.options?.keycodes?.[pinNumber] || 0);
      const mask = Number(this.options?.modifierMasks?.[pinNumber] || 0);
      const macroIndex = Number(this.options?.macroIndices?.[pinNumber] || 0);
      const midiMode = Number(this.options?.defaultInputMode || 1) === 2;
      const gamepadMode = [3, 4].includes(Number(this.options?.defaultInputMode || 1));
      const midiNote = Number(this.options?.midiNotes?.[pinNumber] || 0);
      const gamepadMask = Number(this.options?.gamepadMasks?.[pinNumber] || 0);

      let lines = [];
      if (midiMode) {
        if (midiNote > 0) lines.push(midiNoteName(midiNote));
      } else if (gamepadMode) {
        // Gamepad mode: only gamepad control assignments are shown. A pin with
        // no controls assigned is unassigned in this mode (not its keyboard key).
        if (gamepadMask > 0) lines.push(...gamepadMaskLabels(gamepadMask));
      } else if (macroIndex > 0) {
        lines.push('M' + macroIndex);
      } else if (keycode > 0) {
        for (let i = 0; i < 8; i++) {
          if (mask & (1 << i)) lines.push(MODIFIER_SHORT[0xe0 + i] || '');
        }
        lines.push(keycode < 0xe0 ? keyLabel(keycode) : (MODIFIER_SHORT[keycode] || keyLabel(keycode)));
      }

      // Position the label. Prefer the <name>-label guide path (id or
      // inkscape:label) for intentional placement/rotation, but only trust it
      // when it actually lies on (or near) the button: guides are authored in
      // the button's layer coordinate space, and boards whose button layer is
      // transformed independently of the guide layer would otherwise misplace
      // the label. Fall back to the shape's true center (transform-aware) when
      // the guide doesn't overlap the shape.
      const guidePath = findByRef(this.container, `${name}-label`);
      const shape = shapesOf(el)[0];
      let shapeBox = null;
      const ctm = this.svgRoot.getScreenCTM();
      if (shape && ctm) {
        const r = shape.getBoundingClientRect();
        const tl = new DOMPoint(r.left, r.top).matrixTransform(ctm.inverse());
        const br = new DOMPoint(r.right, r.bottom).matrixTransform(ctm.inverse());
        shapeBox = {
          x: Math.min(tl.x, br.x),
          y: Math.min(tl.y, br.y),
          width: Math.abs(br.x - tl.x),
          height: Math.abs(br.y - tl.y),
        };
      }

      let cx, cy, rotation = null;
      if (guidePath) {
        const ep = parsePathEndpoints(guidePath.getAttribute('d') || '');
        if (ep) {
          const gx = (ep.x1 + ep.x2) / 2;
          const gy = (ep.y1 + ep.y2) / 2;
          // Trust the guide only if it plausibly sits on the button. The
          // tolerance (half the shape's diagonal) allows guides that offset
          // the label slightly from dead-center while rejecting guides in a
          // mismatched coordinate space.
          let guideOnShape = true;
          if (shapeBox) {
            const tol = Math.hypot(shapeBox.width, shapeBox.height) / 2 + 2;
            guideOnShape =
              gx >= shapeBox.x - tol && gx <= shapeBox.x + shapeBox.width + tol &&
              gy >= shapeBox.y - tol && gy <= shapeBox.y + shapeBox.height + tol;
          }
          if (guideOnShape) {
            cx = gx;
            cy = gy;
            const dx = ep.x2 - ep.x1;
            const dy = ep.y2 - ep.y1;
            if (dx !== 0 || dy !== 0) {
              rotation = Math.atan2(dy, dx) * (180 / Math.PI);
              if (rotation > 90 || rotation < -90) rotation += 180;
            }
          }
        }
      }
      if (cx === undefined && shapeBox) {
        // Fall back to the shape's true center (transform-aware).
        cx = shapeBox.x + shapeBox.width / 2;
        cy = shapeBox.y + shapeBox.height / 2;
      } else if (cx === undefined && shape) {
        const rect = shape.getBoundingClientRect();
        cx = rect.left + rect.width / 2;
        cy = rect.top + rect.height / 2;
        if (ctm) {
          const pt = new DOMPoint(cx, cy).matrixTransform(ctm.inverse());
          cx = pt.x;
          cy = pt.y;
        }
      }
      if (cx === undefined || cy === undefined) continue;

      // cx/cy are in SVG-root user units (from the guide path or the shape's
      // transform-aware center). The label text, however, lives inside the
      // button's <g>, which may be nested in a transformed layer (e.g. a
      // "buttons" layer with a translate). Placing it at root units directly
      // would double-apply that layer transform and push it off-screen. Convert
      // the root-space position into the label's own local coordinate system so
      // it renders at the intended screen location regardless of layer nesting.
      const labelCtm = labelEl.getScreenCTM();
      if (labelCtm) {
        const svgCtm = this.svgRoot.getScreenCTM();
        if (svgCtm) {
          const screenPt = new DOMPoint(cx, cy).matrixTransform(svgCtm);
          const local = screenPt.matrixTransform(labelCtm.inverse());
          cx = local.x;
          cy = local.y;
        }
      }

      const ns = 'http://www.w3.org/2000/svg';
      while (labelEl.firstChild) labelEl.removeChild(labelEl.firstChild);
      labelEl.removeAttribute('x');
      labelEl.removeAttribute('y');
      if (lines.length > 0) {
        // 26px on screen per line, counter-scaled like the font.
        const lineHeight = 26 / labelScale;
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

    const led = this.options?.led;
    const mode = led?.ledMode ?? 0;
    const color = led?.colorNormalByMode?.[mode] != null
      ? intToCss(led.colorNormalByMode[mode])
      : (led?.colorNormal != null ? intToCss(led.colorNormal) : '#4caf50');

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

  // Per-key LEDs only open the color popover in custom LED mode, so only show
  // the pointer cursor (and click affordance) in that mode.
  applyLedCursors() {
    const custom = Number(this.options?.led?.ledMode ?? 0) === 0;
    ledElements(this.container).forEach((el) => {
      el.style.setProperty('cursor', custom ? 'pointer' : 'not-allowed');
    });
    if (custom) this.hideLedTooltip();
  }

  // ---- LED tooltip ------------------------------------------------------

  // Custom hover tooltip for LEDs in non-custom LED modes, where clicking an
  // LED is inert: explains why. Shows instantly on hover (no native delay).
  showLedTooltip(el) {
    if (this.ledTooltipEl) return;
    const tip = document.createElement('div');
    tip.className = 'board-tooltip';
    tip.textContent = 'Must be on custom mode to change individual LED colors.';
    document.body.appendChild(tip);
    this.ledTooltipEl = tip;
    this.positionLedTooltip(el);

    this.tooltipHide = () => this.hideLedTooltip();
    window.addEventListener('scroll', this.tooltipHide, { capture: true, passive: true });
    window.addEventListener('resize', this.tooltipHide);
  }

  positionLedTooltip(el) {
    const tip = this.ledTooltipEl;
    if (!tip) return;
    const rect = el.getBoundingClientRect();
    const tw = tip.offsetWidth;
    const th = tip.offsetHeight;
    let left = rect.left + rect.width / 2 - tw / 2;
    left = Math.max(8, Math.min(left, window.innerWidth - tw - 8));
    let top = rect.top - th - 8;
    if (top < 8) top = rect.bottom + 8;
    tip.style.left = left + 'px';
    tip.style.top = top + 'px';
  }

  hideLedTooltip() {
    if (this.tooltipHide) {
      window.removeEventListener('scroll', this.tooltipHide, { capture: true });
      window.removeEventListener('resize', this.tooltipHide);
      this.tooltipHide = null;
    }
    if (this.ledTooltipEl) {
      this.ledTooltipEl.remove();
      this.ledTooltipEl = null;
    }
  }

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
          s.style.setProperty('fill', 'var(--bg-2)', 'important');
          s.style.setProperty('fill-opacity', '0.6', 'important');
          s.style.setProperty('stroke', 'var(--nord13)', 'important');
          s.style.setProperty('stroke-width', '3', 'important');
        } else {
          s.style.setProperty('fill', 'var(--bg-2)', 'important');
          s.style.removeProperty('fill-opacity');
          s.style.setProperty('stroke', 'var(--bg-4)', 'important');
          s.style.setProperty('stroke-width', '2', 'important');
        }
      });
    });
  }

  // Style the touch ring as a single object: base fill + one label at its
  // center. Called on render and on mouseleave (to undo the hover highlight).
  applyRing() {
    if (!this.ringElement) return;
    this.ringShapes.forEach((s) => {
      s.style.setProperty('fill', 'var(--bg-2)', 'important');
      s.style.removeProperty('fill-opacity');
      s.style.setProperty('stroke', 'var(--bg-4)', 'important');
      s.style.setProperty('stroke-width', '2', 'important');
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

    // The touch ring is one clickable object that opens the ring modal.
    if (this.ringElement) {
      this.ringElement.addEventListener('click', () => this.callbacks.onRingClick?.());
      this.ringElement.style.setProperty('cursor', 'pointer');
      this.ringShapes.forEach((s) => {
        s.style.setProperty('cursor', 'pointer');
      });
      this.ringElement.addEventListener('mouseenter', () => {
        this.ringShapes.forEach((s) => s.style.setProperty('fill-opacity', '0.6', 'important'));
      });
      this.ringElement.addEventListener('mouseleave', () => this.applyRing());
    }

    this.container.querySelectorAll('[id]').forEach((el) => {
      const m = el.id.match(/^led-?\d+$/);
      if (!m) return;
      el.addEventListener('click', () => {
        const idx = parseInt(el.id.replace(/^led-?/, ''), 10);
        this.callbacks.onLedClick?.(idx, el);
      });
      el.addEventListener('mouseenter', () => {
        if (Number(this.options?.led?.ledMode ?? 0) !== 0) this.showLedTooltip(el);
      });
      el.addEventListener('mouseleave', () => this.hideLedTooltip());
    });

    const testBtn = findByRef(this.container, 'test-btn');
    if (testBtn) {
      testBtn.addEventListener('click', () => this.callbacks.onTest?.());
      testBtn.style.setProperty('cursor', 'pointer');
    }
  }
}
