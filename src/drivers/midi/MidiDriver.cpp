#include "drivers/midi/MidiDriver.h"
#include "storagemanager.h"
#include "drivers/shared/driverhelper.h"
#include "drivers/shared/serialhelper.h"
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
