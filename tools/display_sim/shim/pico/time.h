//
// Host shim for "pico/time.h" (MP2040 display simulator).
//
// helper.h includes this and declares getMillis()/getMicro(); the host
// implementations live in sim_stubs.cpp.
//
#ifndef _SIM_PICO_TIME_H_
#define _SIM_PICO_TIME_H_

#include <stdint.h>

// helper.h uses NUM_BANK0_GPIOS but only includes pico/time.h; the real SDK
// gets it via pico/platform.h. Provide it here too.
#ifndef NUM_BANK0_GPIOS
#define NUM_BANK0_GPIOS 30
#endif

#endif
