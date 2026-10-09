//
// Framebuffer extraction + PNG output for the display simulator.
//
#ifndef _SIM_FRAMEBUFFER_H_
#define _SIM_FRAMEBUFFER_H_

#include <cstdint>
#include <string>
#include <vector>

#include "GPGFX.h"

// A monochrome frame: one byte per pixel, 0 or 1.
struct Frame {
    int w = 0;
    int h = 0;
    std::vector<uint8_t> px;
};

// Read the renderer's framebuffer pixel-for-pixel (via the public getPixel),
// then optionally rotate 180 deg (flip) and/or invert, mirroring the panel's
// hardware remap options.
Frame extractFrame(GPGFX& gfx, int w, int h, bool flip, bool invert);

// Write the frame as an 8-bit RGB PNG, nearest-neighbour scaled by `scale`.
// `fg`/`bg` are 0xRRGGBB colors for lit/unlit pixels.
bool writePng(const std::string& path, const Frame& frame, int scale,
              uint32_t fg, uint32_t bg);

// Write the frame as a PNG (see writePng) or as a packed 1bpp bitmask:
// 128 bits/row, MSB-first, 64 rows -> 1024 bytes. Used by the screenshot
// scripts to build pixel-accurate SVG overlays.
bool writeMask(const std::string& path, const Frame& frame);

// RGBA pixels for a screen surface (SDL). Lit pixels use `fg`, unlit `bg`,
// nearest-neighbour scaled by `scale`.
std::vector<uint32_t> toArgbPixels(const Frame& frame, int scale,
                                   uint32_t fg, uint32_t bg, uint32_t alpha);

#endif
