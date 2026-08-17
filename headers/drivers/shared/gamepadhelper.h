#ifndef _GAMEPAD_HELPER_H_
#define _GAMEPAD_HELPER_H_

#include "storagemanager.h"
#include "gamepadmapping.h"
#include "types.h"
#include "enums.pb.h"

//
// MP2040 Gamepad helper
//
// Shared between the gamepad input drivers (XInput, Switch Pro). Each pin
// can be assigned a gamepad control mask (Config.gamepadMapping.masks, see
// GamepadMapping in config.proto): the dpad directions in bits 0-3 and the
// buttons (B1..A2) in bits 4-17. Multiple controls OR'd into one pin's mask
// fire simultaneously when the pin is held. The assembled state plus the
// SOCD-cleaned dpad is what the drivers serialize into their USB reports.
//

// Analog stick neutral / range. Sticks are unsigned 16-bit with center at
// GAMEPAD_JOYSTICK_MID; the drivers scale to their own encodings.
#define GAMEPAD_JOYSTICK_MIN 0
#define GAMEPAD_JOYSTICK_MID 0x7FFF
#define GAMEPAD_JOYSTICK_MAX 0xFFFF

// Gamepad state assembled from key state: current dpad directions plus held
// buttons, plus optional analog stick position (from a touch ring or future
// analog source). Analog sticks default to center so boards without an analog
// source report a neutral stick (no behavior change).
struct GamepadState
{
	uint8_t dpad {0};
	uint16_t buttons {0};

	// Left / right analog sticks. Center = GAMEPAD_JOYSTICK_MID. Only the
	// configured target stick is driven by the ring; the other stays center.
	uint16_t lx {GAMEPAD_JOYSTICK_MID};
	uint16_t ly {GAMEPAD_JOYSTICK_MID};
	uint16_t rx {GAMEPAD_JOYSTICK_MID};
	uint16_t ry {GAMEPAD_JOYSTICK_MID};
	// True when a touch ring is driving the analog stick this frame.
	bool ringActive {false};
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
// Each pressed pin contributes the controls in its gamepad mapping entry: the
// low nibble becomes dpad directions, the button bits (>> 4) become buttons.
static inline void buildGamepadState(GamepadState& state)
{
	const KeyMask& keyState = Storage::getInstance().keyState;

	state.buttons = 0;
	state.dpad = 0;

	const uint32_t keyCount = Storage::getInstance().getKeyCount();
	for (Pin_t pin = 0; pin < (Pin_t)keyCount; pin++) {
		if (!keyState.test(pin)) continue;
		const uint32_t mask = Storage::getInstance().getGamepadMask(pin);
		if (mask == 0) continue;
		state.dpad |= mask & GAMEPAD_MASK_DPAD;
		state.buttons |= (mask >> 4) & 0x3FFFu;
	}

	// Sticks stay centered unless a ring drives the configured stick.
	state.lx = GAMEPAD_JOYSTICK_MID;
	state.ly = GAMEPAD_JOYSTICK_MID;
	state.rx = GAMEPAD_JOYSTICK_MID;
	state.ry = GAMEPAD_JOYSTICK_MID;
	state.ringActive = false;
}

// Apply a touch-ring stick vector (lx/ly in -1..1, axis-aligned so +x = right,
// +y = up) to the configured analog stick in `state`. When `active` is false
// (no ring touch) the stick is left at its center. Normalizes magnitude so a
// diagonal reading doesn't exceed the stick range.
static inline void applyRingToStick(GamepadState& state, float lx, float ly, bool active, bool rightStick)
{
	uint16_t& x = rightStick ? state.rx : state.lx;
	uint16_t& y = rightStick ? state.ry : state.ly;
	state.ringActive = active;

	if (!active) {
		x = GAMEPAD_JOYSTICK_MID;
		y = GAMEPAD_JOYSTICK_MID;
		return;
	}

	// Clamp the (already unit-ish) vector and map -1..1 to 16-bit stick range.
	const float mid = (float)GAMEPAD_JOYSTICK_MID;
	const float half = mid;
	float cx = lx < -1.0f ? -1.0f : (lx > 1.0f ? 1.0f : lx);
	float cy = ly < -1.0f ? -1.0f : (ly > 1.0f ? 1.0f : ly);
	x = (uint16_t)(mid + cx * half);
	y = (uint16_t)(mid - cy * half);   // screen Y up => stick Y inverted
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