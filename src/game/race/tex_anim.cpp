#include "tex_anim.h"

#include <stdio.h>

#include "../../gfx/textures/texture_set.h"
#include "../../util/data_pack.h"

namespace game {

namespace {

// Lagoon_TideStep's table (D_002A36B0, D_002A36C8): the tide comes in through [edge i, edge i + 1) at speed i,
// fast in the middle and slow at the ends, and goes out through the second half the same way.
const int16_t kTideEdge[11] = {0, 64, 384, 480, 512, 496, 400, 208, 80, 32, 0};
const int8_t kTideSpeed[10] = {4, 8, 4, 1, 1, 4, 8, 6, 2, 1};

// The GS's pages: a T4 page is 128 x 128 pixels, a T8 page 128 x 64, and a buffer bw x 64 wide has bw / 2 pages a
// row (at least one). Where pixel (x, y) of a bw-wide buffer is in a picture uploaded `width` wide: the same page
// position, its page counted in the picture's rows of pages.
void PageRemap(int psm, int bw, int width, int x, int y, int& ox, int& oy) {
    const int pw = 128, ph = psm == kT8 ? 64 : 128;
    int row = bw >> 1 ? bw >> 1 : 1, uploadBw = (width + 63) / 64, uploadRow = uploadBw >> 1 ? uploadBw >> 1 : 1;
    int page = y / ph * row + x / pw;
    ox = page % uploadRow * pw + x % pw;
    oy = page / uploadRow * ph + y % ph;
}

}  // namespace

std::map<int, ManifestEntry> ReadManifest(const std::string& path) {
    std::map<int, ManifestEntry> out;
    std::vector<std::string> lines;
    if (!util::Data_ReadLines(path, lines)) return out;
    for (const std::string& text : lines) {
        const char* line = text.c_str();
        if (line[0] == '#') continue;
        int n, palette;
        unsigned w, h, dbp, cbp;
        char psm[16], clutPsm[16], match[16], img[32], clut[32], file[256];
        if (sscanf(line, "%d %d %15s %u %u %x %x %15s %15s %31s %31s %255s", &n, &palette, psm, &w, &h, &dbp, &cbp,
                   clutPsm, match, img, clut, file) == 12 &&
            !out.count((int)dbp))
            out[(int)dbp] = {file, (int)w, (int)h};
    }
    return out;
}

bool Tex_LoadAnimation(gfx::Draw2D& d, const std::string& textures, const std::string& dir,
                       const std::map<int, ManifestEntry>& manifest, const TexCopy& c, TexAnimation& a) {
    a.copy = c;
    a.x = a.y = -1;
    auto src = manifest.find(c.sbp), dst = manifest.find(c.dbp);
    if (src == manifest.end() || dst == manifest.end()) return false;
    // modelconv gives an upload its own copy (u<DBP>.dds) where the file shares the picture with another.
    char upload[32];
    snprintf(upload, sizeof upload, ".BIN\\u%04X.dds", c.dbp);
    a.target = textures + dir + upload;
    if (!util::Data_Exists(a.target)) a.target = textures + dst->second.path;
    if (!gfx::LoadDds(textures + src->second.path, a.source, a.sourceDdsW, a.sourceDdsH) ||
        !gfx::LoadDds(a.target, a.pixels, a.targetDdsW, a.targetDdsH))
        return false;
    a.sourceW = src->second.width;
    a.sourceH = src->second.height;
    a.targetW = dst->second.width;
    a.targetH = dst->second.height;
    if (a.copy.w > a.targetW) a.copy.w = (uint16_t)a.targetW;
    if (a.copy.h > a.targetH) a.copy.h = (uint16_t)a.targetH;
    a.original = a.pixels;
    d.GetFile(a.target);   // the models' texture, if they haven't loaded it yet
    return true;
}

void Tex_Copy(gfx::Draw2D& d, TexAnimation& a, int sx, int sy) {
    if (sx == a.x && sy == a.y) return;
    a.x = sx;
    a.y = sy;
    const TexCopy& c = a.copy;
    // In the DDS files' pixels: a mod's larger pictures take the copy at their own scale.
    int w = c.w * a.targetDdsW / a.targetW, h = c.h * a.targetDdsH / a.targetH;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            int px, py;
            PageRemap(c.psm, c.bw, a.sourceW, sx + x * a.targetW / a.targetDdsW, sy + y * a.targetH / a.targetDdsH,
                      px, py);
            uint32_t v = 0;
            if (px < a.sourceW && py < a.sourceH)
                v = a.source[(size_t)(py * a.sourceDdsH / a.sourceH) * a.sourceDdsW + px * a.sourceDdsW / a.sourceW];
            a.pixels[(size_t)y * a.targetDdsW + x] = v;
        }
    }
    d.WriteFile(a.target, a.pixels.data());
}

void Tex_Restore(gfx::Draw2D& d, TexAnimation& a) {
    if (a.x < 0) return;   // not written since the file's pictures
    a.pixels = a.original;
    a.x = a.y = -1;
    d.WriteFile(a.target, a.pixels.data());
}

void Lagoon_TideStep(int16_t& tide, bool& ebb) {
    int16_t x = tide, cur = x;
    if (x <= 0) ebb = false;
    for (int i = 0; i < 10; ++i) {
        if (kTideEdge[i] <= x && x < kTideEdge[i + 1] && !ebb) {
            tide = cur = (int16_t)(x + kTideSpeed[i]);
            break;
        }
        if (kTideEdge[i] >= x && kTideEdge[i + 1] < x && ebb) {
            tide = cur = (int16_t)(x - kTideSpeed[i]);
            if (cur >= 0x1FF) ebb = !ebb;
            break;
        }
    }
    if (cur >= 0x1FF) ebb = !ebb;
}

}  // namespace game
