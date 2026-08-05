#include "touch/TouchGpio.h"

#include "BoardConfig.h"

#include "hardware/gpio.h"

#include "touch.pio.h"

// ---- Board tuning defaults (override in BoardConfig.h) ----

// PIO countdown start; caps the per-pad measurement window. A reading that
// saturates at this value means the pad never discharged (missing resistor).
#ifndef TOUCH_TIMEOUT
#define TOUCH_TIMEOUT 10000
#endif

// Samples per pad used to find the idle baseline during calibration.
#ifndef TOUCH_CAL_SAMPLES
#define TOUCH_CAL_SAMPLES 8
#endif

// Percent over the baseline that counts as a touch.
#ifndef TOUCH_MARGIN
#define TOUCH_MARGIN 15
#endif

// Percent over the baseline below which a touch releases (hysteresis).
#ifndef TOUCH_HYSTERESIS
#define TOUCH_HYSTERESIS 5
#endif

// Fixed threshold overrides (0 = auto-calibrate at boot).
#ifndef TOUCH_THRESHOLD_GP00
#define TOUCH_THRESHOLD_GP00 0
#endif
#ifndef TOUCH_THRESHOLD_GP01
#define TOUCH_THRESHOLD_GP01 0
#endif
#ifndef TOUCH_THRESHOLD_GP02
#define TOUCH_THRESHOLD_GP02 0
#endif
#ifndef TOUCH_THRESHOLD_GP03
#define TOUCH_THRESHOLD_GP03 0
#endif
#ifndef TOUCH_THRESHOLD_GP04
#define TOUCH_THRESHOLD_GP04 0
#endif
#ifndef TOUCH_THRESHOLD_GP05
#define TOUCH_THRESHOLD_GP05 0
#endif
#ifndef TOUCH_THRESHOLD_GP06
#define TOUCH_THRESHOLD_GP06 0
#endif
#ifndef TOUCH_THRESHOLD_GP07
#define TOUCH_THRESHOLD_GP07 0
#endif
#ifndef TOUCH_THRESHOLD_GP08
#define TOUCH_THRESHOLD_GP08 0
#endif
#ifndef TOUCH_THRESHOLD_GP09
#define TOUCH_THRESHOLD_GP09 0
#endif
#ifndef TOUCH_THRESHOLD_GP10
#define TOUCH_THRESHOLD_GP10 0
#endif
#ifndef TOUCH_THRESHOLD_GP11
#define TOUCH_THRESHOLD_GP11 0
#endif
#ifndef TOUCH_THRESHOLD_GP12
#define TOUCH_THRESHOLD_GP12 0
#endif
#ifndef TOUCH_THRESHOLD_GP13
#define TOUCH_THRESHOLD_GP13 0
#endif
#ifndef TOUCH_THRESHOLD_GP14
#define TOUCH_THRESHOLD_GP14 0
#endif
#ifndef TOUCH_THRESHOLD_GP15
#define TOUCH_THRESHOLD_GP15 0
#endif
#ifndef TOUCH_THRESHOLD_GP16
#define TOUCH_THRESHOLD_GP16 0
#endif
#ifndef TOUCH_THRESHOLD_GP17
#define TOUCH_THRESHOLD_GP17 0
#endif
#ifndef TOUCH_THRESHOLD_GP18
#define TOUCH_THRESHOLD_GP18 0
#endif
#ifndef TOUCH_THRESHOLD_GP19
#define TOUCH_THRESHOLD_GP19 0
#endif
#ifndef TOUCH_THRESHOLD_GP20
#define TOUCH_THRESHOLD_GP20 0
#endif
#ifndef TOUCH_THRESHOLD_GP21
#define TOUCH_THRESHOLD_GP21 0
#endif
#ifndef TOUCH_THRESHOLD_GP22
#define TOUCH_THRESHOLD_GP22 0
#endif
#ifndef TOUCH_THRESHOLD_GP23
#define TOUCH_THRESHOLD_GP23 0
#endif
#ifndef TOUCH_THRESHOLD_GP24
#define TOUCH_THRESHOLD_GP24 0
#endif
#ifndef TOUCH_THRESHOLD_GP25
#define TOUCH_THRESHOLD_GP25 0
#endif
#ifndef TOUCH_THRESHOLD_GP26
#define TOUCH_THRESHOLD_GP26 0
#endif
#ifndef TOUCH_THRESHOLD_GP27
#define TOUCH_THRESHOLD_GP27 0
#endif
#ifndef TOUCH_THRESHOLD_GP28
#define TOUCH_THRESHOLD_GP28 0
#endif
#ifndef TOUCH_THRESHOLD_GP29
#define TOUCH_THRESHOLD_GP29 0
#endif

static const uint32_t defaultTouchThresholds[NUM_BANK0_GPIOS] = {
    TOUCH_THRESHOLD_GP00, TOUCH_THRESHOLD_GP01, TOUCH_THRESHOLD_GP02, TOUCH_THRESHOLD_GP03,
    TOUCH_THRESHOLD_GP04, TOUCH_THRESHOLD_GP05, TOUCH_THRESHOLD_GP06, TOUCH_THRESHOLD_GP07,
    TOUCH_THRESHOLD_GP08, TOUCH_THRESHOLD_GP09, TOUCH_THRESHOLD_GP10, TOUCH_THRESHOLD_GP11,
    TOUCH_THRESHOLD_GP12, TOUCH_THRESHOLD_GP13, TOUCH_THRESHOLD_GP14, TOUCH_THRESHOLD_GP15,
    TOUCH_THRESHOLD_GP16, TOUCH_THRESHOLD_GP17, TOUCH_THRESHOLD_GP18, TOUCH_THRESHOLD_GP19,
    TOUCH_THRESHOLD_GP20, TOUCH_THRESHOLD_GP21, TOUCH_THRESHOLD_GP22, TOUCH_THRESHOLD_GP23,
    TOUCH_THRESHOLD_GP24, TOUCH_THRESHOLD_GP25, TOUCH_THRESHOLD_GP26, TOUCH_THRESHOLD_GP27,
    TOUCH_THRESHOLD_GP28, TOUCH_THRESHOLD_GP29
};

TouchGpio::TouchGpio()
    : pio(nullptr), smOffset(0), mask(0)
{
    for (uint32_t i = 0; i < NUM_BANK0_GPIOS; i++)
    {
        smForPin[i] = 0xFF;
        thresholdOn[i] = 0;
        thresholdOff[i] = 0;
        active[i] = false;
        touched[i] = false;
    }
}

void TouchGpio::setup(Mask_t touchMask)
{
    mask = touchMask & ((1u << NUM_BANK0_GPIOS) - 1u);
    if (mask == 0) return;

    // The ws2812 LED driver uses pio0, so run touch sensing on pio1.
    pio = pio1;
    smOffset = pio_add_program(pio, &capsense_program);

    for (Pin_t pin = 0; pin < (Pin_t)NUM_BANK0_GPIOS; pin++)
    {
        if (!(mask & (1u << pin))) continue;

        int sm = pio_claim_unused_sm(pio, true);
        if (sm < 0) continue;   // out of state machines

        smForPin[pin] = (uint8_t)sm;

        // No internal pulls: the pad's ~1M ohm resistor to ground provides the
        // discharge path, and the PIO owns the pin for its full drive cycle.
        gpio_set_pulls(pin, false, false);
        pio_gpio_init(pio, pin);

        pio_sm_config c = capsense_program_get_default_config(smOffset);
        sm_config_set_set_pins(&c, pin, 1);    // 'set pins' drives the pad
        sm_config_set_jmp_pin(&c, pin);        // 'jmp pin' watches the pad

        pio_sm_clear_fifos(pio, sm);
        pio_sm_init(pio, sm, smOffset, &c);
        pio_sm_set_enabled(pio, sm, true);
    }

    calibrate();
}

void TouchGpio::calibrate()
{
    for (Pin_t pin = 0; pin < (Pin_t)NUM_BANK0_GPIOS; pin++)
    {
        if (smForPin[pin] == 0xFF) continue;

        if (defaultTouchThresholds[pin] > 0)
        {
            // Fixed threshold from the board config: no calibration needed.
            thresholdOn[pin] = defaultTouchThresholds[pin];
            thresholdOff[pin] = thresholdOn[pin] - (thresholdOn[pin] * TOUCH_HYSTERESIS / 100);
            active[pin] = true;
            touched[pin] = false;
            continue;
        }

        // Idle baseline: the lowest of several samples is robust to noise and
        // to a finger resting on the pad during boot.
        uint32_t baseline = 0xFFFFFFFF;
        bool saturated = false;
        for (uint32_t i = 0; i < TOUCH_CAL_SAMPLES; i++)
        {
            uint32_t v = readPin(pin);
            if (v >= TOUCH_TIMEOUT) { saturated = true; break; }
            if (v < baseline) baseline = v;
        }

        // The pad never discharged: no resistor or pad connected. Disable it.
        if (saturated || baseline == 0xFFFFFFFF)
        {
            active[pin] = false;
            continue;
        }

        thresholdOn[pin] = baseline + (baseline * TOUCH_MARGIN / 100);
        thresholdOff[pin] = baseline + (baseline * TOUCH_HYSTERESIS / 100);
        active[pin] = true;
        touched[pin] = false;
    }
}

Mask_t TouchGpio::scan()
{
    if (mask == 0) return 0;

    Mask_t result = 0;
    for (Pin_t pin = 0; pin < (Pin_t)NUM_BANK0_GPIOS; pin++)
    {
        if (smForPin[pin] == 0xFF || !active[pin]) continue;

        uint32_t v = readPin(pin);
        if (v >= TOUCH_TIMEOUT) continue;   // saturated: keep the previous state

        // Threshold with release hysteresis so a noisy reading near the edge
        // can't chatter the key.
        if (!touched[pin] && v > thresholdOn[pin])
            touched[pin] = true;
        else if (touched[pin] && v < thresholdOff[pin])
            touched[pin] = false;

        if (touched[pin]) result |= (1u << pin);
    }
    return result;
}

bool TouchGpio::isTouched(Pin_t pin)
{
    if (pin < 0 || pin >= (Pin_t)NUM_BANK0_GPIOS) return false;
    if (smForPin[pin] == 0xFF || !active[pin]) return false;

    uint32_t v = readPin(pin);
    if (v >= TOUCH_TIMEOUT) return false;
    return v > thresholdOn[pin];
}

uint32_t TouchGpio::readPin(Pin_t pin)
{
    if (smForPin[pin] == 0xFF) return TOUCH_TIMEOUT;

    uint sm = smForPin[pin];
    pio_sm_put_blocking(pio, sm, TOUCH_TIMEOUT);   // trigger a measurement
    uint32_t remaining = pio_sm_get_blocking(pio, sm);
    if (remaining == 0) return TOUCH_TIMEOUT;      // 0 sentinel: never discharged
    return TOUCH_TIMEOUT - remaining;
}
