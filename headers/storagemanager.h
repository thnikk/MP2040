#ifndef STORAGE_H_
#define STORAGE_H_

#include <stdint.h>
#include "FlashPROM.h"
#include "hardware/gpio.h"

#include "enums.h"
#include "helper.h"
#include "types.h"
#include "keymask.h"

#include "config.pb.h"

#define SI Storage::getInstance()

// Live LED options pushed from the web config (core 0) to the running LED
// controller (core 1). Only user-tunable scalars; board properties are
// unaffected. Not persisted — the full config is written on Save.
struct LedPreview
{
    uint32_t ledMode;
    // Per-mode speed, indexed by LedMode (0-100 percent config value).
    uint32_t ledSpeed[6];
    uint32_t ledSpeedCount;
    // Per-mode brightness (0-255), indexed by LedMode.
    uint32_t brightnessByMode[6];
    uint32_t brightnessByModeCount;
    // Per-mode normal/pressed colors, indexed by LedMode.
    uint32_t colorNormalByMode[6];
    uint32_t colorPressedByMode[6];
    uint32_t colorCount;
    uint32_t ledTimeout;       // inactivity timeout in seconds (0 = always on)
    // Per-key colors for custom mode. A value of 0 (or no entry) uses Custom
    // mode's colorNormalByMode[0] / colorPressedByMode[0].
    uint32_t ledNormalColorCount;
    uint32_t ledNormalColors[MAX_KEYS];
    uint32_t ledPressedColorCount;
    uint32_t ledPressedColors[MAX_KEYS];
};

// Storage manager for board config, LED options, and thread-safe settings
class Storage {
public:
	Storage(Storage const&) = delete;
	void operator=(Storage const&)  = delete;
	static Storage& getInstance() // Thread-safe storage ensures cross-thread talk
	{
		static Storage instance;
		return instance;
	}

	Config& getConfig() { return config; }
	KeyMapping& getKeyMapping() { return config.keyMapping; }
	LEDOptions& getLedOptions() { return config.ledOptions; }
	int32_t getWebConfigPin() { return config.webConfigPin; }
	// Global MIDI output options (channel 0-15, velocity 1-127). Defaults are
	// 0 / 127 for configs without the field.
	uint32_t getMidiChannel() {
		return config.has_midiOptions ? config.midiOptions.channel : 0;
	}
	uint32_t getMidiVelocity() {
		return config.has_midiOptions ? config.midiOptions.velocity : 127;
	}
	void setMidiChannel(uint32_t channel) {
		config.midiOptions.channel = channel;
		config.has_midiOptions = true;
	}
	void setMidiVelocity(uint32_t velocity) {
		config.midiOptions.velocity = velocity;
		config.has_midiOptions = true;
	}
	// Input mode used at boot when no boot-mode pin is held. INPUT_MODE_CONFIG
	// is never a valid default (config mode is entered via the web config pin),
	// so it maps to keyboard; this also keeps old configs without the field on
	// keyboard mode.
	InputMode getDefaultInputMode() {
		if (!config.has_defaultInputMode || config.defaultInputMode == INPUT_MODE_CONFIG)
			return INPUT_MODE_KEYBOARD;
		return config.defaultInputMode;
	}
	void setDefaultInputMode(InputMode mode) {
		config.defaultInputMode = mode;
		config.has_defaultInputMode = true;
	}
	// USB serial (CDC) command interface. Off by default; the board must reboot
	// after toggling it because the USB descriptors are fixed at enumeration.
	bool getSerialConfigEnabled() {
		return config.has_serialConfigEnabled ? config.serialConfigEnabled : false;
	}
	void setSerialConfigEnabled(bool enabled) {
		config.serialConfigEnabled = enabled;
		config.has_serialConfigEnabled = true;
	}
	// Profile support: all four profiles live in config.profiles (index 0 =
	// base, 1-3 = alternates). The active profile is copied into the working
	// top-level fields (keyMapping / midiOptions / ledOptions) at boot, which
	// is what the drivers read. Switching takes effect on the next boot.
	uint32_t getActiveProfile() {
		return config.has_activeProfile ? config.activeProfile : 0;
	}
	void setActiveProfile(uint32_t profile) {
		config.activeProfile = profile;
		config.has_activeProfile = true;
	}
	Profile* getProfile(uint32_t index) {
		return (index < config.profiles_count) ? &config.profiles[index] : nullptr;
	}
	pb_size_t getProfileCount() { return config.profiles_count; }
	// Global macros (M1-M8). Per-key triggers reference these by 1-based
	// index via config.macroIndices.
	Macro* getMacros() { return config.macros; }
	pb_size_t getMacroCount() { return config.macros_count; }
	// Boot-mode shortcut pin (USB bootloader), from the board's PIN_BOOT define.
	// A physical board property (like the web config pin), never a user setting.
	int32_t getBootPin() { return bootPin; }
	// Capacitive touch pin mask, from the board's TOUCH_GPxx defines. A physical
	// board property (like the web config pin), never a user setting.
	GpioMask getTouchPinMask() { return touchPinMask; }
	// Matrix input mode (rows/cols > 0) is a physical board property from the
	// MATRIX_* defines. In matrix mode the key state mask bit N is the linear
	// matrix key (row N/COLS, col N%COLS) rather than a GPIO.
	bool isMatrixMode() { return matrixRows > 0 && matrixCols > 0; }
	uint8_t getMatrixRows() { return matrixRows; }
	uint8_t getMatrixCols() { return matrixCols; }
	const Pin_t* getMatrixRowPins() { return matrixRowPins; }
	const Pin_t* getMatrixColPins() { return matrixColPins; }
	// Scan polarity (board property). false = active-low: rows idle high and
	// driven low to scan, columns pulled up, a pressed column reads low
	// (diodes point toward the row pins). true = active-high: rows idle low and
	// driven high to scan, columns pulled down, a pressed column reads high
	// (diodes point toward the column pins).
	bool isMatrixActiveHigh() { return matrixActiveHigh; }

	// Total number of keys the board can report: rows*cols for matrix boards,
	// NUM_BANK0_GPIOS for direct boards. Bounds the debounce / driver / web
	// config loops (direct boards never exceed the GPIO count even though the
	// key-state mask can hold MAX_KEYS).
	uint32_t getKeyCount() {
		return isMatrixMode()
			? (uint32_t)matrixRows * matrixCols
			: (uint32_t)NUM_BANK0_GPIOS;
	}

	void init();
	bool save();
	bool save(const bool force);
	// Copy the active profile into the working top-level fields the drivers
	// read. Called at boot and after the active profile is edited via the web
	// config.
	void applyActiveProfile();
	void SetConfigMode(bool); 			// Config Mode (on-boot)
	bool GetConfigMode();

	void SetConfigButtonVisible(bool);	// Config button visibility (on-boot)
	bool GetConfigButtonVisible();

	// Core0 -> Core1 key state sharing. The mask is wider than a single word,
	// so writes use a seqlock (keyStateSeq) instead of relying on atomic 32-bit
	// stores. core0 readers share the writer's core and can read keyState
	// directly; core1 (LED controller) must go through getKeyState().
	void publishKeyState(const KeyMask&);
	KeyMask getKeyState();
	KeyMask keyState;

	// Core0 -> Core1 live LED preview (web config, not persisted). Publish from
	// the web config handler; the LED controller consumes on its update loop.
	void publishLedPreview(const LedPreview&);
	bool consumeLedPreview(LedPreview&);

	// Seed a LedPreview from the current working config (ledOptions + key
	// mapping) so a live update can publish the full theme state. Used by the
	// boot-window restore and the serial LED commands.
	void buildLedPreviewFromConfig(LedPreview& preview);

	void ResetSettings(); 				// EEPROM Reset Feature

private:
	Storage() : keyState(), keyStateSeq(0) {}
	bool CONFIG_MODE = false; 			// Config mode (boot)
	bool CONFIG_BUTTON_VISIBLE = false; // Config button visible (boot)
	Config config;
	int32_t bootPin = -1; 				// Boot-mode shortcut pin (board property)
	GpioMask touchPinMask = 0; 			// Capacitive touch pins (board property)
	// Matrix geometry (board property). rows*cols <= MAX_KEYS keys.
	uint8_t matrixRows = 0;
	uint8_t matrixCols = 0;
	Pin_t matrixRowPins[NUM_BANK0_GPIOS] = {};
	Pin_t matrixColPins[NUM_BANK0_GPIOS] = {};
	bool matrixActiveHigh = false; 	// scan polarity (board property)
	// Key-state seqlock: bumped by publishKeyState so core1 readers can detect
	// a torn multi-word mask. Even = stable, odd = write in progress.
	volatile uint32_t keyStateSeq = 0;
	volatile uint32_t ledPreviewGen = 0;
	LedPreview ledPreview;
	uint32_t lastConsumedLedPreviewGen = 0;
};

#endif
