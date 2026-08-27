/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: Copyright (c) 2024 OpenStickCommunity (gp2040-ce.info)
 */

#ifndef PICO_BOARD_CONFIG_H_
#define PICO_BOARD_CONFIG_H_

#include "class/hid/hid.h"

#define BOARD_CONFIG_LABEL "BeatBoard"

// Capacitive touch keypad with a 4-pad touch ring.
//
// 6 primary touch pads (GP04, 02, 01, 00, 07, 06) act as buttons 1-6. A ring
// (GP26 up, GP27 right, GP28 down, GP29 left) uses interlocking tines to sense
// finger position and drives the analog stick in gamepad modes, volume/scroll
// in keyboard mode, and pitch bend in MIDI mode (see src/touch/TouchRing.cpp).
// All 10 pads use the ~1M ohm resistor PIO capsense driver.
//
// Keycodes (keyboard mode):
#define KEYCODE_GP04 HID_KEY_F13
#define KEYCODE_GP02 HID_KEY_F14
#define KEYCODE_GP01 HID_KEY_F15
#define KEYCODE_GP00 HID_KEY_F16
#define KEYCODE_GP07 HID_KEY_F17
#define KEYCODE_GP06 HID_KEY_F18

// Gamepad default mapping (used in gamepad input modes)
#define GAMEPAD_GP00 GAMEPAD_PIN_MASK_B1   // button 1 -> B1
#define GAMEPAD_GP07 GAMEPAD_PIN_MASK_B2   // button 6 -> R1
#define GAMEPAD_GP06 GAMEPAD_PIN_MASK_L1   // button 5 -> L1
#define GAMEPAD_GP04 GAMEPAD_PIN_MASK_B3   // button 4 -> B4
#define GAMEPAD_GP02 GAMEPAD_PIN_MASK_B4   // button 3 -> B3
#define GAMEPAD_GP01 GAMEPAD_PIN_MASK_R1   // button 2 -> B2

#define DEFAULT_INPUT_MODE INPUT_MODE_XINPUT

// Capacitive touch pads: hand these pins to the touch driver instead of
// treating them as plain buttons.
#define TOUCH_GP00 1
#define TOUCH_GP01 1
#define TOUCH_GP02 1
#define TOUCH_GP04 1
#define TOUCH_GP06 1
#define TOUCH_GP07 1
#define TOUCH_GP26 1
#define TOUCH_GP27 1
#define TOUCH_GP28 1
#define TOUCH_GP29 1

// Touch ring pins, in order of their angle around the ring. The TouchRing
// mapping places pad at 0° = right, 90° = up, 180° = left, 270° = down, so:
//   RING_PAD0 (right, 0°)   = GP27
//   RING_PAD1 (up,   90°)   = GP26
//   RING_PAD2 (left, 180°)  = GP29
//   RING_PAD3 (down, 270°)  = GP28
#define RING_PAD0 27
#define RING_PAD1 26
#define RING_PAD2 29
#define RING_PAD3 28

// LEDs
#define LED_PIN 3
#define LED_FORMAT LED_FORMAT_GRB
#define LEDS_PER_KEY 1
#define LED_BRIGHTNESS_DEFAULT 50
#define LED_COLOR_NORMAL 0x000000
#define LED_COLOR_PRESSED 0xFFFFFF

// Total number of LEDs in the strip (one per button).
#define LED_COUNT 6

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
#define LED_INDEX_GP00 3
#define LED_INDEX_GP01 2
#define LED_INDEX_GP02 1
#define LED_INDEX_GP04 0
#define LED_INDEX_GP06 5
#define LED_INDEX_GP07 4

// Spatial layout of the LED strip for 2D effects (ripple, chase, ...).
// Each entry is the LED strip index at that (row, col); -1 = empty cell.
// A 1x6 horizontal strip.
#define BOARD_LED_POSITION_COLS 6
#define BOARD_LED_POSITIONS \
    { 0, 1, 2, 3, 4, 5 }

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
// its button.
#define PIN_WEBCONFIG 1

// Optional boot-mode shortcut pin. For a touch pad, touch and hold the pad
// within the boot window to enter the USB bootloader instead. -1 = disabled.
#define PIN_BOOT 2

#endif
