#include "ButtonLayoutScreen.h"
#include "buttonlayouts.h"
#include "drivermanager.h"
#include "storagemanager.h"
#include "drivers/shared/gamepadhelper.h"

#include <cctype>

// ---- per-console input-name tables (ported from GP2040-CE) ----------------

static const char * displayNames[INPUT_HISTORY_MAX_MODES][INPUT_HISTORY_MAX_INPUTS] = {
	{		// HID / DINPUT
			CHAR_UP, CHAR_DOWN, CHAR_LEFT, CHAR_RIGHT,
			CHAR_UL, CHAR_UR, CHAR_DL, CHAR_DR,
			CHAR_CROSS, CHAR_CIRCLE, CHAR_SQUARE, CHAR_TRIANGLE,
			"L1", "R1", "L2", "R2",
			"SL", "ST", "L3", "R3", "PS", "A2"
	},
	{		// Switch
			CHAR_UP, CHAR_DOWN, CHAR_LEFT, CHAR_RIGHT,
			CHAR_UL, CHAR_UR, CHAR_DL, CHAR_DR,
			"B", "A", "Y", "X",
			"L", "R", "ZL", "ZR",
			"-", "+", "LS", "RS", CHAR_HOME_S, CHAR_CAP_S
	},
	{		// XInput
			CHAR_UP, CHAR_DOWN, CHAR_LEFT, CHAR_RIGHT,
			CHAR_UL, CHAR_UR, CHAR_DL, CHAR_DR,
			"A", "B", "X", "Y",
			"LB", "RB", "LT", "RT",
			CHAR_VIEW_X, CHAR_MENU_X, "LS", "RS", CHAR_HOME_X, "A2"
	},
	{		// Keyboard / HID-KB
			CHAR_UP, CHAR_DOWN, CHAR_LEFT, CHAR_RIGHT,
			CHAR_UL, CHAR_UR, CHAR_DL, CHAR_DR,
			"B1", "B2", "B3", "B4",
			"L1", "R1", "L2", "R2",
			"S1", "S2", "L3", "R3", "A1", "A2"
	},
	{		// PS4
			CHAR_UP, CHAR_DOWN, CHAR_LEFT, CHAR_RIGHT,
			CHAR_UL, CHAR_UR, CHAR_DL, CHAR_DR,
			CHAR_CROSS, CHAR_CIRCLE, CHAR_SQUARE, CHAR_TRIANGLE,
			"L1", "R1", "L2", "R2",
			CHAR_SHARE_P, "OP", "L3", "R3", CHAR_HOME_P, CHAR_TPAD_P
	},
	{		// GEN/MD Mini
			CHAR_UP, CHAR_DOWN, CHAR_LEFT, CHAR_RIGHT,
			CHAR_UL, CHAR_UR, CHAR_DL, CHAR_DR,
			"A", "B", "X", "Y",
			"", "Z", "", "C",
			"M", "S", "", "", "", ""
	},
	{		// Neo Geo Mini
			CHAR_UP, CHAR_DOWN, CHAR_LEFT, CHAR_RIGHT,
			CHAR_UL, CHAR_UR, CHAR_DL, CHAR_DR,
			"B", "D", "A", "C",
			"", "", "", "",
			"SE", "ST", "", "", "", ""
	},
	{		// PC Engine/TG16 Mini
			CHAR_UP, CHAR_DOWN, CHAR_LEFT, CHAR_RIGHT,
			CHAR_UL, CHAR_UR, CHAR_DL, CHAR_DR,
			"I", "II", "", "",
			"", "", "", "",
			"SE", "RUN", "", "", "", ""
	},
	{		// Egret II Mini
			CHAR_UP, CHAR_DOWN, CHAR_LEFT, CHAR_RIGHT,
			CHAR_UL, CHAR_UR, CHAR_DL, CHAR_DR,
			"A", "B", "C", "D",
			"", "E", "", "F",
			"CRD", "ST", "", "", "MN", ""
	},
	{		// Astro City Mini
			CHAR_UP, CHAR_DOWN, CHAR_LEFT, CHAR_RIGHT,
			CHAR_UL, CHAR_UR, CHAR_DL, CHAR_DR,
			"A", "B", "D", "E",
			"", "C", "", "F",
			"CRD", "ST", "", "", "", ""
	},
	{		// Original Xbox
			CHAR_UP, CHAR_DOWN, CHAR_LEFT, CHAR_RIGHT,
			CHAR_UL, CHAR_UR, CHAR_DL, CHAR_DR,
			"A", "B", "X", "Y",
			"BL", "WH", "L", "R",
			"BK", "ST", "LS", "RS", "", ""
	}
};

static const std::map<InputMode, uint16_t> displayModeLookup = {
	{INPUT_MODE_MIDI, 0},
	{INPUT_MODE_SWITCH_PRO, 1},
	{INPUT_MODE_XINPUT, 2},
	{INPUT_MODE_KEYBOARD, 3},
	{INPUT_MODE_CONFIG, 3},
};

// ---- keyboard keycode -> short name (ported from GP2040-CE) ----------------

static std::string keycodeToName(uint8_t code) {
	if (code == 0x00) return "";

	// A-Z (0x04-0x1D)
	if (code >= 0x04 && code <= 0x1D)
		return std::string(1, 'A' + (code - 0x04));

	// 1-9 (0x1E-0x26), 0 (0x27)
	if (code >= 0x1E && code <= 0x26)
		return std::string(1, '1' + (code - 0x1E));
	if (code == 0x27) return "0";

	switch (code) {
		case 0x28: return "Ent";
		case 0x29: return "Esc";
		case 0x2A: return "Bsp";
		case 0x2B: return "Tab";
		case 0x2C: return "Spc";
		case 0x2D: return "-";
		case 0x2E: return "=";
		case 0x2F: return "[";
		case 0x30: return "]";
		case 0x31: return "\\";
		case 0x33: return ";";
		case 0x34: return "'";
		case 0x35: return "`";
		case 0x36: return ",";
		case 0x37: return ".";
		case 0x38: return "/";
		case 0x39: return "Cap";
		case 0x46: return "PSc";
		case 0x47: return "Scr";
		case 0x48: return "Pau";
		case 0x49: return "Ins";
		case 0x4A: return "Hm";
		case 0x4B: return "PU";
		case 0x4C: return "Del";
		case 0x4D: return "End";
		case 0x4E: return "PD";
		case 0x4F: return "Rt";
		case 0x50: return "Lt";
		case 0x51: return "Dn";
		case 0x52: return "Up";
		case 0x53: return "Num";
		case 0x54: return "N/";
		case 0x55: return "N*";
		case 0x56: return "N-";
		case 0x57: return "N+";
		case 0x58: return "NE";
		case 0x65: return "App";
		case 0x66: return "Pwr";
		case 0x67: return "NEq";
		case 0xE0: return "CL";
		case 0xE1: return "SL";
		case 0xE2: return "AL";
		case 0xE3: return "GL";
		case 0xE4: return "CR";
		case 0xE5: return "SR";
		case 0xE6: return "AR";
		case 0xE7: return "GR";
		case 0xE8: return "Nxt";
		case 0xE9: return "Prv";
		case 0xF0: return "Stp";
		case 0xF1: return "P/P";
		case 0xF2: return "Mut";
		case 0xF3: return "V+";
		case 0xF4: return "V-";
	}

	// F1-F12 (0x3A-0x45)
	if (code >= 0x3A && code <= 0x45)
		return "F" + std::to_string(code - 0x3A + 1);

	// F13-F24 (0x68-0x73)
	if (code >= 0x68 && code <= 0x73)
		return "F" + std::to_string(code - 0x68 + 13);

	// Numpad 1-9 (0x59-0x61)
	if (code >= 0x59 && code <= 0x61)
		return "N" + std::string(1, '1' + (code - 0x59));

	if (code == 0x62) return "N0";
	if (code == 0x63) return "N.";

	return "";
}

static std::string modifierPrefix(uint8_t mask) {
	static const char* names[] = {"CL", "SL", "AL", "GL", "CR", "SR", "AR", "GR"};
	std::string prefix;
	for (uint8_t bit = 0; bit < 8; bit++) {
		if (mask & (1 << bit)) {
			prefix += names[bit];
			prefix += "+";
		}
	}
	return prefix;
}

void ButtonLayoutScreen::init() {
	isInputHistoryEnabled = getDisplayOptions().inputHistoryEnabled;
	inputHistoryX = getDisplayOptions().inputHistoryEnabled ? 0 : 0;
	inputHistoryY = 7;
	inputHistoryLength = 21;
	inputHistoryTimeout = getDisplayOptions().inputHistoryTimeout;
	lastInputTime = getMillis();
	bannerDelayStart = getMillis();
	inputMode = DriverManager::getInstance().getInputMode();

	footer = "";
	historyString = "";
	inputHistory.clear();
	lastInput.fill(false);

	// Layout coordinates map 1:1 to the panel (authored for 128x64): use the
	// full panel as the viewport so no scaling/offset is applied at draw time.
	// The status bar and input-history footer are drawn on top at fixed rows,
	// so layouts should keep their buttons clear of rows 0-7 and 56-63.
	setViewport(0, 0, getRenderer()->getDriver()->getMetrics()->height, getRenderer()->getDriver()->getMetrics()->width);

	// load layout (pushElement adds each element to the display list)
	LayoutManager::LayoutList currLayout = LayoutManager::getInstance().getLayout(
		(ButtonLayout)(getDisplayOptions().has_buttonLayout ? getDisplayOptions().buttonLayout : BUTTON_LAYOUT_BOARD_DEFINED));
	for (auto it = currLayout.begin(); it != currLayout.end(); ++it) {
		pushElement(*it);
	}

	// start with profile mode displayed
	bannerDisplay = true;
	prevProfileNumber = -1;

	prevLayout = getDisplayOptions().has_buttonLayout ? getDisplayOptions().buttonLayout : BUTTON_LAYOUT_BOARD_DEFINED;
	prevOrientation = getDisplayOptions().orientation;

	// any macro trigger assigned anywhere -> show the "M" status flag
	macroEnabled = false;
	for (uint32_t i = 0; i < Storage::getInstance().getConfig().macroIndices_count; i++) {
		if (Storage::getInstance().getConfig().macroIndices[i] > 0) { macroEnabled = true; break; }
	}

	showInputMode = true;
	showSocdMode = true;
	showMacroMode = true;
	showProfileMode = false;

	getRenderer()->clearScreen();
}

void ButtonLayoutScreen::shutdown() {
	clearElements();
}

int8_t ButtonLayoutScreen::update() {
	uint8_t profileNumber = Storage::getInstance().getActiveProfile();

	// reload if the layout changed in config mode
	bool configMode = Storage::getInstance().GetConfigMode();
	if (configMode) {
		uint8_t layout = getDisplayOptions().has_buttonLayout ? getDisplayOptions().buttonLayout : BUTTON_LAYOUT_BOARD_DEFINED;
		ButtonLayoutOrientation orientation = getDisplayOptions().orientation;
		bool inputHistoryEnabled = getDisplayOptions().inputHistoryEnabled;
		if ((prevLayout != layout) || (isInputHistoryEnabled != inputHistoryEnabled) || (prevOrientation != orientation)) {
			shutdown();
			init();
		}
	}

	// profile change banner
	if (prevProfileNumber != profileNumber) {
		bannerDelayStart = getMillis();
		prevProfileNumber = profileNumber;
		bannerDisplay = true;
	}

	generateHeader();
	if (isInputHistoryEnabled)
		processInputHistory();

	return -1;
}

void ButtonLayoutScreen::generateHeader() {
	// Limit to 21 chars with 6x8 font for now
	statusBar.clear();
	statusBarRight.clear();

	// Display Profile # banner
	if (bannerDisplay) {
		if (((getMillis() - bannerDelayStart) / 1000) < bannerDelay) {
			if (bannerMessage.empty()) {
				statusBar = "PROFILE " + std::to_string(Storage::getInstance().getActiveProfile());
			} else {
				statusBar = bannerMessage;
			}
			return;
		} else {
			bannerDisplay = false;
			bannerMessage.clear();
		}
	}

	if (showInputMode) {
		switch (inputMode) {
			case INPUT_MODE_KEYBOARD: statusBar += "HID-KB"; break;
			case INPUT_MODE_MIDI:     statusBar += "MIDI"; break;
			case INPUT_MODE_XINPUT:   statusBar += "XINPUT"; break;
			case INPUT_MODE_SWITCH_PRO: statusBar += "SWPRO"; break;
			case INPUT_MODE_CONFIG:   statusBar += "CONFIG"; break;
			default:                  statusBar += "HID"; break;
		}
	}

	if (showProfileMode) {
		statusBarRight += " P:";
		statusBarRight += std::to_string(Storage::getInstance().getActiveProfile());
	}

	if (showMacroMode && macroEnabled) statusBarRight += " M";

	if (showSocdMode) {
		switch (Storage::getInstance().getSocdMode()) {
			case SOCD_MODE_NEUTRAL:               statusBarRight += " SOCD-N"; break;
			case SOCD_MODE_UP_PRIORITY:           statusBarRight += " SOCD-U"; break;
			case SOCD_MODE_SECOND_INPUT_PRIORITY: statusBarRight += " SOCD-L"; break;
			case SOCD_MODE_FIRST_INPUT_PRIORITY:  statusBarRight += " SOCD-F"; break;
			case SOCD_MODE_BYPASS:                statusBarRight += " SOCD-X"; break;
			default: break;
		}
	}

	trim(statusBar);
	trim(statusBarRight);
}

void ButtonLayoutScreen::drawScreen() {
	if (bannerDisplay) {
		getRenderer()->drawRectangle(0, 0, 128, 7, false, true);
		getRenderer()->drawText(0, 0, statusBar, false);
	} else {
		uint8_t rightX = 21 - statusBarRight.length();
		getRenderer()->drawText(0, 0, statusBar);
		if (!statusBarRight.empty())
			getRenderer()->drawText(rightX, 0, statusBarRight);
	}
	if (isInputHistoryEnabled)
		getRenderer()->drawText(0, 7, footer);
}

GPLever* ButtonLayoutScreen::addLever(uint16_t startX, uint16_t startY, uint16_t sizeX, uint16_t sizeY, uint16_t strokeColor, uint16_t fillColor) {
	GPLever* lever = new GPLever();
	lever->setRenderer(getRenderer());
	lever->setPosition(startX, startY);
	lever->setStrokeColor(strokeColor);
	lever->setFillColor(fillColor);
	lever->setRadius(sizeX);
	lever->setShowCardinal(true);
	lever->setShowOrdinal(false);
	lever->setViewport(this->getViewport());
	return (GPLever*)addElement(lever);
}

GPButton* ButtonLayoutScreen::addButton(uint16_t startX, uint16_t startY, uint16_t sizeX, uint16_t sizeY, uint16_t strokeColor, uint16_t fillColor, int16_t inputMask) {
	GPButton* button = new GPButton();
	button->setRenderer(getRenderer());
	button->setPosition(startX, startY);
	button->setStrokeColor(strokeColor);
	button->setFillColor(fillColor);
	button->setSize(sizeX, sizeY);
	button->setInputMask(inputMask);
	button->setViewport(this->getViewport());
	return (GPButton*)addElement(button);
}

GPShape* ButtonLayoutScreen::addShape(uint16_t startX, uint16_t startY, uint16_t sizeX, uint16_t sizeY, uint16_t strokeColor, uint16_t fillColor) {
	GPShape* shape = new GPShape();
	shape->setRenderer(getRenderer());
	shape->setPosition(startX, startY);
	shape->setStrokeColor(strokeColor);
	shape->setFillColor(fillColor);
	shape->setSize(sizeX, sizeY);
	shape->setViewport(this->getViewport());
	return (GPShape*)addElement(shape);
}

GPSprite* ButtonLayoutScreen::addSprite(uint16_t startX, uint16_t startY, uint16_t sizeX, uint16_t sizeY) {
	GPSprite* sprite = new GPSprite();
	sprite->setRenderer(getRenderer());
	sprite->setPosition(startX, startY);
	sprite->setSize(sizeX, sizeY);
	sprite->setViewport(this->getViewport());
	return (GPSprite*)addElement(sprite);
}

GPWidget* ButtonLayoutScreen::pushElement(GPButtonLayout element) {
	if (element.elementType == GP_ELEMENT_LEVER) {
		return addLever(element.parameters.x1, element.parameters.y1, element.parameters.x2, element.parameters.y2, element.parameters.stroke, element.parameters.fill);
	} else if ((element.elementType == GP_ELEMENT_BTN_BUTTON) || (element.elementType == GP_ELEMENT_DIR_BUTTON) || (element.elementType == GP_ELEMENT_PIN_BUTTON)) {
		GPButton* button = addButton(element.parameters.x1, element.parameters.y1, element.parameters.x2, element.parameters.y2, element.parameters.stroke, element.parameters.fill, element.parameters.value);

		// set type of button
		button->setInputType(element.elementType);
		button->setShape((GPShape_Type)element.parameters.shape);
		button->setAngle(element.parameters.angleStart);
		button->setAngleEnd(element.parameters.angleEnd);
		button->setClosed(element.parameters.closed);

		return (GPWidget*)button;
	} else if (element.elementType == GP_ELEMENT_SPRITE) {
		return addSprite(element.parameters.x1, element.parameters.y1, element.parameters.x2, element.parameters.y2);
	} else if (element.elementType == GP_ELEMENT_SHAPE) {
		GPShape* shape = addShape(element.parameters.x1, element.parameters.y1, element.parameters.x2, element.parameters.y2, element.parameters.stroke, element.parameters.fill);
		shape->setShape((GPShape_Type)element.parameters.shape);
		shape->setAngle(element.parameters.angleStart);
		shape->setAngleEnd(element.parameters.angleEnd);
		shape->setClosed(element.parameters.closed);
		return shape;
	}
	return NULL;
}

void ButtonLayoutScreen::processInputHistory() {
	std::deque<std::string> pressed;

	// Current input snapshot. Gamepad modes resolve the 22 controls through the
	// assembled gamepad state (dpad exact-match + buttons); keyboard mode shows
	// the actual pressed keycodes instead.
	const bool keyboardMode = (inputMode == INPUT_MODE_KEYBOARD);
	GamepadState state;
	if (!keyboardMode)
		buildGamepadState(state);

	std::array<bool, INPUT_HISTORY_MAX_INPUTS> currentInput = {
		!keyboardMode && (state.dpad == GAMEPAD_MASK_UP),
		!keyboardMode && (state.dpad == GAMEPAD_MASK_DOWN),
		!keyboardMode && (state.dpad == GAMEPAD_MASK_LEFT),
		!keyboardMode && (state.dpad == GAMEPAD_MASK_RIGHT),
		!keyboardMode && (state.dpad == (GAMEPAD_MASK_UP | GAMEPAD_MASK_LEFT)),
		!keyboardMode && (state.dpad == (GAMEPAD_MASK_UP | GAMEPAD_MASK_RIGHT)),
		!keyboardMode && (state.dpad == (GAMEPAD_MASK_DOWN | GAMEPAD_MASK_LEFT)),
		!keyboardMode && (state.dpad == (GAMEPAD_MASK_DOWN | GAMEPAD_MASK_RIGHT)),
		!keyboardMode && ((state.buttons & GAMEPAD_MASK_B1) != 0),
		!keyboardMode && ((state.buttons & GAMEPAD_MASK_B2) != 0),
		!keyboardMode && ((state.buttons & GAMEPAD_MASK_B3) != 0),
		!keyboardMode && ((state.buttons & GAMEPAD_MASK_B4) != 0),
		!keyboardMode && ((state.buttons & GAMEPAD_MASK_L1) != 0),
		!keyboardMode && ((state.buttons & GAMEPAD_MASK_R1) != 0),
		!keyboardMode && ((state.buttons & GAMEPAD_MASK_L2) != 0),
		!keyboardMode && ((state.buttons & GAMEPAD_MASK_R2) != 0),
		!keyboardMode && ((state.buttons & GAMEPAD_MASK_S1) != 0),
		!keyboardMode && ((state.buttons & GAMEPAD_MASK_S2) != 0),
		!keyboardMode && ((state.buttons & GAMEPAD_MASK_L3) != 0),
		!keyboardMode && ((state.buttons & GAMEPAD_MASK_R3) != 0),
		!keyboardMode && ((state.buttons & GAMEPAD_MASK_A1) != 0),
		!keyboardMode && ((state.buttons & GAMEPAD_MASK_A2) != 0),
	};

	// Track last input time
	for (auto b : currentInput) {
		if (b) { lastInputTime = getMillis(); break; }
	}

	uint8_t mode = (displayModeLookup.count(inputMode) > 0) ? displayModeLookup.at(inputMode) : 0;
	if (inputMode == INPUT_MODE_SWITCH_PRO && !Storage::getInstance().getUseNintendoLayout())
		mode = 2;

	// Check if any new keys have been pressed
	if (lastInput != currentInput) {
		if (keyboardMode) {
			// Keyboard mode: list every held pin's keycode (with modifier prefix).
			Storage& s = Storage::getInstance();
			const KeyMask keyState = s.getKeyState();
			KeyMapping& mapping = s.getKeyMapping();
			const uint32_t keyCount = s.getKeyCount();
			for (uint32_t pin = 0; pin < keyCount; pin++) {
				if (!keyState.test(pin)) continue;
				uint8_t kc = (pin < mapping.keycodes_count) ? (uint8_t)mapping.keycodes[pin] : 0;
				if (kc == 0) continue;
				uint8_t mod = (pin < mapping.modifierMasks_count) ? (uint8_t)mapping.modifierMasks[pin] : 0;
				std::string name = modifierPrefix(mod) + keycodeToName(kc);
				if (!name.empty()) pressed.push_back(name);
			}
		} else {
			// Gamepad mode: map held controls to the console-specific names.
			for (uint8_t x = 0; x < INPUT_HISTORY_MAX_INPUTS; x++) {
				if (currentInput[x]) {
					std::string inputChar = std::string(displayNames[mode][x]);
					if (!inputChar.empty()) pressed.push_back(inputChar);
				}
			}
		}
		// Update the last keypress array
		lastInput = currentInput;
	}

	if (pressed.size() > 0) {
		std::string newInput;
		for (const auto &s : pressed) {
			if (!newInput.empty()) newInput += "+";
			newInput += s;
		}
		inputHistory.push_back(newInput);
	}

	if (inputHistory.size() > (inputHistoryLength / 2) + 1) {
		inputHistory.pop_front();
	}

	std::string ret;
	for (auto it = inputHistory.crbegin(); it != inputHistory.crend(); ++it) {
		std::string newRet = ret;
		if (!newRet.empty()) newRet = " " + newRet;
		newRet = *it + newRet;
		ret = newRet;
		if (ret.size() >= inputHistoryLength) break;
	}

	if (ret.size() >= inputHistoryLength) {
		historyString = ret.substr(ret.size() - inputHistoryLength);
	} else {
		historyString = ret;
	}

	// Clear history on inactivity timeout
	if (inputHistoryTimeout > 0 && !inputHistory.empty()) {
		if ((getMillis() - lastInputTime) > (inputHistoryTimeout * 1000)) {
			inputHistory.clear();
			historyString.clear();
		}
	}

	footer = historyString;
}

void ButtonLayoutScreen::trim(std::string &s) {
	s.erase(s.begin(), std::find_if(s.begin(), s.end(),
			std::not1(std::ptr_fun<int, int>(std::isspace))));
}