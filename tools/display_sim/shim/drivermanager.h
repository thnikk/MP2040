//
// Host shim for "drivermanager.h" (MP2040 display simulator).
//
// The display only asks the driver manager for the current input mode (to
// label the status bar / pick a remap editor). The real header drags in the
// whole GPDriver hierarchy, so this shim exposes just that, plus a setter the
// simulator uses to pretend the board is in a given input mode.
//
#ifndef _SIM_DRIVERMANAGER_H
#define _SIM_DRIVERMANAGER_H

#include "enums.pb.h"

class DriverManager {
public:
    DriverManager(DriverManager const&) = delete;
    void operator=(DriverManager const&) = delete;
    static DriverManager& getInstance() {
        static DriverManager instance;
        return instance;
    }
    InputMode getInputMode() { return inputMode; }
    void setInputMode(InputMode mode) { inputMode = mode; }
private:
    DriverManager() {}
    InputMode inputMode = INPUT_MODE_KEYBOARD;
};

#endif
