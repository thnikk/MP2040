#include "GPGFX_UI.h"

#include <cstring>

#include "drivermanager.h"

GPGFX_UI::GPGFX_UI() {
}

KeyMask GPGFX_UI::getKeyState() {
	return Storage::getInstance().getKeyState();
}

bool GPGFX_UI::pressedPin(uint32_t index) {
	return Storage::getInstance().getKeyState().test(index);
}

bool GPGFX_UI::pressedGamepad(uint32_t stateMask) {
	Storage& s = Storage::getInstance();
	const KeyMask keyState = s.getKeyState();

	// Translate a gamepad state mask (dpad bits 0-3, buttons bits 0-13) into
	// the per-pin mapping space: dpad shares bit positions, buttons live at
	// bits 4-17 in the pin mapping.
	const uint32_t stateButtons = stateMask & ~(uint32_t)GAMEPAD_MASK_DPAD;
	const uint32_t pinMask = (stateMask & GAMEPAD_MASK_DPAD) | (stateButtons << 4);

	for (uint32_t pin = 0; pin < MAX_KEYS; pin++) {
		if (keyState.test(pin) && (s.getGamepadMask(pin) & pinMask) != 0)
			return true;
	}
	return false;
}

Config& GPGFX_UI::getConfig() {
	return Storage::getInstance().getConfig();
}

DisplayOptions& GPGFX_UI::getDisplayOptions() {
	return Storage::getInstance().getDisplayOptions();
}

InputMode GPGFX_UI::getInputMode() {
	return DriverManager::getInstance().getInputMode();
}

uint16_t GPGFX_UI::map(uint16_t x, uint16_t in_min, uint16_t in_max, uint16_t out_min, uint16_t out_max) {
	return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}