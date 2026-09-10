/*
 * SPDX-License-Identifier: MIT
 *
 * One-time, one-way migration from a GP2040-th config blob left in flash.
 *
 * GP2040-th and MP2040 share the same flash tail and ConfigFooter magic, so
 * flashing MP2040 over GP2040-th leaves the old protobuf blob readable. Its
 * schema differs entirely from MP2040's, so instead of pulling in GP2040's
 * nanopb descriptors this header implements a minimal protobuf wire-format
 * reader that extracts only the fields with an MP2040 equivalent (pin key
 * maps, gamepad options, LED colors).
 *
 * This file is pure C with no project dependencies so the extractor can be
 * unit-tested on the host. The MP2040 Config stuffing lives in
 * src/gp2040migrate.cpp (gp2040MigrateToConfig), mirroring the mapping in
 * www/js/core/gp2040.js.
 */

#ifndef GP2040MIGRATE_H_
#define GP2040MIGRATE_H_

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// GP2040-th GPIO-indexed pins (NUM_BANK0_GPIOS) and pin sets (base mapping +
// up to 3 alternates in ProfileOptions.gpioMappingsSets).
#define GP2040_PIN_COUNT 30
#define GP2040_SET_COUNT 4

// Custom-theme buttons in GP2040-th AnimationOptions field order:
// customThemeUp..customThemeA2 (fields 9-26), pressed variants (27-44).
#define GP2040_THEME_BUTTONS 18

typedef struct
{
	uint32_t keycodes[GP2040_PIN_COUNT];
	uint32_t modifiers[GP2040_PIN_COUNT];
	int32_t actions[GP2040_PIN_COUNT];
	uint32_t customButtonMasks[GP2040_PIN_COUNT];
	uint32_t customDpadMasks[GP2040_PIN_COUNT];
} Gp2040PinSet;

typedef struct
{
	Gp2040PinSet sets[GP2040_SET_COUNT];
	uint8_t setCount; // base + alternates found (0 = no pin data)
	// Config.gamepadOptions (presence-tracked; absent fields keep defaults).
	bool hasGamepad;
	bool hasInputMode, hasDpadMode, hasSocdMode, hasProfileNumber, hasDebounce;
	uint32_t inputMode, dpadMode, socdMode, profileNumber, debounceDelay;
	bool useNintendoLayout;
	// Config.ledOptions.
	bool hasBrightness;
	uint32_t brightnessMaximum;
	// Config.animationOptions.
	bool hasCustomTheme;
	bool hasStaticColors;
	uint32_t staticNormal, staticPressed;
	bool hasTheme[GP2040_THEME_BUTTONS];
	uint32_t themeNormal[GP2040_THEME_BUTTONS];
	uint32_t themePressed[GP2040_THEME_BUTTONS];
	bool hasAnim;
	uint32_t animIndex;
} Gp2040Config;

#ifdef __cplusplus
extern "C" {
#endif

// Top-level scan: true when the blob carries GP2040-th markers (Config field
// 12 boardConfig / field 13 gpioMappings, both length-delimited). MP2040
// encodes varints (touchMargin/touchRelease) at those field numbers, so a
// genuine MP2040 blob never matches.
bool gp2040LooksLike(const uint8_t* data, size_t len);

// Extract the migratable fields. Returns true when anything usable was found
// (pin sets, gamepad options, LED data). Never fails on truncated input:
// fields that don't fully decode are skipped.
bool gp2040Extract(const uint8_t* data, size_t len, Gp2040Config* out);

// GP2040-th GpioAction (+ custom masks) to an MP2040 GAMEPAD_PIN_MASK_* bit
// mask. *mapped is false for actions with no MP2040 equivalent (menu nav,
// sustain, turbo, macros, ...); NONE/RESERVED/ASSIGNED_TO_ADDON are neutral
// (mask 0, *mapped true).
uint32_t gp2040GamepadMask(int32_t action, uint32_t customButtons, uint32_t customDpad, bool* mapped);

// GP2040-th InputMode to MP2040 InputMode. Modes MP2040 doesn't implement
// fall back to keyboard with *fallback set (mirrors GP2040_INPUT_MODES).
int gp2040InputMode(uint32_t gpInput, bool* fallback);

// GP2040-th baseAnimationIndex to the closest MP2040 ledMode. *known is false
// for unmapped indices (caller keeps the board default).
int gp2040LedMode(uint32_t gpAnim, bool* known);

// GP2040-th GpioAction to a custom-theme button index (0-17), or -1.
int gp2040ThemeIndex(int32_t action);

#ifdef __cplusplus
}

// Map an extracted GP2040-th config into MP2040's Config (which must already
// hold applyDefaults output: only pins 0-29 and globals are overwritten, the
// rest keeps board defaults). Mirrors www/js/core/gp2040.js. Requires
// config.pb.h (Config) to be included before this header.
void gp2040MigrateToConfig(const Gp2040Config& src, Config& config);
#endif

#endif
