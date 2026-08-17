#ifndef _TOUCH_RING_H_
#define _TOUCH_RING_H_

#include <stdint.h>
#include "types.h"

//
// Touch ring input.
//
// A circular analog control made from 4 pie-slice capacitive pads arranged
// around a ring (interlocking tines). Each pad connects through the TouchGpio
// driver, which returns a raw discharge count proportional to how much of the
// pad a finger is over. This helper combines the four readings into a single
// contact point: an angle around the ring and a touch magnitude.
//
// Boards opt in by defining RING_GP00..RING_GP03 (the four pins, in physical
// order around the ring) plus the tunables below in BoardConfig.h. The ring is
// then interpreted per input mode by the active driver (analog stick in
// gamepad modes, volume/scroll in keyboard mode, pitch bend in MIDI mode).
//

// Result of a ring read. `active` means a touch was detected on the ring;
// `magnitude` (0..1) is how strongly, `angleDeg` (0..359) the position around
// the ring (0 = the RING_GP00 direction, increasing toward RING_GP01), and
// `deltaDegrees` the angular motion since the previous read (for rate-based
// consumers like volume / scroll). lx/ly are the cartesian stick vector
// derived from angle + magnitude, ready to feed a gamepad stick.
struct RingState
{
	bool active;
	float magnitude;
	float angleDeg;
	float deltaDegrees;
	float lx;   // +right
	float ly;   // +up
};

class TouchRing {
public:
	TouchRing(TouchRing const&) = delete;
	void operator=(TouchRing const&) = delete;
	static TouchRing& getInstance()
	{
		static TouchRing instance;
		return instance;
	}

	// Load the configured ring pins from BoardConfig.h. Returns true if a ring
	// is configured (4 pins). Idempotent; safe to call once at boot.
	bool initialize();

	// Read the ring once, computing the contact point. Uses the TouchGpio raw
	// readings. Safe to call every scan cycle (EMA smoothing provides
	// stability). Consumes a `touchValues` array (NUM_BANK0_GPIOS entries) or,
	// if null, reads the pads directly via TouchGpio.
	void process(const uint32_t* touchValues);

	bool isConfigured() const { return configured; }
	const RingState& getState() const { return state; }

private:
	TouchRing();
	Pin_t pins[4];
	bool configured;
	RingState state;
	float prevAngleDeg;
	bool hadPrev;
	// EMA of the vector (normalized) for stability.
	float emaX;
	float emaY;
};

#endif // _TOUCH_RING_H_