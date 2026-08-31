// Vite dev server for the MP2040 configurator.
//
// - `npm run dev`       → serves the static www/ and mounts the mock API as
//                         middleware (server/app.js), so /api/* and /board.svg
//                         come from the parsed board config.
// - `npm run dev-board` → proxies /api and /board.svg to a real board via
//                         VITE_DEV_BASE_URL (default http://192.168.7.1).
//
// Board selection for the mock: VITE_MP2040_BOARD or MP2040_BOARDCONFIG.

import { readFileSync } from 'fs';
import path from 'node:path';
import { defineConfig, loadEnv } from 'vite';
import { createMockApp, transformDevHtml } from './server/app.js';

// The configurator loads its JS via classic <script src> tags (each file
// defines globals the next one uses), which Vite serves as plain files. Those
// aren't in Vite's module graph, so edits never trigger a reload. Watch the
// exact scripts referenced from index.html and send a full-reload ourselves;
// CSS files are left alone (Vite hot-swaps those in place).
function classicScriptReload() {
  const htmlPath = path.join(process.cwd(), 'index.html');
  const scripts = new Set(
    [...readFileSync(htmlPath, 'utf8').matchAll(/<script\s+src="([^"]+)"/g)]
      .map((m) => m[1])
      .filter((src) => !src.startsWith('/@vite'))
  );
  return {
    name: 'classic-script-reload',
    configureServer(server) {
      server.watcher.on('change', (file) => {
        if (!file.endsWith('.js')) return;
        const url = '/' + path.relative(server.config.root, file);
        if (scripts.has(url)) server.ws.send({ type: 'full-reload', path: url });
      });
    },
  };
}

export default defineConfig(({ mode }) => {
  const env = loadEnv(mode, process.cwd(), '');
  const baseUrl = env.VITE_DEV_BASE_URL;
  const proxyToBoard = Boolean(baseUrl);

  return {
    root: '.',
    server: {
      port: 3000,
      open: false,
      ...(proxyToBoard
        ? {
            proxy: {
              // timeout/proxyTimeout 0: the board long-polls /api/getPinState
              // (parks the request until a button changes), so don't let the
              // proxy's default 120s socket timeout kill it.
              '/api': { target: baseUrl, changeOrigin: true, timeout: 0, proxyTimeout: 0 },
              '/board.svg': { target: baseUrl, changeOrigin: true },
            },
          }
        : {}),
    },
    plugins: [
      classicScriptReload(),
      ...(proxyToBoard
        ? []
        : [
            {
              name: 'mock-api',
              configureServer(server) {
                // Mount the Express mock at the root; it only handles /api and
                // /board.svg and lets everything else fall through to Vite.
                server.middlewares.use(createMockApp());
              },
            },
            {
              // Mock-only HTML customizations (Development section + dev-colored
              // favicon). Applied as a Vite transform so index.html still goes
              // through Vite's pipeline (which injects the HMR client), giving
              // live CSS updates in the browser while editing.
              name: 'mock-dev-html',
              transformIndexHtml(html) {
                return transformDevHtml(html);
              },
            },
          ]),
    ],
  };
});
