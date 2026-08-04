#ifndef _LED_CONTROLLER_H_
#define _LED_CONTROLLER_H_

#include <stdint.h>
#include "pico/time.h"
#include "config.pb.h"

class Neopixel;

class LedController {
public:
    LedController();
    void setup();
    void update();
private:
    void configure();

    Neopixel* neopixel;
    int32_t dataPin;
    LEDFormat_Proto ledFormat;
    uint32_t ledsPerKey;
    uint32_t brightnessMaximum;
    uint32_t colorNormal;
    uint32_t colorPressed;
    absolute_time_t nextRunTime;
};

#endif
