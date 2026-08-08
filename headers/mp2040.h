#ifndef MP2040_H_
#define MP2040_H_

#include "types.h"
#include "keymask.h"
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

    enum class BootAction {
        NONE,
        ENTER_WEBCONFIG_MODE,
        ENTER_USB_MODE,
    };
    BootAction getBootAction();
    // True if the given pin/index is held at boot. Handles direct-pin buttons
    // and matrix key indices; touch pads defer to the boot window in run().
    bool isBootPinHeld(int32_t pin);

    // GPIO manipulation for setup
    void initializeKeyGpio();
    void deinitializeKeyGpio();

    // Matrix input mode: drive each row low and read the columns, producing a
    // key-state mask where bit N = linear key (row N/COLS, col N%COLS).
    KeyMask scanMatrix();
};

#endif
