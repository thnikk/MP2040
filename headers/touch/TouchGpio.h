#ifndef _TOUCH_GPIO_H_
#define _TOUCH_GPIO_H_

#include "hardware/pio.h"
#include "pico/types.h"
#include "types.h"

// Capacitive touch input using one PIO state machine per pad.
//
// Each pad connects to a GPIO with a ~1M ohm resistor to ground. A PIO state
// machine charges the pad HIGH, releases it to an input, and counts cycles
// until it discharges below the logic threshold. A finger on the pad adds
// capacitance, slowing the discharge and raising the count. Because the timing
// is done entirely in the PIO, measurements are deterministic and cost the CPU
// only two FIFO accesses per pad.
//
// Thresholds are auto-calibrated at setup() from the idle baseline (the
// lowest of several samples). A pad can instead pin a fixed threshold with the
// board's TOUCH_THRESHOLD_GPxx define.
class TouchGpio {
public:
	TouchGpio(TouchGpio const&) = delete;
	void operator=(TouchGpio const&)  = delete;
	static TouchGpio& getInstance() // Thread-safe singleton
	{
		static TouchGpio instance;
		return instance;
	}

	// Claim PIO state machines for every set bit in touchMask and set up their
	// thresholds. Safe to call once at boot. When `useStored` is true (a web
	// config reboot, where the pad that triggered the reboot is still held)
	// the thresholds saved by the last normal boot are loaded instead of being
	// re-sampled; pads without a stored entry are calibrated fresh.
	void setup(GpioMask touchMask, bool useStored);

	// Measure all configured pads and return a mask of currently-touched pins.
	// Applies per-pad thresholds with release hysteresis.
	GpioMask scan();

	// Read the raw discharge counts for a set of pins (bit set in `pins`).
	// out[pin] is set to the raw value for each requested configured pad (0 if
	// the pin isn't a configured touch pad). Returns the mask of pins that were
	// actually read. Unlike scan(), this exposes the proportional capacitance
	// reading so a caller can interpolate position/strength (e.g. the touch
	// ring) rather than just a pressed bit.
	GpioMask readValues(GpioMask pins, uint32_t* out);

private:
	TouchGpio();
	// Auto-calibrate all configured pads, then persist the thresholds to the
	// stored config so a web config reboot can load them.
	void calibrate();
	// Load the stored thresholds (falling back to fresh calibration for pads
	// without an entry).
	void loadCalibration();
	// Calibrate a single pad (fixed threshold from the board config, or the
	// lowest of several idle samples + margin).
	void calibratePin(Pin_t pin);
	uint32_t readPin(Pin_t pin);

	PIO pio;
	uint32_t smOffset;
	GpioMask mask;
	uint8_t smForPin[NUM_BANK0_GPIOS];      // 0xFF = pin not configured
	uint32_t thresholdOn[NUM_BANK0_GPIOS];
	uint32_t thresholdOff[NUM_BANK0_GPIOS];
	bool active[NUM_BANK0_GPIOS];
	bool touched[NUM_BANK0_GPIOS];
};

#endif
