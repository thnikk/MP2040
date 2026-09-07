#ifndef _USBHOSTMANAGER_H_
#define _USBHOSTMANAGER_H_

#include <vector>

#include "usblistener.h"
#include "pio_usb.h"
#include "host/usbh.h"
#include "host/usbh_pvt.h"

// Owns the PIO-USB host controller (RHPort 1) and dispatches TinyUSB's HID
// host callbacks to any registered USBListener. Ported from GP2040-th, but
// trimmed to HID-only: MP2040 only needs the host port for a PS4/PS5 auth
// dongle, not the XInput host passthrough addon, so no custom TinyUSB host
// class driver is registered here.
//
// Configured directly from the board's USB_HOST_PIN_* macros (see
// BoardConfig.h), matching MP2040's existing no-abstraction-layer style.
// A board that doesn't define USB_HOST_PIN_DP simply never starts the host
// controller; start() becomes a no-op.
class USBHostManager {
public:
    USBHostManager(USBHostManager const&) = delete;
    void operator=(USBHostManager const&) = delete;

    static USBHostManager& getInstance() {
        static USBHostManager instance;
        return instance;
    }

    void start();    // Bring up the PIO-USB host controller, if configured
    void shutdown(); // Tear it down (e.g. before a reboot)
    void process();  // Pump tuh_task(); call every loop iteration on Core0

    void pushListener(USBListener* listener);

    void hid_mount_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* desc_report, uint16_t desc_len);
    void hid_umount_cb(uint8_t dev_addr, uint8_t instance);
    void hid_report_received_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len);
    void hid_set_report_complete_cb(uint8_t dev_addr, uint8_t instance, uint8_t report_id, uint8_t report_type, uint16_t len);
    void hid_get_report_complete_cb(uint8_t dev_addr, uint8_t instance, uint8_t report_id, uint8_t report_type, uint16_t len);

private:
    USBHostManager() : tuhReady(false) {}

    std::vector<USBListener*> listeners;
    pio_usb_configuration_t pioConfig = PIO_USB_DEFAULT_CONFIG;
    bool tuhReady;
};

#endif
