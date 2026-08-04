#ifndef _NEOPIXEL_H_
#define _NEOPIXEL_H_

#include <stdint.h>
#include "hardware/pio.h"
#include "enums.pb.h"

class Neopixel {
public:
    Neopixel(int32_t pin, uint32_t numLeds, LEDFormat_Proto format);
    ~Neopixel();
    void setPixel(uint32_t index, uint8_t r, uint8_t g, uint8_t b);
    void fill(uint8_t r, uint8_t g, uint8_t b);
    void off();
    void show();
    uint32_t getLedCount() { return numLeds; }
    LEDFormat_Proto getFormat() { return format; }
private:
    uint32_t pixelWord(uint8_t r, uint8_t g, uint8_t b);

    uint32_t pin;
    uint32_t numLeds;
    LEDFormat_Proto format;
    PIO pio;
    uint sm;
    uint8_t* ledData;   // numLeds * 3 bytes (R, G, B)
};

#endif
