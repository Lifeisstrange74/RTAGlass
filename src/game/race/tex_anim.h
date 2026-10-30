// The animated textures of the courses and the field: the GS local-to-local copies their tasks send each frame (a
// window of a source picture into a destination's top left; the water scrolls and flips), done on the PC by
// rewriting the destination DDS's pixels in the texture set (Draw2D::WriteFile). And Lagoon_TideStep (0x0023E018),
// the tide the beaches' surf follows. Shared by CourseObjects (the races) and FieldScrolls (the field). See
// docs/race.md, "Course objects", and field.md, "Objects".
#pragma once

#include <stdint.h>

#include <map>
#include <string>
#include <vector>

#include "../../gfx/draw2d.h"

namespace game {

const uint16_t kT4 = 0x14, kT8 = 0x13;   // the copies' PSMs

// A course's or a field block's texture manifest (textures/<DIR>/<FILE>.BIN.txt), by where the file uploads each
// texture (DBP).
struct ManifestEntry {
    std::string path;
    int width, height;
};
std::map<int, ManifestEntry> ReadManifest(const std::string& path);

// One copy: BITBLTBUF from (sbp) to (dbp), both bw x 64 wide in psm; TRXPOS's source corner from the frame; TRXREG
// w x h.
struct TexCopy {
    uint16_t sbp, dbp;
    uint8_t bw, psm;
    uint16_t w, h;
    int16_t speedX, speedY;   // +0x18, +0x1A: (frame x speed) / 16 ...
    uint16_t maskX, maskY;    // +0x1C, +0x1E: ... & mask
};
struct TexAnimation {
    TexCopy copy;
    std::string target;              // the destination's DDS, as the models name it
    std::vector<uint32_t> source;    // the source's pixels, as decoded at its upload's width
    std::vector<uint32_t> pixels;    // the destination's pixels, the copy written into them
    std::vector<uint32_t> original;  // the destination's pixels as the file has them
    int sourceW, sourceH, targetW, targetH;   // the game's sizes (the manifest's)
    int sourceDdsW, sourceDdsH, targetDdsW, targetDdsH;   // the DDS files' (larger for a mod)
    int x, y;                        // the copy's source corner last written (-1: none yet)
};

// The copy `c` of a file whose pictures are textures/`dir`.BIN/... (its manifest `manifest`): both pictures read, the
// destination's own upload copy (u<DBP>.dds) if modelconv made one. False if either isn't there.
bool Tex_LoadAnimation(gfx::Draw2D& d, const std::string& textures, const std::string& dir,
                       const std::map<int, ManifestEntry>& manifest, const TexCopy& c, TexAnimation& a);
// The copy's source corner (sx, sy) written into the destination's top left w x h (nothing if it's the last one).
void Tex_Copy(gfx::Draw2D& d, TexAnimation& a, int sx, int sy);
// The destination back as its file has it, if a copy changed it.
void Tex_Restore(gfx::Draw2D& d, TexAnimation& a);
// (frame x speed) / 16 & mask, the division rounding towards 0 as the game's does.
inline int Tex_Step(int frame, int speed, int mask) { return (frame * speed / 16) & mask; }
// Lagoon_TideStep (0x0023E018): the tide from 0 up to 512 and back; `ebb` (D_00335708) while it goes out.
void Lagoon_TideStep(int16_t& tide, bool& ebb);

}  // namespace game
