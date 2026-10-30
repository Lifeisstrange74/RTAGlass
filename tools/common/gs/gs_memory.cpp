#include "gs_memory.h"

#include <string.h>

#include <algorithm>

namespace gs {
namespace {

// Block order inside a page, indexed [block row][block column].
const uint8_t kBlock32[4][8] = {
    {0, 1, 4, 5, 16, 17, 20, 21},
    {2, 3, 6, 7, 18, 19, 22, 23},
    {8, 9, 12, 13, 24, 25, 28, 29},
    {10, 11, 14, 15, 26, 27, 30, 31},
};
const uint8_t kBlock16[8][4] = {
    {0, 2, 8, 10}, {1, 3, 9, 11}, {4, 6, 12, 14}, {5, 7, 13, 15},
    {16, 18, 24, 26}, {17, 19, 25, 27}, {20, 22, 28, 30}, {21, 23, 29, 31},
};
// PSMT8 pages use the PSMCT32 block order, PSMT4 pages the PSMCT16 one.
#define kBlock8 kBlock32
#define kBlock4 kBlock16

// Pixel order inside a block: word (CT32), halfword (CT16), byte (T8) or nibble (T4) index.
uint8_t gColumn32[8][8];
uint8_t gColumn16[8][16];
uint8_t gColumn8[16][16];
uint16_t gColumn4[16][32];

// Blocks are 4 columns stacked vertically. Within a column, CT32 stores two rows of 8 pixels as words
// {0,1,4,5,8,9,12,13} / {2,3,6,7,10,11,14,15}; the narrower formats interleave further. The tables are
// built from that structure rather than typed in (checked against reference data in tests/gs).
void BuildTables() {
    static bool built = false;
    if (built) return;
    built = true;
    static const int kCt32Row[2][8] = {{0, 1, 4, 5, 8, 9, 12, 13}, {2, 3, 6, 7, 10, 11, 14, 15}};
    for (int y = 0; y < 8; ++y)
        for (int x = 0; x < 8; ++x) gColumn32[y][x] = (uint8_t)((y >> 1) * 16 + kCt32Row[y & 1][x]);

    // CT16: a column is 16x2 halfwords; halfword pairs share CT32 word slots, low half = left 8 pixels.
    static const int kCt16Row[2][16] = {
        {0, 2, 8, 10, 16, 18, 24, 26, 1, 3, 9, 11, 17, 19, 25, 27},
        {4, 6, 12, 14, 20, 22, 28, 30, 5, 7, 13, 15, 21, 23, 29, 31}};
    for (int y = 0; y < 8; ++y)
        for (int x = 0; x < 16; ++x) gColumn16[y][x] = (uint8_t)((y >> 1) * 32 + kCt16Row[y & 1][x]);

    // T8: a column is 16x4 bytes; even columns use this pattern. Odd columns swap the patterns of the
    // upper and lower row pairs, but the byte-in-word offset (+1 on rows 2-3) stays with the row pair.
    static const int kT8Rows[4][16] = {
        {0, 4, 16, 20, 32, 36, 48, 52, 2, 6, 18, 22, 34, 38, 50, 54},
        {8, 12, 24, 28, 40, 44, 56, 60, 10, 14, 26, 30, 42, 46, 58, 62},
        {33, 37, 49, 53, 1, 5, 17, 21, 35, 39, 51, 55, 3, 7, 19, 23},
        {41, 45, 57, 61, 9, 13, 25, 29, 43, 47, 59, 63, 11, 15, 27, 31}};
    for (int y = 0; y < 16; ++y) {
        int col = y >> 2, r = y & 3;
        for (int x = 0; x < 16; ++x) {
            int v = (col & 1) ? kT8Rows[r ^ 2][x] + (r < 2 ? -1 : 1) : kT8Rows[r][x];
            gColumn8[y][x] = (uint8_t)(col * 64 + v);
        }
    }

    // T4: a column is 32x4 nibbles, same idea with 4 groups per row.
    static const int kT4Rows[4][32] = {
        {0, 8, 32, 40, 64, 72, 96, 104, 2, 10, 34, 42, 66, 74, 98, 106,
         4, 12, 36, 44, 68, 76, 100, 108, 6, 14, 38, 46, 70, 78, 102, 110},
        {16, 24, 48, 56, 80, 88, 112, 120, 18, 26, 50, 58, 82, 90, 114, 122,
         20, 28, 52, 60, 84, 92, 116, 124, 22, 30, 54, 62, 86, 94, 118, 126},
        {65, 73, 97, 105, 1, 9, 33, 41, 67, 75, 99, 107, 3, 11, 35, 43,
         69, 77, 101, 109, 5, 13, 37, 45, 71, 79, 103, 111, 7, 15, 39, 47},
        {81, 89, 113, 121, 17, 25, 49, 57, 83, 91, 115, 123, 19, 27, 51, 59,
         85, 93, 117, 125, 21, 29, 53, 61, 87, 95, 119, 127, 23, 31, 55, 63}};
    for (int y = 0; y < 16; ++y) {
        int col = y >> 2, r = y & 3;
        for (int x = 0; x < 32; ++x) {
            int v = (col & 1) ? kT4Rows[r ^ 2][x] + (r < 2 ? -1 : 1) : kT4Rows[r][x];
            gColumn4[y][x] = (uint16_t)(col * 128 + v);
        }
    }
}

const uint32_t kMask = LocalMemory::kSize - 1;

// Byte offsets (or nibble offsets for T4) of a pixel.
inline uint32_t Addr32(uint32_t bp, uint32_t bw, int x, int y) {
    uint32_t page = (uint32_t)(y >> 5) * bw + (uint32_t)(x >> 6);
    uint32_t block = bp + page * 32 + kBlock32[(y >> 3) & 3][(x >> 3) & 7];
    return (block * 256 + gColumn32[y & 7][x & 7] * 4) & kMask;
}
inline uint32_t Addr16(uint32_t bp, uint32_t bw, int x, int y) {
    uint32_t page = (uint32_t)(y >> 6) * bw + (uint32_t)(x >> 6);
    uint32_t block = bp + page * 32 + kBlock16[(y >> 3) & 7][(x >> 4) & 3];
    return (block * 256 + gColumn16[y & 7][x & 15] * 2) & kMask;
}
inline uint32_t Addr8(uint32_t bp, uint32_t bw, int x, int y) {
    uint32_t pagesPerRow = bw >> 1 ? bw >> 1 : 1;
    uint32_t page = (uint32_t)(y >> 6) * pagesPerRow + (uint32_t)(x >> 7);
    uint32_t block = bp + page * 32 + kBlock8[(y >> 4) & 3][(x >> 4) & 7];
    return (block * 256 + gColumn8[y & 15][x & 15]) & kMask;
}
inline uint32_t Addr4(uint32_t bp, uint32_t bw, int x, int y) {   // nibble address
    uint32_t pagesPerRow = bw >> 1 ? bw >> 1 : 1;
    uint32_t page = (uint32_t)(y >> 7) * pagesPerRow + (uint32_t)(x >> 7);
    uint32_t block = bp + page * 32 + kBlock4[(y >> 4) & 7][(x >> 5) & 3];
    return (block * 512 + gColumn4[y & 15][x & 31]) & (kMask * 2 + 1);
}

}  // namespace

uint64_t HashUpload(const Transfer& t, const uint8_t* data) {
    uint64_t v = 0xCBF29CE484222325ull;
    uint32_t head[3] = {t.dpsm, t.w, t.h};
    const uint8_t* h = (const uint8_t*)head;
    for (size_t i = 0; i < sizeof(head); ++i) { v ^= h[i]; v *= 0x100000001B3ull; }
    uint32_t n = t.TotalBytes();
    for (uint32_t i = 0; i < n; ++i) { v ^= data[i]; v *= 0x100000001B3ull; }
    return v;
}

Tex0 Tex0::From(uint64_t v) {
    Tex0 t;
    t.tbp0 = (uint32_t)(v & 0x3FFF);
    t.tbw = (uint32_t)((v >> 14) & 0x3F);
    t.psm = (uint32_t)((v >> 20) & 0x3F);
    t.tw = (uint32_t)((v >> 26) & 0xF);
    t.th = (uint32_t)((v >> 30) & 0xF);
    t.tcc = (uint32_t)((v >> 34) & 1);
    t.tfx = (uint32_t)((v >> 35) & 3);
    t.cbp = (uint32_t)((v >> 37) & 0x3FFF);
    t.cpsm = (uint32_t)((v >> 51) & 0xF);
    t.csm = (uint32_t)((v >> 55) & 1);
    t.csa = (uint32_t)((v >> 56) & 0x1F);
    t.cld = (uint32_t)((v >> 61) & 7);
    return t;
}

Texa Texa::From(uint64_t v) {
    Texa t;
    t.ta0 = (uint8_t)(v & 0xFF);
    t.aem = ((v >> 15) & 1) != 0;
    t.ta1 = (uint8_t)((v >> 32) & 0xFF);
    return t;
}

uint32_t Expand16(uint32_t c, const Texa& texa) {
    uint32_t r = (c & 0x1F) << 3, g = ((c >> 5) & 0x1F) << 3, b = ((c >> 10) & 0x1F) << 3;
    uint32_t a = (c & 0x8000) ? texa.ta1 : ((texa.aem && (c & 0x7FFF) == 0) ? 0 : texa.ta0);
    return r | (g << 8) | (b << 16) | (a << 24);
}

LocalMemory::LocalMemory() {
    BuildTables();
    mem_ = new uint8_t[kSize];
    memset(mem_, 0, kSize);
}

LocalMemory::~LocalMemory() { delete[] mem_; }

uint32_t LocalMemory::ReadPixel(uint32_t psm, uint32_t bp, uint32_t bw, int x, int y) const {
    switch (psm) {
    case PSMCT32: case PSMZ32: { uint32_t v; memcpy(&v, mem_ + Addr32(bp, bw, x, y), 4); return v; }
    case PSMCT24: case PSMZ24: { uint32_t v; memcpy(&v, mem_ + Addr32(bp, bw, x, y), 4); return v & 0xFFFFFF; }
    case PSMCT16: case PSMCT16S: case PSMZ16: case PSMZ16S: {
        uint16_t v; memcpy(&v, mem_ + Addr16(bp, bw, x, y), 2); return v;
    }
    case PSMT8: return mem_[Addr8(bp, bw, x, y)];
    case PSMT4: { uint32_t n = Addr4(bp, bw, x, y); return (mem_[n >> 1] >> ((n & 1) * 4)) & 0xF; }
    case PSMT8H: return mem_[Addr32(bp, bw, x, y) + 3];
    case PSMT4HL: return mem_[Addr32(bp, bw, x, y) + 3] & 0xF;
    case PSMT4HH: return mem_[Addr32(bp, bw, x, y) + 3] >> 4;
    }
    return 0;
}

void LocalMemory::WritePixel(uint32_t psm, uint32_t bp, uint32_t bw, int x, int y, uint32_t value) {
    switch (psm) {
    case PSMCT32: case PSMZ32: memcpy(mem_ + Addr32(bp, bw, x, y), &value, 4); break;
    case PSMCT24: case PSMZ24: {
        uint8_t* p = mem_ + Addr32(bp, bw, x, y);
        p[0] = (uint8_t)value; p[1] = (uint8_t)(value >> 8); p[2] = (uint8_t)(value >> 16);
        break;
    }
    case PSMCT16: case PSMCT16S: case PSMZ16: case PSMZ16S: {
        uint16_t v = (uint16_t)value; memcpy(mem_ + Addr16(bp, bw, x, y), &v, 2); break;
    }
    case PSMT8: mem_[Addr8(bp, bw, x, y)] = (uint8_t)value; break;
    case PSMT4: {
        uint32_t n = Addr4(bp, bw, x, y);
        uint8_t& b = mem_[n >> 1];
        b = (n & 1) ? (uint8_t)((b & 0x0F) | (value << 4)) : (uint8_t)((b & 0xF0) | (value & 0xF));
        break;
    }
    case PSMT8H: mem_[Addr32(bp, bw, x, y) + 3] = (uint8_t)value; break;
    case PSMT4HL: { uint8_t& b = mem_[Addr32(bp, bw, x, y) + 3]; b = (uint8_t)((b & 0xF0) | (value & 0xF)); break; }
    case PSMT4HH: { uint8_t& b = mem_[Addr32(bp, bw, x, y) + 3]; b = (uint8_t)((b & 0x0F) | (value << 4)); break; }
    }
}

void LocalMemory::ReadClut(const Tex0& t, const Texa& texa, uint32_t* clut, int count) const {
    // CSM1: the CLUT is a small image at CBP (16x16 entries for T8, 8x2 for T4) in CPSM.
    // T8 entries swap index bits 3 and 4 (the CSM1 arrangement); T4 picks a 16-entry group via CSA.
    bool ct16 = t.cpsm == PSMCT16 || t.cpsm == PSMCT16S;
    for (int i = 0; i < count; ++i) {
        int x, y;
        if (count == 256) {
            int j = (i & ~0x18) | ((i & 0x08) << 1) | ((i & 0x10) >> 1);
            x = j & 15;
            y = j >> 4;
        } else {
            // CSA counts 16-entry groups; in a CT32 CLUT a group is an 8x2 cell of the 16x16 table.
            int group = t.csa & 15;
            x = (i & 7) + (group & 1) * 8;
            y = (i >> 3) + (group >> 1) * 2;
        }
        uint32_t c = ReadPixel(ct16 ? PSMCT16 : PSMCT32, t.cbp, 1, x, y);
        clut[i] = ct16 ? Expand16(c, texa) : c;
    }
}

void LocalMemory::DecodeTexture(const Tex0& t, const Texa& texa, uint32_t* out) const {
    DecodeRect(t, texa, 0, 0, 1 << t.tw, 1 << t.th, out);
}

void LocalMemory::DecodeRect(const Tex0& t, const Texa& texa, int x0, int y0, int w, int h, uint32_t* out) const {
    uint32_t bw = t.tbw ? t.tbw : 1;
    uint32_t clut[256];
    switch (t.psm) {
    case PSMT8: case PSMT8H:
        ReadClut(t, texa, clut, 256);
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x) out[y * w + x] = clut[ReadPixel(t.psm, t.tbp0, bw, x0 + x, y0 + y)];
        break;
    case PSMT4: case PSMT4HL: case PSMT4HH:
        ReadClut(t, texa, clut, 16);
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x) out[y * w + x] = clut[ReadPixel(t.psm, t.tbp0, bw, x0 + x, y0 + y)];
        break;
    case PSMCT32:
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x) out[y * w + x] = ReadPixel(PSMCT32, t.tbp0, bw, x0 + x, y0 + y);
        break;
    case PSMCT24:
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x) {
                uint32_t c = ReadPixel(PSMCT24, t.tbp0, bw, x0 + x, y0 + y);
                uint32_t a = (texa.aem && c == 0) ? 0 : texa.ta0;
                out[y * w + x] = c | (a << 24);
            }
        break;
    case PSMCT16: case PSMCT16S:
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x)
                out[y * w + x] = Expand16(ReadPixel(t.psm, t.tbp0, bw, x0 + x, y0 + y), texa);
        break;
    default:
        for (int i = 0; i < w * h; ++i) out[i] = 0xFFFF00FF;   // unsupported format: magenta
        break;
    }
}

Transfer Transfer::From(uint64_t bitbltbuf, uint64_t trxpos, uint64_t trxreg) {
    Transfer t;
    t.dbp = (uint32_t)((bitbltbuf >> 32) & 0x3FFF);
    t.dbw = (uint32_t)((bitbltbuf >> 48) & 0x3F);
    t.dpsm = (uint32_t)((bitbltbuf >> 56) & 0x3F);
    t.dx = (uint32_t)((trxpos >> 32) & 0x7FF);
    t.dy = (uint32_t)((trxpos >> 48) & 0x7FF);
    t.w = (uint32_t)(trxreg & 0xFFF);
    t.h = (uint32_t)((trxreg >> 32) & 0xFFF);
    t.next = 0;
    return t;
}

uint32_t Transfer::BitsPerPixel() const {
    switch (dpsm) {
    case PSMCT32: case PSMZ32: return 32;
    case PSMCT24: case PSMZ24: return 24;
    case PSMCT16: case PSMCT16S: case PSMZ16: case PSMZ16S: return 16;
    case PSMT8: case PSMT8H: return 8;
    case PSMT4: case PSMT4HL: case PSMT4HH: return 4;
    }
    return 32;
}

void LocalMemory::WriteImage(Transfer& t, const uint8_t* data, uint32_t bytes) {
    uint32_t bpp = t.BitsPerPixel();
    uint32_t total = t.w * t.h;
    uint32_t count = bytes * 8 / bpp;
    uint32_t bw = t.dbw ? t.dbw : 1;
    for (uint32_t i = 0; i < count && t.next < total; ++i, ++t.next) {
        int x = (int)(t.dx + t.next % t.w), y = (int)(t.dy + t.next / t.w);
        uint32_t v;
        switch (bpp) {
        case 32: memcpy(&v, data + i * 4, 4); break;
        case 24: v = data[i * 3] | (data[i * 3 + 1] << 8) | (data[i * 3 + 2] << 16); break;
        case 16: v = data[i * 2] | (data[i * 2 + 1] << 8); break;
        case 8: v = data[i]; break;
        default: v = (data[i >> 1] >> ((i & 1) * 4)) & 0xF; break;
        }
        WritePixel(t.dpsm, t.dbp, bw, x, y, v);
    }
}

uint32_t PixelBlock(uint32_t psm, uint32_t bp, uint32_t bw, int x, int y) {
    bw = bw ? bw : 1;
    switch (psm) {
    case PSMCT16: case PSMCT16S: case PSMZ16: case PSMZ16S: return Addr16(bp, bw, x, y) >> 8;
    case PSMT8: return Addr8(bp, bw, x, y) >> 8;
    case PSMT4: return Addr4(bp, bw, x, y) >> 9;
    }
    return Addr32(bp, bw, x, y) >> 8;
}

void RectBlocks(uint32_t psm, uint32_t bp, uint32_t bw, int x, int y, int w, int h, std::vector<uint16_t>& out) {
    // Block size in pixels: CT32 8x8, CT16 16x8, T8 16x16, T4 32x16.
    int bx = 8, by = 8;
    switch (psm) {
    case PSMCT16: case PSMCT16S: case PSMZ16: case PSMZ16S: bx = 16; break;
    case PSMT8: bx = 16; by = 16; break;
    case PSMT4: bx = 32; by = 16; break;
    }
    out.clear();
    for (int cy = y - y % by; cy < y + h; cy += by)
        for (int cx = x - x % bx; cx < x + w; cx += bx)
            out.push_back((uint16_t)PixelBlock(psm, bp, bw, cx < x ? x : cx, cy < y ? y : cy));
    std::sort(out.begin(), out.end());
    out.erase(std::unique(out.begin(), out.end()), out.end());
}

}  // namespace gs
