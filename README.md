# MP2040

Firmare for new RP2040-based keypads. This is a replacement for the old
[Unified 2022](https://github.com/thnikk/unified-2022) firmware, utilizing
the advantages of GP2040 like the RNDIS web server.

A stripped-down firmware for keyboards and macro pads based on
[GP2040-th](https://github.com/thnikk/GP2040-th). It keeps the
parts that are useful for keyboard-style devices and drops everything else.
The intention is to have a more stripped down and stable base
to work with and focus on usability.

- **Keyboard input**: each GPIO maps directly to a USB HID keycode (or
  modifier / multimedia key), sent as a full 256-key NKRO report.
- **Basic LEDs**: a small PIO WS2812 driver lights a per-key LED strip;
  keys glow the "normal" color and brighten to the "pressed" color.
- **RNDIS web config**: hold the web-config pin at boot to expose the device
  as a network device with a tiny web page for remapping keys and LED colors.
- **Board config files**: pin/keycode/LED defaults live in
  `configs/<Board>/BoardConfig.h`; new boards are just a new folder.

Everything else from GP2040-th (console drivers, display, USB host, the React
configurator, the addon system) has been removed.

## Input Modes
MP2040 currently supports two input modes:
- **Keyboard**: Full NKRO HID report
- **MIDI**: Send MIDI notes on a configured channel with global and per-key velocities

## Key Scanning
How the board reads the keys
- **Direct GPIO**: Each key is wired directly to a GPIO pin
- **Matrix**: Keys are wired to grids of rows and columns
- **Capacitive Touch**: Each key is a capacitive touch pad

## Web Config
MP2040 uses a simple raw html+css+js web config. No typescript, react, or bootstap.

![Web Config](assets/web-config.png)

## Build (Docker)

```sh
# build the firmware
python3 docker-build.py -b 2k
```

Output: `build/MP2040_<version>_<sha>_<Board>.uf2`

`docker-build.py` flags: `-b <Board>`, `-c` clean, `-v` verbose, `-f` flash to
board, `-n` nuke first, `-p <path>` flash mount.

## Layout

- `configs/<Board>/BoardConfig.h`: keycode, LED and web-config pin defaults
- `headers/`: all headers, parallel structure to `src/`
- `src/`: core loop (`mp2040.cpp`), LED controller (`leds/`), drivers
- `src/drivers/keyboard/`: HID keyboard driver
- `src/drivers/net/`: RNDIS network driver
- `src/configs/webconfig.cpp`: minimal JSON API for the web page
- `proto/`: nanopb schemas for key mapping + LED options (flash storage)
- `lib/`: vendored libs (tinyusb, nanopb, rndis, httpd, ws2812, ...)
- `www/` + `tools/makefsdata.py`: static config page, embedded into the
  firmware as `lib/httpd/fsdata.c` at build time
