//
// Framebuffer extraction + PNG output for the display simulator.
//
#include "framebuffer.h"

#include <cstdio>
#include <cstring>

#include <zlib.h>

Frame extractFrame(GPGFX& gfx, int w, int h, bool flip, bool invert) {
    Frame frame;
    frame.w = w;
    frame.h = h;
    frame.px.assign((size_t)w * h, 0);

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            uint8_t v = (uint8_t)(gfx.getPixel((uint16_t)x, (uint16_t)y) & 1u);
            if (invert) v ^= 1u;
            int dx = flip ? (w - 1 - x) : x;
            int dy = flip ? (h - 1 - y) : y;
            frame.px[(size_t)dy * w + dx] = v;
        }
    }
    return frame;
}

static void putU32(std::vector<uint8_t>& out, uint32_t v) {
    out.push_back((uint8_t)(v >> 24));
    out.push_back((uint8_t)(v >> 16));
    out.push_back((uint8_t)(v >> 8));
    out.push_back((uint8_t)(v));
}

static void putChunk(std::vector<uint8_t>& png, const char type[4],
                     const std::vector<uint8_t>& data) {
    putU32(png, (uint32_t)data.size());
    size_t crcStart = png.size();
    png.insert(png.end(), type, type + 4);
    png.insert(png.end(), data.begin(), data.end());
    uLong crc = crc32(0L, Z_NULL, 0);
    crc = crc32(crc, png.data() + crcStart, (uInt)(png.size() - crcStart));
    putU32(png, (uint32_t)crc);
}

bool writeMask(const std::string& path, const Frame& frame) {
    if (frame.w <= 0 || frame.h <= 0) return false;
    if (frame.w != 128 || frame.h != 64) return false;
    std::vector<uint8_t> mask((size_t)128 * 64 / 8, 0);
    for (int y = 0; y < frame.h; y++) {
        for (int x = 0; x < frame.w; x++) {
            if (!frame.px[(size_t)y * frame.w + x]) continue;
            size_t byte = (size_t)y * (128 / 8) + (size_t)(x / 8);
            mask[byte] |= (uint8_t)(0x80 >> (x & 7)); // MSB-first
        }
    }
    FILE* f = fopen(path.c_str(), "wb");
    if (f == nullptr) return false;
    size_t written = fwrite(mask.data(), 1, mask.size(), f);
    fclose(f);
    return written == mask.size();
}

bool writePng(const std::string& path, const Frame& frame, int scale,
              uint32_t fg, uint32_t bg) {
    if (frame.w <= 0 || frame.h <= 0 || scale < 1) return false;

    const int outW = frame.w * scale;
    const int outH = frame.h * scale;

    // Raw scanlines: one filter byte (0) then RGB triples.
    std::vector<uint8_t> raw;
    raw.reserve((size_t)outH * (1 + outW * 3));
    const uint8_t fr = (fg >> 16) & 0xFF, fgn = (fg >> 8) & 0xFF, fb = fg & 0xFF;
    const uint8_t br = (bg >> 16) & 0xFF, bgn = (bg >> 8) & 0xFF, bb = bg & 0xFF;
    for (int y = 0; y < outH; y++) {
        raw.push_back(0);
        int sy = y / scale;
        for (int x = 0; x < outW; x++) {
            int sx = x / scale;
            uint8_t v = frame.px[(size_t)sy * frame.w + sx];
            raw.push_back(v ? fr : br);
            raw.push_back(v ? fgn : bgn);
            raw.push_back(v ? fb : bb);
        }
    }

    uLongf compLen = compressBound((uLong)raw.size());
    std::vector<uint8_t> comp(compLen);
    if (compress2(comp.data(), &compLen, raw.data(), (uLong)raw.size(), 9) != Z_OK)
        return false;
    comp.resize(compLen);

    std::vector<uint8_t> png = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};

    std::vector<uint8_t> ihdr;
    putU32(ihdr, (uint32_t)outW);
    putU32(ihdr, (uint32_t)outH);
    ihdr.push_back(8);   // bit depth
    ihdr.push_back(2);   // color type: truecolor RGB
    ihdr.push_back(0);   // compression
    ihdr.push_back(0);   // filter
    ihdr.push_back(0);   // interlace
    putChunk(png, "IHDR", ihdr);
    putChunk(png, "IDAT", comp);
    putChunk(png, "IEND", {});

    FILE* f = fopen(path.c_str(), "wb");
    if (f == nullptr) return false;
    size_t written = fwrite(png.data(), 1, png.size(), f);
    fclose(f);
    return written == png.size();
}

std::vector<uint32_t> toArgbPixels(const Frame& frame, int scale,
                                   uint32_t fg, uint32_t bg, uint32_t alpha) {
    std::vector<uint32_t> out;
    if (frame.w <= 0 || frame.h <= 0 || scale < 1) return out;
    const int outW = frame.w * scale;
    const int outH = frame.h * scale;
    out.resize((size_t)outW * outH);
    // ARGB8888 with non-premultiplied alpha.
    uint32_t f = (alpha << 24) | (fg & 0x00FFFFFF);
    uint32_t b = (alpha << 24) | (bg & 0x00FFFFFF);
    for (int y = 0; y < outH; y++) {
        int sy = y / scale;
        for (int x = 0; x < outW; x++) {
            int sx = x / scale;
            uint8_t v = frame.px[(size_t)sy * frame.w + sx];
            out[(size_t)y * outW + x] = v ? f : b;
        }
    }
    return out;
}
