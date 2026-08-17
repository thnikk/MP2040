#ifndef _GAMEPAD_MAPPING_H_
#define _GAMEPAD_MAPPING_H_

#include <stdint.h>

//
// MP2040 Gamepad control constants
//
// Shared by the firmware gamepad logic (gamepadhelper.h), the board default
// defines (GAMEPAD_IDXxx in BoardConfig.h) and the web configurator. Kept
// dependency-free so the board config and the storage manager can reference
// them before anything else is set up.
//

// Sentinel for BoardConfig.h GAMEPAD_GPxx / GAMEPAD_IDXxx defaults: a value of -1
// means the pin has no default gamepad assignment (unmapped until configured).
#define GAMEPAD_UNMAPPED (-1)

// Per-pin gamepad control masks (GamepadMapping.masks). dpad directions
// occupy bits 0-3; buttons occupy bits 4-17 so a single 32-bit mask can hold
// a dpad direction and buttons at the same time.
#define GAMEPAD_PIN_MASK_UP    (1U << 0)
#define GAMEPAD_PIN_MASK_DOWN  (1U << 1)
#define GAMEPAD_PIN_MASK_LEFT  (1U << 2)
#define GAMEPAD_PIN_MASK_RIGHT (1U << 3)
#define GAMEPAD_PIN_MASK_B1    (1U << 4)
#define GAMEPAD_PIN_MASK_B2    (1U << 5)
#define GAMEPAD_PIN_MASK_B3    (1U << 6)
#define GAMEPAD_PIN_MASK_B4    (1U << 7)
#define GAMEPAD_PIN_MASK_L1    (1U << 8)
#define GAMEPAD_PIN_MASK_R1    (1U << 9)
#define GAMEPAD_PIN_MASK_L2    (1U << 10)
#define GAMEPAD_PIN_MASK_R2    (1U << 11)
#define GAMEPAD_PIN_MASK_S1    (1U << 12)
#define GAMEPAD_PIN_MASK_S2    (1U << 13)
#define GAMEPAD_PIN_MASK_L3    (1U << 14)
#define GAMEPAD_PIN_MASK_R3    (1U << 15)
#define GAMEPAD_PIN_MASK_A1    (1U << 16)
#define GAMEPAD_PIN_MASK_A2    (1U << 17)

#define GAMEPAD_PIN_MASK_DPAD     (GAMEPAD_PIN_MASK_UP | GAMEPAD_PIN_MASK_DOWN | GAMEPAD_PIN_MASK_LEFT | GAMEPAD_PIN_MASK_RIGHT)
#define GAMEPAD_PIN_MASK_BUTTONS  0x003FFF0u

// Gamepad state masks (assembled GamepadState fields, not per-pin). The dpad
// and button fields are separate so they can share bit numbering.
#define GAMEPAD_MASK_UP    (1U << 0)
#define GAMEPAD_MASK_DOWN  (1U << 1)
#define GAMEPAD_MASK_LEFT  (1U << 2)
#define GAMEPAD_MASK_RIGHT (1U << 3)

#define GAMEPAD_MASK_B1    (1U << 0)
#define GAMEPAD_MASK_B2    (1U << 1)
#define GAMEPAD_MASK_B3    (1U << 2)
#define GAMEPAD_MASK_B4    (1U << 3)
#define GAMEPAD_MASK_L1    (1U << 4)
#define GAMEPAD_MASK_R1    (1U << 5)
#define GAMEPAD_MASK_L2    (1U << 6)
#define GAMEPAD_MASK_R2    (1U << 7)
#define GAMEPAD_MASK_S1    (1U << 8)
#define GAMEPAD_MASK_S2    (1U << 9)
#define GAMEPAD_MASK_L3    (1U << 10)
#define GAMEPAD_MASK_R3    (1U << 11)
#define GAMEPAD_MASK_A1    (1U << 12)
#define GAMEPAD_MASK_A2    (1U << 13)

#define GAMEPAD_MASK_DPAD  (GAMEPAD_MASK_UP | GAMEPAD_MASK_DOWN | GAMEPAD_MASK_LEFT | GAMEPAD_MASK_RIGHT)

#endif // _GAMEPAD_MAPPING_H_