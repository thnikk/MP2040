#ifndef _MAINMENUSCREEN_H_
#define _MAINMENUSCREEN_H_

#include "GPGFX_UI_widgets.h"
#include "GPGFX_UI_types.h"

// On-device mini menu. Opened by holding the configured key combo (~500ms);
// navigated with the configured menu pins (dpad + select/back). The
// DisplayController translates key presses into these navigation calls, so the
// screen itself stays input-agnostic.
class MainMenuScreen : public GPScreen {
	public:
		MainMenuScreen() {}
		MainMenuScreen(GPGFX* renderer) { setRenderer(renderer); }
		virtual int8_t update();
		virtual void init();
		virtual void shutdown();

		// Navigation actions fed by the DisplayController (UP/DOWN/LEFT/RIGHT
		// move/scroll, SELECT triggers, BACK exits). Returns a screen change
		// request (1 = back to buttons).
		int8_t handleNavigation(uint8_t action);
	protected:
		virtual void drawScreen();
	private:
		enum MenuAction {
			MENU_ACTION_UP = 0,
			MENU_ACTION_DOWN,
			MENU_ACTION_LEFT,
			MENU_ACTION_RIGHT,
			MENU_ACTION_SELECT,
			MENU_ACTION_BACK,
		};

		std::vector<MenuEntry> menu;
		uint16_t menuIndex = 0;
		uint32_t lastActivity = 0;

		void rebuildMenu();
		int8_t onSelect();
		void onLeft();
		void onRight();
		void applySpinnerValue(uint16_t index, uint32_t value);
		void saveAndPublish();
		uint32_t currentProfile() const;
		uint32_t currentLedMode() const;
		uint32_t currentBrightness() const;
};

#endif