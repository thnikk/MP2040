//
// Host shim for "storagemanager.h" (MP2040 display simulator).
//
// The real Storage manager pulls in FlashPROM, GPIO and the USB config path.
// The display subsystem only reads/writes a small slice of the in-memory
// Config plus the key-state mask, so this shim reimplements that slice over a
// plain in-memory Config seeded from the board's BoardConfig.h. The menu logic
// (staged values, save prompt, spinner clamps) runs unmodified against it.
//
// Kept deliberately close to the real header's behaviour so the simulator
// shows the same values the firmware would for a fresh config.
//
#ifndef _SIM_STORAGEMANAGER_H_
#define _SIM_STORAGEMANAGER_H_

#include <stdint.h>
#include <string.h>

#include "config.pb.h"
#include "keymask.h"
#include "types.h"
#include "BoardConfig.h"
#include "helper.h"   // getMillis()/getMicro() + board defines (as real header does)

// Flash region reserved for the emulated EEPROM (see lib/FlashPROM). Only used
// for the Info page's "EEPROM used/total" line.
#ifndef EEPROM_SIZE_BYTES
#define EEPROM_SIZE_BYTES 0x10000
#endif

#define SI Storage::getInstance()

#define MAX_HOTKEYS 16
#define MAX_HOTKEY_KEYS 8
#define MAX_BOOT_KEYS 8

#define LED_PREVIEW_STATUS_UNSET 0xFFFFFFFF
#define LED_PREVIEW_MIN_UNSET 0xFFFFFFFF
#define LED_PREVIEW_MAX_UNSET 0xFFFFFFFF
#define LED_PREVIEW_STATUS_COLOR_UNSET 0xFFFFFFFF

// Mirror of the firmware's LedPreview (see headers/storagemanager.h).
struct LedPreview
{
    uint32_t ledMode;
    uint32_t ledSpeed[7];
    uint32_t ledSpeedCount;
    uint32_t brightnessByMode[7];
    uint32_t brightnessByModeCount;
    uint32_t colorNormalByMode[7];
    uint32_t colorPressedByMode[7];
    uint32_t colorCount;
    uint32_t ledTimeout;
    uint32_t statusLedEnabled;
    uint32_t statusLedBrightnessMinimum;
    uint32_t statusLedBrightnessMaximum;
    uint32_t statusLedColor;
    uint32_t ledNormalColorCount;
    uint32_t ledNormalColors[MAX_KEYS];
    uint32_t ledPressedColorCount;
    uint32_t ledPressedColors[MAX_KEYS];
};

class Storage {
public:
    Storage(Storage const&) = delete;
    void operator=(Storage const&) = delete;
    static Storage& getInstance() {
        static Storage instance;
        return instance;
    }

    Config& getConfig() { return config; }
    KeyMapping& getKeyMapping() { return config.keyMapping; }
    LEDOptions& getLedOptions() { return config.ledOptions; }
    DisplayOptions& getDisplayOptions() { return config.displayOptions; }
    GamepadMapping& getGamepadMapping() { return config.gamepadMapping; }

    void SetMenuActive(bool active) { menuActive = active; }
    bool GetMenuActive() { return menuActive; }
    void requestMenuToggle() { menuToggleRequested = true; }
    bool consumeMenuToggle() {
        const bool request = menuToggleRequested;
        menuToggleRequested = false;
        return request;
    }

    InputMode getDefaultInputMode() {
        if (!config.has_defaultInputMode || config.defaultInputMode == INPUT_MODE_CONFIG)
            return INPUT_MODE_KEYBOARD;
        return config.defaultInputMode;
    }
    void setDefaultInputMode(InputMode mode) {
        config.defaultInputMode = mode;
        config.has_defaultInputMode = true;
    }

    SOCDMode getSocdMode() {
        return config.has_socdMode ? config.socdMode : SOCD_MODE_UP_PRIORITY;
    }
    void setSocdMode(SOCDMode mode) {
        config.socdMode = mode;
        config.has_socdMode = true;
    }
    DpadMode getDpadMode() {
        return config.has_dpadMode ? config.dpadMode : DPAD_MODE_DIGITAL;
    }
    void setDpadMode(DpadMode mode) {
        config.dpadMode = mode;
        config.has_dpadMode = true;
    }
    bool getUseNintendoLayout() {
        return config.has_useNintendoLayout ? config.useNintendoLayout : false;
    }
    void setUseNintendoLayout(bool enabled) {
        config.useNintendoLayout = enabled;
        config.has_useNintendoLayout = true;
    }

    uint32_t getGamepadMask(uint32_t pin) {
        return config.has_gamepadMapping && pin < config.gamepadMapping.masks_count
            ? config.gamepadMapping.masks[pin] : 0;
    }
    void setGamepadMappingCount(pb_size_t count) {
        config.gamepadMapping.masks_count = count > MAX_KEYS ? MAX_KEYS : count;
        config.has_gamepadMapping = true;
    }

    uint32_t getActiveProfile() {
        return config.has_activeProfile ? config.activeProfile : 0;
    }
    void setActiveProfile(uint32_t profile) {
        config.activeProfile = profile;
        config.has_activeProfile = true;
    }
    Profile* getProfile(uint32_t index) {
        return (index < config.profiles_count) ? &config.profiles[index] : nullptr;
    }
    pb_size_t getProfileCount() { return config.profiles_count; }

    uint32_t getEepromUsedBytes() { return 0; }

    bool isMatrixMode() { return matrixRows > 0 && matrixCols > 0; }
    uint32_t getKeyCount() {
        return isMatrixMode() ? (uint32_t)matrixRows * matrixCols : (uint32_t)NUM_BANK0_GPIOS;
    }

    void SetConfigMode(bool v) { CONFIG_MODE = v; }
    bool GetConfigMode() { return CONFIG_MODE; }

    void applyActiveProfile() {
        if (getProfile(getActiveProfile()) != nullptr)
            config.keyMapping = config.profiles[getActiveProfile()].keyMapping;
    }
    bool save(bool force = false) {
        (void)force;
        return true;
    }

    // core0 -> core1 sharing; single-threaded on the host, so plain access.
    void publishKeyState(const KeyMask& s) { keyState = s; }
    KeyMask getKeyState() { return keyState; }
    KeyMask keyState;

    void publishLedPreview(const LedPreview& p) { ledPreview = p; }
    bool consumeLedPreview(LedPreview& out) { out = ledPreview; return false; }
    void buildLedPreviewFromConfig(LedPreview& preview) {
        memset(&preview, 0, sizeof(preview));
        preview.ledMode = config.ledOptions.ledMode;
        preview.ledSpeedCount = 7;
        preview.brightnessByModeCount = 7;
        preview.colorCount = 7;
        for (int i = 0; i < 7; i++) {
            preview.ledSpeed[i] = config.ledOptions.ledSpeeds[i];
            preview.brightnessByMode[i] = config.ledOptions.brightnessByMode[i];
            preview.colorNormalByMode[i] = config.ledOptions.colorNormalByMode[i];
            preview.colorPressedByMode[i] = config.ledOptions.colorPressedByMode[i];
        }
        preview.statusLedColor = LED_PREVIEW_STATUS_COLOR_UNSET;
    }

    KeyMask hotkeySuppressed;

private:
    Storage() : keyState(), config(), ledPreview() { seed(); }

    void seed() {
        // ---- board default fallbacks (same as src/storagemanager.cpp) ----
#ifndef HAS_I2C_DISPLAY
#define HAS_I2C_DISPLAY 0
#endif
#ifndef DISPLAY_I2C_BLOCK
#define DISPLAY_I2C_BLOCK 0
#endif
#ifndef DISPLAY_I2C_ADDR
#define DISPLAY_I2C_ADDR 0x3C
#endif
#ifndef DISPLAY_I2C_SDA_PIN
#define DISPLAY_I2C_SDA_PIN -1
#endif
#ifndef DISPLAY_I2C_SCL_PIN
#define DISPLAY_I2C_SCL_PIN -1
#endif
#ifndef DISPLAY_SIZE
#define DISPLAY_SIZE 3
#endif
#ifndef DISPLAY_FLIP
#define DISPLAY_FLIP 0
#endif
#ifndef DISPLAY_INVERT
#define DISPLAY_INVERT 0
#endif
#ifndef DISPLAY_BUTTON_LAYOUT
#define DISPLAY_BUTTON_LAYOUT 5
#endif
#ifndef DISPLAY_ORIENTATION
#define DISPLAY_ORIENTATION 0
#endif
#ifndef DISPLAY_SAVER_TIMEOUT
#define DISPLAY_SAVER_TIMEOUT 0
#endif
#ifndef DISPLAY_SAVER_MODE
#define DISPLAY_SAVER_MODE 5
#endif
#ifndef SPLASH_MODE
#define SPLASH_MODE 0
#endif
#ifndef SPLASH_DURATION
#define SPLASH_DURATION 3
#endif
#ifndef DISPLAY_INPUT_HISTORY
#define DISPLAY_INPUT_HISTORY 1
#endif
#ifndef INPUT_HISTORY_TIMEOUT
#define INPUT_HISTORY_TIMEOUT 3
#endif
#ifndef DISPLAY_MENU_UP_PIN
#define DISPLAY_MENU_UP_PIN -1
#endif
#ifndef DISPLAY_MENU_DOWN_PIN
#define DISPLAY_MENU_DOWN_PIN -1
#endif
#ifndef DISPLAY_MENU_LEFT_PIN
#define DISPLAY_MENU_LEFT_PIN -1
#endif
#ifndef DISPLAY_MENU_RIGHT_PIN
#define DISPLAY_MENU_RIGHT_PIN -1
#endif
#ifndef DISPLAY_MENU_SELECT_PIN
#define DISPLAY_MENU_SELECT_PIN -1
#endif
#ifndef DISPLAY_MENU_BACK_PIN
#define DISPLAY_MENU_BACK_PIN -1
#endif
#ifndef DEFAULT_INPUT_MODE
#define DEFAULT_INPUT_MODE INPUT_MODE_KEYBOARD
#endif
#ifndef LED_MODE
#define LED_MODE 0
#endif
#ifndef LED_SPEED
#define LED_SPEED 50
#endif
#ifndef LED_BRIGHTNESS_DEFAULT
#define LED_BRIGHTNESS_DEFAULT 128
#endif
#ifndef LED_COLOR_NORMAL
#define LED_COLOR_NORMAL 0x000000
#endif
#ifndef LED_COLOR_PRESSED
#define LED_COLOR_PRESSED 0xFFFFFF
#endif

        // ---- display options ----
        DisplayOptions& d = config.displayOptions;
        d.enabled = !!HAS_I2C_DISPLAY;   d.has_enabled = true;
        d.i2cBlock = DISPLAY_I2C_BLOCK;  d.has_i2cBlock = true;
        d.sdaPin = DISPLAY_I2C_SDA_PIN;  d.has_sdaPin = true;
        d.sclPin = DISPLAY_I2C_SCL_PIN;  d.has_sclPin = true;
        d.i2cAddress = DISPLAY_I2C_ADDR; d.has_i2cAddress = true;
        d.size = DISPLAY_SIZE;           d.has_size = true;
        d.flip = DISPLAY_FLIP;           d.has_flip = true;
        d.invert = DISPLAY_INVERT;       d.has_invert = true;
        d.buttonLayout = (ButtonLayout)DISPLAY_BUTTON_LAYOUT; d.has_buttonLayout = true;
        d.orientation = (ButtonLayoutOrientation)DISPLAY_ORIENTATION; d.has_orientation = true;
        d.splashMode = (SplashMode)SPLASH_MODE; d.has_splashMode = true;
        d.splashDuration = SPLASH_DURATION; d.has_splashDuration = true;
        d.displaySaverTimeout = DISPLAY_SAVER_TIMEOUT; d.has_displaySaverTimeout = true;
        d.displaySaverMode = (DisplaySaverMode)DISPLAY_SAVER_MODE; d.has_displaySaverMode = true;
        d.inputHistoryEnabled = DISPLAY_INPUT_HISTORY; d.has_inputHistoryEnabled = true;
        d.inputHistoryTimeout = INPUT_HISTORY_TIMEOUT; d.has_inputHistoryTimeout = true;
        d.menuUpPin = DISPLAY_MENU_UP_PIN;         d.has_menuUpPin = true;
        d.menuDownPin = DISPLAY_MENU_DOWN_PIN;     d.has_menuDownPin = true;
        d.menuLeftPin = DISPLAY_MENU_LEFT_PIN;     d.has_menuLeftPin = true;
        d.menuRightPin = DISPLAY_MENU_RIGHT_PIN;   d.has_menuRightPin = true;
        d.menuSelectPin = DISPLAY_MENU_SELECT_PIN; d.has_menuSelectPin = true;
        d.menuBackPin = DISPLAY_MENU_BACK_PIN;     d.has_menuBackPin = true;
        config.has_displayOptions = true;

        // ---- LED options (per-mode arrays seeded, matching the firmware) ----
        LEDOptions& l = config.ledOptions;
        l.has_dataPin = true; l.dataPin = -1;
        l.has_ledMode = true; l.ledMode = LED_MODE;
        l.has_ledSpeed = true; l.ledSpeed = LED_SPEED;
        l.has_brightnessMaximum = true; l.brightnessMaximum = LED_BRIGHTNESS_DEFAULT;
        l.has_colorNormal = true; l.colorNormal = LED_COLOR_NORMAL;
        l.has_colorPressed = true; l.colorPressed = LED_COLOR_PRESSED;
        l.ledSpeeds_count = 7;
        l.brightnessByMode_count = 7;
        l.colorNormalByMode_count = 7;
        l.colorPressedByMode_count = 7;
        for (int i = 0; i < 7; i++) {
            l.ledSpeeds[i] = LED_SPEED;
            l.brightnessByMode[i] = LED_BRIGHTNESS_DEFAULT;
            l.colorNormalByMode[i] = LED_COLOR_NORMAL;
            l.colorPressedByMode[i] = LED_COLOR_PRESSED;
        }
        config.has_ledOptions = true;

        // ---- key + gamepad mapping sizes ----
#ifdef MATRIX_ROWS
        matrixRows = MATRIX_ROWS;
#endif
#ifdef MATRIX_COLS
        matrixCols = MATRIX_COLS;
#endif
        const uint32_t keys = getKeyCount();
        config.keyMapping.keycodes_count = (pb_size_t)keys;
        config.keyMapping.modifierMasks_count = (pb_size_t)keys;
        config.keyMapping.midiNotes_count = (pb_size_t)keys;
        config.keyMapping.midiVelocities_count = (pb_size_t)keys;
        config.has_keyMapping = true;
        config.gamepadMapping.masks_count = (pb_size_t)keys;
        config.has_gamepadMapping = true;

        config.defaultInputMode = DEFAULT_INPUT_MODE;
        config.has_defaultInputMode = true;
        config.socdMode = SOCD_MODE_UP_PRIORITY;
        config.has_socdMode = true;
        config.dpadMode = DPAD_MODE_DIGITAL;
        config.has_dpadMode = true;

        // ---- profiles ----
        config.profiles_count = 2;
        for (uint32_t i = 0; i < config.profiles_count; i++) {
            config.profiles[i].has_keyMapping = true;
            config.profiles[i].keyMapping = config.keyMapping;
            config.profiles[i].has_ledMode = true;
            config.profiles[i].ledMode = LED_MODE;
        }
        config.activeProfile = 0;
        config.has_activeProfile = true;
    }

    bool CONFIG_MODE = false;
    Config config;
    uint8_t matrixRows = 0;
    uint8_t matrixCols = 0;
    volatile bool menuActive = false;
    volatile bool menuToggleRequested = false;
    LedPreview ledPreview;
};

#endif
