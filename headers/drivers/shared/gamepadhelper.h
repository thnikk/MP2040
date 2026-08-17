#ifndef _GAMEPAD_HELPER_H_
#define _GAMEPAD_HELPER_H_

#include "storagemanager.h"
#include "types.h"
#include "enums.pb.h"

//
// MP2040 Gamepad helper
//
// Shared between the gamepad input drivers (XInput, Switch Pro). Each pin
// can be assigned a gamepad control mask (KeyMapping.gamepadMasks): the dpad
// directions in bits 0-3 and the buttons (B1..A2) in bits 4-17. Multiple
// controls OR'd into one pin's mask fire simultaneously when the pin is held.
// The assembled state plus the SOCD-cleaned dpad is what the drivers
// serialize into their USB reports.
//

// Per-pin gamepad control masks (KeyMapping.gamepadMasks). dpad directions
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

// Gamepad state assembled from key state: current dpad directions plus held
// buttons. Analog sticks and triggers stay at their mid/off values (no analog
// inputs in MP2040); the drivers hard-code centered sticks / digital triggers.
struct GamepadState
{
	uint8_t dpad {0};
	uint16_t buttons {0};
};

// DPAD direction history for the SOCD cleaner. Values index dpadMasks.
enum DpadDirection
{
	DIRECTION_NONE = 0,
	DIRECTION_UP,
	DIRECTION_DOWN,
	DIRECTION_LEFT,
	DIRECTION_RIGHT
};

static const uint8_t dpadMasks[] =
{
	GAMEPAD_MASK_UP,
	GAMEPAD_MASK_DOWN,
	GAMEPAD_MASK_LEFT,
	GAMEPAD_MASK_RIGHT,
};

static inline uint8_t getMaskFromDirection(DpadDirection direction)
{
	return dpadMasks[direction - 1];
}

// Assemble the raw (un-cleaned) gamepad state from the current key state.
// Each pressed pin contributes the controls in its gamepadMasks entry: the
// low nibble becomes dpad directions, the button bits (>> 4) become buttons.
static inline void buildGamepadState(GamepadState& state)
{
	const KeyMapping& keyMapping = Storage::getInstance().getKeyMapping();
	const KeyMask& keyState = Storage::getInstance().keyState;

	state.buttons = 0;
	state.dpad = 0;

	for (Pin_t pin = 0; pin < (Pin_t)keyMapping.gamepadMasks_count; pin++) {
		if (!keyState.test(pin)) continue;
		const uint32_t mask = keyMapping.gamepadMasks[pin];
		if (mask == 0) continue;
		state.dpad |= mask & GAMEPAD_MASK_DPAD;
		state.buttons |= (mask >> 4) & 0x3FFFu;
	}
}

/**
 * @brief Run SOCD cleaning against a D-pad value.
 *
 * Ported from GP2040-CE (src/gamepad/GamepadState.cpp). Each axis keeps its
 * own last-pressed history so the second/first-input-priority modes can reselect.
 *
 * @param mode The SOCD cleaning mode.
 * @param dpad The GamepadState.dpad value.
 * @return uint8_t The clean D-pad value.
 */
static inline uint8_t runSOCDCleaner(SOCDMode mode, uint8_t dpad)
{
	if (mode == SOCD_MODE_BYPASS) {
		return dpad;
	}

	static DpadDirection lastUD = DIRECTION_NONE;
	static DpadDirection lastLR = DIRECTION_NONE;
	uint8_t newDpad = 0;

	switch (dpad & (GAMEPAD_MASK_UP | GAMEPAD_MASK_DOWN)) {
		case (GAMEPAD_MASK_UP | GAMEPAD_MASK_DOWN):
			if (mode == SOCD_MODE_UP_PRIORITY) {
				newDpad |= GAMEPAD_MASK_UP;
				lastUD = DIRECTION_UP;
			} else if (mode == SOCD_MODE_SECOND_INPUT_PRIORITY && lastUD != DIRECTION_NONE)
				newDpad |= (lastUD == DIRECTION_UP) ? GAMEPAD_MASK_DOWN : GAMEPAD_MASK_UP;
			else if (mode == SOCD_MODE_FIRST_INPUT_PRIORITY && lastUD != DIRECTION_NONE)
				newDpad |= (lastUD == DIRECTION_UP) ? GAMEPAD_MASK_UP : GAMEPAD_MASK_DOWN;
			else
				lastUD = DIRECTION_NONE;
			break;

		case GAMEPAD_MASK_UP:
			newDpad |= GAMEPAD_MASK_UP;
			lastUD = DIRECTION_UP;
			break;

		case GAMEPAD_MASK_DOWN:
			newDpad |= GAMEPAD_MASK_DOWN;
			lastUD = DIRECTION_DOWN;
			break;

		default:
			lastUD = DIRECTION_NONE;
			break;
	}

	switch (dpad & (GAMEPAD_MASK_LEFT | GAMEPAD_MASK_RIGHT)) {
		case (GAMEPAD_MASK_LEFT | GAMEPAD_MASK_RIGHT):
			if (mode == SOCD_MODE_SECOND_INPUT_PRIORITY && lastLR != DIRECTION_NONE)
				newDpad |= (lastLR == DIRECTION_LEFT) ? GAMEPAD_MASK_RIGHT : GAMEPAD_MASK_LEFT;
			else if (mode == SOCD_MODE_FIRST_INPUT_PRIORITY && lastLR != DIRECTION_NONE)
				newDpad |= (lastLR == DIRECTION_LEFT) ? GAMEPAD_MASK_LEFT : GAMEPAD_MASK_RIGHT;
			else
				lastLR = DIRECTION_NONE;
			break;

		case GAMEPAD_MASK_LEFT:
			newDpad |= GAMEPAD_MASK_LEFT;
			lastLR = DIRECTION_LEFT;
			break;

		case GAMEPAD_MASK_RIGHT:
			newDpad |= GAMEPAD_MASK_RIGHT;
			lastLR = DIRECTION_RIGHT;
			break;

		default:
			lastLR = DIRECTION_NONE;
			break;
	}

	return newDpad;
}

#endif // _GAMEPAD_HELPER_H_