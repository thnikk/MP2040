// Vite dev server for the MP2040 configurator.
//
// - `npm run dev`       → serves the static www/ and mounts the mock API as
//                         middleware (server/app.js), so /api/* and /board.svg
//                         come from the parsed board config.
// - `npm run dev-board` → proxies /api and /board.svg to a real board via
//                         VITE_DEV_BASE_URL (default http://192.168.7.1).
//
// Board selection for the mock: VITE_MP2040_BOARD or MP2040_BOARDCONFIG.

import { defineConfig, loadEnv } from 'vite';
import { createMockApp } from './server/app.js';

export default defineConfig(({ mode }) => {
  const env = loadEnv(mode, process.cwd(), '');
  const baseUrl = env.VITE_DEV_BASE_URL;
  const proxyToBoard = Boolean(baseUrl);

  return {
    root: '.',
    server: {
      port: 3000,
      open: env.VITE_DEV_OPEN !== 'false',
      ...(proxyToBoard
        ? {
            proxy: {
              '/api': { target: baseUrl, changeOrigin: true },
              '/board.svg': { target: baseUrl, changeOrigin: true },
            },
          }
        : {}),
    },
    plugins: proxyToBoard
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
        ],
  };
});
