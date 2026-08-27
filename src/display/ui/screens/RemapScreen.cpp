#include "RemapScreen.h"
#include "storagemanager.h"

// Same short keycode-name table used by the input history (keyboard mode).
static std::string remapKeycodeName(uint8_t code) {
	if (code == 0x00) return "(none)";
	if (code >= 0x04 && code <= 0x1D) return std::string(1, 'A' + (code - 0x04));
	if (code >= 0x1E && code <= 0x26) return std::string(1, '1' + (code - 0x1E));
	if (code == 0x27) return "0";
	if (code >= 0x3A && code <= 0x45) return "F" + std::to_string(code - 0x3A + 1);
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
		case 0x65: return "App";
		default: return "0x" + std::to_string(code);
	}
}

void RemapScreen::init() {
	getRenderer()->clearScreen();
	selectedPin = 0;
	assigning = false;
}

void RemapScreen::shutdown() {
	clearElements();
}

int8_t RemapScreen::update() {
	return -1;
}

int8_t RemapScreen::handleNavigation(uint8_t action) {
	Storage& s = Storage::getInstance();
	const uint32_t keyCount = s.getKeyCount();
	KeyMapping& mapping = s.getKeyMapping();

	if (!assigning) {
		switch (action) {
			case ACTION_UP:
				selectedPin = (selectedPin + keyCount - 1) % keyCount;
				break;
			case ACTION_DOWN:
				selectedPin = (selectedPin + 1) % keyCount;
				break;
			case ACTION_SELECT:
				assigning = true;
				break;
			case ACTION_BACK:
				save();
				return 1; // back to buttons
			default:
				break;
		}
		return -1;
	}

	// assigning: edit the selected pin's keycode
	switch (action) {
		case ACTION_LEFT: {
			uint8_t kc = (selectedPin < mapping.keycodes_count) ? (uint8_t)mapping.keycodes[selectedPin] : 0;
			mapping.keycodes[selectedPin] = kc - 1;
			break;
		}
		case ACTION_RIGHT: {
			uint8_t kc = (selectedPin < mapping.keycodes_count) ? (uint8_t)mapping.keycodes[selectedPin] : 0;
			mapping.keycodes[selectedPin] = kc + 1;
			break;
		}
		case ACTION_UP: {
			uint8_t kc = (selectedPin < mapping.keycodes_count) ? (uint8_t)mapping.keycodes[selectedPin] : 0;
			mapping.keycodes[selectedPin] = (uint8_t)(kc - 8);
			break;
		}
		case ACTION_DOWN: {
			uint8_t kc = (selectedPin < mapping.keycodes_count) ? (uint8_t)mapping.keycodes[selectedPin] : 0;
			mapping.keycodes[selectedPin] = (uint8_t)(kc + 8);
			break;
		}
		case ACTION_SELECT:
		case ACTION_BACK:
			assigning = false;
			break;
		default:
			break;
	}
	return -1;
}

void RemapScreen::save() {
	Storage::getInstance().save(true);
}

void RemapScreen::drawScreen() {
	Storage& s = Storage::getInstance();
	KeyMapping& mapping = s.getKeyMapping();
	const uint32_t keyCount = s.getKeyCount();

	uint8_t kc = (selectedPin < mapping.keycodes_count) ? (uint8_t)mapping.keycodes[selectedPin] : 0;

	getRenderer()->drawText((21 - 5) / 2, 0, "REMAP");
	getRenderer()->drawText(2, 2, assigning ? "Set keycode:" : "Select pin:");

	std::string line = "PIN " + std::to_string(selectedPin);
	if (selectedPin < mapping.keycodes_count)
		line += " [" + std::to_string(kc) + "]";
	getRenderer()->drawText(2, 3, line);

	getRenderer()->drawText(2, 5, remapKeycodeName(kc));

	if (assigning) {
		getRenderer()->drawText(2, 7, "L/R=1 U/D=8 A=ok B=back");
	} else {
		getRenderer()->drawText(2, 7, "A=edit B=save");
	}
}