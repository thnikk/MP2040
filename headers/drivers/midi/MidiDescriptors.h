#pragma once

#include <stdint.h>
#include "tusb.h"

// MIDI channel used for all note messages (0-15). Hardcoded for now; a
// configurable channel can be added later.
#define MIDI_CHANNEL 0

#define MIDI_VELOCITY 127

static const uint8_t midi_string_language[]    = { 0x09, 0x04 };
static const uint8_t midi_string_manfacturer[] = "Open Stick Community";
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
