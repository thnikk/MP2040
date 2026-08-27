#ifndef MP2040CORE1_H_
#define MP2040CORE1_H_

#include "leds/LedController.h"
#include "display/DisplayController.h"

class MP2040Aux {
public:
	MP2040Aux();
    ~MP2040Aux();
    void setup();           // setup core1
    void run();             // loop core1
    bool ready(){ return isReady; }
private:
    LedController ledController;
    DisplayController displayController;
    bool isReady;
};

#endif
