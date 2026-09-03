/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: Copyright (c) 2021 Jason Skuby (mytechtoybox.com)
 */

#ifndef FLASHPROM_H_
#define FLASHPROM_H_

#include <stdint.h>
#include <string.h>
#include <pico/lock_core.h>
#include <pico/multicore.h>
#include <hardware/flash.h>
#include <hardware/timer.h>

#define EEPROM_SIZE_BYTES    0x10000          // Reserve 64k of flash memory (must be 4k-sector aligned)
// The on-RAM write cache holds only the config-bearing tail of the flash
// region, so the region size is decoupled from RAM usage. Max encoded config
// (Config_size) + footer must fit here (checked by static_assert in Storage).
#define EEPROM_CACHE_BYTES   0x9000           // 36k RAM cache (must be divisible by 256)
#define EEPROM_ADDRESS_START _u(0x101F0000) // The flash region starts here; config lives at its tail
// End of the reserved flash region (also the end of the write cache mapping).
#define EEPROM_FLASH_TAIL    (EEPROM_ADDRESS_START + EEPROM_SIZE_BYTES)

// Warning: If the write wait is too long it can stall other processes
#define EEPROM_WRITE_WAIT    50             // Amount of time in ms to wait before blocking core1 and committing to flash

class FlashPROM
{
	public:
		void start();
		void commit();
		void commitNow();
		void reset();

		static uint8_t writeCache[EEPROM_CACHE_BYTES];
};

inline FlashPROM EEPROM;

#endif
