#ifndef _LED_CONTROLLER_H_
#define _LED_CONTROLLER_H_

#include <stdint.h>
#include "pico/time.h"
#include "hardware/gpio.h"
#include "config.pb.h"
#include "leds/LedLayout.h"
#include "types.h"

class Neopixel;
struct LedPreview;

// LED theme modes (matches the web config "LED Mode" dropdown)
enum LedMode {
    LED_MODE_STATIC = 0,
    LED_MODE_CYCLE,    // rainbow wheel
    LED_MODE_REACTIVE, // white -> rainbow -> off fade
    LED_MODE_BPS,      // color tracks keypress rate
    LED_MODE_RIPPLE,   // rings propagate outward from pressed keys
};

// Concurrent ripple limit: enough for full-board hammering without letting
// ripple state grow unbounded.
#define MAX_RIPPLES 8

// A propagating ring spawned by a key press. The ring lives at the grid cell
// (row, col) and expands outward one cell per theme step.
struct Ripple {
    int8_t row;
    int8_t col;
    int16_t radius;
    bool active;
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

    // Theme state advance (run at the configured animation speed)
    void advanceThemeState();

    // Theme renderers (unified-2022 ports)
    void renderStatic();
    void renderCycle();
    void renderReactive();
    void renderBps();
    void renderRipple();
    int16_t maxGridDistance(int8_t row, int8_t col);
    void spawnRipple(int8_t row, int8_t col);

    Neopixel* neopixel;
    int32_t dataPin;
    LEDFormat_Proto ledFormat;
    uint32_t ledsPerKey;
    uint32_t ledCount;
    uint32_t stripCount;
    uint32_t ledMode;
    uint32_t ledSpeed;        // theme step interval in ms (computed from config)
    uint32_t lastThemeMillis; // last theme state advance time
    int32_t pinLedIndices[NUM_BANK0_GPIOS];
    uint32_t brightnessMaximum;
    uint32_t colorNormal;
    uint32_t colorPressed;
    absolute_time_t nextRunTime;

    // Theme state
    bool* pressedLeds;
    int* ledSat;
    int* ledVal;
    int hue;
    Mask_t prevKeyState;
    uint32_t bpsCount;
    uint32_t lastBpsMillis;
    uint16_t bpsColor;
    uint16_t lastColor;
    Ripple ripples[MAX_RIPPLES];
};

#endif
