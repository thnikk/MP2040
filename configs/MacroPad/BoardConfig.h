/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: Copyright (c) 2024 OpenStickCommunity (gp2040-ce.info)
 */

#ifndef PICO_BOARD_CONFIG_H_
#define PICO_BOARD_CONFIG_H_

#include "class/hid/hid.h"

#define BOARD_CONFIG_LABEL "MacroPad"

// Matrix input mode. Defining MATRIX_ROWS/COLS scans a key matrix instead of
// reading each key directly: keys live at row/column intersections. Key N =
// (row N/COLS, col N%COLS), so KEYCODE_IDXxx / LED_INDEX_IDXxx below map the
// linear key index to a keycode / LED. Wire every switch column to the column
// pins and every row bus to the row pins. Each switch needs a diode (prevents
// ghosting); the scan polarity below must match their orientation.
//
// 4 rows x 3 cols = 12 keys.
#define MATRIX_ROWS 4
#define MATRIX_COLS 3
#define MATRIX_ROW_PINS { 26, 27, 28, 29 }
#define MATRIX_COL_PINS { 6, 7, 0 }

// Scan polarity. This board's diodes point toward the COLUMN pins, so rows are
// driven HIGH to scan and a pressed column reads HIGH (active-high). Boards
// whose diodes point toward the ROW pins omit this (active-low: rows driven
// low, pressed column reads low).
#define MATRIX_ACTIVE_HIGH 1

// Key mapping by linear matrix key index (row * MATRIX_COLS + col). Unlisted
// indices default to 0 (unassigned), so only define overrides.
#define KEYCODE_IDX00 HID_KEY_1
#define KEYCODE_IDX01 HID_KEY_2
#define KEYCODE_IDX02 HID_KEY_3
#define KEYCODE_IDX03 HID_KEY_4
#define KEYCODE_IDX04 HID_KEY_5
#define KEYCODE_IDX05 HID_KEY_6
#define KEYCODE_IDX06 HID_KEY_7
#define KEYCODE_IDX07 HID_KEY_8
#define KEYCODE_IDX08 HID_KEY_9
#define KEYCODE_IDX09 HID_KEY_0
#define KEYCODE_IDX10 HID_KEY_ENTER
#define KEYCODE_IDX11 HID_KEY_SPACE

// Optional modifier mask overrides by key index (KEYBOARD_MODIFIER_* from
// tinyusb). For a modifier key, set the index's mask here; its keycode can
// then be 0. Unlisted indices default to 0.

// LEDs
#define LED_PIN 1
#define LED_FORMAT LED_FORMAT_GRB
#define LEDS_PER_KEY 1
#define LED_BRIGHTNESS_MAX 255
#define LED_BRIGHTNESS_STEPS 1
#define LED_COLOR_NORMAL 0x00FF00
#define LED_COLOR_PRESSED 0xFFFFFF

// Total number of LEDs in the strip. 0 = derive from the pin mappings / grid.
#define LED_COUNT 12

// Default LED theme mode (0=static, 1=cycle, 2=reactive, 3=bps, 4=ripple)
#define LED_MODE 0

// Default LED animation speed (1-255, higher = faster; 236 = default)
#define LED_SPEED 236

// Key index -> LED strip index mapping. -1 = key has no LED. The LED(s) at
// the mapped index (LEDS_PER_KEY of them) light up when the key is pressed.
#define LED_INDEX_IDX00 0
#define LED_INDEX_IDX01 4
#define LED_INDEX_IDX02 8
#define LED_INDEX_IDX03 1
#define LED_INDEX_IDX04 5
#define LED_INDEX_IDX05 9
#define LED_INDEX_IDX06 2
#define LED_INDEX_IDX07 6
#define LED_INDEX_IDX08 10
#define LED_INDEX_IDX09 3
#define LED_INDEX_IDX10 7
#define LED_INDEX_IDX11 11

// Spatial layout of the LED strip for 2D effects (ripple, chase, ...).
// Each entry is the LED strip index at that (row, col); -1 = empty cell.
// This model is effectively sideways
#define BOARD_LED_POSITION_COLS 4
#define BOARD_LED_POSITIONS \
    { 0, 1, 2, 3 }, \
    { 4, 5, 6, 7 }, \
    { 8, 9, 10, 11 }

// Web config boot key (linear matrix key index). Hold key index 0 (the "1"
// key at row 0, col 0) while powering on to enter web config mode.
#define PIN_WEBCONFIG 0

// Optional boot-mode shortcut key (linear matrix key index). Hold the key at
// boot to enter the USB bootloader instead. -1 = disabled.
#define PIN_BOOT 1

#endif
