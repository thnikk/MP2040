#include "hotkeys.h"

// True if every key of a hotkey combo is currently held. Keys outside the
// board's key range are treated as never held so a bad stored entry can't
// wedge a hotkey into firing forever.
static bool comboHeld(const HotkeyEntry& hotkey, const KeyMask& keyState)
{
	for (pb_size_t k = 0; k < hotkey.keys_count; k++)
	{
		const uint32_t key = hotkey.keys[k];
		if (key >= MAX_KEYS || !keyState.test(key))
			return false;
	}
	return true;
}

void HotkeyController::process()
{
	Storage& s = Storage::getInstance();
	const Config& config = s.getConfig();
	const KeyMask& keyState = s.keyState;
	const bool menuActive = s.GetMenuActive();

	KeyMask suppressed;
	uint8_t macroIndex = 0;
	uint16_t held = 0;

	// First match wins, like GP2040-th's if/else-if hotkey chain: only the
	// highest-priority fired combo is suppressed and acted on.
	for (pb_size_t i = 0; i < config.hotkeys_count; i++)
	{
		const HotkeyEntry& hotkey = config.hotkeys[i];
		if (hotkey.action == HOTKEY_NONE || hotkey.keys_count == 0)
			continue;
		if (!comboHeld(hotkey, keyState))
			continue;

		const HotkeyAction action = (HotkeyAction)hotkey.action;

		// While the on-device menu is open only a "toggle menu" hotkey acts (it
		// closes the menu); other actions stay inert so menu navigation can't
		// trip profile / SOCD / macro hotkeys.
		if (menuActive && action != HOTKEY_TOGGLE_MENU)
			continue;

		const uint16_t bit = 1u << i;
		held = bit;
		const bool rising = !(prevComboHeld & bit);

		for (pb_size_t k = 0; k < hotkey.keys_count; k++)
		{
			const uint32_t key = hotkey.keys[k];
			if (key < MAX_KEYS)
				suppressed.set(key);
		}

		if (isMacroHotkeyAction(action))
		{
			// Hold-to-play: keep the macro running until the combo is released.
			macroIndex = macroIndexForAction(action);
		}
		else if (rising)
		{
			dispatch(action);
		}
		break;
	}

	prevComboHeld = held;
	s.hotkeySuppressed = suppressed;
	s.hotkeyMacroIndex = macroIndex;
}

void HotkeyController::dispatch(HotkeyAction action)
{
	Storage& s = Storage::getInstance();
	switch (action)
	{
		case HOTKEY_TOGGLE_MENU:
			// No config change: just ask the core-1 display controller to flip
			// the mini menu.
			s.requestMenuToggle();
			return;
		case HOTKEY_SOCD_UP_PRIORITY:
			s.setSocdMode(SOCD_MODE_UP_PRIORITY);
			break;
		case HOTKEY_SOCD_NEUTRAL:
			s.setSocdMode(SOCD_MODE_NEUTRAL);
			break;
		case HOTKEY_SOCD_LAST_INPUT:
			s.setSocdMode(SOCD_MODE_SECOND_INPUT_PRIORITY);
			break;
		case HOTKEY_SOCD_FIRST_INPUT:
			s.setSocdMode(SOCD_MODE_FIRST_INPUT_PRIORITY);
			break;
		case HOTKEY_SOCD_BYPASS:
			s.setSocdMode(SOCD_MODE_BYPASS);
			break;
		case HOTKEY_LOAD_PROFILE_1:
		case HOTKEY_LOAD_PROFILE_2:
		case HOTKEY_LOAD_PROFILE_3:
		case HOTKEY_LOAD_PROFILE_4:
			loadProfile((uint32_t)(action - HOTKEY_LOAD_PROFILE_1));
			break;
		case HOTKEY_NEXT_PROFILE:
			loadProfile((s.getActiveProfile() + 1) % 4);
			break;
		case HOTKEY_PREVIOUS_PROFILE:
			loadProfile((s.getActiveProfile() + 3) % 4);
			break;
		default:
			return; // no config change, nothing to persist
	}

	s.save(true);
}

void HotkeyController::loadProfile(uint32_t index)
{
	Storage& s = Storage::getInstance();
	if (index >= s.getProfileCount())
		return;
	s.setActiveProfile(index);
	s.applyActiveProfile();
	// Push the new profile's LED state live so the strip reflects it now
	// (mirrors the serial "profile" command).
	LedPreview preview;
	s.buildLedPreviewFromConfig(preview);
	s.publishLedPreview(preview);
}
