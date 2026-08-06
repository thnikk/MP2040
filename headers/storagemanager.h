#ifndef STORAGE_H_
#define STORAGE_H_

#include <stdint.h>
#include "FlashPROM.h"
#include "hardware/gpio.h"

#include "enums.h"
#include "helper.h"
#include "types.h"

#include "config.pb.h"

#define SI Storage::getInstance()

// Live LED options pushed from the web config (core 0) to the running LED
// controller (core 1). Only user-tunable scalars; board properties are
// unaffected. Not persisted — the full config is written on Save.
struct LedPreview
{
    uint32_t ledMode;
    uint32_t ledSpeed;         // raw 0-100 percent config value
    uint32_t brightnessMaximum;
    uint32_t colorNormal;
    uint32_t colorPressed;
    uint32_t ledTimeout;       // inactivity timeout in seconds (0 = always on)
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
	// Boot-mode shortcut pin (USB bootloader), from the board's PIN_BOOT define.
	// A physical board property (like the web config pin), never a user setting.
	int32_t getBootPin() { return bootPin; }
	// Capacitive touch pin mask, from the board's TOUCH_GPxx defines. A physical
	// board property (like the web config pin), never a user setting.
	Mask_t getTouchPinMask() { return touchPinMask; }
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

	// Core0 -> Core1 key state sharing. Volatile 32-bit mask is atomic on RP2040.
	void publishKeyState(Mask_t);
	volatile Mask_t keyState;

	// Core0 -> Core1 live LED preview (web config, not persisted). Publish from
	// the web config handler; the LED controller consumes on its update loop.
	void publishLedPreview(const LedPreview&);
	bool consumeLedPreview(LedPreview&);

	void ResetSettings(); 				// EEPROM Reset Feature

private:
	Storage() : keyState(0) {}
	bool CONFIG_MODE = false; 			// Config mode (boot)
	bool CONFIG_BUTTON_VISIBLE = false; // Config button visible (boot)
	Config config;
	int32_t bootPin = -1; 				// Boot-mode shortcut pin (board property)
	Mask_t touchPinMask = 0; 			// Capacitive touch pins (board property)
	// Matrix geometry (board property). rows*cols <= 30 keys.
	uint8_t matrixRows = 0;
	uint8_t matrixCols = 0;
	Pin_t matrixRowPins[NUM_BANK0_GPIOS] = {};
	Pin_t matrixColPins[NUM_BANK0_GPIOS] = {};
	bool matrixActiveHigh = false; 	// scan polarity (board property)
	volatile uint32_t ledPreviewGen = 0;
	LedPreview ledPreview;
	uint32_t lastConsumedLedPreviewGen = 0;
};

#endif
