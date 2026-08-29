#ifndef _TOUCH_RING_H_
#define _TOUCH_RING_H_

#include <stdint.h>
#include "hardware/platform_defs.h"
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
// Boards opt in by defining RING_PAD0..RING_PAD3 (the four pins, in physical
// order around the ring) plus the tunables below in BoardConfig.h. The ring is
// then interpreted per input mode by the active driver (analog stick in
// gamepad modes, volume/scroll in keyboard mode, pitch bend in MIDI mode).
//

// Result of a ring read. `active` means a touch was detected on the ring;
// `magnitude` (0..1) is how strongly, `angleDeg` (0..359) the position around
// the ring (0 = the RING_PAD0 direction, increasing toward RING_PAD1), and
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
	// readings with each pad's idle baseline subtracted (so only the finger's
	// added capacitance drives the centroid). Safe to call every scan cycle
	// (EMA smoothing provides stability). Consumes a `touchValues` array
	// (NUM_BANK0_GPIOS raw readings) or, if null, reads the pads directly via
	// TouchGpio.
	void process(const uint32_t* touchValues);

	bool isConfigured() const { return configured; }
	const RingState& getState() const { return state; }

	// Bitmask of the four ring pads. Used by the key setup so the ring pads
	// get handed to the PIO capsense driver even though they have no keycode
	// (the ring reads them as proportional capacitance, not as keys).
	GpioMask getRingMask() const {
		GpioMask m = 0;
		for (int i = 0; i < 4; i++)
		{
			if (pins[i] < (Pin_t)NUM_BANK0_GPIOS)
				m |= 1u << pins[i];
		}
		return m;
	}

private:
	TouchRing();
	Pin_t pins[4];
	bool configured;
	RingState state;
	float prevAngleDeg;
	// Unfiltered direction angle from the previous read, for the angular
	// velocity estimate.
	float prevRawAngle;
	// EMA of the signed angular velocity (radians/frame). Coherent motion
	// builds this up; the back-and-forth jitter of a held position cancels to
	// ~zero. Drives the adaptive EMA coefficient so the filter stays heavy
	// while holding (killing direction jitter) and opens up when sliding.
	float vel;
	bool hadPrev;
	// Active-state latch for engage/release hysteresis. Unlike state.active
	// (cleared each process()), this persists so a marginal hold that dips
	// below the engage threshold doesn't flicker the ring off.
	bool wasActive;
	// EMA of the vector (normalized) for stability.
	float emaX;
	float emaY;
};

#endif // _TOUCH_RING_H_