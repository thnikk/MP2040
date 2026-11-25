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

	// A horizontal swipe on the bottom status bar requests a profile change:
	// +1 for a left swipe (next profile), -1 for a right swipe (previous), and
	// 0 when there is nothing pending. The caller consumes and applies it so
	// profile changes still go through the normal hotkey path.
	int8_t takeProfileSwipe();

	bool isPresent() const { return initialized; }

private:
	TouchKeypad() {}

	KeyMask state;
	uint32_t lastScanUs = 0;
	bool initialized = false;
	// Bottom-bar gesture tracking. A touch that starts in the footer owns the
	// gesture until it lifts, and its points are kept out of the key grid.
	bool swipeActive = false;
	bool swipeTriggered = false;
	uint16_t swipeStartX = 0;
	uint16_t swipeStartY = 0;
	int8_t pendingProfileStep = 0;
};

#endif
