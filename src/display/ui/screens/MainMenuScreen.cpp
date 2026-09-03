#include "MainMenuScreen.h"
#include "system.h"
#include "drivermanager.h"
#include "version.h"

#include <cctype>

uint8_t MainMenuScreen::savedMenuIndex = 0;

static const char* ledModeNames[] = {
    ANIMATION_CUSTOM_NAME,   ANIMATION_CYCLE_NAME,   ANIMATION_REACTIVE_NAME,
    ANIMATION_BPS_NAME,      ANIMATION_RIPPLE_NAME,  ANIMATION_RAIN_NAME,
    ANIMATION_FIRE_NAME,
};
static const int ledModeCount = sizeof(ledModeNames) / sizeof(ledModeNames[0]);

static std::string formatBytes(uint32_t bytes) {
    if (bytes >= 1024 * 1024)
        return std::to_string(bytes / (1024 * 1024)) + "MB";
    return std::to_string(bytes / 1024) + "K";
}

void MainMenuScreen::init() {
    getRenderer()->clearScreen();
    currentMenu = &mainMenu;
    menuBackStack.clear();
    menuIndex = savedMenuIndex;

    exitToScreen = -1;

    gpMenu = new GPMenu();
    gpMenu->setRenderer(getRenderer());
    gpMenu->setPosition(8, 16);
    gpMenu->setStrokeColor(1);
    gpMenu->setFillColor(1);
    gpMenu->setMenuSize(18, 4);
    gpMenu->setViewport(this->getViewport());
    gpMenu->setShape(GPShape_Type::GP_SHAPE_SQUARE);
    gpMenu->setMenuData(currentMenu);
    gpMenu->setMenuTitle(MAIN_MENU_NAME);
    addElement(gpMenu);

    changeRequiresReboot = false;
    changeRequiresSave = false;
    screenIsPrompting = false;
    promptChoice = false;

    prevInputMode = Storage::getInstance().getDefaultInputMode();
    updateInputMode = prevInputMode;

    prevSocdMode = Storage::getInstance().getSocdMode();
    updateSocdMode = prevSocdMode;

    prevDpadMode = Storage::getInstance().getDpadMode();
    updateDpadMode = prevDpadMode;

    prevProfile = (uint8_t)Storage::getInstance().getActiveProfile();
    updateProfile = prevProfile;

    prevDisplaySaverTimeout = Storage::getInstance().getDisplayOptions().displaySaverTimeout;
    updateDisplaySaverTimeout = prevDisplaySaverTimeout;

    prevDisplaySaverMode = Storage::getInstance().getDisplayOptions().displaySaverMode;
    updateDisplaySaverMode = prevDisplaySaverMode;

    prevInputHistoryTimeout = Storage::getInstance().getDisplayOptions().inputHistoryTimeout;
    updateInputHistoryTimeout = prevInputHistoryTimeout;

    prevAnimationIndex = (uint8_t)Storage::getInstance().getLedOptions().ledMode;
    updateAnimationIndex = prevAnimationIndex;

    prevBrightness = (uint8_t)Storage::getInstance().getLedOptions().brightnessByMode[prevAnimationIndex];
    updateBrightness = prevBrightness;

    prevSpeed = (uint8_t)Storage::getInstance().getLedOptions().ledSpeeds[prevAnimationIndex];
    updateSpeed = prevSpeed;

    prevColorNormal = Storage::getInstance().getLedOptions().colorNormalByMode[prevAnimationIndex];
    updateColorNormal = prevColorNormal;
    prevColorPressed = Storage::getInstance().getLedOptions().colorPressedByMode[prevAnimationIndex];
    updateColorPressed = prevColorPressed;

    // populate the profiles menu
    profilesMenu.clear();
    uint8_t profileCount = Storage::getInstance().getProfileCount();
    if (profileCount == 0) profileCount = 1;
    for (uint8_t profileCtr = 0; profileCtr < profileCount; profileCtr++) {
        std::string menuLabel = "Profile " + std::to_string(profileCtr + 1);
        MenuEntry menuEntry = {menuLabel, NULL, nullptr, std::bind(&MainMenuScreen::currentProfile, this), std::bind(&MainMenuScreen::selectProfile, this), profileCtr};
        profilesMenu.push_back(menuEntry);
    }

    animationMenu.clear();
    for (int i = 0; i < ledModeCount; i++) {
        std::string name = ledModeNames[i];
        for (auto &c : name) c = toupper(c);
        animationMenu.push_back({name, NULL, nullptr,
            std::bind(&MainMenuScreen::currentAnimation, this),
            std::bind(&MainMenuScreen::selectAnimation, this), i});
    }

    brightnessMenu.clear();
    {
        MenuEntry entry;
        entry.isSpinner = true;
        entry.currentValue = std::bind(&MainMenuScreen::currentBrightness, this);
        entry.displayValue = [this]() -> std::string {
            return std::to_string(updateBrightness);
        };
        brightnessMenu.push_back(entry);
    }

    speedMenu.clear();
    {
        MenuEntry speedEntry;
        speedEntry.isSpinner = true;
        speedEntry.currentValue = std::bind(&MainMenuScreen::currentSpeed, this);
        speedEntry.displayValue = [this]() -> std::string {
            return std::to_string(updateSpeed) + "%";
        };
        speedMenu.push_back(speedEntry);
    }

    histTimeoutMenu.clear();
    {
        MenuEntry histEntry;
        histEntry.isSpinner = true;
        histEntry.currentValue = std::bind(&MainMenuScreen::currentInputHistoryTimeout, this);
        histEntry.displayValue = [this]() -> std::string {
            if (updateInputHistoryTimeout == 0) return "Off";
            return std::to_string(updateInputHistoryTimeout) + "s";
        };
        histTimeoutMenu.push_back(histEntry);
    }

    displayTimeoutMenu.clear();
    {
        MenuEntry spinnerEntry;
        spinnerEntry.isSpinner = true;
        spinnerEntry.currentValue = std::bind(&MainMenuScreen::currentDisplaySaverTimeout, this);
        spinnerEntry.displayValue = [this]() -> std::string {
            if (updateDisplaySaverTimeout == 0) return "Off";
            if (currentSpinnerUnit == 0)
                return std::to_string(updateDisplaySaverTimeout) + "s";
            else
                return std::to_string(updateDisplaySaverTimeout / 60) + "m";
        };
        displayTimeoutMenu.push_back(spinnerEntry);
    }

    displaySaverModeMenu.clear();
    displaySaverModeMenu.push_back({DISPLAY_SAVER_OFF_NAME, NULL, nullptr,
        std::bind(&MainMenuScreen::currentDisplaySaverMode, this),
        std::bind(&MainMenuScreen::selectDisplaySaverMode, this), 0});
    displaySaverModeMenu.push_back({DISPLAY_SAVER_SNOW_NAME, NULL, nullptr,
        std::bind(&MainMenuScreen::currentDisplaySaverMode, this),
        std::bind(&MainMenuScreen::selectDisplaySaverMode, this), 1});
    displaySaverModeMenu.push_back({DISPLAY_SAVER_BOUNCE_NAME, NULL, nullptr,
        std::bind(&MainMenuScreen::currentDisplaySaverMode, this),
        std::bind(&MainMenuScreen::selectDisplaySaverMode, this), 2});
    displaySaverModeMenu.push_back({DISPLAY_SAVER_PIPES_NAME, NULL, nullptr,
        std::bind(&MainMenuScreen::currentDisplaySaverMode, this),
        std::bind(&MainMenuScreen::selectDisplaySaverMode, this), 3});
    displaySaverModeMenu.push_back({DISPLAY_SAVER_TOAST_NAME, NULL, nullptr,
        std::bind(&MainMenuScreen::currentDisplaySaverMode, this),
        std::bind(&MainMenuScreen::selectDisplaySaverMode, this), 4});
    displaySaverModeMenu.push_back({DISPLAY_SAVER_STARS_NAME, NULL, nullptr,
        std::bind(&MainMenuScreen::currentDisplaySaverMode, this),
        std::bind(&MainMenuScreen::selectDisplaySaverMode, this), 5});

    displayMenu.clear();
    displayMenu.push_back({"Idle Timeout", NULL, &displayTimeoutMenu,
        std::bind(&MainMenuScreen::modeValue, this),
        std::bind(&MainMenuScreen::testMenu, this)});
    displayMenu.push_back({"Screen Saver", NULL, &displaySaverModeMenu,
        std::bind(&MainMenuScreen::modeValue, this),
        std::bind(&MainMenuScreen::testMenu, this)});
    displayMenu.push_back({"Hist Timeout", NULL, &histTimeoutMenu,
        std::bind(&MainMenuScreen::modeValue, this),
        std::bind(&MainMenuScreen::testMenu, this)});

    {
        auto makeColorEntry = [](uint32_t* color) {
            MenuEntry entry;
            entry.isSpinner = true;
            entry.currentValue = [color]() -> int32_t { return (int32_t)*color; };
            entry.displayValue = [color]() -> std::string {
                char buf[12];
                snprintf(buf, sizeof(buf), "|%02X|%02X|%02X|",
                    (uint8_t)((*color >> 16) & 0xFF),
                    (uint8_t)((*color >> 8) & 0xFF),
                    (uint8_t)(*color & 0xFF));
                return std::string(buf);
            };
            return entry;
        };
        colorNormalMenu.clear();
        colorNormalMenu.push_back(makeColorEntry(&updateColorNormal));
        colorPressedMenu.clear();
        colorPressedMenu.push_back(makeColorEntry(&updateColorPressed));
    }

    colorMenu.clear();
    colorMenu.push_back({"Normal", NULL, &colorNormalMenu,
        std::bind(&MainMenuScreen::modeValue, this),
        std::bind(&MainMenuScreen::testMenu, this)});
    colorMenu.push_back({"Pressed", NULL, &colorPressedMenu,
        std::bind(&MainMenuScreen::modeValue, this),
        std::bind(&MainMenuScreen::testMenu, this)});

    ledMenu.clear();
    ledMenu.push_back({"Mode", NULL, &animationMenu,
        std::bind(&MainMenuScreen::modeValue, this),
        std::bind(&MainMenuScreen::testMenu, this)});
    ledMenu.push_back({"Brightness", NULL, &brightnessMenu,
        std::bind(&MainMenuScreen::modeValue, this),
        std::bind(&MainMenuScreen::testMenu, this)});
    ledMenu.push_back({"Speed", NULL, &speedMenu,
        std::bind(&MainMenuScreen::modeValue, this),
        std::bind(&MainMenuScreen::testMenu, this)});
    ledMenu.push_back({"Colors", NULL, &colorMenu,
        std::bind(&MainMenuScreen::modeValue, this),
        std::bind(&MainMenuScreen::testMenu, this)});

    rebootMenu.clear();
    rebootMenu.push_back({"Normal", NULL, nullptr,
        std::bind(&MainMenuScreen::modeValue, this),
        std::bind(&MainMenuScreen::selectRebootNormal, this), 0});
    rebootMenu.push_back({"Web Config", NULL, nullptr,
        std::bind(&MainMenuScreen::modeValue, this),
        std::bind(&MainMenuScreen::selectRebootWebConfig, this), 1});
    rebootMenu.push_back({"Bootsel", NULL, nullptr,
        std::bind(&MainMenuScreen::modeValue, this),
        std::bind(&MainMenuScreen::selectRebootBootsel, this), 2});

    mainMenu.clear();
    mainMenu.push_back({"Input Mode", NULL, &inputModeMenu, std::bind(&MainMenuScreen::modeValue, this), std::bind(&MainMenuScreen::testMenu, this)});
    mainMenu.push_back({"SOCD Mode", NULL, &socdModeMenu, std::bind(&MainMenuScreen::modeValue, this), std::bind(&MainMenuScreen::testMenu, this)});
    mainMenu.push_back({"D-Pad Mode", NULL, &dpadModeMenu, std::bind(&MainMenuScreen::modeValue, this), std::bind(&MainMenuScreen::testMenu, this)});
    mainMenu.push_back({"Profile", NULL, &profilesMenu, std::bind(&MainMenuScreen::modeValue, this), std::bind(&MainMenuScreen::testMenu, this)});
    mainMenu.push_back({"Display", NULL, &displayMenu, std::bind(&MainMenuScreen::modeValue, this), std::bind(&MainMenuScreen::testMenu, this)});
    mainMenu.push_back({"LED Config", NULL, &ledMenu, std::bind(&MainMenuScreen::modeValue, this), std::bind(&MainMenuScreen::testMenu, this)});
    mainMenu.push_back({"Remap", NULL, nullptr, std::bind(&MainMenuScreen::modeValue, this), std::bind(&MainMenuScreen::selectRemap, this)});
    mainMenu.push_back({"Reboot", NULL, &rebootMenu, std::bind(&MainMenuScreen::modeValue, this), std::bind(&MainMenuScreen::testMenu, this)});
    mainMenu.push_back({"Info", NULL, &infoMenu, std::bind(&MainMenuScreen::modeValue, this), std::bind(&MainMenuScreen::testMenu, this)});
    mainMenu.push_back({"Save & Exit", NULL, &saveMenu, std::bind(&MainMenuScreen::modeValue, this), std::bind(&MainMenuScreen::testMenu, this)});

    gpMenu->setMenuData(currentMenu);
    gpMenu->setMenuTitle(MAIN_MENU_NAME);
    if (menuIndex >= gpMenu->getDataSize())
        menuIndex = 0;
    gpMenu->setIndex(menuIndex);
}

void MainMenuScreen::shutdown() {
    clearElements();
    gpMenu = nullptr;
    exitToScreen = -1;
}

void MainMenuScreen::drawScreen() {
	if (currentMenu == &infoMenu) {
        gpMenu->setVisibility(false);
        getRenderer()->drawText((21 - 4) / 2, 0, "Info");
        std::string versionLine = MP2040VERSION;
        if (versionLine == "dev") {
            versionLine = MP2040BUILD;
        } else {
            const std::string prerelease = "-prerelease";
            size_t pos = versionLine.find(prerelease);
            if (pos != std::string::npos)
                versionLine.replace(pos, prerelease.length(), "-p");
            versionLine += " ";
            versionLine += MP2040BUILD;
        }
        getRenderer()->drawText(2, 2, BOARD_CONFIG_LABEL);
        getRenderer()->drawText(2, 3, versionLine);
        getRenderer()->drawText(2, 4, "Flash " + formatBytes(System::getUsedFlash()) + "/" + formatBytes(System::getTotalFlash()));
        getRenderer()->drawText(2, 5, "RAM " + formatBytes(System::getUsedHeap()) + "/" + formatBytes(System::getTotalHeap()));
        getRenderer()->drawText(2, 6, std::string(__DATE__) + " " + MP2040CONFIG);
        getRenderer()->drawText(2, 7, "EEPROM " + formatBytes(Storage::getInstance().getEepromUsedBytes()) + "/" + formatBytes(EEPROM_SIZE_BYTES));
        return;
    }
    bool isSpinnerView = currentMenu->size() > 0 && currentMenu->at(menuIndex).isSpinner;
    gpMenu->setVisibility(!screenIsPrompting && !isSpinnerView);

    if (!screenIsPrompting) {
        if (isSpinnerView) {
            getRenderer()->drawText(
                (21 - gpMenu->getMenuTitle().length()) / 2, 0,
                gpMenu->getMenuTitle().c_str());
            std::string valueStr = currentMenu->at(0).displayValue();
            if (currentMenu == &colorNormalMenu || currentMenu == &colorPressedMenu) {
                int textX = (20 - valueStr.length()) / 2;
                getRenderer()->drawText(textX, 2, "|R |G |B |");
                getRenderer()->drawText(textX, 3, valueStr);
                int digitCol = textX + 1 + currentSpinnerUnit + currentSpinnerUnit / 2;
                getRenderer()->drawText(digitCol, 4, "^");
                getRenderer()->drawText(2, 5,
                    CHAR_UP CHAR_DOWN ":val " CHAR_LEFT CHAR_RIGHT ":ch");
            } else {
                getRenderer()->drawText(
                    (21 - valueStr.length()) / 2, 3, valueStr.c_str());
                if (currentMenu == &displayTimeoutMenu)
                    getRenderer()->drawText(2, 5,
                        CHAR_UP CHAR_DOWN ":adjust " CHAR_LEFT CHAR_RIGHT ":unit");
            }
            getRenderer()->drawText(3, 6, "B1:set B2:back");
        }
    } else {
        getRenderer()->drawText(1, 1, "Config has changed.");
        if (changeRequiresSave && !changeRequiresReboot) {
            getRenderer()->drawText(3, 3, "Would you like");
            getRenderer()->drawText(6, 4, "to save?");
        } else if (changeRequiresSave && changeRequiresReboot) {
            getRenderer()->drawText(3, 3, "Would you like");
            getRenderer()->drawText(1, 4, "to save & restart?");
        } else {
        }

        if (promptChoice) getRenderer()->drawText(5, 6, CHAR_RIGHT);
        getRenderer()->drawText(6, 6, "Yes");
        if (!promptChoice) getRenderer()->drawText(11, 6, CHAR_RIGHT);
        getRenderer()->drawText(12, 6, "No");
    }
}

void MainMenuScreen::setMenu(std::vector<MenuEntry>* menu) {
    currentMenu = menu;
}

int8_t MainMenuScreen::update() {
    // An exit request with staged changes becomes the save prompt.
    if ((exitToScreen != -1) && (changeRequiresSave || changeRequiresReboot)) {
        exitToScreenBeforePrompt = exitToScreen;
        exitToScreen = -1;
        screenIsPrompting = true;
    }
    return exitToScreen;
}

int8_t MainMenuScreen::handleNavigation(uint8_t action) {
    updateMenuNavigation(action);
    // An exit request with staged changes becomes the save prompt.
    if ((exitToScreen != -1) && (changeRequiresSave || changeRequiresReboot)) {
        exitToScreenBeforePrompt = exitToScreen;
        exitToScreen = -1;
        screenIsPrompting = true;
    }
    return exitToScreen;
}

void MainMenuScreen::updateMenuNavigation(uint8_t action) {
    if (currentMenu == &infoMenu) {
        // Info page is read-only: B1 (select) or B2 (back) returns to the main
        // menu; direction inputs are ignored.
        if (action == MENU_ACTION_SELECT || action == MENU_ACTION_BACK) {
            if (!menuBackStack.empty()) {
                MenuBackEntry back = menuBackStack.back();
                menuBackStack.pop_back();
                currentMenu = back.menu;
                menuIndex = back.index;
                gpMenu->setMenuData(currentMenu);
                gpMenu->setMenuTitle(back.title);
                gpMenu->setIndex(menuIndex);
            }
        }
        isPressed = true;
        return;
    }
    bool changeIndex = false;
    uint16_t menuSize = gpMenu->getDataSize();
    bool isSpinnerItem = false;
    if (currentMenu->size() > 0 && currentMenu->at(menuIndex).isSpinner)
        isSpinnerItem = true;

    switch (action) {
        case MENU_ACTION_UP:
            if (!screenIsPrompting) {
                if (isSpinnerItem) {
                    adjustSpinnerValue(1);
                } else {
                    if (menuIndex > 0) {
                        menuIndex--;
                    } else {
                        menuIndex = menuSize - 1;
                    }
                    changeIndex = true;
                }
            } else {
                promptChoice = !promptChoice;
            }
            isPressed = true;
            break;
        case MENU_ACTION_DOWN:
            if (!screenIsPrompting) {
                if (isSpinnerItem) {
                    adjustSpinnerValue(-1);
                } else {
                    if (menuIndex < menuSize - 1) {
                        menuIndex++;
                    } else {
                        menuIndex = 0;
                    }
                    changeIndex = true;
                }
            } else {
                promptChoice = !promptChoice;
            }
            isPressed = true;
            break;
        case MENU_ACTION_LEFT:
            if (screenIsPrompting) {
                promptChoice = !promptChoice;
            } else if (isSpinnerItem) {
                switchSpinnerUnit(-1);
            }
            isPressed = true;
            break;
        case MENU_ACTION_RIGHT:
            if (screenIsPrompting) {
                promptChoice = !promptChoice;
            } else if (isSpinnerItem) {
                switchSpinnerUnit(1);
            }
            isPressed = true;
            break;
        case MENU_ACTION_SELECT:
            if (!screenIsPrompting) {
                if (isSpinnerItem) {
                    saveSpinnerValue();
                    if (!menuBackStack.empty()) {
                        MenuBackEntry back = menuBackStack.back();
                        menuBackStack.pop_back();
                        currentMenu = back.menu;
                        menuIndex = back.index;
                        changeIndex = true;
                        gpMenu->setMenuData(currentMenu);
                        gpMenu->setMenuTitle(back.title);
                    }
                } else if (currentMenu->at(menuIndex).submenu != nullptr) {
                    menuBackStack.push_back({currentMenu, menuIndex, gpMenu->getMenuTitle()});
                    currentMenu = currentMenu->at(menuIndex).submenu;
                    if (currentMenu->size() > 0 && currentMenu->at(0).isSpinner) {
                        if (currentMenu == &displayTimeoutMenu)
                            spinnerValueSnapshot = updateDisplaySaverTimeout;
                        else if (currentMenu == &histTimeoutMenu)
                            histSpinnerValueSnapshot = updateInputHistoryTimeout;
                        else if (currentMenu == &brightnessMenu)
                            brightnessSpinnerSnapshot = updateBrightness;
                        else if (currentMenu == &speedMenu)
                            speedSpinnerSnapshot = updateSpeed;
                        else if (currentMenu == &colorNormalMenu) {
                            spinnerValueSnapshot = updateColorNormal;
                            currentSpinnerUnit = 0;
                        } else if (currentMenu == &colorPressedMenu) {
                            spinnerValueSnapshot = updateColorPressed;
                            currentSpinnerUnit = 0;
                        }
                    }
                    gpMenu->setMenuData(currentMenu);
                    gpMenu->setMenuTitle(menuBackStack.back().menu->at(menuBackStack.back().index).label);
                    menuIndex = 0;
                    for (size_t i = 0; i < currentMenu->size(); i++) {
                        if (currentMenu->at(i).optionValue != -1 &&
                            currentMenu->at(i).currentValue() == currentMenu->at(i).optionValue) {
                            menuIndex = i;
                            break;
                        }
                    }
                    changeIndex = true;
                } else {
                    currentMenu->at(menuIndex).action();
                }
            } else {
                if (promptChoice) {
                    saveOptions();
                } else {
                    resetOptions();
                    exitToScreen = DisplayMode::BUTTONS;
                    exitToScreenBeforePrompt = DisplayMode::BUTTONS;
                    isPressed = false;
                }
            }
            isPressed = true;
            break;
        case MENU_ACTION_BACK:
            if (!screenIsPrompting) {
                if (isSpinnerItem)
                    revertSpinnerValue();
                if (!menuBackStack.empty()) {
                    MenuBackEntry back = menuBackStack.back();
                    menuBackStack.pop_back();
                    currentMenu = back.menu;
                    menuIndex = back.index;
                    changeIndex = true;
                    gpMenu->setMenuData(currentMenu);
                    gpMenu->setMenuTitle(back.title);
                } else {
                    exitToScreen = DisplayMode::BUTTONS;
                    exitToScreenBeforePrompt = DisplayMode::BUTTONS;
                    isPressed = false;
                }
            } else {
                screenIsPrompting = false;
                isPressed = false;
            }
            isPressed = true;
            break;
        default:
            break;
    }

    if (changeIndex) gpMenu->setIndex(menuIndex);
}

void MainMenuScreen::saveAndExit() {
    savedMenuIndex = menuIndex;
    if (changeRequiresSave) {
        saveOptions();
    }
    // Always leave the menu after an explicit Save & Exit.
    if (exitToScreen == -1)
        exitToScreen = DisplayMode::BUTTONS;
}

void MainMenuScreen::selectInputMode() {
    if (currentMenu->at(menuIndex).optionValue != -1) {
        InputMode valueToSave = (InputMode)currentMenu->at(menuIndex).optionValue;
        prevInputMode = Storage::getInstance().getDefaultInputMode();
        updateInputMode = valueToSave;

        if (prevInputMode != valueToSave) {
            // input mode requires a save and reboot
            changeRequiresReboot = true;
            changeRequiresSave = true;
        }
    }
}

int32_t MainMenuScreen::currentInputMode() {
    return updateInputMode;
}

void MainMenuScreen::selectSOCDMode() {
    if (currentMenu->at(menuIndex).optionValue != -1) {
        SOCDMode valueToSave = (SOCDMode)currentMenu->at(menuIndex).optionValue;
        prevSocdMode = Storage::getInstance().getSocdMode();
        updateSocdMode = valueToSave;

        if (prevSocdMode != valueToSave) changeRequiresSave = true;
    }
}

int32_t MainMenuScreen::currentSOCDMode() {
    return updateSocdMode;
}

void MainMenuScreen::selectDpadMode() {
    if (currentMenu->at(menuIndex).optionValue != -1) {
        DpadMode valueToSave = (DpadMode)currentMenu->at(menuIndex).optionValue;
        prevDpadMode = Storage::getInstance().getDpadMode();
        updateDpadMode = valueToSave;

        if (prevDpadMode != valueToSave) changeRequiresSave = true;
    }
}

int32_t MainMenuScreen::currentDpadMode() {
    return updateDpadMode;
}

void MainMenuScreen::resetOptions() {
    Storage& s = Storage::getInstance();
    if (changeRequiresSave) {
        if (prevInputMode != updateInputMode) updateInputMode = prevInputMode;
        if (prevSocdMode != updateSocdMode) updateSocdMode = prevSocdMode;
        if (prevDpadMode != updateDpadMode) updateDpadMode = prevDpadMode;
        if (prevProfile != updateProfile) updateProfile = prevProfile;
        if (prevDisplaySaverTimeout != updateDisplaySaverTimeout) updateDisplaySaverTimeout = prevDisplaySaverTimeout;
        if (prevDisplaySaverMode != updateDisplaySaverMode) updateDisplaySaverMode = prevDisplaySaverMode;
        if (prevInputHistoryTimeout != updateInputHistoryTimeout) updateInputHistoryTimeout = prevInputHistoryTimeout;
        if (prevAnimationIndex != updateAnimationIndex) updateAnimationIndex = prevAnimationIndex;
        if (prevBrightness != updateBrightness) updateBrightness = prevBrightness;
        if (prevSpeed != updateSpeed) updateSpeed = prevSpeed;
        if (prevColorNormal != updateColorNormal) updateColorNormal = prevColorNormal;
        if (prevColorPressed != updateColorPressed) updateColorPressed = prevColorPressed;
    }

    changeRequiresSave = false;
    changeRequiresReboot = false;
    screenIsPrompting = false;

    // Discard any live LED preview (the strip may still show a staged mode).
    LedPreview preview;
    s.buildLedPreviewFromConfig(preview);
    s.publishLedPreview(preview);
}

void MainMenuScreen::saveOptions() {
    Storage& s = Storage::getInstance();

    if (changeRequiresSave) {
        bool saveHasChanged = false;

        if (prevInputMode != updateInputMode) {
            s.setDefaultInputMode(updateInputMode);
            saveHasChanged = true;
        }
        if (prevSocdMode != updateSocdMode) {
            s.setSocdMode(updateSocdMode);
            saveHasChanged = true;
        }
        if (prevDpadMode != updateDpadMode) {
            s.setDpadMode(updateDpadMode);
            saveHasChanged = true;
        }
        if (prevProfile != updateProfile) {
            s.setActiveProfile(updateProfile);
            s.applyActiveProfile();
            saveHasChanged = true;
        }
        if (prevDisplaySaverTimeout != updateDisplaySaverTimeout) {
            s.getDisplayOptions().displaySaverTimeout = updateDisplaySaverTimeout;
            saveHasChanged = true;
        }
        if (prevDisplaySaverMode != updateDisplaySaverMode) {
            s.getDisplayOptions().displaySaverMode = (DisplaySaverMode)updateDisplaySaverMode;
            saveHasChanged = true;
        }
        if (prevInputHistoryTimeout != updateInputHistoryTimeout) {
            s.getDisplayOptions().inputHistoryTimeout = updateInputHistoryTimeout;
            saveHasChanged = true;
        }
        if (prevAnimationIndex != updateAnimationIndex) {
            s.getLedOptions().ledMode = updateAnimationIndex;
            // ledMode is per-profile (the durable source re-applied at boot);
            // mirror it so the change survives a reboot.
            Profile* profile = s.getProfile(s.getActiveProfile());
            if (profile != nullptr) {
                profile->has_ledMode = true;
                profile->ledMode = updateAnimationIndex;
            }
            saveHasChanged = true;
        }
        if (prevBrightness != updateBrightness) {
            s.getLedOptions().brightnessByMode[updateAnimationIndex] = updateBrightness;
            saveHasChanged = true;
        }
        if (prevSpeed != updateSpeed) {
            s.getLedOptions().ledSpeeds[updateAnimationIndex] = updateSpeed;
            saveHasChanged = true;
        }
        if (prevColorNormal != updateColorNormal) {
            s.getLedOptions().colorNormalByMode[updateAnimationIndex] = updateColorNormal;
            saveHasChanged = true;
        }
        if (prevColorPressed != updateColorPressed) {
            s.getLedOptions().colorPressedByMode[updateAnimationIndex] = updateColorPressed;
            saveHasChanged = true;
        }

        if (saveHasChanged) {
            s.save(true);
            // Push the edited LED state live so the strip reflects it now.
            LedPreview preview;
            s.buildLedPreviewFromConfig(preview);
            s.publishLedPreview(preview);
        }
        changeRequiresSave = false;
        changeRequiresReboot = false;
        screenIsPrompting = false;
    }

    // Input mode changes require a reboot to re-enumerate the USB driver.
    if (prevInputMode != updateInputMode) {
        System::reboot(System::BootMode::DEFAULT);
    }

    if (exitToScreenBeforePrompt != -1) {
        exitToScreen = exitToScreenBeforePrompt;
        exitToScreenBeforePrompt = -1;
    }
}

void MainMenuScreen::selectProfile() {
    if (currentMenu->at(menuIndex).optionValue != -1) {
        uint8_t valueToSave = currentMenu->at(menuIndex).optionValue;
        prevProfile = (uint8_t)Storage::getInstance().getActiveProfile();
        updateProfile = valueToSave;

        if (prevProfile != valueToSave) changeRequiresSave = true;
    }
}

int32_t MainMenuScreen::currentProfile() {
    return updateProfile;
}

void MainMenuScreen::selectDisplaySaverTimeout() {
    if (currentMenu->at(menuIndex).optionValue != -1) {
        uint32_t valueToSave = currentMenu->at(menuIndex).optionValue;
        prevDisplaySaverTimeout = Storage::getInstance().getDisplayOptions().displaySaverTimeout;
        updateDisplaySaverTimeout = valueToSave;

        if (prevDisplaySaverTimeout != valueToSave) changeRequiresSave = true;
    }
}

int32_t MainMenuScreen::currentDisplaySaverTimeout() {
    return updateDisplaySaverTimeout;
}

void MainMenuScreen::selectDisplaySaverMode() {
    if (currentMenu->at(menuIndex).optionValue != -1) {
        uint8_t valueToSave = currentMenu->at(menuIndex).optionValue;
        prevDisplaySaverMode = Storage::getInstance().getDisplayOptions().displaySaverMode;
        updateDisplaySaverMode = valueToSave;

        if (prevDisplaySaverMode != valueToSave) changeRequiresSave = true;
    }
}

int32_t MainMenuScreen::currentDisplaySaverMode() {
    return updateDisplaySaverMode;
}

void MainMenuScreen::selectInputHistoryTimeout() {
    if (currentMenu->at(menuIndex).optionValue != -1) {
        uint16_t valueToSave = currentMenu->at(menuIndex).optionValue;
        prevInputHistoryTimeout = Storage::getInstance().getDisplayOptions().inputHistoryTimeout;
        updateInputHistoryTimeout = valueToSave;

        if (prevInputHistoryTimeout != valueToSave) changeRequiresSave = true;
    }
}

int32_t MainMenuScreen::currentInputHistoryTimeout() {
    return updateInputHistoryTimeout;
}

void MainMenuScreen::selectAnimation() {
    if (currentMenu->at(menuIndex).optionValue != -1) {
        uint8_t valueToSave = currentMenu->at(menuIndex).optionValue;
        updateAnimationIndex = valueToSave;
        if (prevAnimationIndex != valueToSave)
            changeRequiresSave = true;
        // Load the newly selected mode's own brightness/speed/colors into the
        // staged spinners and their baseline so the preview and the spinners
        // reflect that mode's config values, not the previously selected mode's.
        const LEDOptions& lo = Storage::getInstance().getLedOptions();
        uint8_t brightness = (uint8_t)(lo.brightnessByMode_count > valueToSave
            ? lo.brightnessByMode[valueToSave] : lo.brightnessMaximum);
        uint8_t speed = (uint8_t)(lo.ledSpeeds_count > valueToSave
            ? lo.ledSpeeds[valueToSave] : lo.ledSpeed);
        if (speed > 100) speed = 100;
        uint32_t normal = lo.colorNormalByMode_count > valueToSave
            ? lo.colorNormalByMode[valueToSave] : lo.colorNormal;
        uint32_t pressed = lo.colorPressedByMode_count > valueToSave
            ? lo.colorPressedByMode[valueToSave] : lo.colorPressed;
        prevBrightness = updateBrightness = brightness;
        prevSpeed = updateSpeed = speed;
        prevColorNormal = updateColorNormal = normal;
        prevColorPressed = updateColorPressed = pressed;
        // Always push a preview so the newly selected mode applies live (and
        // switching back to the saved mode re-applies it).
        previewLedState();
    }
}

int32_t MainMenuScreen::currentAnimation() {
    return updateAnimationIndex;
}

int32_t MainMenuScreen::currentBrightness() {
    return updateBrightness;
}

int32_t MainMenuScreen::currentSpeed() {
    return updateSpeed;
}

void MainMenuScreen::selectRemap() {
    savedMenuIndex = menuIndex;
    exitToScreen = DisplayMode::REMAP;
}

void MainMenuScreen::selectRebootNormal() {
    System::reboot(System::BootMode::DEFAULT);
}

void MainMenuScreen::selectRebootWebConfig() {
    System::reboot(System::BootMode::WEBCONFIG);
}

void MainMenuScreen::selectRebootBootsel() {
    System::reboot(System::BootMode::USB);
}

void MainMenuScreen::adjustSpinnerValue(int8_t direction) {
    if (currentMenu == &displayTimeoutMenu) {
        uint32_t raw = updateDisplaySaverTimeout;

        if (raw == 0 && direction > 0) {
            updateDisplaySaverTimeout = 5;
            currentSpinnerUnit = 0;
            if (prevDisplaySaverTimeout != updateDisplaySaverTimeout)
                changeRequiresSave = true;
            return;
        }

        if (currentSpinnerUnit == 0) {
            int32_t displayVal = raw / 5;
            displayVal += direction;
            if (displayVal > 120) displayVal = 120;
            else if (displayVal < 0) displayVal = 0;
            updateDisplaySaverTimeout = displayVal * 5;
        } else {
            int32_t displayVal = raw / 60;
            displayVal += direction;
            if (displayVal > 30) displayVal = 30;
            else if (displayVal < 1) displayVal = 1;
            updateDisplaySaverTimeout = displayVal * 60;
        }

        if (prevDisplaySaverTimeout != updateDisplaySaverTimeout)
            changeRequiresSave = true;
    } else if (currentMenu == &histTimeoutMenu) {
        int32_t displayVal = updateInputHistoryTimeout;
        if (displayVal == 0 && direction > 0) {
            displayVal = 1;
        } else {
            displayVal += direction;
            if (displayVal > 60) displayVal = 60;
            else if (displayVal < 0) displayVal = 0;
        }
        updateInputHistoryTimeout = displayVal;
        if (prevInputHistoryTimeout != updateInputHistoryTimeout)
            changeRequiresSave = true;
    } else if (currentMenu == &speedMenu) {
        int32_t val = updateSpeed + direction * 5;
        if (val > 100) val = 100;
        else if (val < 0) val = 0;
        updateSpeed = val;
        if (prevSpeed != updateSpeed) changeRequiresSave = true;
        previewLedState();
    } else if (currentMenu == &brightnessMenu) {
        int32_t val = updateBrightness + direction * 5;
        if (val > 255) val = 255;
        else if (val < 0) val = 0;
        updateBrightness = val;
        if (prevBrightness != updateBrightness) changeRequiresSave = true;
        previewLedState();
    } else if (currentMenu == &colorNormalMenu || currentMenu == &colorPressedMenu) {
        uint32_t* color = (currentMenu == &colorNormalMenu) ? &updateColorNormal : &updateColorPressed;
        uint32_t* prev = (currentMenu == &colorNormalMenu) ? &prevColorNormal : &prevColorPressed;
        uint8_t shift = (5 - currentSpinnerUnit) * 4;
        int32_t nibble = (*color >> shift) & 0xF;
        nibble += direction;
        if (nibble > 15) nibble = 15;
        else if (nibble < 0) nibble = 0;
        *color = (*color & ~(0xF << shift)) | ((uint32_t)nibble << shift);
        if (*prev != *color) changeRequiresSave = true;
        previewLedState();
    }
}

void MainMenuScreen::switchSpinnerUnit(int8_t direction) {
    if (currentMenu == &colorNormalMenu || currentMenu == &colorPressedMenu) {
        if (direction > 0)
            currentSpinnerUnit = (currentSpinnerUnit + 1) % 6;
        else
            currentSpinnerUnit = (currentSpinnerUnit + 5) % 6;
        return;
    }
    if (currentMenu != &displayTimeoutMenu) return;
    if (currentSpinnerUnit == 0 && direction > 0) {
        if (updateDisplaySaverTimeout > 0 && updateDisplaySaverTimeout < 60)
            updateDisplaySaverTimeout = 60;
        currentSpinnerUnit = 1;
    } else if (currentSpinnerUnit == 1 && direction < 0) {
        if (updateDisplaySaverTimeout / 60 > 30)
            updateDisplaySaverTimeout = 600;
        currentSpinnerUnit = 0;
    }
}

void MainMenuScreen::saveSpinnerValue() {
    Storage& s = Storage::getInstance();
    if (currentMenu == &displayTimeoutMenu) {
        if (spinnerValueSnapshot != updateDisplaySaverTimeout) {
            prevDisplaySaverTimeout = updateDisplaySaverTimeout;
            s.getDisplayOptions().displaySaverTimeout = updateDisplaySaverTimeout;
            s.save(true);
        }
    } else if (currentMenu == &histTimeoutMenu) {
        if (histSpinnerValueSnapshot != updateInputHistoryTimeout) {
            prevInputHistoryTimeout = updateInputHistoryTimeout;
            s.getDisplayOptions().inputHistoryTimeout = updateInputHistoryTimeout;
            s.save(true);
        }
    } else if (currentMenu == &speedMenu) {
        if (speedSpinnerSnapshot != updateSpeed) {
            prevSpeed = updateSpeed;
            s.getLedOptions().ledSpeeds[updateAnimationIndex] = updateSpeed;
            s.save(true);
            LedPreview preview;
            s.buildLedPreviewFromConfig(preview);
            s.publishLedPreview(preview);
        }
    } else if (currentMenu == &brightnessMenu) {
        if (brightnessSpinnerSnapshot != updateBrightness) {
            prevBrightness = updateBrightness;
            s.getLedOptions().brightnessByMode[updateAnimationIndex] = updateBrightness;
            s.save(true);
            LedPreview preview;
            s.buildLedPreviewFromConfig(preview);
            s.publishLedPreview(preview);
        }
    } else if (currentMenu == &colorNormalMenu) {
        if (spinnerValueSnapshot != updateColorNormal) {
            prevColorNormal = updateColorNormal;
            s.getLedOptions().colorNormalByMode[updateAnimationIndex] = updateColorNormal;
            s.save(true);
            LedPreview preview;
            s.buildLedPreviewFromConfig(preview);
            s.publishLedPreview(preview);
        }
    } else if (currentMenu == &colorPressedMenu) {
        if (spinnerValueSnapshot != updateColorPressed) {
            prevColorPressed = updateColorPressed;
            s.getLedOptions().colorPressedByMode[updateAnimationIndex] = updateColorPressed;
            s.save(true);
            LedPreview preview;
            s.buildLedPreviewFromConfig(preview);
            s.publishLedPreview(preview);
        }
    }
}

void MainMenuScreen::revertSpinnerValue() {
    if (currentMenu == &displayTimeoutMenu) {
        updateDisplaySaverTimeout = spinnerValueSnapshot;
        prevDisplaySaverTimeout = spinnerValueSnapshot;
    } else if (currentMenu == &histTimeoutMenu) {
        updateInputHistoryTimeout = histSpinnerValueSnapshot;
        prevInputHistoryTimeout = histSpinnerValueSnapshot;
    } else if (currentMenu == &speedMenu) {
        updateSpeed = speedSpinnerSnapshot;
        prevSpeed = speedSpinnerSnapshot;
    } else if (currentMenu == &brightnessMenu) {
        updateBrightness = brightnessSpinnerSnapshot;
        prevBrightness = brightnessSpinnerSnapshot;
    } else if (currentMenu == &colorNormalMenu) {
        updateColorNormal = spinnerValueSnapshot;
        prevColorNormal = spinnerValueSnapshot;
    } else if (currentMenu == &colorPressedMenu) {
        updateColorPressed = spinnerValueSnapshot;
        prevColorPressed = spinnerValueSnapshot;
    }
    previewLedState();
}

void MainMenuScreen::previewLedState() {
    Storage& s = Storage::getInstance();
    LedPreview preview;
    s.buildLedPreviewFromConfig(preview);
    // Overlay the staged (not-yet-saved) LED values so the strip reflects
    // what's being edited right now (MP2040's LedPreview pipeline replaces
    // GP2040-th's setPreviewColor). Mode too, so a staged mode change is
    // visible while scrubbing its per-mode brightness/speed/colors.
    const uint32_t mode = updateAnimationIndex < 7 ? updateAnimationIndex : 0;
    preview.ledMode = mode;
    if (mode < preview.ledSpeedCount) preview.ledSpeed[mode] = updateSpeed;
    if (mode < preview.brightnessByModeCount) preview.brightnessByMode[mode] = updateBrightness;
    if (mode < preview.colorCount) {
        preview.colorNormalByMode[mode] = updateColorNormal;
        preview.colorPressedByMode[mode] = updateColorPressed;
    }
    s.publishLedPreview(preview);
}