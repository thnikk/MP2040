#ifndef _REMAPSCREEN_H_
#define _REMAPSCREEN_H_

#include "GPGFX_UI_widgets.h"
#include "GPGFX_UI_types.h"
#include "layoutmanager.h"
#include "enums.pb.h"
#include "storagemanager.h"
#include "drivermanager.h"

// On-device key remapping (ported from GP2040-th). The board layout is drawn
// with a cursor that moves between the layout's pin buttons (nearest-button
// navigation). Selecting a pin opens a mode-specific editor:
//   - Keyboard: manage the pin's keycode + modifier (add / clear), browse key
//     categories (letters, numbers, punctuation, nav, function, numpad,
//     system, media), then pick a modifier preset.
//   - Gamepad (XInput / Switch Pro): a manage screen lists the pin's assigned
//     controls (dpad directions + buttons B1..A2); selecting one removes it.
//     "+ Add Button" opens the action browser, where picking a control toggles
//     its bit and returns to the manage screen so the list stays current.
//   - MIDI: set the pin's MIDI note (0-127).
//
// Unlike GP2040-th, MP2040 has no GpioAction concept: keyboard pins hold a
// keycode + modifier mask, gamepad pins hold a control bitmask, and MIDI pins
// hold a note. Persistence writes the active profile's KeyMapping (keycodes /
// modifierMasks / midiNotes) or the global GamepadMapping, then saves.
//
// Exiting with staged changes (BACK or a toggle-close) raises a Yes/No save
// prompt instead of committing silently. Yes saves to flash; No discards by
// restoring the mapping snapshots taken at init().
enum RemapMode {
	REMAP_LAYOUT,
	REMAP_GAMEPAD_MANAGE,
	REMAP_ACTION_SELECT,
	REMAP_KBD_MANAGE,
	REMAP_KBD_SELECT,
	REMAP_KBD_MODIFIER,
	REMAP_MIDI
};

class RemapScreen : public GPScreen {
	public:
		RemapScreen() {}
		RemapScreen(GPGFX* renderer) { setRenderer(renderer); }
		virtual int8_t update();
		virtual void init();
		virtual void shutdown();

		// Fed by the DisplayController (nav pins). Returns a target DisplayMode
		// (REMAP stays, MAIN_MENU on back) or -1.
		int8_t handleNavigation(uint8_t action);
		// Toggle-close: behave like a root B2 so staged changes hit the save
		// prompt instead of being silently saved. Returns the exit target or
		// -1 (prompt shown).
		int8_t requestClose() override;
	protected:
		virtual void drawScreen();
	private:
		enum RemapAction {
			ACTION_UP = 0,
			ACTION_DOWN,
			ACTION_LEFT,
			ACTION_RIGHT,
			ACTION_SELECT,
			ACTION_BACK,
		};

		RemapMode mode = REMAP_LAYOUT;
		std::vector<GPButtonLayout> layoutElements;
		size_t cursorIndex = 0;

		uint8_t actionCategory = 0;
		uint16_t actionCategoryIndex = 0;
		uint8_t gpManageIndex = 0;

		bool hasChanges = false;

		uint8_t kbdManageIndex = 0;
		uint8_t kbdPendingKeycode = 0;
		uint8_t kbdCategory = 0;
		uint16_t kbdCategoryIndex = 0;
		uint8_t kbdModifierIndex = 0;

		uint8_t midiNote = 0;
		uint8_t midiNoteSnapshot = 0;

		InputMode currentMode;
		bool returnToMenu = false;

		// Yes/No save prompt shown when exiting with staged changes. Mirrors
		// the mini menu's prompt (see MainMenuScreen). pendingExitMode is the
		// display the prompt resolves to (MAIN_MENU on BACK, BUTTONS on
		// toggle-close).
		bool screenIsPrompting = false;
		bool promptChoice = false;
		int8_t pendingExitMode = -1;

		// Mapping snapshots for the prompt's "No" (discard) path. Remap edits
		// the live config.keyMapping / config.gamepadMapping in place, so a
		// discard must restore them (and the active profile's stored copy).
		KeyMapping keyMappingSnapshot;
		bool keyMappingHasSnapshot = false;
		GamepadMapping gamepadSnapshot;
		bool gamepadHasSnapshot = false;

		void raiseSavePrompt(int8_t exitMode);
		void discardChanges();
		void drawSavePrompt();

		void enterGamepadManage();
		void enterActionSelect();
		void toggleAction(uint32_t controlBit);
		void clearAction();

		void enterKbdManage();
		void enterKbdSelect();
		void enterKbdModifier();
		void enterMidi();

		void clearKeyboardKey();
		void assignKeyboardKey(uint8_t keycode, uint8_t modifierMask);
		void persistKeyboardKeyToConfig(uint8_t pin);
		void persistGamepadMaskToConfig(uint8_t pin);
		void persistMidiNoteToConfig(uint8_t pin);

		bool updateLayout(uint8_t action);
		bool updateGamepadManage(uint8_t action);
		bool updateActionSelect(uint8_t action);
		bool updateKbdManage(uint8_t action);
		bool updateKbdSelect(uint8_t action);
		bool updateKbdModifier(uint8_t action);
		bool updateMidi(uint8_t action);

		int8_t findNearestPin(int8_t dirX, int8_t dirY);

		void drawLayout();
		void drawGamepadManage();
		void drawActionSelect();
		void drawKbdManage();
		void drawKbdSelect();
		void drawKbdModifier();
		void drawMidi();

		void save();
		void exitToMainMenu();
};

#endif