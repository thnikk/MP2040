#pragma once

#include <stdint.h>
#include "tusb.h"

#define KEYBOARD_KEY_REPORT_ID 0x01
#define KEYBOARD_MULTIMEDIA_REPORT_ID 0x02
#define KEYBOARD_MOUSE_REPORT_ID 0x03

#define KEYBOARD_MULTIMEDIA_NEXT_TRACK  0XE8
#define KEYBOARD_MULTIMEDIA_PREV_TRACK  0XE9
#define KEYBOARD_MULTIMEDIA_STOP 	    0XF0
#define KEYBOARD_MULTIMEDIA_PLAY_PAUSE  0XF1
#define KEYBOARD_MULTIMEDIA_MUTE 	    0XF2
#define KEYBOARD_MULTIMEDIA_VOLUME_UP   0XF3
#define KEYBOARD_MULTIMEDIA_VOLUME_DOWN 0XF4

// Mouse button keycodes (custom, above the multimedia range). A key mapped to
// one of these clicks the corresponding mouse button while held, alongside any
// keyboard keys. Routed to the mouse report (0x03) by KeyboardDriver.
#define MOUSE_BUTTON_LEFT    0xF5
#define MOUSE_BUTTON_RIGHT   0xF6
#define MOUSE_BUTTON_MIDDLE  0xF7
#define MOUSE_BUTTON_BACK    0xF8
#define MOUSE_BUTTON_FORWARD 0xF9

// Mouse button bits (mirrors the tinyusb HID mouse button bitmap).
#define MOUSE_BUTTON_LEFT_BIT    0x01
#define MOUSE_BUTTON_RIGHT_BIT   0x02
#define MOUSE_BUTTON_MIDDLE_BIT  0x04
#define MOUSE_BUTTON_BACK_BIT    0x08
#define MOUSE_BUTTON_FORWARD_BIT 0x10

/// Standard HID Boot Protocol Keyboard Report.
typedef struct
{
	uint8_t reportId = KEYBOARD_KEY_REPORT_ID;
	uint8_t modifier;  /**< Keyboard modifier (KEYBOARD_MODIFIER_* masks). */
	uint8_t keycode[32]; /**< Key codes of the currently pressed keys. */
	uint8_t multimedia;
} KeyboardReport;

/// Mouse buttons + wheel report (sent alongside the keyboard report). The
/// wheel bytes carry the touch ring's scroll (vertical or horizontal) when
/// the ring is in keyboard scroll mode.
typedef struct
{
	uint8_t reportId = KEYBOARD_MOUSE_REPORT_ID;
	uint8_t buttons = 0;   /**< MOUSE_BUTTON_*_BIT mask of held mouse buttons. */
	int8_t wheelY = 0;     /**< Vertical wheel delta. */
	int8_t wheelX = 0;     /**< Horizontal wheel delta. */
} MouseReport;

static const uint8_t keyboard_string_language[]    = { 0x09, 0x04 };
static const uint8_t keyboard_string_manfacturer[] = "thnikk";
static const uint8_t keyboard_string_product[]     = "MP2040";
static const uint8_t keyboard_string_version[]     = "1.0";

static const uint8_t *keyboard_string_descriptors[] __attribute__((unused)) =
{
	keyboard_string_language,
	keyboard_string_manfacturer,
	keyboard_string_product,
	keyboard_string_version
};

static const uint8_t keyboard_device_descriptor[] =
{
	sizeof(tusb_desc_device_t),	// bLength
	TUSB_DESC_DEVICE,			// bDescriptorType
	0x10, 0x01,					// bcdUSB
	0x00,						// bDeviceClass
	0x00,						// bDeviceSubClass
	0x00,						// bDeviceProtocol
	64,							// bMaxPacketSize0
	0xfe, 0xca,					// idVendor
	0x01, 0x00,					// idProduct
	0x00, 0x01,					// bcdDevice
	0x01,						// iManufacturer
	0x02,						// iProduct
	0x00,						// iSerialNumber
	0x01						// bNumConfigurations
};

// Device descriptor when the serial (CDC) interface is enabled. Composite
// devices use the IAD (Interface Association Descriptor) device class, and the
// extra interfaces get their own PID so the OS doesn't cache the keyboard-only
// driver against the composite device.
static const uint8_t keyboard_serial_device_descriptor[] =
{
	sizeof(tusb_desc_device_t),	// bLength
	TUSB_DESC_DEVICE,			// bDescriptorType
	0x10, 0x01,					// bcdUSB
	TUSB_CLASS_MISC,			// bDeviceClass (composite)
	MISC_SUBCLASS_COMMON,		// bDeviceSubClass
	MISC_PROTOCOL_IAD,			// bDeviceProtocol
	64,							// bMaxPacketSize0
	0xfe, 0xca,					// idVendor
	0x01, 0x01,					// idProduct (keyboard + serial)
	0x00, 0x01,					// bcdDevice
	0x01,						// iManufacturer
	0x02,						// iProduct
	0x00,						// iSerialNumber
	0x01						// bNumConfigurations
};

enum
{
	ITF_NUM_HID_KEYBOARD,
	ITF_NUM_TOTAL_KEYBOARD
};

#define  CONFIG_TOTAL_LEN  (TUD_CONFIG_DESC_LEN + TUD_HID_DESC_LEN)

#define EPNUM_HID   0x81

static const uint8_t keyboard_report_descriptor[] =
	{
		0x05, 0x01, // Usage Page (Generic Desktop),
		0x09, 0x06, // Usage (Keyboard),
		0xA1, 0x01, 	// Collection (Application),

		// Report ID (1)
		0x85, KEYBOARD_KEY_REPORT_ID,
		// Modifier byte
		0x05, 0x07,			 // Usage Page (Key Codes),
		0x19, 0xE0,			 // Usage Minimum (224),
		0x29, 0xE7,			 // Usage Maximum (231),
		0x15, 0x00,			 // Logical Minimum (0),
		0x25, 0x01,			 // Logical Maximum (1),
		0x75, 0x01,			 // Report Size (1),
		0x95, 0x08,			 // Report Count (8),
		0x81, 0x02,			 // Input (Data, Variable, Absolute)
		// Keycodes
		0x05, 0x07,			 // Usage Page (Key Codes),
		0x19, 0x00,			 // Usage Minimum (0),
		0x2A, 0xFF, 0x00, 	 // Usage Maximum (255),
		0x15, 0x00,			 // Logical Minimum (0),
		0x25, 0x01,			 // Logical Maximum (1),
		0x75, 0x01,			 // Report Size (1),
		0x96, 0x00, 0x01,	 // Report Count (256),
		0x81, 0x02,			 // Input (Data, Variable, Absolute)
		0xC0,			// End Collection
		0x05, 0x0C, //Usage Page (Consumer Devices)
		0x09, 0x01, //Usage (Consumer Control)
		0xA1, 0x01, 	//Collection (Application)

		//Report ID (2)
		0x85, KEYBOARD_MULTIMEDIA_REPORT_ID,
		0x05, 0x0C,			 //Usage Page (Consumer Devices)
		0x15, 0x00,			 //Logical Minimum (0)
		0x25, 0x01,			 //Logical Maximum (1)
		0x75, 0x01,			 //Report Size (1)
		0x95, 0x07,			 //Report Count (7)
		0x09, 0xB5,			 //Usage (Scan Next Track)
		0x09, 0xB6,			 //Usage (Scan Previous Track)
		0x09, 0xB7,			 //Usage (Stop)
		0x09, 0xCD,			 //Usage (Play/Pause)
		0x09, 0xE2,			 //Usage (Mute)
		0x09, 0xE9,			 //Usage (Volume Increment)
		0x09, 0xEA,			 //Usage (Volume Decrement)
		0x81, 0x02,			 //Input (Data,Var,Abs,NWrp,Lin,Pref,NNul,Bit)
		0x95, 0x01,			 //Report Count (1)
		0x81, 0x01,			 //Input (Const,Ary,Abs)
		0xC0,			//End Collection

		//Report ID (3) — mouse buttons + wheel (wheel carries the ring scroll)
		0x85, KEYBOARD_MOUSE_REPORT_ID,
		0x05, 0x01,			 //Usage Page (Generic Desktop)
		0x09, 0x02,			 //Usage (Mouse)
		0xA1, 0x01,			 //Collection (Application)
		0x09, 0x01,			 //Usage (Pointer)
		0xA1, 0x00,			 //Collection (Physical)
		0x05, 0x09,			 //Usage Page (Button)
		0x19, 0x01,			 //Usage Minimum (1)
		0x29, 0x05,			 //Usage Maximum (5)
		0x15, 0x00,			 //Logical Minimum (0)
		0x25, 0x01,			 //Logical Maximum (1)
		0x75, 0x01,			 //Report Size (1)
		0x95, 0x05,			 //Report Count (5)
		0x81, 0x02,			 //Input (Data, Var, Abs)
		0x95, 0x01,			 //Report Count (1)
		0x75, 0x03,			 //Report Size (3)
		0x81, 0x01,			 //Input (Const)
		0x05, 0x01,			 //Usage Page (Generic Desktop)
		0x09, 0x38,			 //Usage (Wheel)
		0x15, 0x81,			 //Logical Minimum (-127)
		0x25, 0x7F,			 //Logical Maximum (127)
		0x75, 0x08,			 //Report Size (8)
		0x95, 0x01,			 //Report Count (1)
		0x81, 0x06,			 //Input (Data, Var, Relative)
		0x09, 0x30,			 //Usage (X, horizontal wheel)
		0x81, 0x06,			 //Input (Data, Var, Relative)
		0xC0,			//End Collection (Physical)
		0xC0,			//End Collection (Application)
};

// Interface number, string index, protocol, report descriptor len, EP In address, size & polling interval
static const uint8_t keyboard_hid_descriptor[] =
{
	0x09,								 	  // bLength
	0x21,								 	  // bDescriptorType (HID)
	0x11, 0x01,							 	  // bcdHID 1.11
	0x00,								 	  // bCountryCode
	0x01,									  // bNumDescriptors
	0x22,									  // bDescriptorType[0] (HID)
	sizeof(keyboard_report_descriptor), 0x00, // wDescriptorLength[0]
};

static const uint8_t keyboard_configuration_descriptor[] =
{
	// Config number, interface count, string index, total length, attribute, power in mA
	TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL_KEYBOARD, 0, CONFIG_TOTAL_LEN, 32, 100),

	// Interface number, string index, protocol, report descriptor len, EP Out & In address, size & polling interval
	TUD_HID_DESCRIPTOR(ITF_NUM_HID_KEYBOARD, 0, HID_ITF_PROTOCOL_KEYBOARD, sizeof(keyboard_report_descriptor), EPNUM_HID, CFG_TUD_HID_EP_BUFSIZE, 1)
};

// ---- Composite keyboard + serial (CDC) variant -----------------------------
// Used when Config.serialConfigEnabled is set: the same HID keyboard interface
// plus a CDC-ACM serial port (tud_cdc_* API) for live control commands. Chosen
// at boot because the descriptor is fixed once the device enumerates.
enum
{
	ITF_NUM_HID_KEYBOARD_SERIAL,
	ITF_NUM_CDC_SERIAL,
	ITF_NUM_CDC_DATA_SERIAL,
	ITF_NUM_TOTAL_SERIAL
};

#define  CONFIG_TOTAL_LEN_SERIAL  (TUD_CONFIG_DESC_LEN + TUD_HID_DESC_LEN + TUD_CDC_DESC_LEN)

// Serial endpoints. HID keeps EPNUM_HID (0x81); the CDC pair gets its own
// endpoints. bInterval 16 on the notification endpoint (per CDC spec), 0 on
// the bulk data endpoints.
#define EPNUM_CDC_NOTIF  0x82
#define EPNUM_CDC_OUT    0x01
#define EPNUM_CDC_IN     0x83

static const uint8_t keyboard_serial_configuration_descriptor[] =
{
	// Config number, interface count, string index, total length, attribute, power in mA
	TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL_SERIAL, 0, CONFIG_TOTAL_LEN_SERIAL, 32, 100),

	// Interface number, string index, protocol, report descriptor len, EP Out & In address, size & polling interval
	TUD_HID_DESCRIPTOR(ITF_NUM_HID_KEYBOARD_SERIAL, 0, HID_ITF_PROTOCOL_KEYBOARD, sizeof(keyboard_report_descriptor), EPNUM_HID, CFG_TUD_HID_EP_BUFSIZE, 1),

	// Interface number, string index, EP notification address & size, EP data (out, in) address & size
	TUD_CDC_DESCRIPTOR(ITF_NUM_CDC_SERIAL, 0, EPNUM_CDC_NOTIF, 8, EPNUM_CDC_OUT, EPNUM_CDC_IN, CFG_TUD_CDC_EP_BUFSIZE)
};
