#include "Neopixel.h"

#include <cstring>

#include "hardware/clocks.h"
#include "hardware/pio.h"
#include "pico/stdlib.h"

#include "ws2812.pio.h"

Neopixel::Neopixel(int32_t pin, uint32_t numLeds, LEDFormat_Proto format)
    : pin(pin), numLeds(numLeds), format(format)
{
    ledData = new uint8_t[numLeds * 3];
    memset(ledData, 0, numLeds * 3);

    PIO pio = pio0;
    uint offset = pio_add_program(pio, &ws2812_program);
    uint sm = pio_claim_unused_sm(pio, true);

    pio_sm_config c = ws2812_program_get_default_config(sm);
    sm_config_set_sideset_pins(&c, pin);
    sm_config_set_out_shift(&c, false, true, 0);
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_TX);

    // 125 MHz system clock / 10 cycles per bit / 800 kHz bitrate
    uint clkdiv = clock_get_hz(clk_sys) / (ws2812_T1 + ws2812_T2 + ws2812_T3) / 800000;
    sm_config_set_clkdiv(&c, clkdiv);

    pio_gpio_init(pio, pin);
    pio_sm_init(pio, sm, offset, &c);
    pio_sm_set_enabled(pio, sm, true);
}

Neopixel::~Neopixel()
{
    delete[] ledData;
    if (numLeds > 0) {
        pio_sm_set_enabled(pio0, 0, false);
    }
}

void Neopixel::setPixel(uint32_t index, uint8_t r, uint8_t g, uint8_t b)
{
    if (index >= numLeds) return;
    ledData[index * 3] = r;
    ledData[index * 3 + 1] = g;
    ledData[index * 3 + 2] = b;
}

void Neopixel::fill(uint8_t r, uint8_t g, uint8_t b)
{
    for (uint32_t i = 0; i < numLeds; i++)
        setPixel(i, r, g, b);
}

void Neopixel::off()
{
    fill(0, 0, 0);
    show();
}

uint32_t Neopixel::pixelWord(uint8_t r, uint8_t g, uint8_t b)
{
    switch (format)
    {
        case LED_FORMAT_GRB:
        case LED_FORMAT_GRBW:
            return (static_cast<uint32_t>(g) << 16) | (static_cast<uint32_t>(r) << 8) | b;
        case LED_FORMAT_RGB:
        case LED_FORMAT_RGBW:
        default:
            return (static_cast<uint32_t>(r) << 16) | (static_cast<uint32_t>(g) << 8) | b;
    }
}

void Neopixel::show()
{
    for (uint32_t i = 0; i < numLeds; i++)
    {
        uint32_t word = pixelWord(ledData[i * 3], ledData[i * 3 + 1], ledData[i * 3 + 2]);
        // Shift left so the 24 significant bits land in the MSBs; the PIO
        // shifts LSB-first, producing the correct wire order.
        pio_sm_put_blocking(pio0, 0, word << 8u);
    }
}
