#include "drivermanager.h"

#include "drivers/net/NetDriver.h"
#include "drivers/keyboard/KeyboardDriver.h"
#include "drivers/midi/MidiDriver.h"
#include "drivers/xinput/XInputDriver.h"
#include "drivers/switchpro/SwitchProDriver.h"

void DriverManager::setup(InputMode mode) {
    switch (mode) {
        case INPUT_MODE_CONFIG:
            driver = new NetDriver();
            break;
        case INPUT_MODE_KEYBOARD:
            driver = new KeyboardDriver();
            break;
        case INPUT_MODE_MIDI:
            driver = new MidiDriver();
            break;
        case INPUT_MODE_XINPUT:
            driver = new XInputDriver();
            break;
        case INPUT_MODE_SWITCH_PRO:
            driver = new SwitchProDriver();
            break;
        default:
            return;
    }

    // Initialize our chosen driver
    driver->initialize();
    inputMode = mode;
}
