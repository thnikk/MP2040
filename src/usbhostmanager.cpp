#include "usbhostmanager.h"

#include "tusb.h"
#include "host/usbh.h"
#include "host/usbh_pvt.h"
#include "hardware/gpio.h"
#include "pico/time.h"

// Board-config USB host port pins (see BoardConfig.h). A board that leaves
// USB_HOST_PIN_DP undefined never starts the host controller: PS4/PS5 modes
// still work without auth, and the web config hides them entirely (see
// getUsbHostPortDefined() in storagemanager).
#ifndef USB_HOST_PIN_DP
#define USB_HOST_PIN_DP -1
#endif
#ifndef USB_HOST_PIN_5V
#define USB_HOST_PIN_5V -1
#endif
#ifndef USB_HOST_PIN_ORDER
#define USB_HOST_PIN_ORDER 0
#endif

void USBHostManager::start() {
    // Nothing to host: no port wired up, or nobody registered a listener
    // (e.g. no driver requested auth-dongle passthrough).
    if (USB_HOST_PIN_DP < 0 || listeners.size() == 0) {
        tuhReady = false;
        return;
    }

    if (USB_HOST_PIN_5V >= 0) { // Feather USB-A style boards require this
        gpio_init(USB_HOST_PIN_5V);
        gpio_set_dir(USB_HOST_PIN_5V, GPIO_IN);
        gpio_pull_up(USB_HOST_PIN_5V);
    }

    pioConfig.pin_dp = USB_HOST_PIN_DP;
    pioConfig.pinout = (USB_HOST_PIN_ORDER == 0) ? PIO_USB_PINOUT_DPDM : PIO_USB_PINOUT_DMDP;
    pioConfig.sm_tx = 1; // ws2812 (PIO0) uses SM0; stay clear of it

    tuh_configure(1, TUH_CFGID_RPI_PIO_USB_CONFIGURATION, &pioConfig);
    tuh_init(BOARD_TUH_RHPORT);
    sleep_us(10); // let the controller settle before use
    tuhReady = true;
}

void USBHostManager::shutdown() {
    if (tuhReady) {
        tuh_deinit(BOARD_TUH_RHPORT);
        tuhReady = false;
    }
}

void USBHostManager::pushListener(USBListener* listener) {
    listeners.push_back(listener);
}

void USBHostManager::process() {
    if (tuhReady) {
        tuh_task();
    }
}

void USBHostManager::hid_mount_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* desc_report, uint16_t desc_len) {
    for (auto listener : listeners)
        listener->mount(dev_addr, instance, desc_report, desc_len);
}

void USBHostManager::hid_umount_cb(uint8_t dev_addr, uint8_t instance) {
    (void)instance;
    for (auto listener : listeners)
        listener->unmount(dev_addr);
}

void USBHostManager::hid_report_received_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len) {
    for (auto listener : listeners)
        listener->report_received(dev_addr, instance, report, len);
}

void USBHostManager::hid_set_report_complete_cb(uint8_t dev_addr, uint8_t instance, uint8_t report_id, uint8_t report_type, uint16_t len) {
    for (auto listener : listeners)
        listener->set_report_complete(dev_addr, instance, report_id, report_type, len);
}

void USBHostManager::hid_get_report_complete_cb(uint8_t dev_addr, uint8_t instance, uint8_t report_id, uint8_t report_type, uint16_t len) {
    for (auto listener : listeners)
        listener->get_report_complete(dev_addr, instance, report_id, report_type, len);
}

// ---- TinyUSB HID host callbacks ----
// These are weak in TinyUSB; providing them here makes them strong.

extern "C" void tuh_hid_mount_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* desc_report, uint16_t desc_len) {
    USBHostManager::getInstance().hid_mount_cb(dev_addr, instance, desc_report, desc_len);
    tuh_hid_receive_report(dev_addr, instance);
}

extern "C" void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t instance) {
    USBHostManager::getInstance().hid_umount_cb(dev_addr, instance);
}

extern "C" void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len) {
    USBHostManager::getInstance().hid_report_received_cb(dev_addr, instance, report, len);
    tuh_hid_receive_report(dev_addr, instance);
}

extern "C" void tuh_hid_set_report_complete_cb(uint8_t dev_addr, uint8_t instance, uint8_t report_id, uint8_t report_type, uint16_t len) {
    if (len != 0)
        USBHostManager::getInstance().hid_set_report_complete_cb(dev_addr, instance, report_id, report_type, len);
}

extern "C" void tuh_hid_get_report_complete_cb(uint8_t dev_addr, uint8_t instance, uint8_t report_id, uint8_t report_type, uint16_t len) {
    if (len != 0)
        USBHostManager::getInstance().hid_get_report_complete_cb(dev_addr, instance, report_id, report_type, len);
}

// No custom TinyUSB host class drivers are registered (built-in HID host is
// all a HID auth dongle needs); this just satisfies the weak symbol TinyUSB
// calls to look for app-provided host class drivers.
extern "C" usbh_class_driver_t const* usbh_app_driver_get_cb(uint8_t* driver_count) {
    *driver_count = 0;
    return NULL;
}
