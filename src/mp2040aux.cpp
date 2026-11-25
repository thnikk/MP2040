#include "mp2040aux.h"
#include "storagemanager.h"

MP2040Aux::MP2040Aux() : isReady(false) {
}

MP2040Aux::~MP2040Aux() {
}

// MP2040Aux will always come after MP2040 setup(), so we can rely on the
// MP2040 setup function for certain setup functions.
void MP2040Aux::setup() {
	// The color keypad must be initialized before the LED controller: its
	// configure() step checks for the screen output and hands it the per-LED
	// colors instead of allocating a WS2812 strip.
	KeypadDisplay::getInstance().setup();
	ledController.setup();
	displayController.setup();

	// Ready to sync Core0 and Core1
	isReady = true;
}

void MP2040Aux::run() {
	while (1) {
		ledController.update();
		displayController.update();
	}
}
