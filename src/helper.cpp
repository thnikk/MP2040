#include "helper.h"

#include "pico/stdlib.h"
#include "hardware/timer.h"

uint32_t getMillis() {
	return to_ms_since_boot(get_absolute_time());
}

uint64_t getMicro() {
	return to_us_since_boot(get_absolute_time());
}
