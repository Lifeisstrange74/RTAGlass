// Emulated GS local memory (4 MB) with the PS2 swizzled pixel layouts, and texture decoding.
//
// Layouts (GS User's Manual): memory is split into 8 KB pages of 32 256-byte blocks. Each storage format
// arranges blocks inside a page and pixels inside a block differently:
//   PSMCT32/24  page 64x32,  block 8x8     PSMCT16/16S  page 64x64,  block 16x8
//   PSMT8       page 128x64, block 16x16   PSMT4        page 128x128, block 32x16
// T8H / T4HL / T4HH live in the top byte / nibbles of a PSMCT32 layout.
//
// Coverage follows docs/gs-inventory.md (what Road Trip uses), not the whole GS:
// CLUTs are CSM1 only, and T4's CSA offset is honoured for CT32 CLUTs.
#pragma once

#include <stdint.h>

#include <vector>

namespace gs {

enum Psm : uint32_t {
    PSMCT32 = 0x00, PSMCT24 = 0x01, PSMCT16 = 0x02, PSMCT16S = 0x0A,
    PSMT8 = 0x13, PSMT4 = 0x14, PSMT8H = 0x1B, PSMT4HL = 0x24, PSMT4HH = 0x2C,
    PSMZ32 = 0x30, PSMZ24 = 0x31, PSMZ16 = 0x32, PSMZ16S = 0x3A,
};

// TEX0 register fields used for decoding.
struct Tex0 {
    uint32_t tbp0, tbw, psm, tw, th, tcc, tfx, cbp, cpsm, csm, csa, cld;
    static Tex0 From(uint64_t v);
};

// A host-to-local image transfer (BITBLTBUF/TRXPOS/TRXREG destination fields). Pixels arrive in raster order
// from (dx, dy), packed at the format's bit depth (T4: low nibble first); next is the resume point.
struct Transfer {
    uint32_t dbp, dbw, dpsm, dx, dy, w, h;
    uint32_t next;
    static Transfer From(uint64_t bitbltbuf, uint64_t trxpos, uint64_t trxreg);
    uint32_t BitsPerPixel() const;
    uint32_t TotalBytes() const { return (w * h * BitsPerPixel() + 7) / 8; }
};

// The texture set's lookup key for an upload (gfx::TextureKey): FNV-1a 64 over the transfer's format and size and
// its t.TotalBytes() bytes of image data.
uint64_t HashUpload(const Transfer& t, const uint8_t* data);

struct Texa {
    uint8_t ta0, ta1;
    bool aem;
    static Texa From(uint64_t v);
};

class LocalMemory {
public:
    static const uint32_t kSize = 4 * 1024 * 1024;

    LocalMemory();
    ~LocalMemory();

    uint8_t* Data() { return mem_; }
    const uint8_t* Data() const { return mem_; }

    // Pixel access in a given format. bp is in 256-byte blocks, bw in 64-pixel units (as in the registers).
    uint32_t ReadPixel(uint32_t psm, uint32_t bp, uint32_t bw, int x, int y) const;
    void WritePixel(uint32_t psm, uint32_t bp, uint32_t bw, int x, int y, uint32_t value);

    // Apply image data to a transfer, continuing from t.next; data may arrive in several pieces.
    void WriteImage(Transfer& t, const uint8_t* data, uint32_t bytes);

    // Decode a whole texture level into 32-bit RGBA (byte order R,G,B,A per pixel; PS2 alpha, 0x80 = 1.0).
    // out must hold (1 << tw) * (1 << th) pixels.
    void DecodeTexture(const Tex0& tex0, const Texa& texa, uint32_t* out) const;

    // Decode a w x h rectangle at (x0, y0) of the texture described by tex0 (tw/th are ignored).
    void DecodeRect(const Tex0& tex0, const Texa& texa, int x0, int y0, int w, int h, uint32_t* out) const;

    // Read the 16 or 256 palette entries for a paletted TEX0, as RGBA.
    void ReadClut(const Tex0& tex0, const Texa& texa, uint32_t* clut, int count) const;

private:
    uint8_t* mem_;
};

// Index (0..16383) of the 256-byte block holding pixel (x, y) of a buffer in format psm.
uint32_t PixelBlock(uint32_t psm, uint32_t bp, uint32_t bw, int x, int y);

// The blocks a w x h rectangle at (x, y) touches, sorted, without duplicates.
void RectBlocks(uint32_t psm, uint32_t bp, uint32_t bw, int x, int y, int w, int h, std::vector<uint16_t>& out);

// Expand a 16-bit PS2 colour (A1 B5 G5 R5) to RGBA using TEXA.
uint32_t Expand16(uint32_t c, const Texa& texa);

}  // namespace gs
