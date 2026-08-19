#include "touch/TouchGpio.h"

#include "BoardConfig.h"
#include "storagemanager.h"

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
    : pio(nullptr), smOffset(0), smCount(0), mask(0)
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

void TouchGpio::setup(GpioMask touchMask, bool useStored)
{
    mask = touchMask & ((1u << NUM_BANK0_GPIOS) - 1u);
    if (mask == 0) return;

    // All pads are multiplexed over a shared pool of state machines on pio1.
    // The ws2812 LED driver uses pio0, so pio1 is entirely ours. Claiming is
    // non-fatal: a board that exhausts the pool (more pads than SMs) degrades
    // gracefully - pads just share SMs - and can never panic the device and
    // kill USB enumeration.
    pio = pio1;
    smOffset = pio_add_program(pio, &capsense_program);

    // Claim up to 4 SMs. Non-fatal so we never halt the CPU on a tight board.
    uint32_t pool[NUM_BANK0_GPIOS];
    smCount = 0;
    for (uint32_t i = 0; i < 4; i++)
    {
        int sm = pio_claim_unused_sm(pio, false);
        if (sm < 0) break;
        pool[smCount++] = (uint32_t)sm;
    }
    if (smCount == 0) return;   // no SMs at all: nothing can be measured

    // Initialize each pool SM once so its program counter sits at the program
    // start (`begin`), waiting for a trigger. readPin() only retargets the
    // SM's pins via pio_sm_set_config, which leaves the PC at `begin`; without
    // this init the SM never runs the capsense program and get_blocking would
    // hang forever.
    for (uint32_t i = 0; i < smCount; i++)
    {
        pio_sm_config init_cfg = capsense_program_get_default_config(smOffset);
        pio_sm_clear_fifos(pio, pool[i]);
        pio_sm_init(pio, pool[i], smOffset, &init_cfg);
        pio_sm_set_enabled(pio, pool[i], true);
    }

    // Round-robin bind each pad to a pool SM. All pads are configured on the
    // GPIO now; the SM is retargeted to the right pin at read time.
    uint32_t slot = 0;
    for (Pin_t pin = 0; pin < (Pin_t)NUM_BANK0_GPIOS; pin++)
    {
        if (!(mask & (1u << pin))) continue;

        smForPin[pin] = (uint8_t)pool[slot % smCount];
        slot++;

        // No internal pulls: the pad's ~1M ohm resistor to ground provides the
        // discharge path, and the PIO owns the pin for its full drive cycle.
        gpio_set_pulls(pin, false, false);
        pio_gpio_init(pio, pin);
    }

    if (useStored)
        loadCalibration();
    else
        calibrate();
}

void TouchGpio::calibrate()
{
    for (Pin_t pin = 0; pin < (Pin_t)NUM_BANK0_GPIOS; pin++)
    {
        if (smForPin[pin] == 0xFF) continue;
        calibratePin(pin);
    }

    // Persist the freshly-calibrated thresholds so a web config reboot (where
    // the pad that triggered the reboot is still being held) can load them
    // instead of re-sampling mid-touch. Fixed-threshold pads are board
    // properties and don't need storing. save() only commits to flash when
    // the config actually changed, so a stable calibration is a no-op.
    Config& config = Storage::getInstance().getConfig();
    config.touchThresholdsOn_count = 0;
    config.touchThresholdsOff_count = 0;
    for (Pin_t pin = 0; pin < (Pin_t)NUM_BANK0_GPIOS; pin++)
    {
        // Clear stale entries first so a pad that is no longer active (e.g.
        // saturated / missing) doesn't keep an outdated stored threshold.
        config.touchThresholdsOn[pin] = 0;
        config.touchThresholdsOff[pin] = 0;
    }
    for (Pin_t pin = 0; pin < (Pin_t)NUM_BANK0_GPIOS; pin++)
    {
        if (smForPin[pin] == 0xFF || !active[pin]) continue;
        if (defaultTouchThresholds[pin] > 0) continue;
        config.touchThresholdsOn[pin] = thresholdOn[pin];
        config.touchThresholdsOff[pin] = thresholdOff[pin];
        config.touchThresholdsOn_count = pin + 1;
        config.touchThresholdsOff_count = pin + 1;
    }
    Storage::getInstance().save(true);
}

void TouchGpio::loadCalibration()
{
    const Config& config = Storage::getInstance().getConfig();
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

        if (pin < (Pin_t)config.touchThresholdsOn_count && config.touchThresholdsOn[pin] > 0)
        {
            // Stored calibration from the last normal boot (pads idle).
            thresholdOn[pin] = config.touchThresholdsOn[pin];
            thresholdOff[pin] = config.touchThresholdsOff[pin];
            active[pin] = true;
            touched[pin] = false;
        }
        else
        {
            // No stored entry (a pad added after the last save, or an old
            // config): calibrate fresh.
            calibratePin(pin);
        }
    }
}

void TouchGpio::calibratePin(Pin_t pin)
{
    if (defaultTouchThresholds[pin] > 0)
    {
        // Fixed threshold from the board config: no calibration needed.
        thresholdOn[pin] = defaultTouchThresholds[pin];
        thresholdOff[pin] = thresholdOn[pin] - (thresholdOn[pin] * TOUCH_HYSTERESIS / 100);
        active[pin] = true;
        touched[pin] = false;
        return;
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
        return;
    }

    thresholdOn[pin] = baseline + (baseline * TOUCH_MARGIN / 100);
    thresholdOff[pin] = baseline + (baseline * TOUCH_HYSTERESIS / 100);
    active[pin] = true;
    touched[pin] = false;
}

GpioMask TouchGpio::scan()
{
    if (mask == 0) return 0;

    GpioMask result = 0;
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

uint32_t TouchGpio::readPin(Pin_t pin)
{
    if (smForPin[pin] == 0xFF) return TOUCH_TIMEOUT;

    const uint sm = smForPin[pin];

    // Retarget this SM's 'set pins' / 'jmp pin' to the pad being read. The
    // pads multiplex over a shared SM pool, so the SM is pointed at a specific
    // pin for the duration of one measurement. pio_sm_set_config safely
    // disables the SM, applies the config, and re-enables it.
    pio_sm_config c = capsense_program_get_default_config(smOffset);
    sm_config_set_set_pins(&c, pin, 1);    // 'set pins' drives the pad
    sm_config_set_jmp_pin(&c, pin);        // 'jmp pin' watches the pad
    pio_sm_set_config(pio, sm, &c);

    pio_sm_put_blocking(pio, sm, TOUCH_TIMEOUT);   // trigger a measurement
    uint32_t remaining = pio_sm_get_blocking(pio, sm);
    if (remaining == 0) return TOUCH_TIMEOUT;      // 0 sentinel: never discharged
    return TOUCH_TIMEOUT - remaining;
}

GpioMask TouchGpio::readValues(GpioMask pins, uint32_t* out)
{
    if (mask == 0) return 0;

    GpioMask result = 0;
    for (Pin_t pin = 0; pin < (Pin_t)NUM_BANK0_GPIOS; pin++)
    {
        if (!(pins & (1u << pin))) continue;
        out[pin] = 0;
        if (smForPin[pin] == 0xFF || !active[pin]) continue;
        out[pin] = readPin(pin);
        result |= (1u << pin);
    }
    return result;
}
