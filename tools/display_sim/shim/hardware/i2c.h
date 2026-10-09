//
// Host shim for "hardware/i2c.h" (MP2040 display simulator).
//
// tiny_ssd1306.cpp only needs the i2c_inst_t type and the two blocking
// transfer calls. On the host those are no-ops (the framebuffer is read back
// with GPGFX::getPixel instead of being flushed to a panel), so the real
// driver compiles and runs unchanged.
//
#ifndef _SIM_HARDWARE_I2C_H_
#define _SIM_HARDWARE_I2C_H_

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct i2c_inst i2c_inst_t;

extern i2c_inst_t *i2c0;
extern i2c_inst_t *i2c1;

void i2c_init(i2c_inst_t *i2c, unsigned int baudrate);
int i2c_write_blocking(i2c_inst_t *i2c, uint8_t addr, const uint8_t *src, size_t len, bool nostop);
int i2c_read_blocking(i2c_inst_t *i2c, uint8_t addr, uint8_t *dst, size_t len, bool nostop);

#ifdef __cplusplus
}
#endif

#endif
