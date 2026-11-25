#include "display/LCD35.h"

#include "BoardConfig.h"

#if defined(MATRIX_TOUCH_KEYPAD) && MATRIX_TOUCH_KEYPAD

#include "hardware/gpio.h"
#include "hardware/pwm.h"
#include "hardware/spi.h"
#include "hardware/timer.h"
#include "pico/stdlib.h"

namespace
{
spi_inst_t* const SPI = LCD_SPI;

// The panel samples CS/DC on the SPI clock edge. The pico-sdk GPIO writes are
// only a few ns ahead of the first SPI clock, while the vendor Arduino code
// has implicit microseconds of overhead between control-line changes and the
// transfer. Without a short settle time the ST7796 intermittently misreads a
// command or treats the start of the next window command as pixel data.
#ifndef LCD_CTRL_DELAY_US
#define LCD_CTRL_DELAY_US 2
#endif

void controlDelay()
{
	busy_wait_us(LCD_CTRL_DELAY_US);
}

void writeBytes(const uint8_t* buf, uint32_t len)
{
	if (len)
		spi_write_blocking(SPI, buf, len);
}
} // namespace

uint16_t LCD35::width() const { return LCD_WIDTH; }
uint16_t LCD35::height() const { return LCD_HEIGHT; }

bool LCD35::setup()
{
	if (ready)
		return true;

	// Backlight PWM first: it also gives an immediate "powered on" cue while
	// the panel finishes its reset/init sequence.
	gpio_set_function(LCD_PIN_BL, GPIO_FUNC_PWM);
	const uint slice = pwm_gpio_to_slice_num(LCD_PIN_BL);
	const uint chan = pwm_gpio_to_channel(LCD_PIN_BL);
	pwm_set_wrap(slice, 100);
	pwm_set_clkdiv(slice, 50);
	pwm_set_chan_level(slice, chan, 0);
	pwm_set_enabled(slice, true);

	// SPI0, 8-bit frames, mode 0, MSB first.
	spi_init(SPI, LCD_SPI_HZ);
	spi_set_format(SPI, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
	gpio_set_function(LCD_PIN_SCK, GPIO_FUNC_SPI);
	gpio_set_function(LCD_PIN_MOSI, GPIO_FUNC_SPI);

	gpio_init(LCD_PIN_CS);
	gpio_set_dir(LCD_PIN_CS, GPIO_OUT);
	gpio_put(LCD_PIN_CS, 1);
	gpio_init(LCD_PIN_DC);
	gpio_set_dir(LCD_PIN_DC, GPIO_OUT);
	gpio_put(LCD_PIN_DC, 1);
	gpio_init(LCD_PIN_RST);
	gpio_set_dir(LCD_PIN_RST, GPIO_OUT);

	reset();

	// Sleep out, then memory access / color format. This mirrors the vendor
	// init sequence for the 3.5" panel (ST7796 with the F0 unlock commands).
	command(0x11);
	sleep_ms(120);

	{
		const uint8_t madctl = 0x88; // MX | MY | BGR
		command(0x36);
		data(&madctl, 1);
	}
	{
		const uint8_t colmod = 0x05; // 16 bits/pixel
		command(0x3A);
		data(&colmod, 1);
	}

	// Unlock / power / VCOM / gamma.
	{
		const uint8_t v[] = { 0xC3 };
		command(0xF0); data(v, sizeof(v));
	}
	{
		const uint8_t v[] = { 0x96 };
		command(0xF0); data(v, sizeof(v));
	}
	{
		const uint8_t v[] = { 0x01 };
		command(0xB4); data(v, sizeof(v));
	}
	{
		const uint8_t v[] = { 0xC6 };
		command(0xB7); data(v, sizeof(v));
	}
	{
		const uint8_t v[] = { 0x80, 0x45 };
		command(0xC0); data(v, sizeof(v));
	}
	{
		const uint8_t v[] = { 0x13 };
		command(0xC1); data(v, sizeof(v));
	}
	{
		const uint8_t v[] = { 0xA7 };
		command(0xC2); data(v, sizeof(v));
	}
	{
		const uint8_t v[] = { 0x0A };
		command(0xC5); data(v, sizeof(v));
	}
	{
		const uint8_t v[] = { 0x40, 0x8A, 0x00, 0x00, 0x29, 0x19, 0xA5, 0x33 };
		command(0xE8); data(v, sizeof(v));
	}
	{
		const uint8_t v[] = { 0xD0, 0x08, 0x0F, 0x06, 0x06, 0x33, 0x30, 0x33,
			0x47, 0x17, 0x13, 0x13, 0x2B, 0x31 };
		command(0xE0); data(v, sizeof(v));
	}
	{
		const uint8_t v[] = { 0xD0, 0x0A, 0x11, 0x0B, 0x09, 0x07, 0x2F, 0x33,
			0x47, 0x38, 0x15, 0x16, 0x2C, 0x32 };
		command(0xE1); data(v, sizeof(v));
	}
	{
		const uint8_t v[] = { 0x3C };
		command(0xF0); data(v, sizeof(v));
	}
	{
		const uint8_t v[] = { 0x69 };
		command(0xF0); data(v, sizeof(v));
	}
	sleep_ms(120);
	command(0x21); // display inversion on (vendor default)
	command(0x29); // display on

	fill(0x0000);
	backlight(LCD_BACKLIGHT_PERCENT);
	ready = true;
	return true;
}

void LCD35::reset()
{
	gpio_put(LCD_PIN_RST, 1);
	sleep_ms(100);
	gpio_put(LCD_PIN_RST, 0);
	sleep_ms(100);
	gpio_put(LCD_PIN_RST, 1);
	sleep_ms(100);
}

void LCD35::command(uint8_t cmd)
{
	gpio_put(LCD_PIN_DC, 0);
	gpio_put(LCD_PIN_CS, 0);
	controlDelay();
	writeBytes(&cmd, 1);
	gpio_put(LCD_PIN_CS, 1);
	controlDelay();
}

void LCD35::data(const uint8_t* buf, uint32_t len)
{
	gpio_put(LCD_PIN_DC, 1);
	gpio_put(LCD_PIN_CS, 0);
	controlDelay();
	writeBytes(buf, len);
	gpio_put(LCD_PIN_CS, 1);
	controlDelay();
}

void LCD35::setWindow(uint16_t x, uint16_t y, uint16_t w, uint16_t h)
{
	if (w == 0 || h == 0)
		return;

	const uint16_t x1 = (uint16_t)(x + w - 1);
	const uint16_t y1 = (uint16_t)(y + h - 1);
	uint8_t buf[4];

	command(0x2A);
	buf[0] = (uint8_t)(x >> 8);
	buf[1] = (uint8_t)x;
	buf[2] = (uint8_t)(x1 >> 8);
	buf[3] = (uint8_t)x1;
	data(buf, sizeof(buf));

	command(0x2B);
	buf[0] = (uint8_t)(y >> 8);
	buf[1] = (uint8_t)y;
	buf[2] = (uint8_t)(y1 >> 8);
	buf[3] = (uint8_t)y1;
	data(buf, sizeof(buf));

	command(0x2C);
}

void LCD35::writePixels(const uint16_t* swapped, uint32_t count)
{
	if (count == 0)
		return;

	gpio_put(LCD_PIN_DC, 1);
	gpio_put(LCD_PIN_CS, 0);
	controlDelay();

	// Blocking write: spi_write_blocking waits for the TX FIFO to drain and
	// BSY to clear, so the panel has definitely received every pixel before
	// CS goes high and the next window command starts. The DMA path could
	// raise CS while the last FIFO entry was still shifting, which made the
	// next 0x2A/0x2B command get interpreted as pixel data.
	writeBytes(reinterpret_cast<const uint8_t*>(swapped), count * 2);

	gpio_put(LCD_PIN_CS, 1);
	controlDelay();
}

void LCD35::fill(uint16_t color565)
{
	static uint16_t chunk[256];
	const uint16_t swapped = (uint16_t)((color565 >> 8) | (color565 << 8));
	for (uint32_t i = 0; i < 256; i++)
		chunk[i] = swapped;

	const uint32_t total = (uint32_t)LCD_WIDTH * LCD_HEIGHT;
	setWindow(0, 0, LCD_WIDTH, LCD_HEIGHT);

	gpio_put(LCD_PIN_DC, 1);
	gpio_put(LCD_PIN_CS, 0);
	controlDelay();

	uint32_t remaining = total;
	while (remaining)
	{
		const uint32_t n = remaining < 256 ? remaining : 256;
		writeBytes(reinterpret_cast<const uint8_t*>(chunk), n * 2);
		remaining -= n;
	}
	gpio_put(LCD_PIN_CS, 1);
	controlDelay();
}

void LCD35::backlight(uint8_t percent)
{
	if (percent > 100)
		percent = 100;
	const uint slice = pwm_gpio_to_slice_num(LCD_PIN_BL);
	const uint chan = pwm_gpio_to_channel(LCD_PIN_BL);
	pwm_set_chan_level(slice, chan, percent);
}

#endif // MATRIX_TOUCH_KEYPAD
