#include "drivers/midi/MidiDriver.h"
#include "storagemanager.h"
#include "drivers/shared/driverhelper.h"
#include "drivers/shared/serialhelper.h"
#include "touch/TouchRing.h"
#include "types.h"

#include "class/midi/midi_device.h"

void MidiDriver::initialize() {
	lastKeyState = 0;

	class_driver = {
	#if CFG_TUSB_DEBUG >= 2
		.name = "MIDI",
	#endif
		.init = midid_init,
		.reset = midid_reset,
		.open = midid_open,
		.control_xfer_cb = midid_control_xfer_cb,
		.xfer_cb = midid_xfer_cb,
		.sof = NULL
	};
}

void MidiDriver::process() {
	const KeyMapping& keyMapping = Storage::getInstance().getKeyMapping();
	const uint8_t globalVelocity = (uint8_t)Storage::getInstance().getMidiVelocity();
	const KeyMask& keyState = Storage::getInstance().keyState;

	// Serial command interface (opt-in via web config). Reads line-based
	// commands on the CDC port; see serialhelper.h for the shared handler.
	if (Storage::getInstance().getSerialConfigEnabled())
		serialCommands.process();

	// Only produce note events while a host has claimed the MIDI interfaces.
	// Keep the previous state in sync so a later mount doesn't send spurious
	// note-offs for keys already held down.
	if (!tud_midi_mounted()) {
		lastKeyState = keyState;
		return;
	}

	// Edge-triggered: send note-on on a press, note-off on a release. This is
	// unlike the keyboard driver's level-triggered bitmap report.
	for (Pin_t pin = 0; pin < (Pin_t)keyMapping.midiNotes_count; pin++) {
		if (pin >= (Pin_t)keyMapping.midiNotes_count) continue;

		uint8_t note = (uint8_t)keyMapping.midiNotes[pin];
		if (note == 0) continue;

		// Per-pin accent velocity overrides the global value.
		uint8_t velocity = globalVelocity;
		if (pin < (Pin_t)keyMapping.midiVelocities_count && keyMapping.midiVelocities[pin] != 0)
			velocity = (uint8_t)keyMapping.midiVelocities[pin];

		bool pressed = keyState.test(pin);
		bool wasPressed = lastKeyState.test(pin);
		if (pressed && !wasPressed)
			sendNote(MIDI_CIN_NOTE_ON, note, velocity);
		else if (!pressed && wasPressed)
			sendNote(MIDI_CIN_NOTE_OFF, note, velocity);
	}

	lastKeyState = keyState;

	// Touch ring: pitch bend. Center of the ring = center (0x2000), up = max
	// (0x3FFF), down = min (0). Only when enabled (ringMidiBehavior == 1).
	if (Storage::getInstance().getRingMidiBehavior() == 1 && TouchRing::getInstance().isConfigured()) {
		const RingState& ring = TouchRing::getInstance().getState();
		uint16_t bend = 0x2000;   // center default (no touch)
		if (ring.active) {
			// ly in -1..1 (top = +1); map to 14-bit bend: +1 => 0x3FFF,
			// -1 => 0. Clamp.
			float f = (ring.ly + 1.0f) / 2.0f;
			if (f < 0.0f) f = 0.0f;
			if (f > 1.0f) f = 1.0f;
			bend = (uint16_t)(f * 0x3FFF);
		}
		if (bend != lastPitchBend) {
			sendPitchBend(bend);
			lastPitchBend = bend;
		}
	}
}

void MidiDriver::sendNote(uint8_t cin, uint8_t note, uint8_t velocity) {
	uint8_t packet[4];
	packet[0] = (uint8_t)(0x00 | cin);                       // cable number 0, code index number
	packet[1] = (uint8_t)((cin == MIDI_CIN_NOTE_ON ? 0x90 : 0x80) |
	                      Storage::getInstance().getMidiChannel());
	packet[2] = note;
	packet[3] = velocity;
	tud_midi_packet_write(packet);
}

// Pitch bend: status 0xE0 | channel, 14-bit value split into LSB/MSB.
// Code index number for a 3-byte (or 2-data-byte) message is MIDI_CIN_PITCH_BEND.
void MidiDriver::sendPitchBend(uint16_t value) {
	uint8_t packet[4];
	packet[0] = (uint8_t)(0x00 | MIDI_CIN_PITCH_BEND_CHANGE);
	packet[1] = (uint8_t)(0xE0 | Storage::getInstance().getMidiChannel());
	packet[2] = (uint8_t)(value & 0x7F);          // LSB
	packet[3] = (uint8_t)((value >> 7) & 0x7F);   // MSB
	tud_midi_packet_write(packet);
}

// tud_hid_get_report_cb
uint16_t MidiDriver::get_report(uint8_t report_id, hid_report_type_t report_type, uint8_t *buffer, uint16_t reqlen) {
	return 0;
}

void MidiDriver::set_report(uint8_t report_id, hid_report_type_t report_type, uint8_t const *buffer, uint16_t bufsize) {}

bool MidiDriver::vendor_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const *request) {
    return false;
}

const uint16_t * MidiDriver::get_descriptor_string_cb(uint8_t index, uint16_t langid) {
	const char *value = (const char *)midi_string_descriptors[index];
	return getStringDescriptor(value, index); // getStringDescriptor returns a static array
}

const uint8_t * MidiDriver::get_descriptor_device_cb() {
    return Storage::getInstance().getSerialConfigEnabled()
        ? midi_serial_device_descriptor
        : midi_device_descriptor;
}

const uint8_t * MidiDriver::get_hid_descriptor_report_cb(uint8_t itf) {
    return nullptr;
}

const uint8_t * MidiDriver::get_descriptor_configuration_cb(uint8_t index) {
    return Storage::getInstance().getSerialConfigEnabled()
        ? midi_serial_configuration_descriptor
        : midi_configuration_descriptor;
}

const uint8_t * MidiDriver::get_descriptor_device_qualifier_cb() {
	return nullptr;
}
