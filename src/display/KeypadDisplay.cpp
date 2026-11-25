#include "display/KeypadDisplay.h"

#include "BoardConfig.h"
#include "display/LCD35.h"
#include "display/fonts/GP_Font_Standard.h"
#include "display/inputlabels.h"
#include "helper.h"
#include "leds/LedLayout.h"
#include "storagemanager.h"

#if defined(MATRIX_TOUCH_KEYPAD) && MATRIX_TOUCH_KEYPAD

namespace
{
constexpr uint8_t FONT_W = 5;
constexpr uint8_t FONT_H = 7;
constexpr uint8_t FONT_ADVANCE = 6;
constexpr uint32_t FONT_CHAR_OFFSET = 32;
constexpr uint32_t BORDER = 3;

constexpr uint32_t RGB(uint8_t r, uint8_t g, uint8_t b)
{
	return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}

inline uint16_t rgb565(uint32_t c)
{
	const uint8_t r = (uint8_t)(c >> 16);
	const uint8_t g = (uint8_t)(c >> 8);
	const uint8_t b = (uint8_t)c;
	return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

// Byte order the panel expects over SPI (high byte first).
inline uint16_t swap16(uint16_t v)
{
	return (uint16_t)((v >> 8) | (v << 8));
}

inline uint32_t luminance(uint32_t c)
{
	const uint32_t r = (c >> 16) & 0xFF;
	const uint32_t g = (c >> 8) & 0xFF;
	const uint32_t b = c & 0xFF;
	return (r * 299 + g * 587 + b * 114) / 1000;
}

uint32_t borderColor(uint32_t c)
{
	if (luminance(c) > 140)
	{
		// Light button: darken the edge.
		const uint32_t r = ((c >> 16) & 0xFF) * 3 / 5;
		const uint32_t g = ((c >> 8) & 0xFF) * 3 / 5;
		const uint32_t b = (c & 0xFF) * 3 / 5;
		return RGB((uint8_t)r, (uint8_t)g, (uint8_t)b);
	}
	// Dark button: lighten the edge.
	const uint32_t r = ((c >> 16) & 0xFF) + 48;
	const uint32_t g = ((c >> 8) & 0xFF) + 48;
	const uint32_t b = (c & 0xFF) + 48;
	return RGB((uint8_t)(r > 255 ? 255 : r),
		(uint8_t)(g > 255 ? 255 : g),
		(uint8_t)(b > 255 ? 255 : b));
}

// Cell boundaries use the same integer split as TouchKeypad so the drawn and
// hit-tested areas agree exactly.
inline uint32_t colX0(uint32_t col) { return col * LCD_WIDTH / MATRIX_COLS; }
inline uint32_t colX1(uint32_t col) { return (col + 1) * LCD_WIDTH / MATRIX_COLS; }
inline uint32_t rowY0(uint32_t row) { return row * (LCD_HEIGHT - KEYPAD_FOOTER_HEIGHT) / MATRIX_ROWS; }
inline uint32_t rowY1(uint32_t row) { return (row + 1) * (LCD_HEIGHT - KEYPAD_FOOTER_HEIGHT) / MATRIX_ROWS; }

const char* inputModeName(InputMode mode)
{
	switch (mode)
	{
		case INPUT_MODE_MIDI: return "MIDI";
		case INPUT_MODE_XINPUT: return "XINPUT";
		case INPUT_MODE_SWITCH_PRO: return "SWPRO";
		case INPUT_MODE_XBOX_ONE: return "XBONE";
		case INPUT_MODE_PS3: return "PS3";
		case INPUT_MODE_PS4: return "PS4";
		case INPUT_MODE_PS5: return "PS5";
		case INPUT_MODE_KEYBOARD:
		default: return "HID-KB";
	}
}

void uppercase(std::string& s)
{
	for (char& c : s)
		if (c >= 'a' && c <= 'z')
			c = (char)(c - 'a' + 'A');
}
} // namespace

uint32_t KeypadDisplay::getLedCount() const
{
	return CELL_COUNT;
}

bool KeypadDisplay::setup()
{
	if (present)
		return true;

	if (!LCD35::getInstance().setup())
		return false;

	for (uint32_t i = 0; i < 256; i++)
		ledToCell[i] = -1;
	for (uint32_t row = 0; row < ROWS; row++)
	{
		for (uint32_t col = 0; col < COLS; col++)
		{
			const int32_t led = BOARD_LED_GRID[row][col];
			if (led >= 0 && led < 256)
				ledToCell[led] = (int32_t)(row * COLS + col);
		}
	}

	for (uint32_t i = 0; i < CELL_COUNT; i++)
	{
		pending[i] = 0;
		shown[i] = 0xFFFFFFFFu;
		dirty[i] = true;
	}

	present = true;
	updateLabels(true);
	updateFooter(true);
	return true;
}

void KeypadDisplay::setPixel(uint32_t ledIndex, uint8_t r, uint8_t g, uint8_t b)
{
	if (!present || ledIndex >= 256)
		return;
	const int32_t cell = ledToCell[ledIndex];
	if (cell < 0 || cell >= (int32_t)CELL_COUNT)
		return;
	pending[cell] = RGB(r, g, b);
}

void KeypadDisplay::off()
{
	if (!present)
		return;
	for (uint32_t i = 0; i < CELL_COUNT; i++)
	{
		pending[i] = 0;
		dirty[i] = true;
	}
	show();
}

uint32_t KeypadDisplay::labelHash() const
{
	Storage& s = Storage::getInstance();
	uint32_t h = 2166136261u;
	auto mix = [&h](uint32_t v) {
		h ^= v;
		h *= 16777619u;
	};
	mix((uint32_t)s.getDefaultInputMode());
	mix(s.getUseNintendoLayout() ? 1u : 0u);
	const Config& cfg = s.getConfig();
	const KeyMapping& km = s.getKeyMapping();
	for (uint32_t pin = 0; pin < CELL_COUNT; pin++)
	{
		mix(pin < (uint32_t)km.keycodes_count ? (uint32_t)km.keycodes[pin] : 0);
		mix(pin < (uint32_t)km.modifierMasks_count ? (uint32_t)km.modifierMasks[pin] : 0);
		mix(pin < (uint32_t)km.midiNotes_count ? (uint32_t)km.midiNotes[pin] : 0);
		mix(s.getGamepadMask(pin));
		mix(pin < (uint32_t)cfg.macroIndices_count ? (uint32_t)cfg.macroIndices[pin] : 0);
	}
	return h;
}

void KeypadDisplay::updateLabels(bool force)
{
	const uint32_t hash = labelHash();
	if (!force && hash == lastLabelHash)
		return;
	lastLabelHash = hash;

	Storage& s = Storage::getInstance();
	const InputMode mode = s.getDefaultInputMode();
	const bool nintendo = s.getUseNintendoLayout();
	for (uint32_t pin = 0; pin < CELL_COUNT; pin++)
	{
		labels[pin] = inputLabelForPin(pin, mode, nintendo);
		uppercase(labels[pin]);
		dirty[pin] = true;
	}
}

void KeypadDisplay::updateFooter(bool force)
{
	if (!present)
		return;

	Storage& s = Storage::getInstance();
	std::string text;
	if (s.getBootWindowActive())
		text = "HOLD KEY 1 FOR WEB CONFIG";
	else if (s.GetConfigMode())
		text = std::string("CFG ") + MP2040_WEBCONFIG_IP;
	else
		text = std::string(inputModeName(s.getDefaultInputMode())) +
			"  P" + std::to_string(s.getActiveProfile() + 1);

	if (!force && text == footerText)
		return;
	footerText = text;
	drawFooter();
}

void KeypadDisplay::show()
{
	if (!present)
		return;

	updateLabels(false);
	updateFooter(false);

	for (uint32_t cell = 0; cell < CELL_COUNT; cell++)
	{
		if (!dirty[cell] && pending[cell] == shown[cell])
			continue;
		drawCell(cell, pending[cell]);
	}
}

uint32_t KeypadDisplay::textWidth(const std::string& text, uint8_t scale)
{
	if (text.empty())
		return 0;
	const uint32_t advance = FONT_ADVANCE * scale;
	return (uint32_t)text.size() * advance - scale;
}

void KeypadDisplay::drawText(uint16_t* buf, uint32_t stride, int32_t x, int32_t y,
	const std::string& text, uint8_t scale, uint16_t color)
{
	for (size_t i = 0; i < text.size(); i++)
	{
		const uint8_t ch = (uint8_t)text[i];
		if (ch < FONT_CHAR_OFFSET)
			continue;
		const uint32_t glyphIndex = ch - FONT_CHAR_OFFSET;
		const uint8_t* glyph = &GP_Font_Standard[glyphIndex * FONT_W];
		const int32_t gx = x + (int32_t)i * FONT_ADVANCE * scale;
		for (uint8_t col = 0; col < FONT_W; col++)
		{
			const uint8_t bits = glyph[col];
			for (uint8_t row = 0; row < FONT_H; row++)
			{
				if (!(bits & (1u << row)))
					continue;
				for (uint8_t sy = 0; sy < scale; sy++)
				{
					const int32_t py = y + (int32_t)row * scale + sy;
					if (py < 0)
						continue;
					for (uint8_t sx = 0; sx < scale; sx++)
					{
						const int32_t px = gx + col * scale + sx;
						if (px < 0)
							continue;
						buf[(uint32_t)py * stride + (uint32_t)px] = color;
					}
				}
			}
		}
	}
}

void KeypadDisplay::drawCell(uint32_t cell, uint32_t color888)
{
	const uint32_t row = cell / COLS;
	const uint32_t col = cell % COLS;
	const uint32_t x0 = colX0(col);
	const uint32_t y0 = rowY0(row);
	const uint32_t w = colX1(col) - x0;
	const uint32_t h = rowY1(row) - y0;
	if (w == 0 || h == 0 || w * h > MAX_CELL_PIXELS)
		return;

	const uint16_t fill = swap16(rgb565(color888));
	const uint16_t border = swap16(rgb565(borderColor(color888)));
	for (uint32_t y = 0; y < h; y++)
	{
		uint16_t* line = &pixelBuf[y * w];
		for (uint32_t x = 0; x < w; x++)
			line[x] = fill;
	}
	for (uint32_t y = 0; y < h; y++)
	{
		for (uint32_t x = 0; x < w; x++)
		{
			if (x < BORDER || y < BORDER || x >= w - BORDER || y >= h - BORDER)
				pixelBuf[y * w + x] = border;
		}
	}

	const std::string& label = labels[cell];
	if (!label.empty())
	{
		const uint32_t maxTextW = w - 2 * (BORDER + 3);
		const uint32_t maxTextH = h - 2 * (BORDER + 3);
		uint8_t scale = 5;
		while (scale > 1 &&
		       (textWidth(label, scale) > maxTextW || FONT_H * scale > maxTextH))
			scale--;
		std::string fitted = label;
		if (textWidth(fitted, scale) > maxTextW)
		{
			const uint32_t maxChars = maxTextW / (FONT_ADVANCE * scale);
			if (maxChars == 0)
				fitted.clear();
			else if (fitted.size() > maxChars)
				fitted.resize(maxChars);
		}
		if (!fitted.empty())
		{
			const uint16_t textColor = swap16(luminance(color888) > 140 ? 0x0000 : 0xFFFF);
			const uint32_t tw = textWidth(fitted, scale);
			const uint32_t th = FONT_H * scale;
			const int32_t tx = (int32_t)(w - tw) / 2;
			const int32_t ty = (int32_t)(h - th) / 2;
			drawText(pixelBuf, w, tx, ty, fitted, scale, textColor);
		}
	}

	LCD35::getInstance().setWindow((uint16_t)x0, (uint16_t)y0, (uint16_t)w, (uint16_t)h);
	LCD35::getInstance().writePixels(pixelBuf, w * h);

	shown[cell] = color888;
	dirty[cell] = false;
}

void KeypadDisplay::drawFooter()
{
	if (!present)
		return;

	const uint32_t footerY = LCD_HEIGHT - KEYPAD_FOOTER_HEIGHT;
	const uint16_t bg = swap16(rgb565(RGB(0x10, 0x14, 0x1A)));
	for (uint32_t y = 0; y < KEYPAD_FOOTER_HEIGHT; y++)
	{
		uint16_t* line = &pixelBuf[y * LCD_WIDTH];
		for (uint32_t x = 0; x < LCD_WIDTH; x++)
			line[x] = bg;
	}

	if (!footerText.empty())
	{
		uint8_t scale = 2;
		if (textWidth(footerText, scale) > LCD_WIDTH - 12)
			scale = 1;
		const uint32_t tw = textWidth(footerText, scale);
		const uint32_t th = FONT_H * scale;
		const int32_t tx = (int32_t)(LCD_WIDTH - tw) / 2;
		const int32_t ty = (int32_t)(KEYPAD_FOOTER_HEIGHT - th) / 2;
		const uint16_t color = swap16(rgb565(RGB(0xD8, 0xDE, 0xE9)));
		drawText(pixelBuf, LCD_WIDTH, tx, ty, footerText, scale, color);
	}

	LCD35::getInstance().setWindow(0, (uint16_t)footerY, LCD_WIDTH, KEYPAD_FOOTER_HEIGHT);
	LCD35::getInstance().writePixels(pixelBuf, LCD_WIDTH * KEYPAD_FOOTER_HEIGHT);
}

#endif
