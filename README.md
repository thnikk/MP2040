# MP2040

Firmware for RP2040-based keypads. This is a replacement for the old
[Unified 2022](https://github.com/thnikk/unified-2022) firmware, utilizing
the advantages of [GP2040-th](https://github.com/thnikk/GP2040-th) like
the RNDIS web server and removing unnecessary features.
The intention is to have a more stripped down and stable base
to work with and focus on usability over runtime board configuration.

- **Keyboard input**: each GPIO maps directly to a USB HID keycode (or
  modifier / multimedia key), sent as a full 256-key NKRO report.
- **Basic LEDs**: a small PIO WS2812 driver lights a per-key LED strip.
- **RNDIS web config**: hold the web-config pin at boot to expose the device
  as a network device with a tiny web page for remapping keys and LED colors.
- **Board config files**: pin/keycode/LED defaults live in
  `configs/<Board>/BoardConfig.h`; new boards are just a new folder.

Everything else from GP2040-th (console drivers, display, USB host, the React
configurator, the addon system) has been removed.

## Supported Boards

All boards require an RP2040 microcontroller. Existing boards using the SAMD21 are __NOT__ compatible.

### Mechanical Keypads
<table>
<tr>
<td> 2k </td> <td> 2kw </td> <td> 4kw </td>
</tr>
</table>

### Touch Keypads

<table>
<tr>
<td> MiniTouch </td> <td> MegaTouch </td> <td> 4kMegaTouch </td> <td> BeatBoard </td>
</tr>
</table>

## Flashing

Download the latest `.uf2` for your board from the
[releases page](https://github.com/thnikk/MP2040/releases).

To flash:

1. Hold the **BOOTSEL** button on the keypad (or double-tap reset) and plug it in.
2. The keypad appears as a USB drive named `RPI-RP2`.
3. Drag the `.uf2` file onto the drive. The keypad reboots with the new firmware.

## Web Config

Hold the first key while powering on to open the web config; hold the second
key to enter the bootloader. The device appears as a network adapter and
serves the configurator at `http://192.168.7.1`, where you can remap keys, set
up macros, and adjust LED colors.

![Web Config](assets/web-config.png)

## Input Modes

MP2040 currently supports two input modes:

- **Keyboard**: Full NKRO HID report
- **MIDI**: Send MIDI notes on a configured channel with global and per-key velocities

## Key Scanning

How the board reads the keys:

- **Direct GPIO**: Each key is wired directly to a GPIO pin
- **Matrix**: Keys are wired to grids of rows and columns
- **Capacitive Touch**: Each key is a capacitive touch pad

## Development

Build the firmware:

```sh
python3 docker-build.py -b 2k
```

Output: `build/MP2040_<version>_<sha>_<Board>.uf2`. `docker-build.py` flags:
`-b <Board>`, `-c` clean, `-v` verbose, `-f` flash to board, `-n` nuke first,
`-p <path>` flash mount.

Run the web config mock server:

```sh
./web-config.py
```

`web-config.py` runs `npm install` if needed, then serves the configurator at
`http://localhost:3000`. Flags: `-b <Board>`, `-u <version>` (fake update),
`-p <port>`, `--dev-board` (proxy to a real board).
