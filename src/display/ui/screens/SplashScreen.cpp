#include "SplashScreen.h"

#include <cctype>
#include <string>
#include "pico/stdlib.h"
#include "storagemanager.h"
#include "helper.h"
#include "GP_Font_3x5.h"

static void drawSplashLabel(GPGFX* renderer, const std::string& text, uint8_t topY) {
	std::string upper;
	for (char c : text) {
		upper += (char)toupper((unsigned char)c);
	}
	uint8_t len = upper.length() > 32 ? 32 : upper.length();
	uint16_t pixelWidth = len * 4;
	uint16_t x = (128 - pixelWidth) / 2;
	renderer->drawRectangle(x, topY, x + pixelWidth, topY + GP_FONT_3x5_HEIGHT, 0, true);
	for (uint8_t i = 0; i < len; i++) {
		uint8_t glyphIndex = (uint8_t)upper[i] - 32;
		if (glyphIndex >= GP_FONT_3x5_COUNT) glyphIndex = '?' - 32;
		const uint8_t* glyph = &GP_Font_3x5[glyphIndex * 3];
		for (uint8_t col = 0; col < GP_FONT_3x5_WIDTH; col++) {
			for (uint8_t row = 0; row < GP_FONT_3x5_HEIGHT; row++) {
				if (glyph[col] & (1 << row)) {
					renderer->drawPixel(x + i * 4 + col, topY + row, 1);
				}
			}
		}
	}
}

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
		drawSplashLabel(getRenderer(), BOARD_CONFIG_LABEL, 3);
		drawSplashLabel(getRenderer(), "MP2040", 56);
	} else if (splashMode == SPLASH_MODE_CLOSEIN) {
		// Close-in. Animate the GP2040 logo
		int timeMS = getMillis();
		getRenderer()->drawSprite((uint8_t *)bootLogoTop, 43, 39, 6, 43, std::min<int>((timeMS / splashSpeed) - 39, 0), 1);
		getRenderer()->drawSprite((uint8_t *)bootLogoBottom, 128, 35, 10, 0, std::max<int>(64 - (timeMS / (splashSpeed * 2)), 30), 1);
		drawSplashLabel(getRenderer(), BOARD_CONFIG_LABEL, 3);
		drawSplashLabel(getRenderer(), "MP2040", 56);
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