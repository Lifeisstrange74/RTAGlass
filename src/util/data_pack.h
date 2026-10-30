// Data packs (.rpk): one compressed file standing in for a folder of data files, e.g. data\textures.rpk for
// data\textures. tools/rpak makes and unpacks them. The game mounts a pack over its folder and then reads files by
// their usual paths: a loose file, if there is one, wins over the pack's copy, so mods are just loose files.
//
// Layout (little-endian):
//   PackHeader                     at 0
//   blob data                      deflate (raw, no zlib header) or stored
//   PackFile[fileCount]            at tocOffset, sorted by name
//   PackBlob[blobCount]
//   names                          namesSize bytes of NUL-terminated paths, '/' separated, relative to the folder
//
// A file is either raw bytes (one blob) or an A8R8G8B8 DDS stored as its pixels, which come back as the same DDS:
//   kPackArgb    data: width * height B,G,R,A pixels
//   kPackIndex8  data: width * height bytes, indices into palette (colors B,G,R,A entries)
//   kPackIndex4  the same with two pixels a byte, the first in the low nibble (at most 16 colours)
// Identical blobs are stored once, so palette variants of one picture share their indices.
#pragma once

#include <stdint.h>

#include <string>
#include <unordered_map>
#include <vector>

namespace util {

const uint32_t kPackVersion = 1;

enum PackKind : uint32_t { kPackRaw = 0, kPackArgb = 1, kPackIndex8 = 2, kPackIndex4 = 3 };
enum PackCodec : uint32_t { kPackStored = 0, kPackDeflate = 1 };

struct PackHeader {
    char magic[4];   // "RPAK"
    uint32_t version, fileCount, blobCount, tocOffset, namesSize, reserved[2];
};

struct PackBlob {
    uint32_t offset, packedSize, size, codec;
    uint32_t crc;   // CRC-32 (zip's) of the unpacked bytes: deflate alone doesn't notice most damage
};

struct PackFile {
    uint32_t name;             // offset into names
    uint32_t kind;             // PackKind
    uint32_t width, height;    // images only
    uint32_t data, palette;    // blob indices (palette: indexed images only)
    uint32_t colors;           // palette entries
    uint32_t reserved;
};

uint32_t Crc32(const uint8_t* data, size_t size);

// Decompresses a raw deflate stream (RFC 1951, any block types) that must fill out exactly. False if it is broken.
bool Inflate(const uint8_t* in, size_t inSize, uint8_t* out, size_t outSize);

// The DDS header of an image entry (and of texconv's files): uncompressed A8R8G8B8, no mipmaps.
void PackDdsHeader(uint8_t hdr[128], uint32_t width, uint32_t height);

class DataPack {
public:
    DataPack() {}
    ~DataPack() { Close(); }
    DataPack(const DataPack&) = delete;
    DataPack& operator=(const DataPack&) = delete;

    // Maps the file and checks the whole table of contents; false (with Error()) if it isn't a valid pack.
    bool Open(const std::string& path);
    void Close();
    const std::string& Error() const { return error_; }

    uint32_t FileCount() const { return header_ ? header_->fileCount : 0; }
    uint32_t BlobCount() const { return header_ ? header_->blobCount : 0; }
    const PackFile& File(uint32_t i) const { return files_[i]; }
    const PackBlob& Blob(uint32_t i) const { return blobs_[i]; }
    const char* Name(const PackFile& f) const { return names_ + f.name; }
    // By path relative to the pack's folder, either slash, any case.
    const PackFile* Find(const std::string& name) const;

    bool ReadBlob(uint32_t index, std::vector<uint8_t>& out) const;
    // The file as it was packed (an image entry comes back as its DDS).
    bool Read(const PackFile& f, std::vector<uint8_t>& out) const;
    // An image entry's pixels, B,G,R,A as in the DDS. False for a raw entry.
    bool ReadImage(const PackFile& f, std::vector<uint32_t>& pixels, int& width, int& height) const;

    // Lower case with '/' separators: the key Find uses.
    static std::string Key(const std::string& name);

private:
    bool Fail(const char* what);

    void* fileHandle_ = nullptr;
    void* mapping_ = nullptr;
    const uint8_t* base_ = nullptr;
    size_t size_ = 0;
    const PackHeader* header_ = nullptr;
    const PackFile* files_ = nullptr;
    const PackBlob* blobs_ = nullptr;
    const char* names_ = nullptr;
    std::unordered_map<std::string, uint32_t> index_;
    std::string error_;
};

// Mounts a pack over a folder (call before any threads read through it). Mounting the same folder again replaces the
// pack. False if the pack won't open.
bool Pack_Mount(const std::string& packPath, const std::string& dir);
// The mounted pack and entry for a path in a mounted folder, if the pack has it.
const DataPack* Pack_Find(const std::string& path, const PackFile** file);

// Data file access through the mounts: the loose file if there is one, else the pack's copy.
bool Data_ReadFile(const std::string& path, std::vector<uint8_t>& out);
// The file as text lines, without their line ends.
bool Data_ReadLines(const std::string& path, std::vector<std::string>& lines);
bool Data_Exists(const std::string& path);
// Every file under dir (loose or packed) whose name ends in ext (any case), as paths relative to dir with '/'
// separators, each once.
void Data_List(const std::string& dir, const char* ext, std::vector<std::string>& out);

}  // namespace util
