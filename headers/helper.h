#ifndef _HELPER_H_
#define _HELPER_H_

#include "pico/time.h"
#include <stdint.h>

// Gamepad control masks, available before BoardConfig.h so boards can use
// GAMEPAD_PIN_MASK_* in their GAMEPAD_IDXxx default defines.
#include "gamepadmapping.h"

#include "BoardConfig.h"

// Time helpers
extern uint32_t getMillis();
extern uint64_t getMicro();

// MP2040 Board Config (64 character limit)
#ifndef MP2040_BOARDCONFIG
#define MP2040_BOARDCONFIG "Unknown"
#endif

#ifndef BOARD_EXTRA_PINS
#define BOARD_EXTRA_PINS {}
#endif

static inline bool isValidPin(int32_t pin) {
    int32_t numBank0GPIOS = NUM_BANK0_GPIOS;
    return pin >= 0 && pin < numBank0GPIOS;
}

#endif
