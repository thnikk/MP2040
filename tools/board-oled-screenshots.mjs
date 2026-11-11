#!/usr/bin/env node
// Board + OLED composite screenshots for the MP2040 display simulator.
//
// For each display state this renders the bare framebuffer via the simulator
// and overlays it inside the `oled` element of the board's SVG, then captures
// the board screenshot in headless Chromium over CDP. The full 128x64 frame is
// fit inside the OLED rect with xMidYMid meet (black letterbox bars read as
// panel bezel), covering the on-SVG logo, so the result looks like the board
// with its real screen lit.
//
// The simulator is compiled for one board; the matching configs/<Board>/board.svg
// is used (build.sh records the baked board in tools/display_sim/build/board).
//
// Usage:
//   node tools/board-oled-screenshots.mjs                 # display at native 1:1
//   node tools/board-oled-screenshots.mjs --scale 3      # whole board 3x instead
//   node tools/board-oled-screenshots.mjs --display-scale 2   # 2px per OLED pixel
//   node tools/board-oled-screenshots.mjs --svg          # also pixel SVG per state
//   node tools/board-oled-screenshots.mjs --dir out --board Springboard
//   node tools/board-oled-screenshots.mjs --fg 7fd0ff    # blue OLED tint
//
// Output: screenshots/board-oled-<state>.png (gitignored). With --svg the
// frame is additionally serialized as true vector pixels inside the board
// (shape-rendering=crispEdges), saved as board-oled-<state>.svg.

import { spawn, execFileSync, execSync } from 'node:child_process';
import { existsSync, mkdirSync, readFileSync, writeFileSync, openSync } from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { DISPLAY_STATES } from './display-state-table.mjs';

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const ROOT = path.resolve(__dirname, '..');
const LOGDIR = '/tmp/opencode';
mkdirSync(LOGDIR, { recursive: true });

// ---- CLI ----------------------------------------------------------------

const argv = process.argv.slice(2);
const arg = (flag, dflt) => {
  const i = argv.indexOf(flag);
  return i >= 0 && argv[i + 1] ? argv[i + 1] : dflt;
};
const SIM = path.resolve(ROOT, arg('--sim', 'tools/display_sim/build/mp2040-display-sim'));
const OUT = path.resolve(ROOT, arg('--dir', 'screenshots'));
const SCALE = parseInt(arg('--scale', '3'), 10) || 1;
const FG = arg('--fg', null);
const BG = arg('--bg', null);
const BASE_CDP = parseInt(arg('--cdp', '9224'), 10);
// --display-scale N (default 1): size the composite so each OLED pixel is
// exactly N output px (1 = the display at native 1:1). When --scale is given
// it wins instead and scales the whole board by that factor.
const DISPLAY_SCALE = parseFloat(arg('--display-scale', '1')) || 1;
const SCALE_SET = argv.includes('--scale');
// --svg: also serialize each composite as a pixel-accurate SVG (in addition to
// the PNG). The simulator writes a 128x64 1bpp mask that the page turns into
// per-pixel <rect>s, so the result stays crisp at any zoom in every viewer.
const SVG_OUT = argv.includes('--svg');

function bakedBoard() {
  const marker = path.join(ROOT, 'tools/display_sim/build/board');
  try {
    const b = readFileSync(marker, 'utf8').trim();
    if (b) return b;
  } catch {}
  return 'Fightboard';
}
const BOARD = arg('--board', null) || bakedBoard();
const BOARD_SVG = path.join(ROOT, 'configs', BOARD, 'board.svg');

if (!existsSync(SIM)) {
  console.error(`simulator not built: ${SIM}\nrun tools/display_sim/build.sh first`);
  process.exit(1);
}
if (!existsSync(BOARD_SVG)) {
  console.error(`no board svg at ${path.relative(ROOT, BOARD_SVG)} (--board ${BOARD})`);
  process.exit(1);
}
mkdirSync(OUT, { recursive: true });

// ---- helpers ------------------------------------------------------------

const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

function portInUse(port) {
  try {
    return execSync(`ss -ltn 2>/dev/null | rg -q ':${port}\\s' && echo yes || echo no`)
      .toString().trim() === 'yes';
  } catch {
    return false;
  }
}

async function findFreePort(start) {
  let port = start;
  for (;;) {
    if (!portInUse(port)) return port;
    port += 1;
  }
}

async function waitForHttp(url, what, timeoutMs = 30000) {
  const start = Date.now();
  for (;;) {
    try {
      const res = await fetch(url);
      if (res.ok) return;
    } catch {}
    if (Date.now() - start > timeoutMs) throw new Error(`timeout waiting for ${what} (${url})`);
    await sleep(250);
  }
}

function killByPort(port) {
  try { execSync(`fuser -k ${port}/tcp >/dev/null 2>&1`); } catch {}
}

function spawnDetached(cmd, args, env, logName) {
  const log = path.join(LOGDIR, logName);
  const out = openSync(log, 'a');
  const child = spawn(cmd, args, {
    cwd: ROOT,
    env: { ...process.env, ...env },
    stdio: ['ignore', out, out],
    detached: true,
  });
  child.unref();
  return child.pid;
}

function cdpConnect(wsUrl) {
  const ws = new WebSocket(wsUrl);
  let id = 0;
  const pending = new Map();
  ws.onmessage = (ev) => {
    const m = JSON.parse(ev.data);
    if (m.id && pending.has(m.id)) {
      const p = pending.get(m.id);
      pending.delete(m.id);
      if (m.error) p.reject(new Error(m.error.message));
      else p.resolve(m.result);
    }
  };
  const ready = new Promise((res, rej) => { ws.onopen = res; ws.onerror = rej; });
  const send = (method, params = {}) => new Promise((resolve, reject) => {
    const mid = ++id;
    pending.set(mid, { resolve, reject });
    ws.send(JSON.stringify({ id: mid, method, params }));
  });
  const evalJs = async (expression) => {
    const res = await send('Runtime.evaluate', { expression, returnByValue: true, awaitPromise: true });
    if (res.exceptionDetails) throw new Error('JS error: ' + (res.exceptionDetails.exception?.description || res.exceptionDetails.text));
    return res.result ? res.result.value : undefined;
  };
  const poll = async (expr, timeoutMs = 20000) => {
    const start = Date.now();
    for (;;) {
      const v = await evalJs(expr);
      if (v) return v;
      if (Date.now() - start > timeoutMs) throw new Error('poll timeout: ' + expr);
      await sleep(300);
    }
  };
  return { ws, ready, send, evalJs, poll };
}

// ---- page builder -------------------------------------------------------

// Board SVG inside a self-contained page. A script locates the `oled`
// element, measures it in the root user coordinate space (so mirrored boards
// like Fightboard-m and the logo underneath still work), and overlays the
// frame either as an <image> (PNG data URI, used for PNG screenshots) or as
// per-pixel <rect>s from the 1bpp mask (used for pixel-accurate SVG output).
// Either way the 128x64 frame is fitted with xMidYMid meet: square pixels,
// letterboxed inside the panel rect.
function buildPage(svgMarkup, payload) {
  const isPixels = payload.kind === 'pixels';

  const inject = `
(function(){
  try {
    var svg = document.querySelector('svg');
    var oled = null;
    var els = svg.querySelectorAll('[id]');
    for (var i = 0; i < els.length; i++) {
      var el = els[i];
      if (el.id === 'oled' || el.getAttribute('inkscape:label') === 'oled') { oled = el; break; }
    }
    if (!oled) { document.documentElement.dataset.oledReady = 'NO-OLED'; return; }
    var svgRect = svg.getBoundingClientRect();
    var or = oled.getBoundingClientRect();
    var inv = svg.getScreenCTM().inverse();
    var pt = svg.createSVGPoint();
    function toUser(cx, cy) {
      pt.x = cx - svgRect.left; pt.y = cy - svgRect.top;
      var p = pt.matrixTransform(inv);
      return { x: p.x, y: p.y };
    }
    var tl = toUser(or.left, or.top);
    var br = toUser(or.right, or.bottom);
    var x = tl.x, y = tl.y, w = br.x - tl.x, h = br.y - tl.y;
    if (!(w > 0 && h > 0)) { document.documentElement.dataset.oledReady = 'BAD-RECT'; return; }
    var ns = 'http://www.w3.org/2000/svg';

    // "meet" fit: the 128x64 frame maps to this many user units per OLED pixel.
    var scl = Math.min(w / 128, h / 64);

    var rx = oled.getAttribute('rx');
    var clipId = null;
    if (rx) {
      var clip = document.createElementNS(ns, 'clipPath');
      clip.setAttribute('id', 'oled-clip');
      var r = document.createElementNS(ns, 'rect');
      r.setAttribute('x', String(x)); r.setAttribute('y', String(y));
      r.setAttribute('width', String(w)); r.setAttribute('height', String(h));
      r.setAttribute('rx', rx);
      r.setAttribute('ry', oled.getAttribute('ry') || rx);
      clip.appendChild(r);
      svg.appendChild(clip);
      clipId = 'url(#oled-clip)';
    }

    // The on-SVG GP2040 logo is the boot splash: hide it so the panel looks
      // like it does while the UI is showing (also keeps it out of the
      // serialized SVG, where the pixel overlay can't "cover" transparent gaps).
      var logoEls = svg.querySelectorAll('[id]');
      for (var l = 0; l < logoEls.length; l++) {
        var logoEl = logoEls[l];
        var logoNames = [logoEl.id, logoEl.getAttribute('inkscape:label')].filter(Boolean);
        if (logoNames.some(function (n) { return n.toLowerCase() === 'logo'; })) {
          logoEl.setAttribute('style', 'display:none');
        }
      }

    ${isPixels ? `
    // Letterboxed content box (square pixels, centered).
    var cx = x + (w - 128 * scl) / 2;
    var cy = y + (h - 64 * scl) / 2;
    var g = document.createElementNS(ns, 'g');
    g.setAttribute('shape-rendering', 'crispEdges');
    g.setAttribute('fill', '__FG__');
    if (clipId) g.setAttribute('clip-path', clipId);
    var bytes = atob('__MASK__');
    for (var row = 0; row < 64; row++) {
      var base = row * 16;
      for (var b = 0; b < 16; b++) {
        var bytev = bytes.charCodeAt(base + b);
        if (!bytev) continue;
        for (var bit = 0; bit < 8; bit++) {
          if (bytev & (0x80 >> bit)) {
            var rect = document.createElementNS(ns, 'rect');
            rect.setAttribute('x', String(cx + (b * 8 + bit) * scl));
            rect.setAttribute('y', String(cy + row * scl));
            rect.setAttribute('width', String(scl));
            rect.setAttribute('height', String(scl));
            g.appendChild(rect);
          }
        }
      }
    }
    svg.appendChild(g);
    ` : `
    var img = document.createElementNS(ns, 'image');
    img.setAttribute('x', String(x));
    img.setAttribute('y', String(y));
    img.setAttribute('width', String(w));
    img.setAttribute('height', String(h));
    img.setAttribute('preserveAspectRatio', 'xMidYMid meet');
    img.setAttribute('href', '__DATA_URI__');
    img.setAttribute('xlink:href', '__DATA_URI__');
    img.style.setProperty('image-rendering', 'pixelated');
    if (clipId) img.setAttribute('clip-path', clipId);
    svg.appendChild(img);
    `}
    document.documentElement.dataset.oledReady = '1';
    document.documentElement.dataset.oledCell = String(scl);
    document.documentElement.dataset.oledRect = ['x','y','w','h'].map(
      function (k) { return k + ':' + ({ x: x, y: y, w: w, h: h }[k].toFixed(2)); }).join(',');
  } catch (e) {
    document.documentElement.dataset.oledReady = 'ERR:' + (e && e.message);
  }
})();`;

  const script = isPixels
    ? inject.replace('__FG__', payload.fg).replace('__MASK__', payload.maskB64)
    : inject.replace('__DATA_URI__', payload.dataUri);

  return `<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<style>
  html, body { margin: 0; padding: 0; background: #fff; }
  svg { display: block; }
</style>
</head>
<body>
${svgMarkup}
<script>${script}</script>
</body>
</html>`;
}

// ---- main ---------------------------------------------------------------

async function main() {
  const cdpPort = await findFreePort(BASE_CDP);
  console.log(`board: ${BOARD}  chromium CDP: http://127.0.0.1:${cdpPort}`);
  console.log(`output: ${path.relative(ROOT, OUT)}/  display ${DISPLAY_SCALE}:1${SCALE_SET ? `  board scale ${SCALE}x` : ''}`);

  const svgMarkup = readFileSync(BOARD_SVG, 'utf8');
  // viewBox "x y w h" for scaling the whole board in layout (user units -> px).
  const vbMatch = svgMarkup.match(/viewBox="[\d.\s-]+ ([\d.]+) ([\d.]+)"/);
  const vbW = vbMatch ? parseFloat(vbMatch[1]) : 602;
  const vbH = vbMatch ? parseFloat(vbMatch[2]) : 188;

  const chromePid = spawnDetached(
    'chromium',
    ['--headless=new', '--no-sandbox', '--disable-gpu',
     `--remote-debugging-port=${cdpPort}`,
     `--user-data-dir=${LOGDIR}/board-oled-profile`,
     '--window-size=800,400',
     'about:blank'],
    {},
    'board-oled-chromium.log',
  );

  let cdp;
  try {
    await waitForHttp(`http://127.0.0.1:${cdpPort}/json/list`, 'chromium CDP');
    const list = await (await fetch(`http://127.0.0.1:${cdpPort}/json/list`)).json();
    const page = list.find((t) => t.type === 'page') || list[0];
    cdp = cdpConnect(page.webSocketDebuggerUrl);
    await cdp.ready;
    await cdp.send('Page.enable');
    await cdp.send('Runtime.enable');

    let good = 0;
    for (const [name, state] of DISPLAY_STATES) {
      // Bare framebuffer frame (1x, tiny data URI) + optional 1bpp mask.
      const framePath = path.join(LOGDIR, `board-oled-${name}.frame.png`);
      const maskPath = path.join(LOGDIR, `board-oled-${name}.mask`);
      const args = ['--screen', state.screen, '--png', framePath, '--scale', '1'];
      if (SVG_OUT) args.push('--mask', maskPath);
      if (state.nav) args.push('--nav', state.nav);
      if (state.saverMode !== undefined) args.push('--saver-mode', String(state.saverMode));
      if (state.timeMs !== undefined) args.push('--time-ms', String(state.timeMs));
      if (state.inputMode) args.push('--input-mode', state.inputMode);
      if (state.config) args.push('--config');
      if (FG) args.push('--fg', FG);
      if (BG) args.push('--bg', BG);
      try {
        execFileSync(SIM, args, { stdio: 'ignore' });
      } catch (e) {
        console.error(`FAILED frame ${name}: ${e.message}`);
        continue;
      }
      const frameB64 = readFileSync(framePath).toString('base64');
      let payload;
      if (SVG_OUT) {
        const mask = readFileSync(maskPath);
        if (mask.length !== 1024)
          throw new Error(`unexpected mask size ${mask.length} (expected 1024 for 128x64)`);
        payload = { kind: 'pixels', maskB64: mask.toString('base64'), fg: FG || '#ffffff' };
      } else {
        payload = { kind: 'image', dataUri: `data:image/png;base64,${frameB64}` };
      }
      const html = buildPage(svgMarkup, payload);
      const htmlPath = path.join(LOGDIR, `board-oled-${name}.html`);
      writeFileSync(htmlPath, html);

      await cdp.send('Page.navigate', { url: `file://${htmlPath}` });
      await cdp.poll(`document.documentElement.dataset.oledReady === '1'`, 15000);
      if (process.env.OLDRECT) {
        const rect = await cdp.evalJs(`document.documentElement.dataset.oledRect || ''`);
        console.error(`  oled rect (${name}): ${rect}`);
      }

      // Pixel-accurate SVG: serialize the board + overlay from the DOM.
      if (SVG_OUT) {
        const svgText = await cdp.evalJs(
          `new XMLSerializer().serializeToString(document.querySelector('svg'))`);
        const svgPath = path.join(OUT, `board-oled-${name}.svg`);
        writeFileSync(svgPath, '<?xml version="1.0" encoding="UTF-8"?>\n' + svgText);
        console.log(`  saved board-oled-${name}.svg (vector pixels)`);
      }

      // Let the (data-URI) image decode before capturing.
      await sleep(350);

      // Size the capture: either the whole-board --scale multiplier, or (the
      // default) scale the SVG layout so each OLED pixel is exactly
      // DISPLAY_SCALE output px. Scaling via CSS layout keeps deviceScaleFactor
      // at 1, so the per-pixel rects land on an integer grid.
      const cell = await cdp.evalJs(
        `parseFloat(document.documentElement.dataset.oledCell || '0')`);
      const mult = SCALE_SET ? SCALE : (cell > 0 ? DISPLAY_SCALE / cell : DISPLAY_SCALE);
      const pxW = Math.ceil(vbW * mult);
      const pxH = Math.ceil(vbH * mult);
      await cdp.evalJs(`(() => {
        const s = document.querySelector('svg');
        s.style.width = '${pxW}px';
        s.style.height = '${pxH}px';
        return true;
      })()`);
      await cdp.send('Emulation.setDeviceMetricsOverride', {
        width: pxW, height: pxH, deviceScaleFactor: 1, mobile: false,
      });
      await sleep(100);

      const shot = await cdp.send('Page.captureScreenshot', { format: 'png' });
      const file = path.join(OUT, `board-oled-${name}.png`);
      writeFileSync(file, Buffer.from(shot.data, 'base64'));
      const oledPx = (mult * cell).toFixed(2);
      console.log(`  saved board-oled-${name}.png (${pxW}x${pxH}, ${oledPx} px/oled px)`);
      good++;
    }
    console.log(`done -> ${good}/${DISPLAY_STATES.length} boards in ${path.relative(ROOT, OUT)}/`);
  } finally {
    if (cdp) cdp.ws.close();
    killByPort(cdpPort);
    try { execSync(`kill ${chromePid} >/dev/null 2>&1`); } catch {}
    await sleep(300);
    const leftover = execSync(`ss -ltnp 2>/dev/null | rg ':${cdpPort}\\b' || true`).toString();
    if (leftover.trim()) console.error('WARNING: port still in use:\n' + leftover);
  }
}

main().catch((e) => { console.error('FAIL:', e.message); process.exit(1); });