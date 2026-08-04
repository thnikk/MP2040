#ifndef STORAGE_H_
#define STORAGE_H_

#include <stdint.h>
#include "FlashPROM.h"

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
    uint32_t ledSpeed;         // raw 1-255 config value
    uint32_t brightnessMaximum;
    uint32_t colorNormal;
    uint32_t colorPressed;
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

	void init();
	bool save();
	bool save(const bool force);

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
	volatile uint32_t ledPreviewGen = 0;
	LedPreview ledPreview;
	uint32_t lastConsumedLedPreviewGen = 0;
};

#endif
