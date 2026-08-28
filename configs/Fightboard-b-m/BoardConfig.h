/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: Copyright (c) 2024 OpenStickCommunity (gp2040-ce.info)
 */

#ifndef PICO_BOARD_CONFIG_H_
#define PICO_BOARD_CONFIG_H_

#include "class/hid/hid.h"
#include "gamepadmapping.h"

// Fightboard-style controller (mirrored wiring, no per-key LEDs / no display):
// 17 buttons (dpad + 13 action) with only an onboard RGB mode indicator.
// Defaults to XInput.
#define BOARD_CONFIG_LABEL "Fightboard-b-m"

// Key mapping. Any GPIO with a nonzero keycode is treated as a key that emits
// the given USB HID keycode while pressed (active-low, internal pull-up).
#define KEYCODE_GP00 HID_KEY_ARROW_UP
#define KEYCODE_GP02 HID_KEY_ARROW_DOWN
#define KEYCODE_GP03 HID_KEY_ARROW_RIGHT
#define KEYCODE_GP01 HID_KEY_ARROW_LEFT
#define KEYCODE_GP09 HID_KEY_SHIFT_LEFT
#define KEYCODE_GP12 HID_KEY_Z
#define KEYCODE_GP13 HID_KEY_X
#define KEYCODE_GP14 HID_KEY_V
#define KEYCODE_GP15 HID_KEY_CONTROL_LEFT
#define KEYCODE_GP28 HID_KEY_ALT_LEFT
#define KEYCODE_GP27 HID_KEY_SPACE
#define KEYCODE_GP26 HID_KEY_C
#define KEYCODE_GP07 HID_KEY_5
#define KEYCODE_GP05 HID_KEY_1
#define KEYCODE_GP08 HID_KEY_EQUAL
#define KEYCODE_GP04 HID_KEY_MINUS
#define KEYCODE_GP06 HID_KEY_9

// Gamepad control mapping for the gamepad input modes (XInput / Switch Pro).
#define GAMEPAD_GP00 GAMEPAD_PIN_MASK_UP
#define GAMEPAD_GP02 GAMEPAD_PIN_MASK_DOWN
#define GAMEPAD_GP03 GAMEPAD_PIN_MASK_RIGHT
#define GAMEPAD_GP01 GAMEPAD_PIN_MASK_LEFT
#define GAMEPAD_GP15 GAMEPAD_PIN_MASK_B3
#define GAMEPAD_GP28 GAMEPAD_PIN_MASK_B4
#define GAMEPAD_GP27 GAMEPAD_PIN_MASK_R1
#define GAMEPAD_GP26 GAMEPAD_PIN_MASK_L1
#define GAMEPAD_GP09 GAMEPAD_PIN_MASK_B1
#define GAMEPAD_GP12 GAMEPAD_PIN_MASK_B2
#define GAMEPAD_GP13 GAMEPAD_PIN_MASK_R2
#define GAMEPAD_GP14 GAMEPAD_PIN_MASK_L2
#define GAMEPAD_GP07 GAMEPAD_PIN_MASK_S1
#define GAMEPAD_GP05 GAMEPAD_PIN_MASK_S2
#define GAMEPAD_GP08 GAMEPAD_PIN_MASK_L3
#define GAMEPAD_GP04 GAMEPAD_PIN_MASK_R3
#define GAMEPAD_GP06 GAMEPAD_PIN_MASK_A1

// Mode indicator LED: the onboard WS2812 (GPIO16) showing the active input
// mode. This chip is wired RGB (not GRB like the per-key chain on other
// Fightboard variants).
#define STATUS_LED_PIN 16
#define STATUS_LED_FORMAT LED_FORMAT_RGB
#define STATUS_LED_BRIGHTNESS_DEFAULT 16

// Web config boot key (S2 / Start) and bootloader shortcut (S1).
#define PIN_WEBCONFIG 5
#define PIN_BOOT 7

// Default input mode for a fight stick.
#define DEFAULT_INPUT_MODE INPUT_MODE_XINPUT

#endif
