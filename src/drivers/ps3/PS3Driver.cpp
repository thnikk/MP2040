/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: Copyright (c) 2024 OpenStickCommunity (gp2040-ce.info)
 * Ported from GP2040-th to MP2040.
 */

#include "drivers/ps3/PS3Driver.h"
#include "drivers/ps3/PS3Descriptors.h"
#include "drivers/shared/driverhelper.h"
#include "drivers/shared/gamepadhelper.h"
#include "touch/TouchRing.h"
#include "storagemanager.h"
#include "pico/rand.h"

#include <cstring>

void PS3Driver::initialize() {
    ps3Report = {
        .reportID = 1,
        .reserved = 0,
        .select_btn = 0, .l3_btn = 0, .r3_btn = 0, .start_btn = 0,
        .dpad_up = 0, .dpad_right = 0, .dpad_down = 0, .dpad_left = 0,
        .l2_btn = 0, .r2_btn = 0, .l1_btn = 0, .r1_btn = 0,
        .triangle_btn = 0, .circle_btn = 0, .cross_btn = 0, .square_btn = 0,
        .ps_btn = 0, .tp_btn = 0,
        .l_x_axis = PS3_JOYSTICK_MID,
        .l_y_axis = PS3_JOYSTICK_MID,
        .r_x_axis = PS3_JOYSTICK_MID,
        .r_y_axis = PS3_JOYSTICK_MID,
        .movePowerStatus = 0,
        .up_axis = 0x00, .right_axis = 0x00, .down_axis = 0x00, .left_axis = 0x00,
        .l2_axis = 0x00, .r2_axis = 0x00, .l1_axis = 0x00, .r1_axis = 0x00,
        .triangle_axis = 0x00, .circle_axis = 0x00, .cross_axis = 0x00, .square_axis = 0x00,
        .reserved2 = {0x00,0x00,0x00},
        .plugged = PS3PluggedInState::PS3_PLUGGED,
        .powerStatus = PS3PowerState::PS3_POWER_FULL,
        .rumbleStatus = PS3WiredState::PS3_WIRED_RUMBLE,
        .reserved3 = {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
        // No IMU on MP2040 boards: motion data reports a fixed center value.
        .accelerometer_x = PS3_CENTER_SIXAXIS, .accelerometer_y = PS3_CENTER_SIXAXIS, .accelerometer_z = PS3_CENTER_SIXAXIS,
        .gyroscope_z = PS3_CENTER_SIXAXIS,
        .reserved4 = PS3_CENTER_SIXAXIS
    };

    // Generate a plausible Bluetooth pairing address (unused over USB, but
    // some hosts read it during enumeration).
    ps3BTInfo = {
        .reserved = {0xFF,0xFF},
        .deviceAddress = { 0x00, 0x20, 0x40, 0xCE, 0x00, 0x00, 0x00 },
        .hostAddress = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }
    };

    for (uint8_t addr = 0; addr < 3; addr++) {
        ps3BTInfo.deviceAddress[4+addr] = (uint8_t)(get_rand_32() % 0xff);
    }

    for (uint8_t addr = 0; addr < 6; addr++) {
        ps3BTInfo.hostAddress[1+addr] = (uint8_t)(get_rand_32() % 0xff);
    }

    class_driver = {
    #if CFG_TUSB_DEBUG >= 2
        .name = "PS3",
    #endif
        .init = hidd_init,
        .reset = hidd_reset,
        .open = hidd_open,
        .control_xfer_cb = hidd_control_xfer_cb,
        .xfer_cb = hidd_xfer_cb,
        .sof = NULL
    };
}

// Assemble a PS3 report from MP2040's per-frame GamepadState and send it to
// the TinyUSB device stack.
void PS3Driver::process() {
    GamepadState gamepad;
    buildGamepadState(gamepad);
    gamepad.dpad = runSOCDCleaner(Storage::getInstance().getSocdMode(), gamepad.dpad);
    applyDpadMode(gamepad);

    // Analog sticks, driven by the touch ring if configured (same as
    // XInput/SwitchPro).
    if (TouchRing::getInstance().isConfigured()) {
        const RingState& ring = TouchRing::getInstance().getState();
        const bool rightStick = Storage::getInstance().getRingStickTarget() == 1;
        applyRingToStick(gamepad, ring.lx, ring.ly, ring.active, rightStick);
    }

    ps3Report.dpad_left    = (gamepad.dpad & GAMEPAD_MASK_LEFT)  ? 1 : 0;
    ps3Report.dpad_down    = (gamepad.dpad & GAMEPAD_MASK_DOWN)  ? 1 : 0;
    ps3Report.dpad_right   = (gamepad.dpad & GAMEPAD_MASK_RIGHT) ? 1 : 0;
    ps3Report.dpad_up      = (gamepad.dpad & GAMEPAD_MASK_UP)    ? 1 : 0;

    ps3Report.cross_btn    = (gamepad.buttons & GAMEPAD_MASK_B1) ? 1 : 0;
    ps3Report.circle_btn   = (gamepad.buttons & GAMEPAD_MASK_B2) ? 1 : 0;
    ps3Report.square_btn   = (gamepad.buttons & GAMEPAD_MASK_B3) ? 1 : 0;
    ps3Report.triangle_btn = (gamepad.buttons & GAMEPAD_MASK_B4) ? 1 : 0;
    ps3Report.l1_btn       = (gamepad.buttons & GAMEPAD_MASK_L1) ? 1 : 0;
    ps3Report.r1_btn       = (gamepad.buttons & GAMEPAD_MASK_R1) ? 1 : 0;
    ps3Report.l2_btn       = (gamepad.buttons & GAMEPAD_MASK_L2) ? 1 : 0;
    ps3Report.r2_btn       = (gamepad.buttons & GAMEPAD_MASK_R2) ? 1 : 0;
    ps3Report.select_btn   = (gamepad.buttons & GAMEPAD_MASK_S1) ? 1 : 0;
    ps3Report.start_btn    = (gamepad.buttons & GAMEPAD_MASK_S2) ? 1 : 0;
    ps3Report.l3_btn       = (gamepad.buttons & GAMEPAD_MASK_L3) ? 1 : 0;
    ps3Report.r3_btn       = (gamepad.buttons & GAMEPAD_MASK_R3) ? 1 : 0;
    ps3Report.ps_btn       = (gamepad.buttons & GAMEPAD_MASK_A1) ? 1 : 0;
    ps3Report.tp_btn       = (gamepad.buttons & GAMEPAD_MASK_A2) ? 1 : 0;

    // 16-bit sticks (center 0x7FFF) down-shift to PS3's 8-bit range.
    ps3Report.l_x_axis = static_cast<uint8_t>(gamepad.lx >> 8);
    ps3Report.l_y_axis = static_cast<uint8_t>(gamepad.ly >> 8);
    ps3Report.r_x_axis = static_cast<uint8_t>(gamepad.rx >> 8);
    ps3Report.r_y_axis = static_cast<uint8_t>(gamepad.ry >> 8);

    // No analog trigger sensing on MP2040 boards: fully on/off like the
    // digital buttons.
    ps3Report.l2_axis = (gamepad.buttons & GAMEPAD_MASK_L2) ? 0xFF : 0;
    ps3Report.r2_axis = (gamepad.buttons & GAMEPAD_MASK_R2) ? 0xFF : 0;

    ps3Report.triangle_axis = (gamepad.buttons & GAMEPAD_MASK_B4) ? 0xFF : 0;
    ps3Report.circle_axis   = (gamepad.buttons & GAMEPAD_MASK_B2) ? 0xFF : 0;
    ps3Report.cross_axis    = (gamepad.buttons & GAMEPAD_MASK_B1) ? 0xFF : 0;
    ps3Report.square_axis   = (gamepad.buttons & GAMEPAD_MASK_B3) ? 0xFF : 0;
    ps3Report.l1_axis       = (gamepad.buttons & GAMEPAD_MASK_L1) ? 0xFF : 0;
    ps3Report.r1_axis       = (gamepad.buttons & GAMEPAD_MASK_R1) ? 0xFF : 0;
    ps3Report.right_axis    = (gamepad.dpad & GAMEPAD_MASK_RIGHT) ? 0xFF : 0;
    ps3Report.left_axis     = (gamepad.dpad & GAMEPAD_MASK_LEFT)  ? 0xFF : 0;
    ps3Report.up_axis       = (gamepad.dpad & GAMEPAD_MASK_UP)    ? 0xFF : 0;
    ps3Report.down_axis     = (gamepad.dpad & GAMEPAD_MASK_DOWN)  ? 0xFF : 0;

    // No IMU on MP2040 boards: report a fixed center value (motionless).
    ps3Report.accelerometer_x = PS3_CENTER_SIXAXIS;
    ps3Report.accelerometer_y = PS3_CENTER_SIXAXIS;
    ps3Report.accelerometer_z = PS3_CENTER_SIXAXIS;
    ps3Report.gyroscope_z = PS3_CENTER_SIXAXIS;
    ps3Report.reserved4 = PS3_CENTER_SIXAXIS;

    // Wake up TinyUSB device
    if (tud_suspended())
        tud_remote_wakeup();

    void * report = &ps3Report;
    uint16_t report_size = sizeof(ps3Report);
    if (memcmp(last_report, report, report_size) != 0)
    {
        // HID ready + report sent, copy previous report
        if (tud_hid_ready() && tud_hid_report(0, report, report_size) == true ) {
            memcpy(last_report, report, report_size);
        }
    }

    // Rumble motor power / player LED (set via set_report, PS3_FEATURE_01)
    // are received and stored in ps3Features but not acted on: MP2040 has
    // no rumble actuators or player-LED subsystem wired by default.
}

// unknown
static constexpr uint8_t output_ps3_0x01[] = {
    0x01, 0x04, 0x00, 0x0b, 0x0c, 0x01, 0x02, 0x18, 
    0x18, 0x18, 0x18, 0x09, 0x0a, 0x10, 0x11, 0x12,
    0x13, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x02,
    0x02, 0x02, 0x02, 0x00, 0x00, 0x00, 0x04, 0x04,
    0x04, 0x04, 0x00, 0x00, 0x04, 0x00, 0x01, 0x02,
    0x07, 0x00, 0x17, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

// calibration data
static constexpr uint8_t output_ps3_0xef[] = {
    0xef, 0x04, 0x00, 0x0b, 0x03, 0x01, 0xa0, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x01, 0xff, 0x01, 0xff, 0x01, 0xff, 0x01, 0xff,
    0x01, 0xff, 0x01, 0xff, 0x01, 0xff, 0x01, 0xff,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x06,
};

// unknown
static constexpr uint8_t output_ps3_0xf5[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // host address - must match 0xf2
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

// unknown
static constexpr uint8_t output_ps3_0xf7[] = {
    0x02, 0x01, 0xf8, 0x02, 0xe2, 0x01, 0x05, 0xff,
    0x04, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

// unknown
static constexpr uint8_t output_ps3_0xf8[] = {
    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

// tud_hid_get_report_cb
uint16_t PS3Driver::get_report(uint8_t report_id, hid_report_type_t report_type, uint8_t *buffer, uint16_t reqlen) {
    if ( report_type == HID_REPORT_TYPE_INPUT ) {
        memcpy(buffer, &ps3Report, sizeof(PS3Report));
        return sizeof(PS3Report);
    } else if ( report_type == HID_REPORT_TYPE_FEATURE ) {
        uint16_t responseLen = 0;
        uint8_t ctr = 0;
        switch(report_id) {
            case PS3ReportTypes::PS3_FEATURE_01:
                responseLen = reqlen;
                memcpy(buffer, output_ps3_0x01, responseLen);
                return responseLen;
            case PS3ReportTypes::PS3_FEATURE_EF:
                responseLen = reqlen;
                memcpy(buffer, output_ps3_0xef, responseLen);
                buffer[6] = efByte;
                return responseLen;
            case PS3ReportTypes::PS3_GET_PAIRING_INFO:
                responseLen = reqlen;
                memcpy(buffer, &ps3BTInfo, responseLen);
                return responseLen;
            case PS3ReportTypes::PS3_FEATURE_F5:
                responseLen = reqlen;
                memcpy(buffer, output_ps3_0xf5, responseLen);
                for (ctr = 0; ctr < 6; ctr++) {
                    buffer[1+ctr] = ps3BTInfo.hostAddress[ctr];
                }
                return responseLen;
            case PS3ReportTypes::PS3_FEATURE_F7:
                responseLen = reqlen;
                memcpy(buffer, output_ps3_0xf7, responseLen);
                return responseLen;
            case PS3ReportTypes::PS3_FEATURE_F8:
                responseLen = reqlen;
                memcpy(buffer, output_ps3_0xf8, responseLen);
                buffer[6] = efByte;
                return responseLen;
        }
    }
    return -1;
}

void PS3Driver::set_report(uint8_t report_id, hid_report_type_t report_type, uint8_t const *buffer, uint16_t bufsize) {
    if ( report_type == HID_REPORT_TYPE_FEATURE ) {
        switch(report_id) {
            case PS3ReportTypes::PS3_FEATURE_EF:
                efByte = buffer[6];
                break;
        }
    } else if (report_type == HID_REPORT_TYPE_OUTPUT ) {
        // DS3 command (rumble motor power / player LED). Stored but not
        // acted on: MP2040 has no rumble actuators or player-LED subsystem
        // wired by default.
        uint8_t const *buf = buffer;
        if (report_id == 0 && bufsize > 0) {
            report_id = buffer[0];
            bufsize = bufsize - 1;
            buf = &buffer[1];
        }
        switch(report_id) {
            case PS3ReportTypes::PS3_FEATURE_01:
                memcpy(&ps3Features, buf, bufsize);
                break;
        }
    }
}

// Only XboxOG and Xbox One use vendor control xfer cb
bool PS3Driver::vendor_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const *request) {
    return false;
}

const uint16_t * PS3Driver::get_descriptor_string_cb(uint8_t index, uint16_t langid) {
	const char *value = (const char *)ps3_string_descriptors[index];
	return getStringDescriptor(value, index); // getStringDescriptor returns a static array
}

const uint8_t * PS3Driver::get_descriptor_device_cb() {
    return ps3_device_descriptor;
}

const uint8_t * PS3Driver::get_hid_descriptor_report_cb(uint8_t itf) {
    return ps3_report_descriptor;
}

const uint8_t * PS3Driver::get_descriptor_configuration_cb(uint8_t index) {
    return ps3_configuration_descriptor;
}

const uint8_t * PS3Driver::get_descriptor_device_qualifier_cb() {
	return nullptr;
}
