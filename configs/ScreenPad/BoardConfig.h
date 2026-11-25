/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: Copyright (c) 2026 thnikk
 */

#ifndef PICO_BOARD_CONFIG_H_
#define PICO_BOARD_CONFIG_H_

#include "class/hid/hid.h"

#define BOARD_CONFIG_LABEL "ScreenPad"

// Waveshare RP2350-Touch-LCD-3.5: a 3.5" 320x480 IPS panel (ST7796 over SPI)
// with an FT6336 capacitive touch controller (I2C). There are no physical
// buttons, so the display itself is a 4x3 on-screen keypad.
//
// The grid is modelled as a 3-column x 4-row matrix board. Keys are linear
// indices (row * MATRIX_COLS + col) exactly like the MacroPad; the touch driver
// synthesizes the matrix state from the touch cells. No GPIO is initialized for
// a key (important on RP2350, where GPIO 0-5 are the QSPI flash pins).
#define MATRIX_ROWS 4
#define MATRIX_COLS 3
#define MATRIX_ROW_PINS {}
#define MATRIX_COL_PINS {}

// Marks the matrix as a touchscreen keypad instead of a wired switch matrix.
// The input driver reads FT6336 touch points, and the LED controller renders
// the 12 per-key LED colors on the screen cells.
#define MATRIX_TOUCH_KEYPAD 1

// Key mapping by linear matrix key index (row * MATRIX_COLS + col).
#define KEYCODE_IDX00 HID_KEY_F13
#define KEYCODE_IDX01 HID_KEY_F14
#define KEYCODE_IDX02 HID_KEY_F15
#define KEYCODE_IDX03 HID_KEY_F16
#define KEYCODE_IDX04 HID_KEY_F17
#define KEYCODE_IDX05 HID_KEY_F18
#define KEYCODE_IDX06 HID_KEY_F19
#define KEYCODE_IDX07 HID_KEY_F20
#define KEYCODE_IDX08 HID_KEY_F21
#define KEYCODE_IDX09 HID_KEY_F22
#define KEYCODE_IDX10 HID_KEY_F23
#define KEYCODE_IDX11 HID_KEY_F24

#define DEFAULT_INPUT_MODE INPUT_MODE_KEYBOARD

// LEDs: no physical strip. The 12 "LEDs" are the on-screen buttons, so Custom
// mode's per-key colors are the idle/pressed button fills and the other LED
// effects animate the grid.
#define LED_PIN -1
#define LED_FORMAT LED_FORMAT_GRB
#define LEDS_PER_KEY 1
#define LED_BRIGHTNESS_DEFAULT 255
#define LED_COLOR_NORMAL 0x2B303B
#define LED_COLOR_PRESSED 0xE6EDF3

#define LED_COUNT 12
#define LED_MODE 0
#define LED_SPEED 50
#define LED_TIMEOUT 0

// Key index -> LED index (the screen cell). Row-major, matching the touch grid.
#define LED_INDEX_IDX00 0
#define LED_INDEX_IDX01 1
#define LED_INDEX_IDX02 2
#define LED_INDEX_IDX03 3
#define LED_INDEX_IDX04 4
#define LED_INDEX_IDX05 5
#define LED_INDEX_IDX06 6
#define LED_INDEX_IDX07 7
#define LED_INDEX_IDX08 8
#define LED_INDEX_IDX09 9
#define LED_INDEX_IDX10 10
#define LED_INDEX_IDX11 11

// 2D layout of the virtual strip: the same 4x3 grid as the touch cells. Used by
// the 2D LED effects (ripple) and by the screen renderer.
#define BOARD_LED_POSITION_COLS 3
#define BOARD_LED_POSITIONS \
    { 0, 1, 2 }, \
    { 3, 4, 5 }, \
    { 6, 7, 8 }, \
    { 9, 10, 11 }

// Boot: hold the top-left on-screen key (matrix key 0) during the boot window
// to enter web config mode. The hardware BOOT button still enters the USB
// bootloader.
#define PIN_WEBCONFIG 0
#define PIN_BOOT -1

// ---------------------------------------------------------------------------
// ST7796 LCD (SPI0)
// ---------------------------------------------------------------------------
#define LCD_SPI spi0
#define LCD_PIN_SCK 18
#define LCD_PIN_MOSI 19
#define LCD_PIN_CS 21
#define LCD_PIN_DC 20
#define LCD_PIN_RST 23
#define LCD_PIN_BL 22
#define LCD_SPI_HZ 100000000
#define LCD_WIDTH 320
#define LCD_HEIGHT 480
#define LCD_BACKLIGHT_PERCENT 70

// Status/footer strip at the bottom of the screen (input mode, profile, web
// config address, boot-window hint). Keys stop above it.
#define KEYPAD_FOOTER_HEIGHT 24

// ---------------------------------------------------------------------------
// FT6336 touch controller (I2C1)
// ---------------------------------------------------------------------------
#define TOUCH_I2C i2c1
#define TOUCH_PIN_SDA 34
#define TOUCH_PIN_SCL 35
#define TOUCH_PIN_RST 24
#define TOUCH_PIN_INT 25
#define TOUCH_I2C_HZ 400000

// Coordinate transform from the controller's native portrait coordinates to
// the panel. The vendor example maps touch (x, y) to (319 - x, 479 - y).
#define TOUCH_RAW_MAX_X 319
#define TOUCH_RAW_MAX_Y 479
#define TOUCH_SWAP_XY 0
#define TOUCH_INVERT_X 1
#define TOUCH_INVERT_Y 1

// Extra release margin (display pixels) around a cell so a finger resting on a
// boundary doesn't flicker between two keys.
#define TOUCH_KEYPAD_RELEASE_MARGIN 4

#endif
