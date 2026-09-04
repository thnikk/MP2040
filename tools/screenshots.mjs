#!/usr/bin/env node
// Screenshot automation for the MP2040 web configurator.
//
// Spawns a mock Vite server (www/ + server/app.js) and a headless Chromium,
// then drives it over CDP with Node's built-in WebSocket (no deps). Captures
// full-page PNGs of the Layout page in every LED mode, in light and dark.
//
// Usage:
//   node tools/screenshots.mjs                      # MacroPad, default LED mode
//   node tools/screenshots.mjs --board 2k --led-mode cycle
//   node tools/screenshots.mjs --modal key          # the 3 key-modal tabs
//   node tools/screenshots.mjs --scale 2            # 2x output (default 1)
//   node tools/screenshots.mjs --dir out --theme light
//   node tools/screenshots.mjs --port 1357 --cdp 9223   # override base ports
//
// Gotchas honored from AGENTS.md:
//   - never --dump-dom/--virtual-time-budget (the mock parks /api/getPinState,
//     so virtual time never advances); use real-timer CDP polling instead
//   - pick free ports (1357/3000 may already be running) and only kill our own
//   - clean up by port/PID, never pkill -f on a vite command line

import { spawn, execSync } from 'node:child_process';
import { mkdirSync, writeFileSync, openSync } from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

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
const BOARD = arg('--board', 'MacroPad');
const OUT = path.resolve(ROOT, arg('--dir', 'screenshots'));
const BASE_PORT = parseInt(arg('--port', '1357'), 10);
const BASE_CDP = parseInt(arg('--cdp', '9223'), 10);
const THEME = arg('--theme', null); // null = keep 'auto' default
const LED_MODE = arg('--led-mode', null); // e.g. 'cycle', 'rain'; default = board's default
const MODAL = arg('--modal', null); // e.g. 'key'
const SCALE = Math.max(1, parseInt(arg('--scale', '1'), 10) || 1);
mkdirSync(OUT, { recursive: true });

// ---- helpers ------------------------------------------------------------

const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

// A port is only "free" if no process listens on it on ANY address family
// (IPv4 or IPv6). Binding 127.0.0.1 alone is not enough: the user's dev server
// may be on [::1]:1357 while 1357 on IPv4 is free, and fuser -k kills both.
function portInUse(port) {
  try {
    return execSync(`ss -ltn 2>/dev/null | rg -q ':${port}\\s' && echo yes || echo no`)
      .toString().trim() === 'yes';
  } catch {
    return false;
  }
}

function findFreePort(start) {
  return new Promise((resolve) => {
    let port = start;
    for (;;) {
      if (!portInUse(port)) return resolve(port);
      port += 1;
    }
  });
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
    cwd: path.join(ROOT, 'www'),
    env: { ...process.env, ...env },
    stdio: ['ignore', out, out],
    detached: true,
  });
  child.unref();
  return child.pid;
}

// ---- CDP client ---------------------------------------------------------

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
      await sleep(400);
    }
  };
  return { ws, ready, send, evalJs, poll };
}

async function screenshot(cdp, name) {
  const shot = await cdp.send('Page.captureScreenshot', {
    format: 'png',
    captureBeyondViewport: true,
  });
  const file = path.join(OUT, name);
  writeFileSync(file, Buffer.from(shot.data, 'base64'));
  console.log(`  saved ${path.relative(ROOT, file)} (${(Buffer.byteLength(shot.data, 'base64') / 1024).toFixed(0)} KB)`);
}

// ---- targets ------------------------------------------------------------

const LED_MODES = {
  custom: '0',
  cycle: '1',
  reactive: '2',
  bps: '3',
  ripple: '4',
  rain: '5',
  fire: '6',
};

const READY_LAYOUT = `
  (() => {
    if (document.getElementById('loading') && !document.getElementById('loading').hidden) return false;
    const svg = document.querySelector('#board-panel svg');
    if (!svg || !svg.getAttribute('width') || !svg.getAttribute('height')) return false;
    if (!document.querySelectorAll('.pin-action-label').length) return false;
    if (!document.getElementById('led-mode')) return false;
    return document.fonts.ready.then(() => true);
  })()
`;

// Click the real theme button so localStorage + active state stay consistent
// (headless defaults prefers-color-scheme to light, so 'auto' would render
// light; we capture dark by default).
const SET_THEME = (t) => `
  (() => {
    const btn = document.querySelector('.theme-btn[data-theme="${t}"]');
    if (btn) { btn.click(); return true; }
    document.documentElement.setAttribute('data-theme', '${t}');
    return true;
  })()
`;

const SET_LED_MODE = (value) => `
  (() => {
    const el = document.getElementById('led-mode');
    el.value = '${value}';
    el.dispatchEvent(new Event('change'));
    return true;
  })()
`;

// ---- key-modal ----------------------------------------------------------

// (name, data-mode, ready selector). The ready selector returns truthy once
// the tab's widget has actually rendered (not just the group being unhidden).
const KEY_MODAL_TABS = [
  ['keyboard', '1', `(() => {
    const g = document.getElementById('key-modal-group-keyboard');
    return !g.hidden && document.querySelectorAll('#key-modal-keyboard .kb-key').length > 0;
  })()`],
  ['midi', '2', `(() => {
    const g = document.getElementById('key-modal-group-midi');
    return !g.hidden && document.querySelectorAll('#key-modal-midi .midi-key-white').length > 0;
  })()`],
  ['gamepad', '3', `(() => {
    const g = document.getElementById('key-modal-group-gamepad');
    return !g.hidden && document.querySelector('#key-modal-gamepad-widget .cgp-svg')?.children.length > 0;
  })()`],
];

const OPEN_KEY_MODAL = `
  (() => { openKeyModal(0); return true; })()
`;

const CLICK_MODAL_TAB = (mode) => `
  (() => {
    document.querySelector('#key-modal-tabs .modal-tab[data-mode="${mode}"]').click();
    return true;
  })()
`;

// Bounding rect of the rounded .modal element (viewport coords, CSS px).
const MODAL_RECT = `
  (() => {
    const r = document.querySelector('#key-modal .modal').getBoundingClientRect();
    return { x: Math.round(r.x), y: Math.round(r.y), w: Math.round(r.width), h: Math.round(r.height) };
  })()
`;

// Capture just the .modal element (clip = its bounding box) and round the
// corners with an ImageMagick alpha mask matching the CSS border-radius
// (20px * scale). Writes <name>.png with transparent corners.
async function screenshotModal(cdp, rect, name) {
  const shot = await cdp.send('Page.captureScreenshot', {
    format: 'png',
    // deviceScaleFactor (set via Emulation.setDeviceMetricsOverride) already
    // scales the output; clip.scale would double-multiply it.
    clip: { x: rect.x, y: rect.y, width: rect.w, height: rect.h, scale: 1 },
  });
  const raw = path.join(OUT, `.${name}.raw.png`);
  writeFileSync(raw, Buffer.from(shot.data, 'base64'));
  const W = rect.w * SCALE;
  const H = rect.h * SCALE;
  const R = 20 * SCALE;
  execSync(
    `magick "${raw}" \\( -size ${W}x${H} xc:none -fill white ` +
    `-draw "roundrectangle 0,0,${W - 1},${H - 1},${R},${R}" \\) ` +
    `-alpha set -compose DstIn -composite "${path.join(OUT, name)}"`,
  );
  try { execSync(`rm -f "${raw}"`); } catch {}
  const file = path.join(OUT, name);
  console.log(`  saved ${path.relative(ROOT, file)} (${(Buffer.byteLength(shot.data, 'base64') / 1024).toFixed(0)} KB @ ${SCALE}x)`);
}

async function captureKeyModal(cdp) {
  await cdp.evalJs(OPEN_KEY_MODAL);
  await cdp.poll(`!document.getElementById('key-modal').hidden`);
  for (const [name, mode, ready] of KEY_MODAL_TABS) {
    await cdp.evalJs(CLICK_MODAL_TAB(mode));
    await cdp.poll(ready);
    await sleep(250);
    const rect = await cdp.evalJs(MODAL_RECT);
    console.log(`key modal (${name}):`);
    await screenshotModal(cdp, rect, `keymodal-${name}-${BOARD}.png`);
  }
}

// ---- main ---------------------------------------------------------------

async function main() {
  const port = await findFreePort(BASE_PORT);
  const cdpPort = await findFreePort(BASE_CDP);
  console.log(`mock server:  http://localhost:${port}  (board ${BOARD})`);
  console.log(`chromium CDP: http://127.0.0.1:${cdpPort}`);

  const serverPid = spawnDetached(
    'npm', ['run', 'dev', '--', '--port', String(port)],
    { VITE_MP2040_BOARD: BOARD },
    'screenshots-server.log',
  );
  const chromePid = spawnDetached(
    'chromium',
    ['--headless=new', '--no-sandbox', '--disable-gpu',
     `--remote-debugging-port=${cdpPort}`,
     `--user-data-dir=${LOGDIR}/screenshots-profile`,
     '--window-size=1100,900',
     'about:blank'],
    {},
    'screenshots-chromium.log',
  );

  try {
    await waitForHttp(`http://localhost:${port}/`, 'mock server');
    await waitForHttp(`http://127.0.0.1:${cdpPort}/json/list`, 'chromium CDP');

    const list = await (await fetch(`http://127.0.0.1:${cdpPort}/json/list`)).json();
    const page = list.find((t) => t.type === 'page') || list[0];
    const cdp = cdpConnect(page.webSocketDebuggerUrl);
    await cdp.ready;
    await cdp.send('Page.enable');
    await cdp.send('Runtime.enable');
    await cdp.send('Emulation.setDeviceMetricsOverride', {
      width: 1100, height: 900, deviceScaleFactor: SCALE, mobile: false,
    });

    await cdp.send('Page.navigate', { url: `http://localhost:${port}/layout` });
    await cdp.poll(READY_LAYOUT);
    if (THEME) {
      await cdp.evalJs(SET_THEME(THEME));
    } else {
      await cdp.evalJs(SET_THEME('dark'));
    }
    await sleep(300);

    if (MODAL && MODAL !== 'key') {
      throw new Error(`unknown --modal '${MODAL}' (expected: key)`);
    }
    if (MODAL === 'key') {
      await captureKeyModal(cdp);
    } else if (LED_MODE) {
      const value = LED_MODES[LED_MODE];
      if (value === undefined) throw new Error(`unknown --led-mode '${LED_MODE}' (expected one of: ${Object.keys(LED_MODES).join(', ')})`);
      console.log(`layout (LED mode ${LED_MODE}):`);
      await cdp.evalJs(SET_LED_MODE(value));
      await sleep(900); // let the LedSim animate a frame or two
      await screenshot(cdp, `layout-${LED_MODE}-${BOARD}.png`);
    } else {
      console.log('layout (default):');
      await screenshot(cdp, `layout-${BOARD}.png`);
    }

    cdp.ws.close();
    console.log(`done → ${path.relative(ROOT, OUT)}/`);
  } finally {
    killByPort(cdpPort);
    killByPort(port);
    try { execSync(`kill ${chromePid} ${serverPid} >/dev/null 2>&1`); } catch {}
    await sleep(300);
    const leftover = execSync(`ss -ltnp 2>/dev/null | rg ':(${port}|${cdpPort})\\b' || true`).toString();
    if (leftover.trim()) console.error('WARNING: ports still in use:\n' + leftover);
  }
}

main().catch((e) => { console.error('FAIL:', e.message); process.exit(1); });