#pragma once

#include <stdint.h>
#include "tusb.h"

static const uint8_t midi_string_language[]    = { 0x09, 0x04 };
static const uint8_t midi_string_manfacturer[] = "thnikk";
static const uint8_t midi_string_product[]     = "MP2040 MIDI";
static const uint8_t midi_string_version[]     = "1.0";

static const uint8_t *midi_string_descriptors[] __attribute__((unused)) =
{
	midi_string_language,
	midi_string_manfacturer,
	midi_string_product,
	midi_string_version
};

static const uint8_t midi_device_descriptor[] =
{
	sizeof(tusb_desc_device_t),	// bLength
	TUSB_DESC_DEVICE,			// bDescriptorType
	0x10, 0x01,					// bcdUSB
	0x00,						// bDeviceClass
	0x00,						// bDeviceSubClass
	0x00,						// bDeviceProtocol
	64,							// bMaxPacketSize0
	0xfe, 0xca,					// idVendor
	0x08, 0x40,					// idProduct (0x4008: MIDI in the Auto PID layout)
	0x00, 0x01,					// bcdDevice
	0x01,						// iManufacturer
	0x02,						// iProduct
	0x00,						// iSerialNumber
	0x01						// bNumConfigurations
};

// Device descriptor when the serial (CDC) interface is enabled. Composite
// devices use the IAD (Interface Association Descriptor) device class, and the
// extra interfaces get their own PID so the OS doesn't cache the MIDI-only
// driver against the composite device.
static const uint8_t midi_serial_device_descriptor[] =
{
	sizeof(tusb_desc_device_t),	// bLength
	TUSB_DESC_DEVICE,			// bDescriptorType
	0x10, 0x01,					// bcdUSB
	TUSB_CLASS_MISC,			// bDeviceClass (composite)
	MISC_SUBCLASS_COMMON,		// bDeviceSubClass
	MISC_PROTOCOL_IAD,			// bDeviceProtocol
	64,							// bMaxPacketSize0
	0xfe, 0xca,					// idVendor
	0x09, 0x40,					// idProduct (0x4009: MIDI + serial in the Auto PID layout)
	0x00, 0x01,					// bcdDevice
	0x01,						// iManufacturer
	0x02,						// iProduct
	0x00,						// iSerialNumber
	0x01						// bNumConfigurations
};

enum
{
	ITF_NUM_MIDI_AUDIO_CONTROL,
	ITF_NUM_MIDI_STREAMING,
	ITF_NUM_TOTAL_MIDI
};

#define MIDI_CONFIG_TOTAL_LEN  (TUD_CONFIG_DESC_LEN + TUD_MIDI_DESC_LEN)

#define EPNUM_MIDI_OUT  0x01
#define EPNUM_MIDI_IN   0x81

static const uint8_t midi_configuration_descriptor[] =
{
	// Config number, interface count, string index, total length, attribute, power in mA
	TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL_MIDI, 0, MIDI_CONFIG_TOTAL_LEN, 0x80, 100),

	// Interface number, string index, EP out, EP in, EP size
	TUD_MIDI_DESCRIPTOR(ITF_NUM_MIDI_AUDIO_CONTROL, 0, EPNUM_MIDI_OUT, EPNUM_MIDI_IN, CFG_TUD_MIDI_EP_BUFSIZE)
};

// ---- Composite MIDI + serial (CDC) variant ---------------------------------
// Used when Config.serialConfigEnabled is set: the same MIDI interfaces plus a
// CDC-ACM serial port (tud_cdc_* API) for live control commands. Chosen at boot
// because the descriptor is fixed once the device enumerates.
enum
{
	ITF_NUM_MIDI_AUDIO_CONTROL_SERIAL,
	ITF_NUM_MIDI_STREAMING_SERIAL,
	ITF_NUM_MIDI_CDC_SERIAL,
	ITF_NUM_MIDI_CDC_DATA_SERIAL,
	ITF_NUM_TOTAL_MIDI_SERIAL
};

#define  MIDI_CONFIG_TOTAL_LEN_SERIAL  (TUD_CONFIG_DESC_LEN + TUD_MIDI_DESC_LEN + TUD_CDC_DESC_LEN)

// Serial endpoints. MIDI keeps its own pair (0x01 out / 0x81 in); the CDC pair
// gets separate endpoints. bInterval 16 on the notification endpoint (per CDC
// spec), 0 on the bulk data endpoints.
#define EPNUM_MIDI_CDC_NOTIF  0x82
#define EPNUM_MIDI_CDC_OUT    0x02
#define EPNUM_MIDI_CDC_IN     0x83

static const uint8_t midi_serial_configuration_descriptor[] =
{
	// Config number, interface count, string index, total length, attribute, power in mA
	TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL_MIDI_SERIAL, 0, MIDI_CONFIG_TOTAL_LEN_SERIAL, 0x80, 100),

	// Interface number, string index, EP out, EP in, EP size
	TUD_MIDI_DESCRIPTOR(ITF_NUM_MIDI_AUDIO_CONTROL_SERIAL, 0, EPNUM_MIDI_OUT, EPNUM_MIDI_IN, CFG_TUD_MIDI_EP_BUFSIZE),

	// Interface number, string index, EP notification address & size, EP data (out, in) address & size
	TUD_CDC_DESCRIPTOR(ITF_NUM_MIDI_CDC_SERIAL, 0, EPNUM_MIDI_CDC_NOTIF, 8, EPNUM_MIDI_CDC_OUT, EPNUM_MIDI_CDC_IN, CFG_TUD_CDC_EP_BUFSIZE)
};
