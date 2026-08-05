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

	// Claim PIO state machines for every set bit in touchMask and calibrate
	// their thresholds. Safe to call once at boot.
	void setup(Mask_t touchMask);

	// Measure all configured pads and return a mask of currently-touched pins.
	// Applies per-pad thresholds with release hysteresis.
	Mask_t scan();

	// Single-shot touch check for a pin (used by the boot webconfig check).
	bool isTouched(Pin_t pin);

private:
	TouchGpio();
	void calibrate();
	uint32_t readPin(Pin_t pin);

	PIO pio;
	uint32_t smOffset;
	Mask_t mask;
	uint8_t smForPin[NUM_BANK0_GPIOS];      // 0xFF = pin not configured
	uint32_t thresholdOn[NUM_BANK0_GPIOS];
	uint32_t thresholdOff[NUM_BANK0_GPIOS];
	bool active[NUM_BANK0_GPIOS];
	bool touched[NUM_BANK0_GPIOS];
};

#endif
