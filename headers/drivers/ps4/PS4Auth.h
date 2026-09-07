/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: Copyright (c) 2024 OpenStickCommunity (gp2040-ce.info)
 * Trimmed for MP2040: USB auth-dongle passthrough only, no embedded RSA
 * signing keys (no mbedtls dependency).
 */

#ifndef _PS4AUTH_H_
#define _PS4AUTH_H_

#include "drivers/shared/gpauthdriver.h"

typedef enum
{
    PS4_GET_CALIBRATION      = 0x02,    // PS4 Controller Calibration
    PS4_DEFINITION           = 0x03,    // PS4 Controller Definition
    PS4_SET_FEATURE_STATE    = 0x05,    // PS4 Controller Features
    PS4_GET_MAC_ADDRESS      = 0x12,    // PS4 Controller MAC
    PS4_SET_HOST_MAC         = 0x13,    // Set Host MAC
    PS4_SET_USB_BT_CONTROL   = 0x14,    // Set USB/BT Control Mode
    PS4_GET_VERSION_DATE     = 0xA3,    // PS4 Controller Version & Date
    PS4_SET_AUTH_PAYLOAD     = 0xF0,    // Set Auth Payload
    PS4_GET_SIGNATURE_NONCE  = 0xF1,    // Get Signature Nonce
    PS4_GET_SIGNING_STATE    = 0xF2,    // Get Signing State
    PS4_RESET_AUTH           = 0xF3     // Unknown (PS4 Report 0xF3)
} PS4AuthReport;


// PS4/PS5 auth-dongle passthrough state, relayed between the console and a
// real controller/dongle plugged into the USB host port.
typedef struct {
    uint8_t ps4_auth_buffer[1064];
    bool dongle_ready = false;
    GPAuthState passthrough_state;
    uint8_t nonce_id;
} PS4AuthData;

class PS4Auth : public GPAuthDriver {
public:
    virtual void initialize();
    virtual bool available();
    void process();
    PS4AuthData * getAuthData() { return &ps4AuthData; }
    void resetAuth();
private:
    PS4AuthData ps4AuthData;
};

#endif
