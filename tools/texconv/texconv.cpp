// Texture converter: builds the PC texture set (data/textures, read by src/gfx/textures/texture_set) from the
// original game files. It finds every GS texture upload, replays it into an emulated GS memory
// (tools/common/gs/gs_memory), and writes each texture as an A8R8G8B8 DDS plus one manifest per game file.
//
// Upload format (see docs/textures.md): .GSL files are DMA chains of uploads, and the .BIN containers
// embed the same packets. Each upload is
//   GIF tag  NLOOP=4 PACKED NREG=1 REGS=A+D
//   A+D      BITBLTBUF (0x50), TRXPOS (0x51), TRXREG (0x52), TRXDIR (0x53)
//   GIF tag  IMAGE, NLOOP = data size in qwords
//   data
// A paletted texture (T8/T4) is normally followed by its CLUT upload (a small CT32/CT16 image), but a CLUT
// can also serve several textures uploaded before it, or textures later in the file with none of their own.
// The real pairing is in each draw's TEX0, mostly inside mesh data not yet reversed, so this tool guesses:
// a CLUT goes to the pending textures it is big enough for (T8 needs 256 entries); anything left over takes
// the most recent compatible CLUT in the file and is marked "guess". A wrong guess costs nothing at run
// time: the game's real texture/CLUT pair misses the set and is decoded from GS memory.
// The tool scans for the A+D register pattern, so it needs no knowledge of the container formats.
//
// Usage: texconv GAMEDIR DATADIR [--force] [--png PNGDIR]
//   DATADIR/textures/<DIR>/<FILE.EXT>.txt and <DIR>/<FILE.EXT>/<n>[_p<k>].dds. Identical images are written once.
//   Existing .dds files are kept (they may be mods) unless --force is given; manifests are always rewritten.
//   --png also writes every image as PNG under PNGDIR, with PNGDIR/index.csv.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#include <map>
#include <string>
#include <vector>

#include "../common/gs/gs_memory.h"
#include "../../src/gfx/textures/texture_set.h"
#include "../../src/util/image_write.h"

namespace {

struct Upload {
    uint32_t offset;       // file offset of the A+D GIF tag
    int index;             // texture number within the file (upload order)
    gs::Transfer t;
    const uint8_t* data;   // image data in the file buffer
    uint64_t hash;         // gs::HashUpload: the upload lookup key
};

uint64_t Rd64(const std::vector<uint8_t>& d, size_t o) {
    uint64_t v;
    memcpy(&v, d.data() + o, 8);
    return v;
}

// Finds the uploads in a file and applies each one to mem as it goes, calling back after every upload.
template <typename F>
void ScanUploads(const std::vector<uint8_t>& d, gs::LocalMemory& mem, F&& onUpload) {
    size_t n = d.size() & ~(size_t)15;
    for (size_t o = 0; o + 0x60 <= n; o += 16) {
        if ((Rd64(d, o + 0x18) & 0xFF) != 0x50 || (Rd64(d, o + 0x28) & 0xFF) != 0x51 ||
            (Rd64(d, o + 0x38) & 0xFF) != 0x52 || (Rd64(d, o + 0x48) & 0xFF) != 0x53)
            continue;
        uint64_t tag = Rd64(d, o), img = Rd64(d, o + 0x50);
        if ((tag & 0x7FFF) != 4 || ((tag >> 58) & 3) != 0 || (tag >> 60) != 1 || (Rd64(d, o + 8) & 0xF) != 0xE)
            continue;   // not a real A+D tag (a coincidence inside pixel data)
        if (((img >> 58) & 3) != 2) continue;
        Upload u;
        u.offset = (uint32_t)o;
        u.index = -1;
        u.t = gs::Transfer::From(Rd64(d, o + 0x10), Rd64(d, o + 0x20), Rd64(d, o + 0x30));
        if ((Rd64(d, o + 0x40) & 3) != 0) continue;   // TRXDIR: only host -> local
        size_t bytes = (size_t)(img & 0x7FFF) * 16;
        if (o + 0x60 + bytes > d.size() || bytes < u.t.TotalBytes()) {
            fprintf(stderr, "  upload at %X: short image data, skipped\n", (unsigned)o);
            continue;
        }
        u.data = d.data() + o + 0x60;
        u.hash = gs::HashUpload(u.t, u.data);
        mem.WriteImage(u.t, u.data, (uint32_t)u.t.TotalBytes());
        onUpload(u);
        o += 0x50 + bytes;   // the loop adds the last 16
    }
}

bool IsPaletted(uint32_t psm) { return psm == gs::PSMT8 || psm == gs::PSMT4; }
bool IsColour(uint32_t psm) { return psm == gs::PSMCT32 || psm == gs::PSMCT16 || psm == gs::PSMCT16S; }

const char* PsmName(uint32_t psm) {
    switch (psm) {
    case gs::PSMCT32: return "CT32";
    case gs::PSMCT24: return "CT24";
    case gs::PSMCT16: return "CT16";
    case gs::PSMCT16S: return "CT16S";
    case gs::PSMT8: return "T8";
    case gs::PSMT4: return "T4";
    }
    return "?";
}

uint64_t Hash(const uint32_t* p, size_t n, int w, int h) {
    uint64_t v = 0xCBF29CE484222325ull;
    auto mix = [&](uint32_t x) {
        for (int i = 0; i < 4; ++i) { v ^= (x >> (i * 8)) & 0xFF; v *= 0x100000001B3ull; }
    };
    mix((uint32_t)w); mix((uint32_t)h);
    for (size_t i = 0; i < n; ++i) mix(p[i]);
    return v;
}

// PS2 alpha (0x80 = opaque) to 0-255.
void RescaleAlpha(std::vector<uint32_t>& img) {
    for (uint32_t& c : img) {
        uint32_t a = c >> 24;
        a = a >= 0x80 ? 255 : a * 2;
        c = (c & 0x00FFFFFFu) | (a << 24);
    }
}

void ListFiles(const std::string& dir, const std::string& rel, std::vector<std::string>& out) {
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA((dir + "\\*").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        std::string name = fd.cFileName;
        if (name == "." || name == "..") continue;
        std::string r = rel.empty() ? name : rel + "\\" + name;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ListFiles(dir + "\\" + name, r, out);
        else out.push_back(r);
    } while (FindNextFileA(h, &fd));
    FindClose(h);
}

bool ReadFile(const std::string& path, std::vector<uint8_t>& d) {
    FILE* fp = fopen(path.c_str(), "rb");
    if (!fp) return false;
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    d.resize((size_t)size);
    size_t got = fread(d.data(), 1, d.size(), fp);
    fclose(fp);
    return got == d.size();
}

bool Exists(const std::string& path) { return GetFileAttributesA(path.c_str()) != INVALID_FILE_ATTRIBUTES; }

void MakeDirs(const std::string& path) {
    for (size_t i = 0; i < path.size(); ++i)
        if (path[i] == '\\' && i > 0) CreateDirectoryA(path.substr(0, i).c_str(), NULL);
    CreateDirectoryA(path.c_str(), NULL);
}

std::string Slashes(std::string s) {
    for (char& c : s) if (c == '\\') c = '/';
    return s;
}

struct Stats { int uploads = 0, textures = 0, images = 0, written = 0, kept = 0, noclut = 0, guessed = 0; };

struct Options {
    std::string textures;   // DATADIR\textures
    std::string png;        // PNG review tree, or empty
    bool force = false;
};

class Converter {
public:
    Converter(const Options& opt, FILE* index) : opt_(opt), index_(index) {}

    void ConvertFile(const std::string& gameDir, const std::string& rel) {
        std::vector<uint8_t> d;
        if (!ReadFile(gameDir + "\\" + rel, d)) return;
        base_ = rel;   // e.g. COURSE\C00.BIN (SYS has both PUTI.BIN and PUTI.GSL)
        rel_ = rel;
        count_ = 0;
        manifest_ = NULL;
        memset(mem_.Data(), 0, gs::LocalMemory::kSize);
        std::vector<Upload> pending;   // paletted textures waiting for their CLUT
        Upload last = {}, last256 = {};   // most recent CLUTs: any size, and 256 entries
        bool haveLast = false, haveLast256 = false;
        // Pending textures that a CLUT can't serve fall back to the most recent compatible one.
        auto fallback = [&](const Upload& p) {
            if (p.t.dpsm == gs::PSMT8) Emit(p, haveLast256 ? &last256 : NULL, true);
            else Emit(p, haveLast ? &last : NULL, true);
        };
        ScanUploads(d, mem_, [&](Upload u) {
            ++stats_.uploads;
            if (IsPaletted(u.t.dpsm)) {
                u.index = count_++;
                pending.push_back(u);
            } else if (IsColour(u.t.dpsm) && !pending.empty() && u.t.w * u.t.h <= 256) {
                // A 256-entry CLUT with a T8 waiting is that T8's; T4s queued with it get a fallback.
                bool big = u.t.w * u.t.h >= 256, forT8 = false;
                for (const Upload& p : pending) forT8 |= p.t.dpsm == gs::PSMT8;
                for (const Upload& p : pending) {
                    if (p.t.dpsm == gs::PSMT8 ? !big : big && forT8) fallback(p);
                    else Emit(p, &u, false);
                }
                pending.clear();
                last = u; haveLast = true;
                if (big) { last256 = u; haveLast256 = true; }
            } else {
                u.index = count_++;
                Emit(u, NULL, false);
            }
        });
        for (const Upload& p : pending) fallback(p);
        if (manifest_) fclose(manifest_);
    }

    const Stats& stats() const { return stats_; }

private:
    // Decode one uploaded texture; a paletted one is written once per palette in its CLUT upload.
    void Emit(const Upload& u, const Upload* clut, bool guess) {
        gs::Tex0 t = {};
        t.tbp0 = u.t.dbp; t.tbw = u.t.dbw; t.psm = u.t.dpsm;
        int palettes = 1;
        if (clut) {
            if (guess) ++stats_.guessed;
            t.cbp = clut->t.dbp; t.cpsm = clut->t.dpsm;
            // A T4 CLUT holds w*h/16 palettes (CSA groups), except that the game pads single CT32 palettes
            // to 16x2, whose right half is junk.
            if (u.t.dpsm == gs::PSMT4 && clut->t.h > 2) palettes = (int)(clut->t.w * clut->t.h / 16);
        } else if (IsPaletted(u.t.dpsm)) {
            ++stats_.noclut;
        }
        const char* match = !IsPaletted(u.t.dpsm) ? "-" : !clut ? "none" : guess ? "guess" : "next";
        gs::Texa texa = gs::Texa::From(0x0000004000008080ull);   // the game's TEXA (from PCSX2 captures)
        int w = (int)u.t.w, h = (int)u.t.h;
        std::vector<uint32_t> img((size_t)w * h);
        for (int p = 0; p < palettes; ++p) {
            t.csa = (uint32_t)p;
            if (IsPaletted(u.t.dpsm) && !clut) {   // no CLUT found: write the raw indices as grey
                int scale = u.t.dpsm == gs::PSMT8 ? 1 : 17;
                for (int y = 0; y < h; ++y)
                    for (int x = 0; x < w; ++x) {
                        uint32_t v = mem_.ReadPixel(t.psm, t.tbp0, t.tbw ? t.tbw : 1, (int)u.t.dx + x, (int)u.t.dy + y);
                        v *= scale;
                        img[(size_t)y * w + x] = v | (v << 8) | (v << 16) | 0x80000000u;
                    }
            } else {
                mem_.DecodeRect(t, texa, (int)u.t.dx, (int)u.t.dy, w, h, img.data());
            }
            RescaleAlpha(img);
            ++stats_.textures;
            std::string path = WriteImage(img, w, h, u.index, palettes > 1 ? p : -1);
            uint64_t clutHash = clut ? clut->hash : 0;
            if (!manifest_) OpenManifest();
            fprintf(manifest_, "%d %d %s %d %d %04X %04X %s %s %016llX %016llX %s\n", u.index, p, PsmName(u.t.dpsm),
                    w, h, u.t.dbp, clut ? clut->t.dbp : 0, clut ? PsmName(clut->t.dpsm) : "-", match,
                    (unsigned long long)u.hash, (unsigned long long)clutHash, path.c_str());
            if (index_)
                fprintf(index_, "%s,%X,%d,%d,%s,%d,%d,%04X,%d,%s,%04X,%s,%d,%016llX,%016llX,%s\n", rel_.c_str(),
                        u.offset, u.index, p, PsmName(u.t.dpsm), w, h, u.t.dbp, u.t.dbw,
                        clut ? PsmName(clut->t.dpsm) : "", clut ? clut->t.dbp : 0, match, palettes,
                        (unsigned long long)u.hash, (unsigned long long)clutHash, path.c_str());
        }
    }

    // Writes the image once per distinct content; returns its path relative to textures/, with '/'.
    std::string WriteImage(const std::vector<uint32_t>& img, int w, int h, int index, int palette) {
        uint64_t hash = Hash(img.data(), img.size(), w, h);
        std::map<uint64_t, std::string>::iterator it = seen_.find(hash);
        if (it != seen_.end()) return it->second;
        char name[64];
        if (palette >= 0) sprintf(name, "%03d_p%d", index, palette);
        else sprintf(name, "%03d", index);
        std::string rel = base_ + "\\" + name;
        std::string dds = opt_.textures + "\\" + rel + ".dds";
        MakeDirs(opt_.textures + "\\" + base_);
        if (opt_.force || !Exists(dds)) {
            WriteDds(dds.c_str(), img.data(), w, h);
            ++stats_.written;
        } else {
            ++stats_.kept;
        }
        if (!opt_.png.empty()) {
            MakeDirs(opt_.png + "\\" + base_);
            WritePng((opt_.png + "\\" + rel + ".png").c_str(), img.data(), w, h);
        }
        ++stats_.images;
        std::string path = Slashes(rel) + ".dds";
        seen_[hash] = path;
        return path;
    }

    void OpenManifest() {
        MakeDirs(opt_.textures + "\\" + base_.substr(0, base_.rfind('\\')));
        manifest_ = fopen((opt_.textures + "\\" + base_ + ".txt").c_str(), "w");
        fprintf(manifest_,
                "# Textures from %s, made by texconv (tools/texconv). Format: src/gfx/textures/texture_set.h\n"
                "# n palette psm width height dbp cbp clut_psm match image_hash clut_hash path\n",
                Slashes(rel_).c_str());
    }

    Options opt_;
    std::string base_, rel_;
    FILE* index_;
    FILE* manifest_ = NULL;
    gs::LocalMemory mem_;
    std::map<uint64_t, std::string> seen_;
    Stats stats_;
    int count_ = 0;
};

}  // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: texconv GAMEDIR DATADIR [--force] [--png PNGDIR]\n");
        return 2;
    }
    std::string game = argv[1];
    Options opt;
    opt.textures = std::string(argv[2]) + "\\textures";
    for (int i = 3; i < argc; ++i) {
        if (strcmp(argv[i], "--force") == 0) opt.force = true;
        else if (strcmp(argv[i], "--png") == 0 && i + 1 < argc) opt.png = argv[++i];
    }
    std::vector<std::string> files;
    ListFiles(game, "", files);
    if (files.empty()) {
        fprintf(stderr, "no game files in %s\n", game.c_str());
        return 2;
    }
    MakeDirs(opt.textures);
    FILE* index = NULL;
    if (!opt.png.empty()) {
        MakeDirs(opt.png);
        index = fopen((opt.png + "\\index.csv").c_str(), "w");
        if (!index) return 2;
        fprintf(index, "file,offset,upload,palette,psm,width,height,dbp,dbw,clut_psm,cbp,clut_match,palettes,"
                       "image_hash,clut_hash,image\n");
    }
    Converter* conv = new Converter(opt, index);
    for (const std::string& f : files) {
        std::string ext = f.substr(f.rfind('.') + 1);
        if (ext != "GSL" && ext != "BIN" && ext != "E3D") continue;
        int before = conv->stats().textures;
        conv->ConvertFile(game, f);
        if (conv->stats().textures != before) printf("%-20s %5d textures\n", f.c_str(), conv->stats().textures - before);
    }
    if (index) fclose(index);
    const Stats& s = conv->stats();
    printf("%d uploads, %d textures (%d with a guessed CLUT, %d with none), %d images: %d written, %d kept\n",
           s.uploads, s.textures, s.guessed, s.noclut, s.images, s.written, s.kept);
    delete conv;
    return 0;
}
