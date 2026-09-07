#ifndef _GPAUTHDRIVER_H_
#define _GPAUTHDRIVER_H_

#include "usblistener.h"

typedef enum {
    auth_idle_state = 0,
    send_auth_console_to_dongle = 1,
    send_auth_dongle_to_console = 2,
    wait_auth_console_to_dongle = 3,
    wait_auth_dongle_to_console = 4,
} GPAuthState;

// Base for USB-host-port auth-dongle passthrough drivers (currently just
// PS4Auth, which also covers PS5 mode). MP2040 only supports relaying a
// console's auth challenge to a real dongle over the host port -- no
// embedded signing keys -- so there's no InputModeAuthType selection here,
// unlike GP2040-th.
class GPAuthDriver {
public:
    virtual void initialize() = 0;
    virtual bool available() = 0;
    virtual USBListener * getListener() { return listener; }
protected:
    USBListener * listener;
};

#endif
