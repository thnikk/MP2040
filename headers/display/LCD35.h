#ifndef _LCD35_H_
#define _LCD35_H_

#include <stdint.h>

// Minimal ST7796 320x480 SPI panel driver for the Waveshare RP2350-Touch-LCD-3.5.
//
// Pixels are pushed as 16-bit RGB565 in big-endian byte order (the panel's
// wire format). Callers building a buffer for writePixels() must byte-swap each
// RGB565 value first; fill() does that internally.
class LCD35
{
public:
	LCD35(LCD35 const&) = delete;
	void operator=(LCD35 const&) = delete;
	static LCD35& getInstance()
	{
		static LCD35 instance;
		return instance;
	}

	bool setup();

	uint16_t width() const;
	uint16_t height() const;

	void fill(uint16_t color565);

	// Set the write window and leave the panel ready for pixel data.
	void setWindow(uint16_t x, uint16_t y, uint16_t w, uint16_t h);

	// Push a window of byte-swapped RGB565 pixels.
	void writePixels(const uint16_t* swapped, uint32_t count);

	void backlight(uint8_t percent);

private:
	LCD35() {}

	void command(uint8_t cmd);
	void data(const uint8_t* buf, uint32_t len);
	void reset();

	int dmaChannel = -1;
	bool ready = false;
};

#endif
