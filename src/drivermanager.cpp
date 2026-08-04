#include "drivermanager.h"

#include "drivers/net/NetDriver.h"
#include "drivers/keyboard/KeyboardDriver.h"

void DriverManager::setup(InputMode mode) {
    switch (mode) {
        case INPUT_MODE_CONFIG:
            driver = new NetDriver();
            break;
        case INPUT_MODE_KEYBOARD:
            driver = new KeyboardDriver();
            break;
        default:
            return;
    }

    // Initialize our chosen driver
    driver->initialize();
    inputMode = mode;
}
