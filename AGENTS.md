# MP2040 Agent Guide

## Build (firmware)
- **Docker only** — no local ARM toolchain
- `python3 docker-build.py -b <Board>` — board from `configs/` dir names (default: `MacroPad`)
- `-c` clean build, `-v` verbose, `-f` flash to board, `-n` nuke first, `-p <path>` flash mount
- First time (or after Dockerfile changes): `docker build -t gp2040-ce-builder .`
- Output: `build/MP2040_<version>_<sha>_<Board>.uf2`

## Build (web configurator, dev only)
- `cd www && npm install` once, then:
  - `npm run dev` — Vite dev server (http://localhost:3000) with a mock API. It parses `configs/<Board>/BoardConfig.h` and serves `/api/*` + `/board.svg` from `server/app.js`, so no board is needed.
  - `VITE_MP2040_BOARD=<Board> npm run dev` — initial board for the mock (default `MacroPad`; e.g. `2k`, `MegaTouch`). The board can also be switched at runtime from the Settings page's "Development" section (mock mode only), which reloads with the new board config.
  - `VITE_FAKE_UPDATE=<version> npm run dev` — mock reports an old version and shows the welcome page's "Update available" card using `<version>` as the fake latest (e.g. `VITE_FAKE_UPDATE=v9.9.9`); no GitHub access needed.
  - `npm run dev-board` — Vite dev server that proxies `/api` and `/board.svg` to a real board at `VITE_DEV_BASE_URL` (default `http://192.168.7.1`).
- Dev files (`node_modules/`, `server/`, `package.json`, `vite.config.js`) are excluded from the firmware by `tools/makefsdata.py`.

## Codegen (automatic during build)
- Protobuf → C: `compile_proto.cmake` runs nanopb generator on `proto/enums.proto` and `proto/config.proto` (creates `generate_proto` target)
- Web assets → C: `tools/makefsdata.py` (pure Python, no npm) turns `www/` into `lib/httpd/fsdata.c` (creates `generate_fsdata` target)

## Architecture
- `configs/<Board>/BoardConfig.h` — per-pin keycodes (`KEYCODE_GPxx`), modifier masks, LED defaults (`LED_PIN`, `LED_FORMAT`, ...), web-config boot pin (`PIN_WEBCONFIG`)
- `headers/` — all headers, parallel structure to `src/`
- `src/mp2040.cpp` — core 0: debounce pins, publish `Storage.keyState`, drive the keyboard/Net driver, run webconfig loop
- `src/mp2040aux.cpp` — core 1: runs `LedController`
- `src/drivers/keyboard/` — HID keyboard driver (direct pin→keycode, NKRO bitmap report)
- `src/drivers/net/` — RNDIS network driver
- `src/configs/webconfig.cpp` — minimal JSON API (`/api/getOptions`, `/api/setOptions`, ...) served over RNDIS when `PIN_WEBCONFIG` is held at boot
- `src/leds/` + `lib/ws2812/` — minimal PIO WS2812 driver
- `lib/` — vendored libs (tinyusb 0.17.0, nanopb, rndis, httpd, FlashPROM, CRC32, ...). tinyusb is a git submodule pinned to `5217cee`.

## Style conventions
- C++: C++17, C: C11, tabs
- No test framework, no linter for firmware

## Key gotchas
- Two build-order dependencies exist: `httpd` depends on `generate_fsdata` (fs.c `#include`s fsdata.c), and `ws2812` depends on `generate_proto` (Neopixel.h uses enums.pb.h). Both are wired in CMake.
- Modifier keys: set the pin's `MODIFIER_GPxx` mask; the keycode can then be 0. The web UI exposes both.
- The keyboard report is a 256-key bitmap (NKRO), not the 6-key boot report.

## Serial (CDC) command interface
- Enabled per-config via `Config.serialConfigEnabled` (web Settings → "Serial control (CDC)"), off by default; requires reboot (USB descriptors are fixed at enumeration).
- When on, the keyboard/MIDI device also exposes a CDC-ACM port. Both drivers share `SerialCommandHandler` (`headers/drivers/shared/serialhelper.h`), called from each driver's `process()`.
- Protocol: **newline-delimited JSON**. Each line is one command object; responses are JSON. Examples: `{"cmd":"help"}`, `{"cmd":"version"}`, `{"cmd":"profile"}`, `{"cmd":"profile","index":1,"persist":true}`, `{"cmd":"led"}`, `{"cmd":"led","mode":3,"speed":60,"brightness":180,"timeout":120}`.
- Profile switch is live by default; `"persist":true` also saves to flash. LED commands always persist + apply live (via `Storage::buildLedPreviewFromConfig` + `publishLedPreview`).
- Resilience: lines that aren't valid JSON or have an unknown `cmd` are dropped without a response (probing software like NZXT CAM can't latch on). Line buffer is 128 bytes; overlong lines are discarded whole.

## Testing
- **Never kill the user's running mock/Vite dev server** (`npm run dev`, usually on port 3000). To test the web server, start a separate instance on port 1357 and clean up only that one; don't kill existing instances.
- Don't build the firmware unless necessary.

## Web conventions
- Don't inline SVGs, styling, or js.

### Icons
- Icons live in `www/icons/` as single-path SVGs (no explicit `fill`, so black). Served at `/icons/<name>.svg` and embedded into the firmware by `makefsdata.py`.
- To add one: save `<name>.svg` to `www/icons/`, add an `.icon-<name>` rule in `style.css` with both `-webkit-mask-image` and `mask-image` URLs, then use `<span class="icon icon-<name>" aria-hidden="true"></span>` in `index.html`.
- Do **not** use `<img src="/icons/...">`. The `.icon` base class paints the SVG via `mask-image` + `background-color: currentColor`, so the icon inherits its container's text color and works in both themes.
- Icons are invisible without an explicit size: add `width`/`height` in a per-context rule (`.heading-icon`, `.nav-link .icon`, `.theme-btn .icon`, `.led-popover .led-color-btn`, ...) or give the span its own width/height.
- `.logo` and `.macro-step-edit-icon` are the same mask technique and follow the same rules.
