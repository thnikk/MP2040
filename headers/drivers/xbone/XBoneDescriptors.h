/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: Copyright (c) 2024 OpenStickCommunity (gp2040-ce.info)
 */

#pragma once

#include <stdint.h>
#include <pico/unique_id.h>
#include <cstring>

#include "tusb.h"

#include "drivers/shared/xgip_protocol.h"

#define XBONE_ENDPOINT_SIZE 64

// 0x80 = std. device
// 

static const uint8_t xbone_string_language[]    = { 0x09, 0x04 };
static const uint8_t xbone_string_manufacturer[] = "thnikk";
static const uint8_t xbone_string_product[]      = "MP2040 (Xbox One)";
static const uint8_t xbone_string_version[]      = "1.0";

static const uint8_t *xbone_string_descriptors[] __attribute__((unused)) =
{
	xbone_string_language,
	xbone_string_manufacturer,
	xbone_string_product,
	xbone_string_version
};

static uint8_t uniqueSerial[] = "012345678ABCDEFGH";
static const uint8_t xboxSecurityMethod[] = "Xbox Security Method 3, Version 1.00, \xa9 2005 Microsoft Corporation. All rights reserved.";
static const uint8_t xboxOSDescriptor[] = "MSFT100\x20\x00";

static const uint8_t * xbone_get_string_descriptor(int index) {
	if ( index == 3 ) {
		// Generate a serial number from the pico's unique ID
		pico_unique_board_id_t id;
		pico_get_unique_board_id(&id);
        for(int i = 0; i < PICO_UNIQUE_BOARD_ID_SIZE_BYTES; i++) {
            uniqueSerial[i] = 'A' + (id.id[i]%25); // some alphanumeric from 'A' to 'Z'
        }
        return uniqueSerial;
	} else if ( index == 4 ) { // security method used
		return xboxSecurityMethod;
	} else if ( index == 0xEE ) { // ONLY WINDOWS DOES THIS??
		return xboxOSDescriptor;
	}

	return xbone_string_descriptors[index];
}

// MOVE THIS TO XBOX ONE DRIVER
typedef enum
{
    GIP_ACK_RESPONSE                = 0x01,    // Xbox One ACK
    GIP_ANNOUNCE                    = 0x02,    // Xbox One Announce
    GIP_KEEPALIVE                   = 0x03,    // Xbox One Keep-Alive
    GIP_DEVICE_DESCRIPTOR           = 0x04,    // Xbox One Definition
    GIP_POWER_MODE_DEVICE_CONFIG    = 0x05,    // Xbox One Power Mode Config
    GIP_AUTH                        = 0x06,    // Xbox One Authentication
    GIP_VIRTUAL_KEYCODE             = 0x07,    // XBox One Guide button pressed
    GIP_CMD_RUMBLE                  = 0x09,    // Xbox One Rumble Command
    GIP_CMD_LED_ON                  = 0x0A,    // Xbox One (LED On)
    GIP_FINAL_AUTH                  = 0x1E,    // Xbox One (Final auth?)
    GIP_INPUT_REPORT                = 0x20,    // Xbox One Input Report
    GIP_HID_REPORT                  = 0x21,    // Xbox One HID Report
} XboxOneReport;

typedef struct
{
    GipHeader_t Header;

    uint8_t sync : 1;
    uint8_t guide : 1;
    uint8_t start : 1;  // menu
    uint8_t back : 1;   // view

    uint8_t a : 1;
    uint8_t b : 1;
    uint8_t x : 1;
    uint8_t y : 1;

    uint8_t dpadUp : 1;
    uint8_t dpadDown : 1;
    uint8_t dpadLeft : 1;
    uint8_t dpadRight : 1;

    uint8_t leftShoulder : 1;
    uint8_t rightShoulder : 1;
    uint8_t leftThumbClick : 1;
    uint8_t rightThumbClick : 1;

    uint16_t leftTrigger;
    uint16_t rightTrigger;

    int16_t leftStickX;
    int16_t leftStickY;
    int16_t rightStickX;
    int16_t rightStickY;

    uint8_t share : 1;
    uint8_t : 7;

    uint8_t reserved2[17]; // 17-byte padding at the end
} __attribute__((packed)) XboxOneGamepad_Data_t;

typedef struct {
    GipHeader_t Header;
    uint8_t sync : 1;
    uint8_t guide : 1;
    uint8_t start : 1;  // menu
    uint8_t back : 1;   // view
} __attribute__((packed)) XboxOneInputHeader_Data_t;

typedef struct
{
    GipHeader_t Header;
    uint8_t unk;
    uint8_t mode;
    uint8_t brightness;
} __attribute__((packed)) XboxOneLED_Data_t;

static const uint8_t xbone_device_qualifier[] =
{
    0x0A,         // bLength
    0x06,         // bDescriptorType (Qualifier Type)
    0x00, 0x02,   // bcdUSB 2.00
    0xFF,         // bDeviceClass
    0xFF,         // bDeviceSubClass
    0xFF,         // bDeviceProtocol
    0x40,         // bMaxPacketSize0 64
    0x01,         // bNumConfigurations
    0x00          // bReserved
};

static const uint8_t xbone_device_descriptor[] =
{
    0x12,       // bLength
	0x01,       // bDescriptorType (Device)
	0x00, 0x02, // bcdUSB 2.00
	0xFF,	      // bDeviceClass
	0xFF,	      // bDeviceSubClass
	0xFF,	      // bDeviceProtocol
	0x40,	      // bMaxPacketSize0 64
	0x95, 0x2E, // idVendor 0x2E95 = SCUF
	0x04, 0x05, // idProduct 0x0504 = SCUF Gaming Controller
	0x01, 0x01, // bcdDevice 1.01?
	0x01,       // iManufacturer (String Index)
	0x02,       // iProduct (String Index)
	0x03,       // iSerialNumber (String Index)
	0x01,       // bNumConfigurations 1
};


static const uint8_t xbone_configuration_descriptor[] =
{
	0x09,        // bLength
	0x02,        // bDescriptorType (Configuration)
	0x20, 0x00,  // wTotalLength 32
	0x01,        // bNumInterfaces 1
	0x01,        // bConfigurationValue
	0x00,        // iConfiguration (String Index)
	0xA0,        // bmAttributes (USB_CONFIG_ATTRIBUTE_RESERVED | USB_CONFIG_ATTRIBUTE_REMOTEWAKEUP)
	0xFA,        // bMaxPower 500mA

	0x09,        // bLength
	0x04,        // bDescriptorType (Interface)
	0x00,        // bInterfaceNumber 0
	0x00,        // bAlternateSetting
	0x02,        // bNumEndpoints 2
	0xFF,        // bInterfaceClass
	0x47,        // bInterfaceSubClass
	0xD0,        // bInterfaceProtocol
	0x00,        // iInterface (String Index)

	0x07,        // bLength
	0x05,        // bDescriptorType (Endpoint)
	0x81,        // bEndpointAddress (IN/D2H)
	0x03,        // bmAttributes (Interrupt)
	0x40, 0x00,  // wMaxPacketSize 64
	0x01,        // bInterval 1 (unit depends on device speed)

	0x07,        // bLength
	0x05,        // bDescriptorType (Endpoint)
	0x02,        // bEndpointAddress (OUT/H2D)
	0x03,        // bmAttributes (Interrupt)
	0x40, 0x00,  // wMaxPacketSize 64
	0x01,        // bInterval 1 (unit depends on device speed)
};

// ---- Composite Xbox One + serial (CDC) variant -----------------------------
// Used when Config.serialConfigEnabled is set: the same GIP vendor interface
// plus a CDC-ACM serial port (tud_cdc_* API) for live control commands. Chosen
// at boot because the descriptor is fixed once the device enumerates. The GIP
// function still binds on Windows via the XGIP10 OS descriptor; the CDC
// function gets its own PDO (usbser.sys). The CDC occupies interface numbers
// 2 and 3 so that interface 1 does not exist: the xone Linux driver probes
// interface 1 as its audio port and aborts if it exists without an audio
// alternate setting.
static const uint8_t xbone_serial_device_descriptor[] =
{
	sizeof(tusb_desc_device_t),	// bLength
	TUSB_DESC_DEVICE,			// bDescriptorType
	0x00, 0x02,					// bcdUSB
	TUSB_CLASS_MISC,			// bDeviceClass (composite)
	MISC_SUBCLASS_COMMON,		// bDeviceSubClass
	MISC_PROTOCOL_IAD,			// bDeviceProtocol
	0x40,						// bMaxPacketSize0 64
	0x95, 0x2E,					// idVendor 0x2E95
	0x04, 0x05,					// idProduct 0x0504 (same as stock: a distinct
								// PID would let xpad-family drivers claim the GIP
								// interface instead of xone. Both variants serve
								// the XGIP10 OS descriptor, so Windows' cached
								// OS-descriptor verdict is unchanged.)
	0x01, 0x01,					// bcdDevice
	0x01,						// iManufacturer
	0x02,						// iProduct
	0x03,						// iSerialNumber
	0x01						// bNumConfigurations
};

enum
{
	ITF_NUM_XBONE,           // 0 (GIP; the xone Linux driver requires this)
	// Interface 1 is intentionally unused: the xone Linux GIP driver probes
	// interface number 1 as its audio port and fails the whole probe with
	// -ENXIO if that interface exists without an audio alternate setting.
	ITF_NUM_CDC_XBONE = 2,   // 2 (CDC comm)
	ITF_NUM_CDC_DATA_XBONE,  // 3 (CDC data)
	ITF_NUM_TOTAL_XBONE      // 4 = max interface number + 1 (not bNumInterfaces)
};

// Number of interfaces in the serial config (0, 2, 3). One fewer than
// ITF_NUM_TOTAL_XBONE because interface 1 is skipped.
#define XBONE_SERIAL_NUM_INTERFACES 3

#define  CONFIG_TOTAL_LEN_XBONE_SERIAL  (TUD_CONFIG_DESC_LEN + (9 + 7 + 7) + TUD_CDC_DESC_LEN)

// Serial endpoints. The GIP interface keeps its stock endpoints (0x81 / 0x02);
// the CDC pair gets its own. bInterval 16 on the notification endpoint (per
// CDC spec), 0 on the bulk data endpoints.
#define EPNUM_CDC_NOTIF_XBONE  0x82
#define EPNUM_CDC_OUT_XBONE    0x01
#define EPNUM_CDC_IN_XBONE     0x83

static const uint8_t xbone_serial_configuration_descriptor[] =
{
	// Config number, interface count, string index, total length, attribute, power in mA
	TUD_CONFIG_DESCRIPTOR(1, XBONE_SERIAL_NUM_INTERFACES, 0, CONFIG_TOTAL_LEN_XBONE_SERIAL, 0xA0, 500),

	// GIP vendor interface (Xbox One controller, subclass 0x47 / protocol 0xD0)
	0x09,        // bLength
	0x04,        // bDescriptorType (Interface)
	ITF_NUM_XBONE, // bInterfaceNumber 0
	0x00,        // bAlternateSetting
	0x02,        // bNumEndpoints 2
	0xFF,        // bInterfaceClass
	0x47,        // bInterfaceSubClass
	0xD0,        // bInterfaceProtocol
	0x00,        // iInterface (String Index)

	0x07,        // bLength
	0x05,        // bDescriptorType (Endpoint)
	0x81,        // bEndpointAddress (IN/D2H)
	0x03,        // bmAttributes (Interrupt)
	0x40, 0x00,  // wMaxPacketSize 64
	0x01,        // bInterval 1 (unit depends on device speed)

	0x07,        // bLength
	0x05,        // bDescriptorType (Endpoint)
	0x02,        // bEndpointAddress (OUT/H2D)
	0x03,        // bmAttributes (Interrupt)
	0x40, 0x00,  // wMaxPacketSize 64
	0x01,        // bInterval 1 (unit depends on device speed)

	// Interface number, string index, EP notification address & size, EP data (out, in) address & size
	TUD_CDC_DESCRIPTOR(ITF_NUM_CDC_XBONE, 0, EPNUM_CDC_NOTIF_XBONE, 8, EPNUM_CDC_OUT_XBONE, EPNUM_CDC_IN_XBONE, CFG_TUD_CDC_EP_BUFSIZE)
};

static uint8_t const * xbone_configuration_descriptor_cb(uint8_t index)
{
  return xbone_configuration_descriptor;
}