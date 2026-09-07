/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: Copyright (c) 2024 OpenStickCommunity (gp2040-ce.info)
 * Ported from GP2040-th to MP2040: no persistent Gamepad object, so the
 * report is assembled from GamepadState each frame (see PS3Driver.cpp).
 * No IMU (motion data reports fixed-center) or rumble actuators (motor
 * reports are parsed and discarded).
 */

#ifndef _PS3_DRIVER_H_
#define _PS3_DRIVER_H_

#include "gpdriver.h"
#include "drivers/ps3/PS3Descriptors.h"

class PS3Driver : public GPDriver {
public:
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
private:
    uint8_t last_report[CFG_TUD_ENDPOINT0_SIZE] = { };
    PS3Report ps3Report;
    PS3Features ps3Features;
    uint8_t lastFeatures[PS3_FEATURES_SIZE] = { };
    PS3BTInfo ps3BTInfo;

    // Identification byte from the H2D 0xEF feature report that must be
    // echoed back in multiple D2H reports for the controller to function.
    uint8_t efByte;
};

#endif // _PS3_DRIVER_H_
