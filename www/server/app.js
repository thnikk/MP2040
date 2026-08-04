// MP2040 Configurator mock API server.
//
// Serves the same endpoints the firmware exposes (/api/getOptions,
// /api/setOptions, /api/getFirmwareVersion, /api/resetSettings) plus
// /board.svg, using data parsed from the selected board's BoardConfig.h.
// Mounted as a Vite middleware in dev (see vite.config.js) or run standalone
// with `node server/app.js` for the mock API on http://localhost:8080.
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

function defaultOptions() {
  const keycodes = [];
  const modifierMasks = [];
  const pinLedIndices = [];
  for (let i = 0; i < 30; i++) {
    keycodes.push(board?.keycodes?.[i] ?? 0);
    modifierMasks.push(board?.modifierMasks?.[i] ?? 0);
    pinLedIndices.push(board?.pinLedIndices?.[i] ?? -1);
  }
  return {
    keycodes,
    modifierMasks,
    led: {
      dataPin: board?.led?.dataPin ?? -1,
      ledFormat: board?.led?.ledFormat ?? 0,
      ledsPerKey: board?.led?.ledsPerKey ?? 1,
      ledCount: board?.led?.ledCount ?? 0,
      ledMode: board?.led?.ledMode ?? 0,
      ledSpeed: board?.led?.ledSpeed ?? 236,
      brightnessMaximum: board?.led?.brightnessMaximum ?? 255,
      brightnessSteps: board?.led?.brightnessSteps ?? 1,
      colorNormal: board?.led?.colorNormal ?? 0x00ff00,
      colorPressed: board?.led?.colorPressed ?? 0xffffff,
      pinLedIndices,
    },
    webConfigPin: board?.webConfigPin ?? -1,
  };
}

export function createMockApp() {
  const app = express();
  app.use(express.json());

  app.get('/api/getOptions', (req, res) => {
    if (!store) store = defaultOptions();
    res.send(store);
  });

  app.post('/api/setOptions', (req, res) => {
    const current = store || defaultOptions();
    const body = req.body || {};
    if (Array.isArray(body.keycodes)) current.keycodes = body.keycodes;
    if (Array.isArray(body.modifierMasks)) current.modifierMasks = body.modifierMasks;
    if (body.led) current.led = { ...current.led, ...body.led };
    store = current;
    res.send(current);
  });

  app.get('/api/getFirmwareVersion', (req, res) => {
    res.send({
      firmwareVersion: 'dev',
      gitCommit: 'mock',
      boardLabel: board?.boardConfigLabel ?? boardId,
    });
  });

  app.post('/api/resetSettings', (req, res) => res.send({ success: true }));

  app.get('/board.svg', (req, res) => {
    if (board?.svgPath) {
      res.type('image/svg+xml').send(readFileSync(board.svgPath));
    } else {
      res.status(404).send('not found');
    }
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
