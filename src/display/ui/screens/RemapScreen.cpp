#include "RemapScreen.h"
#include "gamepadmapping.h"
#include "system.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <climits>

// --- Action (gamepad control) categories ----------------------------------
// MP2040 assigns per-pin gamepad control masks (GamepadMapping.masks, see
// gamepadmapping.h). Each entry toggles one bit of the mask.

struct ActionEntry {
	uint32_t bit;
	const char* name;
};

struct ActionCategory {
	const char* name;
	const ActionEntry* entries;
	uint8_t count;
};

static const ActionEntry dpadActions[] = {
	{ GAMEPAD_PIN_MASK_UP,    "Up" },
	{ GAMEPAD_PIN_MASK_DOWN,  "Down" },
	{ GAMEPAD_PIN_MASK_LEFT,  "Left" },
	{ GAMEPAD_PIN_MASK_RIGHT, "Right" },
};

static const ActionEntry buttonActions[] = {
	{ GAMEPAD_PIN_MASK_B1, "B1" },
	{ GAMEPAD_PIN_MASK_B2, "B2" },
	{ GAMEPAD_PIN_MASK_B3, "B3" },
	{ GAMEPAD_PIN_MASK_B4, "B4" },
	{ GAMEPAD_PIN_MASK_L1, "L1" },
	{ GAMEPAD_PIN_MASK_R1, "R1" },
	{ GAMEPAD_PIN_MASK_L2, "L2" },
	{ GAMEPAD_PIN_MASK_R2, "R2" },
	{ GAMEPAD_PIN_MASK_S1, "S1" },
	{ GAMEPAD_PIN_MASK_S2, "S2" },
	{ GAMEPAD_PIN_MASK_L3, "L3" },
	{ GAMEPAD_PIN_MASK_R3, "R3" },
	{ GAMEPAD_PIN_MASK_A1, "A1" },
	{ GAMEPAD_PIN_MASK_A2, "A2" },
};

static const ActionCategory actionCategories[] = {
	{ "D-Pad",   dpadActions,   sizeof(dpadActions)   / sizeof(dpadActions[0]) },
	{ "Buttons", buttonActions, sizeof(buttonActions) / sizeof(buttonActions[0]) },
};

static const uint8_t actionCategoryCount = sizeof(actionCategories) / sizeof(actionCategories[0]);

// The set controls of a mask, in browse order (D-Pad then Buttons). Out holds
// every possible control (4 dpad + 14 buttons); returns the count.
static uint8_t collectSetActions(uint32_t mask, ActionEntry* out) {
	uint8_t n = 0;
	for (uint8_t c = 0; c < actionCategoryCount; c++) {
		for (uint8_t i = 0; i < actionCategories[c].count; i++) {
			if (mask & actionCategories[c].entries[i].bit)
				out[n++] = actionCategories[c].entries[i];
		}
	}
	return n;
}

// --- Keyboard keycode browser (same table as GP2040-th) -------------------

struct KeyEntry {
	uint8_t code;
	const char* name;
};

struct KeyCategory {
	const char* name;
	const KeyEntry* entries;
	uint8_t count;
};

static const KeyEntry lettersKeys[] = {
	{ 0x04, "A" }, { 0x05, "B" }, { 0x06, "C" }, { 0x07, "D" },
	{ 0x08, "E" }, { 0x09, "F" }, { 0x0A, "G" }, { 0x0B, "H" },
	{ 0x0C, "I" }, { 0x0D, "J" }, { 0x0E, "K" }, { 0x0F, "L" },
	{ 0x10, "M" }, { 0x11, "N" }, { 0x12, "O" }, { 0x13, "P" },
	{ 0x14, "Q" }, { 0x15, "R" }, { 0x16, "S" }, { 0x17, "T" },
	{ 0x18, "U" }, { 0x19, "V" }, { 0x1A, "W" }, { 0x1B, "X" },
	{ 0x1C, "Y" }, { 0x1D, "Z" },
};

static const KeyEntry numbersKeys[] = {
	{ 0x1E, "1" }, { 0x1F, "2" }, { 0x20, "3" }, { 0x21, "4" },
	{ 0x22, "5" }, { 0x23, "6" }, { 0x24, "7" }, { 0x25, "8" },
	{ 0x26, "9" }, { 0x27, "0" },
};

static const KeyEntry punctKeys[] = {
	{ 0x2D, "-" },   { 0x2E, "=" },    { 0x2F, "[" },  { 0x30, "]" },
	{ 0x31, "\\" },  { 0x33, ";" },    { 0x34, "'" },  { 0x35, "`" },
	{ 0x36, "," },   { 0x37, "." },    { 0x38, "/" },
};

static const KeyEntry navKeys[] = {
	{ 0x52, "Up" },    { 0x51, "Down" },  { 0x50, "Left" },
	{ 0x4F, "Right" }, { 0x4A, "Home" },  { 0x4D, "End" },
	{ 0x4B, "PgUp" },  { 0x4E, "PgDn" },  { 0x49, "Ins" },
	{ 0x4C, "Del" },
};

static const KeyEntry funcKeys[] = {
	{ 0x3A, "F1" },  { 0x3B, "F2" },  { 0x3C, "F3" },  { 0x3D, "F4" },
	{ 0x3E, "F5" },  { 0x3F, "F6" },  { 0x40, "F7" },  { 0x41, "F8" },
	{ 0x42, "F9" },  { 0x43, "F10" }, { 0x44, "F11" }, { 0x45, "F12" },
	{ 0x68, "F13" }, { 0x69, "F14" }, { 0x6A, "F15" }, { 0x6B, "F16" },
	{ 0x6C, "F17" }, { 0x6D, "F18" }, { 0x6E, "F19" }, { 0x6F, "F20" },
	{ 0x70, "F21" }, { 0x71, "F22" }, { 0x72, "F23" }, { 0x73, "F24" },
};

static const KeyEntry numpadKeys[] = {
	{ 0x53, "NumLk" }, { 0x54, "N/" },  { 0x55, "N*" },
	{ 0x56, "N-" },    { 0x57, "N+" },  { 0x58, "NEn" },
	{ 0x59, "N1" },    { 0x5A, "N2" },  { 0x5B, "N3" },
	{ 0x5C, "N4" },    { 0x5D, "N5" },  { 0x5E, "N6" },
	{ 0x5F, "N7" },    { 0x60, "N8" },  { 0x61, "N9" },
	{ 0x62, "N0" },    { 0x63, "N." },
};

static const KeyEntry sysKeys[] = {
	{ 0x29, "Esc" },    { 0x2B, "Tab" },   { 0x39, "Caps" },
	{ 0x28, "Enter" },  { 0x2A, "Bksp" },  { 0x2C, "Space" },
	{ 0x46, "PrtSc" },  { 0x47, "ScrlLk" },{ 0x48, "Pause" },
	{ 0x65, "App" },    { 0x66, "Power" },
};

static const KeyEntry mediaKeys[] = {
	{ 0xE8, "NextTrk" }, { 0xE9, "PrevTrk" }, { 0xF0, "Stop" },
	{ 0xF1, "Play/P" },  { 0xF2, "Mute" },    { 0xF3, "Vol+" },
	{ 0xF4, "Vol-" },
};

static const KeyCategory keyCategories[] = {
	{ "Letters", lettersKeys, sizeof(lettersKeys)/sizeof(lettersKeys[0]) },
	{ "Numbers", numbersKeys, sizeof(numbersKeys)/sizeof(numbersKeys[0]) },
	{ "Punct",   punctKeys,   sizeof(punctKeys)/sizeof(punctKeys[0]) },
	{ "Navigate",navKeys,     sizeof(navKeys)/sizeof(navKeys[0]) },
	{ "Function",funcKeys,    sizeof(funcKeys)/sizeof(funcKeys[0]) },
	{ "Numpad",  numpadKeys,  sizeof(numpadKeys)/sizeof(numpadKeys[0]) },
	{ "System",  sysKeys,     sizeof(sysKeys)/sizeof(sysKeys[0]) },
	{ "Media",   mediaKeys,   sizeof(mediaKeys)/sizeof(mediaKeys[0]) },
};

static const uint8_t kbdCategoryCount = sizeof(keyCategories)/sizeof(keyCategories[0]);
static const uint8_t kbdSelectCategoryCount = 7;

struct ModifierEntry {
	uint8_t mask;
	const char* name;
};

static const ModifierEntry modifierPresets[] = {
	{ 0x00, "None" },
	{ 0x02, "Shift" },
	{ 0x01, "Ctrl" },
	{ 0x04, "Alt" },
	{ 0x08, "Win" },
	{ 0x03, "S+C" },
	{ 0x06, "S+A" },
	{ 0x05, "C+A" },
	{ 0x0A, "W+S" },
	{ 0x09, "W+C" },
	{ 0x0C, "W+A" },
};

static const uint8_t modifierCount = sizeof(modifierPresets)/sizeof(modifierPresets[0]);

static const char* getKeyName(uint8_t code) {
	if (code == 0) return "None";
	for (uint8_t c = 0; c < kbdCategoryCount; c++) {
		for (uint8_t i = 0; i < keyCategories[c].count; i++) {
			if (keyCategories[c].entries[i].code == code)
				return keyCategories[c].entries[i].name;
		}
	}
	return "Key";
}

static const char* getModifierName(uint8_t mask) {
	for (uint8_t i = 0; i < modifierCount; i++) {
		if (modifierPresets[i].mask == mask)
			return modifierPresets[i].name;
	}
	return "Mod";
}

// MIDI pitch class names (index 0-11 = C..B), matching the web UI.
static const char* midiPitchNames[] = {
	"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B",
};

void RemapScreen::init() {
	getRenderer()->clearScreen();
	mode = REMAP_LAYOUT;
	cursorIndex = 0;
	hasChanges = false;
	returnToMenu = false;
	screenIsPrompting = false;
	promptChoice = false;
	pendingExitMode = -1;

	// Snapshot the mappings the remap edits so the exit prompt's "No" can
	// restore them (the live config is mutated in place as the user assigns).
	Storage& storage = Storage::getInstance();
	keyMappingSnapshot = storage.getKeyMapping();
	keyMappingHasSnapshot = storage.getConfig().has_keyMapping;
	gamepadSnapshot = storage.getGamepadMapping();
	gamepadHasSnapshot = storage.getConfig().has_gamepadMapping;

	actionCategory = 0;
	actionCategoryIndex = 0;
	gpManageIndex = 0;
	kbdManageIndex = 0;
	kbdPendingKeycode = 0;
	kbdCategory = 0;
	kbdCategoryIndex = 0;
	kbdModifierIndex = 0;
	midiField = 0;
	midiNoteIdx = -1;
	midiOctave = 4;
	midiVelocity = 0;

	currentMode = DriverManager::getInstance().getInputMode();

	// Layout coordinates map 1:1 to the panel (authored for 128x64): use the
	// full panel as the viewport so no scaling/offset is applied at draw time.
	setViewport(0, 0, getRenderer()->getDriver()->getMetrics()->height,
		getRenderer()->getDriver()->getMetrics()->width);

	// Collect the pin button elements from the board's single-panel layout.
	layoutElements.clear();
	uint8_t layout = Storage::getInstance().getDisplayOptions().has_buttonLayout
		? Storage::getInstance().getDisplayOptions().buttonLayout
		: BUTTON_LAYOUT_BOARD_DEFINED;
	LayoutManager::LayoutList layoutList = LayoutManager::getInstance().getLayout((ButtonLayout)layout);
	for (auto& elem : layoutList) {
		if (elem.elementType == GP_ELEMENT_PIN_BUTTON)
			layoutElements.push_back(elem);
	}
}

void RemapScreen::shutdown() {
	// No auto-save here: staged changes are committed (or discarded) by the
	// exit prompt, so tearing the screen down must not silently persist them.
	clearElements();
}

int8_t RemapScreen::update() {
	if (returnToMenu) {
		returnToMenu = false;
		return DisplayMode::MAIN_MENU;
	}
	return -1;
}

int8_t RemapScreen::handleNavigation(uint8_t action) {
	// While the save prompt is up, all navigation drives Yes/No: directions
	// flip the choice, SELECT resolves it (exiting to the pending target),
	// BACK cancels back to the remap layout with the changes still staged.
	if (screenIsPrompting) {
		switch (action) {
			case ACTION_UP:
			case ACTION_DOWN:
			case ACTION_LEFT:
			case ACTION_RIGHT:
				promptChoice = !promptChoice;
				break;
			case ACTION_SELECT:
				if (promptChoice) {
					save();
				} else {
					discardChanges();
				}
				hasChanges = false;
				screenIsPrompting = false;
				promptChoice = false;
				{
					const int8_t target = pendingExitMode;
					pendingExitMode = -1;
					if (target >= 0) return target;
				}
				break;
			case ACTION_BACK:
				screenIsPrompting = false;
				promptChoice = false;
				break;
			default:
				break;
		}
		return -1;
	}

	switch (mode) {
		case REMAP_LAYOUT:          updateLayout(action); break;
		case REMAP_GAMEPAD_MANAGE:  updateGamepadManage(action); break;
		case REMAP_ACTION_SELECT:   updateActionSelect(action); break;
		case REMAP_KBD_MANAGE:      updateKbdManage(action); break;
		case REMAP_KBD_SELECT:      updateKbdSelect(action); break;
		case REMAP_KBD_MODIFIER:    updateKbdModifier(action); break;
		case REMAP_MIDI:            updateMidi(action); break;
	}
	return -1;
}

// ---- layout mode ---------------------------------------------------------

bool RemapScreen::updateLayout(uint8_t action) {
	if (layoutElements.size() == 0) {
		// No layout: only BACK works.
		if (action == ACTION_BACK) { exitToMainMenu(); return true; }
		return false;
	}

	switch (action) {
		case ACTION_UP: {
			int8_t idx = findNearestPin(0, -1);
			if (idx >= 0 && (size_t)idx < layoutElements.size()) cursorIndex = idx;
			return true;
		}
		case ACTION_DOWN: {
			int8_t idx = findNearestPin(0, 1);
			if (idx >= 0 && (size_t)idx < layoutElements.size()) cursorIndex = idx;
			return true;
		}
		case ACTION_LEFT: {
			int8_t idx = findNearestPin(-1, 0);
			if (idx >= 0 && (size_t)idx < layoutElements.size()) cursorIndex = idx;
			return true;
		}
		case ACTION_RIGHT: {
			int8_t idx = findNearestPin(1, 0);
			if (idx >= 0 && (size_t)idx < layoutElements.size()) cursorIndex = idx;
			return true;
		}
		case ACTION_SELECT:
			if (currentMode == INPUT_MODE_KEYBOARD) {
				enterKbdManage();
			} else if (currentMode == INPUT_MODE_MIDI) {
				enterMidi();
			} else {
				enterGamepadManage();
			}
			return true;
		case ACTION_BACK:
			if (hasChanges) {
				raiseSavePrompt(DisplayMode::MAIN_MENU);
			} else {
				exitToMainMenu();
			}
			return true;
		default:
			return false;
	}
}

void RemapScreen::raiseSavePrompt(int8_t exitMode) {
	pendingExitMode = exitMode;
	screenIsPrompting = true;
	promptChoice = false;
}

int8_t RemapScreen::requestClose() {
	// Toggle-close behaves like a root B2: staged changes raise the save
	// prompt instead of being silently committed on teardown.
	if (hasChanges) {
		raiseSavePrompt(DisplayMode::BUTTONS);
		return -1;
	}
	return DisplayMode::BUTTONS;
}

void RemapScreen::discardChanges() {
	Storage& s = Storage::getInstance();
	s.getKeyMapping() = keyMappingSnapshot;
	s.getConfig().has_keyMapping = keyMappingHasSnapshot;
	s.getGamepadMapping() = gamepadSnapshot;
	s.getConfig().has_gamepadMapping = gamepadHasSnapshot;
	// Keyboard/MIDI edits also mirror into the active profile's stored
	// KeyMapping (see persistKeyboardKeyToConfig), so restore that too.
	Profile* profile = s.getProfile(s.getActiveProfile());
	if (profile != nullptr) {
		profile->keyMapping = keyMappingSnapshot;
		profile->has_keyMapping = keyMappingHasSnapshot;
	}
}

void RemapScreen::enterGamepadManage() {
	mode = REMAP_GAMEPAD_MANAGE;
	gpManageIndex = 0;
}

void RemapScreen::enterActionSelect() {
	mode = REMAP_ACTION_SELECT;
	actionCategory = 0;
	actionCategoryIndex = 0;
}

void RemapScreen::toggleAction(uint32_t controlBit) {
	if (cursorIndex >= layoutElements.size()) return;
	uint8_t pin = layoutElements[cursorIndex].parameters.value;
	GamepadMapping& mapping = Storage::getInstance().getGamepadMapping();
	if (pin >= mapping.masks_count) {
		mapping.masks_count = pin + 1;
		Storage::getInstance().setGamepadMappingCount(mapping.masks_count);
	}
	mapping.masks[pin] ^= controlBit;
	persistGamepadMaskToConfig(pin);
	hasChanges = true;
}

void RemapScreen::clearAction() {
	if (cursorIndex >= layoutElements.size()) return;
	uint8_t pin = layoutElements[cursorIndex].parameters.value;
	GamepadMapping& mapping = Storage::getInstance().getGamepadMapping();
	if (pin < mapping.masks_count)
		mapping.masks[pin] = 0;
	persistGamepadMaskToConfig(pin);
	hasChanges = true;
}

bool RemapScreen::updateGamepadManage(uint8_t action) {
	if (cursorIndex >= layoutElements.size()) return false;
	uint8_t pin = layoutElements[cursorIndex].parameters.value;
	uint32_t mask = Storage::getInstance().getGamepadMask(pin);
	ActionEntry setActions[18];
	uint8_t setCount = collectSetActions(mask, setActions);
	uint16_t itemCount = setCount + 1; // +1 = Add

	switch (action) {
		case ACTION_UP:
			if (gpManageIndex > 0) gpManageIndex--;
			return true;
		case ACTION_DOWN:
			if (gpManageIndex < itemCount - 1) gpManageIndex++;
			return true;
		case ACTION_SELECT:
			if (gpManageIndex < setCount) {
				toggleAction(setActions[gpManageIndex].bit);
			} else {
				enterActionSelect();
			}
			return true;
		case ACTION_BACK:
			mode = REMAP_LAYOUT;
			return true;
		default:
			return false;
	}
}

bool RemapScreen::updateActionSelect(uint8_t action) {
	uint16_t catSize = actionCategories[actionCategory].count + 1; // +1 = Clear

	switch (action) {
		case ACTION_LEFT:
			if (actionCategory > 0) {
				actionCategory--;
				actionCategoryIndex = 0;
			}
			return true;
		case ACTION_RIGHT:
			if (actionCategory < actionCategoryCount - 1) {
				actionCategory++;
				actionCategoryIndex = 0;
			}
			return true;
		case ACTION_UP:
			if (actionCategoryIndex > 0) actionCategoryIndex--;
			return true;
		case ACTION_DOWN:
			if (actionCategoryIndex < catSize - 1) actionCategoryIndex++;
			return true;
		case ACTION_SELECT:
			if (actionCategoryIndex < actionCategories[actionCategory].count) {
				uint32_t bit = actionCategories[actionCategory].entries[actionCategoryIndex].bit;
				toggleAction(bit);
			} else {
				clearAction();
			}
			// Drop back to the manage screen so the updated assignment list
			// (and any controls that were toggled off) is visible immediately.
			gpManageIndex = 0;
			mode = REMAP_GAMEPAD_MANAGE;
			return true;
		case ACTION_BACK:
			mode = REMAP_GAMEPAD_MANAGE;
			return true;
		default:
			return false;
	}
}

// ---- keyboard remap ------------------------------------------------------

void RemapScreen::enterKbdManage() {
	mode = REMAP_KBD_MANAGE;
	kbdManageIndex = 0;
}

void RemapScreen::enterKbdSelect() {
	mode = REMAP_KBD_SELECT;
	kbdCategory = 0;
	kbdCategoryIndex = 0;
}

void RemapScreen::enterKbdModifier() {
	mode = REMAP_KBD_MODIFIER;
	kbdModifierIndex = 0;
}

void RemapScreen::enterMidi() {
	mode = REMAP_MIDI;
	midiField = 0;
	if (cursorIndex >= layoutElements.size()) return;
	uint8_t pin = layoutElements[cursorIndex].parameters.value;
	const KeyMapping& mapping = Storage::getInstance().getKeyMapping();
	uint8_t note = (pin < mapping.midiNotes_count) ? (uint8_t)mapping.midiNotes[pin] : 0;
	if (note == 0) {
		midiNoteIdx = -1;
		midiOctave = 4;
	} else {
		midiNoteIdx = note % 12;
		midiOctave = (int8_t)(note / 12) - 1;
	}
	midiVelocity = (pin < mapping.midiVelocities_count) ? (uint8_t)mapping.midiVelocities[pin] : 0;
}

void RemapScreen::clearKeyboardKey() {
	if (cursorIndex >= layoutElements.size()) return;
	uint8_t pin = layoutElements[cursorIndex].parameters.value;
	KeyMapping& mapping = Storage::getInstance().getKeyMapping();
	if (pin < mapping.keycodes_count) mapping.keycodes[pin] = 0;
	if (pin < mapping.modifierMasks_count) mapping.modifierMasks[pin] = 0;
	persistKeyboardKeyToConfig(pin);
	hasChanges = true;
}

void RemapScreen::assignKeyboardKey(uint8_t keycode, uint8_t modifierMask) {
	if (cursorIndex >= layoutElements.size()) return;
	uint8_t pin = layoutElements[cursorIndex].parameters.value;
	KeyMapping& mapping = Storage::getInstance().getKeyMapping();
	if (pin >= mapping.keycodes_count) mapping.keycodes_count = pin + 1;
	mapping.keycodes[pin] = keycode;
	if (pin >= mapping.modifierMasks_count) mapping.modifierMasks_count = pin + 1;
	mapping.modifierMasks[pin] = modifierMask;
	persistKeyboardKeyToConfig(pin);
	hasChanges = true;
}

// The working copy (config.keyMapping) is the active profile's mapping, but
// the profile store is the durable source of truth (re-applied at boot via
// applyActiveProfile). Mirror single-pin edits into the active profile so they
// survive a reboot.
void RemapScreen::persistKeyboardKeyToConfig(uint8_t pin) {
	Storage& s = Storage::getInstance();
	const KeyMapping& work = s.getKeyMapping();
	Profile* profile = s.getProfile(s.getActiveProfile());
	if (profile == nullptr) return;
	profile->has_keyMapping = true;
	profile->keyMapping = work;
}

void RemapScreen::persistGamepadMaskToConfig(uint8_t pin) {
	// GamepadMapping is global; the working copy is authoritative.
	(void)pin;
}

void RemapScreen::persistMidiNoteToConfig(uint8_t pin) {
	Storage& s = Storage::getInstance();
	const KeyMapping& work = s.getKeyMapping();
	Profile* profile = s.getProfile(s.getActiveProfile());
	if (profile == nullptr) return;
	profile->has_keyMapping = true;
	profile->keyMapping = work;
}

bool RemapScreen::updateKbdManage(uint8_t action) {
	if (cursorIndex >= layoutElements.size()) return false;
	uint8_t pin = layoutElements[cursorIndex].parameters.value;
	const KeyMapping& mapping = Storage::getInstance().getKeyMapping();
	uint8_t kc = (pin < mapping.keycodes_count) ? (uint8_t)mapping.keycodes[pin] : 0;
	uint8_t mod = (pin < mapping.modifierMasks_count) ? (uint8_t)mapping.modifierMasks[pin] : 0;
	uint8_t itemCount = (kc ? 1 : 0) + (mod ? 1 : 0) + 1;

	switch (action) {
		case ACTION_UP:
			if (kbdManageIndex > 0) kbdManageIndex--;
			return true;
		case ACTION_DOWN:
			if (kbdManageIndex < itemCount - 1) kbdManageIndex++;
			return true;
		case ACTION_SELECT: {
			uint8_t keyIdx = kc ? 0 : 255;
			uint8_t modIdx = (kc && mod) ? 1 : (mod ? 0 : 255);
			uint8_t addIdx = itemCount - 1;
			if (kbdManageIndex == addIdx) {
				if (kc) {
					kbdPendingKeycode = kc;
					enterKbdModifier();
				} else {
					enterKbdSelect();
				}
			} else if (kbdManageIndex == keyIdx) {
				clearKeyboardKey();
			} else if (kbdManageIndex == modIdx) {
				KeyMapping& work = Storage::getInstance().getKeyMapping();
				if (pin < work.modifierMasks_count) work.modifierMasks[pin] = 0;
				persistKeyboardKeyToConfig(pin);
				hasChanges = true;
			}
			kbdManageIndex = 0;
			return true;
		}
		case ACTION_BACK:
			mode = REMAP_LAYOUT;
			return true;
		default:
			return false;
	}
}

bool RemapScreen::updateKbdSelect(uint8_t action) {
	uint16_t catSize = keyCategories[kbdCategory].count;

	switch (action) {
		case ACTION_LEFT:
			if (kbdCategory > 0) {
				kbdCategory--;
				kbdCategoryIndex = 0;
			}
			return true;
		case ACTION_RIGHT:
			if (kbdCategory < kbdSelectCategoryCount - 1) {
				kbdCategory++;
				kbdCategoryIndex = 0;
			}
			return true;
		case ACTION_UP:
			if (kbdCategoryIndex > 0) kbdCategoryIndex--;
			return true;
		case ACTION_DOWN:
			if (kbdCategoryIndex < catSize - 1) kbdCategoryIndex++;
			return true;
		case ACTION_SELECT:
			assignKeyboardKey(keyCategories[kbdCategory].entries[kbdCategoryIndex].code, 0);
			enterKbdManage();
			return true;
		case ACTION_BACK:
			enterKbdManage();
			return true;
		default:
			return false;
	}
}

bool RemapScreen::updateKbdModifier(uint8_t action) {
	switch (action) {
		case ACTION_UP:
			if (kbdModifierIndex > 0) kbdModifierIndex--;
			return true;
		case ACTION_DOWN:
			if (kbdModifierIndex < modifierCount - 1) kbdModifierIndex++;
			return true;
		case ACTION_SELECT:
			assignKeyboardKey(kbdPendingKeycode, modifierPresets[kbdModifierIndex].mask);
			kbdPendingKeycode = 0;
			enterKbdManage();
			return true;
		case ACTION_BACK:
			enterKbdManage();
			return true;
		default:
			return false;
	}
}

bool RemapScreen::updateMidi(uint8_t action) {
	switch (action) {
		case ACTION_UP:
			switch (midiField) {
				case 0:
					midiNoteIdx = (midiNoteIdx >= 11) ? -1 : midiNoteIdx + 1;
					break;
				case 1:
					if (midiOctave < 9) midiOctave++;
					break;
				case 2:
					if (midiVelocity < 127) midiVelocity++;
					break;
			}
			return true;
		case ACTION_DOWN:
			switch (midiField) {
				case 0:
					midiNoteIdx = (midiNoteIdx < 0) ? 11 : midiNoteIdx - 1;
					break;
				case 1:
					if (midiOctave > -1) midiOctave--;
					break;
				case 2:
					if (midiVelocity > 0) midiVelocity--;
					break;
			}
			return true;
		case ACTION_LEFT:
			midiField = (midiField + 2) % 3;
			return true;
		case ACTION_RIGHT:
			midiField = (midiField + 1) % 3;
			return true;
		case ACTION_SELECT:
			if (cursorIndex < layoutElements.size()) {
				uint8_t pin = layoutElements[cursorIndex].parameters.value;
				KeyMapping& mapping = Storage::getInstance().getKeyMapping();
				uint8_t note = 0;
				if (midiNoteIdx >= 0) {
					note = (uint8_t)((midiOctave + 1) * 12 + midiNoteIdx);
					if (note > 127) note = 127;
				}
				if (pin >= mapping.midiNotes_count) mapping.midiNotes_count = pin + 1;
				mapping.midiNotes[pin] = note;
				if (pin >= mapping.midiVelocities_count) mapping.midiVelocities_count = pin + 1;
				mapping.midiVelocities[pin] = midiVelocity;
				persistMidiNoteToConfig(pin);
				hasChanges = true;
			}
			mode = REMAP_LAYOUT;
			return true;
		case ACTION_BACK:
			mode = REMAP_LAYOUT;
			return true;
		default:
			return false;
	}
}

// ---- layout cursor movement ---------------------------------------------

int8_t RemapScreen::findNearestPin(int8_t dirX, int8_t dirY) {
	if (layoutElements.size() <= 1) return -1;

	GPButtonLayout& cur = layoutElements[cursorIndex];
	int16_t cx = cur.parameters.x1 + cur.parameters.x2 / 2;
	int16_t cy = cur.parameters.y1 + cur.parameters.y2 / 2;

	int8_t bestIdx = -1;
	int32_t bestScore = INT32_MAX;

	for (size_t i = 0; i < layoutElements.size(); i++) {
		if (i == cursorIndex) continue;
		GPButtonLayout& e = layoutElements[i];
		int16_t ex = e.parameters.x1 + e.parameters.x2 / 2;
		int16_t ey = e.parameters.y1 + e.parameters.y2 / 2;
		int16_t dx = ex - cx;
		int16_t dy = ey - cy;

		if ((dirX > 0 && dx <= 0) || (dirX < 0 && dx >= 0)) continue;
		if ((dirY > 0 && dy <= 0) || (dirY < 0 && dy >= 0)) continue;

		int32_t primaryDist = (dirX != 0) ? abs(dx) : abs(dy);
		int32_t perpDist = (dirX != 0) ? abs(dy) : abs(dx);
		int32_t score = perpDist * 6 + primaryDist;

		if (score < bestScore) {
			bestScore = score;
			bestIdx = i;
		}
	}
	return bestIdx;
}

// ---- drawing -------------------------------------------------------------

void RemapScreen::drawLayout() {
	double scaleX = this->getScaleX();
	double scaleY = this->getScaleY();

	uint16_t offsetX = 0;
	if (scaleX > 0.0f) {
		offsetX = ((getRenderer()->getDriver()->getMetrics()->width
			- (uint16_t)((double)(this->getViewport().right - this->getViewport().left) * scaleX)) / 2);
	}
	uint16_t vpTop = this->getViewport().top;
	uint16_t vpLeft = this->getViewport().left;

	for (size_t i = 0; i < layoutElements.size(); i++) {
		GPButtonLayout& elem = layoutElements[i];
		uint16_t cx = elem.parameters.x1;
		uint16_t cy = elem.parameters.y1;

		if (scaleX > 0.0f)
			cx = (uint16_t)((double)cx * scaleX + vpLeft) + offsetX;
		if (scaleY > 0.0f)
			cy = (uint16_t)((double)cy * scaleY + vpTop);

		uint8_t fill = (i == cursorIndex) ? 1 : 0;

		if (elem.parameters.shape == GP_SHAPE_ELLIPSE) {
			uint16_t radius = (uint16_t)((double)elem.parameters.x2 * scaleX);
			if (radius == 0) radius = elem.parameters.x2;
			getRenderer()->drawEllipse(cx, cy, radius, radius, 1, fill);
		} else if (elem.parameters.shape == GP_SHAPE_SQUARE) {
			uint16_t sizeX = (uint16_t)((double)elem.parameters.x2 * scaleX);
			uint16_t sizeY = (uint16_t)((double)elem.parameters.y2 * scaleY);
			if (sizeX == 0) sizeX = elem.parameters.x2;
			if (sizeY == 0) sizeY = elem.parameters.y2;
			getRenderer()->drawRectangle(cx, cy, sizeX, sizeY, 1, fill);
		}
	}

	// Top info bar (row 0): GP pin + current assignment.
	if (cursorIndex < layoutElements.size()) {
		uint8_t pinNum = layoutElements[cursorIndex].parameters.value;
		char topBuf[22];
		if (currentMode == INPUT_MODE_KEYBOARD) {
			const KeyMapping& mapping = Storage::getInstance().getKeyMapping();
			uint8_t kc = (pinNum < mapping.keycodes_count) ? (uint8_t)mapping.keycodes[pinNum] : 0;
			uint8_t mod = (pinNum < mapping.modifierMasks_count) ? (uint8_t)mapping.modifierMasks[pinNum] : 0;
			if (kc != 0) {
				if (mod)
					snprintf(topBuf, sizeof(topBuf), "GP%02d:%s %s", pinNum, getKeyName(kc), getModifierName(mod));
				else
					snprintf(topBuf, sizeof(topBuf), "GP%02d:%s", pinNum, getKeyName(kc));
			} else {
				snprintf(topBuf, sizeof(topBuf), "GP%02d:--", pinNum);
			}
		} else if (currentMode == INPUT_MODE_MIDI) {
			const KeyMapping& mapping = Storage::getInstance().getKeyMapping();
			uint8_t note = (pinNum < mapping.midiNotes_count) ? (uint8_t)mapping.midiNotes[pinNum] : 0;
			if (note != 0)
				snprintf(topBuf, sizeof(topBuf), "GP%02d:Note %d", pinNum, note);
			else
				snprintf(topBuf, sizeof(topBuf), "GP%02d:--", pinNum);
		} else {
			uint32_t mask = Storage::getInstance().getGamepadMask(pinNum);
			char tmp[64] = "";
			if (mask != 0) {
				const char* names[] = {"UP", "DN", "LT", "RT", "B1", "B2", "B3", "B4",
					"L1", "R1", "L2", "R2", "S1", "S2", "L3", "R3", "A1", "A2"};
				for (uint8_t b = 0; b < 18; b++) {
					if (mask & (1u << b)) {
						if (tmp[0]) strncat(tmp, " ", sizeof(tmp) - strlen(tmp) - 1);
						strncat(tmp, names[b], sizeof(tmp) - strlen(tmp) - 1);
					}
				}
			} else {
				strncpy(tmp, "--", sizeof(tmp) - 1);
			}
			snprintf(topBuf, sizeof(topBuf), "GP%02d:%s", pinNum, tmp);
		}
		getRenderer()->drawText(0, 0, topBuf);
	} else {
		getRenderer()->drawText(0, 0, "No board layout");
	}

	// Bottom action bar (row 7).
	if (layoutElements.size() > 0) {
		getRenderer()->drawText(0, 7, "B1:assign B2:back");
	} else {
		getRenderer()->drawText(0, 7, "B2:back");
	}
}

void RemapScreen::drawActionSelect() {
	char lineBuf[22];

	snprintf(lineBuf, sizeof(lineBuf), "<%s[%d/%d]>",
		actionCategories[actionCategory].name,
		actionCategory + 1, actionCategoryCount);
	getRenderer()->drawText(0, 0, lineBuf);

	// Mark controls already assigned to the selected pin so toggling them
	// (on or off) is legible while browsing.
	uint32_t mask = 0;
	if (cursorIndex < layoutElements.size()) {
		uint8_t pin = layoutElements[cursorIndex].parameters.value;
		mask = Storage::getInstance().getGamepadMask(pin);
	}

	uint16_t catSize = actionCategories[actionCategory].count + 1; // +1 = Clear
	uint8_t pageSize = 4;
	uint16_t page = actionCategoryIndex / pageSize;
	uint16_t pageStart = page * pageSize;
	uint8_t onPage = catSize - pageStart;
	if (onPage > pageSize) onPage = pageSize;

	for (uint8_t i = 0; i < onPage; i++) {
		uint16_t idx = pageStart + i;
		getRenderer()->drawText(1, 2 + i, (idx == actionCategoryIndex) ? CHAR_RIGHT : " ");
		if (idx < actionCategories[actionCategory].count) {
			getRenderer()->drawText(2, 2 + i, actionCategories[actionCategory].entries[idx].name);
			if (mask & actionCategories[actionCategory].entries[idx].bit)
				getRenderer()->drawText(10, 2 + i, "*");
		} else {
			getRenderer()->drawText(2, 2 + i, "Clear");
		}
	}

	if (catSize > pageSize) {
		uint16_t totalPages = (catSize + pageSize - 1) / pageSize;
		snprintf(lineBuf, sizeof(lineBuf), "Page %d/%d", page + 1, totalPages);
		getRenderer()->drawText(11, 7, lineBuf);
	}
}

void RemapScreen::drawGamepadManage() {
	if (cursorIndex >= layoutElements.size()) return;
	char lineBuf[22];

	uint8_t pin = layoutElements[cursorIndex].parameters.value;
	uint32_t mask = Storage::getInstance().getGamepadMask(pin);
	ActionEntry setActions[18];
	uint8_t setCount = collectSetActions(mask, setActions);
	uint16_t itemCount = setCount + 1; // +1 = Add

	getRenderer()->drawText(0, 0, "Button mapping:");

	uint8_t pageSize = 4;
	uint16_t page = gpManageIndex / pageSize;
	uint16_t pageStart = page * pageSize;
	uint8_t onPage = itemCount - pageStart;
	if (onPage > pageSize) onPage = pageSize;

	for (uint8_t i = 0; i < onPage; i++) {
		uint16_t idx = pageStart + i;
		getRenderer()->drawText(1, 2 + i, (idx == gpManageIndex) ? CHAR_RIGHT : " ");
		if (idx < setCount) {
			getRenderer()->drawText(3, 2 + i, "x");
			getRenderer()->drawText(5, 2 + i, setActions[idx].name);
		} else {
			getRenderer()->drawText(3, 2 + i, "+ Add Button");
		}
	}

	if (itemCount > pageSize) {
		uint16_t totalPages = (itemCount + pageSize - 1) / pageSize;
		snprintf(lineBuf, sizeof(lineBuf), "Page %d/%d", page + 1, totalPages);
		getRenderer()->drawText(11, 7, lineBuf);
	}
}

void RemapScreen::drawKbdManage() {
	if (cursorIndex >= layoutElements.size()) return;
	uint8_t pin = layoutElements[cursorIndex].parameters.value;
	const KeyMapping& mapping = Storage::getInstance().getKeyMapping();
	uint8_t kc = (pin < mapping.keycodes_count) ? (uint8_t)mapping.keycodes[pin] : 0;
	uint8_t mod = (pin < mapping.modifierMasks_count) ? (uint8_t)mapping.modifierMasks[pin] : 0;

	getRenderer()->drawText(0, 0, "Key mapping:");

	uint8_t y = 2;
	if (kc) {
		getRenderer()->drawText(1, y, (kbdManageIndex == 0) ? CHAR_RIGHT : " ");
		getRenderer()->drawText(3, y, "x");
		getRenderer()->drawText(5, y, getKeyName(kc));
		y++;
	}
	if (mod) {
		uint8_t idx = (kc ? 1 : 0);
		getRenderer()->drawText(1, y, (kbdManageIndex == idx) ? CHAR_RIGHT : " ");
		getRenderer()->drawText(3, y, "x");
		getRenderer()->drawText(5, y, getModifierName(mod));
		y++;
	}
	uint8_t addIdx = (kc ? 1 : 0) + (mod ? 1 : 0);
	getRenderer()->drawText(1, y, (kbdManageIndex == addIdx) ? CHAR_RIGHT : " ");
	getRenderer()->drawText(3, y, kc ? "+ Add Mod" : "+ Add Key");
}

void RemapScreen::drawKbdSelect() {
	char lineBuf[22];

	snprintf(lineBuf, sizeof(lineBuf), "<%s[%d/%d]>",
		keyCategories[kbdCategory].name,
		kbdCategory + 1, kbdSelectCategoryCount);
	getRenderer()->drawText(0, 0, lineBuf);

	uint16_t catSize = keyCategories[kbdCategory].count;
	uint8_t pageSize = 4;
	uint16_t page = kbdCategoryIndex / pageSize;
	uint16_t pageStart = page * pageSize;
	uint8_t onPage = catSize - pageStart;
	if (onPage > pageSize) onPage = pageSize;

	for (uint8_t i = 0; i < onPage; i++) {
		uint16_t idx = pageStart + i;
		getRenderer()->drawText(1, 2 + i, (idx == kbdCategoryIndex) ? CHAR_RIGHT : " ");
		getRenderer()->drawText(2, 2 + i, keyCategories[kbdCategory].entries[idx].name);
	}

	if (catSize > pageSize) {
		uint16_t totalPages = (catSize + pageSize - 1) / pageSize;
		snprintf(lineBuf, sizeof(lineBuf), "Page %d/%d", page + 1, totalPages);
		getRenderer()->drawText(11, 7, lineBuf);
	}
}

void RemapScreen::drawKbdModifier() {
	char lineBuf[22];

	getRenderer()->drawText(0, 0, "Modifier");

	uint8_t pageSize = 4;
	uint8_t page = kbdModifierIndex / pageSize;
	uint8_t pageStart = page * pageSize;
	uint8_t onPage = modifierCount - pageStart;
	if (onPage > pageSize) onPage = pageSize;

	for (uint8_t i = 0; i < onPage; i++) {
		uint8_t idx = pageStart + i;
		getRenderer()->drawText(1, 2 + i, (idx == kbdModifierIndex) ? CHAR_RIGHT : " ");
		getRenderer()->drawText(2, 2 + i, modifierPresets[idx].name);
	}

	if (modifierCount > pageSize) {
		uint8_t totalPages = (modifierCount + pageSize - 1) / pageSize;
		snprintf(lineBuf, sizeof(lineBuf), "Page %d/%d", page + 1, totalPages);
		getRenderer()->drawText(11, 7, lineBuf);
	}
}

void RemapScreen::drawMidi() {
	char buf[8];

	getRenderer()->drawText((21 - (int)strlen("MIDI remap")) / 2, 0, "MIDI remap");

	// Three side-by-side fields, laid out like the LED color spinner: labels
	// on row 2, values on row 3, caret under the focused field on row 4.
	getRenderer()->drawText(2, 2, "Note");
	getRenderer()->drawText(9, 2, "Oct");
	getRenderer()->drawText(15, 2, "Vel");

	if (midiNoteIdx >= 0)
		snprintf(buf, sizeof(buf), "%s", midiPitchNames[midiNoteIdx]);
	else
		snprintf(buf, sizeof(buf), "None");
	getRenderer()->drawText(4 - (int)(strlen(buf) / 2), 3, buf);

	snprintf(buf, sizeof(buf), "%d", midiOctave);
	getRenderer()->drawText(10 - (int)(strlen(buf) / 2), 3, buf);

	snprintf(buf, sizeof(buf), "%d", midiVelocity);
	getRenderer()->drawText(16 - (int)(strlen(buf) / 2), 3, buf);

	static const uint8_t caretX[3] = {4, 10, 16};
	getRenderer()->drawText(caretX[midiField], 4, "^");

	// Hints (rows 6-7), matching the LED color spinner.
	getRenderer()->drawText(2, 6, CHAR_UP CHAR_DOWN ":val " CHAR_LEFT CHAR_RIGHT ":field");
	getRenderer()->drawText(3, 7, "B1:set B2:back");
}

void RemapScreen::drawScreen() {
	if (screenIsPrompting) {
		drawSavePrompt();
		return;
	}
	switch (mode) {
		case REMAP_LAYOUT:          drawLayout(); break;
		case REMAP_GAMEPAD_MANAGE:  drawGamepadManage(); break;
		case REMAP_ACTION_SELECT:   drawActionSelect(); break;
		case REMAP_KBD_MANAGE:      drawKbdManage(); break;
		case REMAP_KBD_SELECT:      drawKbdSelect(); break;
		case REMAP_KBD_MODIFIER:    drawKbdModifier(); break;
		case REMAP_MIDI:            drawMidi(); break;
	}
}

void RemapScreen::drawSavePrompt() {
	getRenderer()->drawText(1, 1, "Config has changed.");
	getRenderer()->drawText(3, 3, "Would you like");
	getRenderer()->drawText(6, 4, "to save?");

	if (promptChoice) getRenderer()->drawText(5, 6, CHAR_RIGHT);
	getRenderer()->drawText(6, 6, "Yes");
	if (!promptChoice) getRenderer()->drawText(11, 6, CHAR_RIGHT);
	getRenderer()->drawText(12, 6, "No");
}

void RemapScreen::save() {
	Storage::getInstance().save(true);
}

void RemapScreen::exitToMainMenu() {
	// No changes to commit here: staged changes are resolved by the exit
	// prompt (save or discard) before this is reached.
	returnToMenu = true;
}