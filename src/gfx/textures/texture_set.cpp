#include "texture_set.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#include "../../util/data_pack.h"

namespace gfx {

namespace {

uint32_t Rd32(const uint8_t* p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24); }

}  // namespace

bool TextureSet::Load(const std::string& dataDir) {
    root_ = dataDir + "\\textures\\";
    // A player's copy has the set packed (tools/rpak); loose files still win over the pack's.
    std::string pack = dataDir + "\\textures.rpk";
    if (GetFileAttributesA(pack.c_str()) != INVALID_FILE_ATTRIBUTES && !util::Pack_Mount(pack, dataDir + "\\textures"))
        return false;
    std::vector<std::string> manifests;
    util::Data_List(dataDir + "\\textures", ".txt", manifests);
    for (const std::string& m : manifests) LoadManifest(m, m.substr(0, m.size() - 4));
    return !entries_.empty();
}

bool TextureSet::LoadManifest(const std::string& file, const std::string& source) {
    std::vector<std::string> lines;
    if (!util::Data_ReadLines(root_ + file, lines)) return false;
    for (const std::string& text : lines) {
        const char* line = text.c_str();
        if (line[0] == '#' || line[0] == 0) continue;
        int n, palette, w, h;
        char psm[8], clutPsm[8], match[8], path[260];
        unsigned dbp, cbp;
        unsigned long long image, clut;
        if (sscanf(line, "%d %d %7s %d %d %x %x %7s %7s %llx %llx %259s", &n, &palette, psm, &w, &h, &dbp, &cbp,
                   clutPsm, match, &image, &clut, path) != 12)
            continue;
        TextureKey key = {image, clut, (uint32_t)palette};
        Entry e;
        e.source = source;
        e.number = n;
        e.palette = palette;
        e.width = w;
        e.height = h;
        e.guessed = strcmp(match, "guess") == 0;
        e.path = path;
        // Every line is findable by name; by upload content, the first file with that upload wins.
        names_[NameKey(source, n, palette)] = entries_.size();
        if (!map_.count(key)) map_[key] = entries_.size();
        entries_.push_back(e);
    }
    return true;
}

const TextureSet::Entry* TextureSet::Find(const TextureKey& key) const {
    auto it = map_.find(key);
    return it == map_.end() ? NULL : &entries_[it->second];
}

const TextureSet::Entry* TextureSet::Find(const std::string& source, int number, int palette) const {
    auto it = names_.find(NameKey(source, number, palette));
    return it == names_.end() ? NULL : &entries_[it->second];
}

std::string TextureSet::NameKey(const std::string& source, int number, int palette) {
    char buf[32];
    sprintf(buf, "#%d#%d", number, palette);
    return source + buf;
}

bool TextureSet::ReadImage(const Entry& e, std::vector<uint32_t>& pixels, int& width, int& height) const {
    return LoadDds(root_ + e.path, pixels, width, height);
}

bool ParseDds(const uint8_t* data, size_t size, std::vector<uint32_t>& pixels, int& width, int& height) {
    if (size < 128 || memcmp(data, "DDS ", 4) != 0 || Rd32(data + 88) != 32 || Rd32(data + 92) != 0x00FF0000 ||
        Rd32(data + 104) != 0xFF000000)
        return false;
    height = (int)Rd32(data + 12);
    width = (int)Rd32(data + 16);
    size_t count = (size_t)width * height;
    if (width <= 0 || height <= 0 || (size - 128) / 4 < count) return false;
    pixels.resize(count);
    memcpy(pixels.data(), data + 128, count * 4);
    return true;
}

bool LoadDds(const std::string& path, std::vector<uint32_t>& pixels, int& width, int& height) {
    // A loose file first (a mod, or the development set), then a mounted pack's copy.
    if (FILE* fp = fopen(path.c_str(), "rb")) {
        uint8_t hdr[128];
        bool ok = fread(hdr, 1, sizeof(hdr), fp) == sizeof(hdr) && memcmp(hdr, "DDS ", 4) == 0 &&
                  Rd32(hdr + 88) == 32 && Rd32(hdr + 92) == 0x00FF0000 && Rd32(hdr + 104) == 0xFF000000;
        if (ok) {
            height = (int)Rd32(hdr + 12);
            width = (int)Rd32(hdr + 16);
            pixels.resize((size_t)width * height);
            ok = fread(pixels.data(), 4, pixels.size(), fp) == pixels.size();
        }
        fclose(fp);
        return ok;
    }
    std::vector<uint8_t> bytes;
    const util::PackFile* f;
    const util::DataPack* pack = util::Pack_Find(path, &f);
    if (!pack) return false;
    if (f->kind != util::kPackRaw) return pack->ReadImage(*f, pixels, width, height);
    return pack->Read(*f, bytes) && ParseDds(bytes.data(), bytes.size(), pixels, width, height);
}

}  // namespace gfx
