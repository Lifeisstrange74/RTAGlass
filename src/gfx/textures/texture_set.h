// The PC texture set: the game's textures pre-converted to DDS (tools/texconv). The game finds them by game file,
// number and palette; the development tool gsplay also finds them by the content of the GS uploads that would
// have created them.
//
// Layout under the data directory (or packed into data/textures.rpk by tools/rpak, which the set mounts over
// textures/ so that loose files still win; see src/util/data_pack.h):
//   textures/<DIR>/<FILE.EXT>.txt      manifest for one game file (e.g. textures/COURSE/C00.BIN.txt)
//   textures/<DIR>/<FILE.EXT>/<n>.dds  images, A8R8G8B8, alpha 0-255 (PS2 0x80 -> 255)
// Manifest lines (whitespace separated, '#' starts a comment):
//   n palette psm width height dbp cbp clut_psm match image_hash clut_hash path
// path is relative to textures/; identical images are stored once, so several entries may share one path.
//
// Upload key (TextureKey): hash of the texture's upload data, hash of its CLUT's upload data (0 for direct colour)
// and the palette (CSA), as computed by gs::HashUpload (tools/common/gs). The GS translator in gsplay sees the same
// bytes when the game uploads them, so the key doesn't depend on where a file was loaded in GS memory.
#pragma once

#include <stdint.h>

#include <string>
#include <unordered_map>
#include <vector>

namespace gfx {

struct TextureKey {
    uint64_t image, clut;
    uint32_t palette;
    bool operator==(const TextureKey& o) const { return image == o.image && clut == o.clut && palette == o.palette; }
};

struct TextureKeyHash {
    size_t operator()(const TextureKey& k) const {
        return (size_t)(k.image ^ (k.clut * 0x9E3779B97F4A7C15ull) ^ k.palette);
    }
};

class TextureSet {
public:
    struct Entry {
        std::string source;   // game file, e.g. "COURSE/C00.BIN"
        int number, palette, width, height;
        bool guessed;         // CLUT pairing was a guess (see docs/textures.md)
        std::string path;     // relative to textures/
    };

    // Reads every manifest under dataDir/textures. Returns false if there is no texture set.
    bool Load(const std::string& dataDir);

    const Entry* Find(const TextureKey& key) const;
    // By game file, texture number within it and palette, e.g. ("SYS/LOGO.GSL", 0, 0).
    const Entry* Find(const std::string& source, int number, int palette = 0) const;
    size_t size() const { return map_.size(); }

    // Reads an A8R8G8B8 DDS (top mip only) as 32-bit B,G,R,A pixels.
    bool ReadImage(const Entry& e, std::vector<uint32_t>& pixels, int& width, int& height) const;

    template <typename F> void ForEach(F&& f) const {
        for (const auto& kv : map_) f(kv.first, entries_[kv.second]);
    }

private:
    bool LoadManifest(const std::string& file, const std::string& source);
    static std::string NameKey(const std::string& source, int number, int palette);

    std::string root_;
    std::vector<Entry> entries_;
    std::unordered_map<TextureKey, size_t, TextureKeyHash> map_;
    std::unordered_map<std::string, size_t> names_;
};

// Reads an A8R8G8B8 DDS (top mip only): the loose file, else a mounted pack's copy.
bool LoadDds(const std::string& path, std::vector<uint32_t>& pixels, int& width, int& height);
bool ParseDds(const uint8_t* data, size_t size, std::vector<uint32_t>& pixels, int& width, int& height);

}  // namespace gfx
