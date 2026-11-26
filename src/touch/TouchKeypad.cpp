#include "touch/TouchKeypad.h"

#include "BoardConfig.h"
#include "storagemanager.h"

#if defined(MATRIX_TOUCH_KEYPAD) && MATRIX_TOUCH_KEYPAD
#include "touch/FT6336U.h"

#include "pico/time.h"

// Polling faster than the controller's report rate only burns I2C time; ~14ms
// is the panel's fastest active scan, so 2ms gives a fresh sample without
// adding bus load to every core-0 loop iteration.
#ifndef TOUCH_KEYPAD_SCAN_INTERVAL_US
#define TOUCH_KEYPAD_SCAN_INTERVAL_US 2000
#endif

#ifndef TOUCH_KEYPAD_RELEASE_MARGIN
#define TOUCH_KEYPAD_RELEASE_MARGIN 4
#endif

namespace
{
constexpr uint32_t ROWS = MATRIX_ROWS;
constexpr uint32_t COLS = MATRIX_COLS;
constexpr uint32_t AREA_H = LCD_HEIGHT - KEYPAD_FOOTER_HEIGHT;

// Horizontal travel that turns a footer touch into a profile swipe. Roughly an
// eighth of the panel width: far enough that finger jitter can't trigger it.
constexpr int32_t SWIPE_MIN_DISTANCE = 40;

inline int32_t abs32(int32_t v) { return v < 0 ? -v : v; }

uint16_t transformAxis(uint16_t raw, uint16_t rawMax, uint16_t outMax, bool invert)
{
	uint32_t v = raw;
	if (v > rawMax) v = rawMax;
	uint32_t out = (v * (uint32_t)(outMax - 1) + rawMax / 2) / rawMax;
	if (invert) out = (uint32_t)(outMax - 1) - out;
	return (uint16_t)out;
}

void pointToDisplay(const Ft6336Point& p, uint16_t& x, uint16_t& y)
{
#if TOUCH_SWAP_XY
	const uint16_t rawX = p.y;
	const uint16_t rawY = p.x;
	const uint16_t maxX = TOUCH_RAW_MAX_Y;
	const uint16_t maxY = TOUCH_RAW_MAX_X;
#else
	const uint16_t rawX = p.x;
	const uint16_t rawY = p.y;
	const uint16_t maxX = TOUCH_RAW_MAX_X;
	const uint16_t maxY = TOUCH_RAW_MAX_Y;
#endif
	x = transformAxis(rawX, maxX, LCD_WIDTH, TOUCH_INVERT_X);
	y = transformAxis(rawY, maxY, LCD_HEIGHT, TOUCH_INVERT_Y);
}

// Cell containing (x, y), or -1 when the point is outside the key area. The
// same integer boundaries are used by KeypadDisplay (col * width / COLS), so a
// cell's drawn area and its hit area always agree.
int32_t cellAt(uint16_t x, uint16_t y)
{
	if (y < KEYPAD_FOOTER_HEIGHT)
		return -1;
	const uint32_t col = (uint32_t)x * COLS / LCD_WIDTH;
	const uint32_t row = (uint32_t)(y - KEYPAD_FOOTER_HEIGHT) * ROWS / AREA_H;
	if (col >= COLS || row >= ROWS)
		return -1;
	return (int32_t)(row * COLS + col);
}
} // namespace

bool TouchKeypad::setup()
{
	if (initialized)
		return true;

	state.clear();
	lastScanUs = 0;
	initialized = FT6336U::getInstance().setup();
	return initialized;
}

KeyMask TouchKeypad::scan()
{
	if (!initialized)
		return KeyMask();

	const uint32_t now = to_us_since_boot(get_absolute_time());
	if (now - lastScanUs < TOUCH_KEYPAD_SCAN_INTERVAL_US)
		return state;
	lastScanUs = now;
	scanCount++;

	Ft6336Point points[2] = {};
	const uint8_t count = FT6336U::getInstance().read(points);
	// Any touch on the panel (keys, gaps, or the top bar) wakes the LEDs.
	Storage::getInstance().setTouchActive(count > 0);

	uint16_t xs[2] = {};
	uint16_t ys[2] = {};
	for (uint8_t i = 0; i < count; i++)
		pointToDisplay(points[i], xs[i], ys[i]);

	// The top bar is a gesture area, not part of the key grid. A touch that
	// starts there is tracked as a horizontal swipe and its points are kept
	// out of the key mask until it lifts, so dragging down into the keys can't
	// press them along the way.
	if (count == 0)
	{
		swipeActive = false;
		swipeTriggered = false;
	}
	else if (!swipeActive)
	{
		for (uint8_t i = 0; i < count; i++)
		{
			if (ys[i] >= KEYPAD_FOOTER_HEIGHT)
				continue;
			swipeActive = true;
			swipeTriggered = false;
			swipeStartX = xs[i];
			swipeStartY = ys[i];
			break;
		}
	}

	if (swipeActive && count > 0 && !swipeTriggered)
	{
		const int32_t dx = (int32_t)xs[0] - (int32_t)swipeStartX;
		const int32_t dy = (int32_t)ys[0] - (int32_t)swipeStartY;
		if (abs32(dx) >= SWIPE_MIN_DISTANCE && abs32(dx) >= abs32(dy))
		{
			// Left = next profile, right = previous.
			pendingProfileStep = dx < 0 ? 1 : -1;
			swipeTriggered = true;
		}
	}

	KeyMask next;
	if (!swipeActive)
	{
		for (uint8_t i = 0; i < count; i++)
		{
			const int32_t cell = cellAt(xs[i], ys[i]);
			if (cell >= 0)
				next.set((uint32_t)cell);
		}
	}

	// Release hysteresis: a cell that is already held stays pressed until every
	// point leaves a small margin around it. New presses only need to enter the
	// cell itself, so press latency is unchanged.
	for (uint32_t idx = 0; idx < ROWS * COLS; idx++)
	{
		if (!state.test(idx) || next.test(idx))
			continue;
		const uint32_t row = idx / COLS;
		const uint32_t col = idx % COLS;
		const int32_t x0 = (int32_t)(col * LCD_WIDTH / COLS) - TOUCH_KEYPAD_RELEASE_MARGIN;
		const int32_t y0 = (int32_t)KEYPAD_FOOTER_HEIGHT +
			(int32_t)(row * AREA_H / ROWS) - TOUCH_KEYPAD_RELEASE_MARGIN;
		const int32_t x1 = (int32_t)((col + 1) * LCD_WIDTH / COLS) + TOUCH_KEYPAD_RELEASE_MARGIN;
		const int32_t y1 = (int32_t)KEYPAD_FOOTER_HEIGHT +
			(int32_t)((row + 1) * AREA_H / ROWS) + TOUCH_KEYPAD_RELEASE_MARGIN;
		for (uint8_t i = 0; i < count; i++)
		{
			if ((int32_t)xs[i] >= x0 && (int32_t)xs[i] < x1 &&
			    (int32_t)ys[i] >= y0 && (int32_t)ys[i] < y1)
			{
				next.set(idx);
				break;
			}
		}
	}

	state = next;
	return state;
}

int8_t TouchKeypad::takeProfileSwipe()
{
	const int8_t step = pendingProfileStep;
	pendingProfileStep = 0;
	return step;
}

#endif // MATRIX_TOUCH_KEYPAD
