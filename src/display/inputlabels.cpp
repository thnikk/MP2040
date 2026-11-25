#include "display/inputlabels.h"

#include "storagemanager.h"

#include <cctype>

// ---- per-console input-name tables (ported from GP2040-CE) ----------------
//
// Index order matches the gamepad state: dpad directions (0-3), diagonals
// (4-7, not directly mappable from a pin mask), then buttons B1..A2 (8-21).
static const char* displayNames[][22] = {
	{		// HID / DINPUT
		"UP", "DN", "LT", "RT",
		"UL", "UR", "DL", "DR",
		"B1", "B2", "B3", "B4",
		"L1", "R1", "L2", "R2",
		"S1", "S2", "L3", "R3", "A1", "A2"
	},
	{		// Switch
		"UP", "DN", "LT", "RT",
		"UL", "UR", "DL", "DR",
		"B", "A", "Y", "X",
		"L", "R", "ZL", "ZR",
		"-", "+", "LS", "RS", "HOME", "CAP"
	},
	{		// XInput
		"UP", "DN", "LT", "RT",
		"UL", "UR", "DL", "DR",
		"A", "B", "X", "Y",
		"LB", "RB", "LT", "RT",
		"VIEW", "MENU", "LS", "RS", "HOME", "A2"
	},
	{		// Keyboard / HID-KB
		"UP", "DN", "LT", "RT",
		"UL", "UR", "DL", "DR",
		"B1", "B2", "B3", "B4",
		"L1", "R1", "L2", "R2",
		"S1", "S2", "L3", "R3", "A1", "A2"
	},
	{		// PS4 / PS3 / PS5
		"UP", "DN", "LT", "RT",
		"UL", "UR", "DL", "DR",
		"X", "O", "SQ", "TR",
		"L1", "R1", "L2", "R2",
		"SHARE", "OPT", "L3", "R3", "HOME", "TPAD"
	},
	{		// Xbox One
		"UP", "DN", "LT", "RT",
		"UL", "UR", "DL", "DR",
		"A", "B", "X", "Y",
		"LB", "RB", "LT", "RT",
		"VIEW", "MENU", "LS", "RS", "HOME", "CAP"
	},
};

// ---- keyboard keycode -> short name (ported from GP2040-CE) ----------------

std::string keycodeToName(uint8_t code)
{
	if (code == 0x00) return "";

	// A-Z (0x04-0x1D)
	if (code >= 0x04 && code <= 0x1D)
		return std::string(1, (char)('A' + (code - 0x04)));

	// 1-9 (0x1E-0x26), 0 (0x27)
	if (code >= 0x1E && code <= 0x26)
		return std::string(1, (char)('1' + (code - 0x1E)));
	if (code == 0x27) return "0";

	switch (code) {
		case 0x28: return "ENT";
		case 0x29: return "ESC";
		case 0x2A: return "BSP";
		case 0x2B: return "TAB";
		case 0x2C: return "SPC";
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
		case 0x39: return "CAP";
		case 0x46: return "PSC";
		case 0x47: return "SCR";
		case 0x48: return "PAU";
		case 0x49: return "INS";
		case 0x4A: return "HM";
		case 0x4B: return "PU";
		case 0x4C: return "DEL";
		case 0x4D: return "END";
		case 0x4E: return "PD";
		case 0x4F: return "RT";
		case 0x50: return "LT";
		case 0x51: return "DN";
		case 0x52: return "UP";
		case 0x53: return "NUM";
		case 0x54: return "N/";
		case 0x55: return "N*";
		case 0x56: return "N-";
		case 0x57: return "N+";
		case 0x58: return "NE";
		case 0x65: return "APP";
		case 0x66: return "PWR";
		case 0x67: return "NEQ";
		case 0xE0: return "CL";
		case 0xE1: return "SL";
		case 0xE2: return "AL";
		case 0xE3: return "GL";
		case 0xE4: return "CR";
		case 0xE5: return "SR";
		case 0xE6: return "AR";
		case 0xE7: return "GR";
		case 0xE8: return "NXT";
		case 0xE9: return "PRV";
		case 0xF0: return "STP";
		case 0xF1: return "P/P";
		case 0xF2: return "MUT";
		case 0xF3: return "V+";
		case 0xF4: return "V-";
	}

	// F1-F12 (0x3A-0x45), F13-F24 (0x68-0x73)
	if (code >= 0x3A && code <= 0x45)
		return "F" + std::to_string(code - 0x3A + 1);
	if (code >= 0x68 && code <= 0x73)
		return "F" + std::to_string(code - 0x68 + 13);

	// Numpad 1-9 (0x59-0x61), 0, .
	if (code >= 0x59 && code <= 0x61)
		return "N" + std::string(1, (char)('1' + (code - 0x59)));
	if (code == 0x62) return "N0";
	if (code == 0x63) return "N.";

	return "";
}

std::string modifierPrefix(uint8_t mask)
{
	static const char* names[] = { "CL", "SL", "AL", "GL", "CR", "SR", "AR", "GR" };
	std::string prefix;
	for (uint8_t bit = 0; bit < 8; bit++)
	{
		if (mask & (1 << bit))
		{
			prefix += names[bit];
			prefix += "+";
		}
	}
	return prefix;
}

std::string midiNoteToName(uint8_t note)
{
	static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
	return std::string(names[note % 12]) + std::to_string((int)note / 12 - 1);
}

std::string gamepadMaskToName(uint16_t mask, InputMode mode, bool nintendoLayout)
{
	if (mask == 0)
		return "";

	uint8_t table = 0; // HID / DINPUT names
	switch (mode)
	{
		case INPUT_MODE_SWITCH_PRO: table = nintendoLayout ? 1 : 2; break;
		case INPUT_MODE_XINPUT: table = 2; break;
		case INPUT_MODE_XBOX_ONE: table = 5; break;
		case INPUT_MODE_PS3:
		case INPUT_MODE_PS4:
		case INPUT_MODE_PS5: table = 4; break;
		case INPUT_MODE_KEYBOARD:
		default: table = 3; break;
	}

	std::string out;
	auto append = [&out](const char* name) {
		if (!name || !name[0]) return;
		if (!out.empty()) out += "+";
		out += name;
	};

	// Dpad directions.
	for (uint8_t i = 0; i < 4; i++)
		if (mask & (1u << i))
			append(displayNames[table][i]);

	// Buttons B1..A2 live in bits 4-17 of the pin mask and map to display name
	// slots 8-21.
	for (uint8_t i = 0; i < 14; i++)
		if (mask & (1u << (4 + i)))
			append(displayNames[table][8 + i]);

	return out;
}

std::string inputLabelForPin(uint32_t pin, InputMode mode, bool nintendoLayout)
{
	Storage& s = Storage::getInstance();
	const Config& config = s.getConfig();
	const KeyMapping& km = s.getKeyMapping();

	if (pin < (uint32_t)MAX_KEYS && pin < (uint32_t)config.macroIndices_count &&
	    config.macroIndices[pin] != 0)
		return "M" + std::to_string(config.macroIndices[pin]);

	const uint8_t keycode = pin < (uint32_t)km.keycodes_count
		? (uint8_t)km.keycodes[pin] : 0;
	const uint8_t modifiers = pin < (uint32_t)km.modifierMasks_count
		? (uint8_t)km.modifierMasks[pin] : 0;
	const uint8_t midiNote = pin < (uint32_t)km.midiNotes_count
		? (uint8_t)km.midiNotes[pin] : 0;

	if (mode == INPUT_MODE_MIDI)
		return midiNote ? midiNoteToName(midiNote) : "";

	if (mode == INPUT_MODE_XINPUT || mode == INPUT_MODE_SWITCH_PRO ||
	    mode == INPUT_MODE_XBOX_ONE || mode == INPUT_MODE_PS3 ||
	    mode == INPUT_MODE_PS4 || mode == INPUT_MODE_PS5)
		return gamepadMaskToName((uint16_t)s.getGamepadMask(pin), mode, nintendoLayout);

	// Keyboard: a modifier-only key has no keycode but still has a label.
	std::string name = modifierPrefix(modifiers);
	name += keycodeToName(keycode);
	return name;
}
