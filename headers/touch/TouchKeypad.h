#ifndef _TOUCH_KEYPAD_H_
#define _TOUCH_KEYPAD_H_

#include "keymask.h"

// Maps FT6336 touch points onto the board's matrix key grid.
//
// The board is a 4x3 matrix in software only: the panel has no switch matrix,
// so a touch cell synthesizes the same linear key index the matrix scanner
// would produce (row * MATRIX_COLS + col). Everything downstream (debounce,
// keycodes, gamepad/MIDI maps, profiles, LED colors) is unchanged.
class TouchKeypad
{
public:
	TouchKeypad(TouchKeypad const&) = delete;
	void operator=(TouchKeypad const&) = delete;
	static TouchKeypad& getInstance()
	{
		static TouchKeypad instance;
		return instance;
	}

	// Reset/configure the touch controller and initialize the grid.
	bool setup();

	// Current pressed-cell mask, throttled to the controller's report rate.
	KeyMask scan();

	bool isPresent() const { return initialized; }

private:
	TouchKeypad() {}

	KeyMask state;
	uint32_t lastScanUs = 0;
	bool initialized = false;
};

#endif
