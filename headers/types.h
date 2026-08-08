/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: Copyright (c) 2023 Brian S. Stephan <bss@incorporeal.org>
 */

#ifndef TYPES_H_
#define TYPES_H_

#include <stdint.h>

// common types
#define	Pin_t		int32_t		// signed to accommodate for -1
// GPIO-indexed bitmask (pins 0-29). Distinct from the key-state KeyMask:
// GPIO masks are bounded by the physical pin count, key-state masks can cover
// up to MAX_KEYS linear matrix indices.
#define GpioMask	uint32_t

#endif
