#ifndef _MAINMENUSCREEN_H_
#define _MAINMENUSCREEN_H_

#include "GPGFX_UI_widgets.h"
#include "GPGFX_UI_types.h"
#include "enums.pb.h"
#include "storagemanager.h"

// On-device mini menu (ported from GP2040-th). Opened by holding the configured
// key combo (~500ms); navigated with the configured menu pins (dpad + B1/B2),
// fed in by the DisplayController as handleNavigation() actions.
//
// Structure: nested submenus via MenuEntry.submenu pointers + a back stack,
// with custom widget rows for spinners. Spinner types:
//   - display saver timeout (seconds / minutes unit switch)
//   - input history timeout (seconds)
//   - LED brightness / speed
//   - hex color editors (|RR|GG|BB| nibble editing with live LED preview)
//
// Values are staged in prevX/updateX pairs. Selecting a list option stages it;
// "Save & Exit" (or the exit prompt) commits via saveOptions(), and "No"
// discards via resetOptions(). LED color/brightness/speed changes are shown
// live on the strip through the LedPreview pipeline.

#define MAIN_MENU_NAME "Mini Menu"

#define INPUT_MODE_KEYBOARD_NAME "Keyboard"
#define INPUT_MODE_MIDI_NAME "MIDI"
#define INPUT_MODE_XINPUT_NAME "XInput"
#define INPUT_MODE_SWITCH_PRO_NAME "Switch Pro"

#define SOCD_MODE_UP_PRIORITY_NAME "Up Priority"
#define SOCD_MODE_NEUTRAL_NAME "Neutral"
#define SOCD_MODE_SECOND_INPUT_PRIORITY_NAME "Last Win"
#define SOCD_MODE_FIRST_INPUT_PRIORITY_NAME "First Win"
#define SOCD_MODE_BYPASS_NAME "Off"

#define ANIMATION_CUSTOM_NAME "Custom"
#define ANIMATION_CYCLE_NAME "Cycle"
#define ANIMATION_REACTIVE_NAME "Reactive"
#define ANIMATION_BPS_NAME "BPS"
#define ANIMATION_RIPPLE_NAME "Ripple"
#define ANIMATION_RAIN_NAME "Rain"
#define ANIMATION_FIRE_NAME "Fire"

#define DISPLAY_SAVER_OFF_NAME "Off"
#define DISPLAY_SAVER_SNOW_NAME "Snow"
#define DISPLAY_SAVER_BOUNCE_NAME "Bounce"
#define DISPLAY_SAVER_PIPES_NAME "Pipes"
#define DISPLAY_SAVER_TOAST_NAME "Toast"
#define DISPLAY_SAVER_STARS_NAME "Stars"

class MainMenuScreen : public GPScreen {
	public:
		MainMenuScreen() {}
		MainMenuScreen(GPGFX* renderer) { setRenderer(renderer); }
		virtual int8_t update();
		virtual void init();
		virtual void shutdown();

		void setMenu(std::vector<MenuEntry>* menu);

		void testMenu() {}
		void saveAndExit();
		int32_t modeValue() { return -1; }

		void selectInputMode();
		int32_t currentInputMode();

		void selectSOCDMode();
		int32_t currentSOCDMode();

		void selectProfile();
		int32_t currentProfile();

		void selectDisplaySaverTimeout();
		int32_t currentDisplaySaverTimeout();

		void selectDisplaySaverMode();
		int32_t currentDisplaySaverMode();

		void selectInputHistoryTimeout();
		int32_t currentInputHistoryTimeout();

		void selectAnimation();
		int32_t currentAnimation();

		int32_t currentBrightness();
		int32_t currentSpeed();

		void selectRemap();
		void selectRebootNormal();
		void selectRebootWebConfig();
		void selectRebootBootsel();

		// Fed by the DisplayController (nav pins / repeat). Returns a target
		// DisplayMode or -1.
		int8_t handleNavigation(uint8_t action);
		// Repeat UP/DOWN only while the current row is a spinner.
		bool wantsNavRepeat(uint8_t action) {
			if (screenIsPrompting) return false;
			if (action != MENU_ACTION_UP && action != MENU_ACTION_DOWN) return false;
			return currentMenu->size() > 0 && currentMenu->at(menuIndex).isSpinner;
		}
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

		void updateMenuNavigation(uint8_t action);

		uint8_t menuIndex = 0;
		bool isPressed = false;
		std::vector<MenuEntry>* currentMenu;
		struct MenuBackEntry {
			std::vector<MenuEntry>* menu;
			uint8_t index;
			std::string title;
		};
		std::vector<MenuBackEntry> menuBackStack;
		GPMenu* gpMenu = nullptr;
		static uint8_t savedMenuIndex;

		bool screenIsPrompting = false;
		bool promptChoice = false;

		int8_t exitToScreenBeforePrompt = -1;
		int8_t exitToScreen = -1;

		void saveOptions();
		void resetOptions();
		bool changeRequiresReboot = false;
		bool changeRequiresSave = false;

		void adjustSpinnerValue(int8_t direction);
		void switchSpinnerUnit(int8_t direction);
		void saveSpinnerValue();
		void revertSpinnerValue();
		uint8_t currentSpinnerUnit = 0;
		// Live LED preview: push the currently-edited LED state to the strip
		// without persisting. Used while scrubbing spinners so the effect is
		// visible immediately (MP2040's LedPreview pipeline replaces GP2040-th's
		// setPreviewColor).
		void previewLedState();

		#define INPUT_MODE_ENTRIES(name, value) {name##_NAME, NULL, nullptr, std::bind(&MainMenuScreen::currentInputMode, this), std::bind(&MainMenuScreen::selectInputMode, this), value},
		#define SOCD_MODE_ENTRIES(name, value)  {name##_NAME, NULL, nullptr, std::bind(&MainMenuScreen::currentSOCDMode, this), std::bind(&MainMenuScreen::selectSOCDMode, this), value},

		std::vector<MenuEntry> inputModeMenu = {
			INPUT_MODE_ENTRIES(INPUT_MODE_KEYBOARD, INPUT_MODE_KEYBOARD)
			INPUT_MODE_ENTRIES(INPUT_MODE_MIDI, INPUT_MODE_MIDI)
			INPUT_MODE_ENTRIES(INPUT_MODE_XINPUT, INPUT_MODE_XINPUT)
			INPUT_MODE_ENTRIES(INPUT_MODE_SWITCH_PRO, INPUT_MODE_SWITCH_PRO)
		};
		InputMode prevInputMode;
		InputMode updateInputMode;

		std::vector<MenuEntry> socdModeMenu = {
			SOCD_MODE_ENTRIES(SOCD_MODE_UP_PRIORITY, SOCD_MODE_UP_PRIORITY)
			SOCD_MODE_ENTRIES(SOCD_MODE_NEUTRAL, SOCD_MODE_NEUTRAL)
			SOCD_MODE_ENTRIES(SOCD_MODE_SECOND_INPUT_PRIORITY, SOCD_MODE_SECOND_INPUT_PRIORITY)
			SOCD_MODE_ENTRIES(SOCD_MODE_FIRST_INPUT_PRIORITY, SOCD_MODE_FIRST_INPUT_PRIORITY)
			SOCD_MODE_ENTRIES(SOCD_MODE_BYPASS, SOCD_MODE_BYPASS)
		};
		SOCDMode prevSocdMode;
		SOCDMode updateSocdMode;

		std::vector<MenuEntry> profilesMenu = {};
		uint8_t prevProfile;
		uint8_t updateProfile;

		std::vector<MenuEntry> displayTimeoutMenu;
		uint32_t prevDisplaySaverTimeout;
		uint32_t updateDisplaySaverTimeout;
		uint32_t spinnerValueSnapshot;

		std::vector<MenuEntry> displaySaverModeMenu;
		uint8_t prevDisplaySaverMode;
		uint8_t updateDisplaySaverMode;

		std::vector<MenuEntry> histTimeoutMenu;
		uint16_t prevInputHistoryTimeout;
		uint16_t updateInputHistoryTimeout;
		uint16_t histSpinnerValueSnapshot;

		std::vector<MenuEntry> displayMenu;

		uint8_t prevAnimationIndex;
		uint8_t updateAnimationIndex;
		uint8_t prevBrightness;
		uint8_t updateBrightness;
		uint8_t brightnessSpinnerSnapshot;
		uint8_t prevSpeed;
		uint8_t updateSpeed;
		uint8_t speedSpinnerSnapshot;

		std::vector<MenuEntry> animationMenu;
		std::vector<MenuEntry> brightnessMenu;
		std::vector<MenuEntry> speedMenu;

		std::vector<MenuEntry> colorNormalMenu;
		std::vector<MenuEntry> colorPressedMenu;
		std::vector<MenuEntry> colorMenu;
		uint32_t prevColorNormal;
		uint32_t updateColorNormal;
		uint32_t prevColorPressed;
		uint32_t updateColorPressed;

		std::vector<MenuEntry> ledMenu;

		std::vector<MenuEntry> rebootMenu;

		std::vector<MenuEntry> saveMenu = {
			{"Yes", NULL, nullptr, std::bind(&MainMenuScreen::modeValue, this), std::bind(&MainMenuScreen::saveAndExit, this), 1},
			{"No",  NULL, nullptr, std::bind(&MainMenuScreen::modeValue, this), std::bind(&MainMenuScreen::testMenu, this), 0},
		};

		std::vector<MenuEntry> mainMenu;
};

#endif