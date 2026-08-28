#include "drivers/keyboard/KeyboardDriver.h"
#include "storagemanager.h"
#include "drivers/shared/driverhelper.h"
#include "drivers/shared/serialhelper.h"
#include "touch/TouchRing.h"
#include "helper.h"
#include "types.h"

// Hard upper bounds for macro step timing (ms). The web config clamps to the
// same range; these keep a corrupt stored config from wedging playback.
#define MACRO_HOLD_MIN_MS 1
#define MACRO_HOLD_MAX_MS 5000
#define MACRO_DELAY_MAX_MS 5000

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

	for (uint8_t i = 0; i < MAX_ACTIVE_MACROS; i++)
		activeMacros[i].macroIndex = 0;
	lastKeyState.clear();
	prevHotkeyMacro = 0;
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
	const Config& config = Storage::getInstance().getConfig();
	const KeyMapping& keyMapping = config.keyMapping;
	const KeyMask& keyState = Storage::getInstance().keyState;
	const KeyMask& hotkeySuppressed = Storage::getInstance().hotkeySuppressed;
	releaseAllKeys();

	const uint32_t now = getMillis();

	// Advance any running macros and apply their held steps to the report.
	updateMacros(config, keyState, now);

	// Direct pin -> keycode mapping. Each pressed pin emits its key (or
	// modifier / multimedia key) while held. A pin with no keycode but a
	// modifier mask still acts as a pure modifier (e.g. a Shift key). Pins
	// mapped to a macro (macroIndices > 0) are handled by updateMacros. Pins
	// that are the trigger keys of a fired hotkey are skipped so the combo
	// doesn't also type its normal keys.
	for (Pin_t pin = 0; pin < (Pin_t)keyMapping.keycodes_count; pin++) {
		if (pin < (Pin_t)MAX_KEYS && config.macroIndices[pin] != 0) continue;
		if (hotkeySuppressed.test(pin)) continue;
		if (!keyState.test(pin)) continue;

		uint8_t keycode = keyMapping.keycodes[pin];
		applyKey(keycode, keyMapping.modifierMasks[pin]);
	}

	// Wake up TinyUSB device
	if (tud_suspended())
		tud_remote_wakeup();

	// Touch ring: volume or scroll, depending on config. Repurposes the
	// multimedia keys (volume) or the mouse wheel (scroll) from ring rotation.
	processRing(now);

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

	// Mouse buttons + wheel ride on their own report (0x03), sent
	// independently of the keyboard report so keys, mouse buttons and the
	// ring's scroll can work at the same time. The wheel bytes are relative, so
	// they're accumulated into the report each frame.
	mouseReport.wheelY = ringWheelY;
	mouseReport.wheelX = ringWheelX;
	constexpr uint8_t MOUSE_PAYLOAD_SIZE = sizeof(mouseReport.buttons) + sizeof(mouseReport.wheelY) + sizeof(mouseReport.wheelX);
	if (memcmp(&mouseReport.buttons, lastMousePayload, MOUSE_PAYLOAD_SIZE) != 0) {
		if (tud_hid_ready()) {
			if (tud_hid_report(mouseReport.reportId, (uint8_t*)&mouseReport.buttons, MOUSE_PAYLOAD_SIZE)) {
				memcpy(lastMousePayload, &mouseReport.buttons, MOUSE_PAYLOAD_SIZE);
			}
		}
	}
	// Consume the accumulated wheel deltas once reported.
	ringWheelY = 0;
	ringWheelX = 0;

	// Serial command interface (opt-in via web config). Reads line-based
	// commands on the CDC port; see serialhelper.h for the shared handler.
	if (Storage::getInstance().getSerialConfigEnabled())
		serialCommands.process();
}

// Keyboard-mode touch ring consumer. Two modes (Config.ringKeyboardMode):
//   VOLUME (2): ring rotation emits volume up/down multimedia keys. Each 45°
//               of net angular motion in one direction = one step.
//   SCROLL (1): ring rotation emits mouse wheel deltas (vertical or horizontal
//               per ringScrollAxis).
void KeyboardDriver::processRing(const uint32_t now) {
	(void)now;
	const RingState& ring = TouchRing::getInstance().getState();
	const uint32_t kbMode = Storage::getInstance().getRingKeyboardMode();

	// Volume: accumulate angular motion into steps (one per 45°). Latch a
	// volume key into the multimedia report for this frame if there were any.
	// Only when no ordinary key is held (a key and a ring slide at the same
	// time is unusual; keys take priority over momentary volume steps).
	if (kbMode == 2) {
		if (ring.active)
			ringVolumeSteps += (int)(ring.deltaDegrees / 45.0f);
		bool keyHeld = false;
		for (uint8_t i = 0; i < (sizeof(keyboardReport.keycode) / sizeof(keyboardReport.keycode[0])); i++) {
			if (keyboardReport.keycode[i] != 0) { keyHeld = true; break; }
		}
		if (!keyHeld && ringVolumeSteps > 0) {
			keyboardReport.reportId = KEYBOARD_MULTIMEDIA_REPORT_ID;
			keyboardReport.multimedia = getMultimedia(KEYBOARD_MULTIMEDIA_VOLUME_UP);
			ringVolumeSteps--;
		} else if (!keyHeld && ringVolumeSteps < 0) {
			keyboardReport.reportId = KEYBOARD_MULTIMEDIA_REPORT_ID;
			keyboardReport.multimedia = getMultimedia(KEYBOARD_MULTIMEDIA_VOLUME_DOWN);
			ringVolumeSteps++;
		}
	}

	// Scroll: accumulate angular motion into wheel deltas. A gentle slide maps
	// to an octet-sized wheel tick; the accumulated deltas are sent in the
	// mouse report by process().
	if (kbMode == 1) {
		if (ring.active) {
			float delta = ring.deltaDegrees;
			if (delta > 90.0f) delta = 90.0f;
			if (delta < -90.0f) delta = -90.0f;
			int8_t tick = (int8_t)(delta * 0.5f);
			if (Storage::getInstance().getRingScrollAxis() == 1)
				ringWheelX += tick;
			else
				ringWheelY += tick;
		}
	}
}

// Apply a keycode with an explicit modifier mask to the reports. Shared by the
// direct pin mapping and macro playback. keycode 0 contributes only modifiers.
void KeyboardDriver::applyKey(uint8_t code, uint8_t modifiers) {
	keyboardReport.modifier |= modifiers;
	if (code == 0) return;

	if (code >= MOUSE_BUTTON_LEFT && code <= MOUSE_BUTTON_FORWARD) {
		// Mouse buttons (custom keycodes above the multimedia range): set the
		// matching button bit. The report is sent separately in process().
		mouseReport.buttons |= 1u << (code - MOUSE_BUTTON_LEFT);
	} else if (code > HID_KEY_GUI_RIGHT) {
		keyboardReport.reportId = KEYBOARD_MULTIMEDIA_REPORT_ID;
		keyboardReport.multimedia = getMultimedia(code);
	} else {
		keyboardReport.reportId = KEYBOARD_KEY_REPORT_ID;
		keyboardReport.keycode[code / 8] |= 1 << (code % 8);
	}
}

void KeyboardDriver::pressKey(uint8_t code) {
	applyKey(code, 0);
}

// Advance macro playback and apply the currently-held steps to the report.
// Edge-triggered on the macro pins: a rising edge starts (or restarts) a
// playback. A press plays the macro through once even if the button is
// released mid-way; only at a cycle boundary does release stop it, and if the
// button is still held then the macro starts another pass.
void KeyboardDriver::updateMacros(const Config& config, const KeyMask& keyState, uint32_t now) {
	const Macro* macros = config.macros;
	const pb_size_t macroCount = config.macros_count;
	const KeyMask& hotkeySuppressed = Storage::getInstance().hotkeySuppressed;

	// Hotkey-triggered macro: a fired hotkey with a macro action plays through
	// a virtual slot (pin = HOTKEY_MACRO_PIN, which never reads as held). It
	// starts/restarts on the rising edge of Storage.hotkeyMacroIndex and, like
	// a held pin, loops until the combo is released (then stops at a cycle
	// boundary because the virtual pin is never held).
	const uint8_t hotkeyMacro = Storage::getInstance().hotkeyMacroIndex;
	if (hotkeyMacro != 0 && hotkeyMacro <= macroCount && hotkeyMacro != prevHotkeyMacro)
	{
		for (uint8_t i = 0; i < MAX_ACTIVE_MACROS; i++)
		{
			if (activeMacros[i].macroIndex == 0)
			{
				activeMacros[i].pin = HOTKEY_MACRO_PIN;
				activeMacros[i].macroIndex = hotkeyMacro;
				activeMacros[i].step = 0;
				activeMacros[i].holding = true;
				activeMacros[i].started = false; // hold timer armed on the first frame
				activeMacros[i].until = now;
				break;
			}
		}
	}
	prevHotkeyMacro = hotkeyMacro;

	// Start (or restart) playback on the rising edge of each macro pin. A
	// re-press while a run is still finishing resets it back to step 0. Pins
	// suppressed by a fired hotkey don't start their own macro.
	for (Pin_t pin = 0; pin < (Pin_t)MAX_KEYS; pin++) {
		const uint32_t macroIndex = pin < (Pin_t)MAX_KEYS ? config.macroIndices[pin] : 0;
		if (macroIndex == 0 || macroIndex > macroCount) continue;
		if (hotkeySuppressed.test(pin)) continue;
		if (!keyState.test(pin) || lastKeyState.test(pin)) continue;

		MacroPlayback* slot = nullptr;
		for (uint8_t i = 0; i < MAX_ACTIVE_MACROS; i++) {
			if (activeMacros[i].macroIndex != 0 && activeMacros[i].pin == (uint8_t)pin)
			{
				slot = &activeMacros[i]; // restart an in-flight run
				break;
			}
		}
		if (slot == nullptr) {
			for (uint8_t i = 0; i < MAX_ACTIVE_MACROS; i++) {
				if (activeMacros[i].macroIndex == 0) { slot = &activeMacros[i]; break; }
			}
		}
		if (slot == nullptr) continue;

		slot->pin = (uint8_t)pin;
		slot->macroIndex = (uint8_t)macroIndex;
		slot->step = 0;
		slot->holding = true;
		slot->started = false; // hold timer armed on the first frame
		slot->until = now;
	}

	// Step each active playback's state machine and apply the held step.
	for (uint8_t i = 0; i < MAX_ACTIVE_MACROS; i++) {
		MacroPlayback& m = activeMacros[i];
		if (m.macroIndex == 0) continue;
		if (m.macroIndex > macroCount) { m.macroIndex = 0; continue; }

		const Macro& macro = macros[m.macroIndex - 1];
		const pb_size_t stepCount = macro.steps_count;
		if (stepCount == 0) { m.macroIndex = 0; continue; }

		// Clamp per-step timing so a corrupt stored config can't wedge playback.
		const auto holdOf = [&](pb_size_t s) -> uint32_t {
			uint32_t v = macro.steps[s].holdMs;
			return v < MACRO_HOLD_MIN_MS ? MACRO_HOLD_MIN_MS
				: (v > MACRO_HOLD_MAX_MS ? MACRO_HOLD_MAX_MS : v);
		};
		const auto delayOf = [&](pb_size_t s) -> uint32_t {
			uint32_t v = macro.steps[s].delayMs;
			return v > MACRO_DELAY_MAX_MS ? MACRO_DELAY_MAX_MS : v;
		};

		if (m.holding) {
			if (!m.started) {
				// Fresh press / step transition: arm the hold timer, then apply
				// the step immediately.
				m.started = true;
				m.until = now + holdOf(m.step);
			} else if (now >= m.until) {
				// Hold finished: release the key and wait out this step's delay.
				m.holding = false;
				m.until = now + delayOf(m.step);
			}
		} else if (now >= m.until) {
			// Delay finished: move to the next step (looping at the end). A
			// completed pass keeps going only if the button is still held;
			// otherwise the run stops at the cycle boundary. A hotkey-triggered
			// macro (virtual pin) stays alive while its hotkey is held and stops
			// at the boundary once released.
			m.step = (m.step + 1) % stepCount;
			if (m.step == 0)
			{
				const bool stillHeld = (m.pin == HOTKEY_MACRO_PIN)
					? (Storage::getInstance().hotkeyMacroIndex == m.macroIndex)
					: keyState.test(m.pin);
				if (!stillHeld)
				{
					m.macroIndex = 0;
					continue;
				}
			}
			m.holding = true;
			m.started = false;
			m.until = now;
		}

		if (m.holding) {
			const MacroStep& step = macro.steps[m.step];
			applyKey(step.keycode, step.modifiers);
		}
	}

	lastKeyState = keyState;
}

void KeyboardDriver::releaseAllKeys(void) {
	keyboardReport.modifier = 0;
	for (uint8_t i = 0; i < (sizeof(keyboardReport.keycode) / sizeof(keyboardReport.keycode[0])); i++) {
		keyboardReport.keycode[i] = 0;
	}
	keyboardReport.multimedia = 0;
	mouseReport.buttons = 0;
	mouseReport.wheelX = 0;
	mouseReport.wheelY = 0;
}

// tud_hid_get_report_cb
uint16_t KeyboardDriver::get_report(uint8_t report_id, hid_report_type_t report_type, uint8_t *buffer, uint16_t reqlen) {
	if ( report_id == KEYBOARD_KEY_REPORT_ID ) {
		memcpy(buffer, (void*) &keyboardReport.modifier, sizeof(KeyboardReport::modifier) + sizeof(KeyboardReport::keycode));
		return sizeof(KeyboardReport::modifier) + sizeof(KeyboardReport::keycode);
	} else if ( report_id == KEYBOARD_MOUSE_REPORT_ID ) {
		memcpy(buffer, (void*) &mouseReport.buttons, sizeof(mouseReport.buttons));
		return sizeof(mouseReport.buttons);
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
    return Storage::getInstance().getSerialConfigEnabled()
        ? keyboard_serial_device_descriptor
        : keyboard_device_descriptor;
}

const uint8_t * KeyboardDriver::get_hid_descriptor_report_cb(uint8_t itf) {
    return keyboard_report_descriptor;
}

const uint8_t * KeyboardDriver::get_descriptor_configuration_cb(uint8_t index) {
    return Storage::getInstance().getSerialConfigEnabled()
        ? keyboard_serial_configuration_descriptor
        : keyboard_configuration_descriptor;
}

const uint8_t * KeyboardDriver::get_descriptor_device_qualifier_cb() {
	return nullptr;
}
