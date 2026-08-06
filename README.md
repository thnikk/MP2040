# MP2040

A stripped-down firmware for keyboards and macro pads based on
[GP2040-th](https://github.com/thnikk/GP2040-th). It keeps the
parts that are useful for keyboard-style devices and drops everything else.
The intention is to have a more stripped down and stable base
to work with and focus on usability.

- **Keyboard input** — each GPIO maps directly to a USB HID keycode (or
  modifier / multimedia key), sent as a full 256-key NKRO report.
- **Basic LEDs** — a small PIO WS2812 driver lights a per-key LED strip;
  keys glow the "normal" color and brighten to the "pressed" color.
- **RNDIS web config** — hold the web-config pin at boot to expose the device
  as a network device with a tiny web page for remapping keys and LED colors.
- **Board config files** — pin/keycode/LED defaults live in
  `configs/<Board>/BoardConfig.h`; new boards are just a new folder.

Everything else from GP2040-th (console drivers, display, USB host, the React
configurator, the addon system) has been removed.

## Build (Docker)

```sh
# build the Docker image (once)
docker build -t gp2040-ce-builder .

# build the firmware
python3 docker-build.py -b Pico
```

Output: `build/MP2040_<version>_<sha>_<Board>.uf2`

`docker-build.py` flags: `-b <Board>`, `-c` clean, `-v` verbose, `-f` flash to
board, `-n` nuke first, `-p <path>` flash mount.

## Layout

- `configs/<Board>/BoardConfig.h` — keycode, LED and web-config pin defaults
- `headers/` — all headers, parallel structure to `src/`
- `src/` — core loop (`mp2040.cpp`), LED controller (`leds/`), drivers
- `src/drivers/keyboard/` — HID keyboard driver
- `src/drivers/net/` — RNDIS network driver
- `src/configs/webconfig.cpp` — minimal JSON API for the web page
- `proto/` — nanopb schemas for key mapping + LED options (flash storage)
- `lib/` — vendored libs (tinyusb, nanopb, rndis, httpd, ws2812, ...)
- `www/` + `tools/makefsdata.py` — static config page, embedded into the
  firmware as `lib/httpd/fsdata.c` at build time

## Web config

Hold `PIN_WEBCONFIG` to ground while powering on. On touch boards the
web-config pin is a touch pad: the keyboard doesn't start for 3 seconds after
power-on, and touching the pad within that window enters web config instead.
The device enumerates as a RNDIS network device; the config page and API are
served over its network interface (defaults to 192.168.7.1 as in GP2040). The
page lets you assign a keycode + modifier to each of the 30 GPIO pins and
adjust LED settings.
