/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: Copyright (c) 2024 OpenStickCommunity (gp2040-ce.info)
 * Ported from GP2040-th to MP2040.
 */

#ifndef _PS4AUTHUSBLISTENER_H_
#define _PS4AUTHUSBLISTENER_H_

#include "usblistener.h"
#include "drivers/ps4/PS4Descriptors.h"
#include "drivers/ps4/PS4Auth.h"

typedef enum {
    no_nonce = 0,
    receiving_nonce = 1,
    nonce_ready = 2,
    signed_nonce_ready = 3,
    sending_nonce = 4
} PS4State;

class PS4AuthUSBListener : public USBListener {
public:
    virtual void setup();
    virtual void mount(uint8_t dev_addr, uint8_t instance, uint8_t const* desc_report, uint16_t desc_len);
    virtual void unmount(uint8_t dev_addr);
    virtual void report_received(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len) {}
    virtual void report_sent(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len) {}
    virtual void set_report_complete(uint8_t dev_addr, uint8_t instance, uint8_t report_id, uint8_t report_type, uint16_t len);
    virtual void get_report_complete(uint8_t dev_addr, uint8_t instance, uint8_t report_id, uint8_t report_type, uint16_t len);
    void process();
    void setAuthData(PS4AuthData * authData) { ps4AuthData = authData; }
    void resetHostData();
private:
    bool host_get_report(uint8_t report_id, void* report, uint16_t len);
    bool host_set_report(uint8_t report_id, void* report, uint16_t len);
    uint8_t ps_dev_addr;
    uint8_t ps_instance;
    PS4AuthData * ps4AuthData;
    uint8_t nonce_page;
    uint8_t nonce_chunk;
    uint8_t report_buffer[PS4_ENDPOINT_SIZE];
    bool awaiting_cb;
    uint8_t noncelen;
    uint32_t crc32;
    PS4State dongle_state;
};

#endif // _PS4AUTHUSBLISTENER_H_
