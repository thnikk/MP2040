#include "MainMenuScreen.h"
#include "storagemanager.h"
#include "drivermanager.h"
#include "system.h"

void MainMenuScreen::init() {
	getRenderer()->clearScreen();
	menuIndex = 0;
	lastActivity = getMillis();
	rebuildMenu();
}

void MainMenuScreen::shutdown() {
	clearElements();
}

void MainMenuScreen::rebuildMenu() {
	menu.clear();

	MenuEntry profile;
	profile.label = "Profile";
	profile.isSpinner = true;
	profile.spinnerMin = 0;
	profile.spinnerMax = 3;
	profile.spinnerStep = 1;
	profile.displayValue = [this]() { return std::to_string(currentProfile()); };
	menu.push_back(profile);

	MenuEntry ledMode;
	ledMode.label = "LED Mode";
	ledMode.isSpinner = true;
	ledMode.spinnerMin = 0;
	ledMode.spinnerMax = 6;
	ledMode.spinnerStep = 1;
	ledMode.displayValue = [this]() {
		static const char* names[] = {"CUSTOM", "CYCLE", "REACTIVE", "BPS", "RIPPLE", "RAIN", "FIRE"};
		uint32_t m = currentLedMode();
		return m < 7 ? names[m] : "?";
	};
	menu.push_back(ledMode);

	MenuEntry brightness;
	brightness.label = "Brightness";
	brightness.isSpinner = true;
	brightness.spinnerMin = 0;
	brightness.spinnerMax = 255;
	brightness.spinnerStep = 5;
	brightness.displayValue = [this]() { return std::to_string(currentBrightness()); };
	menu.push_back(brightness);

	MenuEntry webconfig;
	webconfig.label = "Web Config";
	webconfig.action = [this]() { System::reboot(System::BootMode::WEBCONFIG); };
	menu.push_back(webconfig);

	MenuEntry bootsel;
	bootsel.label = "Bootsel";
	bootsel.action = [this]() { System::reboot(System::BootMode::USB); };
	menu.push_back(bootsel);

	MenuEntry exit;
	exit.label = "Exit";
	exit.action = [this]() { /* handled in onSelect via return */ };
	menu.push_back(exit);
}

int8_t MainMenuScreen::handleNavigation(uint8_t action) {
	lastActivity = getMillis();
	switch (action) {
		case MENU_ACTION_UP:
			if (menuIndex > 0) menuIndex--;
			break;
		case MENU_ACTION_DOWN:
			if (menuIndex < menu.size() - 1) menuIndex++;
			break;
		case MENU_ACTION_LEFT:
			onLeft();
			break;
		case MENU_ACTION_RIGHT:
			onRight();
			break;
		case MENU_ACTION_SELECT:
			return onSelect();
			break;
		case MENU_ACTION_BACK:
			return 1; // back to buttons screen
		default:
			break;
	}
	return -1;
}

void MainMenuScreen::onLeft() {
	MenuEntry& entry = menu[menuIndex];
	if (!entry.isSpinner) return;
	int32_t value = 0;
	if (menuIndex == 0) value = (int32_t)currentProfile();
	else if (menuIndex == 1) value = (int32_t)currentLedMode();
	else if (menuIndex == 2) value = (int32_t)currentBrightness();
	value -= entry.spinnerStep;
	if (value < entry.spinnerMin) value = entry.spinnerMax;
	applySpinnerValue(menuIndex, value);
}

void MainMenuScreen::onRight() {
	MenuEntry& entry = menu[menuIndex];
	if (!entry.isSpinner) return;
	int32_t value = 0;
	if (menuIndex == 0) value = (int32_t)currentProfile();
	else if (menuIndex == 1) value = (int32_t)currentLedMode();
	else if (menuIndex == 2) value = (int32_t)currentBrightness();
	value += entry.spinnerStep;
	if (value > entry.spinnerMax) value = entry.spinnerMin;
	applySpinnerValue(menuIndex, value);
}

void MainMenuScreen::applySpinnerValue(uint16_t index, uint32_t value) {
	Storage& s = Storage::getInstance();
	switch (index) {
		case 0: // profile
			s.setActiveProfile(value);
			s.applyActiveProfile();
			break;
		case 1: // LED mode (per-profile)
		{
			Profile* profile = s.getProfile(s.getActiveProfile());
			if (profile != nullptr) { profile->has_ledMode = true; profile->ledMode = value; }
			s.getLedOptions().ledMode = value;
			break;
		}
		case 2: // brightness (current mode, global)
		{
			LEDOptions& led = s.getLedOptions();
			if (led.brightnessByMode_count < 7) led.brightnessByMode_count = 7;
			uint32_t m = led.ledMode < 7 ? led.ledMode : 0;
			led.brightnessByMode[m] = value;
			break;
		}
		default:
			break;
	}
	saveAndPublish();
}

int8_t MainMenuScreen::onSelect() {
	MenuEntry& entry = menu[menuIndex];
	if (entry.label == "Exit")
		return 1; // back to buttons screen
	if (entry.action) {
		entry.action();
	}
	return -1;
}

void MainMenuScreen::saveAndPublish() {
	Storage& s = Storage::getInstance();
	s.save(true);
	LedPreview preview;
	s.buildLedPreviewFromConfig(preview);
	s.publishLedPreview(preview);
}

uint32_t MainMenuScreen::currentProfile() const {
	return Storage::getInstance().getActiveProfile();
}

uint32_t MainMenuScreen::currentLedMode() const {
	const LEDOptions& led = Storage::getInstance().getLedOptions();
	return led.ledMode < 7 ? led.ledMode : 0;
}

uint32_t MainMenuScreen::currentBrightness() const {
	const LEDOptions& led = Storage::getInstance().getLedOptions();
	uint32_t m = led.ledMode < 7 ? led.ledMode : 0;
	return m < led.brightnessByMode_count ? led.brightnessByMode[m] : led.brightnessMaximum;
}

int8_t MainMenuScreen::update() {
	return -1;
}

void MainMenuScreen::drawScreen() {
	getRenderer()->drawText((21 - 4) / 2, 0, "MENU");

	// visible window: entries start at row 2, 7 rows fit (rows 2-8)
	const uint8_t visible = 7;
	uint16_t start = menuIndex < visible ? 0 : menuIndex - (visible - 1);
	for (uint8_t i = 0; i < visible; i++) {
		uint16_t idx = start + i;
		if (idx >= menu.size()) break;
		MenuEntry& entry = menu[idx];
		std::string line = entry.label;
		if (entry.isSpinner && entry.displayValue) {
			line += ": " + entry.displayValue();
		}
		getRenderer()->drawText(2, 2 + i, line);
	}

	// cursor
	getRenderer()->drawText(1, 2 + (menuIndex - start), CHAR_RIGHT);
}