// Data pack tool: packs a data folder into one .rpk file (src/util/data_pack.h), and back.
//
// The Release build packs data\textures, data\models and data\collision into dist\RTAGlass\data\<folder>.rpk with
// it, and the game reads the pack where there is no loose file. Packing is lossless: unpack gives back every file
// byte for byte.
//   - An A8R8G8B8 DDS in texconv's layout is stored as its pixels: 4-bit indices and a palette when it has at most
//     16 colours, 8-bit with at most 256, else the pixels themselves. Any other file is stored as it is.
//   - Each blob is deflated (src/util/image_write's deflate), or stored when that doesn't make it smaller.
//   - Identical blobs are stored once (the palette variants of a picture share their indices).
//
// Usage:
//   rpak pack DIR OUT.rpk [--update]   --update: do nothing if OUT is newer than everything in DIR
//   rpak unpack PACK DIR [--force]     existing files are kept (they may be mods) unless --force
//   rpak list PACK
//   rpak test PACK                     reads every file back
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#include <algorithm>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

#include "../../src/util/data_pack.h"
#include "../../src/util/image_write.h"

namespace {

using util::DataPack;
using util::PackBlob;
using util::PackFile;

struct Input {
    std::string rel, path;   // rel: '/' separated, relative to the folder
};

uint64_t Time(const FILETIME& t) { return (uint64_t)t.dwHighDateTime << 32 | t.dwLowDateTime; }

// Every file under dir, and the newest write time of the files and folders (a folder's changes when a file in it
// is added or removed).
void Walk(const std::string& dir, const std::string& rel, std::vector<Input>& out, uint64_t& newest) {
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA((dir + "\\*").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        std::string name = fd.cFileName;
        if (name == "." || name == "..") continue;
        newest = std::max(newest, Time(fd.ftLastWriteTime));
        std::string r = rel.empty() ? name : rel + "/" + name;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) Walk(dir + "\\" + name, r, out, newest);
        else out.push_back({r, dir + "\\" + name});
    } while (FindNextFileA(h, &fd));
    FindClose(h);
}

bool ReadAll(const std::string& path, std::vector<uint8_t>& out) {
    FILE* fp = fopen(path.c_str(), "rb");
    if (!fp) return false;
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    out.resize(size > 0 ? (size_t)size : 0);
    bool ok = size >= 0 && fread(out.data(), 1, out.size(), fp) == out.size();
    fclose(fp);
    return ok;
}

uint64_t Fnv(const uint8_t* p, size_t n) {
    uint64_t h = 0xCBF29CE484222325ull;
    for (size_t i = 0; i < n; ++i) h = (h ^ p[i]) * 0x100000001B3ull;
    return h;
}

class Packer {
public:
    explicit Packer(FILE* fp) : fp_(fp) {}

    // The blob's index: an identical earlier one, or a new one written now.
    uint32_t Add(std::vector<uint8_t>&& data) {
        uint64_t h = Fnv(data.data(), data.size());
        auto range = seen_.equal_range(h);
        for (auto it = range.first; it != range.second; ++it)
            if (raw_[it->second] == data) return it->second;
        PackBlob b = {(uint32_t)ftell(fp_), 0, (uint32_t)data.size(), util::kPackStored,
                      util::Crc32(data.data(), data.size())};
        std::vector<uint8_t> z = data.empty() ? std::vector<uint8_t>() : DeflateRaw(data.data(), data.size());
        const std::vector<uint8_t>& out = !z.empty() && z.size() < data.size() ? z : data;
        if (&out == &z) b.codec = util::kPackDeflate;
        b.packedSize = (uint32_t)out.size();
        fwrite(out.data(), 1, out.size(), fp_);
        uint32_t index = (uint32_t)blobs_.size();
        blobs_.push_back(b);
        seen_.insert({h, index});
        raw_.push_back(std::move(data));
        return index;
    }
    const std::vector<PackBlob>& Blobs() const { return blobs_; }

private:
    FILE* fp_;
    std::vector<PackBlob> blobs_;
    std::vector<std::vector<uint8_t>> raw_;
    std::unordered_multimap<uint64_t, uint32_t> seen_;
};

// A DDS that PackDdsHeader gives back exactly: its width and height.
bool IsPlainDds(const std::vector<uint8_t>& f, uint32_t& w, uint32_t& h) {
    if (f.size() < 128) return false;
    memcpy(&h, f.data() + 12, 4);
    memcpy(&w, f.data() + 16, 4);
    if (w == 0 || h == 0 || w > 16384 || h > 16384 || f.size() != 128 + (size_t)w * h * 4) return false;
    uint8_t hdr[128];
    util::PackDdsHeader(hdr, w, h);
    return memcmp(hdr, f.data(), 128) == 0;
}

void AddImage(Packer& packer, const std::vector<uint8_t>& file, uint32_t w, uint32_t h, PackFile& e) {
    const uint32_t* px = (const uint32_t*)(file.data() + 128);
    size_t count = (size_t)w * h;
    e.width = w;
    e.height = h;
    // The palette in order of first appearance, so pictures that differ only in palette get the same indices.
    std::vector<uint32_t> palette;
    std::unordered_map<uint32_t, uint8_t> slot;
    std::vector<uint8_t> index(count);
    uint32_t last = 0;
    int lastSlot = -1;
    for (size_t i = 0; i < count && palette.size() <= 256; ++i) {
        if (px[i] != last || lastSlot < 0) {
            auto it = slot.find(px[i]);
            if (it == slot.end()) {
                if (palette.size() == 256) {
                    palette.push_back(0);   // too many: marks the image as direct colour
                    break;
                }
                it = slot.insert({px[i], (uint8_t)palette.size()}).first;
                palette.push_back(px[i]);
            }
            last = px[i];
            lastSlot = it->second;
        }
        index[i] = (uint8_t)lastSlot;
    }
    if (palette.size() > 256) {
        e.kind = util::kPackArgb;
        e.data = packer.Add(std::vector<uint8_t>(file.begin() + 128, file.end()));
        return;
    }
    if (palette.size() <= 16) {
        std::vector<uint8_t> packed((count + 1) / 2);
        for (size_t i = 0; i < count; ++i) packed[i >> 1] |= (uint8_t)(index[i] << ((i & 1) * 4));
        e.kind = util::kPackIndex4;
        e.data = packer.Add(std::move(packed));
    } else {
        e.kind = util::kPackIndex8;
        e.data = packer.Add(std::move(index));
    }
    std::vector<uint8_t> pal(palette.size() * 4);
    memcpy(pal.data(), palette.data(), pal.size());
    e.palette = packer.Add(std::move(pal));
    e.colors = (uint32_t)palette.size();
}

int Pack(const std::string& dir, const std::string& out, bool update) {
    DWORD t0 = GetTickCount();
    std::vector<Input> inputs;
    uint64_t newest = 0;
    WIN32_FILE_ATTRIBUTE_DATA root;
    if (!GetFileAttributesExA(dir.c_str(), GetFileExInfoStandard, &root) ||
        !(root.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
        fprintf(stderr, "rpak: %s is not a folder\n", dir.c_str());
        return 1;
    }
    newest = Time(root.ftLastWriteTime);
    Walk(dir, "", inputs, newest);
    std::sort(inputs.begin(), inputs.end(),
              [](const Input& a, const Input& b) { return DataPack::Key(a.rel) < DataPack::Key(b.rel); });
    for (size_t i = 1; i < inputs.size(); ++i) {
        if (DataPack::Key(inputs[i].rel) == DataPack::Key(inputs[i - 1].rel)) {
            fprintf(stderr, "rpak: %s and %s differ only in case\n", inputs[i - 1].rel.c_str(), inputs[i].rel.c_str());
            return 1;
        }
    }

    if (update) {
        WIN32_FILE_ATTRIBUTE_DATA a;
        DataPack old;
        if (GetFileAttributesExA(out.c_str(), GetFileExInfoStandard, &a) && Time(a.ftLastWriteTime) >= newest &&
            old.Open(out) && old.FileCount() == inputs.size()) {
            printf("%s is up to date\n", out.c_str());
            return 0;
        }
    }

    std::string tmp = out + ".tmp";
    FILE* fp = fopen(tmp.c_str(), "wb");
    if (!fp) {
        fprintf(stderr, "rpak: can't write %s\n", tmp.c_str());
        return 1;
    }
    util::PackHeader header = {};
    fwrite(&header, sizeof header, 1, fp);
    Packer packer(fp);
    std::vector<PackFile> files;
    std::string names;
    int kinds[4] = {};
    uint64_t inputBytes = 0;
    for (const Input& in : inputs) {
        std::vector<uint8_t> data;
        if (!ReadAll(in.path, data)) {
            fprintf(stderr, "rpak: can't read %s\n", in.path.c_str());
            fclose(fp);
            DeleteFileA(tmp.c_str());
            return 1;
        }
        inputBytes += data.size();
        PackFile e = {};
        e.name = (uint32_t)names.size();
        names.append(in.rel).push_back(0);
        uint32_t w, h;
        if (IsPlainDds(data, w, h)) {
            AddImage(packer, data, w, h, e);
        } else {
            e.kind = util::kPackRaw;
            e.data = packer.Add(std::move(data));
        }
        ++kinds[e.kind];
        files.push_back(e);
    }
    while (ftell(fp) % 4) fputc(0, fp);
    memcpy(header.magic, "RPAK", 4);
    header.version = util::kPackVersion;
    header.fileCount = (uint32_t)files.size();
    header.blobCount = (uint32_t)packer.Blobs().size();
    header.tocOffset = (uint32_t)ftell(fp);
    header.namesSize = (uint32_t)names.size();
    fwrite(files.data(), sizeof(PackFile), files.size(), fp);
    fwrite(packer.Blobs().data(), sizeof(PackBlob), packer.Blobs().size(), fp);
    fwrite(names.data(), 1, names.size(), fp);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    fwrite(&header, sizeof header, 1, fp);
    bool ok = !ferror(fp);
    ok &= fclose(fp) == 0;
    if (!ok || !MoveFileExA(tmp.c_str(), out.c_str(), MOVEFILE_REPLACE_EXISTING)) {
        fprintf(stderr, "rpak: can't write %s\n", out.c_str());
        DeleteFileA(tmp.c_str());
        return 1;
    }
    printf("%s: %u files (%d raw, %d direct colour, %d 8-bit, %d 4-bit images), %u blobs\n", out.c_str(),
           (unsigned)files.size(), kinds[0], kinds[1], kinds[2], kinds[3], (unsigned)packer.Blobs().size());
    printf("%.1f MB in, %.1f MB out, %lu ms\n", inputBytes / 1e6, size / 1e6, (unsigned long)(GetTickCount() - t0));
    return 0;
}

// The folders a file path needs.
void MakeDirs(const std::string& path) {
    for (size_t i = 0; (i = path.find('\\', i + 1)) != std::string::npos;)
        CreateDirectoryA(path.substr(0, i).c_str(), nullptr);
}

bool OpenPack(DataPack& pack, const std::string& path) {
    if (pack.Open(path)) return true;
    fprintf(stderr, "rpak: %s: %s\n", path.c_str(), pack.Error().c_str());
    return false;
}

int Unpack(const std::string& packPath, const std::string& dir, bool force) {
    DataPack pack;
    if (!OpenPack(pack, packPath)) return 1;
    int written = 0, kept = 0, failed = 0;
    std::vector<uint8_t> data;
    for (uint32_t i = 0; i < pack.FileCount(); ++i) {
        const PackFile& f = pack.File(i);
        std::string path = dir + "\\" + pack.Name(f);
        std::replace(path.begin(), path.end(), '/', '\\');
        if (!force && GetFileAttributesA(path.c_str()) != INVALID_FILE_ATTRIBUTES) {
            ++kept;
            continue;
        }
        MakeDirs(path);
        FILE* fp = nullptr;
        if (!pack.Read(f, data) || !(fp = fopen(path.c_str(), "wb")) ||
            fwrite(data.data(), 1, data.size(), fp) != data.size()) {
            fprintf(stderr, "rpak: can't unpack %s\n", pack.Name(f));
            ++failed;
        } else {
            ++written;
        }
        if (fp) fclose(fp);
    }
    printf("%d files written, %d kept (--force replaces them), %d failed\n", written, kept, failed);
    return failed ? 1 : 0;
}

int List(const std::string& packPath) {
    DataPack pack;
    if (!OpenPack(pack, packPath)) return 1;
    static const char* const kKind[4] = {"raw", "argb", "index8", "index4"};
    uint64_t size = 0, packed = 0;
    for (uint32_t i = 0; i < pack.FileCount(); ++i) {
        const PackFile& f = pack.File(i);
        const PackBlob& b = pack.Blob(f.data);
        if (f.kind == util::kPackRaw) {
            printf("%-6s %10u  %s\n", kKind[f.kind], b.size, pack.Name(f));
        } else {
            char dims[32];
            snprintf(dims, sizeof dims, "%ux%u", f.width, f.height);
            printf("%-6s %10s  %s", kKind[f.kind], dims, pack.Name(f));
            if (f.colors) printf("  (%u colours)", f.colors);
            printf("\n");
        }
    }
    for (uint32_t i = 0; i < pack.BlobCount(); ++i) {
        size += pack.Blob(i).size;
        packed += pack.Blob(i).packedSize;
    }
    printf("%u files, %u blobs: %.1f MB, %.1f MB packed\n", pack.FileCount(), pack.BlobCount(), size / 1e6,
           packed / 1e6);
    return 0;
}

int Test(const std::string& packPath) {
    DWORD t0 = GetTickCount();
    DataPack pack;
    if (!OpenPack(pack, packPath)) return 1;
    int bad = 0;
    uint64_t bytes = 0;
    std::vector<uint8_t> data;
    for (uint32_t i = 0; i < pack.BlobCount(); ++i) {
        if (!pack.ReadBlob(i, data) && ++bad <= 10) fprintf(stderr, "blob %u is broken\n", i);
        bytes += data.size();
    }
    DWORD ms = GetTickCount() - t0;
    printf("%u blobs, %.1f MB read in %lu ms; %d broken\n", pack.BlobCount(), bytes / 1e6, (unsigned long)ms, bad);
    return bad ? 1 : 0;
}

int Usage() {
    fprintf(stderr,
            "usage: rpak pack DIR OUT.rpk [--update]\n"
            "       rpak unpack PACK DIR [--force]\n"
            "       rpak list PACK\n"
            "       rpak test PACK\n");
    return 2;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 3) return Usage();
    std::string cmd = argv[1];
    const char* flag = argc > 4 ? argv[4] : "";
    if (argc > 5) return Usage();
    if (cmd == "pack" && argc >= 4 && (!*flag || strcmp(flag, "--update") == 0))
        return Pack(argv[2], argv[3], *flag != 0);
    if (cmd == "unpack" && argc >= 4 && (!*flag || strcmp(flag, "--force") == 0))
        return Unpack(argv[2], argv[3], *flag != 0);
    if (cmd == "list") return List(argv[2]);
    if (cmd == "test") return Test(argv[2]);
    return Usage();
}
