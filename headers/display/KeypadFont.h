#ifndef _KEYPAD_FONT_H_
#define _KEYPAD_FONT_H_

#include <stdint.h>

// Generated bitmap font/icon data (see tools/gen_keypad_fonts.py).
//
// Font glyphs are 8-bit alpha bitmaps for ASCII 32..126. `xoff`/`yoff` place
// the glyph's top-left relative to the text pen and the line top; `advance` is
// the pen step after the glyph.
struct KeypadGlyph
{
	uint32_t offset; // byte offset into the font's alpha data
	int16_t xoff;
	int16_t yoff;
	uint16_t width;
	uint16_t height;
	uint8_t advance;
};

struct KeypadFont
{
	uint8_t lineHeight;
	const KeypadGlyph* glyphs;
	const uint8_t* alpha;
};

#endif
