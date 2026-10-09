//
// Host shim for the pico SDK bits the display driver touches, plus a settable
// virtual clock so animated screens (splash close-in, screen savers) can be
// captured deterministically.
//
#ifndef _SIM_CLOCK_H_
#define _SIM_CLOCK_H_
#include <stdint.h>
void simSetVirtualMillis(uint32_t ms);
void simUseRealClock();
#endif
