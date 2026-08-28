/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: Copyright (c) 2024 OpenStickCommunity (gp2040-ce.info)
 */

#ifndef PICO_BOARD_CONFIG_H_
#define PICO_BOARD_CONFIG_H_

#include "class/hid/hid.h"
#include "gamepadmapping.h"

// Springboard-style controller: 17 buttons (dpad + 13 action), per-key WS2812
// LEDs, an onboard RGB mode indicator, and an SSD1306/SH1106 OLED display on
// I2C1 (SDA 10 / SCL 11). Defaults to XInput.
#define BOARD_CONFIG_LABEL "Springboard"

// Key mapping. Any GPIO with a nonzero keycode is treated as a key that emits
// the given USB HID keycode while pressed (active-low, internal pull-up).
#define KEYCODE_GP28 HID_KEY_ARROW_UP
#define KEYCODE_GP15 HID_KEY_ARROW_DOWN
#define KEYCODE_GP26 HID_KEY_ARROW_RIGHT
#define KEYCODE_GP14 HID_KEY_ARROW_LEFT
#define KEYCODE_GP06 HID_KEY_SHIFT_LEFT
#define KEYCODE_GP07 HID_KEY_Z
#define KEYCODE_GP04 HID_KEY_X
#define KEYCODE_GP27 HID_KEY_V
#define KEYCODE_GP05 HID_KEY_CONTROL_LEFT
#define KEYCODE_GP03 HID_KEY_ALT_LEFT
#define KEYCODE_GP02 HID_KEY_SPACE
#define KEYCODE_GP29 HID_KEY_C
#define KEYCODE_GP13 HID_KEY_5
#define KEYCODE_GP08 HID_KEY_1
#define KEYCODE_GP12 HID_KEY_EQUAL
#define KEYCODE_GP09 HID_KEY_MINUS
#define KEYCODE_GP01 HID_KEY_9

// Gamepad control mapping for the gamepad input modes (XInput / Switch Pro).
#define GAMEPAD_GP28 GAMEPAD_PIN_MASK_UP
#define GAMEPAD_GP15 GAMEPAD_PIN_MASK_DOWN
#define GAMEPAD_GP14 GAMEPAD_PIN_MASK_LEFT
#define GAMEPAD_GP26 GAMEPAD_PIN_MASK_RIGHT
#define GAMEPAD_GP05 GAMEPAD_PIN_MASK_B3
#define GAMEPAD_GP03 GAMEPAD_PIN_MASK_B4
#define GAMEPAD_GP02 GAMEPAD_PIN_MASK_R1
#define GAMEPAD_GP29 GAMEPAD_PIN_MASK_L1
#define GAMEPAD_GP06 GAMEPAD_PIN_MASK_B1
#define GAMEPAD_GP07 GAMEPAD_PIN_MASK_B2
#define GAMEPAD_GP04 GAMEPAD_PIN_MASK_R2
#define GAMEPAD_GP27 GAMEPAD_PIN_MASK_L2
#define GAMEPAD_GP13 GAMEPAD_PIN_MASK_S1
#define GAMEPAD_GP08 GAMEPAD_PIN_MASK_S2
#define GAMEPAD_GP12 GAMEPAD_PIN_MASK_L3
#define GAMEPAD_GP09 GAMEPAD_PIN_MASK_R3
#define GAMEPAD_GP01 GAMEPAD_PIN_MASK_A1

// Per-key LEDs on GPIO0 (GRB), one LED per key, 12 total.
#define LED_PIN 0
#define LED_FORMAT LED_FORMAT_GRB
#define LEDS_PER_KEY 1
#define LED_COUNT 12

// Pin → LED strip index mapping. -1 = pin has no LED.
#define LED_INDEX_GP02 0
#define LED_INDEX_GP03 1
#define LED_INDEX_GP04 2
#define LED_INDEX_GP05 5
#define LED_INDEX_GP06 4
#define LED_INDEX_GP07 3
#define LED_INDEX_GP14 8
#define LED_INDEX_GP15 7
#define LED_INDEX_GP26 6
#define LED_INDEX_GP27 9
#define LED_INDEX_GP28 10
#define LED_INDEX_GP29 11

// Default LED theme mode (0=custom, 1=cycle, 2=reactive, 3=bps, 4=ripple, 5=rain)
#define LED_MODE 0

// Default LED animation speed (0-100 percent, higher = faster; 50 = default)
#define LED_SPEED 80

// LED inactivity timeout (seconds): strip turns off after this long with no
// key held (any press wakes it). 0 = always on.
#define LED_TIMEOUT 60
#define LED_BRIGHTNESS_DEFAULT 20

// Spatial layout of the LED strip for 2D effects (ripple, chase, ...).
// Each entry is the LED strip index at that (row, col); -1 = empty cell.
#define BOARD_LED_POSITION_COLS 6
#define BOARD_LED_POSITIONS \
    {  9, 10, 11,  0,  1,  2 }, \
    {  8,  7,  6,  5,  4,  3 }

// Optional per-key colors for Custom mode (LED_MODE 0). Each key's normal
// (unpressed) color overrides LED_COLOR_NORMAL_MODE_CUSTOM for that key.
// Indexed by the button's GPIO pin (direct-pin board), like KEYCODE_GPxx.
// 0 = unset (key uses the mode colors).
#define LED_COLOR_NORMAL_GP06 0x00FF00 // B1 green
#define LED_COLOR_NORMAL_GP07 0xFF0000 // B2 red
#define LED_COLOR_NORMAL_GP05 0x0000FF // B3 blue
#define LED_COLOR_NORMAL_GP03 0xFFFF00 // B4 yellow
#define LED_COLOR_NORMAL_GP02 0xFF8800 // R1 orange
#define LED_COLOR_NORMAL_GP04 0xFF0088 // R2 pink
#define LED_COLOR_NORMAL_GP29 0x8800FF // L1 purple
#define LED_COLOR_NORMAL_GP27 0x00FFFF // L2 aqua
#define LED_COLOR_NORMAL_GP28 0xFFFFFF // Up
#define LED_COLOR_NORMAL_GP14 0xFFFFFF // Left
#define LED_COLOR_NORMAL_GP15 0xFFFFFF // Down
#define LED_COLOR_NORMAL_GP26 0xFFFFFF // Right

// Custom mode pressed color: black for every key (no per-key overrides).
#define LED_COLOR_PRESSED_MODE_CUSTOM 0x000000

// Mode indicator LED: the onboard WS2812 (GPIO16) showing the active input
// mode. This chip is wired RGB (not GRB like the per-key chain).
#define STATUS_LED_PIN 16
#define STATUS_LED_FORMAT LED_FORMAT_RGB
#define STATUS_LED_BRIGHTNESS_DEFAULT 16

// Web config boot key (S2 / Start) and bootloader shortcut (S1).
#define PIN_WEBCONFIG 8
#define PIN_BOOT 13

// Default input mode for a fight stick.
#define DEFAULT_INPUT_MODE INPUT_MODE_XINPUT

// ---- On-screen display (SSD1306/SH1106, 128x64, I2C1 SDA 10 / SCL 11) ----
#define HAS_I2C_DISPLAY 1
#define DISPLAY_I2C_BLOCK 1
#define DISPLAY_I2C_SDA_PIN 10
#define DISPLAY_I2C_SCL_PIN 11
#define DISPLAY_SIZE 3
#define DISPLAY_SAVER_TIMEOUT 60  // seconds, 0 = never
#define DISPLAY_SAVER_MODE 5 // DISPLAY_SAVER_STARS
#define SPLASH_MODE 0        // SPLASH_MODE_STATIC (board-fixed)
#define SPLASH_DURATION 3    // seconds
#define DISPLAY_INPUT_HISTORY 1
#define INPUT_HISTORY_TIMEOUT 3
#define DISPLAY_BUTTON_LAYOUT 5 // BUTTON_LAYOUT_BOARD_DEFINED

// Mini-menu key combo: B1(6) + R3(9) + L3(12), all held ~500ms.
#define DISPLAY_MENU_COMBO 6, 9, 12
// Menu navigation pins: the dpad keys + B1/B2.
#define DISPLAY_MENU_UP_PIN 28
#define DISPLAY_MENU_DOWN_PIN 15
#define DISPLAY_MENU_LEFT_PIN 14
#define DISPLAY_MENU_RIGHT_PIN 26
#define DISPLAY_MENU_SELECT_PIN 6
#define DISPLAY_MENU_BACK_PIN 7

// Single-panel display layout (128x64). Ported from GP2040-th's Springboard
// BoardConfig (DEFAULT_BOARD_LAYOUT_A dpad + DEFAULT_BOARD_LAYOUT_B action
// buttons). GP2040-th renders these as a split layout through a vertically
// compressed viewport (input history on), scaling every coordinate by 0.75 and
// translating by (+16, +8) with radii scaled to match; those positions are
// baked in below as literal 1:1 panel coordinates.
#define BOARD_DISPLAY_LAYOUT \
    { \
    /* dpad: GP27 L2 / GP28 up / GP29 L1 (top), GP14 left / GP15 down / GP26 right */ \
    {GP_ELEMENT_PIN_BUTTON, {21, 23, 6, 6, 1, 1, 27, GP_SHAPE_ELLIPSE}}, \
    {GP_ELEMENT_PIN_BUTTON, {35, 25, 6, 6, 1, 1, 28, GP_SHAPE_ELLIPSE}}, \
    {GP_ELEMENT_PIN_BUTTON, {49, 27, 6, 6, 1, 1, 29, GP_SHAPE_ELLIPSE}}, \
    {GP_ELEMENT_PIN_BUTTON, {19, 37, 6, 6, 1, 1, 14, GP_SHAPE_ELLIPSE}}, \
    {GP_ELEMENT_PIN_BUTTON, {33, 39, 6, 6, 1, 1, 15, GP_SHAPE_ELLIPSE}}, \
    {GP_ELEMENT_PIN_BUTTON, {47, 41, 6, 6, 1, 1, 26, GP_SHAPE_ELLIPSE}}, \
    /* utility: L3(12) S1(13) */ \
    {GP_ELEMENT_PIN_BUTTON, {16, 48, 3, 3, 1, 1, 12, GP_SHAPE_ELLIPSE}}, \
    {GP_ELEMENT_PIN_BUTTON, {24, 48, 3, 3, 1, 1, 13, GP_SHAPE_ELLIPSE}}, \
    /* action buttons: A1(1) / R1(2) B4(3) R2(4) / B3(5) B1(6) B2(7) / S2(8) R3(9) */ \
    {GP_ELEMENT_PIN_BUTTON, {73, 17, 3, 3, 1, 1,  1, GP_SHAPE_ELLIPSE}}, \
    {GP_ELEMENT_PIN_BUTTON, {77, 27, 6, 6, 1, 1,  2, GP_SHAPE_ELLIPSE}}, \
    {GP_ELEMENT_PIN_BUTTON, {91, 25, 6, 6, 1, 1,  3, GP_SHAPE_ELLIPSE}}, \
    {GP_ELEMENT_PIN_BUTTON, {105, 23, 6, 6, 1, 1,  4, GP_SHAPE_ELLIPSE}}, \
    {GP_ELEMENT_PIN_BUTTON, {79, 41, 6, 6, 1, 1,  5, GP_SHAPE_ELLIPSE}}, \
    {GP_ELEMENT_PIN_BUTTON, {93, 39, 6, 6, 1, 1,  6, GP_SHAPE_ELLIPSE}}, \
    {GP_ELEMENT_PIN_BUTTON, {107, 37, 6, 6, 1, 1,  7, GP_SHAPE_ELLIPSE}}, \
    {GP_ELEMENT_PIN_BUTTON, {103, 48, 3, 3, 1, 1,  8, GP_SHAPE_ELLIPSE}}, \
    {GP_ELEMENT_PIN_BUTTON, {111, 48, 3, 3, 1, 1,  9, GP_SHAPE_ELLIPSE}} \
    }

#endif
