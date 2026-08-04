# MP2040 Agent Guide

## Build (firmware)
- **Docker only** — no local ARM toolchain
- `python3 docker-build.py -b <Board>` — board from `configs/` dir names (default: `Pico`)
- `-c` clean build, `-v` verbose, `-f` flash to board, `-n` nuke first, `-p <path>` flash mount
- First time (or after Dockerfile changes): `docker build -t gp2040-ce-builder .`
- Output: `build/MP2040_<version>_<sha>_<Board>.uf2`

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
