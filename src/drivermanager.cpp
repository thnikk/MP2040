#include "drivermanager.h"

#include "drivers/net/NetDriver.h"
#include "drivers/keyboard/KeyboardDriver.h"
#include "drivers/midi/MidiDriver.h"

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
        default:
            return;
    }

    // Initialize our chosen driver
    driver->initialize();
    inputMode = mode;
}
