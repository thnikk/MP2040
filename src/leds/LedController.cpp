#include "leds/LedController.h"
#include "storagemanager.h"
#include "types.h"
#include "Neopixel.h"

#include <algorithm>
#include <cmath>
#include <cstring>

// Theme step interval (ms) bounds per effect, indexed by LedMode. The 0-100%
// speed slider maps exponentially into these: 0% = slowest, 100% = fastest.
// CUSTOM is unused (no animation); BPS steps on the fixed render cadence and
// is handled separately in renderBps(). RAIN uses the interval for its fade
// step; the drop interval itself is a fixed random 1-3s (see update()).
static const SpeedRange speedRanges[] = {
    { 0, 0 },    // LED_MODE_CUSTOM
    { 6, 117 },  // LED_MODE_CYCLE   (256-step wheel: ~1.5s .. ~30s / rev)
    { 16, 250 }, // LED_MODE_REACTIVE (32-step fade: ~0.5s .. ~8s)
    { 0, 0 },    // LED_MODE_BPS     (handled separately)
    { 50, 660 }, // LED_MODE_RIPPLE  (radius +1/step: ~0.3s .. ~4s cross)
    { 25, 200 }, // LED_MODE_RAIN    (32-step fade: ~0.8s .. ~6.4s)
};

// Rain drop interval bounds (ms): a random drop fires every 0.2-2 seconds.
#define RAIN_DROP_MIN_MS 200
#define RAIN_DROP_MAX_MS 2000

// Length of the pressed->normal gradient trailing a ripple ring, in grid
// cells. The ring itself (behind == 0) is full pressed; the trail fades
// linearly back to the normal color over this many cells.
#define RIPPLE_TRAIL_CELLS 4

// Suspend/wake fade step (0-255 per 20ms render tick). 255/10 = ~25 ticks,
// so the fade in/out takes roughly half a second.
#define LED_FADE_STEP 10

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
    ledMode(LED_MODE_CUSTOM),
    ledSpeedPercent{50, 50, 50, 50, 50, 50},
    ledSpeed(20),
    lastThemeMillis(0),
    ledTimeoutMs(0),
    ledLastActivityMillis(0),
    ledState(LedState::ON),
    ledDim(255),
    brightnessByMode{255, 255, 255, 255, 255, 255},
    colorNormalByMode{0x00FF00, 0x00FF00, 0x00FF00, 0x00FF00, 0x00FF00, 0x00FF00},
    colorPressedByMode{0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF},
    ledColorCount(0),
    nextRunTime(nil_time),
    pressedLeds(nullptr),
    ledSat(nullptr),
    ledVal(nullptr),
    hue(0),
    prevKeyState(0),
    bpsCount(0),
    lastBpsMillis(0),
    bpsColor(0),
    lastColor(0),
    rainDropMillis(0),
    rainRandState(0)
{
    for (Pin_t pin = 0; pin < (Pin_t)MAX_KEYS; pin++)
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
    // Config speed is 0-100 percent (higher = faster). recomputeLedSpeed()
    // maps the current mode's percent to a per-effect theme step interval.
    for (uint32_t i = 0; i < 6; i++)
        ledSpeedPercent[i] = i < ledOptions.ledSpeeds_count && ledOptions.ledSpeeds[i] <= 100
            ? ledOptions.ledSpeeds[i] : 50;
    recomputeLedSpeed();
    for (uint32_t i = 0; i < 6; i++)
    {
        brightnessByMode[i] = i < ledOptions.brightnessByMode_count
            ? ledOptions.brightnessByMode[i] : ledOptions.brightnessMaximum;
        if (brightnessByMode[i] > 255) brightnessByMode[i] = 255;
    }
    for (uint32_t i = 0; i < 6; i++)
    {
        colorNormalByMode[i] = i < ledOptions.colorNormalByMode_count
            ? ledOptions.colorNormalByMode[i] : ledOptions.colorNormal;
        colorPressedByMode[i] = i < ledOptions.colorPressedByMode_count
            ? ledOptions.colorPressedByMode[i] : ledOptions.colorPressed;
    }
    // Inactivity timeout (0-600s), 0 = always on. Clamp defensively against
    // hand-edited configs. The clock starts at boot so fresh boards stay lit
    // until the first release.
    ledTimeoutMs = (ledOptions.ledTimeout > 600 ? 600 : ledOptions.ledTimeout) * 1000u;
    ledLastActivityMillis = to_ms_since_boot(get_absolute_time());
    ledState = LedState::ON;
    ledDim = 255;
    for (Pin_t pin = 0; pin < (Pin_t)MAX_KEYS; pin++)
    {
        pinLedIndices[pin] = pin < (Pin_t)ledOptions.pinLedIndices_count
            ? ledOptions.pinLedIndices[pin] : -1;
    }

    // Mirror the active profile's per-key colors for custom mode. A key with
    // a normal color but no pressed entry uses the global pressed color.
    const KeyMapping& km = Storage::getInstance().getKeyMapping();
    ledColorCount = km.ledNormalColors_count < MAX_KEYS ? km.ledNormalColors_count : MAX_KEYS;
    for (Pin_t pin = 0; pin < (Pin_t)ledColorCount; pin++)
    {
        ledNormalColors[pin] = km.ledNormalColors[pin];
        ledPressedColors[pin] = pin < (Pin_t)km.ledPressedColors_count
            ? km.ledPressedColors[pin] : 0;
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
    for (Pin_t pin = 0; pin < (Pin_t)MAX_KEYS; pin++)
    {
        if (pinLedIndices[pin] >= 0)
            total = std::max(total, (uint32_t)(pinLedIndices[pin] + ledsPerKey));
    }
    if (total == 0)
    {
        const KeyMapping& keyMapping = Storage::getInstance().getKeyMapping();
        for (Pin_t pin = 0; pin < (Pin_t)MAX_KEYS; pin++)
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
    for (uint32_t i = 0; i < MAX_RIPPLES; i++)
        ripples[i].active = false;
    prevKeyState = 0;
    hue = 0;
    lastThemeMillis = 0;
    bpsCount = 0;
    bpsColor = 0;
    lastColor = 0;
    rainRandState = to_ms_since_boot(get_absolute_time()) ^ 0x9E3779B9u;
    rainDropMillis = 0;

    nextRunTime = make_timeout_time_ms(0);
}

void LedController::update()
{
    if (neopixel == nullptr) return;

    // Apply any live LED options pushed from the web config (core 0).
    // Static scratch buffer: LedPreview is ~1KB (per-key color arrays), too
    // large to keep on the 4KB core-1 stack for the whole update() frame.
    static LedPreview preview;
    if (Storage::getInstance().consumeLedPreview(preview))
        applyLedPreview(preview);

    if (!time_reached(nextRunTime)) return;
    nextRunTime = make_timeout_time_ms(20);

    const KeyMask keyState = Storage::getInstance().getKeyState();

    // Inactivity timeout: any held key keeps the LEDs on; once the last key
    // is released the strip fades out after ledTimeoutMs and a fresh press
    // fades it back in. While fully off we skip rendering entirely (no show()
    // calls), so the only per-tick work is the timestamp bookkeeping below.
    const uint32_t now = to_ms_since_boot(get_absolute_time());
    if (ledTimeoutMs > 0)
    {
        if (keyState.any())
            ledLastActivityMillis = now;

        switch (ledState)
        {
            case LedState::ON:
                if (now - ledLastActivityMillis >= ledTimeoutMs)
                    ledState = LedState::FADING_OUT;
                break;

            case LedState::FADING_OUT:
                // A press mid-fade cancels the suspend and ramps back up.
                if (keyState.any())
                    ledState = LedState::FADING_IN;
                break;

            case LedState::OFF:
                if (keyState.any())
                {
                    ledState = LedState::FADING_IN;
                    ledDim = 0;
                }
                else
                {
                    prevKeyState = keyState; // stay consistent for wake-up
                    return;
                }
                break;

            case LedState::FADING_IN:
                break;
        }
    }
    else
    {
        ledState = LedState::ON;
    }

    // Step the fade; the ON state holds full brightness.
    switch (ledState)
    {
        case LedState::FADING_OUT:
            if (ledDim <= LED_FADE_STEP)
            {
                ledDim = 0;
                ledState = LedState::OFF;
                neopixel->off();
                prevKeyState = keyState; // stay consistent for wake-up
                return;
            }
            ledDim -= LED_FADE_STEP;
            break;

        case LedState::FADING_IN:
            if (ledDim >= 255 - LED_FADE_STEP)
            {
                ledDim = 255;
                ledState = LedState::ON;
            }
            else
            {
                ledDim += LED_FADE_STEP;
            }
            break;

        default:
            ledDim = 255;
            break;
    }

    // BPS press-rate counter: count rising edges of any key.
    const KeyMask rising = keyState & ~prevKeyState;
    if (rising.any())
        bpsCount++;
    prevKeyState = keyState;

    // Build per-LED pressed state from the pin -> LED mapping.
    const uint32_t keyCount = Storage::getInstance().getKeyCount();
    for (uint32_t i = 0; i < stripCount; i++)
        pressedLeds[i] = false;
    for (Pin_t pin = 0; pin < (Pin_t)keyCount; pin++)
    {
        if (!keyState.test(pin)) continue;
        int32_t idx = pinLedIndices[pin];
        if (idx < 0) continue;
        for (uint32_t l = 0; l < ledsPerKey; l++)
        {
            if (idx + (int32_t)l < (int32_t)stripCount)
                pressedLeds[idx + l] = true;
        }
    }

    // Spawn ripples at the grid position of each newly pressed key.
    if (rising.any())
    {
        for (Pin_t pin = 0; pin < (Pin_t)keyCount; pin++)
        {
            if (!rising.test(pin)) continue;
            int32_t idx = pinLedIndices[pin];
            if (idx < 0) continue;
            for (uint32_t row = 0; row < LED_GRID_ROWS; row++)
            {
                for (uint32_t col = 0; col < LED_GRID_COLS; col++)
                {
                    if (BOARD_LED_GRID[row][col] == idx)
                    {
                        spawnRipple(row, col);
                        row = LED_GRID_ROWS;
                        break;
                    }
                }
            }
        }
    }

    // Advance the theme state at the configured speed. Catch up on any missed
    // steps so higher speeds (shorter intervals) actually run faster than the
    // 20ms render cadence.
    if (ledMode != LED_MODE_CUSTOM)
    {
        if (now - lastThemeMillis >= ledSpeed)
        {
            uint32_t elapsed = now - lastThemeMillis;
            uint32_t steps = elapsed / ledSpeed;
            for (uint32_t s = 0; s < steps; s++)
                advanceThemeState();
            lastThemeMillis = now - (elapsed % ledSpeed);
        }
    }

    // Rain: fire a random drop every 1-3 seconds, lighting one unpressed LED
    // at full brightness (it then fades in advanceThemeState). Held LEDs are
    // skipped so presses never act as drops.
    if (ledMode == LED_MODE_RAIN)
    {
        rainRandState ^= now; // stir the PRNG so drops stay varied
        if (now >= rainDropMillis && stripCount > 0)
        {
            // Uniformly pick among unpressed LEDs (two-pass: count, then
            // select the kth). If every LED is held, skip the drop.
            uint32_t unpressed = 0;
            for (uint32_t i = 0; i < stripCount; i++)
            {
                if (!pressedLeds[i]) unpressed++;
            }
            if (unpressed > 0)
            {
                uint32_t pick = rainRandom() % unpressed;
                for (uint32_t i = 0; i < stripCount; i++)
                {
                    if (!pressedLeds[i])
                    {
                        if (pick == 0)
                        {
                            ledVal[i] = 255;
                            break;
                        }
                        pick--;
                    }
                }
            }
            rainDropMillis = now + RAIN_DROP_MIN_MS
                + (rainRandom() % (RAIN_DROP_MAX_MS - RAIN_DROP_MIN_MS + 1));
        }
    }

    switch (ledMode)
    {
        case LED_MODE_CYCLE:    renderCycle();    break;
        case LED_MODE_REACTIVE: renderReactive(); break;
        case LED_MODE_BPS:      renderBps();      break;
        case LED_MODE_RIPPLE:   renderRipple();   break;
        case LED_MODE_RAIN:     renderRain();     break;
        default:                renderCustom();   break;
    }
}

// Apply live LED options from the web config without rebuilding the strip.
// Only user-tunable scalars; board properties stay as configured at boot.
void LedController::applyLedPreview(const LedPreview& preview)
{
    ledMode = preview.ledMode;
    // Config speed is 0-100 percent (higher = faster). recomputeLedSpeed()
    // maps the current mode's percent to a per-effect theme step interval.
    for (uint32_t i = 0; i < 6; i++)
        ledSpeedPercent[i] = i < preview.ledSpeedCount && preview.ledSpeed[i] <= 100
            ? preview.ledSpeed[i] : 50;
    recomputeLedSpeed();
    for (uint32_t i = 0; i < 6; i++)
    {
        brightnessByMode[i] = i < preview.brightnessByModeCount
            ? preview.brightnessByMode[i] : 255;
        if (brightnessByMode[i] > 255) brightnessByMode[i] = 255;
    }
    for (uint32_t i = 0; i < 6; i++)
    {
        colorNormalByMode[i] = i < preview.colorCount
            ? preview.colorNormalByMode[i] : 0x00FF00;
        colorPressedByMode[i] = i < preview.colorCount
            ? preview.colorPressedByMode[i] : 0xFFFFFF;
    }
    // Per-key colors for custom mode; keys without an entry use Custom mode's
    // normal/pressed colors.
    ledColorCount = preview.ledNormalColorCount < MAX_KEYS
        ? preview.ledNormalColorCount : MAX_KEYS;
    for (Pin_t pin = 0; pin < (Pin_t)ledColorCount; pin++)
    {
        ledNormalColors[pin] = preview.ledNormalColors[pin];
        ledPressedColors[pin] = pin < (Pin_t)preview.ledPressedColorCount
            ? preview.ledPressedColors[pin] : 0;
    }
    // Inactivity timeout (0-600s), 0 = always on; clamp defensively.
    ledTimeoutMs = (preview.ledTimeout > 600 ? 600 : preview.ledTimeout) * 1000u;
    ledLastActivityMillis = to_ms_since_boot(get_absolute_time());
    ledState = LedState::ON;
    ledDim = 255;

    // Reset theme state so the new mode starts from a clean slate.
    hue = 0;
    lastThemeMillis = 0;
    if (stripCount > 0)
    {
        std::memset(ledSat, 0, stripCount * sizeof(int));
        std::memset(ledVal, 0, stripCount * sizeof(int));
    }
    for (uint32_t i = 0; i < MAX_RIPPLES; i++)
        ripples[i].active = false;
    prevKeyState = Storage::getInstance().getKeyState();
    rainRandState = to_ms_since_boot(get_absolute_time()) ^ 0x9E3779B9u;
    rainDropMillis = 0;
}

// Map the 0-100% speed to a theme step interval (ms) for the current mode.
// Exponential interpolation so each percent step feels similar across the
// whole range: 0% = slowest (maxInterval), 100% = fastest (minInterval).
void LedController::recomputeLedSpeed()
{
    uint32_t minInterval = 0, maxInterval = 0;
    if (ledMode < sizeof(speedRanges) / sizeof(speedRanges[0]))
    {
        minInterval = speedRanges[ledMode].minIntervalMs;
        maxInterval = speedRanges[ledMode].maxIntervalMs;
    }
    if (minInterval == 0 || maxInterval == 0)
    {
        ledSpeed = 20; // CUSTOM (or unknown mode): no animation, default cadence
        return;
    }
    uint32_t pct = ledSpeedPercent[ledMode < 6 ? ledMode : 0];
    if (pct > 100) pct = 100;
    float t = powf((float)minInterval / (float)maxInterval, (float)pct / 100.0f);
    ledSpeed = (uint32_t)((float)maxInterval * t);
    if (ledSpeed < 1) ledSpeed = 1;
    if (ledSpeed > 1000) ledSpeed = 1000;
}

// The current mode's normal/pressed colors (Custom = index 0, Ripple = 4,
// Rain = 5). Cycle/Reactive/BPS are hue-based and never call these.
uint32_t LedController::currentNormalColor() const
{
    return colorNormalByMode[ledMode < 6 ? ledMode : 0];
}

uint32_t LedController::currentPressedColor() const
{
    return colorPressedByMode[ledMode < 6 ? ledMode : 0];
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

        case LED_MODE_RIPPLE:
            for (uint32_t i = 0; i < MAX_RIPPLES; i++)
            {
                if (!ripples[i].active) continue;
                ripples[i].radius++;
                // Keep the ripple alive until its gradient trail has fully
                // passed the grid; deactivating at the ring's edge would cut
                // off the trailing cells still visible on the board.
                if (ripples[i].radius > maxGridDistance(ripples[i].row, ripples[i].col) + RIPPLE_TRAIL_CELLS)
                    ripples[i].active = false;
            }
            break;

        case LED_MODE_RAIN:
            for (uint32_t i = 0; i < stripCount; i++)
            {
                // Held LEDs show the pressed color in renderRain() and are not
                // treated as drops; freeze their fade while pressed.
                if (!pressedLeds[i] && ledVal[i] > 0)
                {
                    ledVal[i] -= 8;
                    if (ledVal[i] < 0) ledVal[i] = 0;
                }
            }
            break;

        default:
            break;
    }
}

// Custom: per-key colors, brightening to the pressed color while held. Keys
// with no per-key entry (or an entry of 0) use Custom mode's normal/pressed
// colors; unmapped LEDs show Custom mode's normal color.
void LedController::renderCustom()
{
    float scale = effBrightness() / 255.0f;
    const uint32_t normalColor = currentNormalColor();
    uint8_t nr = static_cast<uint8_t>(((normalColor >> 16) & 0xFF) * scale);
    uint8_t ng = static_cast<uint8_t>(((normalColor >> 8) & 0xFF) * scale);
    uint8_t nb = static_cast<uint8_t>((normalColor & 0xFF) * scale);

    // Unmapped LEDs show Custom mode's normal color.
    for (uint32_t i = 0; i < stripCount; i++)
        neopixel->setPixel(i, nr, ng, nb);

    const uint32_t keyCount = Storage::getInstance().getKeyCount();
    const uint32_t pressedColor = currentPressedColor();
    for (Pin_t pin = 0; pin < (Pin_t)keyCount; pin++)
    {
        int32_t idx = pinLedIndices[pin];
        if (idx < 0) continue;

        const bool hasCustom = pin < (Pin_t)ledColorCount;
        uint32_t normal = hasCustom && ledNormalColors[pin] != 0
            ? ledNormalColors[pin] : normalColor;
        uint32_t pressed = hasCustom && ledPressedColors[pin] != 0
            ? ledPressedColors[pin] : pressedColor;
        uint8_t kR = static_cast<uint8_t>(((normal >> 16) & 0xFF) * scale);
        uint8_t kG = static_cast<uint8_t>(((normal >> 8) & 0xFF) * scale);
        uint8_t kB = static_cast<uint8_t>((normal & 0xFF) * scale);
        uint8_t pR = static_cast<uint8_t>(((pressed >> 16) & 0xFF) * scale);
        uint8_t pG = static_cast<uint8_t>(((pressed >> 8) & 0xFF) * scale);
        uint8_t pB = static_cast<uint8_t>((pressed & 0xFF) * scale);

        for (uint32_t l = 0; l < ledsPerKey; l++)
        {
            uint32_t i = idx + l;
            if (i >= stripCount) break;
            if (pressedLeds[i])
                neopixel->setPixel(i, pR, pG, pB);
            else
                neopixel->setPixel(i, kR, kG, kB);
        }
    }
    neopixel->show();
}

// Cycle: rainbow wheel, each LED offset by index; pressed LEDs flash white.
void LedController::renderCycle()
{
    const uint32_t brightness = effBrightness();
    for (uint32_t i = 0; i < stripCount; i++)
    {
        if (pressedLeds[i])
        {
            neopixel->setPixel(i, brightness, brightness, brightness);
        }
        else
        {
            uint8_t r, g, b;
            hsvToRgb(static_cast<uint8_t>(hue + i * 20), 255, brightness, r, g, b);
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
                 ledVal[i] * effBrightness() / 255, r, g, b);
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

    // Color-smoothing step per render (fixed 20ms cadence). The 0-100% speed
    // maps linearly to 1..8: 0% = slow (~5s full swing), 100% = fast.
    uint32_t pct = ledSpeedPercent[ledMode < 6 ? ledMode : 0];
    if (pct > 100) pct = 100;
    const uint16_t bpsSpeed = (uint16_t)(1 + (pct * 7) / 100);
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
    const uint32_t brightness = effBrightness();
    for (uint32_t i = 0; i < stripCount; i++)
    {
        if (pressedLeds[i])
        {
            neopixel->setPixel(i, brightness, brightness, brightness);
        }
        else
        {
            uint8_t r, g, b;
            hsvToRgb(static_cast<uint8_t>(finalColor + 100), 255, brightness, r, g, b);
            neopixel->setPixel(i, r, g, b);
        }
    }
    neopixel->show();
}

// Largest Chebyshev ring a ripple can reach before it leaves the grid.
int16_t LedController::maxGridDistance(int8_t row, int8_t col)
{
    int16_t maxDist = 0;
    for (uint32_t r = 0; r < LED_GRID_ROWS; r++)
    {
        for (uint32_t c = 0; c < LED_GRID_COLS; c++)
        {
            if (BOARD_LED_GRID[r][c] < 0) continue;
            int16_t dr = static_cast<int16_t>(r) - row;
            if (dr < 0) dr = -dr;
            int16_t dc = static_cast<int16_t>(c) - col;
            if (dc < 0) dc = -dc;
            int16_t dist = dr > dc ? dr : dc;
            if (dist > maxDist) maxDist = dist;
        }
    }
    return maxDist;
}

// Start a new ripple at (row, col). Reuse the oldest slot when all are busy.
void LedController::spawnRipple(int8_t row, int8_t col)
{
    uint32_t oldest = 0;
    for (uint32_t i = 0; i < MAX_RIPPLES; i++)
    {
        if (!ripples[i].active)
        {
            ripples[i].row = row;
            ripples[i].col = col;
            ripples[i].radius = 0;
            ripples[i].active = true;
            return;
        }
        if (ripples[i].radius > ripples[oldest].radius)
            oldest = i;
    }
    ripples[oldest].row = row;
    ripples[oldest].col = col;
    ripples[oldest].radius = 0;
}

// Ripple: a single pressed-color ring expands outward from each pressed key,
// trailed by a smooth gradient interpolating pressed -> normal. The ring
// (behind == 0) is full pressed; cells behind it fade back to normal over
// RIPPLE_TRAIL_CELLS. Overlapping ripples composite by max intensity so the
// nearest/most-intense ring wins.
void LedController::renderRipple()
{
    float scale = effBrightness() / 255.0f;
    const uint32_t normalColor = currentNormalColor();
    const uint32_t pressedColor = currentPressedColor();
    uint8_t nr = static_cast<uint8_t>(((normalColor >> 16) & 0xFF) * scale);
    uint8_t ng = static_cast<uint8_t>(((normalColor >> 8) & 0xFF) * scale);
    uint8_t nb = static_cast<uint8_t>((normalColor & 0xFF) * scale);
    uint8_t pr = static_cast<uint8_t>(((pressedColor >> 16) & 0xFF) * scale);
    uint8_t pg = static_cast<uint8_t>(((pressedColor >> 8) & 0xFF) * scale);
    uint8_t pb = static_cast<uint8_t>((pressedColor & 0xFF) * scale);

    for (uint32_t row = 0; row < LED_GRID_ROWS; row++)
    {
        for (uint32_t col = 0; col < LED_GRID_COLS; col++)
        {
            int32_t idx = BOARD_LED_GRID[row][col];
            if (idx < 0 || idx >= (int32_t)stripCount) continue;

            int16_t t = 0; // 0..255 intensity (255 = full pressed)
            for (uint32_t r = 0; r < MAX_RIPPLES; r++)
            {
                if (!ripples[r].active) continue;
                int16_t dr = static_cast<int16_t>(row) - ripples[r].row;
                if (dr < 0) dr = -dr;
                int16_t dc = static_cast<int16_t>(col) - ripples[r].col;
                if (dc < 0) dc = -dc;
                int16_t behind = ripples[r].radius - (dr > dc ? dr : dc);
                int16_t rt;
                if (behind < 0 || behind >= RIPPLE_TRAIL_CELLS)
                    rt = 0;
                else if (behind == 0)
                    rt = 255;
                else
                    rt = static_cast<int16_t>(255 - (behind * 255) / RIPPLE_TRAIL_CELLS);
                if (rt > t) t = rt;
            }

            neopixel->setPixel(idx,
                static_cast<uint8_t>(nr + (static_cast<int32_t>(pr - nr) * t) / 255),
                static_cast<uint8_t>(ng + (static_cast<int32_t>(pg - ng) * t) / 255),
                static_cast<uint8_t>(nb + (static_cast<int32_t>(pb - nb) * t) / 255));
        }
    }
    neopixel->show();
}

// xorshift32 PRNG for rain drop selection. Deterministic and dependency-free;
// the seed is (re)initialized from the boot clock and stirred each drop.
uint32_t LedController::rainRandom()
{
    uint32_t x = rainRandState;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    rainRandState = x;
    return x;
}

// Rain: LEDs default to black. update() periodically lights a random LED at
// full normal color; advanceThemeState() fades it back to black. Pressed LEDs
// always show the pressed color.
void LedController::renderRain()
{
    float scale = effBrightness() / 255.0f;
    const uint32_t normalColor = currentNormalColor();
    const uint32_t pressedColor = currentPressedColor();
    uint8_t nr = static_cast<uint8_t>(((normalColor >> 16) & 0xFF) * scale);
    uint8_t ng = static_cast<uint8_t>(((normalColor >> 8) & 0xFF) * scale);
    uint8_t nb = static_cast<uint8_t>((normalColor & 0xFF) * scale);
    uint8_t pr = static_cast<uint8_t>(((pressedColor >> 16) & 0xFF) * scale);
    uint8_t pg = static_cast<uint8_t>(((pressedColor >> 8) & 0xFF) * scale);
    uint8_t pb = static_cast<uint8_t>((pressedColor & 0xFF) * scale);

    for (uint32_t i = 0; i < stripCount; i++)
    {
        if (pressedLeds[i])
        {
            neopixel->setPixel(i, pr, pg, pb);
        }
        else
        {
            uint32_t v = ledVal[i] > 0 ? static_cast<uint32_t>(ledVal[i]) : 0;
            neopixel->setPixel(i,
                static_cast<uint8_t>(nr * v / 255),
                static_cast<uint8_t>(ng * v / 255),
                static_cast<uint8_t>(nb * v / 255));
        }
    }
    neopixel->show();
}
