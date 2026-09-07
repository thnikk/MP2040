/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: Copyright (c) 2024 OpenStickCommunity (gp2040-ce.info)
 * Ported from GP2040-th to MP2040: no persistent Gamepad object (report
 * assembled from GamepadState each frame), no PS4ControllerIDMode (always
 * reports real PS4/Razer Panthera VID/PID, no DS4-emulation toggle), no
 * touchpad X/Y positioning (single digital touchpad button, always
 * centered -- MP2040 has no physical touchpad), no IMU (motion data stays
 * neutral) and no rumble/LED actuators (feature reports parsed and
 * discarded). Auth is always attempted via the USB host port (see
 * PS4Auth.h) -- there is no embedded-key mode.
 */

#ifndef _PS4_DRIVER_H_
#define _PS4_DRIVER_H_

#include "gpdriver.h"
#include "drivers/ps4/PS4Descriptors.h"
#include "drivers/ps4/PS4Auth.h"

// PS4 Auth buffer must be used by callbacks and process():
// Send back in 56 byte chunks:
//    256 byte - nonce signature
//    16 byte  - ps4 serial
//    256 byte - RSA N
//    256 byte - RSA E
//    256 byte - ps4 signature
//    24 byte  - zero padding
class PS4Driver : public GPDriver {
public:
    PS4Driver(uint32_t type): controllerType(type) {}
    virtual void initialize();
    virtual void process();
    virtual uint16_t get_report(uint8_t report_id, hid_report_type_t report_type, uint8_t *buffer, uint16_t reqlen);
    virtual void set_report(uint8_t report_id, hid_report_type_t report_type, uint8_t const *buffer, uint16_t bufsize);
    virtual bool vendor_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const *request);
    virtual const uint16_t * get_descriptor_string_cb(uint8_t index, uint16_t langid);
    virtual const uint8_t * get_descriptor_device_cb();
    virtual const uint8_t * get_hid_descriptor_report_cb(uint8_t itf);
    virtual const uint8_t * get_descriptor_configuration_cb(uint8_t index);
    virtual const uint8_t * get_descriptor_device_qualifier_cb();
    virtual USBListener * get_usb_auth_listener();
    bool getAuthSent() { return authsent; }
private:
    uint8_t last_report[CFG_TUD_ENDPOINT0_SIZE] = { };
    uint8_t last_report_counter;
    uint16_t last_axis_counter;
    PS4Report ps4Report;
    TouchpadData touchpadData;
    PSSensorData sensorData;
    uint32_t last_report_timer;
    PS4Auth * ps4AuthDriver;
    PS4AuthData * ps4AuthData;
    uint8_t cur_nonce_chunk;
    uint8_t cur_nonce_id;
    uint32_t controllerType; // PS4_CONTROLLER or PS4_ARCADESTICK (PS5 mode)
    PS4FeatureOutputReport ps4Features;
    uint8_t lastFeatures[PS4_FEATURES_SIZE] = { };
    bool authsent;
};

#endif // _PS4_DRIVER_H_
