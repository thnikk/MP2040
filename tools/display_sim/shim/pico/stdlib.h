//
// Host shim for "pico/stdlib.h" (MP2040 display simulator).
//
// The display subsystem only pulls a handful of things from the pico SDK:
// common libc and NUM_BANK0_GPIOS (used by helper.h's pin bounds check). We
// provide those here instead of linking the SDK, so the real rendering code
// can be compiled unchanged on the host.
//
#ifndef _SIM_PICO_STDLIB_H_
#define _SIM_PICO_STDLIB_H_

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

// RP2040 bank-0 GPIO count. Kept in sync with the pico SDK.
#ifndef NUM_BANK0_GPIOS
#define NUM_BANK0_GPIOS 30
#endif

#endif
