// MP2040 Configurator mock API server.
//
// Serves the same endpoints the firmware exposes (/api/getOptions,
// /api/setOptions, /api/setLedPreview, /api/getFirmwareVersion,
// /api/resetSettings, /api/reboot) plus /board.svg, using data parsed from
// the selected board's BoardConfig.h. Mounted as a Vite middleware in dev
// (see vite.config.js) or run standalone with `node server/app.js` for the
// mock API on http://localhost:8080.
//
// Board selection: VITE_MP2040_BOARD or MP2040_BOARDCONFIG env (default
// MacroPad) is the initial board; the running server can switch boards at
// runtime via GET/POST /api/board (the configurator's Settings page exposes a
// dropdown in mock mode only). getFirmwareVersion reports `mock: true` so the
// UI can distinguish the mock from a real board. VITE_FAKE_UPDATE (e.g.
// "v9.9.9") makes the mock report an old version plus a fake latest release so
// the welcome page's update card can be tested without GitHub access.

import express from 'express';
import { readFileSync } from 'fs';
import path from 'path';
import { fileURLToPath } from 'url';
import { findBoardConfigDir, listBoardConfigs, parseBoardConfig } from './parseBoardConfig.js';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);
const rootDir = path.resolve(__dirname, '..', '..');

let boardId = (
  process.env.VITE_MP2040_BOARD || process.env.MP2040_BOARDCONFIG || 'MacroPad'
).toLowerCase();

const configDir = findBoardConfigDir(boardId, rootDir);
let board = parseBoardConfig(configDir, rootDir);
// Use the canonical config directory name so /api/board ids match the ids in
// /api/boards (dropdown preselection).
boardId = configDir || boardId;

console.log(`MP2040 mock server → board: ${board?.boardConfigLabel ?? boardId}`);

let store = null;

// Switch the active mock board: re-parse its BoardConfig.h and drop the store
// (the options are board-specific: key count, keycodes, LED layout, matrix).
// Request handlers read `board` at request time, so no other state needs
// rebuilding. Returns false if the id doesn't match a board config.
function loadBoard(id) {
  const dir = findBoardConfigDir(id, rootDir);
  if (!dir) return false;
  boardId = dir;
  board = parseBoardConfig(dir, rootDir);
  store = null;
  return true;
}

// Number of keys the active board can report (rows*cols for matrix boards,
// all bank-0 GPIOs for direct boards).
function keyCount() {
  return Math.max(board?.keyCount ?? 0, 1);
}

// Mock favicon: the same logo.svg as the real board but in a dev color (Nord
// 15) so a mock/dev browser tab is clearly distinguishable from the firmware's
// red (Nord 11) icon. Inlined as a data URI so no extra request / file is
// needed, and the shared index.html (and firmware) keeps the original color.
const MOCK_FAVICON_COLOR = '#B48EAD';
const mockFaviconSvg = readFileSync(
  path.join(rootDir, 'www', 'icons', 'logo.svg'), 'utf8'
).replace('#bf616a', MOCK_FAVICON_COLOR);
const mockFaviconHref = `data:image/svg+xml;base64,${Buffer.from(mockFaviconSvg).toString('base64')}`;

// Build a profile object. `src` provides the starting arrays/scalars (e.g. the
// base options) so alternates default to a copy of the base.
function makeProfile(src = {}) {
  const keycodes = [];
  const modifierMasks = [];
  for (let i = 0; i < keyCount(); i++) {
    keycodes.push(src.keycodes?.[i] ?? 0);
    modifierMasks.push(src.modifierMasks?.[i] ?? 0);
  }
  return {
    keycodes,
    modifierMasks,
    midiNotes: Array.isArray(src.midiNotes) ? src.midiNotes.slice() : new Array(keyCount()).fill(0),
    midiVelocities: Array.isArray(src.midiVelocities) ? src.midiVelocities.slice() : new Array(keyCount()).fill(0),
    midi: {
      channel: src.midi?.channel ?? 0,
      velocity: src.midi?.velocity ?? 127,
    },
    led: {
      ledMode: src.led?.ledMode ?? 0,
      ledSpeed: src.led?.ledSpeed ?? 50,
      ledSpeeds: Array.isArray(src.led?.ledSpeeds)
        ? src.led.ledSpeeds.slice()
        : Array(7).fill(src.led?.ledSpeed ?? 50),
      brightnessMaximum: src.led?.brightnessMaximum ?? 255,
      brightnessByMode: Array.isArray(src.led?.brightnessByMode) && src.led.brightnessByMode.length >= 7
        ? src.led.brightnessByMode.slice()
        : Array(7).fill(src.led?.brightnessMaximum ?? 255),
      colorNormal: src.led?.colorNormal ?? 0x00ff00,
      colorPressed: src.led?.colorPressed ?? 0xffffff,
      colorNormalByMode: Array.isArray(src.led?.colorNormalByMode)
        ? src.led.colorNormalByMode.slice()
        : Array(7).fill(src.led?.colorNormal ?? 0x00ff00),
      colorPressedByMode: Array.isArray(src.led?.colorPressedByMode)
        ? src.led.colorPressedByMode.slice()
        : Array(7).fill(src.led?.colorPressed ?? 0xffffff),
      ledNormalColors: Array.isArray(src.led?.ledNormalColors) ? src.led.ledNormalColors.slice() : [],
      ledPressedColors: Array.isArray(src.led?.ledPressedColors) ? src.led.ledPressedColors.slice() : [],
    },
  };
}

function defaultOptions() {
  const keycodes = [];
  const modifierMasks = [];
  const pinLedIndices = [];
  const macroIndices = [];
  for (let i = 0; i < keyCount(); i++) {
    keycodes.push(board?.keycodes?.[i] ?? 0);
    modifierMasks.push(board?.modifierMasks?.[i] ?? 0);
    pinLedIndices.push(board?.pinLedIndices?.[i] ?? -1);
    macroIndices.push(0);
  }
  const base = {
    keycodes,
    modifierMasks,
    midiNotes: new Array(keyCount()).fill(0),
    midiVelocities: new Array(keyCount()).fill(0),
    // Global macros: per-key triggers + the M1-M8 definitions.
    macroIndices,
    macros: Array.from({ length: 8 }, () => ({ steps: [] })),
    defaultInputMode: 1,
    debounceInterval: 5,
    serialConfigEnabled: false,
    midi: {
      channel: 0,
      velocity: 127,
    },
    led: {
      dataPin: board?.led?.dataPin ?? -1,
      ledFormat: board?.led?.ledFormat ?? 0,
      ledsPerKey: board?.led?.ledsPerKey ?? 1,
      ledCount: board?.led?.ledCount ?? 0,
      ledMode: board?.led?.ledMode ?? 0,
      ledSpeed: board?.led?.ledSpeed ?? 50,
      ledSpeeds: Array.isArray(board?.led?.ledSpeeds)
        ? board.led.ledSpeeds.slice()
        : Array(7).fill(board?.led?.ledSpeed ?? 50),
      ledTimeout: board?.led?.ledTimeout ?? 0,
      hasStatusLed: board?.led?.hasStatusLed ?? false,
      statusLedEnabled: board?.led?.statusLedEnabled ?? 1,
      brightnessMaximum: board?.led?.brightnessMaximum ?? 255,
      brightnessByMode: Array.isArray(board?.led?.brightnessByMode) && board.led.brightnessByMode.length >= 7
        ? board.led.brightnessByMode.slice()
        : Array(7).fill(board?.led?.brightnessMaximum ?? 255),
      colorNormal: board?.led?.colorNormal ?? 0x00ff00,
      colorPressed: board?.led?.colorPressed ?? 0xffffff,
      colorNormalByMode: Array.isArray(board?.led?.colorNormalByMode)
        ? board.led.colorNormalByMode.slice()
        : Array(7).fill(board?.led?.colorNormal ?? 0x00ff00),
      colorPressedByMode: Array.isArray(board?.led?.colorPressedByMode)
        ? board.led.colorPressedByMode.slice()
        : Array(7).fill(board?.led?.colorPressed ?? 0xffffff),
      // Empty per-key color arrays = "use the global colors" (legacy config),
      // matching the firmware.
      ledNormalColors: [],
      ledPressedColors: [],
      pinLedIndices,
    },
    webConfigPin: board?.webConfigPin ?? -1,
    matrix: {
      enabled: !!board?.matrix?.enabled,
      rows: board?.matrix?.rows ?? 0,
      cols: board?.matrix?.cols ?? 0,
      rowPins: board?.matrix?.rowPins ?? [],
      colPins: board?.matrix?.colPins ?? [],
      activeHigh: !!board?.matrix?.activeHigh,
    },
  };
  return {
    ...base,
    activeProfile: 0,
    profiles: [0, 1, 2, 3].map(() => makeProfile(base)),
  };
}

export function createMockApp() {
  const app = express();
  app.use(express.json());
  // The firmware sends no ETags; without this Express would answer every
  // repeated GET (e.g. the /api/getPinState long-poll) with a 304 instead
  // of the JSON body.
  app.set('etag', false);

  app.get('/api/getOptions', (req, res) => {
    if (!store) store = defaultOptions();
    res.send(store);
  });

  // Mock-only board switcher. The real board never exposes these endpoints.
  app.get('/api/boards', (req, res) => {
    res.send(listBoardConfigs(rootDir));
  });

  app.get('/api/board', (req, res) => {
    res.send({ board: boardId, boardConfigLabel: board?.boardConfigLabel ?? boardId });
  });

  app.post('/api/board', (req, res) => {
    const id = req.body?.board;
    if (typeof id !== 'string' || !loadBoard(id)) {
      res.status(404).send({ error: `Unknown board '${id}'` });
      return;
    }
    console.log(`MP2040 mock server → board: ${board?.boardConfigLabel ?? boardId}`);
    res.send({ board: boardId, boardConfigLabel: board?.boardConfigLabel ?? boardId });
  });

  app.post('/api/setOptions', (req, res) => {
    const current = store || defaultOptions();
    const body = req.body || {};

    // Which profile is being edited? Defaults to the active profile.
    let profileIndex = Number.isInteger(body.profileIndex)
      ? body.profileIndex
      : current.activeProfile;
    if (!Number.isInteger(profileIndex) || profileIndex < 0 || profileIndex > 3)
      profileIndex = 0;
    const profile = current.profiles[profileIndex] || makeProfile(current);

    if (Array.isArray(body.keycodes)) profile.keycodes = body.keycodes;
    if (Array.isArray(body.modifierMasks)) profile.modifierMasks = body.modifierMasks;
    if (Array.isArray(body.midiNotes)) profile.midiNotes = body.midiNotes;
    if (Array.isArray(body.midiVelocities)) profile.midiVelocities = body.midiVelocities;
    // Global macros (shared across profiles), like the firmware.
    if (Array.isArray(body.macroIndices)) current.macroIndices = body.macroIndices;
    if (Array.isArray(body.macros)) current.macros = body.macros;
    if (body.midi) profile.midi = { ...profile.midi, ...body.midi };
    if (body.led) {
      // ledTimeout / statusLedEnabled are global (non-profile) LED options,
      // like the firmware.
      const { ledTimeout, statusLedEnabled, ...profileLed } = body.led;
      profile.led = { ...profile.led, ...profileLed };
      if (ledTimeout !== undefined) {
        current.led.ledTimeout = Math.max(0, Math.min(600, Number(ledTimeout) || 0));
      }
      if (statusLedEnabled !== undefined) {
        current.led.statusLedEnabled = statusLedEnabled ? 1 : 0;
      }
    }
    if (body.defaultInputMode !== undefined) current.defaultInputMode = body.defaultInputMode;
    if (typeof body.serialConfigEnabled === 'boolean') current.serialConfigEnabled = body.serialConfigEnabled;
    if (Number.isInteger(body.debounceInterval)) {
      current.debounceInterval = Math.max(0, Math.min(100, Number(body.debounceInterval) || 0));
    }
    if (Number.isInteger(body.activeProfile)) {
      current.activeProfile = Math.min(3, Math.max(0, body.activeProfile));
    }
    current.profiles[profileIndex] = profile;

    // Refresh the top-level working copy to mirror the active profile.
    const active = current.profiles[current.activeProfile] || makeProfile(current);
    current.keycodes = active.keycodes;
    current.modifierMasks = active.modifierMasks;
    current.midiNotes = active.midiNotes;
    current.midiVelocities = active.midiVelocities;
    current.midi = { ...current.midi, ...active.midi };
    current.led = { ...current.led, ...active.led };

    store = current;
    res.send(current);
  });

  app.post('/api/setLedPreview', (req, res) => {
    const current = store || defaultOptions();
    const led = req.body?.led || {};
    current.led = { ...current.led, ...led };
    store = current;
    res.send(current);
  });

  app.get('/api/getPinState', (req, res) => {
    // The real board parks the request and only answers when the key state
    // changes (WebConfig::loop()). The mock has no buttons, so it never
    // changes: hold the connection open without responding.
  });

  app.get('/api/getFirmwareVersion', (req, res) => {
    // VITE_FAKE_UPDATE (e.g. "v9.9.9") lets the welcome page's update card be
    // tested in mock mode: the server reports an old released version and a
    // fake "latest" release, so the card shows without a real GitHub release
    // or any network access.
    const fakeUpdate = process.env.VITE_FAKE_UPDATE;
    res.send({
      firmwareVersion: fakeUpdate ? 'v0.1.0' : 'dev',
      gitCommit: fakeUpdate ? 'v0.1.0' : 'mock',
      boardLabel: board?.boardConfigLabel ?? boardId,
      // Lets the configurator show the mock-only board switcher. The real
      // board never returns this field.
      mock: true,
      // Mock-only: the fake latest release to compare against.
      ...(fakeUpdate ? { fakeLatestVersion: fakeUpdate } : {}),
    });
  });

  app.post('/api/resetSettings', (req, res) => res.send({ success: true }));

  app.post('/api/reboot', (req, res) => res.send({ success: true }));

  app.get('/board.svg', (req, res) => {
    if (board?.svgPath) {
      res.type('image/svg+xml').send(readFileSync(board.svgPath));
    } else {
      res.status(404).send('not found');
    }
  });

  // The web UI is one HTML file routed client-side (/, /layout, /settings);
  // mirror the firmware's route mapping so those paths work in dev too. The
  // mock-only Development section (board switcher) is injected here so it never
  // ships in the firmware's embedded index.html.
  const devSection = `
      <section id="mock-board-section">
        <h2><span class="heading-icon icon icon-code" aria-hidden="true"></span>Development</h2>
        <div class="general-form">
          <label for="mock-board">Board</label>
          <span class="icon icon-info field-info" data-tooltip='Switch which board config the mock API serves. Only available in dev mode.'></span>
          <select id="mock-board"></select>
        </div>
      </section>
`;
  app.get(['/', '/layout', '/settings'], (req, res) => {
    let html = readFileSync(path.join(__dirname, '..', 'index.html'), 'utf8');
    // Mock-only Development section (board switcher): injected here so it never
    // ships in the firmware's embedded index.html.
    html = html.replace(
      '<div id="page-settings" class="page" hidden>',
      '<div id="page-settings" class="page" hidden>\n' + devSection
    );
    // Dev-colored favicon (data URI) instead of the firmware's red logo.svg.
    html = html.replace(
      '<link rel="icon" type="image/svg+xml" href="/icons/logo.svg">',
      `<link rel="icon" type="image/svg+xml" href="${mockFaviconHref}">`
    );
    res.type('html').send(html);
  });

  return app;
}

// Run standalone when executed directly.
if (process.argv[1] && path.resolve(process.argv[1]) === __filename) {
  const port = process.env.PORT || 8080;
  createMockApp().listen(port, () => {
    console.log(`MP2040 mock API listening at http://localhost:${port}`);
  });
}
