#ifndef _FT6336U_H_
#define _FT6336U_H_

#include <stdint.h>

// Minimal FT6336 capacitive touch controller driver (I2C).
//
// The panel exposes 2 touch points in registers 0x02-0x0E. Everything is polled
// from core 0; there is no interrupt handling, so a scan is just one I2C burst.
// Pin/block/address configuration lives in BoardConfig.h.
struct Ft6336Point
{
	uint16_t x;
	uint16_t y;
};

class FT6336U
{
public:
	FT6336U(FT6336U const&) = delete;
	void operator=(FT6336U const&) = delete;
	static FT6336U& getInstance()
	{
		static FT6336U instance;
		return instance;
	}

	// Initialize the I2C block, reset the controller and put it in fast active
	// polling mode. Returns true when the chip ID matches.
	bool setup();

	// Read the current touch points. Returns the number of active points (0-2)
	// and fills `points`. A point reported as released is skipped.
	uint8_t read(Ft6336Point points[2]);

	bool isPresent() const { return present; }

private:
	FT6336U() {}

	void writeReg(uint8_t reg, uint8_t value);
	uint8_t readReg(uint8_t reg);

	bool present = false;
};

#endif
