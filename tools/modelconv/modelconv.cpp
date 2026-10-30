// modelconv: converts the game's E3D models to OBJ for the PC data set (data/models/<DIR>/<FILE>.obj).
//
// An E3D file is a VU1 DMA/VIF stream (header: two u32 offsets near the end of the file, then a DMA cnt tag whose
// upper half holds the first VIF codes). The models handled here use the lit-strip layout of VU1 slot 0x20
// (docs/vu1-microcode.md), one strip per MSCAL:
//   STCYCL; UNPACK V4-32 x1 (GIF tag, NLOOP = vertex count); UNPACK V3-32 x4N (per vertex: position, normal,
//   colour 0-255, STQ); MSCAL.
//
// SYS/S00.E3D and S01.E3D, the course miniatures of the course select, use the model-table layout
// (docs/course_select.md):
//   u32 offset[64] (model n = course id n; 0x20-byte entries are empty stubs), then each model is a DMA chain
//   (cnt ... ret) of batches for VU1 slot 0x40:
//     UNPACK V3-32 to top+0x192: an A+D GS state packet (TEX0_2, CLAMP_2, TEX1_2) for the strips that follow;
//     UNPACK V4-32 x1 to top+0: the header (x = vertex count, y = GIF tag word 1 with the strip's PRIM);
//     UNPACK V3-32 x5N to top+1: per vertex position, colour 1, colour 2, colour 3, STQ; MSCAL 0x40.
//   The course select weights the colours (1, 0, 0), so colour 1 is the vertex colour. Positions sit on a grid of
//   200-unit cells (course n at x = 200 (n & 7), z = 200 (n / 8)); the converter moves each to the origin, so a
//   model spans about 0..200 in x and z, y up. Textures are the matching GSL's (S00.E3D -> SYS/S00.GSL), found by
//   TEX0's TBP, CBP and CSA in the texture set's manifest, which texconv must have written first.
//   Output: models/SYS/S00/<id>.obj + .mtl (two-digit decimal course id).
// Other E3D layouts are reported and skipped.
//
// The Adventure's world, FLD/<abc>.BIN, is 64 blocks of 1600 units in the same layout: models/FLD/<abc>/ and
// collision/FLD/<abc>.pmp (docs/field.md).
// The race courses, COURSE/Cnn.BIN, and the event courses, ACTION/Ann.BIN (course ids 0x12 on), hold their field as
// the same kind of model table, with world positions and also programs 0x30 (billboards) and 0x60 (see ParseChain and
// ConvertCourse); their textures are the course file's own (textures/COURSE/Cnn.BIN.txt). The sky comes from the
// executable. Output: models/COURSE/Cnn/<block>.obj (models/ACTION/Ann/), map.obj (the HUD's course map) and sky.obj
// (docs/race.md, hud.md).
// The opponents' paths come from the executable too: ai/<NAME>.txt (docs/ai.md), and so do the trackside cameras
// of the finish and the replay: camera/<NAME>.txt (docs/finish.md), and the Adventure's messages:
// story/messages.txt (docs/messages.md).
//
// The cars, CAR0-4/Qnn.BIN and CARS/Q150.BIN, and their tires, CARS/TIRE.BIN: six sections of program 0x20 strips
// each (see ConvertCar). Output: models/CARn/Qnn/<section>.obj and shadow.obj, models/CARS/TIRE/<section>.obj
// and the parts, CARS/PARTS.BIN: models/CARS/PARTS/<n>.obj (ConvertParts), and the close-up wheels, CARS/WHEEL.BIN:
// models/CARS/WHEEL/<n>.obj (ConvertWheels)
// (docs/cars.md).
//
// Usage: modelconv GAMEDIR DATADIR   (e.g. modelconv ..\OriginalGameFiles data)
//        modelconv GAMEDIR DATADIR --messages   (just story/messages.txt and story/mail.txt)
#include <stdio.h>
#include <string.h>
#include <windows.h>

#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "../../src/gfx/mesh.h"

namespace {

bool ReadFile(const std::string& path, std::vector<uint8_t>& out) {
    FILE* fp = fopen(path.c_str(), "rb");
    if (!fp) return false;
    fseek(fp, 0, SEEK_END);
    long n = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    out.resize(n > 0 ? (size_t)n : 0);
    bool ok = fread(out.data(), 1, out.size(), fp) == out.size();
    fclose(fp);
    return ok;
}

uint32_t Rd32(const uint8_t* p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24); }

float RdF(const uint8_t* p) {
    float f;
    memcpy(&f, p, 4);
    return f;
}

gfx::MeshVertex NewVertex() {
    gfx::MeshVertex v = {};
    v.color[3] = 1.0f;
    return v;
}

// Walks the VIF stream and collects the strips as triangles. Returns false for an unsupported layout.
bool ParseE3D(const std::vector<uint8_t>& b, gfx::Mesh& mesh, int& strips) {
    strips = 0;
    uint32_t expect = 0;   // vertex count from the last GIF tag
    for (size_t pos = 0x18; pos + 4 <= b.size();) {
        uint32_t code = Rd32(&b[pos]);
        pos += 4;
        uint32_t cmd = (code >> 24) & 0x7F, num = (code >> 16) & 0xFF;
        if ((cmd & 0x60) != 0x60) continue;   // STCYCL, MSCAL, NOP ...: no data follows
        uint32_t vn = (cmd >> 2) & 3, vl = cmd & 3, n = num ? num : 256;
        size_t size = (size_t)(32 >> vl) / 8 * (vn + 1) * n;
        if (pos + size > b.size()) break;   // trailing data past the stream (the file ends with a short tail)
        const uint8_t* d = &b[pos];
        if ((cmd & 0x0F) == 0x0C && n == 1) {          // V4-32: the strip's GIF tag
            expect = Rd32(d) & 0x7FFF;
        } else if ((cmd & 0x0F) == 0x08) {             // V3-32: the vertices
            if (n != expect * 4) return false;
            uint32_t base = (uint32_t)mesh.vertices.size();
            for (uint32_t v = 0; v < expect; ++v, d += 48) {
                gfx::MeshVertex mv = NewVertex();
                for (int i = 0; i < 3; ++i) {
                    mv.pos[i] = RdF(d + i * 4);
                    mv.normal[i] = RdF(d + 12 + i * 4);
                    mv.color[i] = RdF(d + 24 + i * 4) / 255.0f;
                }
                mesh.vertices.push_back(mv);
            }
            // Strip to triangles, alternating the winding so faces keep one orientation.
            for (uint32_t i = 2; i < expect; ++i) {
                uint32_t a = base + i - 2, c = base + i - 1, e = base + i;
                if (i & 1) std::swap(a, c);
                mesh.indices.push_back(a);
                mesh.indices.push_back(c);
                mesh.indices.push_back(e);
            }
            ++strips;
            expect = 0;
        }
        pos += (size + 3) & ~(size_t)3;
    }
    return strips > 0;
}

// A texture from a texture-set manifest, keyed by where its GSL put it in GS memory.
struct ManifestTexture {
    int dbp, cbp, palette;
    int width, height;   // the image's size, which can be less than TEX0's power-of-two size
    std::string path;    // relative to textures/
};

std::vector<ManifestTexture> ReadManifest(const std::string& path) {
    std::vector<ManifestTexture> out;
    FILE* fp = fopen(path.c_str(), "r");
    if (!fp) return out;
    char line[512];
    while (fgets(line, sizeof(line), fp)) {
        if (line[0] == '#') continue;
        int n, palette;
        char psm[16], clutPsm[16], match[16], img[32], clut[32], file[256];
        unsigned w, h, dbp, cbp;
        if (sscanf(line, "%d %d %15s %u %u %x %x %15s %15s %31s %31s %255s", &n, &palette, psm, &w, &h, &dbp, &cbp,
                   clutPsm, match, img, clut, file) == 12)
            out.push_back({(int)dbp, (int)cbp, palette, (int)w, (int)h, file});
    }
    fclose(fp);
    return out;
}

std::vector<ManifestTexture> ReadManifestFor(const std::string& data, const std::string& rel) {
    std::string path = data + "\\textures\\" + rel + ".txt";
    for (char& c : path)
        if (c == '/') c = '\\';
    std::vector<ManifestTexture> textures = ReadManifest(path);
    if (textures.empty()) printf("%s: no texture manifest (run texconv first)\n", rel.c_str());
    return textures;
}

// The GS state a batch draws with (TEX0_2, CLAMP_2), from the batch's own A+D packet or set beforehand.
struct GsState {
    uint64_t tex0 = 0, clamp = 0;
};

// Collects strips into a mesh, one part per texture and wrap mode. Texture paths are written as `texPrefix` +
// the manifest path.
class MeshBuilder {
public:
    MeshBuilder(const std::vector<ManifestTexture>& textures, const std::string& texPrefix, gfx::Mesh& mesh)
        : textures_(textures), texPrefix_(texPrefix), mesh_(mesh), missing(0), strips(0) {
        mesh_.hasNormals = false;
        mesh_.hasUvs = true;
    }

    // A strip with PRIM `prim` (GIF tag word 1 bits 15+), drawn with `gs`. UVs are TEX0's normalized coordinates.
    // `paint` (program 0x20's header w) picks the car's paint colour 1 or 2, or none (0). `lit`: the vertices carry
    // normals and specular strengths. `timed`: they carry the dusk and night colours (programs 0x30/0x40).
    void Strip(std::vector<gfx::MeshVertex> verts, uint32_t prim, const GsState& gs, int paint = 0, bool lit = false,
               bool timed = false) {
        if (lit) mesh_.hasNormals = mesh_.hasSpecular = true;
        if (timed) mesh_.hasTimeColors = true;
        bool textured = (prim & 0x10) != 0;   // PRIM.TME
        // WMS/WMT: 0 repeat, 1 clamp, 2 region clamp (here always the image's own edge), 3 region repeat.
        int wms = (int)(gs.clamp & 3), wmt = (int)((gs.clamp >> 2) & 3);
        bool clampU = wms == 1 || wms == 2, clampV = wmt == 1 || wmt == 2;
        std::string material = "color", texture;
        if (textured) {
            int tbp = (int)(gs.tex0 & 0x3FFF), cbp = (int)((gs.tex0 >> 37) & 0x3FFF);
            int csa = (int)((gs.tex0 >> 56) & 0x1F), psm = (int)((gs.tex0 >> 20) & 0x3F);
            int tw = 1 << ((gs.tex0 >> 26) & 0xF), th = 1 << ((gs.tex0 >> 30) & 0xF);
            bool direct = psm == 0x00 || psm == 0x01 || psm == 0x02 || psm == 0x0A;   // no CLUT: CBP is junk
            const ManifestTexture* found = nullptr;
            for (const ManifestTexture& t : textures_)
                if (t.dbp == tbp && (direct || t.cbp == cbp) && (!found || t.palette == csa)) found = &t;
            char name[300];
            if (found) {
                texture = texPrefix_ + found->path;
                snprintf(name, sizeof name, "%s_%c%c", found->path.c_str(), clampU ? 'c' : 'r', clampV ? 'c' : 'r');
                // TEX0 addresses a power-of-two area; the image may only fill part of it (the sky is 512x96 of
                // 512x128).
                float su = (float)tw / found->width, sv = (float)th / found->height;
                for (gfx::MeshVertex& v : verts) {
                    v.uv[0] *= su;
                    v.uv[1] *= sv;
                }
            } else {
                ++missing;
                snprintf(name, sizeof name, "missing_%04X_%04X", tbp, cbp);
            }
            material = name;
            for (char& c : material)
                if (c == '/' || c == '.') c = '_';
        }
        if (paint) material += "_paint" + std::to_string(paint);
        auto it = partOf_.find(material);
        if (it == partOf_.end()) {
            gfx::MeshPart part;
            part.material = material;
            part.texture = texture;
            part.clampU = clampU;
            part.clampV = clampV;
            part.paint = paint;
            it = partOf_.insert(std::make_pair(material, mesh_.parts.size())).first;
            mesh_.parts.push_back(part);
            partIndices_.emplace_back();
        }
        uint32_t base = (uint32_t)mesh_.vertices.size();
        mesh_.vertices.insert(mesh_.vertices.end(), verts.begin(), verts.end());
        // Strip to triangles, alternating the winding so faces keep one orientation.
        std::vector<uint32_t>& idx = partIndices_[it->second];
        for (uint32_t i = 2; i < verts.size(); ++i) {
            uint32_t a = base + i - 2, c = base + i - 1, e = base + i;
            if (i & 1) std::swap(a, c);
            idx.push_back(a);
            idx.push_back(c);
            idx.push_back(e);
        }
        ++strips;
    }

    // One run of triangles per material.
    void Finish() {
        for (size_t i = 0; i < mesh_.parts.size(); ++i) {
            mesh_.parts[i].first = (uint32_t)mesh_.indices.size();
            mesh_.parts[i].count = (uint32_t)partIndices_[i].size();
            mesh_.indices.insert(mesh_.indices.end(), partIndices_[i].begin(), partIndices_[i].end());
        }
    }

    int missing, strips;

private:
    const std::vector<ManifestTexture>& textures_;
    std::string texPrefix_;
    gfx::Mesh& mesh_;
    std::map<std::string, size_t> partOf_;   // material -> part
    std::vector<std::vector<uint32_t>> partIndices_;
};

// A vertex of programs 0x40 and 0x30: position, colours 1-3, STQ (five V3 qwords). The programs blend three colour
// sets by the race's weights vf09 (day, dusk, night: the time of day, docs/race.md), and as the code takes the
// lanes they are
//   day   = colour 1 with alpha colour 2.x,
//   dusk  = (colour 1.x, colour 2.y, colour 2.z) with alpha colour 2.x,
//   night = colour 3 with alpha STQ.z (the program then puts 1 in its place: the vertices have no Q).
gfx::MeshVertex ReadVertex5(const uint8_t* d, float ox, float oz) {
    gfx::MeshVertex mv = NewVertex();
    mv.pos[0] = RdF(d) + ox;
    mv.pos[1] = RdF(d + 4);
    mv.pos[2] = RdF(d + 8) + oz;
    auto colour = [&](int off) { return std::min(1.0f, std::max(0.0f, RdF(d + off) / 255.0f)); };
    auto alpha = [&](int off) { return std::min(1.0f, std::max(0.0f, RdF(d + off))); };
    for (int c = 0; c < 3; ++c) {
        mv.color[c] = colour(12 + c * 4);
        mv.night[c] = colour(36 + c * 4);
    }
    mv.dusk[0] = mv.color[0];
    mv.dusk[1] = colour(28);
    mv.dusk[2] = colour(32);
    mv.color[3] = mv.dusk[3] = alpha(24);
    mv.night[3] = alpha(56);
    mv.uv[0] = RdF(d + 48);
    mv.uv[1] = RdF(d + 52);
    return mv;
}

// A vertex of programs 0x20 and 0x50 (the cars): position, normal, colour 0-255, then (s, t, specular): the program
// takes the third word as the highlight's strength and puts 1 there for Q.
gfx::MeshVertex ReadVertex4(const uint8_t* d) {
    gfx::MeshVertex mv = NewVertex();
    for (int i = 0; i < 3; ++i) {
        mv.pos[i] = RdF(d + i * 4);
        mv.normal[i] = RdF(d + 12 + i * 4);
        mv.color[i] = std::min(1.0f, std::max(0.0f, RdF(d + 24 + i * 4) / 255.0f));
    }
    mv.uv[0] = RdF(d + 36);
    mv.uv[1] = RdF(d + 40);
    mv.specular = RdF(d + 44);
    return mv;
}

// Converts one DMA chain (cnt ... ret) of VU1 batches, starting at `off`, into `out`. `ox`/`oz` move the
// positions. A batch is its UNPACKs followed by the MSCAL that runs it:
//   UNPACK V3-32 to top+0x192: an A+D packet (TEX0_2, CLAMP_2, TEX1_2) for this batch and the next ones;
//   MSCAL 0x08, program 0x40: V4-32 x1 to top+0 (header: x = vertex count, y = GIF tag word 1, its bit 1 the fog
//     set: MeshVertex::cueFog) and V3-32 x5N to top+1 (ReadVertex5);
//   MSCAL 0x06, program 0x30: the same header, then V3-32 x(1 + 5N) to top+1: an anchor and the vertices around it,
//     a sprite the program turns to face the camera (a billboard: trees, flags);
//   MSCAL 0x0C, program 0x60: V3-32 x(2 + 3N) to top+0: the header as two qwords (GIF tag, then a second word),
//     then per vertex position, colour (0-255), STQ;
//   MSCAL 0x04, program 0x20 (car bodies, tires): the V4 header (w = the paint: 0 none, 1 or 2), then V3-32 x4N to
//     top+1 (ReadVertex4), lit by a directional light;
//   MSCAL 0x0A, program 0x50: the same input (the car's shadow, lights).
// Returns the strip count, or -1 for data it doesn't understand.
int ParseChain(const std::vector<uint8_t>& b, size_t off, float ox, float oz, GsState gs, MeshBuilder& out) {
    uint32_t header[4] = {};
    const uint8_t* data = nullptr;   // the last vertex UNPACK
    uint32_t dataN = 0, dataAddr = 0;
    for (size_t pos = off; pos + 16 <= b.size();) {
        uint32_t tag = Rd32(&b[pos]), qwc = tag & 0xFFFF, kind = (tag >> 28) & 7;
        size_t end = std::min(b.size(), pos + 16 + (size_t)qwc * 16);
        // The VIF codes: the tag's upper two words, then the packet's data.
        for (size_t v = pos + 8; v + 4 <= end;) {
            uint32_t code = Rd32(&b[v]);
            v = v == pos + 12 ? pos + 16 : v + 4;
            uint32_t cmd = (code >> 24) & 0x7F, num = (code >> 16) & 0xFF, addr = code & 0x3FF;
            if (cmd == 0x14 || cmd == 0x15) {   // MSCAL / MSCALF: run the batch
                uint32_t prog = code & 0xFFFF;
                std::vector<gfx::MeshVertex> verts;
                uint32_t prim = 0;
                if (!data) continue;
                if (prog == 0x08 || prog == 0x06) {
                    uint32_t count = header[0] & 0x7FFF, extra = prog == 0x06 ? 1 : 0;
                    if (dataAddr != 1 || dataN != extra + count * 5) return -1;
                    float anchor[3] = {RdF(data) + ox, RdF(data + 4), RdF(data + 8) + oz};
                    for (uint32_t i = 0; i < count; ++i) {
                        gfx::MeshVertex mv = ReadVertex5(data + (extra + i * 5) * 12, ox, oz);
                        if (extra) {   // positions are relative to the anchor
                            mv.billboard = true;
                            for (int k = 0; k < 3; ++k) {
                                mv.anchor[k] = anchor[k];
                                mv.pos[k] += anchor[k] - (k == 0 ? ox : k == 2 ? oz : 0.0f);
                            }
                        }
                        // Header y bit 1: the strip fogs by vf11, the distance cue's numbers, not vf10's
                        // (reference/vu/mpg_0028E748.vsm: program 0x40 at 0x0118-0x0138, 0x30 at 0x0490-0x04B0).
                        mv.cueFog = (header[1] & 2) != 0;
                        verts.push_back(mv);
                    }
                    prim = header[1] >> 15;
                    if (verts.size() >= 3) out.Strip(verts, prim, gs, 0, false, true);
                    data = nullptr;
                    continue;
                } else if (prog == 0x0C) {
                    uint32_t count = Rd32(data) & 0x7FFF;
                    if (dataAddr != 0 || dataN != 2 + count * 3) return -1;
                    for (uint32_t i = 0; i < count; ++i) {
                        const uint8_t* d = data + (2 + i * 3) * 12;
                        gfx::MeshVertex mv = NewVertex();
                        mv.pos[0] = RdF(d) + ox;
                        mv.pos[1] = RdF(d + 4);
                        mv.pos[2] = RdF(d + 8) + oz;
                        for (int c = 0; c < 3; ++c)
                            mv.color[c] = std::min(1.0f, std::max(0.0f, RdF(d + 12 + c * 4) / 255.0f));
                        float q = RdF(d + 32);
                        if (q == 0) q = 1;
                        mv.uv[0] = RdF(d + 24) / q;
                        mv.uv[1] = RdF(d + 28) / q;
                        verts.push_back(mv);
                    }
                    prim = Rd32(data + 4) >> 15;
                } else if (prog == 0x04 || prog == 0x0A) {
                    uint32_t count = header[0] & 0x7FFF;
                    if (dataAddr != 1 || dataN != count * 4) return -1;
                    for (uint32_t i = 0; i < count; ++i) {
                        gfx::MeshVertex mv = ReadVertex4(data + i * 48);
                        mv.pos[0] += ox;
                        mv.pos[2] += oz;
                        verts.push_back(mv);
                    }
                    // The program takes vf18 for any w other than 0, then vf17 for an odd one.
                    int paint = prog == 0x04 && header[3] ? (header[3] & 1 ? 1 : 2) : 0;
                    if (verts.size() >= 3) out.Strip(verts, header[1] >> 15, gs, paint, true);
                    data = nullptr;
                    continue;
                } else {
                    return -1;
                }
                if (verts.size() >= 3) out.Strip(verts, prim, gs);
                data = nullptr;
                continue;
            }
            if ((cmd & 0x60) != 0x60) continue;   // STCYCL, NOP, ...: no data follows
            uint32_t vn = (cmd >> 2) & 3, vl = cmd & 3, n = num ? num : 256;
            size_t size = ((size_t)(32 >> vl) * (vn + 1) * n + 31) / 32 * 4;
            if (v + size > end) return -1;
            const uint8_t* d = &b[v];
            v += size;
            if ((cmd & 0x0F) == 0x0C && n == 1 && addr == 0) {   // V4-32: header
                for (int i = 0; i < 4; ++i) header[i] = Rd32(d + i * 4);
            } else if ((cmd & 0x0F) == 0x08 && addr == 0x192) {   // V3-32: A+D state (a GIF tag, then the data)
                for (uint32_t q = 1; q < n; ++q) {
                    const uint8_t* e = d + q * 12;
                    uint64_t value = Rd32(e) | ((uint64_t)Rd32(e + 4) << 32);
                    if (e[8] == 0x06 || e[8] == 0x07) gs.tex0 = value;
                    if (e[8] == 0x08 || e[8] == 0x09) gs.clamp = value;
                }
            } else if ((cmd & 0x0F) == 0x08) {   // V3-32: vertices (and for program 0x60 the header)
                data = d;
                dataN = n;
                dataAddr = addr;
            }
        }
        if (kind == 6 || kind == 7) break;   // ret / end: the model is done
        pos = end;
    }
    return out.strips;
}

// Converts the chain at `off` to an OBJ. `uvInset` pulls the texture coordinates that far into the middle of their
// range. Returns 1 if written, 0 if there was nothing to write, -1 on a write error.
int WriteChain(const std::vector<uint8_t>& b, size_t off, float ox, float oz, const GsState& gs,
               const std::vector<ManifestTexture>& textures, const std::string& texPrefix, const std::string& out,
               const std::string& what, float uvInset = 0) {
    gfx::Mesh mesh;
    MeshBuilder builder(textures, texPrefix, mesh);
    int strips = ParseChain(b, off, ox, oz, gs, builder);
    if (strips < 0) printf("%s: not understood, skipped\n", what.c_str());
    if (strips <= 0) return 0;   // (0: a stub with an empty chain)
    builder.Finish();
    if (uvInset > 0) {
        float lo[2] = {1e30f, 1e30f}, hi[2] = {-1e30f, -1e30f};
        for (const gfx::MeshVertex& v : mesh.vertices)
            for (int k = 0; k < 2; ++k) {
                lo[k] = std::min(lo[k], v.uv[k]);
                hi[k] = std::max(hi[k], v.uv[k]);
            }
        for (gfx::MeshVertex& v : mesh.vertices)
            for (int k = 0; k < 2; ++k) v.uv[k] += v.uv[k] <= lo[k] ? uvInset : v.uv[k] >= hi[k] ? -uvInset : 0;
    }
    std::string note = what + " by modelconv (tools/modelconv)";
    if (!SaveObj(out, mesh, note.c_str())) {
        printf("can't write %s\n", out.c_str());
        return -1;
    }
    printf("%s: %d strips, %u triangles, %u materials%s -> %s\n", what.c_str(), strips,
           (unsigned)(mesh.indices.size() / 3), (unsigned)mesh.parts.size(),
           builder.missing ? " (some textures not found)" : "", out.c_str());
    return 1;
}

// A model table: u32 offset[count] (relative to the table), each a chain, 0x20-byte entries being empty stubs.
bool IsModelTable(const std::vector<uint8_t>& b) { return b.size() > 0x190 && Rd32(&b[0]) == 0x190; }

// The end of table entry `id`: the next entry's start.
size_t EntryEnd(const std::vector<uint8_t>& b, int id, int count) {
    size_t off = Rd32(&b[id * 4]), next = b.size();
    for (int k = 0; k < count; ++k) {
        size_t o = Rd32(&b[k * 4]);
        if (o > off && o < next) next = o;
    }
    return next;
}

// Creates root\a\b\... for each directory in `rel` ("a/b/file").
void MakeDirs(std::string root, const std::string& rel) {
    for (size_t p = 0, q; (q = rel.find('/', p)) != std::string::npos; p = q + 1) {
        root += "\\" + rel.substr(p, q - p);
        CreateDirectoryA(root.c_str(), nullptr);
    }
}

std::string WinPath(std::string p) {
    for (char& c : p)
        if (c == '/') c = '\\';
    return p;
}

// Converts every model of a model-table file (rel = "SYS/S00.E3D") to models/SYS/S00/<id>.obj. Returns the number
// written, or -1 on a write error.
int ConvertModelTable(const std::vector<uint8_t>& b, const std::string& rel, const std::string& data) {
    std::string stem = rel.substr(0, rel.size() - 4);
    std::vector<ManifestTexture> textures = ReadManifestFor(data, stem + ".GSL");
    std::string models = data + "\\models";
    CreateDirectoryA(models.c_str(), nullptr);
    MakeDirs(models, stem + "/");
    int written = 0;
    for (int id = 0; id < 64; ++id) {
        size_t off = Rd32(&b[id * 4]);
        if (off >= b.size() || EntryEnd(b, id, 64) - off <= 0x40) continue;   // an empty stub
        char file[16];
        snprintf(file, sizeof file, "%02d.obj", id);
        // Course n sits in the cell at (200 (n & 7), 200 (n / 8)); move it to the origin. The .mtl sits in
        // models/<DIR>/<NAME>/, three levels below the data directory.
        int r = WriteChain(b, off, -200.0f * (id & 7), -200.0f * (id / 8), GsState(), textures, "../../../textures/",
                           models + "\\" + WinPath(stem) + "\\" + file,
                           "Course " + std::to_string(id) + " from " + rel);
        if (r < 0) return -1;
        written += r;
    }
    return written;
}

// The race's sky, in the game's executable: a ceiling and a ring around the camera (D_00290A40, drawn by
// Sky_Draw 0x00225608 with the GS state packet D_0029D4A0). It uses whatever texture the course loaded at TBP 0x3CFE.
const uint32_t kElfSky = 0x00290A40;
const uint32_t kElfSkyState = 0x0029D4A0;
// Endurance Run's (course 0x11, C20) two more rings around the camera (func_002256A8): clouds and stars (D_002962E0,
// SYS/SORA.GSL's 1024x128 at TBP 0x3DFA, faded in by the night weight) and a band below the horizon (D_00298A50,
// untextured, faded in by the day weight).
const uint32_t kElfClouds = 0x002962E0;
const uint32_t kElfHorizon = 0x00298A50;
const uint64_t kCloudsTex0 = 0x2007DF45E9443DFAull, kCloudsClamp = 0x1FC00000009ull;

// The executable's file offset of address `va` (the PT_LOAD segments), or 0.
size_t ElfOffset(const std::vector<uint8_t>& elf, uint32_t va) {
    if (elf.size() < 0x34) return 0;
    uint32_t phoff = Rd32(&elf[0x1C]), phnum = elf[0x2C] | (elf[0x2D] << 8);
    for (uint32_t i = 0; i < phnum && phoff + i * 32 + 32 <= elf.size(); ++i) {
        const uint8_t* ph = &elf[phoff + i * 32];
        uint32_t type = Rd32(ph), offset = Rd32(ph + 4), vaddr = Rd32(ph + 8), filesz = Rd32(ph + 16);
        if (type == 1 && va >= vaddr && va < vaddr + filesz) return offset + (va - vaddr);
    }
    return 0;
}

// The GS state an A+D packet in the executable sets (a GIF tag, then data/register pairs).
GsState ElfGsState(const std::vector<uint8_t>& elf, uint32_t va) {
    GsState gs;
    size_t o = ElfOffset(elf, va);
    if (!o) return gs;
    uint32_t n = Rd32(&elf[o]) & 0x7FFF;
    for (uint32_t i = 0; i < n && o + 32 + i * 16 <= elf.size(); ++i) {
        const uint8_t* e = &elf[o + 16 + i * 16];
        uint64_t value = Rd32(e) | ((uint64_t)Rd32(e + 4) << 32);
        if (e[8] == 0x06 || e[8] == 0x07) gs.tex0 = value;
        if (e[8] == 0x08 || e[8] == 0x09) gs.clamp = value;
    }
    return gs;
}

// The course files by course id (the loader's table D_0029BE50 gives each id a place on the disc): ids 0-0x11 are
// COURSE/C00-C14 and C18-C20, ids 0x12-0x26 ACTION/A00-A20 (the events and the Adventure's own stages). C16 is on
// the disc but no id uses it.
const int kCourseIds = 0x27;
std::string CourseFile(int id) {
    static const char* const kCourse[0x12] = {"C00", "C01", "C02", "C03", "C04", "C05", "C06", "C07", "C08",
                                              "C09", "C10", "C11", "C12", "C13", "C14", "C18", "C19", "C20"};
    if (id < 0x12) return kCourse[id];
    char name[8];
    snprintf(name, sizeof name, "A%02d", id - 0x12);
    return name;
}

// Whether course `id`'s file has a map section (Loader_LoadCourse 0x0020C188: the race courses and ids 0x13, 0x14,
// 0x16, 0x17, 0x19, 0x1A, 0x1C, 0x25 and 0x26). Without one the objects start a word earlier.
bool CourseHasMap(int id) {
    return id < 0x12 || id == 0x13 || id == 0x14 || id == 0x16 || id == 0x17 || id == 0x19 || id == 0x1A ||
           id == 0x1C || id == 0x25 || id == 0x26;
}

// A race course, COURSE/Cnn.BIN or ACTION/Ann.BIN: u32 offsets {GS texture uploads, field, collision (PMP), map,
// objects} (no map on some ACTION files). The field is a model table of 65 entries: 0-63 the blocks of an 8x8 grid of
// 200-unit cells (block n covers x 200 (n & 7), z 200 (n / 8)), 64 the things drawn after the cars (see-through:
// fences, trees as billboards). Positions are world coordinates. Output: models/<DIR>/<NAME>/<block>.obj, plus
// map.obj, the HUD's course map, sky.obj, the executable's sky with this course's texture, and the collision data,
// collision/<NAME>.pmp.
// texconv stores an image once, so a course that uploads the same picture to several places (DBPs) has them share a
// DDS. Some of those places are animated apart (Course_ObjectsTask's copies: C12's water, A13's), so each such
// upload gets its own copy, textures/<DIR>/<FILE>.BIN/u<DBP>.dds, written if missing, and the course's models use
// it. The race's copies write to it when it exists.
void SeparateSharedUploads(std::vector<ManifestTexture>& textures, const std::string& data, const std::string& rel) {
    std::map<std::string, std::set<int>> uploads;
    for (const ManifestTexture& t : textures) uploads[t.path].insert(t.dbp);
    for (ManifestTexture& t : textures) {
        if (uploads[t.path].size() < 2) continue;
        char name[32];
        snprintf(name, sizeof name, "/u%04X.dds", t.dbp);
        std::string alias = rel + name;
        std::string from = WinPath(data + "\\textures\\" + t.path), to = WinPath(data + "\\textures\\" + alias);
        CreateDirectoryA(WinPath(data + "\\textures\\" + rel).c_str(), nullptr);
        CopyFileA(from.c_str(), to.c_str(), TRUE);   // keeps an existing copy (an edit survives)
        t.path = alias;
    }
}

int ConvertCourse(const std::vector<uint8_t>& file, const std::string& rel, const std::string& data,
                  const std::vector<uint8_t>& elf, bool hasMap) {
    if (file.size() < 0x20) return 0;
    size_t base = Rd32(&file[4]);
    if (base >= file.size()) return 0;
    std::vector<uint8_t> b(file.begin() + base, file.end());
    if (!IsModelTable(b)) {
        printf("%s: no field model table, skipped\n", rel.c_str());
        return 0;
    }
    std::string stem = rel.substr(0, rel.size() - 4);
    std::vector<ManifestTexture> textures = ReadManifestFor(data, rel);
    SeparateSharedUploads(textures, data, rel);
    std::string models = data + "\\models";
    CreateDirectoryA(models.c_str(), nullptr);
    MakeDirs(models, stem + "/");
    std::string dir = models + "\\" + WinPath(stem) + "\\";
    int written = 0;
    for (int id = 0; id < 65; ++id) {
        size_t off = Rd32(&b[id * 4]);
        if (off >= b.size() || EntryEnd(b, id, 65) - off <= 0x40) continue;
        char file[16];
        snprintf(file, sizeof file, "%02d.obj", id);
        int r = WriteChain(b, off, 0, 0, GsState(), textures, "../../../textures/", dir + file,
                           rel + " block " + std::to_string(id));
        if (r < 0) return -1;
        written += r;
    }
    // The course map the race HUD draws (RaceMap_Draw 0x002427D0): the map section is {u32 offset 0, offset 1,
    // count 0, count 1}, and the race draws part 1, program 0x50 strips of the track's outline in world coordinates
    // at y 0. map.obj.
    size_t map = Rd32(&file[12]), mapEnd = Rd32(&file[16]);
    if (hasMap && map + 16 <= mapEnd && mapEnd <= file.size()) {
        std::vector<uint8_t> m(file.begin() + map, file.begin() + mapEnd);
        size_t part = Rd32(&m[4]) / 16 * 16;
        if (Rd32(&m[12]) && part < m.size()) {
            int r = WriteChain(m, part, 0, 0, GsState(), textures, "../../../textures/", dir + "map.obj", rel + " map");
            if (r < 0) return -1;
            written += r;
        }
    }
    // The course's objects (Loader_LoadCourse, D_0177F580): header words 4 on (3 on without a map) bound one section
    // each, up to the first zero. A section is {u32 model offset, u32 texture chain offset, u32 next (0)}; the model
    // is a program 0x04 chain (lit, as the car bodies) at the model offset + 0x10, and its texture a 128x128 T8 the
    // loader moves to TBP 0x2B26 + 0x44 n (CBP + 0x40), which every section's upload has at 0 / 0x40 in the file. The
    // n-th such texture in the manifest is section n's. objN.obj; the race's course task places them
    // (docs/race.md).
    //
    // A world block (Loader_LoadField 0x0020BAC8) has one section of up to 6 models, {u32 model offset ..., u32
    // texture chain offset, 0}, into D_0177F580 + 0x18 x slot. Its upload goes where the file says (the first
    // BITBLTBUF's DBP), and the field's handlers draw the models with that picture (func_0025D028's table, e.g.
    // func_0025D5F8's TEX0 at 0x3CB8 for block 39's windmill sails). objN.obj is the section's model N.
    bool worldBlock = stem.compare(0, 4, "FLD/") == 0;
    // Block 11 (FLD/023, My City and My Garage; Loader_LoadField's block-0xB branch) has 0x34 more model tables and
    // 0x1A more PMPs between its map and its objects: header words 4-0x37 and 0x38-0x51, the objects at word 0x52.
    // Table 2k is the lot of My City's resident k before they move in, 2k + 1 their building; each has one cell's
    // model and maybe an entry 64 (the see-through things drawn after the cars). city_II.obj and city_II_64.obj,
    // and collision/FLD/023/city_KK.pmp (docs/field.md, "My City's buildings").
    bool city = stem == "FLD/023" && file.size() >= 0x54 * 4 && Rd32(&file[0x53 * 4]) == file.size();
    if (city) {
        for (int i = 0; i < 0x34; ++i) {
            size_t s = Rd32(&file[(4 + i) * 4]), e = Rd32(&file[(5 + i) * 4]);
            std::vector<uint8_t> t(file.begin() + s, file.begin() + e);
            if (!IsModelTable(t)) continue;
            for (int id = 0; id < 65; ++id) {
                size_t off = Rd32(&t[id * 4]);
                if (off >= t.size() || EntryEnd(t, id, 65) - off <= 0x40) continue;
                char name[24];
                if (id == 64) snprintf(name, sizeof name, "city_%02d_64.obj", i);
                else snprintf(name, sizeof name, "city_%02d.obj", i);
                int r = WriteChain(t, off, 0, 0, GsState(), textures, "../../../textures/", dir + name,
                                   rel + " city table " + std::to_string(i) + " entry " + std::to_string(id));
                if (r < 0) return -1;
                written += r;
            }
        }
        std::string dirC = data + "\\collision\\FLD\\023";
        CreateDirectoryA((data + "\\collision").c_str(), nullptr);
        CreateDirectoryA((data + "\\collision\\FLD").c_str(), nullptr);
        CreateDirectoryA(dirC.c_str(), nullptr);
        for (int k = 0; k < 0x1A; ++k) {
            size_t s = Rd32(&file[(0x38 + k) * 4]), e = Rd32(&file[(0x39 + k) * 4]);
            char name[24];
            snprintf(name, sizeof name, "\\city_%02d.pmp", k);
            FILE* fp = fopen((dirC + name).c_str(), "wb");
            if (!fp || fwrite(&file[s], 1, e - s, fp) != e - s) {
                if (fp) fclose(fp);
                printf("can't write %s\n", (dirC + name).c_str());
                return -1;
            }
            fclose(fp);
        }
    }
    // (C03's last door, sections 2 and 3, has its CLUT at 0x20 instead: func_002423B8's TEX0s for courses 3 and 0x13
    // make those two objects T4 128x128 with CBP = TBP + 0x20.)
    std::vector<ManifestTexture> objectTextures;
    for (const ManifestTexture& t : textures)
        if (t.dbp == 0 && (t.cbp == 0x40 || t.cbp == 0x20)) objectTextures.push_back(t);
    size_t words = city ? 0x54 : std::min<size_t>(Rd32(&file[0]), 0x30) / 4;
    for (size_t k = city ? 0x52 : hasMap ? 4 : 3, n = 0; k + 1 < words && Rd32(&file[(k + 1) * 4]); ++k) {
        size_t s = Rd32(&file[k * 4]), e = Rd32(&file[(k + 1) * 4]);
        if (s + 16 > e || e > file.size()) break;
        std::vector<uint8_t> m(file.begin() + s, file.begin() + e);
        size_t count = 1;
        while (worldBlock && (count + 2) * 4 + 4 <= m.size() && Rd32(&m[(count + 2) * 4])) ++count;
        if (worldBlock && count > 6) {
            printf("%s: objects not in the one-section layout, skipped\n", rel.c_str());   // "MISC OBJ COUNT OVER !"
            break;
        }
        // A world block's upload: its pictures in order, at their own addresses (the handlers' TEX0s are T8 at
        // them, CBP their CLUTs').
        std::vector<const ManifestTexture*> pictures;
        if (worldBlock) {
            size_t chain = Rd32(&m[count * 4]);
            for (size_t p = chain; p + 16 <= m.size();) {
                uint32_t tag = Rd32(&m[p]), qwc = tag & 0xFFFF;
                if ((tag >> 24) == 0x70) break;
                for (size_t q = p + 16; q + 16 <= p + 16 + (size_t)qwc * 16 && q + 16 <= m.size(); q += 16) {
                    if (m[q + 8] != 0x50) continue;   // A+D BITBLTBUF
                    int dbp = (int)(Rd32(&m[q + 4]) & 0x3FFF);
                    for (const ManifestTexture& t : textures)
                        if (t.dbp == dbp) {
                            pictures.push_back(&t);
                            break;
                        }
                    break;
                }
                p += 16 + (size_t)qwc * 16;
            }
        }
        for (size_t j = 0; j < count; ++j, ++n) {
            size_t model = Rd32(&m[j * 4]) + 0x10;
            if (model >= m.size()) continue;
            std::vector<ManifestTexture> own;
            GsState gs;   // Course_ClearObjects' TEX0 (D_0177EA30), at the file's addresses
            gs.tex0 = 0x40ull << 37 | 7ull << 30 | 7ull << 26 | 0x13ull << 20 | 2ull << 14 | 1ull << 34;
            if (worldBlock) {
                // The upload's first picture; My City's three objects (func_0025C088) each take their own, the
                // first with CLAMP_2 4 (s repeats).
                const ManifestTexture* found = pictures.empty() ? nullptr : pictures[city && j < pictures.size() ? j : 0];
                if (found) {
                    own.push_back(*found);
                    int tw = 0, th = 0;
                    while ((1 << tw) < found->width) ++tw;
                    while ((1 << th) < found->height) ++th;
                    gs.tex0 = (uint64_t)found->dbp | 2ull << 14 | 0x13ull << 20 | (uint64_t)tw << 26 |
                              (uint64_t)th << 30 | 1ull << 34 | (uint64_t)found->cbp << 37;
                    gs.clamp = city && j == 0 ? 4 : 5;   // the objects' GS state (D_0029D240): clamped both ways
                }
            } else if (n < objectTextures.size()) {
                own.push_back(objectTextures[n]);
                gs.tex0 = (gs.tex0 & ~(0x3FFFull << 37)) | (uint64_t)objectTextures[n].cbp << 37;
            }
            char name[16];
            snprintf(name, sizeof name, "obj%d.obj", (int)n);
            int r = WriteChain(m, model, 0, 0, gs, own, "../../../textures/", dir + name,
                               rel + " object " + std::to_string(n));
            if (r < 0) return -1;
            // A model with nothing in part 0 but strips in part 1 (header {off0, off1, n0, n1}): block 22's UFO,
            // the only one, all program 0x50 (unlit, func_002234C8 draws both parts).
            size_t header = Rd32(&m[j * 4]);
            if (r == 0 && worldBlock && header + 16 <= m.size() && Rd32(&m[header + 12])) {
                r = WriteChain(m, header + (Rd32(&m[header + 4]) & ~15u), 0, 0, gs, own, "../../../textures/",
                               dir + name, rel + " object " + std::to_string(n) + " part 1");
                if (r < 0) return -1;
            }
            written += r;
        }
    }
    size_t sky = ElfOffset(elf, kElfSky);
    // The world's blocks share one sky (models/FLD/sky.obj)
    if (sky && !worldBlock) {
        int r = WriteChain(elf, sky, 0, 0, ElfGsState(elf, kElfSkyState), textures, "../../../textures/",
                           dir + "sky.obj", rel + " sky");
        if (r < 0) return -1;
        written += r;
    } else if (!sky) {
        printf("%s: no sky (the executable SLUS_203.98 wasn't found)\n", rel.c_str());
    }
    // Cloud Hill's field (ACTION/A16, Adventure only) draws the clouds ring always and no band (func_00220B20). A16
    // uploads its own ring picture to the ring's 0x3DFA after the field's SORA.GSL (GUESS: the loader's jobs run in
    // order, so A16's is the one seen).
    if (sky && (stem == "COURSE/C20" || stem == "ACTION/A16")) {
        GsState clouds;
        clouds.tex0 = kCloudsTex0;
        clouds.clamp = kCloudsClamp;
        bool a16 = stem == "ACTION/A16";
        int r = WriteChain(elf, ElfOffset(elf, kElfClouds), 0, 0, clouds,
                           a16 ? textures : ReadManifestFor(data, "SYS/SORA.GSL"), "../../../textures/",
                           dir + "clouds.obj", rel + " clouds");
        if (r < 0) return -1;
        written += r;
    }
    if (sky && stem == "COURSE/C20") {
        int r = WriteChain(elf, ElfOffset(elf, kElfHorizon), 0, 0, GsState(), textures, "../../../textures/",
                       dir + "horizon.obj", rel + " horizon");
        if (r < 0) return -1;
        written += r;
    }
    // The collision data (PMP), as the file has it: collision/Cnn.pmp (format in data/README.md). Its offsets are
    // relative to its start, so it needs no conversion. ACTION/A03, A09, A12, A14 and A17 end a few bytes before
    // their PMP's end word: the PS2 reads whole sectors, so the rest is the sector's padding, written as zeros.
    size_t pmp = Rd32(&file[8]), pmpEnd = Rd32(&file[12]);
    if (pmp + 0x608 <= pmpEnd && pmp < file.size() && pmpEnd <= ((file.size() + 0x7FF) & ~(size_t)0x7FF)) {
        std::vector<uint8_t> pmpData(pmpEnd - pmp, 0);
        memcpy(pmpData.data(), &file[pmp], (pmpEnd < file.size() ? pmpEnd : file.size()) - pmp);
        std::string dirC = data + "\\collision";
        CreateDirectoryA(dirC.c_str(), nullptr);
        std::string name = stem.substr(stem.find_last_of('/') + 1);
        if (stem.compare(0, 4, "FLD/") == 0) {   // the world's blocks: collision/FLD/<abc>.pmp
            dirC += "\\FLD";
            CreateDirectoryA(dirC.c_str(), nullptr);
        }
        std::string out = dirC + "\\" + name + ".pmp";
        FILE* fp = fopen(out.c_str(), "wb");
        if (!fp || fwrite(pmpData.data(), 1, pmpData.size(), fp) != pmpData.size()) {
            if (fp) fclose(fp);
            printf("can't write %s\n", out.c_str());
            return -1;
        }
        fclose(fp);
        printf("%s collision: %u bytes -> %s\n", rel.c_str(), (unsigned)(pmpEnd - pmp), out.c_str());
    }
    return written;
}

// My Garage's rooms, SHOP/GARAGE.BIN: ten chunks of 0x53000 bytes, interior n (save +0x652) in chunk n (Loader_RoomStep's
// town-0 branch, 0x0020CDC0). A chunk is {u32 model offset, model end, ...}; the model is {u32 offset A, offset B, count
// A, count B}, its main chain at +0x10 and chain B at offset B, both drawn (Vu1PacketAddCall, func_002234C8 with mask
// 2), under GS state 0x20045645E53221B2 (func_00226C28: TEX0 T8 at 0x21B2, CLUT 0x22B2, the chunk's 512 x 128
// picture). The GS uploads follow the model: the shared 640 x 256 sheet (the room's sprites, 000.dds) and the chunk's
// own picture, the n-th 512 x 128 one in the file's manifest. models/SHOP/GARAGE/roomN.obj, roomN_b.obj.
int ConvertGarage(const std::vector<uint8_t>& b, const std::string& data) {
    const size_t kChunk = 0x53000;
    std::vector<ManifestTexture> all = ReadManifestFor(data, "SHOP/GARAGE.BIN"), rooms;
    for (const ManifestTexture& t : all)
        if (t.width == 512 && t.height == 128) rooms.push_back(t);
    std::string dir = data + "\\models\\SHOP";
    CreateDirectoryA((data + "\\models").c_str(), nullptr);
    CreateDirectoryA(dir.c_str(), nullptr);
    dir += "\\GARAGE";
    CreateDirectoryA(dir.c_str(), nullptr);
    GsState gs;
    gs.tex0 = 0x20045645E53221B2ull;
    gs.clamp = 5;
    int written = 0;
    for (size_t n = 0; n < 10 && (n + 1) * kChunk <= b.size() && n < rooms.size(); ++n) {
        std::vector<uint8_t> c(b.begin() + n * kChunk, b.begin() + (n + 1) * kChunk);
        size_t model = Rd32(&c[0]), end = Rd32(&c[4]);
        if (model + 0x20 > end || end > c.size()) continue;
        std::vector<ManifestTexture> own(1, rooms[n]);
        for (const ManifestTexture& t : all)
            if (t.width == 640) {
                own.push_back(t);
                break;
            }
        std::string name = dir + "\\room" + std::to_string(n);
        int r = WriteChain(c, model + 0x10, 0, 0, gs, own, "../../../textures/", name + ".obj",
                           "SHOP/GARAGE.BIN room " + std::to_string(n));
        if (r < 0) return -1;
        written += r;
        uint32_t at = Rd32(&c[model + 4]), count = Rd32(&c[model + 12]);
        if (count && model + at + 0x10 <= end) {
            r = WriteChain(c, model + (at & ~15u), 0, 0, gs, own, "../../../textures/", name + "_b.obj",
                           "SHOP/GARAGE.BIN room " + std::to_string(n) + " B");
            if (r < 0) return -1;
            written += r;
        }
    }
    return written;
}

// The car's shadow, in the executable: a quad of 2 x 3 under the car drawn with the car's own texture, whose
// bottom right quarter is the shadow (Cars_Draw 0x002228A8, program 0x50).
const uint32_t kElfCarShadow = 0x00290570;

// TEX0 for a 128x128 texture at block 0 of its file with its CLUT at `cbp` (the files' layout before the loader
// moves them; the textures' manifests keep these addresses).
GsState FileTexture(int psm, int cbp) {
    GsState gs;
    gs.tex0 = (2ull << 14) | ((uint64_t)psm << 20) | (7ull << 26) | (7ull << 30) | (1ull << 34) | ((uint64_t)cbp << 37);
    gs.clamp = 5;   // CLAMP_2 of the cars' GS state (D_0029D240): clamp both ways
    return gs;
}

// A car file (CARn/Qnn.BIN, CARS/Q150.BIN; Loader_LoadCar 0x0020C7B8) or CARS/TIRE.BIN (func_0020AEF0): u32
// offsets {six sections, GS texture uploads, end}. Only the part before the uploads is kept in memory, and the loader
// points D_0177F310[car] (tires: D_0177EAF8) at the six sections. A section is {u32 offset A, offset B, count A,
// count B}, then at +0x10 its main chain (program 0x20), then chains A and B (program 0x50, the car's lights:
// func_00222360 draws A, the headlights, of section 0 or 1 and B, the brake lights, of sections 0 and 2). Bodies:
// section 0 near, 1 far, 2 or 3 by the car's part flags (bit 0x1000), 4 and 5 by bits 0x2000/0x4000. Tires: 0 and 1
// the front ones, 2 the rear pair, 3 all four seen from far, 4 and 5 another set (part flag 0x400).
// Output: models/<DIR>/<NAME>/<section>.obj, the lights as <section>_head.obj and <section>_brake.obj, and for a body
// shadow.obj (kElfCarShadow with its texture).
int ConvertCar(const std::vector<uint8_t>& b, const std::string& rel, const std::string& data,
               const std::vector<uint8_t>& elf, const GsState& gs) {
    if (b.size() < 0x30) return 0;
    uint32_t h[8];
    for (int i = 0; i < 8; ++i) h[i] = Rd32(&b[i * 4]);
    if (h[0] != 0x30 || h[7] != b.size()) {
        printf("%s: not a car file, skipped\n", rel.c_str());
        return 0;
    }
    std::string stem = rel.substr(0, rel.size() - 4);
    std::vector<ManifestTexture> textures = ReadManifestFor(data, rel);
    std::string models = data + "\\models";
    CreateDirectoryA(models.c_str(), nullptr);
    MakeDirs(models, stem + "/");
    std::string dir = models + "\\" + WinPath(stem) + "\\";
    // The texture's path relative to the .mtl: models/<DIR>/<NAME>/ is three levels below the data directory.
    const std::string prefix = "../../../textures/";
    // A body's sections 4 and 5 (the jet turbines, part flag 0x2000, and 0x4000's part) aren't in its own texture:
    // Car_Draw sets CARS/PARTS.BIN's picture (TEX0 T8 at 0x1BC2, then TEX2 T4 with its CLUT at 0x1BE2) in palette 10
    // or 11. That's the file's T4 at 0 with its CLUT at 0x20, loaded at 0x1BC2.
    bool body = rel.find("TIRE") == std::string::npos;
    std::vector<ManifestTexture> parts;
    if (body) parts = ReadManifestFor(data, "CARS/PARTS.BIN");
    int written = 0;
    for (int s = 0; s < 6; ++s) {
        if (h[s] + 0x20 > h[s + 1]) continue;
        bool part = body && s >= 4;
        GsState partGs = FileTexture(0x14, 0x20);
        partGs.tex0 |= (uint64_t)(10 + s - 4) << 56;
        int r = WriteChain(b, h[s] + 0x10, 0, 0, part ? partGs : gs, part ? parts : textures, prefix,
                           dir + std::to_string(s) + ".obj", rel + " section " + std::to_string(s));
        if (r < 0) return -1;
        written += r;
        // The lights (func_00222360): chain A (headlights) and B (brake lights) at the section's offsets, drawn
        // when their count isn't 0, in the car's texture.
        for (int c = 0; c < 2 && body; ++c) {
            uint32_t at = Rd32(&b[h[s] + c * 4]), count = Rd32(&b[h[s] + 8 + c * 4]);
            if (!count || h[s] + at + 0x10 > h[s + 1]) continue;
            std::string name = std::to_string(s) + (c ? "_brake" : "_head");
            r = WriteChain(b, h[s] + (at & ~15u), 0, 0, gs, textures, prefix, dir + name + ".obj", rel + " " + name);
            if (r < 0) return -1;
            written += r;
        }
    }
    size_t shadow = ElfOffset(elf, kElfCarShadow);
    if (shadow && body) {
        // Half a texel in from the quarter's edges: the quarter next to it is opaque up to the border, and the
        // filter would draw that column as a line along the shadow's edge.
        int r = WriteChain(elf, shadow, 0, 0, gs, textures, prefix, dir + "shadow.obj", rel + " shadow", 0.5f / 128);
        if (r < 0) return -1;
    }
    return written;
}

// CARS/PARTS.BIN (func_0020AEF0 step 3): u32 offsets of 12 models (the car file's section layout), then at +0x30 its
// GS uploads, kept from 0x1BC2. D_0177EAC8[n] points at model n. Car_Draw draws model n (0-8) for part flag 1 << n
// with the file's T4 picture in palette n (TEX2 CSA n); part 6 (0x40, the propellers) is model 6 and four of model
// 7 near, model 11 far (still in palette 6). Model 9 (part 0x200) takes the T8 picture 1 + 10 x the record's +7
// (here picture 1); model 10 (part 0x400, the other tires) CARS/TIRE.BIN's picture.
// Output: models/CARS/PARTS/<n>.obj.
int ConvertParts(const std::vector<uint8_t>& b, const std::string& data) {
    const int kModels = 12;
    if (b.size() < 0x40 || Rd32(&b[0]) != 0x40) {
        printf("CARS/PARTS.BIN: not understood, skipped\n");
        return 0;
    }
    std::vector<ManifestTexture> parts = ReadManifestFor(data, "CARS/PARTS.BIN");
    std::vector<ManifestTexture> tires = ReadManifestFor(data, "CARS/TIRE.BIN");
    std::string models = data + "\\models";
    CreateDirectoryA(models.c_str(), nullptr);
    MakeDirs(models, "CARS/PARTS/");
    std::string dir = models + "\\CARS\\PARTS\\";
    int written = 0;
    for (int n = 0; n < kModels; ++n) {
        uint32_t off = Rd32(&b[n * 4]), end = Rd32(&b[n * 4 + 4]);
        if (off + 0x20 > end || end > b.size()) continue;
        GsState gs = FileTexture(0x14, 0x20);
        if (n == 9) {
            gs.tex0 = 0x22 | (1ull << 14) | (0x13ull << 20) | (6ull << 26) | (5ull << 30) | (1ull << 34) |
                      (0x2Aull << 37);   // T8 64x32
        } else {
            gs.tex0 |= (uint64_t)(n == 11 ? 6 : n) << 56;
        }
        int r = WriteChain(b, off + 0x10, 0, 0, gs, n == 10 ? tires : parts, "../../../textures/",
                           dir + std::to_string(n) + ".obj", "CARS/PARTS.BIN model " + std::to_string(n));
        if (r < 0) return -1;
        written += r;
    }
    return written;
}

// CARS/WHEEL.BIN (func_0020AEF0, D_0177AB30[16]): u32 offsets of 16 models (0x50 the first), each in the car file's
// section layout, all program 0x20 and untextured. Model 0 is the tire, models 1-15 the hubs of Wheels part items
// 0-14 (their painted batches take the wheel colour). The close-up wheels (func_00221570) and the Wheel card draw
// them. Output: models/CARS/WHEEL/<n>.obj.
int ConvertWheels(const std::vector<uint8_t>& b, const std::string& data) {
    const int kModels = 16;
    if (b.size() < 0x50 || Rd32(&b[0]) != 0x50) {
        printf("CARS/WHEEL.BIN: not understood, skipped\n");
        return 0;
    }
    std::string models = data + "\\models";
    CreateDirectoryA(models.c_str(), nullptr);
    MakeDirs(models, "CARS/WHEEL/");
    std::string dir = models + "\\CARS\\WHEEL\\";
    int written = 0;
    for (int n = 0; n < kModels; ++n) {
        uint32_t off = Rd32(&b[n * 4]), end = n + 1 < kModels ? Rd32(&b[n * 4 + 4]) : (uint32_t)b.size();
        if (off + 0x20 > end || end > b.size()) continue;
        int r = WriteChain(b, off + 0x10, 0, 0, FileTexture(0x14, 0x20), {}, "../../../textures/",
                           dir + std::to_string(n) + ".obj", "CARS/WHEEL.BIN model " + std::to_string(n));
        if (r < 0) return -1;
        written += r;
    }
    return written;
}

void FindFiles(const std::string& dir, const std::string& rel, const char* ext, std::vector<std::string>& out) {
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA((dir + "\\*").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        std::string name = fd.cFileName;
        if (name == "." || name == "..") continue;
        std::string r = rel.empty() ? name : rel + "/" + name;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            FindFiles(dir + "\\" + name, r, ext, out);
        else if (name.size() > 4 && _stricmp(name.c_str() + name.size() - 4, ext) == 0)
            out.push_back(r);
    } while (FindNextFileA(h, &fd));
    FindClose(h);
}

}  // namespace

// The opponents' paths, in the executable (docs/ai.md). D_002BD578 holds a {lines, zones} pointer pair per path:
// lines are 0x18 bytes {x0, z0, x1, z1, split x, split z}, short gates across the racing line; zones are 8 bytes
// {in line, out line, previous, previous alternative, next, next alternative, rank, 0}, the stretches between two
// gates. The zones follow the lines, and the next path's lines follow the zones. Ai_Init (0x0024FCD8) picks the path
// by course id (0x0B uses 0x06's, 0x10 0x0F's) and, in a Quick Race, the speed table D_002A9B80[D_002AC098[
// D_002AC080[path]]]: one byte per zone, the speed to hold there in 5 km/h, 0 for none. Path 0x13 has the table
// D_002ABE80 and the later ones D_002ABF80 (0x1C, 0x25, 0x26 have paths). Output: ai/<NAME>.txt.
const uint32_t kElfAiPaths = 0x002BD578, kElfAiSpeeds = 0x002A9B80, kElfAiGroup = 0x002AC080,
               kElfAiSpeedTable = 0x002AC098, kElfAiSpeeds13 = 0x002ABE80, kElfAiSpeedsLate = 0x002ABF80;
const int kAiPaths = kCourseIds;

int ConvertAiPaths(const std::vector<uint8_t>& elf, const std::string& data) {
    size_t table = ElfOffset(elf, kElfAiPaths);
    if (!table) {
        printf("no AI paths (the executable SLUS_203.98 wasn't found)\n");
        return 0;
    }
    // Where each path's zones end: at the next table in memory.
    std::vector<uint32_t> starts;
    for (int t = 0; t < kAiPaths; ++t)
        if (Rd32(&elf[table + t * 8])) {
            starts.push_back(Rd32(&elf[table + t * 8]));
            starts.push_back(Rd32(&elf[table + t * 8 + 4]));
        }
    starts.push_back(kElfAiPaths);   // the last path's zones end at the table
    std::string dir = data + "\\ai";
    CreateDirectoryA(dir.c_str(), nullptr);
    int written = 0;
    for (int id = 0; id < kCourseIds; ++id) {
        int t = id == 0x0B ? 0x06 : id == 0x10 ? 0x0F : id;
        uint32_t lines = Rd32(&elf[table + t * 8]), zones = Rd32(&elf[table + t * 8 + 4]), end = 0xFFFFFFFF;
        for (uint32_t s : starts)
            if (s > zones && s < end) end = s;
        size_t lo = ElfOffset(elf, lines), zo = ElfOffset(elf, zones);
        size_t nl = (zones - lines) / 0x18, nz = end != 0xFFFFFFFF ? (end - zones) / 8 : 0;
        if (!lines || !lo || !zo || nz == 0 || nz > 0x100) continue;
        const uint8_t* speed;
        if (t >= 0x12) {
            speed = &elf[ElfOffset(elf, t == 0x13 ? kElfAiSpeeds13 : kElfAiSpeedsLate)];
        } else {
            size_t group = elf[ElfOffset(elf, kElfAiSpeedTable) + elf[ElfOffset(elf, kElfAiGroup) + t]];
            speed = &elf[ElfOffset(elf, kElfAiSpeeds) + group * 0x100];
        }
        std::string name = CourseFile(id);
        std::string out = dir + "\\" + name + ".txt";
        FILE* fp = fopen(out.c_str(), "w");
        if (!fp) {
            printf("can't write %s\n", out.c_str());
            return -1;
        }
        fprintf(fp, "# The opponents' path on course %d (%s), from SLUS_203.98 by modelconv. See docs/ai.md.\n", id,
                name.c_str());
        fprintf(fp, "# line x0 z0 x1 z1 splitX splitZ: a gate across the racing line\n");
        for (size_t i = 0; i < nl; ++i) {
            const uint8_t* l = &elf[lo + i * 0x18];
            fprintf(fp, "line %.9g %.9g %.9g %.9g %.9g %.9g\n", RdF(l), RdF(l + 4), RdF(l + 8), RdF(l + 12),
                    RdF(l + 16), RdF(l + 20));
        }
        fprintf(fp, "# zone in out prev prevAlt next nextAlt rank speed: the stretch from gate in to gate out, its\n"
                    "# neighbours (two where the path forks), its place for the race order, its speed (5 km/h, 0 any)\n");
        for (size_t i = 0; i < nz; ++i) {
            const uint8_t* z = &elf[zo + i * 8];
            fprintf(fp, "zone %d %d %d %d %d %d %d %d\n", z[0], z[1], z[2], z[3], z[4], z[5], z[6], speed[i] & 0x7F);
        }
        fclose(fp);
        printf("AI path %d: %u gates, %u zones -> %s\n", id, (unsigned)nl, (unsigned)nz, out.c_str());
        ++written;
    }
    return written;
}

// The towns' cars on the field, in the executable (docs/field.md, "Town cars"). D_002BE438 holds 8 bytes a town
// {name, s16 block, s8 places, s8 cars}; the cars are the town's driver records after its places' people
// (D_002C22C8[town] + 16 x (places + i): {paint, body, u16 part flags at +8, name}). D_002C2E78[town] points at each
// car's start {f32 x, y, z, u16 yaw} in the town's block (Car_Init 0x00218458), and D_002D98A8[town] at each car's
// {gates, zones} (func_0025FBC0): a closed loop in the race paths' format whose zones follow its gates, as many of
// each. A car without one stands. Zone links past the loop's end (a slip in the data: Papaya Island's car 1) are
// written as the loop's own (k - 1, k + 1). Output: ai/FLD/T<town>_<car>.txt, "car RECORD X Y Z YAW FLAGS" and the
// path, for towns 1-21.
const uint32_t kElfTowns = 0x002BE438, kElfDriverRecords = 0x002C22C8, kElfTownStarts = 0x002C2E78,
               kElfTownPaths = 0x002D98A8;

int ConvertTownCars(const std::vector<uint8_t>& elf, const std::string& data) {
    size_t towns = ElfOffset(elf, kElfTowns), starts = ElfOffset(elf, kElfTownStarts);
    size_t paths = ElfOffset(elf, kElfTownPaths), records = ElfOffset(elf, kElfDriverRecords);
    if (!towns || !starts || !paths || !records) return 0;
    std::string dir = data + "\\ai\\FLD";
    CreateDirectoryA((data + "\\ai").c_str(), nullptr);
    CreateDirectoryA(dir.c_str(), nullptr);
    int written = 0;
    for (int t = 1; t < 22; ++t) {
        const uint8_t* town = &elf[towns + t * 8];
        int places = (int8_t)town[6], cars = (int8_t)town[7];
        size_t start = ElfOffset(elf, Rd32(&elf[starts + t * 4]));
        size_t path = ElfOffset(elf, Rd32(&elf[paths + t * 4]));
        size_t record = ElfOffset(elf, Rd32(&elf[records + t * 4]));
        if (cars <= 0 || !start || !record) continue;
        for (int i = 0; i < cars; ++i) {
            const uint8_t* s = &elf[start + i * 16];
            const uint8_t* r = &elf[record + (places + i) * 16];
            char file[32];
            snprintf(file, sizeof file, "\\T%02d_%02d.txt", t, i);
            const std::string name = dir + file;
            FILE* fp = fopen(name.c_str(), "w");
            if (!fp) {
                printf("can't write %s\n", name.c_str());
                return -1;
            }
            fprintf(fp, "# Town %d's car %d on the field, from SLUS_203.98 by modelconv. See docs/field.md.\n", t, i);
            fprintf(fp, "# car RECORD X Y Z YAW FLAGS: its driver record in the town's group, its start in the town's\n"
                        "# block and its heading (0 +z, 0x4000 +x), the record's part flags\n");
            fprintf(fp, "car %d %.9g %.9g %.9g 0x%04X 0x%04X\n", places + i, RdF(s), RdF(s + 4), RdF(s + 8),
                    s[12] | s[13] << 8, r[8] | r[9] << 8);
            uint32_t gates = path ? Rd32(&elf[path + i * 8]) : 0, zones = path ? Rd32(&elf[path + i * 8 + 4]) : 0;
            size_t go = gates ? ElfOffset(elf, gates) : 0, zo = zones ? ElfOffset(elf, zones) : 0;
            int n = go && zo && zones > gates ? (int)((zones - gates) / 0x18) : 0;
            if (n > 0x100) n = 0;
            if (n > 0) {
                fprintf(fp, "# line x0 z0 x1 z1 splitX splitZ, then zone in out prev prevAlt next nextAlt rank speed\n");
                for (int k = 0; k < n; ++k) {
                    const uint8_t* l = &elf[go + k * 0x18];
                    fprintf(fp, "line %.9g %.9g %.9g %.9g %.9g %.9g\n", RdF(l), RdF(l + 4), RdF(l + 8), RdF(l + 12),
                            RdF(l + 16), RdF(l + 20));
                }
                for (int k = 0; k < n; ++k) {
                    uint8_t z[8];
                    memcpy(z, &elf[zo + k * 8], 8);
                    const int loop[6] = {k, (k + 1) % n, (k + n - 1) % n, (k + n - 1) % n, (k + 1) % n, (k + 1) % n};
                    for (int j = 0; j < 6; ++j)
                        if (z[j] >= n) z[j] = (uint8_t)loop[j];
                    fprintf(fp, "zone %d %d %d %d %d %d %d 0\n", z[0], z[1], z[2], z[3], z[4], z[5], z[6]);
                }
            }
            fclose(fp);
            ++written;
        }
    }
    // The ending's towns 22-31 (func_0026CA48's scenes 0-9). Field_CreateTasks' ending mode puts the player's car in
    // slot 0 and Field_TownCarsTask the town's cars in slots 1 on (town 22, Cloud Hill: the two teammates in slots 1
    // and 2 first). A slot's start row and its path (the ending's flag 0x80000 in Ai_Init) are both the slot's own.
    // Only slot 0 has a path; the rest stand. RECORD is the group index, or -1 the save's car 0, -2/-3 its cars 1/2.
    // Town 22's last two cars would read past its start table into Peach Town's: they're left out.
    std::vector<uint32_t> startPtrs;
    for (int t = 0; t < 32; ++t) startPtrs.push_back(Rd32(&elf[starts + t * 4]));
    for (int t = 22; t < 32; ++t) {
        const uint8_t* town = &elf[towns + t * 8];
        int cars = (int8_t)town[7];
        uint32_t startVa = Rd32(&elf[starts + t * 4]);
        size_t start = ElfOffset(elf, startVa);
        size_t path = ElfOffset(elf, Rd32(&elf[paths + t * 4]));
        size_t record = ElfOffset(elf, Rd32(&elf[records + t * 4]));
        if (!start) continue;
        uint32_t endVa = 0xFFFFFFFF;
        for (uint32_t p : startPtrs)
            if (p > startVa && p < endVa) endVa = p;
        int rows = (int)((endVa - startVa) / 16);
        const int team = t == 22 ? 2 : 0;
        for (int slot = 0; slot <= cars + team && slot < rows; ++slot) {
            const uint8_t* s = &elf[start + slot * 16];
            int rec = slot == 0 ? -1 : slot <= team ? -1 - slot : slot - 1 - team;
            uint16_t flags = 0;
            if (rec >= 0) {
                if (!record) continue;
                const uint8_t* r = &elf[record + rec * 16];
                flags = (uint16_t)(r[8] | r[9] << 8);
            }
            char file[32];
            snprintf(file, sizeof file, "\\T%02d_%02d.txt", t, slot);
            const std::string name = dir + file;
            FILE* fp = fopen(name.c_str(), "w");
            if (!fp) {
                printf("can't write %s\n", name.c_str());
                return -1;
            }
            fprintf(fp, "# The ending's town %d (scene %d), slot %d, from SLUS_203.98 by modelconv. See docs/finish.md.\n",
                    t, t - 22, slot);
            fprintf(fp, "# car RECORD X Y Z YAW FLAGS: its driver record in the town's group (-1 the save's car 0, -2/-3\n"
                        "# its cars 1/2), its start in the town's block and its heading, the record's part flags\n");
            fprintf(fp, "car %d %.9g %.9g %.9g 0x%04X 0x%04X\n", rec, RdF(s), RdF(s + 4), RdF(s + 8),
                    s[12] | s[13] << 8, flags);
            uint32_t gates = path ? Rd32(&elf[path + slot * 8]) : 0, zones = path ? Rd32(&elf[path + slot * 8 + 4]) : 0;
            size_t go = gates ? ElfOffset(elf, gates) : 0, zo = zones ? ElfOffset(elf, zones) : 0;
            int n = go && zo && zones > gates ? (int)((zones - gates) / 0x18) : 0;
            if (n > 0x100) n = 0;
            if (n > 0) {
                fprintf(fp, "# line x0 z0 x1 z1 splitX splitZ, then zone in out prev prevAlt next nextAlt rank speed\n");
                for (int k = 0; k < n; ++k) {
                    const uint8_t* l = &elf[go + k * 0x18];
                    fprintf(fp, "line %.9g %.9g %.9g %.9g %.9g %.9g\n", RdF(l), RdF(l + 4), RdF(l + 8), RdF(l + 12),
                            RdF(l + 16), RdF(l + 20));
                }
                for (int k = 0; k < n; ++k) {
                    uint8_t z[8];
                    memcpy(z, &elf[zo + k * 8], 8);
                    const int loop[6] = {k, (k + 1) % n, (k + n - 1) % n, (k + n - 1) % n, (k + 1) % n, (k + 1) % n};
                    for (int j = 0; j < 6; ++j)
                        if (z[j] >= n) z[j] = (uint8_t)loop[j];
                    fprintf(fp, "zone %d %d %d %d %d %d %d 0\n", z[0], z[1], z[2], z[3], z[4], z[5], z[6]);
                }
            }
            fclose(fp);
            ++written;
        }
    }
    printf("town cars: %d -> %s\n", written, dir.c_str());
    return written;
}

// The trackside cameras of the finish and the replay, in the executable (docs/finish.md): D_002A2A50 points to a
// list per course id of 0x20-byte entries {f32 x, y, z, radius, zoom; s16 eye x, y, z, reach; u32 flags}, the last
// with flag 0x8000. Camera_TrackSide (0x00238B18) uses the first whose sphere holds the car. Output:
// camera/<NAME>.txt.
const uint32_t kElfTrackCameras = 0x002A2A50;

int ConvertTrackCameras(const std::vector<uint8_t>& elf, const std::string& data) {
    size_t table = ElfOffset(elf, kElfTrackCameras);
    if (!table) return 0;
    std::string dir = data + "\\camera";
    CreateDirectoryA(dir.c_str(), nullptr);
    int written = 0;
    for (int id = 0; id < kCourseIds; ++id) {
        size_t list = ElfOffset(elf, Rd32(&elf[table + id * 4]));
        if (!list) continue;
        std::string name = CourseFile(id);
        std::string out = dir + "\\" + name + ".txt";
        FILE* fp = fopen(out.c_str(), "w");
        if (!fp) {
            printf("can't write %s\n", out.c_str());
            return -1;
        }
        fprintf(fp, "# The trackside cameras on course %d (%s), from SLUS_203.98 by modelconv. See docs/finish.md.\n"
                    "# camera x y z radius zoom eyeX eyeY eyeZ reach flags: used while the car is within radius of\n"
                    "# (x, y, z); flags 0x1000 look from the eye (0x20: absolute, else from x y z) while within reach,\n"
                    "# 2 zoom with the distance, 0xC follow reach behind the car (pitch eyeX, yaw eyeY), 0x8000 the\n"
                    "# last (used everywhere else)\n",
                id, name.c_str());
        int n = 0;
        for (;; ++n) {
            const uint8_t* e = &elf[list + n * 0x20];
            uint32_t flags = Rd32(e + 0x1C);
            fprintf(fp, "camera %.9g %.9g %.9g %.9g %.9g %d %d %d %d 0x%X\n", RdF(e), RdF(e + 4), RdF(e + 8),
                    RdF(e + 12), RdF(e + 16), (int16_t)(e[0x14] | e[0x15] << 8), (int16_t)(e[0x16] | e[0x17] << 8),
                    (int16_t)(e[0x18] | e[0x19] << 8), (int16_t)(e[0x1A] | e[0x1B] << 8), flags);
            if (flags & 0x8000 || n > 0x100) break;
        }
        fclose(fp);
        printf("track cameras %d: %d -> %s\n", id, n + 1, out.c_str());
        ++written;
    }
    return written;
}

// The Adventure's message tables (docs/messages.md): D_002A7ED0[town] points to a list of tables by place, each
// table a list of strings: [0] the speaker's name, [n] message n. A message is its control codes before the text, the
// text, and one end code with its arguments (Message_Task, 0x0023B168). Output: story/messages.txt, one line per
// message, bytes below 0x20, from 0x7F and '\\' written as \\xHH.
const uint32_t kElfMessageTowns = 0x002A7ED0;
const int kMessageTowns = 22;   // the towns, then the stops 10-21 (20 and 21: Quick-Pic Shops 12 and 11)

// The argument bytes of each pre-control code (Message_RunPreCodes, 0x0023AD00), -1 for none.
const int kPreArgs[0x1E] = {-1, 2, 2, 3, 1, 2, 2, 1, 2, -1, -1, 2, -1, 2, 6, 1, 1, 2, 0, 1, 2, 1, 1, 1, 1, 2, 2, 1, 2, 3};
// The argument bytes of each end code (Message_Task's switch), -1 for none; 9 and 13 have their own lists.
const int kEndArgs[0x19] = {0, 2, 2, 1, 1, 3, 1, 3, 2, 0, -1, 2, 1, 0, 1, 0, 2, 2, 1, 1, 2, 3, 2, 5, 0};

// A message's length in bytes at file offset `o`, or 0 if it isn't one.
size_t MessageLength(const std::vector<uint8_t>& elf, size_t o) {
    size_t p = o;
    auto at = [&](size_t k) -> int { return k < elf.size() ? elf[k] : -1; };
    while (at(p) > 0 && at(p) < 0x20) {   // (a 0 after the codes is end code 0: Q's Factory's rally line 39)
        int c = at(p++);
        if (c >= 0x1E || kPreArgs[c] < 0) return 0;
        p += kPreArgs[c];
    }
    for (;;) {
        int c = at(p);
        if (c < 0) return 0;
        if (c == 0x0A || (c >= 0x20 && c < 0x80)) {
            ++p;
        } else if (c >= 0x80) {
            p += 2;
        } else {
            ++p;
            if (c == 0x09) {   // items, each 0-terminated, until a 0x09; then a target per item
                int items = 0;
                do {
                    while (at(p) > 0) ++p;
                    if (at(p) < 0 || ++items > 16) return 0;
                    ++p;
                } while (at(p) != 0x09);
                p += 1 + items;
            } else if (c == 0x0D) {   // stamp ids up to a 0
                while (at(p) > 0) ++p;
                ++p;
            } else if (c > 0x18 || kEndArgs[c] < 0) {
                return 0;
            } else {
                p += kEndArgs[c];
            }
            return p <= elf.size() ? p - o : 0;
        }
    }
}

int ConvertMessages(const std::vector<uint8_t>& elf, const std::string& data) {
    size_t towns = ElfOffset(elf, kElfMessageTowns);
    if (!towns) return 0;
    // Every table, to tell where each list and table ends.
    std::set<uint32_t> lists, tables;
    for (int t = 0; t < kMessageTowns; ++t) {
        uint32_t list = Rd32(&elf[towns + t * 4]);
        if (ElfOffset(elf, list)) lists.insert(list);
    }
    lists.insert(kElfMessageTowns);
    auto next = [](const std::set<uint32_t>& s, uint32_t v) {
        auto it = s.upper_bound(v);
        return it == s.end() ? 0xFFFFFFFFu : *it;
    };
    std::set<uint32_t> all(lists);
    for (uint32_t list : lists) {
        if (list == kElfMessageTowns) continue;
        for (uint32_t a = list; a < next(lists, list); a += 4) {
            uint32_t table = Rd32(&elf[ElfOffset(elf, a)]);
            if (!ElfOffset(elf, table) || table < 0x002A0000 || table >= kElfMessageTowns) break;
            tables.insert(table);
        }
    }
    for (uint32_t t : tables) all.insert(t);
    std::string dir = data + "\\story";
    CreateDirectoryA(dir.c_str(), nullptr);
    std::string out = dir + "\\messages.txt";
    FILE* fp = fopen(out.c_str(), "w");
    if (!fp) {
        printf("can't write %s\n", out.c_str());
        return -1;
    }
    fprintf(fp, "# The Adventure's messages (D_002A7ED0), from SLUS_203.98 by modelconv. See docs/messages.md.\n"
                "# table TOWN PLACE starts a place's table; then N TEXT for message N (0 is the speaker's name).\n"
                "# Bytes below 0x20, from 0x7F and '\\' are written \\xHH: control codes, two-byte characters.\n");
    int count = 0;
    for (int t = 0; t < kMessageTowns; ++t) {
        uint32_t list = Rd32(&elf[towns + t * 4]);
        if (!ElfOffset(elf, list)) continue;
        int place = 0;
        for (uint32_t a = list; a < next(lists, list); a += 4, ++place) {
            uint32_t table = Rd32(&elf[ElfOffset(elf, a)]);
            if (!tables.count(table)) break;
            fprintf(fp, "table %d %d\n", t, place);
            uint32_t end = next(all, table);
            for (int n = 0; table + n * 4 < end; ++n) {
                size_t s = ElfOffset(elf, Rd32(&elf[ElfOffset(elf, table + n * 4)]));
                if (!s) break;
                size_t len = 0;
                if (n == 0) {
                    while (s + len < elf.size() && elf[s + len]) ++len;
                } else if (elf[s] == 0) {
                    len = 1;   // an empty message (the Store Manager's 3): just end code 0, and the table goes on
                } else {
                    len = MessageLength(elf, s);
                    if (!len) break;   // past the table's end (the next data isn't a message)
                }
                fprintf(fp, "%d ", n);
                for (size_t k = 0; k < len; ++k) {
                    uint8_t c = elf[s + k];
                    if (c < 0x20 || c >= 0x7F || c == '\\') fprintf(fp, "\\x%02X", c);
                    else fputc(c, fp);
                }
                fputc('\n', fp);
                ++count;
            }
        }
    }
    fclose(fp);
    printf("messages: %d -> %s\n", count, out.c_str());
    return count;
}

// The E-mails (D_002DB948): 128 pointers, each to a 0-terminated list of strings: the subject, the sender, then up to
// 11 body lines (My Garage's reader, func_00264498). data/story/mail.txt, in messages.txt's way: "mail N", then
// "K TEXT" for string K.
const uint32_t kElfMail = 0x002DB948;

int ConvertMail(const std::vector<uint8_t>& elf, const std::string& data) {
    if (!ElfOffset(elf, kElfMail)) return 0;
    std::string out = data + "\\story\\mail.txt";
    CreateDirectoryA((data + "\\story").c_str(), nullptr);
    FILE* fp = fopen(out.c_str(), "w");
    if (!fp) {
        printf("can't write %s\n", out.c_str());
        return -1;
    }
    fprintf(fp, "# The E-mails (D_002DB948), from SLUS_203.98 by modelconv. See docs/buildings.md, \"My Garage\".\n"
                "# mail N starts mail N; then 0 SUBJECT, 1 SENDER, 2.. its lines. $N is the player's car's name.\n"
                "# Bytes below 0x20, from 0x7F and '\\' are written \\xHH (two-byte characters: a heart, a note).\n");
    int count = 0;
    for (uint32_t m = 0; m < 128; ++m) {
        size_t list = ElfOffset(elf, Rd32(&elf[ElfOffset(elf, kElfMail + m * 4)]));
        if (!list) break;
        fprintf(fp, "mail %u\n", m);
        for (int k = 0; list + k * 4 + 4 <= elf.size(); ++k) {
            size_t at = ElfOffset(elf, Rd32(&elf[list + k * 4]));
            if (!at) break;
            fprintf(fp, "%d ", k);
            for (; at < elf.size() && elf[at]; ++at) {
                uint8_t c = elf[at];
                if (c < 0x20 || c >= 0x7F || c == '\\') fprintf(fp, "\\x%02X", c);
                else fputc(c, fp);
            }
            fputc('\n', fp);
        }
        ++count;
    }
    fclose(fp);
    printf("mail: %d -> %s\n", count, out.c_str());
    return count;
}

// A small model file laid out as a car file's single section, {0x10: the section, the GS uploads, the end}, its
// chain at 0x20 in the cars' lit strips: `rel` (e.g. "SYS/OPTION.BIN") to models/<dir>/0.obj, drawn with `gs`.
// Returns -1 on a write error, else 0 (a missing or different file is skipped).
int ConvertSection(const std::string& game, const std::string& data, const char* rel, const char* dir,
                   const GsState& gs, const char* what) {
    std::vector<uint8_t> b;
    std::string path = game + "\\" + rel;
    for (char& c : path) if (c == '/') c = '\\';
    if (!ReadFile(path, b) || b.size() < 0x30 || Rd32(&b[0]) != 0x10 || Rd32(&b[8]) != b.size()) {
        printf("%s: missing or not a one-section layout, skipped\n", rel);
        return 0;
    }
    std::string models = data + "\\models";
    CreateDirectoryA(models.c_str(), nullptr);
    MakeDirs(models, std::string(dir) + "/");
    std::string out = models + "\\" + dir + "\\0.obj";
    for (char& c : out) if (c == '/') c = '\\';
    int r = WriteChain(b, 0x20, 0, 0, gs, ReadManifestFor(data, rel), "../../../textures/", out, rel);
    if (r < 0) return -1;
    printf("%s: %s -> models/%s/0.obj\n", rel, what, dir);
    return 0;
}

// SYS/OPTION.BIN, the Settings screen's DualShock (loader job 0x3C0300, D_0029B428[0] into model slot 3), with its
// one T8 128 x 128 picture (CLUT at 0x40); and SYS/COIN.BIN, the field's ChoroQ coin (loaded at boot by
// func_0020B348 to D_003FA780, D_00335718 its section), drawn by func_00241D38 with TEX0 0x2003AA0599405D48: its T4
// 64 x 64 picture at 0x1D48, CLUT 0x1D50, where the file puts them.
int ConvertPad(const std::string& game, const std::string& data) {
    if (ConvertSection(game, data, "SYS/OPTION.BIN", "SYS/OPTION", FileTexture(0x13, 0x40), "the pad") < 0) return -1;
    GsState coin;
    coin.tex0 = 0x2003AA0599405D48ull & ((1ull << 51) - 1);   // TBP to CBP; CSM/CSA/CLD are left out
    coin.clamp = 5;
    return ConvertSection(game, data, "SYS/COIN.BIN", "SYS/COIN", coin, "the coin");
}

int main(int argc, char** argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: modelconv GAMEDIR DATADIR\n");
        return 2;
    }
    std::string game = argv[1], data = argv[2];
    bool padOnly = argc > 3 && !strcmp(argv[3], "--pad");   // just SYS/OPTION.BIN and SYS/COIN.BIN, below
    if (argc > 3 && !strcmp(argv[3], "--wheels")) {   // just CARS/WHEEL.BIN
        std::vector<uint8_t> wheelFile;
        if (!ReadFile(game + "\\CARS\\WHEEL.BIN", wheelFile)) return 1;
        int n = ConvertWheels(wheelFile, data);
        printf("%d wheels converted\n", n);
        return n < 0 ? 1 : 0;
    }
    if (argc > 3 && !strcmp(argv[3], "--ai")) {
        // Just the executable's paths: the races' opponents and the field's town cars.
        std::vector<uint8_t> elf;
        ReadFile(game + "\\SLUS_203.98", elf);
        return ConvertAiPaths(elf, data) < 0 || ConvertTownCars(elf, data) < 0 ? 1 : 0;
    }
    if (argc > 3 && !strcmp(argv[3], "--messages")) {   // just story/messages.txt
        std::vector<uint8_t> elf;
        ReadFile(game + "\\SLUS_203.98", elf);
        return ConvertMessages(elf, data) < 0 || ConvertMail(elf, data) < 0 ? 1 : 0;
    }
    if (ConvertPad(game, data) < 0) return 1;
    if (padOnly) return 0;
    std::vector<std::string> files;
    FindFiles(game, "", ".E3D", files);
    int converted = 0;
    for (const std::string& f : files) {
        std::vector<uint8_t> bytes;
        gfx::Mesh mesh;
        int strips = 0;
        if (!ReadFile(game + "\\" + f, bytes)) {
            printf("can't read %s\n", f.c_str());
            continue;
        }
        if (IsModelTable(bytes)) {
            int n = ConvertModelTable(bytes, f, data);
            if (n < 0) return 1;
            converted += n > 0;
            continue;
        }
        if (!ParseE3D(bytes, mesh, strips)) {
            printf("skipped %s (not the lit-strip layout)\n", f.c_str());
            continue;
        }
        std::string dir = data + "\\models";
        CreateDirectoryA(dir.c_str(), nullptr);
        MakeDirs(dir, f);
        std::string out = data + "\\models\\" + WinPath(f.substr(0, f.size() - 4)) + ".obj";
        std::string note = "From " + f + " by modelconv (tools/modelconv)";
        if (!SaveObj(out, mesh, note.c_str())) {
            printf("can't write %s\n", out.c_str());
            return 1;
        }
        printf("%s: %d strips, %u triangles -> %s\n", f.c_str(), strips, (unsigned)(mesh.indices.size() / 3),
               out.c_str());
        ++converted;
    }
    printf("%d of %u E3D files converted\n", converted, (unsigned)files.size());

    // The race courses: every COURSE file, then the ACTION files by course id.
    std::vector<uint8_t> elf;
    ReadFile(game + "\\SLUS_203.98", elf);
    std::vector<std::string> courses;
    FindFiles(game + "\\COURSE", "COURSE", ".BIN", courses);
    size_t raceCourses = courses.size();
    for (int id = 0x12; id < kCourseIds; ++id) courses.push_back("ACTION/" + CourseFile(id) + ".BIN");
    int done = 0;
    for (size_t i = 0; i < courses.size(); ++i) {
        const std::string& f = courses[i];
        std::vector<uint8_t> bytes;
        if (!ReadFile(game + "\\" + f, bytes)) {
            printf("can't read %s\n", f.c_str());
            continue;
        }
        int n = ConvertCourse(bytes, f, data, elf, i < raceCourses || CourseHasMap(0x12 + (int)(i - raceCourses)));
        if (n < 0) return 1;
        done += n > 0;
    }
    printf("%d of %u courses converted\n", done, (unsigned)courses.size());
    // The Adventure's world: FLD/<abc>.BIN, one 1600-unit block each, in the course files' layout (ConvertCourse).
    std::vector<std::string> blocks;
    FindFiles(game + "\\FLD", "FLD", ".BIN", blocks);
    done = 0;
    for (const std::string& f : blocks) {
        std::vector<uint8_t> bytes;
        if (!ReadFile(game + "\\" + f, bytes)) {
            printf("can't read %s\n", f.c_str());
            continue;
        }
        int n = ConvertCourse(bytes, f, data, elf, true);
        if (n < 0) return 1;
        done += n > 0;
    }
    printf("%d of %u world blocks converted\n", done, (unsigned)blocks.size());
    // The field's sky: the race's sky model with SYS/SORA.GSL's picture, which the field loads to the sky's TBP
    // 0x3CFE (func_00210158's loader job 0x340008, D_0029AF58[8]): models/FLD/sky.obj.
    if (size_t sky = ElfOffset(elf, kElfSky)) {
        int r = WriteChain(elf, sky, 0, 0, ElfGsState(elf, kElfSkyState), ReadManifestFor(data, "SYS/SORA.GSL"),
                           "../../textures/", data + "\\models\\FLD\\sky.obj", "FLD sky");
        if (r < 0) return 1;
    }
    if (ConvertAiPaths(elf, data) < 0) return 1;
    if (ConvertTownCars(elf, data) < 0) return 1;
    if (ConvertTrackCameras(elf, data) < 0) return 1;
    if (ConvertMessages(elf, data) < 0) return 1;
    if (ConvertMail(elf, data) < 0) return 1;

    // The cars: bodies Q000-Q150 (T8 at block 0, CLUT at 0x40) and the tires (T4, CLUT at 0x20).
    std::vector<std::string> cars;
    const char* const carDirs[] = {"CAR0", "CAR1", "CAR2", "CAR3", "CAR4", "CARS"};
    for (const char* d : carDirs) FindFiles(game + "\\" + d, d, ".BIN", cars);
    done = 0;
    int bodies = 0;
    for (const std::string& f : cars) {
        bool tire = f == "CARS/TIRE.BIN";
        if (!tire && f.find("/Q") == std::string::npos) continue;   // PARTS (below) and WHEEL
        ++bodies;
        std::vector<uint8_t> bytes;
        if (!ReadFile(game + "\\" + f, bytes)) {
            printf("can't read %s\n", f.c_str());
            continue;
        }
        int n = ConvertCar(bytes, f, data, elf, tire ? FileTexture(0x14, 0x20) : FileTexture(0x13, 0x40));
        if (n < 0) return 1;
        done += n > 0;
    }
    printf("%d of %d car files converted\n", done, bodies);
    std::vector<uint8_t> garage;
    if (ReadFile(game + "\\SHOP\\GARAGE.BIN", garage)) {
        int n = ConvertGarage(garage, data);
        if (n < 0) return 1;
        printf("%d garage models converted\n", n);
    }
    std::vector<uint8_t> partsFile;
    if (ReadFile(game + "\\CARS\\PARTS.BIN", partsFile)) {
        int n = ConvertParts(partsFile, data);
        if (n < 0) return 1;
        printf("%d car parts converted\n", n);
    }
    std::vector<uint8_t> wheelFile;
    if (ReadFile(game + "\\CARS\\WHEEL.BIN", wheelFile)) {
        int n = ConvertWheels(wheelFile, data);
        if (n < 0) return 1;
        printf("%d wheels converted\n", n);
    }
    return 0;
}
