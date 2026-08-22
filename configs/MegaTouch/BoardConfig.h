/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: Copyright (c) 2024 OpenStickCommunity (gp2040-ce.info)
 */

#ifndef PICO_BOARD_CONFIG_H_
#define PICO_BOARD_CONFIG_H_

#include "class/hid/hid.h"

#define BOARD_CONFIG_LABEL "MegaTouch"

// Capacitive touch keypad. Each key is a copper pad on a GPIO with a ~1M ohm
// resistor to ground; the PIO measures the pad's discharge time (see
// src/touch/). Marking a pin TOUCH_GPxx hands it to the touch driver instead
// of treating it as a button. GP26/27 share the ADC input path, which only
// raises their baseline capacitance a bit - the boot-time calibration absorbs
// that.
#define KEYCODE_GP26 HID_KEY_Z
#define KEYCODE_GP27 HID_KEY_X
#define KEYCODE_GP01 HID_KEY_ESCAPE
#define KEYCODE_GP02 HID_KEY_GRAVE

#define TOUCH_GP26 1
#define TOUCH_GP27 1
#define TOUCH_GP01 1
#define TOUCH_GP02 1

// Optional per-pin fixed touch thresholds (0 = auto-calibrate at boot). Counts
// are PIO cycles; the idle baseline is a few thousand. Only set these if a pad
// needs manual tuning.
// #define TOUCH_THRESHOLD_GP26 2500
// #define TOUCH_THRESHOLD_GP27 2500

// LEDs
#define LED_PIN 3
#define LED_FORMAT LED_FORMAT_GRB
#define LEDS_PER_KEY 1
#define LED_BRIGHTNESS_DEFAULT 50
#define LED_COLOR_NORMAL 0x000000
#define LED_COLOR_PRESSED 0xFFFFFF

// Total number of LEDs in the strip. 0 = derive from the pin mappings / grid.
#define LED_COUNT 2

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
#define LED_INDEX_GP26 0
#define LED_INDEX_GP27 1

// Spatial layout of the LED strip for 2D effects (ripple, chase, ...).
// Each entry is the LED strip index at that (row, col); -1 = empty cell.
#define BOARD_LED_POSITION_COLS 2
#define BOARD_LED_POSITIONS \
    { 0, 1 }

// Mode indicator LED: a single WS2812 on the PCB showing the active input
// mode. Board-fixed colors (keyboard / MIDI / web config). -1 = none.
#define STATUS_LED_PIN 12
#define STATUS_LED_ENABLE_PIN 11 // power gate; pulled high to enable the LED
// Default state of the mode indicator LED on fresh/reset configs. 1 = on,
// 0 = off. The web config can still toggle it per-save.
#define STATUS_LED_ENABLED_DEFAULT 0

// Web config boot pin. GP01 is a touch pad: the keyboard doesn't start until
// ~3 seconds after power-on (WEB_CONFIG_TOUCH_WINDOW_MS); touch and hold the
// pad briefly within that window to enter web config mode instead of sending
// key 3.
#define PIN_WEBCONFIG 1

// Optional boot-mode shortcut pin. For a touch pad, touch and hold the pad
// within the boot window to enter the USB bootloader instead. -1 = disabled.
#define PIN_BOOT 2

#endif
