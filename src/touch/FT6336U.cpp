#include "touch/FT6336U.h"

#include "BoardConfig.h"

#if defined(MATRIX_TOUCH_KEYPAD) && MATRIX_TOUCH_KEYPAD
#include "helper.h"

#include "hardware/gpio.h"
#include "hardware/i2c.h"
#include "pico/stdlib.h"

// FT6336 register map (only the registers used here).
#define FT6336_ADDR 0x38
#define FT6336_REG_TD_STATUS 0x02
#define FT6336_REG_P1_XH 0x03
#define FT6336_REG_CHIP_ID 0xA3
#define FT6336_REG_CTRL 0x86
#define FT6336_REG_ACTIVE_RATE 0x88
#define FT6336_REG_G_MODE 0xA4

// Touch event field in each point's XH byte.
#define FT6336_EVENT_RELEASE 0x01
#define FT6336_EVENT_MASK 0xC0
#define FT6336_EVENT_SHIFT 6

#define FT6336_POINT_BYTES 6

static void ftDelayMs(uint32_t ms)
{
	sleep_ms(ms);
}

bool FT6336U::setup()
{
	i2c_init(TOUCH_I2C, TOUCH_I2C_HZ);
	gpio_set_function(TOUCH_PIN_SDA, GPIO_FUNC_I2C);
	gpio_set_function(TOUCH_PIN_SCL, GPIO_FUNC_I2C);
	gpio_pull_up(TOUCH_PIN_SDA);
	gpio_pull_up(TOUCH_PIN_SCL);

	// The interrupt line is unused (polling mode), but configuring it as a
	// pulled-up input keeps a floating pad from coupling noise into the panel.
	gpio_init(TOUCH_PIN_INT);
	gpio_set_dir(TOUCH_PIN_INT, GPIO_IN);
	gpio_pull_up(TOUCH_PIN_INT);

	// Hardware reset.
	gpio_init(TOUCH_PIN_RST);
	gpio_set_dir(TOUCH_PIN_RST, GPIO_OUT);
	gpio_put(TOUCH_PIN_RST, 1);
	ftDelayMs(10);
	gpio_put(TOUCH_PIN_RST, 0);
	ftDelayMs(10);
	gpio_put(TOUCH_PIN_RST, 1);
	ftDelayMs(300);

	// Working mode, stay active (no monitor sleep), fastest scan rate, polling
	// interface.
	writeReg(0x00, 0x00);
	writeReg(FT6336_REG_CTRL, 0x00);
	writeReg(FT6336_REG_ACTIVE_RATE, 0x01);
	writeReg(FT6336_REG_G_MODE, 0x00);

	present = readReg(FT6336_REG_CHIP_ID) == 0x64;
	return present;
}

void FT6336U::writeReg(uint8_t reg, uint8_t value)
{
	uint8_t buf[2] = { reg, value };
	i2c_write_blocking(TOUCH_I2C, FT6336_ADDR, buf, sizeof(buf), false);
}

uint8_t FT6336U::readReg(uint8_t reg)
{
	uint8_t value = 0;
	if (i2c_write_blocking(TOUCH_I2C, FT6336_ADDR, &reg, 1, true) < 0)
		return 0;
	if (i2c_read_blocking(TOUCH_I2C, FT6336_ADDR, &value, 1, false) < 0)
		return 0;
	return value;
}

uint8_t FT6336U::read(Ft6336Point points[2])
{
	if (!present)
		return 0;

	// TD_STATUS through P2_MISC in one burst: status, then two 6-byte point
	// records (XH/XL/YH/YL/weight/misc).
	uint8_t buf[1 + 2 * FT6336_POINT_BYTES] = {};
	const uint8_t reg = FT6336_REG_TD_STATUS;
	if (i2c_write_blocking(TOUCH_I2C, FT6336_ADDR, &reg, 1, true) < 0)
		return 0;
	if (i2c_read_blocking(TOUCH_I2C, FT6336_ADDR, buf, sizeof(buf), false) < 0)
		return 0;

	uint8_t count = buf[0] & 0x0F;
	if (count > 2)
		count = 2;

	uint8_t active = 0;
	for (uint8_t i = 0; i < count; i++)
	{
		const uint8_t* p = &buf[1 + i * FT6336_POINT_BYTES];
		const uint8_t event = (p[0] & FT6336_EVENT_MASK) >> FT6336_EVENT_SHIFT;
		// A release report still carries coordinates; don't emit it as a press.
		if (event == FT6336_EVENT_RELEASE)
			continue;

		const uint16_t x = ((uint16_t)(p[0] & 0x0F) << 8) | p[1];
		const uint16_t y = ((uint16_t)(p[2] & 0x0F) << 8) | p[3];
		points[active].x = x;
		points[active].y = y;
		active++;
	}
	return active;
}

#endif // MATRIX_TOUCH_KEYPAD
