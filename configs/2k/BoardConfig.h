/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: Copyright (c) 2024 OpenStickCommunity (gp2040-ce.info)
 */

#ifndef PICO_BOARD_CONFIG_H_
#define PICO_BOARD_CONFIG_H_

#include "class/hid/hid.h"

#define BOARD_CONFIG_LABEL "2k"

// Main key mapping. Each GPIO with a nonzero keycode is treated as a key that
// emits the given USB HID keycode while pressed (active-low, internal pull-up).
// 0 = unassigned pin.
#define KEYCODE_GP00 0
#define KEYCODE_GP01 0
#define KEYCODE_GP02 0
#define KEYCODE_GP03 0
#define KEYCODE_GP04 0
#define KEYCODE_GP05 0
#define KEYCODE_GP06 0
#define KEYCODE_GP07 0
#define KEYCODE_GP08 0
#define KEYCODE_GP09 0
#define KEYCODE_GP10 0
#define KEYCODE_GP11 0
#define KEYCODE_GP12 0
#define KEYCODE_GP13 0
#define KEYCODE_GP14 0
#define KEYCODE_GP15 0
#define KEYCODE_GP16 0
#define KEYCODE_GP17 0
#define KEYCODE_GP18 0
#define KEYCODE_GP19 0
#define KEYCODE_GP20 0
#define KEYCODE_GP21 0
#define KEYCODE_GP22 0
#define KEYCODE_GP23 0
#define KEYCODE_GP24 0
#define KEYCODE_GP25 0
// GP26 is the WS2812 LED data line, not a key
#define KEYCODE_GP26 0
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

// Total number of LEDs in the strip. 0 = derive from the pin mappings below.
#define LED_COUNT 2

// Pin → LED strip index mapping. -1 = pin has no LED. The LED(s) at the
// mapped index (LEDS_PER_KEY of them) light up when the pin is pressed.
#define LED_INDEX_GP28 0
#define LED_INDEX_GP29 1

// Web config boot pin (hold to ground at boot to enter web config mode).
// GP27 is button 3, so holding button 3 while powering on opens the web config.
#define PIN_WEBCONFIG 27

#endif
