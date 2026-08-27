#ifndef _DISPLAYSAVERSCREEN_H_
#define _DISPLAYSAVERSCREEN_H_

#include <vector>
#include "GPGFX_UI_widgets.h"
#include "bitmaps.h"
#include "keymask.h"

const uint8_t SCREEN_WIDTH = 128;
const uint8_t SCREEN_HEIGHT = 64;

class DisplaySaverScreen : public GPScreen {
	public:
		DisplaySaverScreen() {}
		DisplaySaverScreen(GPGFX* renderer) { setRenderer(renderer); }
		virtual int8_t update();
		virtual void init();
		virtual void shutdown();
	protected:
		virtual void drawScreen();
		KeyMask prevKeyState;
		uint32_t enteredSaverTime = 0;
		DisplaySaverMode displaySaverMode;

		// snow screen (compact flake list instead of a full-screen grid)
		static const uint8_t NUM_FLAKES = 48;
		int16_t flakeX[NUM_FLAKES];
		int16_t flakeY[NUM_FLAKES];
		uint8_t flakeSpeed[NUM_FLAKES];
		int8_t flakeDrift[NUM_FLAKES];
		void initSnowScene();
		void drawSnowScene();

		// bounce
		uint16_t bounceSpriteX = 0;
		uint16_t bounceSpriteY = 0;
		uint16_t bounceSpriteWidth = 128;
		uint16_t bounceSpriteHeight = 35;
		double bounceSpriteVelocityX = 1;
		double bounceSpriteVelocityY = 1;
		double bounceScale = 0.5;
		void drawBounceScene();

		// pipes
		void drawPipeScene();

		// stars
		static const uint8_t NUM_STARS = 8;
		uint8_t stars[NUM_STARS][2] = {};
		uint8_t starSizes[NUM_STARS] = {};
		uint32_t starsEnteredTime = 0;
		int16_t occasionalStarX = 0;
		int16_t occasionalStarY = 0;
		uint32_t nextStarTime = 0;
		void initStarsScene();
		void drawStarsScene();

		// toaster
		struct ToastParams {
			uint8_t* image;
			uint16_t width;
			uint16_t height;
			double scale;
			int16_t x;
			int16_t y;
			int16_t dx;
			int16_t dy;
		};

		std::vector<ToastParams> toasters;
		uint16_t numberOfToasters = 10;
		uint16_t toasterSpriteWidth = 43;
		uint16_t toasterSpriteHeight = 39;
		void initToasters();
		void drawToasterScene();
};

#endif