#ifndef _LED_CONTROLLER_H_
#define _LED_CONTROLLER_H_

#include <stdint.h>
#include "pico/time.h"
#include "hardware/gpio.h"
#include "config.pb.h"
#include "leds/LedLayout.h"
#include "types.h"
#include "keymask.h"

class Neopixel;
struct LedPreview;

// LED theme modes (matches the web config "LED Mode" dropdown)
enum LedMode {
    LED_MODE_CUSTOM = 0, // per-key colors (unset keys use Custom mode's colors)
    LED_MODE_CYCLE,    // rainbow wheel
    LED_MODE_REACTIVE, // white -> rainbow -> off fade
    LED_MODE_BPS,      // color tracks keypress rate
    LED_MODE_RIPPLE,   // rings propagate outward from pressed keys
    LED_MODE_RAIN,     // random drops light up and fade back to black
    LED_MODE_FIRE,     // per-LED ember flicker in the normal color
    LED_MODE_COUNT,    // number of modes (per-mode arrays are sized to this)
};

// Concurrent ripple limit: enough for full-board hammering without letting
// ripple state grow unbounded.
#define MAX_RIPPLES 8

// Theme step interval (ms) range per effect. The 0-100% speed slider maps
// exponentially into [min, max]: 0% = slowest, 100% = fastest. BPS is handled
// separately (it steps on the fixed render cadence, not the interval).
struct SpeedRange {
    uint32_t minIntervalMs;
    uint32_t maxIntervalMs;
};

// A propagating ring spawned by a key press. The ring lives at the grid cell
// (row, col) and expands outward one cell per theme step.
struct Ripple {
    int8_t row;
    int8_t col;
    int16_t radius;
    bool active;
};

// Inactivity-timeout state. The strip fades out when the timeout expires and
// fades back in on the next press, so the transition isn't an abrupt cut.
enum class LedState {
    ON,          // rendering at full brightness
    FADING_OUT,  // timeout expired; dimming toward off
    OFF,         // dark; render loop paused until the next press
    FADING_IN,   // a press woke the strip; brightening toward ON
};

class LedController {
public:
    LedController();
    ~LedController();
    void setup();
    void update();
    // Number of LEDs currently driven on the strip
    uint32_t getLedCount();
    // LED strip index at a grid position, or -1 if the cell is empty
    int32_t getLedAt(uint32_t row, uint32_t col);
    uint32_t getGridRows() { return LED_GRID_ROWS; }
    uint32_t getGridCols() { return LED_GRID_COLS; }
private:
    void configure();

    // Apply live LED options from the web config (core 0 -> core 1)
    void applyLedPreview(const LedPreview&);

    // Map the current 0-100% speed to a theme step interval for the current
    // mode. Called from configure() and applyLedPreview().
    void recomputeLedSpeed();

    // Theme state advance (run at the configured animation speed)
    void advanceThemeState();

    // Theme renderers (unified-2022 ports)
    void renderCustom();
    void renderCycle();
    void renderReactive();
    void renderBps();
    void renderRipple();
    void renderRain();
    void renderFire();
    int16_t maxGridDistance(int8_t row, int8_t col);
    void spawnRipple(int8_t row, int8_t col);
    uint32_t rainRandom();
    uint32_t fireRandom();
    // BrightnessMaximum scaled by the fade multiplier (ledDim). Full when the
    // timeout is disabled or the strip is awake; less while fading. Uses the
    // current mode's per-mode brightness.
    uint32_t effBrightness() const {
        return brightnessByMode[ledMode < LED_MODE_COUNT ? ledMode : 0] * ledDim / 255;
    }

    // Mode indicator LED: a separate single-WS2812 strip (board-fixed pin)
    // showing the active input mode. Independent of the theme strip.
    void updateStatusLed();

    Neopixel* neopixel;
    Neopixel* statusLed;
    uint32_t lastStatusColor; // last color shown on the status LED
    bool statusLedEnabled;    // user toggle: false = mode indicator off
    // Mode indicator LED never dims below this (0-255) while enabled, so it
    // stays lit as a "device is powered" indicator during the strip's
    // inactivity fade.
    uint32_t statusLedBrightnessMinimum;
    // Mode indicator LED brightness cap (0-255) when awake. Runtime override
    // of the board's STATUS_LED_BRIGHTNESS_DEFAULT.
    uint32_t statusLedBrightnessMaximum;
    int32_t dataPin;
    LEDFormat_Proto ledFormat;
    uint32_t ledsPerKey;
    uint32_t ledCount;
    uint32_t stripCount;
    uint32_t ledMode;
    // Per-mode config speed (0-100 percent, higher = faster), indexed by
    // LedMode. recomputeLedSpeed() maps the current mode's percent to a theme
    // step interval.
    uint32_t ledSpeedPercent[LED_MODE_COUNT];
    uint32_t ledSpeed;        // theme step interval in ms (computed from percent)
    uint32_t lastThemeMillis; // last theme state advance time
    // Inactivity timeout: LEDs go dark after ledTimeoutMs with no key held (a
    // held key keeps them on; any press wakes them). 0 = always on.
    uint32_t ledTimeoutMs;
    uint32_t ledLastActivityMillis; // last time a key was held (ms since boot)
    LedState ledState;              // timeout state machine (see above)
    uint8_t ledDim;                 // 0-255 fade multiplier applied to all output
    int32_t pinLedIndices[MAX_KEYS];
    // Per-mode brightness (0-255), indexed by LedMode.
    uint32_t brightnessByMode[LED_MODE_COUNT];
    // Per-mode normal/pressed colors, indexed by LedMode. Only modes that
    // render them use them (Custom fallback = index 0, Ripple, Rain, Fire).
    uint32_t colorNormalByMode[LED_MODE_COUNT];
    uint32_t colorPressedByMode[LED_MODE_COUNT];
    // Current mode's normal/pressed colors (indexed by ledMode).
    uint32_t currentNormalColor() const;
    uint32_t currentPressedColor() const;
    // Per-key colors for custom mode, mirrored from the active key mapping.
    // ledColorCount is the number of populated entries; keys >= count (or a
    // count of 0) use Custom mode's currentNormalColor / currentPressedColor.
    uint32_t ledColorCount;
    uint32_t ledNormalColors[MAX_KEYS];
    uint32_t ledPressedColors[MAX_KEYS];
    absolute_time_t nextRunTime;

    // Theme state
    bool* pressedLeds;
    int* ledSat;
    int* ledVal;
    int hue;
    KeyMask prevKeyState;
    uint32_t bpsCount;
    uint32_t lastBpsMillis;
    uint16_t bpsColor;
    uint16_t lastColor;
    Ripple ripples[MAX_RIPPLES];
    uint32_t rainDropMillis; // next random drop time (ms since boot)
    uint32_t rainRandState;  // xorshift PRNG state for drop selection
    uint32_t fireRandState;  // xorshift PRNG state for fire flicker
};

#endif
