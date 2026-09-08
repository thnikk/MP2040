/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: Copyright (c) 2024 OpenStickCommunity (gp2040-ce.info)
 * Ported from GP2040-th to MP2040.
 */

#include "drivers/ps4/PS4Driver.h"
#include "drivers/shared/driverhelper.h"
#include "drivers/shared/gamepadhelper.h"
#include "touch/TouchRing.h"
#include "storagemanager.h"
#include "CRC32.h"
#include "class/hid/hid.h"

#include <cstring>

// force a report to be sent every X ms
#define PS4_KEEPALIVE_TIMER 5

// Controller calibration
static constexpr uint8_t output_0x02[] = {
    0xfe, 0xff, 0x0e, 0x00, 0x04, 0x00, 0xd4, 0x22,
    0x2a, 0xdd, 0xbb, 0x22, 0x5e, 0xdd, 0x81, 0x22,
    0x84, 0xdd, 0x1c, 0x02, 0x1c, 0x02, 0x85, 0x1f,
    0xb0, 0xe0, 0xc6, 0x20, 0xb5, 0xe0, 0xb1, 0x20,
    0x83, 0xdf, 0x0c, 0x00
};

// Controller descriptor (byte[4] = 0x00 for PS4_CONTROLLER, 0x07 for
// PS4_ARCADESTICK -- the latter is PS5 mode; see PS4Driver.h)
static constexpr uint8_t output_0x03[] = {
    0x21, 0x27, 0x04, 0xcf, 0x00, 0x2c, 0x56,
    0x08, 0x00, 0x3d, 0x00, 0xe8, 0x03, 0x04, 0x00,
    0xff, 0x7f, 0x0d, 0x0d, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

// Bluetooth device and host details
static constexpr uint8_t output_0x12[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // device MAC address
    0x08, 0x25, 0x00,                   // BT device class
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00  // host MAC address
};

// Controller firmware version and datestamp
static constexpr uint8_t output_0xa3[] = {
    0x4a, 0x75, 0x6e, 0x20, 0x20, 0x39, 0x20, 0x32,
    0x30, 0x31, 0x37, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x31, 0x32, 0x3a, 0x33, 0x36, 0x3a, 0x34, 0x31,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x01, 0x08, 0xb4, 0x01, 0x00, 0x00, 0x00,
    0x07, 0xa0, 0x10, 0x20, 0x00, 0xa0, 0x02, 0x00
};

// Nonce Page Size: 0x38 (56)
// Response Page Size: 0x38 (56)
static constexpr uint8_t output_0xf3[] = {
    0x0, 0x38, 0x38, 0, 0, 0, 0
};

void PS4Driver::initialize() {
    memset(&ps4Features, 0, sizeof(ps4Features));

    // No physical touchpad on MP2040 boards: always centered, unpressed.
    touchpadData.p1.unpressed = 1;
    touchpadData.p1.set_x(PS4_TP_X_MAX / 2);
    touchpadData.p1.set_y(PS4_TP_Y_MAX / 2);
    touchpadData.p2.unpressed = 1;
    touchpadData.p2.set_x(PS4_TP_X_MAX / 2);
    touchpadData.p2.set_y(PS4_TP_Y_MAX / 2);

    sensorData.powerLevel = 0xB; // 0x00-0x0A, 0x00-0x0B if charging
    sensorData.charging = 1;     // report as plugged in
    sensorData.headphones = 0;
    sensorData.microphone = 0;
    sensorData.extension = 0;
    // No IMU on MP2040 boards: motion data stays neutral.
    sensorData.gyroscope.x = 0;
    sensorData.gyroscope.y = 0;
    sensorData.gyroscope.z = 0;
    sensorData.accelerometer.x = 0;
    sensorData.accelerometer.y = 0;
    sensorData.accelerometer.z = 0;

    ps4Report = {
        .report_id = 0x01,
        .left_stick_x = PS4_JOYSTICK_MID,
        .left_stick_y = PS4_JOYSTICK_MID,
        .right_stick_x = PS4_JOYSTICK_MID,
        .right_stick_y = PS4_JOYSTICK_MID,
        .dpad = 0x08,
        .button_west = 0, .button_south = 0, .button_east = 0, .button_north = 0,
        .button_l1 = 0, .button_r1 = 0, .button_l2 = 0, .button_r2 = 0,
        .button_select = 0, .button_start = 0, .button_l3 = 0, .button_r3 = 0, .button_home = 0,
        .sensor_data = sensorData, .touchpad_active = 0, .padding = 0, .tpad_increment = 0,
        .touchpad_data = touchpadData,
        .mystery_2 = { }
    };

    class_driver = {
    #if CFG_TUSB_DEBUG >= 2
        .name = "PS4",
    #endif
        .init = hidd_init,
        .reset = hidd_reset,
        .open = hidd_open,
        .control_xfer_cb = hidd_control_xfer_cb,
        .xfer_cb = hidd_xfer_cb,
        .sof = NULL
    };

    last_report_counter = 0;
    last_axis_counter = 0;
    last_report_timer = to_ms_since_boot(get_absolute_time());
    cur_nonce_id = 1;
    cur_nonce_chunk = 0;

    // Auth is only ever attempted via the USB host port (see PS4Auth.h).
    // Boards without one leave ps4AuthDriver unusable and get_report() just
    // serves the unauthenticated feature reports below.
    ps4AuthDriver = new PS4Auth();
    ps4AuthData = nullptr;
    authsent = false;
    if (ps4AuthDriver->available()) {
        ps4AuthDriver->initialize();
        ps4AuthData = ps4AuthDriver->getAuthData();
    }
}

// Assemble a PS4 report from MP2040's per-frame GamepadState and send it to
// the TinyUSB device stack. Also drives the PS4Auth state machine each
// frame (trivial: it's just relaying reports through a USB host listener,
// no CPU-heavy signing, so no separate aux-core hook is needed).
void PS4Driver::process() {
    if (ps4AuthDriver != nullptr && ps4AuthDriver->available()) {
        ps4AuthDriver->process();
    }

    GamepadState gamepad;
    buildGamepadState(gamepad);
    gamepad.dpad = runSOCDCleaner(Storage::getInstance().getSocdMode(), gamepad.dpad);
    applyDpadMode(gamepad);

    if (TouchRing::getInstance().isConfigured()) {
        const RingState& ring = TouchRing::getInstance().getState();
        const bool rightStick = Storage::getInstance().getRingStickTarget() == 1;
        applyRingToStick(gamepad, ring.lx, ring.ly, ring.active, rightStick);
    }

    switch (gamepad.dpad & GAMEPAD_MASK_DPAD)
    {
        case GAMEPAD_MASK_UP:                        ps4Report.dpad = PS4_HAT_UP;        break;
        case GAMEPAD_MASK_UP | GAMEPAD_MASK_RIGHT:   ps4Report.dpad = PS4_HAT_UPRIGHT;   break;
        case GAMEPAD_MASK_RIGHT:                     ps4Report.dpad = PS4_HAT_RIGHT;     break;
        case GAMEPAD_MASK_DOWN | GAMEPAD_MASK_RIGHT: ps4Report.dpad = PS4_HAT_DOWNRIGHT; break;
        case GAMEPAD_MASK_DOWN:                      ps4Report.dpad = PS4_HAT_DOWN;      break;
        case GAMEPAD_MASK_DOWN | GAMEPAD_MASK_LEFT:  ps4Report.dpad = PS4_HAT_DOWNLEFT;  break;
        case GAMEPAD_MASK_LEFT:                      ps4Report.dpad = PS4_HAT_LEFT;      break;
        case GAMEPAD_MASK_UP | GAMEPAD_MASK_LEFT:    ps4Report.dpad = PS4_HAT_UPLEFT;    break;
        default:                                     ps4Report.dpad = PS4_HAT_NOTHING;   break;
    }

    ps4Report.button_south    = (gamepad.buttons & GAMEPAD_MASK_B1) ? 1 : 0;
    ps4Report.button_east     = (gamepad.buttons & GAMEPAD_MASK_B2) ? 1 : 0;
    ps4Report.button_west     = (gamepad.buttons & GAMEPAD_MASK_B3) ? 1 : 0;
    ps4Report.button_north    = (gamepad.buttons & GAMEPAD_MASK_B4) ? 1 : 0;
    ps4Report.button_l1       = (gamepad.buttons & GAMEPAD_MASK_L1) ? 1 : 0;
    ps4Report.button_r1       = (gamepad.buttons & GAMEPAD_MASK_R1) ? 1 : 0;
    ps4Report.button_l2       = (gamepad.buttons & GAMEPAD_MASK_L2) ? 1 : 0;
    ps4Report.button_r2       = (gamepad.buttons & GAMEPAD_MASK_R2) ? 1 : 0;
    ps4Report.button_select   = (gamepad.buttons & GAMEPAD_MASK_S1) ? 1 : 0;
    ps4Report.button_start    = (gamepad.buttons & GAMEPAD_MASK_S2) ? 1 : 0;
    ps4Report.button_l3       = (gamepad.buttons & GAMEPAD_MASK_L3) ? 1 : 0;
    ps4Report.button_r3       = (gamepad.buttons & GAMEPAD_MASK_R3) ? 1 : 0;
    ps4Report.button_home     = (gamepad.buttons & GAMEPAD_MASK_A1) ? 1 : 0;
    ps4Report.button_touchpad = (gamepad.buttons & GAMEPAD_MASK_A2) ? 1 : 0;

    // 16-bit sticks (center 0x7FFF) down-shift to PS4's 8-bit range.
    ps4Report.left_stick_x = static_cast<uint8_t>(gamepad.lx >> 8);
    ps4Report.left_stick_y = static_cast<uint8_t>(gamepad.ly >> 8);
    ps4Report.right_stick_x = static_cast<uint8_t>(gamepad.rx >> 8);
    ps4Report.right_stick_y = static_cast<uint8_t>(gamepad.ry >> 8);

    // No analog trigger sensing on MP2040 boards: fully on/off.
    ps4Report.left_trigger = (gamepad.buttons & GAMEPAD_MASK_L2) ? 0xFF : 0;
    ps4Report.right_trigger = (gamepad.buttons & GAMEPAD_MASK_R2) ? 0xFF : 0;

    // No physical touchpad on MP2040 boards: emulate a single centered
    // finger while the button is held, same as PS3's touchpad button.
    touchpadData.p1.unpressed = ps4Report.button_touchpad ? 0 : 1;
    ps4Report.touchpad_active = ps4Report.button_touchpad ? 0x01 : 0x00;
    static bool pointOneTouched = false;
    static uint8_t touchCounter = 0;
    if (!pointOneTouched && !touchpadData.p1.unpressed) {
        touchCounter = (touchCounter < PS4_TP_MAX_COUNT ? touchCounter + 1 : 0);
        touchpadData.p1.counter = touchCounter;
        pointOneTouched = true;
    } else if (pointOneTouched && touchpadData.p1.unpressed) {
        pointOneTouched = false;
    }
    ps4Report.touchpad_data = touchpadData;

    // Wake up TinyUSB device
    if (tud_suspended())
        tud_remote_wakeup();

    uint32_t now = to_ms_since_boot(get_absolute_time());
    void * report = &ps4Report;
    uint16_t report_size = sizeof(ps4Report);
    if (memcmp(last_report, report, report_size) != 0)
    {
        // HID ready + report sent, copy previous report
        if (tud_hid_ready() && tud_hid_report(0, report, report_size) == true ) {
            memcpy(last_report, report, report_size);
        }
        // keep track of our last successful report, for keepalive purposes
        last_report_timer = now;
    } else {
        // some games apparently can miss reports, or they rely on official behavior of getting frequent
        // updates. we normally only send a report when the value changes; if we increment the counters
        // every time we generate the report (every loop), we apparently overburden TinyUSB and introduce
        // roughly 1ms of latency. but we want to loop often and report on every true update in order to
        // achieve our tight <1ms report timing when we *do* have a different report to send.
        if ((now - last_report_timer) > PS4_KEEPALIVE_TIMER) {
            last_report_counter = (last_report_counter+1) & 0x3F;
            ps4Report.report_counter = last_report_counter;   // report counter is 6 bits
            ps4Report.axis_timing = now;                      // axis counter is 16 bits
            // the *next* process() will be a forced report (or real user input)
        }
    }

    // Rumble motor power / LED color (set via set_report, report_id 0) are
    // received into ps4Features but not acted on: MP2040 has no rumble
    // actuators or player-LED subsystem wired by default.
}

USBListener * PS4Driver::get_usb_auth_listener() {
    if (ps4AuthDriver != nullptr && ps4AuthDriver->available()) {
        return ps4AuthDriver->getListener();
    }
    return nullptr;
}

// tud_hid_get_report_cb
// NOTE: TinyUSB's hidd_control_xfer_cb already prepends report_id to its
// control buffer and shrinks reqlen by one before calling us. The static
// tables below are payload captures without an ID byte, so they are copied
// as-is (only the input struct carries its ID at [0], hence the +1 skip).
// Static reports (calibration/definition/MAC/version) are served even
// without auth data, like a real DS4; only the nonce/signature exchange
// needs the auth dongle.
uint16_t PS4Driver::get_report(uint8_t report_id, hid_report_type_t report_type, uint8_t *buffer, uint16_t reqlen) {
    if ( report_type != HID_REPORT_TYPE_FEATURE ) {
        if (report_id != 1) return 0;
        uint16_t len = sizeof(ps4Report) - 1;
        if (len > reqlen) len = reqlen;
        memcpy(buffer, ((const uint8_t*)&ps4Report) + 1, len);
        return len;
    }

    uint8_t data[64] = {};
    uint32_t crc32;
    uint16_t responseLen = 0;
    switch(report_id) {
        case PS4AuthReport::PS4_GET_CALIBRATION:
            responseLen = sizeof(output_0x02);
            if (responseLen > reqlen) responseLen = reqlen;
            memcpy(buffer, output_0x02, responseLen);
            return responseLen;
        case PS4AuthReport::PS4_DEFINITION:
            responseLen = sizeof(output_0x03);
            if (responseLen > reqlen) responseLen = reqlen;
            memcpy(buffer, output_0x03, responseLen);
            if (responseLen > 4) buffer[4] = (uint8_t)controllerType; // Change controller type in definition
            return responseLen;

        case PS4AuthReport::PS4_GET_MAC_ADDRESS:
            responseLen = sizeof(output_0x12);
            if (responseLen > reqlen) responseLen = reqlen;
            memcpy(buffer, output_0x12, responseLen);
            return responseLen;
        case PS4AuthReport::PS4_GET_VERSION_DATE:
            responseLen = sizeof(output_0xa3);
            if (responseLen > reqlen) responseLen = reqlen;
            memcpy(buffer, output_0xa3, responseLen);
            return responseLen;
        // Relay our dongle-signed nonce chunks back to the console
        case PS4AuthReport::PS4_GET_SIGNATURE_NONCE:
            if ( ps4AuthData == nullptr || ps4AuthDriver == nullptr) {
                return 0;
            }
            if (reqlen < 63) {
                return 0;
            }
            data[0] = 0xF1;
            data[1] = cur_nonce_id;    // nonce_id
            data[2] = cur_nonce_chunk; // next_part
            data[3] = 0;

            // 56 byte chunks
            memcpy(&data[4], &ps4AuthData->ps4_auth_buffer[cur_nonce_chunk*56], 56);

            // calculate the CRC32 of the buffer and write it back
            crc32 = CRC32::calculate(data, 60);
            memcpy(&data[60], &crc32, sizeof(uint32_t));
            memcpy(buffer, &data[1], 63); // move data over to buffer
            cur_nonce_chunk++;
            if ( cur_nonce_chunk == 19 ) { // done!
                ps4AuthData->passthrough_state = GPAuthState::auth_idle_state;
                authsent = true;
                cur_nonce_chunk = 0;
            }
            return 63;

        case PS4AuthReport::PS4_GET_SIGNING_STATE:  // Are we ready to sign?
            if ( ps4AuthData == nullptr || ps4AuthDriver == nullptr) {
                return 0;
            }
            if (reqlen < 15) {
                return 0;
            }
            data[0] = 0xF2;
            data[1] = cur_nonce_id;
            data[2] = (ps4AuthData->passthrough_state == GPAuthState::send_auth_dongle_to_console) ? 0 : 16; // 0 means auth is ready, 16 means we're still signing
            memset(&data[3], 0, 9);
            crc32 = CRC32::calculate(data, 12);
            memcpy(&data[12], &crc32, sizeof(uint32_t));
            memcpy(buffer, &data[1], 15); // move data over to buffer
            return 15;
        case PS4AuthReport::PS4_RESET_AUTH:         // Reset the Authentication
            responseLen = sizeof(output_0xf3);
            if (responseLen > reqlen) responseLen = reqlen;
            memcpy(buffer, output_0xf3, responseLen);
            if (ps4AuthData != nullptr) {
                ps4AuthData->passthrough_state = GPAuthState::auth_idle_state;
            }
            if (ps4AuthDriver != nullptr) {
                ps4AuthDriver->resetAuth();
            }
            return responseLen;
        default:
            break;
    };
    return 0;
}

// Only PS4 does anything with set report
void PS4Driver::set_report(uint8_t report_id, hid_report_type_t report_type, uint8_t const *buffer, uint16_t bufsize) {
    if (( report_type != HID_REPORT_TYPE_FEATURE ) && ( report_type != HID_REPORT_TYPE_OUTPUT ))
        return;

    if (report_type == HID_REPORT_TYPE_OUTPUT) {
        if (report_id == 0) {
            uint16_t len = bufsize;
            if (len > sizeof(ps4Features)) len = sizeof(ps4Features);
            memcpy(&ps4Features, buffer, len);
        }
    } else if (report_type == HID_REPORT_TYPE_FEATURE) {
        if (report_id == PS4AuthReport::PS4_SET_HOST_MAC) {
            //
        } else if (report_id == PS4AuthReport::PS4_SET_USB_BT_CONTROL) {
            //
        } else if (report_id == PS4AuthReport::PS4_SET_AUTH_PAYLOAD) {
            if (ps4AuthData == nullptr) {
                return;
            }
            uint8_t sendBuffer[64];
            uint8_t nonce_id;
            uint8_t nonce_page;
            uint16_t buflen;
            if (bufsize != 63 ) {
                return;
            }
            // Calculate CRC32 of buffer
            sendBuffer[0] = report_id;
            memcpy(&sendBuffer[1], buffer, bufsize);
            buflen = bufsize + 1;
            if ( CRC32::calculate(sendBuffer, buflen-sizeof(uint32_t)) !=
                    *((unsigned int*)&sendBuffer[buflen-sizeof(uint32_t)])) {
                return; // CRC32 failed on set report
            }
            // 256 byte nonce, 4 56-byte chunks, 1 32-byte chunk
            nonce_id = buffer[0];
            nonce_page = buffer[1];
            if ( nonce_page == 4 ) {    // Nonce page 4 : 32 bytes
                memcpy(&ps4AuthData->ps4_auth_buffer[nonce_page*56], &sendBuffer[4], 32);
                ps4AuthData->nonce_id = nonce_id;
                ps4AuthData->passthrough_state = GPAuthState::send_auth_console_to_dongle;
            } else {                    // Nonce page 0-3 : 56 bytes
                memcpy(&ps4AuthData->ps4_auth_buffer[nonce_page*56], &sendBuffer[4], 56);
            }
            if ( nonce_page == 0 ) { // Set our passthrough state on first nonce
                cur_nonce_id = nonce_id; // update current nonce ID
            } else if ( nonce_id != cur_nonce_id ) {
                // ERROR: Nonce ID is incorrect
                ps4AuthData->passthrough_state = GPAuthState::auth_idle_state;
            }
        }
    }
}

// Only XboxOG and Xbox One use vendor control xfer cb
bool PS4Driver::vendor_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const *request) {
    return false;
}

const uint16_t * PS4Driver::get_descriptor_string_cb(uint8_t index, uint16_t langid) {
    const char *value = (const char *)ps4_string_descriptors[index];
    return getStringDescriptor(value, index); // getStringDescriptor returns a static array
}

const uint8_t * PS4Driver::get_descriptor_device_cb() {
    return ps4_device_descriptor;
}

const uint8_t * PS4Driver::get_hid_descriptor_report_cb(uint8_t itf) {
    return ps4_report_descriptor;
}

const uint8_t * PS4Driver::get_descriptor_configuration_cb(uint8_t index) {
    return ps4_configuration_descriptor;
}

const uint8_t * PS4Driver::get_descriptor_device_qualifier_cb() {
    return nullptr;
}
