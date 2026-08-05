#ifndef MP2040_H_
#define MP2040_H_

#include "types.h"
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
    Mask_t buttonGpios;
    Mask_t touchGpios;
    Mask_t debouncedGpio;
    uint32_t gpioDebounceTime[NUM_BANK0_GPIOS];

    // Boot web-config window for touch boards. While nonzero, run() waits up to
    // WEB_CONFIG_TOUCH_WINDOW_MS before starting the keyboard so the web config
    // pad can be touched to enter web config. 0 = no window (button boards).
    uint32_t webconfigTouchDeadline = 0;

    enum class BootAction {
        NONE,
        ENTER_WEBCONFIG_MODE,
        ENTER_USB_MODE,
    };
    BootAction getBootAction();

    // GPIO manipulation for setup
    void initializeKeyGpio();
    void deinitializeKeyGpio();
};

#endif
