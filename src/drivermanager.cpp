#include "drivermanager.h"

#include "drivers/net/NetDriver.h"
#include "drivers/keyboard/KeyboardDriver.h"
#include "drivers/midi/MidiDriver.h"
#include "drivers/xinput/XInputDriver.h"
#include "drivers/switchpro/SwitchProDriver.h"
#include "drivers/xbone/XBoneDriver.h"
#include "drivers/ps3/PS3Driver.h"
#include "drivers/ps4/PS4Driver.h"

#include "usbhostmanager.h"

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
        case INPUT_MODE_XBOX_ONE:
            driver = new XBoneDriver();
            break;
        case INPUT_MODE_PS3:
            driver = new PS3Driver();
            break;
        case INPUT_MODE_PS4:
            driver = new PS4Driver(PS4_CONTROLLER);
            break;
        case INPUT_MODE_PS5:
            // PS5 mode reuses PS4Driver's PS4_ARCADESTICK personality --
            // see PS4Driver.h.
            driver = new PS4Driver(PS4_ARCADESTICK);
            break;
        default:
            return;
    }

    // Initialize our chosen driver
    driver->initialize();
    inputMode = mode;

    // If the driver wants the USB host port (PS4/PS5 auth-dongle
    // passthrough), register it and bring the host controller up. A no-op
    // on boards that don't define a host port (see usbhostmanager.cpp).
    USBListener* authListener = driver->get_usb_auth_listener();
    if (authListener != nullptr) {
        USBHostManager::getInstance().pushListener(authListener);
        USBHostManager::getInstance().start();
    }
}
