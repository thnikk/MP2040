/*
 * SPDX-License-Identifier: MIT
 *
 * GP2040-th config migration: minimal protobuf wire-format extractor plus the
 * MP2040 Config mapping. See headers/gp2040migrate.h for the design notes.
 */

#include "config.pb.h"
#include "enums.pb.h"
#include "gp2040migrate.h"
#include "keymask.h" // MAX_KEYS

// GP2040-th Config field numbers (proto/config.proto).
#define GP_F_GAMEPAD 2
#define GP_F_LED 7
#define GP_F_ANIM 8
#define GP_F_PROFILES 11
#define GP_F_BOARD_CONFIG 12
#define GP_F_GPIO 13

// GP2040-th GamepadOptions fields.
#define GP_GP_INPUT_MODE 1
#define GP_GP_DPAD_MODE 2
#define GP_GP_SOCD_MODE 3
#define GP_GP_PROFILE_NUMBER 9
#define GP_GP_DEBOUNCE 11
#define GP_GP_NINTENDO_LAYOUT 39

// GP2040-th GpioMappings fields.
#define GP_GPIO_PINS 1
#define GP_GPIO_KEYCODES 4
#define GP_GPIO_MODIFIERS 5

// GP2040-th GpioMappingInfo fields.
#define GP_PIN_ACTION 1
#define GP_PIN_CUSTOM_DPAD 3
#define GP_PIN_CUSTOM_BUTTONS 4

// GP2040-th ProfileOptions fields.
#define GP_PROF_SETS 2

// GP2040-th LEDOptions fields.
#define GP_LED_BRIGHTNESS 5

// GP2040-th AnimationOptions_Proto fields.
#define GP_ANIM_INDEX 1
#define GP_ANIM_HAS_CUSTOM 8
#define GP_ANIM_THEME_FIRST 9
#define GP_ANIM_THEME_LAST 26
#define GP_ANIM_THEME_PRESSED_FIRST 27
#define GP_ANIM_THEME_PRESSED_LAST 44
#define GP_ANIM_STATIC_NORMAL 46
#define GP_ANIM_STATIC_PRESSED 47

// GP2040-th GpioAction values with an MP2040 gamepad equivalent (BUTTON_PRESS_*).
// MP2040 packs dpad (bits 0-3) and buttons B1-A2 (bits 4-17) into one mask.
static const uint32_t actionBits[] = {
	1u << 0, // 1 up
	1u << 1, // 2 down
	1u << 2, // 3 left
	1u << 3, // 4 right
	1u << 4, // 5 B1
	1u << 5, // 6 B2
	1u << 6, // 7 B3
	1u << 7, // 8 B4
	1u << 8, // 9 L1
	1u << 9, // 10 R1
	1u << 10, // 11 L2
	1u << 11, // 12 R2
	1u << 12, // 13 S1
	1u << 13, // 14 S2
	1u << 16, // 15 A1
	1u << 17, // 16 A2
	1u << 14, // 17 L3
	1u << 15, // 18 R3
};

// GP2040-th customButtonMask bit (B1=0..A2=13) to MP2040 mask bit.
static const uint32_t buttonBits[] = {
	1u << 4, 1u << 5, 1u << 6, 1u << 7, 1u << 8, 1u << 9, 1u << 10, 1u << 11,
	1u << 12, 1u << 13, 1u << 14, 1u << 15, 1u << 16, 1u << 17,
};

// GP2040-th GpioAction to custom-theme button index (GP2040_THEME_BUTTONS).
static const int8_t themeIndex[] = {
	0, 1, 2, 3, // 1-4 Up/Down/Left/Right
	4, 5, 6, 7, 8, 9, 10, 11, 12, 13, // 5-14 B1..S2
	16, 17, // 15-16 A1/A2
	14, 15, // 17-18 L3/R3
};

uint32_t gp2040GamepadMask(int32_t action, uint32_t customButtons, uint32_t customDpad, bool* mapped)
{
	bool ok = true;
	uint32_t mask = 0;
	if (action == -10 || action == -5 || action == 0)
	{
		mask = 0; // NONE / RESERVED / ASSIGNED_TO_ADDON: neutral
	}
	else if (action >= 1 && action <= 18)
	{
		mask = actionBits[action - 1];
	}
	else if (action == 40) // CUSTOM_BUTTON_COMBO
	{
		mask = customDpad & 0xfu;
		for (uint32_t b = 0; b < 14; b++)
		{
			if (customButtons & (1u << b))
				mask |= buttonBits[b];
		}
	}
	else
	{
		ok = false; // menu nav, sustain, turbo, macros, ...: no equivalent
	}
	if (mapped)
		*mapped = ok;
	return mask;
}

int gp2040InputMode(uint32_t gpInput, bool* fallback)
{
	bool fb = false;
	int mode = INPUT_MODE_KEYBOARD;
	switch (gpInput)
	{
	case 3: mode = INPUT_MODE_KEYBOARD; break;
	case 0: mode = INPUT_MODE_XINPUT; break;
	case 15: mode = INPUT_MODE_SWITCH_PRO; break;
	case 5: mode = INPUT_MODE_XBOX_ONE; break;
	case 2: mode = INPUT_MODE_PS3; break;
	case 4: mode = INPUT_MODE_PS4; break;
	case 13: mode = INPUT_MODE_PS5; break;
	default: mode = INPUT_MODE_KEYBOARD; fb = true; break;
	}
	if (fallback)
		*fallback = fb;
	return mode;
}

int gp2040LedMode(uint32_t gpAnim, bool* known)
{
	bool ok = true;
	int mode = 0;
	switch (gpAnim)
	{
	case 0: // static
	case 3: // static theme
	case 5: // custom theme
		mode = 0; // custom
		break;
	case 1: mode = 5; break; // rainbow -> rain
	case 2: mode = 1; break; // chase -> cycle
	case 4: mode = 4; break; // ripple -> ripple
	default: ok = false; break;
	}
	if (known)
		*known = ok;
	return mode;
}

int gp2040ThemeIndex(int32_t action)
{
	if (action < 1 || action > 18)
		return -1;
	return themeIndex[action - 1];
}

// -----------------------------------------------------
// Wire-format reader
// -----------------------------------------------------

typedef struct
{
	const uint8_t* p;
	const uint8_t* end;
} GpCursor;

static bool gpVarint(GpCursor* c, uint64_t* v)
{
	uint64_t val = 0;
	for (uint32_t shift = 0; shift < 70; shift += 7)
	{
		if (c->p >= c->end)
			return false;
		uint8_t b = *c->p++;
		val |= (uint64_t)(b & 0x7fu) << shift;
		if ((b & 0x80u) == 0)
		{
			*v = val;
			return true;
		}
	}
	return false; // overlong / malformed
}

static bool gpTag(GpCursor* c, uint32_t* field, uint32_t* wire)
{
	uint64_t tag;
	if (!gpVarint(c, &tag))
		return false;
	*field = (uint32_t)(tag >> 3);
	*wire = (uint32_t)(tag & 7u);
	return *field != 0;
}

// Skip one value; for length-delimited also returns its span.
static bool gpSkip(GpCursor* c, uint32_t wire, const uint8_t** val, size_t* vlen)
{
	uint64_t v;
	switch (wire)
	{
	case 0: return gpVarint(c, &v);
	case 1:
		if ((size_t)(c->end - c->p) < 8)
			return false;
		c->p += 8;
		return true;
	case 2: {
		if (!gpVarint(c, &v))
			return false;
		if (v > (uint64_t)(c->end - c->p))
			return false;
		if (val)
			*val = c->p;
		if (vlen)
			*vlen = (size_t)v;
		c->p += (size_t)v;
		return true;
	}
	case 5:
		if ((size_t)(c->end - c->p) < 4)
			return false;
		c->p += 4;
		return true;
	default: return false; // groups are unused by nanopb
	}
}

bool gp2040LooksLike(const uint8_t* data, size_t len)
{
	if (!data || len == 0)
		return false;
	GpCursor c = { data, data + len };
	while (c.p < c.end)
	{
		uint32_t field, wire;
		if (!gpTag(&c, &field, &wire))
			return false;
		if ((field == GP_F_BOARD_CONFIG || field == GP_F_GPIO) && wire == 2)
			return true;
		if (!gpSkip(&c, wire, nullptr, nullptr))
			return false;
	}
	return false;
}

// -----------------------------------------------------
// Message parsers (each fills part of Gp2040Config)
// -----------------------------------------------------

static void gpParsePin(const uint8_t* data, size_t len, Gp2040PinSet* set, uint32_t pin)
{
	GpCursor c = { data, data + len };
	while (c.p < c.end)
	{
		uint32_t field, wire;
		uint64_t v;
		if (!gpTag(&c, &field, &wire))
			return;
		if (wire != 0)
		{
			if (!gpSkip(&c, wire, nullptr, nullptr))
				return;
			continue;
		}
		if (!gpVarint(&c, &v))
			return;
		if (field == GP_PIN_ACTION)
			set->actions[pin] = (int32_t)(v & 0xffffffffu); // sign-extends negatives
		else if (field == GP_PIN_CUSTOM_DPAD)
			set->customDpadMasks[pin] = (uint32_t)v;
		else if (field == GP_PIN_CUSTOM_BUTTONS)
			set->customButtonMasks[pin] = (uint32_t)v;
	}
}

static void gpParseGpioSet(const uint8_t* data, size_t len, Gp2040PinSet* set)
{
	uint32_t pin = 0, keys = 0, mods = 0;
	GpCursor c = { data, data + len };
	while (c.p < c.end)
	{
		uint32_t field, wire;
		const uint8_t* val = nullptr;
		size_t vlen = 0;
		uint64_t v;
		if (!gpTag(&c, &field, &wire))
			return;
		if (field == GP_GPIO_PINS && wire == 2)
		{
			if (!gpSkip(&c, wire, &val, &vlen))
				return;
			if (pin < GP2040_PIN_COUNT)
				gpParsePin(val, vlen, set, pin);
			pin++;
		}
		else if ((field == GP_GPIO_KEYCODES || field == GP_GPIO_MODIFIERS) && wire == 0)
		{
			if (!gpVarint(&c, &v))
				return;
			if (field == GP_GPIO_KEYCODES)
			{
				if (keys < GP2040_PIN_COUNT)
					set->keycodes[keys] = (uint32_t)v > 255 ? 255 : (uint32_t)v;
				keys++;
			}
			else
			{
				if (mods < GP2040_PIN_COUNT)
					set->modifiers[mods] = (uint32_t)v > 255 ? 255 : (uint32_t)v;
				mods++;
			}
		}
		else if (!gpSkip(&c, wire, nullptr, nullptr))
		{
			return;
		}
	}
}

static void gpParseGamepad(const uint8_t* data, size_t len, Gp2040Config* out)
{
	out->hasGamepad = true;
	GpCursor c = { data, data + len };
	while (c.p < c.end)
	{
		uint32_t field, wire;
		uint64_t v;
		if (!gpTag(&c, &field, &wire))
			return;
		if (wire != 0)
		{
			if (!gpSkip(&c, wire, nullptr, nullptr))
				return;
			continue;
		}
		if (!gpVarint(&c, &v))
			return;
		switch (field)
		{
		case GP_GP_INPUT_MODE: out->inputMode = (uint32_t)v; out->hasInputMode = true; break;
		case GP_GP_DPAD_MODE: out->dpadMode = (uint32_t)v; out->hasDpadMode = true; break;
		case GP_GP_SOCD_MODE: out->socdMode = (uint32_t)v; out->hasSocdMode = true; break;
		case GP_GP_PROFILE_NUMBER: out->profileNumber = (uint32_t)v; out->hasProfileNumber = true; break;
		case GP_GP_DEBOUNCE: out->debounceDelay = (uint32_t)v; out->hasDebounce = true; break;
		case GP_GP_NINTENDO_LAYOUT: out->useNintendoLayout = v != 0; break;
		default: break;
		}
	}
}

static void gpParseAnim(const uint8_t* data, size_t len, Gp2040Config* out)
{
	GpCursor c = { data, data + len };
	while (c.p < c.end)
	{
		uint32_t field, wire;
		uint64_t v;
		if (!gpTag(&c, &field, &wire))
			return;
		if (wire != 0)
		{
			if (!gpSkip(&c, wire, nullptr, nullptr))
				return;
			continue;
		}
		if (!gpVarint(&c, &v))
			return;
		if (field == GP_ANIM_INDEX)
		{
			out->animIndex = (uint32_t)v;
			out->hasAnim = true;
		}
		else if (field == GP_ANIM_HAS_CUSTOM)
		{
			out->hasCustomTheme = v != 0;
		}
		else if (field >= GP_ANIM_THEME_FIRST && field <= GP_ANIM_THEME_LAST)
		{
			uint32_t i = field - GP_ANIM_THEME_FIRST;
			out->themeNormal[i] = (uint32_t)v & 0xffffffu;
			out->hasTheme[i] = true;
		}
		else if (field >= GP_ANIM_THEME_PRESSED_FIRST && field <= GP_ANIM_THEME_PRESSED_LAST)
		{
			uint32_t i = field - GP_ANIM_THEME_PRESSED_FIRST;
			out->themePressed[i] = (uint32_t)v & 0xffffffu;
			out->hasTheme[i] = true;
		}
		else if (field == GP_ANIM_STATIC_NORMAL)
		{
			out->staticNormal = (uint32_t)v & 0xffffffu;
			out->hasStaticColors = true;
		}
		else if (field == GP_ANIM_STATIC_PRESSED)
		{
			out->staticPressed = (uint32_t)v & 0xffffffu;
			out->hasStaticColors = true;
		}
	}
}

bool gp2040Extract(const uint8_t* data, size_t len, Gp2040Config* out)
{
	if (!data || len == 0 || !out)
		return false;
	*out = Gp2040Config();
	// GpioAction default: the proto default (first enum value) is NONE (-10).
	for (uint32_t s = 0; s < GP2040_SET_COUNT; s++)
		for (uint32_t p = 0; p < GP2040_PIN_COUNT; p++)
			out->sets[s].actions[p] = -10;
	uint32_t altSets = 0;
	GpCursor c = { data, data + len };
	while (c.p < c.end)
	{
		uint32_t field, wire;
		const uint8_t* val = nullptr;
		size_t vlen = 0;
		uint64_t v;
		if (!gpTag(&c, &field, &wire))
			break; // trailing garbage: keep what decoded so far
		if (wire == 2 && (field == GP_F_GAMEPAD || field == GP_F_LED || field == GP_F_ANIM ||
				field == GP_F_PROFILES || field == GP_F_GPIO))
		{
			if (!gpSkip(&c, wire, &val, &vlen))
				break;
			if (field == GP_F_GPIO)
			{
				gpParseGpioSet(val, vlen, &out->sets[0]);
				if (out->setCount == 0)
					out->setCount = 1;
			}
			else if (field == GP_F_PROFILES)
			{
				GpCursor pc = { val, val + vlen };
				while (pc.p < pc.end)
				{
					uint32_t pf, pw;
					const uint8_t* pv = nullptr;
					size_t pvlen = 0;
					if (!gpTag(&pc, &pf, &pw))
						break;
					if (pf == GP_PROF_SETS && pw == 2)
					{
						if (!gpSkip(&pc, pw, &pv, &pvlen))
							break;
						if (altSets < GP2040_SET_COUNT - 1)
						{
							gpParseGpioSet(pv, pvlen, &out->sets[1 + altSets]);
							altSets++;
							out->setCount = (uint8_t)(1 + altSets);
						}
					}
					else if (!gpSkip(&pc, pw, nullptr, nullptr))
					{
						break;
					}
				}
			}
			else if (field == GP_F_GAMEPAD)
			{
				gpParseGamepad(val, vlen, out);
			}
			else if (field == GP_F_ANIM)
			{
				gpParseAnim(val, vlen, out);
			}
			else // GP_F_LED
			{
				GpCursor lc = { val, val + vlen };
				while (lc.p < lc.end)
				{
					uint32_t lf, lw;
					if (!gpTag(&lc, &lf, &lw))
						break;
					if (lf == GP_LED_BRIGHTNESS && lw == 0)
					{
						if (!gpVarint(&lc, &v))
							break;
						out->brightnessMaximum = (uint32_t)v > 255 ? 255 : (uint32_t)v;
						out->hasBrightness = true;
					}
					else if (!gpSkip(&lc, lw, nullptr, nullptr))
					{
						break;
					}
				}
			}
		}
		else if (!gpSkip(&c, wire, nullptr, nullptr))
		{
			break;
		}
	}
	return out->setCount > 0 || out->hasGamepad || out->hasBrightness ||
		out->hasStaticColors || out->hasAnim;
}

// Clamp helpers (mirror the web converter's gpClamp).
static uint32_t gpClampU(uint32_t v, uint32_t hi)
{
	return v > hi ? hi : v;
}

// -----------------------------------------------------
// MP2040 Config mapping (mirrors www/js/core/gp2040.js)
// -----------------------------------------------------

void gp2040MigrateToConfig(const Gp2040Config& src, Config& config)
{
	// Per-profile key maps. Missing alternates copy the base (like
	// seedProfiles); pins past GP2040_PIN_COUNT keep the board defaults from
	// applyDefaults. Unmapped (zero) entries are backfilled with board
	// defaults by normalizeKeyMapping afterwards, as with a web import.
	if (src.setCount > 0)
	{
		bool knownAnim = false;
		int ledMode = 0;
		if (src.hasAnim)
			ledMode = gp2040LedMode(src.animIndex, &knownAnim);
		config.profiles_count = GP2040_SET_COUNT;
		for (uint8_t s = 0; s < GP2040_SET_COUNT; s++)
		{
			const Gp2040PinSet& set = src.sets[s < src.setCount ? s : 0];
			Profile& profile = config.profiles[s];
			profile.has_keyMapping = true;
			KeyMapping& km = profile.keyMapping;
			km.keycodes_count = MAX_KEYS;
			km.modifierMasks_count = MAX_KEYS;
			km.ledNormalColors_count = MAX_KEYS;
			km.ledPressedColors_count = MAX_KEYS;
			for (uint32_t pin = 0; pin < GP2040_PIN_COUNT; pin++)
			{
				km.keycodes[pin] = set.keycodes[pin];
				km.modifierMasks[pin] = set.modifiers[pin];
				if (src.hasCustomTheme)
				{
					int theme = gp2040ThemeIndex(set.actions[pin]);
					km.ledNormalColors[pin] = theme >= 0 && src.hasTheme[theme]
						? src.themeNormal[theme]
						: (src.hasStaticColors ? src.staticNormal : 0);
					km.ledPressedColors[pin] = theme >= 0 && src.hasTheme[theme]
						? src.themePressed[theme]
						: (src.hasStaticColors ? src.staticPressed : 0);
				}
			}
			if (knownAnim)
			{
				profile.has_ledMode = true;
				profile.ledMode = (uint32_t)ledMode;
			}
		}
		// The base gamepad mapping is global (shared by all profiles).
		config.has_gamepadMapping = true;
		config.gamepadMapping.masks_count = MAX_KEYS;
		for (uint32_t pin = 0; pin < GP2040_PIN_COUNT; pin++)
		{
			bool mapped = false;
			config.gamepadMapping.masks[pin] = gp2040GamepadMask(
				src.sets[0].actions[pin],
				src.sets[0].customButtonMasks[pin],
				src.sets[0].customDpadMasks[pin], &mapped);
		}
		if (src.hasProfileNumber)
		{
			uint32_t active = src.profileNumber > 0 ? src.profileNumber - 1 : 0;
			config.activeProfile = gpClampU(active, 3);
			config.has_activeProfile = true;
		}
	}
	// Global options (applied with or without pin data).
	if (src.hasInputMode)
		config.defaultInputMode = (InputMode)gp2040InputMode(src.inputMode, nullptr);
	if (src.hasDpadMode)
		config.dpadMode = (DpadMode)gpClampU(src.dpadMode, 2);
	if (src.hasSocdMode)
		config.socdMode = (SOCDMode)gpClampU(src.socdMode, 4);
	config.useNintendoLayout = src.useNintendoLayout;
	if (src.hasDebounce)
		config.debounceInterval = gpClampU(src.debounceDelay, 100);
	// LED globals. Wiring stays board-fixed; only user-tunable colors and the
	// per-mode brightness migrate (mirrors the web converter).
	if (src.hasBrightness)
	{
		config.ledOptions.brightnessByMode_count = 7;
		for (uint32_t i = 0; i < 7; i++)
			config.ledOptions.brightnessByMode[i] = src.brightnessMaximum;
	}
	if (src.hasStaticColors)
	{
		config.ledOptions.colorNormalByMode_count = 7;
		config.ledOptions.colorPressedByMode_count = 7;
		for (uint32_t i = 0; i < 7; i++)
		{
			config.ledOptions.colorNormalByMode[i] = src.staticNormal;
			config.ledOptions.colorPressedByMode[i] = src.staticPressed;
		}
	}
}
