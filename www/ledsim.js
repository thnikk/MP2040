// LedSim — browser-side simulation of the firmware's LED themes
// (src/leds/LedController.cpp). Mirrors each mode on the board SVG so the web
// configurator shows what the physical strip is doing, even without a board.

const LED_MODE_CUSTOM = 0;
const LED_MODE_CYCLE = 1;
const LED_MODE_REACTIVE = 2;
const LED_MODE_BPS = 3;
const LED_MODE_RIPPLE = 4;
const LED_MODE_RAIN = 5;

const MAX_RIPPLES = 8;

// Rain drop interval bounds (ms): a random drop fires every 0.2-2 seconds.
const RAIN_DROP_MIN_MS = 200;
const RAIN_DROP_MAX_MS = 2000;

// Theme step interval (ms) bounds per mode, indexed by LED_MODE_* (mirrors
// speedRanges[] in LedController.cpp). The 0-100% speed maps exponentially
// into these: 0% = slowest, 100% = fastest. BPS steps on the fixed render
// cadence and is handled separately in renderBps().
const speedRanges = [
  { min: 0, max: 0 },     // LED_MODE_CUSTOM
  { min: 6, max: 117 },   // LED_MODE_CYCLE
  { min: 16, max: 250 },  // LED_MODE_REACTIVE
  { min: 0, max: 0 },     // LED_MODE_BPS
  { min: 50, max: 660 },  // LED_MODE_RIPPLE
  { min: 25, max: 200 },  // LED_MODE_RAIN (fade step; drop interval is fixed 1-3s)
];

// HSV -> RGB matching the firmware's hsvToRgb (Adafruit ColorHSV variant).
// hue/sat/val are 0-255; integer math mirrors the C++ exactly.
function hsvToRgb(h, s, v) {
  if (s === 0) return [v, v, v];
  const hue32 = Math.floor((h * 1529) / 255);
  const sextant = hue32 >> 8;
  const f = hue32 & 0xFF;
  const pv = Math.floor((v * (255 - s)) / 255);
  const qv = Math.floor((v * (255 - Math.floor((s * f) / 255))) / 255);
  const tv = Math.floor((v * (255 - Math.floor((s * (255 - f)) / 255))) / 255);
  switch (sextant) {
    case 0: return [v, tv, pv];
    case 1: return [qv, v, pv];
    case 2: return [pv, v, tv];
    case 3: return [pv, qv, v];
    case 4: return [tv, pv, v];
    default: return [v, pv, qv];
  }
}

function chebyshev(a, b) {
  if (!a || !b) return 0;
  return Math.max(Math.abs(a.x - b.x), Math.abs(a.y - b.y));
}

class LedSim {
  // positions: array of {x, y} SVG-space centers, indexed by LED strip index.
  // Missing LEDs are null (not drawn in the SVG) and contribute nothing.
  constructor(positions) {
    this.positions = positions;
    this.count = positions.length;
    this.pressed = new Array(this.count).fill(false);
    this.ledSat = new Array(this.count).fill(0);
    this.ledVal = new Array(this.count).fill(0);
    this.hue = 0;
    this.prevHeld = new Set();
    this.bpsCount = 0;
    this.bpsColor = 0;
    this.lastColor = 0;
    this.ripples = [];
    this.rainDropMillis = 0;
    this.rainRandState = 0;

    this.mode = LED_MODE_CUSTOM;
    this.themeInterval = 20;
    this.brightness = 255;
    this.colorNormal = 0x00ff00;
    this.colorPressed = 0xffffff;
    this.ledsPerKey = 1;
    this.pinLedIndices = [];
    this.lastThemeMillis = performance.now();
    this.lastBpsMillis = performance.now();

    // Grid cell unit: median nearest-neighbor Chebyshev distance, used to
    // turn real pixel spacing into grid-cell radii for the ripple theme.
    const neighbors = [];
    for (let i = 0; i < this.count; i++) {
      if (!this.positions[i]) continue;
      let best = Infinity;
      for (let j = 0; j < this.count; j++) {
        if (i === j || !this.positions[j]) continue;
        const d = chebyshev(this.positions[i], this.positions[j]);
        if (d < best) best = d;
      }
      if (best !== Infinity) neighbors.push(best);
    }
    neighbors.sort((a, b) => a - b);
    this.cell = neighbors.length ? neighbors[Math.floor(neighbors.length / 2)] : 1;
    if (!(this.cell > 0)) this.cell = 1;
  }

  // Apply live LED options; resets theme state like the firmware's
  // applyLedPreview (LedController.cpp:256).
  setParams(p) {
    this.mode = p.ledMode ?? LED_MODE_CUSTOM;
    this.ledSpeedPercent = (p.ledSpeed ?? 50) <= 100 ? p.ledSpeed : 50;
    this.recomputeInterval();
    // Always render at full brightness in the config UI so colors are easy to
    // see; brightnessMaximum still dims the physical board via setLedPreview.
    this.brightness = 255;
    this.colorNormal = p.colorNormal ?? 0x00ff00;
    this.colorPressed = p.colorPressed ?? 0xffffff;
    this.normalColors = p.ledNormalColors || [];
    this.pressedColors = p.ledPressedColors || [];
    this.ledsPerKey = Math.max(1, p.ledsPerKey || 1);
    this.pinLedIndices = p.pinLedIndices || [];
    this.resetTheme();
  }

  // Map the 0-100% speed to a theme step interval for the current mode
  // (exponential, mirroring LedController::recomputeLedSpeed()).
  recomputeInterval() {
    const r = speedRanges[this.mode] || { min: 0, max: 0 };
    if (!r.min || !r.max) {
      this.themeInterval = 20; // CUSTOM (or unknown mode)
      return;
    }
    const pct = Math.max(0, Math.min(100, this.ledSpeedPercent));
    const t = Math.pow(r.min / r.max, pct / 100);
    this.themeInterval = Math.max(1, Math.min(1000, Math.round(r.max * t)));
  }

  resetTheme() {
    this.hue = 0;
    this.ledSat.fill(0);
    this.ledVal.fill(0);
    this.ripples = [];
    this.bpsCount = 0;
    this.bpsColor = 0;
    this.lastColor = 0;
    this.prevHeld = new Set();
    this.rainRandState = (Math.floor(performance.now()) ^ 0x9e3779b9) | 0;
    this.rainDropMillis = 0;
    this.resync();
  }

  // Realign the animation clocks (call when resuming after a pause so the
  // catch-up logic doesn't fast-forward the whole hidden period).
  resync() {
    this.lastThemeMillis = performance.now();
    this.lastBpsMillis = performance.now();
  }

  // Update held pins from the board's long-polled state; counts rising edges
  // for BPS and spawns ripples, matching the firmware's update().
  setHeld(pins) {
    const held = new Set(pins || []);
    const rising = [];
    for (const pin of held) {
      if (!this.prevHeld.has(pin)) rising.push(pin);
    }
    this.prevHeld = held;
    if (rising.length) this.bpsCount++;

    this.pressed.fill(false);
    held.forEach((pin) => {
      const idx = this.pinLedIndices[pin];
      if (idx === undefined || idx < 0) return;
      for (let l = 0; l < this.ledsPerKey; l++) {
        const i = idx + l;
        if (i >= 0 && i < this.count) this.pressed[i] = true;
      }
    });

    for (const pin of rising) {
      const idx = this.pinLedIndices[pin];
      if (idx === undefined || idx < 0) continue;
      this.spawnRipple(idx);
    }
  }

  // Chebyshev distance in grid cells between two strip indices.
  distCells(a, b) {
    return Math.round(chebyshev(this.positions[a], this.positions[b]) / this.cell);
  }

  // Largest ring (in cells) a ripple from `origin` can reach.
  maxGridDistance(origin) {
    let max = 0;
    for (let j = 0; j < this.count; j++) {
      max = Math.max(max, this.distCells(origin, j));
    }
    return max;
  }

  // Start a ripple at a pressed LED; reuse the oldest slot when all are busy.
  spawnRipple(origin) {
    const ripple = { origin, radius: 0, active: true };
    for (let i = 0; i < this.ripples.length; i++) {
      if (!this.ripples[i].active) {
        this.ripples[i] = ripple;
        return;
      }
    }
    if (this.ripples.length < MAX_RIPPLES) {
      this.ripples.push(ripple);
    } else {
      let oldest = 0;
      for (let i = 1; i < this.ripples.length; i++) {
        if (this.ripples[i].radius > this.ripples[oldest].radius) oldest = i;
      }
      this.ripples[oldest] = ripple;
    }
  }

  // Advance the theme state one step at the configured animation speed.
  advanceThemeState() {
    switch (this.mode) {
      case LED_MODE_CYCLE:
        this.hue--;
        break;

      case LED_MODE_REACTIVE:
        for (let i = 0; i < this.count; i++) {
          if (!this.pressed[i]) {
            if (this.ledSat[i] < 255) this.ledSat[i] = Math.min(255, this.ledSat[i] + 8);
            if (this.ledSat[i] === 255 && this.ledVal[i] > 0) this.ledVal[i] = Math.max(0, this.ledVal[i] - 8);
          } else {
            this.ledSat[i] = 0;
            this.ledVal[i] = 255;
          }
        }
        this.hue -= 8;
        if (this.hue < 0) this.hue = 255;
        break;

      case LED_MODE_RIPPLE:
        for (let i = 0; i < this.ripples.length; i++) {
          if (!this.ripples[i].active) continue;
          this.ripples[i].radius++;
          if (this.ripples[i].radius > this.maxGridDistance(this.ripples[i].origin))
            this.ripples[i].active = false;
        }
        break;

      case LED_MODE_RAIN:
        for (let i = 0; i < this.count; i++) {
          // Held LEDs show the pressed color in renderRain() and are not
          // treated as drops; freeze their fade while pressed.
          if (!this.pressed[i] && this.ledVal[i] > 0) {
            this.ledVal[i] = Math.max(0, this.ledVal[i] - 8);
          }
        }
        break;
    }
  }

  // xorshift32 PRNG mirroring LedController::rainRandom().
  rainRandom() {
    let x = this.rainRandState | 0;
    x ^= x << 13;
    x ^= x >>> 17;
    x ^= x << 5;
    this.rainRandState = x | 0;
    return x >>> 0;
  }

  scaled(color, scale) {
    return [
      Math.floor(((color >> 16) & 0xFF) * scale),
      Math.floor(((color >> 8) & 0xFF) * scale),
      Math.floor((color & 0xFF) * scale),
    ];
  }

  renderCustom() {
    const scale = this.brightness / 255;
    const n = this.scaled(this.colorNormal, scale);
    const p = this.scaled(this.colorPressed, scale);

    // Unmapped LEDs (and keys with no per-key color, or a value of 0) show the
    // global colors; per-key entries override them.
    const out = [];
    for (let i = 0; i < this.count; i++) {
      out.push(this.pressed[i] ? p.slice() : n.slice());
    }

    for (let pin = 0; pin < this.pinLedIndices.length; pin++) {
      const idx = this.pinLedIndices[pin];
      if (idx === undefined || idx < 0) continue;
      const hasCustom = pin < this.normalColors.length;
      const normal = hasCustom && this.normalColors[pin] !== 0
        ? this.normalColors[pin] : this.colorNormal;
      const pressed = hasCustom && this.pressedColors[pin] !== 0
        ? this.pressedColors[pin] : this.colorPressed;
      const ns = this.scaled(normal, scale);
      const ps = this.scaled(pressed, scale);
      for (let l = 0; l < this.ledsPerKey; l++) {
        const i = idx + l;
        if (i >= this.count) break;
        out[i] = this.pressed[i] ? ps.slice() : ns.slice();
      }
    }
    return out;
  }

  renderCycle() {
    const out = [];
    for (let i = 0; i < this.count; i++) {
      if (this.pressed[i]) {
        out.push([this.brightness, this.brightness, this.brightness]);
      } else {
        out.push(hsvToRgb((this.hue + i * 20) & 0xFF, 255, this.brightness));
      }
    }
    return out;
  }

  renderReactive() {
    const out = [];
    for (let i = 0; i < this.count; i++) {
      out.push(hsvToRgb((this.hue + i * 50) & 0xFF, this.ledSat[i],
        Math.floor((this.ledVal[i] * this.brightness) / 255)));
    }
    return out;
  }

  renderBps() {
    const now = performance.now();
    if (now - this.lastBpsMillis >= 1000) {
      this.lastColor = this.bpsColor;
      this.bpsColor = this.bpsCount * 10;
      this.bpsCount = 0;
      this.lastBpsMillis = now;
    }
    // Color-smoothing step per render (fixed cadence); 0-100% maps linearly
    // to 1..8, mirroring renderBps() in LedController.cpp.
    const pct = Math.max(0, Math.min(100, this.ledSpeedPercent));
    const bpsSpeed = 1 + Math.floor((pct * 7) / 100);
    if (this.lastColor > this.bpsColor) {
      this.lastColor -= bpsSpeed;
      if (this.lastColor - this.bpsColor < bpsSpeed) this.lastColor = this.bpsColor;
    } else if (this.lastColor < this.bpsColor) {
      this.lastColor += bpsSpeed;
      if (this.bpsColor - this.lastColor < bpsSpeed) this.lastColor = this.bpsColor;
    }

    const finalColor = this.lastColor % 256;
    const out = [];
    for (let i = 0; i < this.count; i++) {
      if (this.pressed[i]) {
        out.push([this.brightness, this.brightness, this.brightness]);
      } else {
        out.push(hsvToRgb((finalColor + 100) & 0xFF, 255, this.brightness));
      }
    }
    return out;
  }

  renderRipple() {
    const scale = this.brightness / 255;
    const n = this.scaled(this.colorNormal, scale);
    const p = this.scaled(this.colorPressed, scale);
    const out = this.pressed.map(() => n.slice());

    for (const ripple of this.ripples) {
      if (!ripple.active) continue;
      for (let j = 0; j < this.count; j++) {
        const dist = this.distCells(ripple.origin, j);
        if (dist === ripple.radius) out[j] = p.slice();
        else if (dist === ripple.radius - 1) out[j] = p.map((c) => c >> 1);
        else if (dist === ripple.radius - 2) out[j] = p.map((c) => c >> 2);
      }
    }
    return out;
  }

  renderRain() {
    const scale = this.brightness / 255;
    const n = this.scaled(this.colorNormal, scale);
    const p = this.scaled(this.colorPressed, scale);
    const out = [];
    for (let i = 0; i < this.count; i++) {
      if (this.pressed[i]) {
        out.push(p.slice());
      } else {
        const v = Math.max(0, this.ledVal[i]);
        out.push([
          Math.floor((n[0] * v) / 255),
          Math.floor((n[1] * v) / 255),
          Math.floor((n[2] * v) / 255),
        ]);
      }
    }
    return out;
  }

  // Advance theme state (catching up missed steps like the firmware) and
  // return the per-LED RGB array for the current frame.
  tick() {
    const now = performance.now();
    if (this.mode === LED_MODE_RAIN) {
      // Fire a random drop every 1-3 seconds on an unpressed LED, mirroring
      // update() in firmware. Held LEDs are skipped so presses never act as drops.
      this.rainRandState ^= Math.floor(now) | 0;
      if (now >= this.rainDropMillis && this.count > 0) {
        // Uniformly pick among unpressed LEDs (count, then select the kth).
        // If every LED is held, skip the drop.
        let unpressed = 0;
        for (let i = 0; i < this.count; i++) {
          if (!this.pressed[i]) unpressed++;
        }
        if (unpressed > 0) {
          let pick = this.rainRandom() % unpressed;
          for (let i = 0; i < this.count; i++) {
            if (!this.pressed[i]) {
              if (pick === 0) { this.ledVal[i] = 255; break; }
              pick--;
            }
          }
        }
        this.rainDropMillis =
          now + RAIN_DROP_MIN_MS +
          (this.rainRandom() % (RAIN_DROP_MAX_MS - RAIN_DROP_MIN_MS + 1));
      }
    }
    if (this.mode !== LED_MODE_CUSTOM) {
      if (now - this.lastThemeMillis >= this.themeInterval) {
        const elapsed = now - this.lastThemeMillis;
        const steps = Math.floor(elapsed / this.themeInterval);
        for (let s = 0; s < steps; s++) this.advanceThemeState();
        this.lastThemeMillis = now - (elapsed % this.themeInterval);
      }
    }
    switch (this.mode) {
      case LED_MODE_CYCLE: return this.renderCycle();
      case LED_MODE_REACTIVE: return this.renderReactive();
      case LED_MODE_BPS: return this.renderBps();
      case LED_MODE_RIPPLE: return this.renderRipple();
      case LED_MODE_RAIN: return this.renderRain();
      default: return this.renderCustom();
    }
  }
}
