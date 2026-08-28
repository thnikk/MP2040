#include "DisplayController.h"

#include <cstring>
#include "storagemanager.h"
#include "hardware/i2c.h"
#include "pico/stdlib.h"
#include "gamepadmapping.h"
#include "drivers/shared/gamepadhelper.h"

static bool gamepadControlHeld(uint32_t stateMask) {
	Storage& s = Storage::getInstance();
	const KeyMask keyState = s.getKeyState();
	const uint32_t stateButtons = stateMask & ~(uint32_t)GAMEPAD_MASK_DPAD;
	const uint32_t pinMask = (stateMask & GAMEPAD_MASK_DPAD) | (stateButtons << 4);
	for (uint32_t pin = 0; pin < MAX_KEYS; pin++) {
		if (keyState.test(pin) && (s.getGamepadMask(pin) & pinMask) != 0)
			return true;
	}
	return false;
}

DisplayController::DisplayController() {
}

DisplayController::~DisplayController() {
	destroyScreen();
}

void DisplayController::setup() {
	DisplayOptions& options = opts();
	if (!options.enabled) return;

	if (options.sdaPin < 0 || options.sclPin < 0) return;

	i2c_inst_t* block = (options.i2cBlock == 1) ? i2c1 : i2c0;

	i2c_init(block, 800000);
	gpio_set_function(options.sdaPin, GPIO_FUNC_I2C);
	gpio_set_function(options.sclPin, GPIO_FUNC_I2C);
	gpio_pull_up(options.sdaPin);
	gpio_pull_up(options.sclPin);

	// Scan for an SSD1306/SH1106 at 0x3C / 0x3D.
	uint8_t address = 0;
	uint8_t probe = 0;
	for (int a = 0x3C; a <= 0x3D; a++) {
		if (i2c_read_blocking(block, (uint8_t)a, &probe, 1, false) >= 0) {
			address = (uint8_t)a;
			break;
		}
	}
	if (address == 0) return;

	GPGFX_DisplayTypeOptions gfxOptions;
	gfxOptions.displayType = DISPLAY_TYPE_SSD1306;
	gfxOptions.i2c = block;
	gfxOptions.size = options.size;
	gfxOptions.address = address;
	gfxOptions.orientation = options.flip;
	gfxOptions.inverted = options.invert;
	gfxOptions.font = {6, 8, GP_Font_Standard};

	gfx.init(gfxOptions);
	displayPresent = true;
	powered = true;
	lastActivity = getMillis();
	lastSaverCheck = getMillis();
	setMode(SPLASH);
}

void DisplayController::update() {
	if (!displayPresent) return;

	const uint32_t now = getMillis();
	KeyMask keyState = Storage::getInstance().getKeyState();

	// Track activity on any key state change.
	if (keyState != prevKeyState) {
		lastActivity = now;
		prevKeyState = keyState;
	}

	// Combo toggles the mini menu; nav drives it while open.
	processCombo();

	if (mode == MAIN_MENU || mode == REMAP) {
		processNav();
	} else {
		memset(navPrev, 0, sizeof(navPrev));
	}

	// Inactivity timeout -> display saver. displaySaverTimeout is in seconds.
	if (mode == BUTTONS) {
		const uint32_t timeout = opts().displaySaverTimeout;
		if (timeout > 0 && (now - lastActivity) > (uint32_t)timeout * 1000) {
			setMode(SAVER);
		}
	}

	// Update the current screen (handles splash -> buttons, saver exit, etc.)
	if (screen != nullptr) {
		int8_t result = screen->update();
		if (result >= 0 && result != (int8_t)mode) {
			setMode((DisplayMode)result);
		}
	}

	// Draw, unless the saver turned the panel off.
	if (screen != nullptr && powered) {
		screen->draw();
	}

	Storage::getInstance().SetMenuActive(mode == MAIN_MENU || mode == REMAP);
}

void DisplayController::processCombo() {
	const uint32_t now = getMillis();
	const bool held = menuComboHeld();

	if (held && !comboArmed) {
		comboArmed = true;
		comboHeldSince = now;
	} else if (!held) {
		comboArmed = false;
	}

	if (held && comboArmed && (now - comboHeldSince) >= 500) {
		comboArmed = false; // one toggle per press
		if (mode == MAIN_MENU || mode == REMAP) {
			setMode(BUTTONS);
		} else if (mode == BUTTONS || mode == SAVER) {
			setMode(MAIN_MENU);
			// Seed the nav edge tracker with the current key states so the
			// combo keys (often B1 + others) don't immediately trigger a
			// select/back on the freshly opened menu.
			for (uint8_t a = 0; a < 6; a++)
				navPrev[a] = navHeld(a);
		}
	}
}

bool DisplayController::menuComboHeld() {
	const DisplayOptions& options = opts();
	if (options.menuCombo_count == 0) return false;
	KeyMask keyState = Storage::getInstance().getKeyState();
	for (uint32_t i = 0; i < options.menuCombo_count; i++) {
		if (options.menuCombo[i] >= MAX_KEYS || !keyState.test(options.menuCombo[i]))
			return false;
	}
	return true;
}

bool DisplayController::navHeld(uint8_t action) {
	const DisplayOptions& options = opts();
	int32_t pin = -1;
	switch (action) {
		case NAV_UP:    pin = options.menuUpPin; break;
		case NAV_DOWN:  pin = options.menuDownPin; break;
		case NAV_LEFT:  pin = options.menuLeftPin; break;
		case NAV_RIGHT: pin = options.menuRightPin; break;
		case NAV_SELECT: pin = options.menuSelectPin; break;
		case NAV_BACK:  pin = options.menuBackPin; break;
		default: break;
	}

	if (pin >= 0)
		return Storage::getInstance().getKeyState().test((uint32_t)pin);

	// No explicit pin: fall back to the gamepad mapping (dpad + B1/B2).
	uint32_t mask = 0;
	switch (action) {
		case NAV_UP:    mask = GAMEPAD_MASK_UP; break;
		case NAV_DOWN:  mask = GAMEPAD_MASK_DOWN; break;
		case NAV_LEFT:  mask = GAMEPAD_MASK_LEFT; break;
		case NAV_RIGHT: mask = GAMEPAD_MASK_RIGHT; break;
		case NAV_SELECT: mask = GAMEPAD_MASK_B1; break;
		case NAV_BACK:  mask = GAMEPAD_MASK_B2; break;
		default: break;
	}
	return gamepadControlHeld(mask);
}

void DisplayController::processNav() {
	if (screen == nullptr) return;
	const uint32_t now = getMillis();
	for (uint8_t action = 0; action < 6; action++) {
		const bool held = navHeld(action);
		if (held && !navPrev[action]) {
			// Edge-trigger: fire the action once, then arm hold-to-repeat if
			// the screen wants it (spinner scrubbing).
			int8_t result = screen->handleNavigation(action);
			if (result >= 0 && result != (int8_t)mode) {
				setMode((DisplayMode)result);
				return;
			}
			if (screen->wantsNavRepeat(action)) {
				repeatActive[action] = true;
				repeatInitial[action] = true;
				repeatSince[action] = now;
				repeatInterval[action] = REPEAT_INTERVAL_MS;
			}
		} else if (held && repeatActive[action] && screen->wantsNavRepeat(action)) {
			// Hold-to-repeat: after the initial delay, re-fire so spinner
			// value scrubbing works. Acceleration matches GP2040-th.
			const uint32_t wait = repeatInitial[action] ? REPEAT_INITIAL_MS : repeatInterval[action];
			if (now - repeatSince[action] >= wait) {
				repeatInitial[action] = false;
				repeatSince[action] = now;
				if (repeatInterval[action] > REPEAT_MIN_MS)
					repeatInterval[action] = repeatInterval[action] - REPEAT_DECREMENT_MS;
				int8_t result = screen->handleNavigation(action);
				if (result >= 0 && result != (int8_t)mode) {
					setMode((DisplayMode)result);
					return;
				}
			}
		} else if (!held) {
			repeatActive[action] = false;
		}
		navPrev[action] = held;
	}
}

void DisplayController::setMode(DisplayMode newMode) {
	if (screen != nullptr && mode == newMode) return;
	mode = newMode;
	lastModeChange = getMillis();
	destroyScreen();

	switch (mode) {
		case SPLASH: {
			SplashScreen* s = new SplashScreen(&gfx);
			s->init();
			screen = s;
			break;
		}
		case BUTTONS: {
			ButtonLayoutScreen* s = new ButtonLayoutScreen(&gfx);
			s->init();
			screen = s;
			break;
		}
		case MAIN_MENU: {
			MainMenuScreen* s = new MainMenuScreen(&gfx);
			s->init();
			screen = s;
			break;
		}
		case REMAP: {
			RemapScreen* s = new RemapScreen(&gfx);
			s->init();
			screen = s;
			break;
		}
		case SAVER: {
			DisplaySaverScreen* s = new DisplaySaverScreen(&gfx);
			s->init();
			screen = s;
			// Display-off saver powers the panel down instead of drawing.
			powered = (opts().displaySaverMode != DISPLAY_SAVER_DISPLAY_OFF);
			gfx.getDriver()->setPower(powered);
			break;
		}
	}
}

void DisplayController::destroyScreen() {
	if (screen != nullptr) {
		screen->shutdown();
		delete screen;
		screen = nullptr;
	}
	if (!powered) {
		powered = true;
		gfx.getDriver()->setPower(true);
	}
}