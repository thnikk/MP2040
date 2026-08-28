#ifndef HOTKEYS_H_
#define HOTKEYS_H_

#include <stdint.h>

#include "keymask.h"
#include "storagemanager.h"
#include "enums.pb.h"

// Detects configurable hotkeys (simultaneous key combos, Config.hotkeys)
// against the published key state and dispatches their actions. Runs on core 0
// each main-loop iteration just before the input driver, so it is naturally
// skipped in web-config mode and while the on-device menu is active.
//
// Combos are matched in Config.hotkeys order; the first one whose keys are all
// held wins (slot 0 has the highest priority). When a hotkey fires:
//   - its trigger keys are added to Storage.hotkeySuppressed so the active
//     driver doesn't also emit their normal output (key / MIDI note / gamepad
//     control),
//   - one-shot actions (SOCD mode, profile switch) dispatch on the press edge
//     and persist, while macro actions hold-to-play by publishing
//     Storage.hotkeyMacroIndex until the combo is released. "Toggle menu"
//     requests a core-1 menu flip without persisting. While the on-device menu
//     is open only the toggle-menu action acts (so navigation can't trip the
//     others).
class HotkeyController {
public:
	static HotkeyController& getInstance() {
		static HotkeyController instance;
		return instance;
	}

	void process();

private:
	HotkeyController() : prevComboHeld(0) {}

	// Dispatch a one-shot action (SOCD / profile). Macro actions are handled in
	// process() via hotkeyMacroIndex instead.
	void dispatch(HotkeyAction action);
	// Switch to a profile (live + persisted), mirroring the serial "profile"
	// command.
	void loadProfile(uint32_t index);

	// Bitmask of which hotkey slots had their combo held on the previous frame
	// (bit i = slot i), for press-edge detection of one-shot actions.
	uint16_t prevComboHeld;
};

// True if the action is a macro-trigger (plays while held).
static inline bool isMacroHotkeyAction(HotkeyAction action)
{
	return action >= HOTKEY_TRIGGER_MACRO_1 && action <= HOTKEY_TRIGGER_MACRO_8;
}

// 1-based macro index (1-8) for a macro-trigger action.
static inline uint8_t macroIndexForAction(HotkeyAction action)
{
	return (uint8_t)(action - HOTKEY_TRIGGER_MACRO_1) + 1;
}

#endif // HOTKEYS_H_
