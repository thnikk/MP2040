#ifndef _KEYPADDISPLAY_H_
#define _KEYPADDISPLAY_H_

#include <stdint.h>
#include <string>

#include "BoardConfig.h"

// On-screen RGB keypad for the color touch panel.
//
// The LED controller computes one RGB color per virtual LED (the same values it
// would send to a WS2812 strip) and calls setPixel()/show(). This class owns the
// ST7796 panel, draws a cell per virtual LED, and only redraws cells whose
// color or label changed. Everything runs on core 1, so panel transfers never
// delay touch input (core 0).
#if defined(MATRIX_TOUCH_KEYPAD) && MATRIX_TOUCH_KEYPAD

class KeypadDisplay
{
public:
	KeypadDisplay(KeypadDisplay const&) = delete;
	void operator=(KeypadDisplay const&) = delete;
	static KeypadDisplay& getInstance()
	{
		static KeypadDisplay instance;
		return instance;
	}

	bool setup();
	bool isPresent() const { return present; }

	void setPixel(uint32_t ledIndex, uint8_t r, uint8_t g, uint8_t b);
	void show();
	void off();
	uint32_t getLedCount() const;

private:
	KeypadDisplay() {}

	static constexpr uint32_t ROWS = MATRIX_ROWS;
	static constexpr uint32_t COLS = MATRIX_COLS;
	static constexpr uint32_t CELL_COUNT = ROWS * COLS;
	static constexpr uint32_t AREA_H = LCD_HEIGHT - KEYPAD_FOOTER_HEIGHT;
	static constexpr uint32_t MAX_CELL_W = (LCD_WIDTH + COLS - 1) / COLS;
	static constexpr uint32_t MAX_CELL_H = (AREA_H + ROWS - 1) / ROWS;
	static constexpr uint32_t MAX_CELL_PIXELS = MAX_CELL_W * MAX_CELL_H;
	// drawFooter reuses the pixel buffer, and the footer can be larger than a
	// single cell now that the buttons are square and the bar takes the rest.
	static constexpr uint32_t FOOTER_PIXELS = LCD_WIDTH * KEYPAD_FOOTER_HEIGHT;
	static constexpr uint32_t BUFFER_PIXELS =
		MAX_CELL_PIXELS > FOOTER_PIXELS ? MAX_CELL_PIXELS : FOOTER_PIXELS;

	void updateLabels(bool force);
	void updateFooter(bool force);
	uint32_t labelHash() const;
	void drawCell(uint32_t cell, uint32_t color888);
	void drawFooter();

	uint16_t pixelBuf[BUFFER_PIXELS] = {};
	int32_t ledToCell[256] = {};
	uint32_t pending[CELL_COUNT] = {};
	uint32_t shown[CELL_COUNT] = {};
	uint32_t iconIds[CELL_COUNT] = {};
	uint32_t fgCurrent[CELL_COUNT] = {};
	bool fgAnimating[CELL_COUNT] = {};
	bool fgInit[CELL_COUNT] = {};
	bool dirty[CELL_COUNT] = {};
	std::string labels[CELL_COUNT];
	std::string footerLeft;
	std::string footerRight;
	std::string footerCenter;
	uint32_t fpsFrames = 0;
	uint32_t fpsLastMs = 0;
	uint32_t renderFps = 0;
	uint32_t inputFps = 0;
	uint32_t lastScanCount = 0;
	uint32_t lastLabelHash = 0xFFFFFFFFu;
	bool present = false;
};

#else

// Stub for boards without a touch keypad: LedController can reference the
// singleton without pulling in the panel driver.
class KeypadDisplay
{
public:
	static KeypadDisplay& getInstance()
	{
		static KeypadDisplay instance;
		return instance;
	}
	bool setup() { return false; }
	bool isPresent() const { return false; }
	void setPixel(uint32_t, uint8_t, uint8_t, uint8_t) {}
	void show() {}
	void off() {}
	uint32_t getLedCount() const { return 0; }
private:
	KeypadDisplay() {}
};

#endif

#endif
