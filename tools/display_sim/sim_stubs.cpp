//
// Host shim implementations: pico i2c no-ops and the virtual/real clock.
//
#include "helper.h"      // declares getMillis()/getMicro()
#include "sim_clock.h"
#include "hardware/i2c.h"

#include <chrono>
#include <cstring>

// ---- i2c ----------------------------------------------------------------
//
// The panel is never written to on the host; the framebuffer is read back with
// GPGFX::getPixel(). Returning success from the transfers lets
// GPGFX_TinySSD1306::init()/drawBuffer() run unchanged. Reads come back as
// zeros, which makes the SH1106 probe fail and settles the driver on a plain
// 128-column SSD1306 (no 2px x-offset), matching the common panel.

struct i2c_inst {
    int unused;
};

static struct i2c_inst s_i2c0;
static struct i2c_inst s_i2c1;

i2c_inst_t *i2c0 = &s_i2c0;
i2c_inst_t *i2c1 = &s_i2c1;

void i2c_init(i2c_inst_t *, unsigned int) {}

int i2c_write_blocking(i2c_inst_t *, uint8_t, const uint8_t *, size_t len, bool) {
    return (int)len;
}

int i2c_read_blocking(i2c_inst_t *, uint8_t, uint8_t *dst, size_t len, bool) {
    if (dst != nullptr) memset(dst, 0, len);
    return (int)len;
}

// ---- clock --------------------------------------------------------------

static bool g_virtual = false;
static uint32_t g_virtualMs = 0;

void simSetVirtualMillis(uint32_t ms) {
    g_virtual = true;
    g_virtualMs = ms;
}

void simUseRealClock() {
    g_virtual = false;
}

uint32_t getMillis() {
    if (g_virtual) return g_virtualMs;
    using namespace std::chrono;
    return (uint32_t)duration_cast<milliseconds>(
        steady_clock::now().time_since_epoch()).count();
}

uint64_t getMicro() {
    if (g_virtual) return (uint64_t)g_virtualMs * 1000ull;
    using namespace std::chrono;
    return (uint64_t)duration_cast<microseconds>(
        steady_clock::now().time_since_epoch()).count();
}
