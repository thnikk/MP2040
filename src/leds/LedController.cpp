#include "leds/LedController.h"
#include "storagemanager.h"
#include "types.h"
#include "Neopixel.h"

LedController::LedController() :
    neopixel(nullptr),
    dataPin(-1),
    ledFormat(LED_FORMAT_GRB),
    ledsPerKey(1),
    brightnessMaximum(255),
    colorNormal(0x00FF00),
    colorPressed(0xFFFFFF),
    nextRunTime(nil_time)
{
}

void LedController::setup()
{
    configure();
}

void LedController::configure()
{
    const LEDOptions& ledOptions = Storage::getInstance().getLedOptions();
    const KeyMapping& keyMapping = Storage::getInstance().getKeyMapping();

    dataPin = ledOptions.dataPin;
    ledFormat = ledOptions.ledFormat;
    ledsPerKey = ledOptions.ledsPerKey > 0 ? ledOptions.ledsPerKey : 1;
    brightnessMaximum = ledOptions.brightnessMaximum;
    colorNormal = ledOptions.colorNormal;
    colorPressed = ledOptions.colorPressed;

    // One LED (or ledsPerKey) per key that has a keycode assigned
    uint32_t ledCount = 0;
    for (Pin_t pin = 0; pin < (Pin_t)NUM_BANK0_GPIOS; pin++)
    {
        if (pin < (Pin_t)keyMapping.keycodes_count && keyMapping.keycodes[pin] != 0)
            ledCount += ledsPerKey;
    }

    delete neopixel;
    neopixel = nullptr;

    if (!isValidPin(dataPin) || ledCount == 0)
        return;

    neopixel = new Neopixel(dataPin, ledCount, ledFormat);
    neopixel->off();
    nextRunTime = make_timeout_time_ms(0);
}

void LedController::update()
{
    if (neopixel == nullptr) return;
    if (!time_reached(nextRunTime)) return;
    nextRunTime = make_timeout_time_ms(20);

    const KeyMapping& keyMapping = Storage::getInstance().getKeyMapping();
    const Mask_t keyState = Storage::getInstance().keyState;

    // Scale colors to the configured brightness maximum (0-255)
    float scale = brightnessMaximum / 255.0f;

    // LED test mode: light every LED with the requested color at full
    // brightness, independent of the configured brightness scale.
    uint32_t testColor;
    if (Storage::getInstance().getLedTest(testColor))
    {
        uint8_t tr = static_cast<uint8_t>((testColor >> 16) & 0xFF);
        uint8_t tg = static_cast<uint8_t>((testColor >> 8) & 0xFF);
        uint8_t tb = static_cast<uint8_t>(testColor & 0xFF);
        for (uint32_t i = 0; i < neopixel->getLedCount(); i++)
            neopixel->setPixel(i, tr, tg, tb);
        neopixel->show();
        return;
    }

    uint8_t nr = static_cast<uint8_t>(((colorNormal >> 16) & 0xFF) * scale);
    uint8_t ng = static_cast<uint8_t>(((colorNormal >> 8) & 0xFF) * scale);
    uint8_t nb = static_cast<uint8_t>((colorNormal & 0xFF) * scale);
    uint8_t pr = static_cast<uint8_t>(((colorPressed >> 16) & 0xFF) * scale);
    uint8_t pg = static_cast<uint8_t>(((colorPressed >> 8) & 0xFF) * scale);
    uint8_t pb = static_cast<uint8_t>((colorPressed & 0xFF) * scale);

    uint32_t ledIndex = 0;
    for (Pin_t pin = 0; pin < (Pin_t)NUM_BANK0_GPIOS; pin++)
    {
        if (pin >= (Pin_t)keyMapping.keycodes_count || keyMapping.keycodes[pin] == 0)
            continue;

        bool pressed = (keyState & (1 << pin)) != 0;
        for (uint32_t l = 0; l < ledsPerKey; l++)
        {
            neopixel->setPixel(ledIndex++,
                pressed ? pr : nr,
                pressed ? pg : ng,
                pressed ? pb : nb);
        }
    }

    neopixel->show();
}
