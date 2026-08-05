/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: Copyright (c) 2024 OpenStickCommunity (gp2040-ce.info)
 */

#ifndef PICO_BOARD_CONFIG_H_
#define PICO_BOARD_CONFIG_H_

#include "class/hid/hid.h"

#define BOARD_CONFIG_LABEL "2k"

// Key mapping. Any GPIO with a nonzero keycode is treated as a key that emits
// the given USB HID keycode while pressed (active-low, internal pull-up).
// Pins not listed here default to 0 (unassigned), so only define overrides.
// GP26 is the WS2812 LED data line, not a key.
#define KEYCODE_GP27 HID_KEY_3
#define KEYCODE_GP28 HID_KEY_1
#define KEYCODE_GP29 HID_KEY_2

// LEDs
#define LED_PIN 26
#define LED_FORMAT LED_FORMAT_GRB
#define LEDS_PER_KEY 1
#define LED_BRIGHTNESS_MAX 255
#define LED_BRIGHTNESS_STEPS 1
#define LED_COLOR_NORMAL 0x00FF00
#define LED_COLOR_PRESSED 0xFFFFFF

// Total number of LEDs in the strip. 0 = derive from the pin mappings / grid.
#define LED_COUNT 2

// Default LED theme mode (0=static, 1=cycle, 2=reactive, 3=bps, 4=ripple)
#define LED_MODE 0

// Default LED animation speed (1-255, higher = faster; 236 = default)
#define LED_SPEED 236

// Pin → LED strip index mapping. -1 = pin has no LED. The LED(s) at the
// mapped index (LEDS_PER_KEY of them) light up when the pin is pressed.
#define LED_INDEX_GP28 0
#define LED_INDEX_GP29 1

// Spatial layout of the LED strip for 2D effects (ripple, chase, ...).
// Each entry is the LED strip index at that (row, col); -1 = empty cell.
#define BOARD_LED_POSITION_COLS 2
#define BOARD_LED_POSITIONS \
    { 0, 1 }

// Web config boot pin (hold to ground at boot to enter web config mode).
// GP27 is button 3, so holding button 3 while powering on opens the web config.
#define PIN_WEBCONFIG 28

// Optional boot-mode shortcut pin (hold to ground at boot to enter the USB
// bootloader). -1 = disabled. Use a spare button GPIO not used as a key.
#define PIN_BOOT 29

#endif
