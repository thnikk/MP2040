#ifndef STORAGE_H_
#define STORAGE_H_

#include <stdint.h>
#include "FlashPROM.h"

#include "enums.h"
#include "helper.h"
#include "types.h"

#include "config.pb.h"

#define SI Storage::getInstance()

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

	// LED test mode (set from core0 / web config, consumed by core1 LED loop)
	void setLedTest(uint32_t color, uint32_t durationMs);
	bool getLedTest(uint32_t& color);

	void ResetSettings(); 				// EEPROM Reset Feature

private:
	Storage() : keyState(0), ledTestColor(0), ledTestUntil(0) {}
	bool CONFIG_MODE = false; 			// Config mode (boot)
	bool CONFIG_BUTTON_VISIBLE = false; // Config button visible (boot)
	volatile uint32_t ledTestColor;
	volatile uint64_t ledTestUntil;
	Config config;
};

#endif
