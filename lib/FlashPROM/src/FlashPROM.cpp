/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: Copyright (c) 2021 Jason Skuby (mytechtoybox.com)
 */

#include "FlashPROM.h"

uint8_t FlashPROM::writeCache[EEPROM_CACHE_BYTES];
volatile static alarm_id_t flashWriteAlarm = 0;
volatile static spin_lock_t *flashLock = nullptr;

// Erase + rewrite the config-bearing tail of the EEPROM flash region from the
// RAM cache. The cache maps to the last EEPROM_CACHE_BYTES of the region, so
// only that tail is ever touched: the rest of the region stays erased and the
// write cost no longer scales with the region size.
static intptr_t eepromTailOffset() {
	return (intptr_t)EEPROM_FLASH_TAIL - (intptr_t)XIP_BASE - EEPROM_CACHE_BYTES;
}

int64_t writeToFlash(alarm_id_t id, void *flashCache)
{
	while (is_spin_locked(flashLock));

	multicore_lockout_start_blocking();
	uint32_t interrupts = spin_lock_blocking(flashLock);

	flash_range_erase(eepromTailOffset(), EEPROM_CACHE_BYTES);
	flash_range_program(eepromTailOffset(), reinterpret_cast<uint8_t *>(flashCache), EEPROM_CACHE_BYTES);

	flashWriteAlarm = 0;

	multicore_lockout_end_blocking();
	spin_unlock(flashLock, interrupts);

	return 0;
}

void FlashPROM::start()
{
	if (flashLock == nullptr)
		flashLock = spin_lock_instance(spin_lock_claim_unused(true));

	memcpy(writeCache, reinterpret_cast<uint8_t *>(EEPROM_FLASH_TAIL - EEPROM_CACHE_BYTES), EEPROM_CACHE_BYTES);
}

/* We don't have an actual EEPROM, so we need to be extra careful about minimizing writes. Instead
	of writing when a commit is requested, we update a time to actually commit. That way, if we receive multiple requests
	to commit in that timeframe, we'll hold off until the user is done sending changes. */
void FlashPROM::commit()
{
	while (is_spin_locked(flashLock));
	if (flashWriteAlarm != 0)
		cancel_alarm(flashWriteAlarm);
	flashWriteAlarm = add_alarm_in_ms(EEPROM_WRITE_WAIT, writeToFlash, writeCache, true);
}

// Synchronously write the cache to flash, cancelling any pending deferred
// write first. Used before a reboot so a save-then-reboot can't drop the write
// (the deferred alarm may never fire once the board resets). No-op when nothing
// is pending.
void FlashPROM::commitNow()
{
	if (flashWriteAlarm == 0)
		return;
	cancel_alarm(flashWriteAlarm);
	writeToFlash(0, writeCache);
}

void FlashPROM::reset()
{
	memset(writeCache, 0, EEPROM_CACHE_BYTES);
	commit();
}
