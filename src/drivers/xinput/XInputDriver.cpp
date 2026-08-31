/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: Copyright (c) 2024 OpenStickCommunity (gp2040-ce.info)
 *
 * Adapted for MP2040: no XInput auth (the Xbox 360 dongle passthrough was
 * non-functional upstream), digital triggers only, gamepad state from
 * Storage.keyState via the shared gamepad helper.
 */

#include "drivers/xinput/XInputDriver.h"
#include "drivers/shared/driverhelper.h"
#include "drivers/shared/gamepadhelper.h"
#include "touch/TouchRing.h"
#include "storagemanager.h"
#include "helper.h"
#include "types.h"

#include <cstring>

#define XINPUT_OUT_SIZE 32

#define XINPUT_DESC_TYPE_RESERVED 0x21
#define XINPUT_SECURITY_DESC_TYPE_RESERVED 0x41

static uint8_t endpoint_in = 0;
static uint8_t endpoint_out = 0;
static uint8_t xinput_out_buffer[XINPUT_OUT_SIZE] = {};

static void xinput_init(void) {}

static void xinput_reset(uint8_t rhport) {
    (void)rhport;
}

static uint16_t xinput_open(uint8_t rhport, tusb_desc_interface_t const *itf_descriptor, uint16_t max_length) {
    uint16_t driver_length = 0;
    // Xbox 360 Vendor USB Interfaces: Control, Audio, Plug-in, Security
    if ( TUSB_CLASS_VENDOR_SPECIFIC == itf_descriptor->bInterfaceClass) {
        driver_length = sizeof(tusb_desc_interface_t) + (itf_descriptor->bNumEndpoints * sizeof(tusb_desc_endpoint_t));
        TU_VERIFY(max_length >= driver_length, 0);

        tusb_desc_interface_t *p_desc = (tusb_desc_interface_t *)itf_descriptor;
        // Xbox 360 Interfaces (Control 0x01, Audio 0x02, Plug-in 0x03)
        if (itf_descriptor->bInterfaceSubClass == 0x5D &&
                ((itf_descriptor->bInterfaceProtocol == 0x01 ) ||
                (itf_descriptor->bInterfaceProtocol == 0x02 ) ||
                (itf_descriptor->bInterfaceProtocol == 0x03 )) ) {
            // Get Xbox 360 Definition
            p_desc = (tusb_desc_interface_t *)tu_desc_next(p_desc);
            TU_VERIFY(XINPUT_DESC_TYPE_RESERVED == p_desc->bDescriptorType, 0);
            driver_length += p_desc->bLength;
            p_desc = (tusb_desc_interface_t *)tu_desc_next(p_desc);
            // Control Endpoints are used for gamepad input/output
            if ( itf_descriptor->bInterfaceProtocol == 0x01 ) {
                TU_ASSERT(usbd_open_edpt_pair(rhport, (const uint8_t*)p_desc, itf_descriptor->bNumEndpoints,
                            TUSB_XFER_INTERRUPT, &endpoint_out, &endpoint_in), 0);
            }
        // Xbox 360 Security Interface
        } else if (itf_descriptor->bInterfaceSubClass == 0xFD &&
                itf_descriptor->bInterfaceProtocol == 0x13) {
            // Xinput reserved endpoint
            p_desc = (tusb_desc_interface_t *)tu_desc_next(p_desc);
            TU_VERIFY(XINPUT_SECURITY_DESC_TYPE_RESERVED == p_desc->bDescriptorType, 0);
            driver_length += p_desc->bLength;
        }
    }

    return driver_length;
}

static bool xinput_device_control_request(uint8_t rhport, uint8_t stage, tusb_control_request_t const *request) {
    (void)rhport;
    (void)stage;
    (void)request;
    return true;
}

static bool xinput_control_complete(uint8_t rhport, tusb_control_request_t const *request) {
    (void)rhport;
    (void)request;
    return true;
}

static bool xinput_xfer_callback(uint8_t rhport, uint8_t ep_addr, xfer_result_t result, uint32_t xferred_bytes) {
    (void)rhport;
    (void)result;
    (void)xferred_bytes;

    if (ep_addr == endpoint_out)
        usbd_edpt_xfer(0, endpoint_out, xinput_out_buffer, XINPUT_OUT_SIZE);

    return true;
}

void XInputDriver::initialize() {
    xinputReport = {
        .report_id = 0,
        .report_size = XINPUT_ENDPOINT_SIZE,
        .buttons1 = 0,
        .buttons2 = 0,
        .lt = 0,
        .rt = 0,
        .lx = 0,
        .ly = 0,
        .rx = 0,
        .ry = 0,
        ._reserved = { },
    };
    memset(&featureBuffer, 0, XINPUT_OUT_SIZE);

    class_driver = {
    #if CFG_TUSB_DEBUG >= 2
        .name = "XINPUT",
    #endif
        .init = xinput_init,
        .reset = xinput_reset,
        .open = xinput_open,
        .control_xfer_cb = xinput_device_control_request,
        .xfer_cb = xinput_xfer_callback,
        .sof = NULL
    };
}

void XInputDriver::process() {
    GamepadState gamepad;
    buildGamepadState(gamepad);
    gamepad.dpad = runSOCDCleaner(Storage::getInstance().getSocdMode(), gamepad.dpad);
    applyDpadMode(gamepad);

    xinputReport.buttons1 = 0
        | ((gamepad.dpad & GAMEPAD_MASK_UP)    ? XBOX_MASK_UP    : 0)
        | ((gamepad.dpad & GAMEPAD_MASK_DOWN)  ? XBOX_MASK_DOWN  : 0)
        | ((gamepad.dpad & GAMEPAD_MASK_LEFT)  ? XBOX_MASK_LEFT  : 0)
        | ((gamepad.dpad & GAMEPAD_MASK_RIGHT) ? XBOX_MASK_RIGHT : 0)
        | ((gamepad.buttons & GAMEPAD_MASK_S2) ? XBOX_MASK_START : 0)
        | ((gamepad.buttons & GAMEPAD_MASK_S1) ? XBOX_MASK_BACK  : 0)
        | ((gamepad.buttons & GAMEPAD_MASK_L3) ? XBOX_MASK_LS    : 0)
        | ((gamepad.buttons & GAMEPAD_MASK_R3) ? XBOX_MASK_RS    : 0);

    xinputReport.buttons2 = 0
        | ((gamepad.buttons & GAMEPAD_MASK_L1) ? XBOX_MASK_LB   : 0)
        | ((gamepad.buttons & GAMEPAD_MASK_R1) ? XBOX_MASK_RB   : 0)
        | ((gamepad.buttons & GAMEPAD_MASK_A1) ? XBOX_MASK_HOME : 0)
        | ((gamepad.buttons & GAMEPAD_MASK_B1) ? XBOX_MASK_A    : 0)
        | ((gamepad.buttons & GAMEPAD_MASK_B2) ? XBOX_MASK_B    : 0)
        | ((gamepad.buttons & GAMEPAD_MASK_B3) ? XBOX_MASK_X    : 0)
        | ((gamepad.buttons & GAMEPAD_MASK_B4) ? XBOX_MASK_Y    : 0);

    // Digital triggers: full on while held, off otherwise.
    xinputReport.lt = (gamepad.buttons & GAMEPAD_MASK_L2) ? 0xFF : 0;
    xinputReport.rt = (gamepad.buttons & GAMEPAD_MASK_R2) ? 0xFF : 0;

    // Analog sticks. A configured touch ring drives the selected stick (left
    // or right); otherwise both stay centered.
    if (TouchRing::getInstance().isConfigured()) {
        const RingState& ring = TouchRing::getInstance().getState();
        const bool rightStick = Storage::getInstance().getRingStickTarget() == 1;
        applyRingToStick(gamepad, ring.lx, ring.ly, ring.active, rightStick);
    }

    // XInput reports sticks as signed centering around 0, so the 16-bit
    // centered values are offset by INT16_MIN.
    xinputReport.lx = (int16_t)gamepad.lx + INT16_MIN;
    xinputReport.ly = (int16_t)gamepad.ly + INT16_MIN;
    xinputReport.rx = (int16_t)gamepad.rx + INT16_MIN;
    xinputReport.ry = (int16_t)gamepad.ry + INT16_MIN;

    // compare against previous report and send new
    if ( memcmp(last_report, &xinputReport, sizeof(XInputReport)) != 0) {
        if ( tud_ready() &&											// Is the device ready?
            (endpoint_in != 0) && (!usbd_edpt_busy(0, endpoint_in)) ) // Is the IN endpoint available?
        {
            usbd_edpt_claim(0, endpoint_in);								// Take control of IN endpoint
            usbd_edpt_xfer(0, endpoint_in, (uint8_t *)&xinputReport, sizeof(XInputReport)); // Send report buffer
            usbd_edpt_release(0, endpoint_in);								// Release control of IN endpoint
            memcpy(last_report, &xinputReport, sizeof(XInputReport)); // save if we sent it
        }
    }

    // clear potential initial uncaught data in endpoint_out from before registration of xfer_cb
    if (tud_ready() &&
        (endpoint_out != 0) && (!usbd_edpt_busy(0, endpoint_out)))
    {
        usbd_edpt_claim(0, endpoint_out);									 // Take control of OUT endpoint
        usbd_edpt_xfer(0, endpoint_out, xinput_out_buffer, XINPUT_OUT_SIZE); 		 // Retrieve report buffer
        usbd_edpt_release(0, endpoint_out);									 // Release control of OUT endpoint
    }

    //---------------
    if (memcmp(xinput_out_buffer, featureBuffer, XINPUT_OUT_SIZE) != 0) { // check if new write to xinput_out_buffer from xinput_xfer_callback
        memcpy(featureBuffer, xinput_out_buffer, XINPUT_OUT_SIZE);
        // The host's OUT reports (player LED, rumble) are consumed so the OUT
        // endpoint keeps draining; MP2040 doesn't surface them to anything yet.
    }
}

// tud_hid_get_report_cb
uint16_t XInputDriver::get_report(uint8_t report_id, hid_report_type_t report_type, uint8_t *buffer, uint16_t reqlen) {
    memcpy(buffer, &xinputReport, sizeof(XInputReport));
    return sizeof(XInputReport);
}

bool XInputDriver::vendor_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const *request) {
    (void)rhport;
    (void)stage;
    (void)request;
    return false;
}

const uint16_t * XInputDriver::get_descriptor_string_cb(uint8_t index, uint16_t langid) {
    return getStringDescriptor((const char *)xinput_get_string_descriptor(index), index);
}

const uint8_t * XInputDriver::get_descriptor_device_cb() {
    return xinput_device_descriptor;
}

const uint8_t * XInputDriver::get_hid_descriptor_report_cb(uint8_t itf) {
    return nullptr;
}

const uint8_t * XInputDriver::get_descriptor_configuration_cb(uint8_t index) {
    return xinput_configuration_descriptor;
}

const uint8_t * XInputDriver::get_descriptor_device_qualifier_cb() {
    return nullptr;
}