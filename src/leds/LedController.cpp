#include "leds/LedController.h"
#include "storagemanager.h"
#include "types.h"
#include "Neopixel.h"

#include <algorithm>
#include <cstring>

// HSV -> RGB matching Adafruit's ColorHSV() as used by unified-2022.
// hue is 0-255 here (unified passes hue*256 as the 16-bit hue); sat/val 0-255.
static void hsvToRgb(uint8_t h, uint8_t s, uint8_t v, uint8_t& r, uint8_t& g, uint8_t& b)
{
    if (s == 0)
    {
        r = g = b = v;
        return;
    }
    // Remap 8-bit hue to 0-1529 across the 6 HSV sextants
    uint32_t hue32 = ((uint32_t)h * 1529) / 255;
    uint8_t sextant = hue32 >> 8;
    uint8_t f = hue32 & 0xFF;

    uint8_t pv = v * (255 - s) / 255;
    uint8_t qv = v * (255 - (s * f) / 255) / 255;
    uint8_t tv = v * (255 - (s * (255 - f)) / 255) / 255;

    switch (sextant)
    {
        case 0: r = v; g = tv; b = pv; break;
        case 1: r = qv; g = v; b = pv; break;
        case 2: r = pv; g = v; b = tv; break;
        case 3: r = pv; g = qv; b = v; break;
        case 4: r = tv; g = pv; b = v; break;
        default: r = v; g = pv; b = qv; break;
    }
}

LedController::LedController() :
    neopixel(nullptr),
    dataPin(-1),
    ledFormat(LED_FORMAT_GRB),
    ledsPerKey(1),
    ledCount(0),
    stripCount(0),
    ledMode(LED_MODE_STATIC),
    ledSpeed(20),
    lastThemeMillis(0),
    brightnessMaximum(255),
    colorNormal(0x00FF00),
    colorPressed(0xFFFFFF),
    nextRunTime(nil_time),
    pressedLeds(nullptr),
    ledSat(nullptr),
    ledVal(nullptr),
    hue(0),
    prevKeyState(0),
    bpsCount(0),
    lastBpsMillis(0),
    bpsColor(0),
    lastColor(0)
{
    for (Pin_t pin = 0; pin < (Pin_t)NUM_BANK0_GPIOS; pin++)
        pinLedIndices[pin] = -1;
}

LedController::~LedController()
{
    delete[] pressedLeds;
    delete[] ledSat;
    delete[] ledVal;
}

void LedController::setup()
{
    configure();
}

uint32_t LedController::getLedCount()
{
    return neopixel != nullptr ? neopixel->getLedCount() : 0;
}

int32_t LedController::getLedAt(uint32_t row, uint32_t col)
{
    if (row >= LED_GRID_ROWS || col >= LED_GRID_COLS)
        return -1;
    return BOARD_LED_GRID[row][col];
}

void LedController::configure()
{
    const LEDOptions& ledOptions = Storage::getInstance().getLedOptions();

    dataPin = ledOptions.dataPin;
    ledFormat = ledOptions.ledFormat;
    ledsPerKey = ledOptions.ledsPerKey > 0 ? ledOptions.ledsPerKey : 1;
    ledCount = ledOptions.ledCount;
    ledMode = ledOptions.ledMode;
    // Config speed is 1-255 (higher = faster). Convert to the theme step
    // interval in ms: 256 - speed. 236 -> 20ms (the default cadence).
    ledSpeed = 256 - (ledOptions.ledSpeed > 0 ? ledOptions.ledSpeed : 236);
    if (ledSpeed < 1) ledSpeed = 1;
    if (ledSpeed > 1000) ledSpeed = 1000;
    brightnessMaximum = ledOptions.brightnessMaximum;
    colorNormal = ledOptions.colorNormal;
    colorPressed = ledOptions.colorPressed;
    for (Pin_t pin = 0; pin < (Pin_t)NUM_BANK0_GPIOS; pin++)
    {
        pinLedIndices[pin] = pin < (Pin_t)ledOptions.pinLedIndices_count
            ? ledOptions.pinLedIndices[pin] : -1;
    }

    // Total strip length: use the configured count, or derive from the highest
    // LED index in the 2D grid, or from the pin mappings.
    uint32_t total = ledCount;
    for (uint32_t row = 0; row < LED_GRID_ROWS; row++)
    {
        for (uint32_t col = 0; col < LED_GRID_COLS; col++)
        {
            if (BOARD_LED_GRID[row][col] >= 0)
                total = std::max(total, (uint32_t)(BOARD_LED_GRID[row][col] + 1));
        }
    }
    for (Pin_t pin = 0; pin < (Pin_t)NUM_BANK0_GPIOS; pin++)
    {
        if (pinLedIndices[pin] >= 0)
            total = std::max(total, (uint32_t)(pinLedIndices[pin] + ledsPerKey));
    }
    if (total == 0)
    {
        const KeyMapping& keyMapping = Storage::getInstance().getKeyMapping();
        for (Pin_t pin = 0; pin < (Pin_t)NUM_BANK0_GPIOS; pin++)
        {
            if (pin < (Pin_t)keyMapping.keycodes_count && keyMapping.keycodes[pin] != 0)
                total += ledsPerKey;
        }
    }

    delete neopixel;
    neopixel = nullptr;

    if (!isValidPin(dataPin) || total == 0)
        return;

    stripCount = total;
    neopixel = new Neopixel(dataPin, stripCount, ledFormat);
    neopixel->off();

    delete[] pressedLeds;
    delete[] ledSat;
    delete[] ledVal;
    pressedLeds = new bool[stripCount];
    ledSat = new int[stripCount];
    ledVal = new int[stripCount];
    std::memset(pressedLeds, 0, stripCount * sizeof(bool));
    std::memset(ledSat, 0, stripCount * sizeof(int));
    std::memset(ledVal, 0, stripCount * sizeof(int));
    prevKeyState = 0;
    hue = 0;
    lastThemeMillis = 0;
    bpsCount = 0;
    bpsColor = 0;
    lastColor = 0;

    nextRunTime = make_timeout_time_ms(0);
}

void LedController::update()
{
    if (neopixel == nullptr) return;

    // Apply any live LED options pushed from the web config (core 0).
    LedPreview preview;
    if (Storage::getInstance().consumeLedPreview(preview))
        applyLedPreview(preview);

    if (!time_reached(nextRunTime)) return;
    nextRunTime = make_timeout_time_ms(20);

    const Mask_t keyState = Storage::getInstance().keyState;

    // BPS press-rate counter: count rising edges of any key.
    if (keyState & ~prevKeyState)
        bpsCount++;
    prevKeyState = keyState;

    // Build per-LED pressed state from the pin -> LED mapping.
    for (uint32_t i = 0; i < stripCount; i++)
        pressedLeds[i] = false;
    for (Pin_t pin = 0; pin < (Pin_t)NUM_BANK0_GPIOS; pin++)
    {
        if (!(keyState & (1 << pin))) continue;
        int32_t idx = pinLedIndices[pin];
        if (idx < 0) continue;
        for (uint32_t l = 0; l < ledsPerKey; l++)
        {
            if (idx + (int32_t)l < (int32_t)stripCount)
                pressedLeds[idx + l] = true;
        }
    }

    // Advance the theme state at the configured speed. Catch up on any missed
    // steps so higher speeds (shorter intervals) actually run faster than the
    // 20ms render cadence.
    if (ledMode != LED_MODE_STATIC)
    {
        uint32_t now = to_ms_since_boot(get_absolute_time());
        if (now - lastThemeMillis >= ledSpeed)
        {
            uint32_t elapsed = now - lastThemeMillis;
            uint32_t steps = elapsed / ledSpeed;
            for (uint32_t s = 0; s < steps; s++)
                advanceThemeState();
            lastThemeMillis = now - (elapsed % ledSpeed);
        }
    }

    switch (ledMode)
    {
        case LED_MODE_CYCLE:    renderCycle();    break;
        case LED_MODE_REACTIVE: renderReactive(); break;
        case LED_MODE_BPS:      renderBps();      break;
        default:                renderStatic();   break;
    }
}

// Apply live LED options from the web config without rebuilding the strip.
// Only user-tunable scalars; board properties stay as configured at boot.
void LedController::applyLedPreview(const LedPreview& preview)
{
    ledMode = preview.ledMode;
    // Config speed is 1-255 (higher = faster). Convert to the theme step
    // interval in ms: 256 - speed. 236 -> 20ms (the default cadence).
    uint32_t speed = preview.ledSpeed > 0 ? preview.ledSpeed : 236;
    ledSpeed = 256 - speed;
    if (ledSpeed < 1) ledSpeed = 1;
    if (ledSpeed > 1000) ledSpeed = 1000;
    brightnessMaximum = preview.brightnessMaximum;
    colorNormal = preview.colorNormal;
    colorPressed = preview.colorPressed;

    // Reset theme state so the new mode starts from a clean slate.
    hue = 0;
    lastThemeMillis = 0;
    if (stripCount > 0)
    {
        std::memset(ledSat, 0, stripCount * sizeof(int));
        std::memset(ledVal, 0, stripCount * sizeof(int));
    }
    prevKeyState = Storage::getInstance().keyState;
}

// Advance the theme state one step (hue / per-LED fade state). Called at the
// configured animation speed; renderers only draw the current state.
void LedController::advanceThemeState()
{
    switch (ledMode)
    {
        case LED_MODE_CYCLE:
            hue--;
            break;

        case LED_MODE_REACTIVE:
            for (uint32_t i = 0; i < stripCount; i++)
            {
                if (!pressedLeds[i])
                {
                    if (ledSat[i] < 255) ledSat[i] += 8;
                    if (ledSat[i] > 255) ledSat[i] = 255;
                    if (ledSat[i] == 255 && ledVal[i] > 0) ledVal[i] -= 8;
                    if (ledVal[i] < 0) ledVal[i] = 0;
                }
                else
                {
                    ledSat[i] = 0;
                    ledVal[i] = 255;
                }
            }
            hue -= 8;
            if (hue < 0) hue = 255;
            break;

        default:
            break;
    }
}

// Static: normal color, pressed LEDs show the pressed color.
void LedController::renderStatic()
{
    float scale = brightnessMaximum / 255.0f;
    uint8_t nr = static_cast<uint8_t>(((colorNormal >> 16) & 0xFF) * scale);
    uint8_t ng = static_cast<uint8_t>(((colorNormal >> 8) & 0xFF) * scale);
    uint8_t nb = static_cast<uint8_t>((colorNormal & 0xFF) * scale);
    uint8_t pr = static_cast<uint8_t>(((colorPressed >> 16) & 0xFF) * scale);
    uint8_t pg = static_cast<uint8_t>(((colorPressed >> 8) & 0xFF) * scale);
    uint8_t pb = static_cast<uint8_t>((colorPressed & 0xFF) * scale);

    for (uint32_t i = 0; i < stripCount; i++)
    {
        if (pressedLeds[i])
            neopixel->setPixel(i, pr, pg, pb);
        else
            neopixel->setPixel(i, nr, ng, nb);
    }
    neopixel->show();
}

// Cycle: rainbow wheel, each LED offset by index; pressed LEDs flash white.
void LedController::renderCycle()
{
    for (uint32_t i = 0; i < stripCount; i++)
    {
        if (pressedLeds[i])
        {
            neopixel->setPixel(i, brightnessMaximum, brightnessMaximum, brightnessMaximum);
        }
        else
        {
            uint8_t r, g, b;
            hsvToRgb(static_cast<uint8_t>(hue + i * 20), 255, brightnessMaximum, r, g, b);
            neopixel->setPixel(i, r, g, b);
        }
    }
    neopixel->show();
}

// Reactive: unpressed LEDs fade white -> rainbow -> off; pressed LEDs flash white.
void LedController::renderReactive()
{
    for (uint32_t i = 0; i < stripCount; i++)
    {
        uint8_t r, g, b;
        hsvToRgb(static_cast<uint8_t>(hue + i * 50), ledSat[i],
                 ledVal[i] * brightnessMaximum / 255, r, g, b);
        neopixel->setPixel(i, r, g, b);
    }
    neopixel->show();
}

// BPS: color reflects keypress rate; pressed LEDs flash white.
void LedController::renderBps()
{
    uint32_t now = to_ms_since_boot(get_absolute_time());
    if (now - lastBpsMillis >= 1000)
    {
        lastColor = bpsColor;
        bpsColor = static_cast<uint16_t>(bpsCount * 10);
        bpsCount = 0;
        lastBpsMillis = now;
    }

    const uint16_t bpsSpeed = 3;
    if (lastColor > bpsColor)
    {
        lastColor -= bpsSpeed;
        if (lastColor - bpsColor < bpsSpeed) lastColor = bpsColor;
    }
    else if (lastColor < bpsColor)
    {
        lastColor += bpsSpeed;
        if (bpsColor - lastColor < bpsSpeed) lastColor = bpsColor;
    }

    uint8_t finalColor = static_cast<uint8_t>(lastColor % 256);
    for (uint32_t i = 0; i < stripCount; i++)
    {
        if (pressedLeds[i])
        {
            neopixel->setPixel(i, brightnessMaximum, brightnessMaximum, brightnessMaximum);
        }
        else
        {
            uint8_t r, g, b;
            hsvToRgb(static_cast<uint8_t>(finalColor + 100), 255, brightnessMaximum, r, g, b);
            neopixel->setPixel(i, r, g, b);
        }
    }
    neopixel->show();
}
