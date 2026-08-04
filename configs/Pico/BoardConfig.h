/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: Copyright (c) 2024 OpenStickCommunity (gp2040-ce.info)
 */

#ifndef PICO_BOARD_CONFIG_H_
#define PICO_BOARD_CONFIG_H_

#include "class/hid/hid.h"

#define BOARD_CONFIG_LABEL "Pico"

// Main key mapping. Each GPIO with a nonzero keycode is treated as a key that
// emits the given USB HID keycode while pressed (active-low, internal pull-up).
// 0 = unassigned pin.
#define KEYCODE_GP00 0
#define KEYCODE_GP01 0
#define KEYCODE_GP02 HID_KEY_ARROW_UP
#define KEYCODE_GP03 HID_KEY_ARROW_DOWN
#define KEYCODE_GP04 HID_KEY_ARROW_RIGHT
#define KEYCODE_GP05 HID_KEY_ARROW_LEFT
#define KEYCODE_GP06 HID_KEY_A
#define KEYCODE_GP07 HID_KEY_B
#define KEYCODE_GP08 HID_KEY_X
#define KEYCODE_GP09 HID_KEY_Y
#define KEYCODE_GP10 HID_KEY_C
#define KEYCODE_GP11 HID_KEY_D
#define KEYCODE_GP12 HID_KEY_E
#define KEYCODE_GP13 HID_KEY_F
#define KEYCODE_GP14 0
#define KEYCODE_GP15 0
#define KEYCODE_GP16 HID_KEY_1
#define KEYCODE_GP17 HID_KEY_2
#define KEYCODE_GP18 HID_KEY_3
#define KEYCODE_GP19 HID_KEY_4
#define KEYCODE_GP20 HID_KEY_5
#define KEYCODE_GP21 HID_KEY_6
#define KEYCODE_GP22 HID_KEY_7
#define KEYCODE_GP23 HID_KEY_8
#define KEYCODE_GP24 HID_KEY_9
#define KEYCODE_GP25 HID_KEY_0
#define KEYCODE_GP26 HID_KEY_SPACE
#define KEYCODE_GP27 HID_KEY_ENTER
#define KEYCODE_GP28 HID_KEY_ESCAPE
#define KEYCODE_GP29 0

// Optional modifier mask for each key pin (KEYBOARD_MODIFIER_* from tinyusb).
// For modifier keys, set this mask; the pin keycode can then be 0.
#ifndef MODIFIER_GP00
#define MODIFIER_GP00 0
#endif

// LEDs
#define LED_PIN 28
#define LED_FORMAT LED_FORMAT_GRB
#define LEDS_PER_KEY 1
#define LED_BRIGHTNESS_MAX 255
#define LED_BRIGHTNESS_STEPS 1
#define LED_COLOR_NORMAL 0x00FF00
#define LED_COLOR_PRESSED 0xFFFFFF

// Web config boot pin (hold to ground at boot to enter web config mode)
#define PIN_WEBCONFIG 21

#endif
