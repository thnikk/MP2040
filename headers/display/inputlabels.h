#ifndef _INPUTLABELS_H_
#define _INPUTLABELS_H_

#include <stdint.h>
#include <string>

#include "enums.pb.h"

// Short human-readable labels for a key's mapping, shared by the on-device
// color keypad (and reusable by other color displays). These mirror the web
// configurator's board-view labels:
//   keyboard -> key name with modifier prefix ("CL+A")
//   MIDI     -> note name ("C4")
//   gamepad  -> console-specific control name(s) ("A", "LB", "L1+R1")
//   macro    -> "M1".."M8"
// An unmapped key returns an empty string.

std::string keycodeToName(uint8_t code);
std::string modifierPrefix(uint8_t mask);
std::string midiNoteToName(uint8_t note);
std::string gamepadMaskToName(uint16_t mask, InputMode mode, bool nintendoLayout);

// Label for pin/key `pin` in `mode`. Reads the current key mapping from
// Storage. Empty when the key has no mapping for the mode.
std::string inputLabelForPin(uint32_t pin, InputMode mode, bool nintendoLayout);

#endif
