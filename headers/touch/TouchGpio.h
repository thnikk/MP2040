#ifndef _TOUCH_GPIO_H_
#define _TOUCH_GPIO_H_

#include "hardware/pio.h"
#include "pico/types.h"
#include "types.h"

// Board tuning defaults (override in BoardConfig.h). These seed the stored
// config's touchMargin / touchRelease when a config is reset or first
// created; at runtime the percentages come from the config, not these macros.
#ifndef TOUCH_MARGIN
#define TOUCH_MARGIN 15
#endif
#ifndef TOUCH_RELEASE
#define TOUCH_RELEASE 15
#endif

// Capacitive touch input using a small pool of PIO state machines shared
// across all pads (one SM does NOT need to be dedicated per pad).
//
// Each pad connects to a GPIO with a ~1M ohm resistor to ground. A PIO state
// machine charges the pad HIGH, releases it to an input, and counts cycles
// until it discharges below the logic threshold. A finger on the pad adds
// capacitance, slowing the discharge and raising the count. Because the timing
// is done entirely in the PIO, measurements are deterministic and cost the CPU
// only two FIFO accesses per pad.
//
// Measurements are synchronous (put_blocking -> SM runs -> get_blocking), so
// a single SM can serve many pads by retargeting its pins between reads. The
// pads are multiplexed round-robin over a pool of up to 4 SMs on pio1; this
// keeps the SM count independent of pad count, so a board can have more pads
// than a PIO has state machines (e.g. BeatBoard's 10 pads) while the LED strip
// keeps its own SM on pio0.
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

	// Re-apply the current config's touchMargin / touchRelease to the live
	// thresholds, deriving them from the stored baselines (or the fixed board
	// threshold). Called after a web config save so tuning takes effect without
	// a reboot.
	void applyConfig();

	// Read the raw discharge counts for a set of pins (bit set in `pins`).
	// out[pin] is set to the raw value for each requested configured pad (0 if
	// the pin isn't a configured touch pad). Returns the mask of pins that were
	// actually read. Unlike scan(), this exposes the proportional capacitance
	// reading so a caller can interpolate position/strength (e.g. the touch
	// ring) rather than just a pressed bit.
	GpioMask readValues(GpioMask pins, uint32_t* out);

	// Idle baseline (raw discharge count with nothing touching) for a pad, or
	// 0 if the pin isn't a configured/active touch pad. Lets a caller measure
	// only the capacitance a finger adds (raw - baseline).
	uint32_t getBaseline(Pin_t pin) const {
		if (pin < 0 || pin >= (Pin_t)NUM_BANK0_GPIOS || !active[pin])
			return 0;
		return baseline[pin];
	}

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
	// Derive thresholdOn/thresholdOff from baseline[pin] using the configured
	// margin (press) and hysteresis (release) percentages.
	void deriveThresholds(Pin_t pin);
	uint32_t readPin(Pin_t pin);

	// All pads are multiplexed over a small shared pool of state machines on
	// pio1. Each measurement is synchronous (put_blocking -> SM runs ->
	// get_blocking), so a single SM can serve many pads by retargeting its
	// 'set pins' / 'jmp pin' to the pad being read between measurements. This
	// keeps a board's pad count from exhausting the PIO's 4 state machines
	// (the LED strip owns one on pio0; a board like BeatBoard has 10 pads).
	PIO pio;
	uint32_t smOffset;
	uint32_t smCount;                       // number of pool SMs actually claimed
	GpioMask mask;                          // set bits = configured touch pads
	uint8_t smForPin[NUM_BANK0_GPIOS];      // pool slot for each pad (0xFF = not configured)
	uint32_t baseline[NUM_BANK0_GPIOS];     // raw idle baseline per pad
	uint32_t thresholdOn[NUM_BANK0_GPIOS];
	uint32_t thresholdOff[NUM_BANK0_GPIOS];
	uint32_t margin;                        // press % over baseline (config)
	uint32_t release;                       // release % of press threshold (config)
	bool active[NUM_BANK0_GPIOS];
	bool touched[NUM_BANK0_GPIOS];
};

#endif
