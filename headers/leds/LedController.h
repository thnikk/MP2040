#ifndef _LED_CONTROLLER_H_
#define _LED_CONTROLLER_H_

#include <stdint.h>
#include "pico/time.h"
#include "hardware/gpio.h"
#include "config.pb.h"
#include "leds/LedLayout.h"

class Neopixel;

class LedController {
public:
    LedController();
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

    Neopixel* neopixel;
    int32_t dataPin;
    LEDFormat_Proto ledFormat;
    uint32_t ledsPerKey;
    uint32_t ledCount;
    int32_t pinLedIndices[NUM_BANK0_GPIOS];
    uint32_t brightnessMaximum;
    uint32_t colorNormal;
    uint32_t colorPressed;
    absolute_time_t nextRunTime;
};

#endif
