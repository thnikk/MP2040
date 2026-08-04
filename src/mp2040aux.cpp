#include "mp2040aux.h"
#include "storagemanager.h"

MP2040Aux::MP2040Aux() : isReady(false) {
}

MP2040Aux::~MP2040Aux() {
}

// MP2040Aux will always come after MP2040 setup(), so we can rely on the
// MP2040 setup function for certain setup functions.
void MP2040Aux::setup() {
	ledController.setup();

	// Ready to sync Core0 and Core1
	isReady = true;
}

void MP2040Aux::run() {
	while (1) {
		ledController.update();
	}
}
