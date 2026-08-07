// MP2040 Configurator mock API server.
//
// Serves the same endpoints the firmware exposes (/api/getOptions,
// /api/setOptions, /api/setLedPreview, /api/getFirmwareVersion,
// /api/resetSettings, /api/reboot) plus /board.svg, using data parsed from
// the selected board's BoardConfig.h. Mounted as a Vite middleware in dev
// (see vite.config.js) or run standalone with `node server/app.js` for the
// mock API on http://localhost:8080.
//
// Board selection: VITE_MP2040_BOARD or MP2040_BOARDCONFIG env (default Pico).

import express from 'express';
import { readFileSync } from 'fs';
import path from 'path';
import { fileURLToPath } from 'url';
import { findBoardConfigDir, parseBoardConfig } from './parseBoardConfig.js';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);
const rootDir = path.resolve(__dirname, '..', '..');

const boardId = (
  process.env.VITE_MP2040_BOARD || process.env.MP2040_BOARDCONFIG || 'Pico'
).toLowerCase();

const configDir = findBoardConfigDir(boardId, rootDir);
const board = parseBoardConfig(configDir, rootDir);

console.log(`MP2040 mock server → board: ${board?.boardConfigLabel ?? boardId}`);

let store = null;

// Build a profile object. `src` provides the starting arrays/scalars (e.g. the
// base options) so alternates default to a copy of the base.
function makeProfile(src = {}) {
  const keycodes = [];
  const modifierMasks = [];
  for (let i = 0; i < 30; i++) {
    keycodes.push(src.keycodes?.[i] ?? 0);
    modifierMasks.push(src.modifierMasks?.[i] ?? 0);
  }
  return {
    keycodes,
    modifierMasks,
    midiNotes: Array.isArray(src.midiNotes) ? src.midiNotes.slice() : new Array(30).fill(0),
    midiVelocities: Array.isArray(src.midiVelocities) ? src.midiVelocities.slice() : new Array(30).fill(0),
    midi: {
      channel: src.midi?.channel ?? 0,
      velocity: src.midi?.velocity ?? 127,
    },
    led: {
      ledMode: src.led?.ledMode ?? 0,
      ledSpeed: src.led?.ledSpeed ?? 50,
      brightnessMaximum: src.led?.brightnessMaximum ?? 255,
      brightnessSteps: src.led?.brightnessSteps ?? 1,
      colorNormal: src.led?.colorNormal ?? 0x00ff00,
      colorPressed: src.led?.colorPressed ?? 0xffffff,
    },
  };
}

function defaultOptions() {
  const keycodes = [];
  const modifierMasks = [];
  const pinLedIndices = [];
  for (let i = 0; i < 30; i++) {
    keycodes.push(board?.keycodes?.[i] ?? 0);
    modifierMasks.push(board?.modifierMasks?.[i] ?? 0);
    pinLedIndices.push(board?.pinLedIndices?.[i] ?? -1);
  }
  const base = {
    keycodes,
    modifierMasks,
    midiNotes: new Array(30).fill(0),
    midiVelocities: new Array(30).fill(0),
    defaultInputMode: 1,
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
      ledTimeout: board?.led?.ledTimeout ?? 0,
      brightnessMaximum: board?.led?.brightnessMaximum ?? 255,
      brightnessSteps: board?.led?.brightnessSteps ?? 1,
      colorNormal: board?.led?.colorNormal ?? 0x00ff00,
      colorPressed: board?.led?.colorPressed ?? 0xffffff,
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
    if (body.midi) profile.midi = { ...profile.midi, ...body.midi };
    if (body.led) {
      // ledTimeout is a global (non-profile) LED option, like the firmware.
      const { ledTimeout, ...profileLed } = body.led;
      profile.led = { ...profile.led, ...profileLed };
      if (ledTimeout !== undefined) {
        current.led.ledTimeout = Math.max(0, Math.min(600, Number(ledTimeout) || 0));
      }
    }
    if (body.defaultInputMode !== undefined) current.defaultInputMode = body.defaultInputMode;
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
    res.send({
      firmwareVersion: 'dev',
      gitCommit: 'mock',
      boardLabel: board?.boardConfigLabel ?? boardId,
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
  // mirror the firmware's route mapping so those paths work in dev too.
  app.get(['/', '/layout', '/settings'], (req, res) => {
    res.type('html').send(readFileSync(path.join(__dirname, '..', 'index.html')));
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
