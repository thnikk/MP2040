#include "SplashScreen.h"

#include "pico/stdlib.h"
#include "storagemanager.h"

void SplashScreen::init() {
	getRenderer()->clearScreen();
	splashStartTime = getMillis();
}

void SplashScreen::shutdown() {
	clearElements();
}

void SplashScreen::drawScreen() {
	SplashMode splashMode = getDisplayOptions().splashMode;
	int splashSpeed = 40;

	if (splashMode == SPLASH_MODE_NONE) {
		getRenderer()->drawText(0, 4, " Splash NOT enabled.");
	} else if (splashMode == SPLASH_MODE_STATIC) {
		// Default static splash (GP2040-th's DEFAULT_SPLASH, 128x64).
		getRenderer()->drawSprite((uint8_t*)defaultSplash, 128, 64, 16, 0, 0, 1);
	} else if (splashMode == SPLASH_MODE_CLOSEIN) {
		// Close-in. Animate the GP2040 logo
		int timeMS = getMillis();
		getRenderer()->drawSprite((uint8_t *)bootLogoTop, 43, 39, 6, 43, std::min<int>((timeMS / splashSpeed) - 39, 0), 1);
		getRenderer()->drawSprite((uint8_t *)bootLogoBottom, 128, 35, 10, 0, std::max<int>(64 - (timeMS / (splashSpeed * 2)), 30), 1);
	}
}

int8_t SplashScreen::update() {
	uint32_t elapsedDuration = getMillis() - splashStartTime;
	uint32_t splashDuration = getDisplayOptions().splashDuration; // seconds
	if (splashDuration != 0 && (elapsedDuration >= splashDuration * 1000)) {
		return 1;
	}
	return -1; // -1 means no change in screen state
}