#include "drivers/keyboard/KeyboardDriver.h"
#include "storagemanager.h"
#include "drivers/shared/driverhelper.h"
#include "types.h"

void KeyboardDriver::initialize() {
	keyboardReport = {
		.modifier = 0,
		.keycode = { 0 },
		.multimedia = 0
	};

	class_driver = {
	#if CFG_TUSB_DEBUG >= 2
		.name = "KEYBOARD",
	#endif
		.init = hidd_init,
		.reset = hidd_reset,
		.open = hidd_open,
		.control_xfer_cb = hidd_control_xfer_cb,
		.xfer_cb = hidd_xfer_cb,
		.sof = NULL
	};
}

uint8_t KeyboardDriver::getMultimedia(uint8_t code) {
	switch (code) {
		case KEYBOARD_MULTIMEDIA_NEXT_TRACK : return 0x01;
		case KEYBOARD_MULTIMEDIA_PREV_TRACK : return 0x02;
		case KEYBOARD_MULTIMEDIA_STOP 	    : return 0x04;
		case KEYBOARD_MULTIMEDIA_PLAY_PAUSE : return 0x08;
		case KEYBOARD_MULTIMEDIA_MUTE 	    : return 0x10;
		case KEYBOARD_MULTIMEDIA_VOLUME_UP  : return 0x20;
		case KEYBOARD_MULTIMEDIA_VOLUME_DOWN: return 0x40;
	}
	return 0;
}

void KeyboardDriver::process() {
	const KeyMapping& keyMapping = Storage::getInstance().getKeyMapping();
	const Mask_t keyState = Storage::getInstance().keyState;
	releaseAllKeys();

	// Direct pin -> keycode mapping. Each pin with a nonzero keycode emits
	// its key (or modifier / multimedia key) while held.
	for (Pin_t pin = 0; pin < (Pin_t)NUM_BANK0_GPIOS; pin++) {
		if (!(keyState & (1 << pin))) continue;
		if (pin >= (Pin_t)keyMapping.keycodes_count) continue;

		uint8_t keycode = keyMapping.keycodes[pin];
		if (keycode == 0) continue;

		keyboardReport.modifier |= keyMapping.modifierMasks[pin];
		pressKey(keycode);
	}

	// Wake up TinyUSB device
	if (tud_suspended())
		tud_remote_wakeup();

	void *keyboard_report_payload;
	uint16_t keyboard_report_size;
	if ( keyboardReport.reportId == KEYBOARD_KEY_REPORT_ID ) {
		keyboard_report_payload = (void *)&keyboardReport.modifier;
		keyboard_report_size = sizeof(KeyboardReport::modifier) + sizeof(KeyboardReport::keycode);

	} else {
		keyboard_report_payload = (void *)&keyboardReport.multimedia;
		keyboard_report_size = sizeof(KeyboardReport::multimedia);
	}

	// If we had a keycode but now have a multimedia key OR report is different
	if (keyboard_report_size != last_report_size ||
			memcmp(last_report, &keyboardReport, last_report_size) != 0) {
		if (tud_hid_ready()) {
			if ( tud_hid_report(keyboardReport.reportId, keyboard_report_payload, keyboard_report_size) ) {
				memcpy(last_report, keyboard_report_payload, keyboard_report_size);
				last_report_size = keyboard_report_size;
			}
		}
	}
}

void KeyboardDriver::pressKey(uint8_t code) {
	if (code > HID_KEY_GUI_RIGHT) {
		keyboardReport.reportId = KEYBOARD_MULTIMEDIA_REPORT_ID;
		keyboardReport.multimedia = getMultimedia(code);
	} else {
		keyboardReport.reportId = KEYBOARD_KEY_REPORT_ID;
		keyboardReport.keycode[code / 8] |= 1 << (code % 8);
	}
}

void KeyboardDriver::releaseAllKeys(void) {
	keyboardReport.modifier = 0;
	for (uint8_t i = 0; i < (sizeof(keyboardReport.keycode) / sizeof(keyboardReport.keycode[0])); i++) {
		keyboardReport.keycode[i] = 0;
	}
	keyboardReport.multimedia = 0;
}

// tud_hid_get_report_cb
uint16_t KeyboardDriver::get_report(uint8_t report_id, hid_report_type_t report_type, uint8_t *buffer, uint16_t reqlen) {
	if ( report_id == KEYBOARD_KEY_REPORT_ID ) {
		memcpy(buffer, (void*) &keyboardReport.modifier, sizeof(KeyboardReport::modifier) + sizeof(KeyboardReport::keycode));
		return sizeof(KeyboardReport::modifier) + sizeof(KeyboardReport::keycode);
	} else {
		memcpy(buffer, (void*) &keyboardReport.multimedia, sizeof(KeyboardReport::multimedia));
		return sizeof(KeyboardReport::multimedia);
	}
}

void KeyboardDriver::set_report(uint8_t report_id, hid_report_type_t report_type, uint8_t const *buffer, uint16_t bufsize) {}

bool KeyboardDriver::vendor_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const *request) {
    return false;
}

const uint16_t * KeyboardDriver::get_descriptor_string_cb(uint8_t index, uint16_t langid) {
	const char *value = (const char *)keyboard_string_descriptors[index];
	return getStringDescriptor(value, index); // getStringDescriptor returns a static array
}

const uint8_t * KeyboardDriver::get_descriptor_device_cb() {
    return keyboard_device_descriptor;
}

const uint8_t * KeyboardDriver::get_hid_descriptor_report_cb(uint8_t itf) {
    return keyboard_report_descriptor;
}

const uint8_t * KeyboardDriver::get_descriptor_configuration_cb(uint8_t index) {
    return keyboard_configuration_descriptor;
}

const uint8_t * KeyboardDriver::get_descriptor_device_qualifier_cb() {
	return nullptr;
}
