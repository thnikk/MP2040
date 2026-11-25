/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: Copyright (c) 2026 thnikk
 */

// Minimal pico-sdk board header for the Waveshare RP2350-Touch-LCD-3.5.
//
// The board uses the RP2350B variant (48 GPIOs) with 16MB of QSPI flash. It has
// no user LED and its GPIOs are wired to the LCD/touch/audio/IMU peripherals, so
// none of the SDK's default peripheral pins are defined here.

#ifndef _BOARDS_SCREENPAD_RP2350_H
#define _BOARDS_SCREENPAD_RP2350_H

// This header may be included from assembler, so keep it preprocessor-only.
pico_board_cmake_set(PICO_PLATFORM, rp2350-arm-s)

// For board detection.
#define SCREENPAD_RP2350

// --- RP2350 VARIANT ---
// The RP2350B has 48 GPIOs (GPIO 30-47 and ADC on GPIO 40-47).
#define PICO_RP2350A 0

// --- FLASH ---
#define PICO_BOOT_STAGE2_CHOOSE_W25Q080 1
#ifndef PICO_FLASH_SPI_CLKDIV
#define PICO_FLASH_SPI_CLKDIV 2
#endif

pico_board_cmake_set_default(PICO_FLASH_SIZE_BYTES, (16 * 1024 * 1024))
#ifndef PICO_FLASH_SIZE_BYTES
#define PICO_FLASH_SIZE_BYTES (16 * 1024 * 1024)
#endif

pico_board_cmake_set_default(PICO_RP2350_A2_SUPPORTED, 1)
#ifndef PICO_RP2350_A2_SUPPORTED
#define PICO_RP2350_A2_SUPPORTED 1
#endif

#endif
