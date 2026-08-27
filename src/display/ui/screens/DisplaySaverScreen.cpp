#include "DisplaySaverScreen.h"

#include "pico/stdlib.h"
#include "storagemanager.h"

void DisplaySaverScreen::init() {
	const DisplayOptions& options = Storage::getInstance().getDisplayOptions();
	displaySaverMode = options.displaySaverMode;
	enteredSaverTime = getMillis();
	prevKeyState = Storage::getInstance().getKeyState();

	getRenderer()->clearScreen();

	switch (displaySaverMode) {
		case DisplaySaverMode::DISPLAY_SAVER_SNOW:
			initSnowScene();
			break;
		case DisplaySaverMode::DISPLAY_SAVER_TOAST:
			initToasters();
			break;
		case DisplaySaverMode::DISPLAY_SAVER_STARS:
			initStarsScene();
			break;
		default:
			break;
	}
}

void DisplaySaverScreen::shutdown() {
	clearElements();
}

void DisplaySaverScreen::drawScreen() {
	switch (displaySaverMode) {
		case DisplaySaverMode::DISPLAY_SAVER_SNOW:
			drawSnowScene();
			break;
		case DisplaySaverMode::DISPLAY_SAVER_BOUNCE:
			drawBounceScene();
			break;
		case DisplaySaverMode::DISPLAY_SAVER_PIPES:
			drawPipeScene();
			break;
		case DisplaySaverMode::DISPLAY_SAVER_TOAST:
			drawToasterScene();
			break;
		case DisplaySaverMode::DISPLAY_SAVER_STARS:
			drawStarsScene();
			break;
		default:
			break;
	}
}

int8_t DisplaySaverScreen::update() {
	// Any key press (the debounced key state differs from when the saver
	// started, which is always "nothing held") returns to the buttons screen.
	if (Storage::getInstance().getKeyState() != prevKeyState)
		return 1;

	return -1;
}

void DisplaySaverScreen::initSnowScene() {
	for (uint8_t i = 0; i < NUM_FLAKES; ++i) {
		flakeX[i] = rand() % SCREEN_WIDTH;
		flakeY[i] = rand() % SCREEN_HEIGHT;
		flakeSpeed[i] = (rand() % 3) + 1;
		flakeDrift[i] = (rand() % 3) - 1;
	}
}

void DisplaySaverScreen::drawSnowScene() {
	getRenderer()->clearScreen();
	for (uint8_t i = 0; i < NUM_FLAKES; ++i) {
		int16_t newX = flakeX[i] + flakeDrift[i];
		if (newX < 0) newX = SCREEN_WIDTH - 1;
		if (newX >= SCREEN_WIDTH) newX = 0;
		int16_t newY = flakeY[i] + flakeSpeed[i];
		if (newY >= SCREEN_HEIGHT) newY = 0;
		flakeX[i] = newX;
		flakeY[i] = newY;
		getRenderer()->drawPixel(newX, newY, 1);
	}
}

void DisplaySaverScreen::drawBounceScene() {
	uint16_t scaledWidth = static_cast<uint16_t>(bounceSpriteWidth * bounceScale);
	uint16_t scaledHeight = static_cast<uint16_t>(bounceSpriteHeight * bounceScale);

	bounceSpriteX += bounceSpriteVelocityX;
	bounceSpriteY += bounceSpriteVelocityY;

	if (bounceSpriteX <= 0 || bounceSpriteX + scaledWidth >= SCREEN_WIDTH) bounceSpriteVelocityX = -bounceSpriteVelocityX;

	if (bounceSpriteY <= 0 || bounceSpriteY + scaledHeight >= SCREEN_HEIGHT) bounceSpriteVelocityY = -bounceSpriteVelocityY;

	getRenderer()->drawSprite((uint8_t *)bootLogoBottom, bounceSpriteWidth, bounceSpriteHeight, 0, bounceSpriteX, bounceSpriteY, 0, bounceScale);
}

void DisplaySaverScreen::drawPipeScene() {
	const uint8_t PIPE_WIDTH = 4;
	const uint8_t PIPE_COLOR = 1;

	uint8_t currentX = 0;
	uint8_t currentY = 0;

	while (currentY < SCREEN_HEIGHT) {
		bool connectRight = rand() % 2;
		bool connectDown = rand() % 2;

		if (connectRight && currentX + PIPE_WIDTH < SCREEN_WIDTH) {
			for (uint8_t i = 0; i < PIPE_WIDTH; ++i) {
				getRenderer()->drawPixel(currentX + i, currentY, PIPE_COLOR);
			}
		}

		if (connectDown && currentY + PIPE_WIDTH < SCREEN_HEIGHT) {
			for (uint8_t i = 0; i < PIPE_WIDTH; ++i) {
				getRenderer()->drawPixel(currentX, currentY + i, PIPE_COLOR);
			}
		}

		getRenderer()->drawPixel(currentX, currentY, PIPE_COLOR);

		currentX += PIPE_WIDTH;
		if (currentX >= SCREEN_WIDTH) {
			currentX = 0;
			currentY += PIPE_WIDTH;
		}
	}
}

void DisplaySaverScreen::initToasters() {
	toasters.clear();
	for (uint16_t i = 0; i < numberOfToasters; ++i) {
		double scale = (static_cast<double>(rand()) / RAND_MAX);
		int16_t dx = (-1 - rand() % 3);
		int16_t dy = (1 + rand() % 3);

		toasters.push_back({
			(uint8_t *)bootLogoTop,
			toasterSpriteWidth,
			toasterSpriteHeight,
			scale,
			static_cast<int16_t>(SCREEN_WIDTH - toasterSpriteWidth * scale),
			static_cast<int16_t>(rand() % (SCREEN_HEIGHT - static_cast<int16_t>(toasterSpriteHeight * scale))),
			static_cast<int16_t>(dx),
			static_cast<int16_t>(dy)
		});
	}
}

void DisplaySaverScreen::drawToasterScene() {
	getRenderer()->clearScreen();
	for (uint16_t i = 0; i < toasters.size(); ++i) {
		ToastParams& sprite = toasters[i];

		getRenderer()->drawSprite(sprite.image, sprite.width, sprite.height, 0, sprite.x, sprite.y, 0, sprite.scale);

		sprite.x += sprite.dx;
		sprite.y += sprite.dy;

		if (sprite.x + sprite.width * sprite.scale < 0) {
			sprite.x = SCREEN_WIDTH;
			sprite.y = rand() % (SCREEN_HEIGHT - static_cast<int16_t>(sprite.height * sprite.scale));
		}

		if (sprite.y > SCREEN_HEIGHT) {
			sprite.y = 0;
		}
	}
}

void DisplaySaverScreen::initStarsScene() {
	starsEnteredTime = getMillis();
	occasionalStarX = 3 + (rand() % (SCREEN_WIDTH - 6));
	occasionalStarY = 3 + (rand() % (SCREEN_HEIGHT - 6));
	nextStarTime = 0;

	const int16_t MOON_CX = 64, MOON_CY = 32, MOON_R2 = 30 * 30;
	const int16_t STAR_MIN_DIST2 = 12 * 12;
	const int16_t MARGIN = 5;
	const int16_t X_MAX = SCREEN_WIDTH - 1 - MARGIN;
	const int16_t Y_MAX = SCREEN_HEIGHT - 1 - MARGIN;

	for (uint8_t i = 0; i < NUM_STARS; ++i) {
		bool valid = false;
		for (uint8_t attempt = 0; attempt < 50; ++attempt) {
			int16_t x = MARGIN + (rand() % (X_MAX - MARGIN + 1));
			int16_t y = MARGIN + (rand() % (Y_MAX - MARGIN + 1));
			int16_t dx = x - MOON_CX, dy = y - MOON_CY;
			if (dx * dx + dy * dy < MOON_R2) continue;
			bool tooClose = false;
			for (uint8_t j = 0; j < i; ++j) {
				int16_t dxs = x - stars[j][0], dys = y - stars[j][1];
				if (dxs * dxs + dys * dys < STAR_MIN_DIST2) {
					tooClose = true;
					break;
				}
			}
			if (!tooClose) { stars[i][0] = x; stars[i][1] = y; starSizes[i] = 2 + (rand() % 3); valid = true; break; }
		}
		if (!valid) { stars[i][0] = MARGIN + (rand() % (X_MAX - MARGIN + 1)); stars[i][1] = MARGIN + (rand() % (Y_MAX - MARGIN + 1)); starSizes[i] = 2 + (rand() % 3); }
	}
}

void DisplaySaverScreen::drawStarsScene() {
	uint32_t elapsed = getMillis() - starsEnteredTime;

	if (elapsed <= 2000) {
		for (uint8_t i = 0; i < NUM_STARS; ++i) {
			uint8_t starSize = rand() % (starSizes[i] + 1);
			int16_t cx = stars[i][0];
			int16_t cy = stars[i][1];
			getRenderer()->drawLine(cx - starSize, cy, cx + starSize, cy, 1, 0);
			getRenderer()->drawLine(cx, cy - starSize, cx, cy + starSize, 1, 0);
		}

		getRenderer()->drawEllipse(64, 32, 28, 28, 1, 1);
		getRenderer()->drawEllipse(70, 26, 25, 25, 0, 1);
	} else {
		const uint32_t GROW_MS = 200;
		const uint32_t SHRINK_MS = 200;
		const uint32_t ANIM_MS = GROW_MS + SHRINK_MS;

		uint32_t now = getMillis();
		if (nextStarTime == 0)
			nextStarTime = now + 2000 + (rand() % 3000);
		if (now >= nextStarTime) {
			uint32_t animPos = now - nextStarTime;
			if (animPos < ANIM_MS) {
				uint8_t starSize;
				if (animPos < GROW_MS)
					starSize = (animPos * 3) / GROW_MS;
				else
					starSize = 3 - ((animPos - GROW_MS) * 3) / SHRINK_MS;
				getRenderer()->drawLine(occasionalStarX - starSize, occasionalStarY, occasionalStarX + starSize, occasionalStarY, 1, 0);
				getRenderer()->drawLine(occasionalStarX, occasionalStarY - starSize, occasionalStarX, occasionalStarY + starSize, 1, 0);
			} else {
				occasionalStarX = 3 + (rand() % (SCREEN_WIDTH - 6));
				occasionalStarY = 3 + (rand() % (SCREEN_HEIGHT - 6));
				nextStarTime = now + 2000 + (rand() % 3000);
			}
		}
	}
}