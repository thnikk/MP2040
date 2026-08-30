#include "pico/multicore.h"

#include "mp2040.h"
#include "mp2040aux.h"

#include <cstdlib>

// Custom implementation of __gnu_cxx::__verbose_terminate_handler() to reduce binary size
namespace __gnu_cxx {
void __verbose_terminate_handler()
{
	abort();
}
}

static MP2040 * mp2040Core0 = nullptr;
static MP2040Aux * mp2040Core1 = nullptr;

// Launch our second core with additional modules loaded in
void core1() {
	multicore_lockout_victim_init(); // block core 1

	// Create MP2040 w/ Additional Modules for Core 1
	mp2040Core1->setup();
	mp2040Core1->run();
}

int main() {
	// Create MP2040 Main Core (core0), Core1 is dependent on Core0
	mp2040Core0 = new MP2040();
	mp2040Core1 = new MP2040Aux();

	// Create MP2040 Main Core - Setup Core0
	mp2040Core0->setup();

	// Create MP2040 Thread for Core1
	multicore_launch_core1(core1);

	// Core 0 is also a lockout victim so core 1 can safely write flash (e.g.
	// flushing a config save before rebooting) and lockouts acquire immediately
	// instead of spinning out the 1s timeout.
	multicore_lockout_victim_init();

	// Sync Core0 and Core1
	while(mp2040Core1->ready() == false ) {
		__asm volatile ("nop\n");
	}
	mp2040Core0->run();

	return 0;
}
