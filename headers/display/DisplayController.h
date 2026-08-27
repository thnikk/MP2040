#ifndef _DISPLAY_CONTROLLER_H_
#define _DISPLAY_CONTROLLER_H_

#include <stdint.h>
#include "pico/time.h"
#include "keymask.h"
#include "config.pb.h"
#include "enums.pb.h"
#include "GPGFX.h"
#include "GPGFX_UI_widgets.h"
#include "GPGFX_UI_screens.h"

// Core-1 display subsystem. Sibling of LedController: owned by MP2040Aux and
// updated every core-1 loop. Owns the GPGFX renderer (including its 1KB
// framebuffer, which stays a member, not on the 4KB core-1 stack) and the
// screen state machine.
//
// Screen flow: SPLASH -> BUTTONS -> (combo held ~500ms) MAIN_MENU / REMAP,
// and BUTTONS -> SAVER after the inactivity timeout. The mini menu / remap set
// Storage.menuActive so core 0 stops sending key presses to USB while open.
class DisplayController {
public:
	DisplayController();
	~DisplayController();

	void setup();
	void update();

	bool isDisplayPresent() { return displayPresent; }

private:
	enum DisplayMode {
		SPLASH = 0,
		BUTTONS,
		MAIN_MENU,
		REMAP,
		SAVER,
	};

	// navigation actions shared by the menu/remap screens
	enum NavAction {
		NAV_UP = 0,
		NAV_DOWN,
		NAV_LEFT,
		NAV_RIGHT,
		NAV_SELECT,
		NAV_BACK,
	};

	void setMode(DisplayMode mode);
	void destroyScreen();
	void updateScreen();
	bool detectDisplay();

	// input handling
	void processCombo();
	void processNav();
	bool navHeld(uint8_t action);
	bool menuComboHeld();

	DisplayOptions& opts() { return Storage::getInstance().getDisplayOptions(); }

	GPGFX gfx;
	bool displayPresent = false;
	bool powered = true;

	DisplayMode mode = BUTTONS;
	GPScreen* screen = nullptr;

	KeyMask prevKeyState;
	uint32_t lastActivity = 0;
	uint32_t comboHeldSince = 0;
	bool comboArmed = false;

	// nav edge tracking
	bool navPrev[6] = {};

	uint32_t lastSaverCheck = 0;
	uint32_t lastModeChange = 0;
};

#endif