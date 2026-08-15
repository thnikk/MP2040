/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: Copyright (c) 2024 OpenStickCommunity (gp2040-ce.info)
 */

#ifndef PICO_BOARD_CONFIG_H_
#define PICO_BOARD_CONFIG_H_

#include "class/hid/hid.h"

#define BOARD_CONFIG_LABEL "4k Wide"

// Key mapping. Any GPIO with a nonzero keycode is treated as a key that emits
// the given USB HID keycode while pressed (active-low, internal pull-up).
// Pins not listed here default to 0 (unassigned), so only define overrides.
// GP26 is the WS2812 LED data line, not a key.
#define KEYCODE_GP27 HID_KEY_ESCAPE
#define KEYCODE_GP28 HID_KEY_D
#define KEYCODE_GP29 HID_KEY_F
#define KEYCODE_GP02 HID_KEY_J
#define KEYCODE_GP01 HID_KEY_K

// LEDs
#define LED_PIN 26
#define LED_FORMAT LED_FORMAT_GRB
#define LEDS_PER_KEY 1
#define LED_BRIGHTNESS_DEFAULT 50
#define LED_COLOR_NORMAL 0x000000
#define LED_COLOR_PRESSED 0xFFFFFF

// Total number of LEDs in the strip. 0 = derive from the pin mappings / grid.
#define LED_COUNT 4

// Default LED theme mode (0=custom, 1=cycle, 2=reactive, 3=bps, 4=ripple, 5=rain)
#define LED_MODE 1

// Default LED animation speed (0-100 percent, higher = faster; 50 = default)
#define LED_SPEED 80

// LED inactivity timeout (seconds): strip turns off after this long with no
// key held (any press wakes it). 0 = always on.
#define LED_TIMEOUT 60

// Optional per-mode LED defaults. Each mode's normal/pressed colors,
// brightness (0-255), and speed (0-100 percent) override the single defaults
// above; unset modes fall back to LED_COLOR_NORMAL / LED_COLOR_PRESSED /
// LED_BRIGHTNESS_DEFAULT / LED_SPEED. Mode names: CUSTOM, CYCLE, REACTIVE,
// BPS, RIPPLE, RAIN, FIRE.
#define LED_COLOR_NORMAL_MODE_CYCLE 0x00FF00
#define LED_COLOR_PRESSED_MODE_CYCLE 0xFFFFFF
#define LED_SPEED_MODE_CYCLE 60

#define LED_COLOR_NORMAL_MODE_RAIN 0x0044FF
#define LED_COLOR_PRESSED_MODE_RAIN 0xFFFFFF
#define LED_SPEED_MODE_RAIN 70

#define LED_COLOR_NORMAL_MODE_FIRE 0xFF6600
#define LED_COLOR_PRESSED_MODE_FIRE 0xFFAA00
#define LED_SPEED_MODE_FIRE 90

// Pin → LED strip index mapping. -1 = pin has no LED. The LED(s) at the
// mapped index (LEDS_PER_KEY of them) light up when the pin is pressed.
#define LED_INDEX_GP28 0
#define LED_INDEX_GP29 1
#define LED_INDEX_GP02 2
#define LED_INDEX_GP01 3

// Spatial layout of the LED strip for 2D effects (ripple, chase, ...).
// Each entry is the LED strip index at that (row, col); -1 = empty cell.
#define BOARD_LED_POSITION_COLS 4
#define BOARD_LED_POSITIONS \
    { 0, 1, 2, 3 }

// Web config boot key: the first main key. Hold it to ground at boot to
// enter web config mode. The round ESC button (GP27) is an extra button,
// not a config/boot key.
#define PIN_WEBCONFIG 28

// Boot-mode shortcut: the second main key. Hold it to ground at boot to
// enter the USB bootloader. -1 = disabled.
#define PIN_BOOT 29

#endif
