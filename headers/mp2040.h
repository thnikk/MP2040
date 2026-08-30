#ifndef MP2040_H_
#define MP2040_H_

#include "types.h"
#include "keymask.h"
#include "system.h"
#include "enums.pb.h"
#include "pico/types.h"
#include "hardware/gpio.h"

class MP2040 {
public:
    MP2040(){}
    ~MP2040(){}
    void setup();           // setup core0
    void run();             // loop core0
private:
    // GPIO debouncer
    void debounceGpioGetAll();
    GpioMask buttonGpios;
    GpioMask touchGpios;
    KeyMask debouncedGpio;
    uint32_t gpioDebounceTime[MAX_KEYS];

    // Boot-pin window for touch boards. While nonzero, run() waits up to
    // WEB_CONFIG_TOUCH_WINDOW_MS before starting the keyboard so a touch pad
    // (web config or boot) can be touched to enter that mode. 0 = no window
    // (button boards).
    uint32_t bootTouchDeadline = 0;

    // Boot mode captured once in setup() from the watchdog scratch register
    // (takeBootMode() resets it, so it can only be read once per boot). Used by
    // getBootAction() and to decide whether the touch pads load their stored
    // calibration (web config reboot) instead of recalibrating.
    System::BootMode bootMode = System::BootMode::DEFAULT;

    enum class BootAction {
        NONE,
        ENTER_WEBCONFIG_MODE,
        ENTER_USB_MODE,
        SET_INPUT_MODE_KEYBOARD,
        SET_INPUT_MODE_MIDI,
        SET_INPUT_MODE_XINPUT,
        SET_INPUT_MODE_SWITCH_PRO,
        SET_INPUT_MODE_XBOX_ONE,
    };
    BootAction getBootAction();
    // InputMode for a SET_INPUT_MODE_* boot action (CONFIG/unknown -> keyboard).
    InputMode bootActionToInputMode(BootAction action);
    // True if the given pin/index is held at boot. Handles direct-pin buttons
    // and matrix key indices; touch pads defer to the boot window in run().
    bool isBootPinHeld(int32_t pin);

    // GPIO manipulation for setup. configBoot = this boot is a web config
    // reboot: touch pads load their stored calibration instead of re-sampling.
    void initializeKeyGpio(bool configBoot);
    void deinitializeKeyGpio();

    // Publish a live-preview restoring the board's configured LED mode (undoes
    // the boot-window cue). Called when the window closes without a touch.
    void restoreBoardLedMode();

    // Matrix input mode: drive each row low and read the columns, producing a
    // key-state mask where bit N = linear key (row N/COLS, col N%COLS).
    KeyMask scanMatrix();
};

#endif
