#ifndef _GPGFX_UI_SCREENS_H_
#define _GPGFX_UI_SCREENS_H_

// Screen-change protocol shared by the display subsystem. Screens return a
// DisplayMode value from update()/handleNavigation() to request a switch;
// -1 means no change.
enum DisplayMode {
    SPLASH = 0,
    BUTTONS,
    MAIN_MENU,
    REMAP,
    SAVER
};

#include "ui/screens/SplashScreen.h"
#include "ui/screens/ButtonLayoutScreen.h"
#include "ui/screens/MainMenuScreen.h"
#include "ui/screens/RemapScreen.h"
#include "ui/screens/DisplaySaverScreen.h"

#endif