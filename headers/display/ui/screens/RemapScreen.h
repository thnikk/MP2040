#ifndef _REMAPSCREEN_H_
#define _REMAPSCREEN_H_

#include "GPGFX_UI_widgets.h"

// On-device key remapping. Lists the board's key indices; selecting one enters
// "assign" mode where the keycode is changed with left/right (1) and up/down
// (8). Back saves the active profile's key mapping and returns to the buttons
// screen. Only the keycode is edited (modifier masks are left alone for v1).
class RemapScreen : public GPScreen {
	public:
		RemapScreen() {}
		RemapScreen(GPGFX* renderer) { setRenderer(renderer); }
		virtual int8_t update();
		virtual void init();
		virtual void shutdown();

		int8_t handleNavigation(uint8_t action);
	protected:
		virtual void drawScreen();
	private:
		enum { ACTION_UP = 0, ACTION_DOWN, ACTION_LEFT, ACTION_RIGHT, ACTION_SELECT, ACTION_BACK };

		uint32_t selectedPin = 0;
		bool assigning = false;
		void save();
};

#endif