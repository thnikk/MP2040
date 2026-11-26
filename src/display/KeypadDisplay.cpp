#include "display/KeypadDisplay.h"

#include "BoardConfig.h"
#include "display/LCD35.h"
#if defined(MP2040_KEYPAD_ICONS) && MP2040_KEYPAD_ICONS
#include "display/fonts/FontAwesomeSolid40.h"
#endif
#include "display/fonts/KeypadFontNunito16.h"
#include "display/fonts/KeypadFontNunito20.h"
#include "display/fonts/KeypadFontNunito28.h"
#include "display/inputlabels.h"
#include "helper.h"
#include "leds/LedLayout.h"
#include "storagemanager.h"
#include "touch/TouchKeypad.h"

#include "pico/time.h"

#include <math.h>

#if defined(MATRIX_TOUCH_KEYPAD) && MATRIX_TOUCH_KEYPAD

namespace
{
constexpr uint32_t CELL_GAP = 2;
constexpr float CELL_RADIUS = 10.0f;
constexpr uint32_t LABEL_PAD = 2;

// Boot-window function icons (Font Awesome Free solid).
constexpr uint32_t BOOT_WEB_CONFIG_ICON = 0xF013; // gear
constexpr uint32_t BOOT_FPS_ICON = 0xF188;        // bug

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

inline uint32_t blend(uint32_t a, uint32_t b, uint32_t cover)
{
	const uint32_t ar = (a >> 16) & 0xFF, ag = (a >> 8) & 0xFF, ab = a & 0xFF;
	const uint32_t br = (b >> 16) & 0xFF, bg = (b >> 8) & 0xFF, bb = b & 0xFF;
	const uint32_t r = ar + ((br - ar) * cover) / 255;
	const uint32_t g = ag + ((bg - ag) * cover) / 255;
	const uint32_t bl = ab + ((bb - ab) * cover) / 255;
	return RGB((uint8_t)r, (uint8_t)g, (uint8_t)bl);
}

// Blend one 565 buffer pixel with a color, for rounded-edge coverage.
void blendPixel(uint16_t* buf, uint32_t index, uint32_t color, uint32_t cover)
{
	if (cover == 0)
		return;
	if (cover >= 255)
	{
		buf[index] = swap16(rgb565(color));
		return;
	}
	const uint16_t old565 = swap16(buf[index]);
	const uint32_t old888 = RGB(
		(uint8_t)(((old565 >> 11) & 0x1F) * 255 / 31),
		(uint8_t)(((old565 >> 5) & 0x3F) * 255 / 63),
		(uint8_t)((old565 & 0x1F) * 255 / 31));
	buf[index] = swap16(rgb565(blend(old888, color, cover)));
}

// Filled rounded rectangle with horizontal anti-aliasing. The vertical edges
// are 1px-stepped, which is imperceptible at the 10px radius used here.
void fillRounded(uint16_t* buf, uint32_t w, uint32_t h,
	float x0, float y0, float x1, float y1, float radius, uint32_t color)
{
	const int32_t ys = (int32_t)ceilf(y0);
	const int32_t ye = (int32_t)ceilf(y1) - 1;
	for (int32_t y = ys; y <= ye; y++)
	{
		if (y < 0 || y >= (int32_t)h)
			continue;
		const float dy = fminf((float)y - y0, y1 - (float)y);
		float inset = 0.0f;
		if (dy < radius)
		{
			const float d = radius - dy;
			inset = radius - sqrtf(radius * radius - d * d);
		}
		const float left = x0 + inset;
		const float right = x1 - inset;
		if (right <= left)
			continue;

		int32_t fullStart = (int32_t)ceilf(left);
		int32_t fullEnd = (int32_t)floorf(right) - 1;
		if (fullStart < 0) fullStart = 0;
		if (fullEnd >= (int32_t)w) fullEnd = (int32_t)w - 1;
		for (int32_t x = fullStart; x <= fullEnd; x++)
			buf[(uint32_t)y * w + x] = swap16(rgb565(color));

		// Partial pixels at the curved left/right boundary.
		const int32_t li = (int32_t)floorf(left);
		const float lcov = 1.0f - (left - (float)li);
		if (li >= 0 && li < (int32_t)w && lcov > 0.0f)
			blendPixel(buf, (uint32_t)y * w + li, color, (uint32_t)(lcov * 255.0f));
		const int32_t ri = (int32_t)floorf(right);
		const float rcov = right - (float)ri;
		if (ri >= 0 && ri < (int32_t)w && rcov > 0.0f)
			blendPixel(buf, (uint32_t)y * w + ri, color, (uint32_t)(rcov * 255.0f));
	}
}

uint32_t textWidth(const std::string& text, const KeypadFont& font)
{
	uint32_t width = 0;
	for (char c : text)
	{
		const uint8_t ch = (uint8_t)c;
		if (ch < 32 || ch > 126)
			continue;
		width += font.glyphs[ch - 32].advance;
	}
	return width;
}

struct InkBounds
{
	int32_t minX;
	int32_t maxX;
	int32_t minY;
	int32_t maxY;
};

// Visual bounds of the drawn ink relative to the text pen/line origin. Used
// for the mode badge so its padding is even around the glyphs, not the line box.
InkBounds textInkBounds(const std::string& text, const KeypadFont& font)
{
	int32_t penX = 0;
	int32_t minX = 0x7FFFFFFF, maxX = -0x7FFFFFFF;
	int32_t minY = 0x7FFFFFFF, maxY = -0x7FFFFFFF;
	for (char c : text)
	{
		const uint8_t ch = (uint8_t)c;
		if (ch < 32 || ch > 126)
			continue;
		const KeypadGlyph& glyph = font.glyphs[ch - 32];
		if (glyph.width != 0 && glyph.height != 0)
		{
			const int32_t gx0 = penX + glyph.xoff;
			const int32_t gx1 = gx0 + glyph.width;
			const int32_t gy0 = glyph.yoff;
			const int32_t gy1 = gy0 + glyph.height;
			if (gx0 < minX) minX = gx0;
			if (gx1 > maxX) maxX = gx1;
			if (gy0 < minY) minY = gy0;
			if (gy1 > maxY) maxY = gy1;
		}
		penX += glyph.advance;
	}
	if (minX > maxX || minY > maxY)
		return {0, 0, 0, 0};
	return {minX, maxX, minY, maxY};
}

void drawText(uint16_t* buf, uint32_t stride, uint32_t maxW, uint32_t maxH,
	int32_t x, int32_t y, const std::string& text, const KeypadFont& font,
	uint32_t textColor, uint32_t fillColor)
{
	int32_t penX = x;
	for (char c : text)
	{
		const uint8_t ch = (uint8_t)c;
		if (ch < 32 || ch > 126)
			continue;
		const KeypadGlyph& glyph = font.glyphs[ch - 32];
		for (uint16_t gy = 0; gy < glyph.height; gy++)
		{
			const int32_t py = y + glyph.yoff + gy;
			if (py < 0 || py >= (int32_t)maxH)
				continue;
			for (uint16_t gx = 0; gx < glyph.width; gx++)
			{
				const int32_t px = penX + glyph.xoff + gx;
				if (px < 0 || px >= (int32_t)maxW)
					continue;
				const uint8_t a = font.alpha[glyph.offset + (uint32_t)gy * glyph.width + gx];
				if (a == 0)
					continue;
				const uint32_t color = a == 255
					? textColor : blend(fillColor, textColor, a);
				buf[(uint32_t)py * stride + (uint32_t)px] = swap16(rgb565(color));
			}
		}
		penX += glyph.advance;
	}
}

struct FontChoice
{
	const KeypadFont* font;
	std::string text;
};

FontChoice chooseFont(const std::string& text, uint32_t maxW, uint32_t maxH)
{
	static const KeypadFont* const candidates[] = {
		&KeypadFontNunito28, &KeypadFontNunito20, &KeypadFontNunito16
	};
	for (const KeypadFont* font : candidates)
	{
		if (font->lineHeight <= maxH && textWidth(text, *font) <= maxW)
			return { font, text };
	}
	// Last resort: truncate at the smallest size.
	std::string fitted = text;
	const KeypadFont* font = &KeypadFontNunito16;
	while (!fitted.empty() && textWidth(fitted, *font) > maxW)
		fitted.pop_back();
	return { font, fitted };
}

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

// Nord-palette colors for the top-bar input-mode badge. These are softer than
// the status LED's per-mode colors, which stay unchanged for the LED itself.
uint32_t inputModeColor(InputMode mode)
{
	switch (mode)
	{
		case INPUT_MODE_MIDI: return 0xD08770;        // Nord12 orange
		case INPUT_MODE_XINPUT: return 0xA3BE8C;      // Nord14 green
		case INPUT_MODE_SWITCH_PRO: return 0xBF616A;  // Nord11 red
		case INPUT_MODE_XBOX_ONE: return 0xB48EAD;    // Nord15 purple
		case INPUT_MODE_PS3: return 0x5E81AC;         // Nord10 dark blue
		case INPUT_MODE_PS4: return 0x88C0D0;         // Nord8 light blue
		case INPUT_MODE_PS5: return 0x81A1C1;         // Nord9 blue
		case INPUT_MODE_KEYBOARD:
		default: return 0xEBCB8B;                     // Nord13 yellow
	}
}

#if defined(MP2040_KEYPAD_ICONS) && MP2040_KEYPAD_ICONS
const KeypadIcon* findIcon(uint32_t codepoint)
{
	int32_t lo = 0;
	int32_t hi = (int32_t)FONTAWESOME_SOLID_COUNT - 1;
	while (lo <= hi)
	{
		const int32_t mid = lo + (hi - lo) / 2;
		const KeypadIcon& icon = FontAwesomeSolidIcons[mid];
		if (icon.codepoint == codepoint)
			return &icon;
		if (icon.codepoint < codepoint)
			lo = mid + 1;
		else
			hi = mid - 1;
	}
	return nullptr;
}

bool drawIcon(uint16_t* buf, uint32_t w, uint32_t h,
	uint32_t left, uint32_t top, uint32_t width, uint32_t height,
	uint32_t codepoint, uint32_t color, uint32_t fillColor)
{
	const KeypadIcon* icon = findIcon(codepoint);
	if (icon == nullptr || width < FONTAWESOME_ICON_SIZE || height < FONTAWESOME_ICON_SIZE)
		return false;
	const uint32_t x0 = left + (width - FONTAWESOME_ICON_SIZE) / 2;
	const uint32_t y0 = top + (height - FONTAWESOME_ICON_SIZE) / 2;
	for (uint32_t y = 0; y < FONTAWESOME_ICON_SIZE; y++)
	{
		for (uint32_t x = 0; x < FONTAWESOME_ICON_SIZE; x++)
		{
			const uint32_t index = icon->offset + y * FONTAWESOME_ICON_ROW_BYTES + (x >> 1);
			const uint8_t packed = FontAwesomeSolidMask[index];
			const uint8_t alpha = (x & 1) ? (packed & 0x0F) : (packed >> 4);
			if (alpha == 0)
				continue;
			const uint32_t blended = alpha == 15
				? color : blend(fillColor, color, alpha * 17);
			buf[(y0 + y) * w + x0 + x] = swap16(rgb565(blended));
		}
	}
	return true;
}
#endif

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
	mix(s.getBootWindowActive() ? 1u : 0u);
	const Config& cfg = s.getConfig();
	const KeyMapping& km = s.getKeyMapping();
	for (uint32_t pin = 0; pin < CELL_COUNT; pin++)
	{
		mix(pin < (uint32_t)km.keycodes_count ? (uint32_t)km.keycodes[pin] : 0);
		mix(pin < (uint32_t)km.modifierMasks_count ? (uint32_t)km.modifierMasks[pin] : 0);
		mix(pin < (uint32_t)km.midiNotes_count ? (uint32_t)km.midiNotes[pin] : 0);
		mix(s.getGamepadMask(pin));
		mix(pin < (uint32_t)cfg.macroIndices_count ? (uint32_t)cfg.macroIndices[pin] : 0);
#if defined(MP2040_KEYPAD_ICONS) && MP2040_KEYPAD_ICONS
		mix(pin < (uint32_t)km.iconIds_count ? (uint32_t)km.iconIds[pin] : 0);
#endif
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
	const KeyMapping& km = s.getKeyMapping();
	const InputMode mode = s.getDefaultInputMode();
	const bool nintendo = s.getUseNintendoLayout();
	const bool booting = s.getBootWindowActive();
	for (uint32_t pin = 0; pin < CELL_COUNT; pin++)
	{
		labels[pin] = inputLabelForPin(pin, mode, nintendo);
		uppercase(labels[pin]);
#if defined(MP2040_KEYPAD_ICONS) && MP2040_KEYPAD_ICONS
		iconIds[pin] = pin < (uint32_t)km.iconIds_count ? km.iconIds[pin] : 0;
#else
		iconIds[pin] = 0;
#endif
		if (booting)
		{
			// Only the two boot functions have a visible key: web config on
			// the first key and the FPS toggle on the third.
			labels[pin].clear();
			iconIds[pin] = 0;
#if defined(MP2040_KEYPAD_ICONS) && MP2040_KEYPAD_ICONS
			if (pin == (uint32_t)PIN_WEBCONFIG)
				iconIds[pin] = BOOT_WEB_CONFIG_ICON;
			else if (pin == (uint32_t)KEYPAD_FPS_KEY)
				iconIds[pin] = BOOT_FPS_ICON;
#endif
		}
		dirty[pin] = true;
	}
}

void KeypadDisplay::updateFooter(bool force)
{
	if (!present)
		return;

	Storage& s = Storage::getInstance();
	std::string left;
	std::string right;
	std::string center;
	if (s.getBootWindowActive())
		left = "HOLD KEY 1 FOR WEB CONFIG";
	else if (s.GetConfigMode())
		left = std::string("CFG ") + MP2040_WEBCONFIG_IP;
	else
	{
		left = inputModeName(s.getDefaultInputMode());
		right = "Profile " + std::to_string(s.getActiveProfile() + 1);
		if (s.getShowFps())
			center = std::to_string(renderFps) + " FPS  " +
				std::to_string(inputFps) + " Hz";
	}

	if (!force && left == footerLeft && right == footerRight &&
		center == footerCenter)
		return;
	footerLeft = left;
	footerRight = right;
	footerCenter = center;
	drawFooter();
}

void KeypadDisplay::show()
{
	if (!present)
		return;

	if (Storage::getInstance().getShowFps())
	{
		const uint32_t now = to_ms_since_boot(get_absolute_time());
		++fpsFrames;
		if (fpsLastMs == 0)
		{
			fpsLastMs = now;
			lastScanCount = TouchKeypad::getInstance().getScanCount();
		}
		else if (now - fpsLastMs >= 1000)
		{
			const uint32_t elapsed = now - fpsLastMs;
			renderFps = fpsFrames * 1000 / elapsed;
			const uint32_t scans = TouchKeypad::getInstance().getScanCount();
			inputFps = (scans - lastScanCount) * 1000 / elapsed;
			lastScanCount = scans;
			fpsFrames = 0;
			fpsLastMs = now;
		}
	}

	updateLabels(false);
	updateFooter(false);

	for (uint32_t cell = 0; cell < CELL_COUNT; cell++)
	{
		if (!dirty[cell] && pending[cell] == shown[cell])
			continue;
		drawCell(cell, pending[cell]);
	}
}

void KeypadDisplay::drawCell(uint32_t cell, uint32_t color888)
{
	const uint32_t row = cell / COLS;
	const uint32_t col = cell % COLS;
	const uint32_t x0 = col * LCD_WIDTH / COLS;
	const uint32_t areaY0 = row * (LCD_HEIGHT - KEYPAD_FOOTER_HEIGHT) / ROWS;
	const uint32_t areaY1 = (row + 1) * (LCD_HEIGHT - KEYPAD_FOOTER_HEIGHT) / ROWS;
	const uint32_t y0 = KEYPAD_FOOTER_HEIGHT + areaY0;
	const uint32_t w = (col + 1) * LCD_WIDTH / COLS - x0;
	const uint32_t h = areaY1 - areaY0;
	if (w == 0 || h == 0 || w * h > MAX_CELL_PIXELS)
		return;

	// Screen background (the gap around the rounded button).
	const uint16_t screenBg = swap16(rgb565(RGB(0x1C, 0x1F, 0x26)));
	for (uint32_t i = 0; i < w * h; i++)
		pixelBuf[i] = screenBg;

	const float innerX0 = (float)CELL_GAP;
	const float innerY0 = (float)CELL_GAP;
	const float innerX1 = (float)w - CELL_GAP;
	const float innerY1 = (float)h - CELL_GAP;

	fillRounded(pixelBuf, w, h, innerX0, innerY0, innerX1, innerY1,
		CELL_RADIUS, color888);

	const uint32_t innerW = (uint32_t)(innerX1 - innerX0);
	const uint32_t innerH = (uint32_t)(innerY1 - innerY0);
	const uint32_t maxTextW = innerW > 2 * LABEL_PAD ? innerW - 2 * LABEL_PAD : innerW;
	const uint32_t maxTextH = innerH > 2 * LABEL_PAD ? innerH - 2 * LABEL_PAD : innerH;
	const uint32_t textColor = luminance(color888) > 140 ? 0x000000 : 0xFFFFFF;

	const std::string& label = labels[cell];
	bool drewIcon = false;
#if defined(MP2040_KEYPAD_ICONS) && MP2040_KEYPAD_ICONS
	if (iconIds[cell] != 0)
	{
		drewIcon = drawIcon(pixelBuf, w, h,
			(uint32_t)innerX0, (uint32_t)innerY0, innerW, innerH,
			iconIds[cell], textColor, color888);
	}
#endif
	if (!drewIcon && !label.empty())
	{
		FontChoice choice = chooseFont(label, maxTextW, maxTextH);
		if (!choice.text.empty())
		{
			const uint32_t tw = textWidth(choice.text, *choice.font);
			const uint32_t th = choice.font->lineHeight;
			const int32_t tx = (int32_t)innerX0 + (int32_t)(innerW - tw) / 2;
			const int32_t ty = (int32_t)innerY0 + (int32_t)(innerH - th) / 2;
			drawText(pixelBuf, w, w, h, tx, ty, choice.text, *choice.font,
				textColor, color888);
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

	const uint32_t footerY = 0;
	const uint32_t bg888 = RGB(0x1C, 0x1F, 0x26);
	const uint16_t bg = swap16(rgb565(bg888));
	for (uint32_t y = 0; y < KEYPAD_FOOTER_HEIGHT; y++)
	{
		uint16_t* line = &pixelBuf[y * LCD_WIDTH];
		for (uint32_t x = 0; x < LCD_WIDTH; x++)
			line[x] = bg;
	}

	// Floating status bar, inset from the screen edges.
	const uint32_t barColor = RGB(0x2B, 0x30, 0x3B);
	constexpr float BAR_MARGIN = 4.0f;
	constexpr float BAR_RADIUS = 12.0f;
	fillRounded(pixelBuf, LCD_WIDTH, KEYPAD_FOOTER_HEIGHT,
		BAR_MARGIN, BAR_MARGIN, (float)LCD_WIDTH - BAR_MARGIN,
		(float)KEYPAD_FOOTER_HEIGHT - BAR_MARGIN, BAR_RADIUS, barColor);

	const uint32_t textColor = RGB(0xD8, 0xDE, 0xE9);
	const uint32_t th = KeypadFontNunito16.lineHeight;
	const int32_t ty = (int32_t)(KEYPAD_FOOTER_HEIGHT - th) / 2;
	constexpr int32_t MODE_PAD = 6;
	constexpr float MODE_RADIUS = 8.0f;

	if (!footerRight.empty())
	{
		// Input mode and profile badges. They share a common height and outer
		// margins; each is sized from its glyph ink bounds so the horizontal
		// padding is even.
		const InkBounds mb = textInkBounds(footerLeft, KeypadFontNunito16);
		const InkBounds pb = textInkBounds(footerRight, KeypadFontNunito16);
		const int32_t modeInkW = mb.maxX - mb.minX;
		const int32_t modeInkH = mb.maxY - mb.minY;
		const int32_t profileInkW = pb.maxX - pb.minX;
		const int32_t profileInkH = pb.maxY - pb.minY;
		const int32_t modeBoxH = modeInkH + 2 * MODE_PAD;
		const int32_t profileBoxH = profileInkH + 2 * MODE_PAD;
		const int32_t boxH = modeBoxH > profileBoxH ? modeBoxH : profileBoxH;
		const float boxY = ((float)KEYPAD_FOOTER_HEIGHT - (float)boxH) / 2.0f;
		const float boxX = boxY;

		const uint32_t modeColor =
			inputModeColor(Storage::getInstance().getDefaultInputMode());
		const uint32_t modeTextColor =
			luminance(modeColor) > 140 ? 0x000000 : 0xFFFFFF;
		const uint32_t profileColor = RGB(0x4C, 0x56, 0x6A); // Nord3
		const uint32_t profileTextColor =
			luminance(profileColor) > 140 ? 0x000000 : 0xFFFFFF;

		const int32_t modeBoxW = modeInkW + 2 * MODE_PAD;
		fillRounded(pixelBuf, LCD_WIDTH, KEYPAD_FOOTER_HEIGHT,
			boxX, boxY, boxX + (float)modeBoxW, boxY + (float)boxH,
			MODE_RADIUS, modeColor);
		drawText(pixelBuf, LCD_WIDTH, LCD_WIDTH, KEYPAD_FOOTER_HEIGHT,
			(int32_t)(boxX - (float)mb.minX + (float)MODE_PAD),
			(int32_t)(boxY - (float)mb.minY +
				(float)(boxH - modeInkH) / 2.0f),
			footerLeft, KeypadFontNunito16, modeTextColor, modeColor);

		const int32_t profileBoxW = profileInkW + 2 * MODE_PAD;
		const float profileX =
			(float)LCD_WIDTH - boxX - (float)profileBoxW;
		fillRounded(pixelBuf, LCD_WIDTH, KEYPAD_FOOTER_HEIGHT,
			profileX, boxY, profileX + (float)profileBoxW, boxY + (float)boxH,
			MODE_RADIUS, profileColor);
		drawText(pixelBuf, LCD_WIDTH, LCD_WIDTH, KEYPAD_FOOTER_HEIGHT,
			(int32_t)(profileX - (float)pb.minX + (float)MODE_PAD),
			(int32_t)(boxY - (float)pb.minY +
				(float)(boxH - profileInkH) / 2.0f),
			footerRight, KeypadFontNunito16, profileTextColor, profileColor);

		if (!footerCenter.empty())
		{
			const InkBounds cb = textInkBounds(footerCenter, KeypadFontNunito16);
			const int32_t cx = (int32_t)(LCD_WIDTH / 2) -
				(cb.minX + cb.maxX) / 2;
			const int32_t cy = (int32_t)(KEYPAD_FOOTER_HEIGHT / 2) -
				(cb.minY + cb.maxY) / 2;
			drawText(pixelBuf, LCD_WIDTH, LCD_WIDTH, KEYPAD_FOOTER_HEIGHT,
				cx, cy, footerCenter, KeypadFontNunito16, textColor, barColor);
		}
	}
	else if (!footerLeft.empty())
	{
		// Boot/config prompts stay centered.
		const uint32_t tw = textWidth(footerLeft, KeypadFontNunito16);
		drawText(pixelBuf, LCD_WIDTH, LCD_WIDTH, KEYPAD_FOOTER_HEIGHT,
			(int32_t)(LCD_WIDTH - tw) / 2, ty, footerLeft,
			KeypadFontNunito16, textColor, barColor);
	}

	LCD35::getInstance().setWindow(0, (uint16_t)footerY, LCD_WIDTH, KEYPAD_FOOTER_HEIGHT);
	LCD35::getInstance().writePixels(pixelBuf, LCD_WIDTH * KEYPAD_FOOTER_HEIGHT);
}

#endif
