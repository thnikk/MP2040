//
// Host shim for tinyusb's "class/hid/hid.h" (MP2040 display simulator).
//
// BoardConfig.h includes this for the HID_KEY_* keycode macros. The display
// subsystem never expands those macros (it draws layouts / menus, not
// keycodes), so an empty shim is enough and keeps tinyusb out of the build.
//
#ifndef _SIM_TUSB_HID_H_
#define _SIM_TUSB_HID_H_

#endif
