#include "data_pack.h"

#include <stdio.h>
#include <string.h>
#include <windows.h>

#include <algorithm>
#include <memory>
#include <set>

namespace util {

namespace {

void Put32(uint8_t* p, uint32_t v) {
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

// An absolute, lower-case, backslash path, for comparing a path with a mount's folder.
std::string FullKey(const std::string& path) {
    char buf[MAX_PATH * 2];
    DWORD n = GetFullPathNameA(path.c_str(), sizeof buf, buf, nullptr);
    std::string s = n && n < sizeof buf ? std::string(buf, n) : path;
    for (char& c : s) c = c == '/' ? '\\' : (char)tolower((unsigned char)c);
    return s;
}

struct Mount {
    std::string dir;   // FullKey of the folder, with a trailing backslash
    std::unique_ptr<DataPack> pack;
};

std::vector<Mount>& Mounts() {
    static std::vector<Mount> mounts;
    return mounts;
}

bool ReadLoose(const std::string& path, std::vector<uint8_t>& out) {
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

bool EndsWith(const std::string& s, const char* ext) {
    size_t n = strlen(ext);
    return s.size() >= n && _stricmp(s.c_str() + s.size() - n, ext) == 0;
}

void ListLoose(const std::string& dir, const std::string& rel, const char* ext, std::vector<std::string>& out) {
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA((dir + "\\*").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        std::string name = fd.cFileName;
        if (name == "." || name == "..") continue;
        std::string r = rel.empty() ? name : rel + "/" + name;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ListLoose(dir + "\\" + name, r, ext, out);
        else if (EndsWith(name, ext)) out.push_back(r);
    } while (FindNextFileA(h, &fd));
    FindClose(h);
}

// ---- inflate (RFC 1951): stored, fixed and dynamic Huffman blocks ----

const int kFastBits = 10;

struct Huffman {
    uint16_t count[16];                  // codes of each length
    uint16_t symbol[288];                // symbols in canonical order
    uint16_t fast[1 << kFastBits];       // by the next kFastBits input bits: length << 9 | symbol, 0 if longer

    // False if the lengths over-subscribe the code (an incomplete code is allowed, as zlib allows it).
    bool Build(const uint8_t* lengths, int n) {
        memset(count, 0, sizeof count);
        memset(fast, 0, sizeof fast);
        for (int i = 0; i < n; ++i) ++count[lengths[i]];
        count[0] = 0;
        int left = 1;
        for (int len = 1; len < 16; ++len) {
            left = (left << 1) - count[len];
            if (left < 0) return false;
        }
        uint16_t offset[16], next[16];
        offset[1] = 0;
        next[1] = 0;
        for (int len = 1; len < 15; ++len) {
            offset[len + 1] = (uint16_t)(offset[len] + count[len]);
            next[len + 1] = (uint16_t)((next[len] + count[len]) << 1);
        }
        for (int i = 0; i < n; ++i) {
            int len = lengths[i];
            if (!len) continue;
            symbol[offset[len]++] = (uint16_t)i;
            int code = next[len]++;
            if (len > kFastBits) continue;
            int rev = 0;   // deflate sends Huffman codes MSB first
            for (int b = 0; b < len; ++b) rev |= ((code >> b) & 1) << (len - 1 - b);
            for (int k = rev; k < (1 << kFastBits); k += 1 << len) fast[k] = (uint16_t)(len << 9 | i);
        }
        return true;
    }
};

class Inflater {
public:
    Inflater(const uint8_t* in, size_t inSize, uint8_t* out, size_t outSize)
        : in_(in), inSize_(inSize), out_(out), outSize_(outSize) {}

    bool Run() {
        int last;
        do {
            last = Bits(1);
            int type = Bits(2);
            bool ok = type == 0 ? Stored() : type == 1 ? Fixed() : type == 2 ? Dynamic() : false;
            if (!ok) return false;
        } while (!last);
        return outPos_ == outSize_ && InBounds();
    }

private:
    // The bit buffer reads ahead with zeros past the end; what it took past the end must all still be unread.
    bool InBounds() const { return pos_ - count_ / 8 <= inSize_; }
    void Refill() {
        while (count_ <= 24) {
            bits_ |= (uint32_t)(pos_ < inSize_ ? in_[pos_] : 0) << count_;
            ++pos_;
            count_ += 8;
        }
    }
    int Bits(int n) {   // n <= 16
        Refill();
        int v = (int)(bits_ & ((1u << n) - 1));
        bits_ >>= n;
        count_ -= n;
        return v;
    }
    int Decode(const Huffman& h) {
        Refill();
        uint16_t e = h.fast[bits_ & ((1 << kFastBits) - 1)];
        if (e) {
            bits_ >>= e >> 9;
            count_ -= e >> 9;
            return e & 511;
        }
        int code = 0, first = 0, index = 0;   // a code longer than kFastBits: one bit at a time
        for (int len = 1; len < 16; ++len) {
            code |= (bits_ >> (len - 1)) & 1;
            int n = h.count[len];
            if (code - first < n) {
                bits_ >>= len;
                count_ -= len;
                return h.symbol[index + code - first];
            }
            index += n;
            first = (first + n) << 1;
            code <<= 1;
        }
        return -1;
    }

    bool Stored() {
        Bits(count_ & 7);   // to a byte boundary
        int len = Bits(16), nlen = Bits(16);
        if (len != (~nlen & 0xFFFF) || outSize_ - outPos_ < (size_t)len) return false;
        while (len--) out_[outPos_++] = (uint8_t)Bits(8);
        return InBounds();
    }

    bool Fixed() {
        uint8_t lengths[288 + 30];
        for (int i = 0; i < 288; ++i) lengths[i] = i < 144 ? 8 : i < 256 ? 9 : i < 280 ? 7 : 8;
        for (int i = 0; i < 30; ++i) lengths[288 + i] = 5;
        return lit_.Build(lengths, 288) && dist_.Build(lengths + 288, 30) && Codes();
    }

    bool Dynamic() {
        static const uint8_t kOrder[19] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};
        int nlit = Bits(5) + 257, ndist = Bits(5) + 1, ncode = Bits(4) + 4;
        if (nlit > 286 || ndist > 30) return false;
        uint8_t lengths[288 + 32] = {};
        for (int i = 0; i < ncode; ++i) lengths[kOrder[i]] = (uint8_t)Bits(3);
        Huffman lens;
        if (!lens.Build(lengths, 19)) return false;
        memset(lengths, 0, sizeof lengths);
        for (int i = 0; i < nlit + ndist;) {
            int sym = Decode(lens);
            if (sym < 0) return false;
            if (sym < 16) {
                lengths[i++] = (uint8_t)sym;
                continue;
            }
            int repeat, value = 0;
            if (sym == 16) {
                if (i == 0) return false;
                value = lengths[i - 1];
                repeat = 3 + Bits(2);
            } else if (sym == 17) {
                repeat = 3 + Bits(3);
            } else {
                repeat = 11 + Bits(7);
            }
            if (i + repeat > nlit + ndist) return false;
            while (repeat--) lengths[i++] = (uint8_t)value;
        }
        if (lengths[256] == 0) return false;   // no end-of-block code
        uint8_t dist[30];
        memcpy(dist, lengths + nlit, ndist);
        return lit_.Build(lengths, nlit) && dist_.Build(dist, ndist) && Codes();
    }

    bool Codes() {
        static const uint16_t kLenBase[29] = {3,  4,  5,  6,  7,  8,  9,  10, 11,  13,  15,  17,  19,  23, 27,
                                              31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
        static const uint8_t kLenExtra[29] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2,
                                              2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
        static const uint16_t kDistBase[30] = {1,    2,    3,    4,    5,    7,     9,     13,    17,    25,
                                               33,   49,   65,   97,   129,  193,   257,   385,   513,   769,
                                               1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
        static const uint8_t kDistExtra[30] = {0, 0, 0, 0, 1, 1, 2, 2,  3,  3,  4,  4,  5,  5,  6,
                                               6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};
        for (;;) {
            int sym = Decode(lit_);
            if (sym < 0) return false;
            if (sym < 256) {
                if (outPos_ == outSize_) return false;
                out_[outPos_++] = (uint8_t)sym;
                continue;
            }
            if (sym == 256) return InBounds();
            sym -= 257;
            if (sym >= 29) return false;
            size_t len = kLenBase[sym] + Bits(kLenExtra[sym]);
            int d = Decode(dist_);
            if (d < 0 || d >= 30) return false;
            size_t dist = kDistBase[d] + Bits(kDistExtra[d]);
            if (dist > outPos_ || outSize_ - outPos_ < len || !InBounds()) return false;
            const uint8_t* from = out_ + outPos_ - dist;
            uint8_t* to = out_ + outPos_;
            for (size_t k = 0; k < len; ++k) to[k] = from[k];   // may overlap: byte by byte
            outPos_ += len;
        }
    }

    const uint8_t* in_;
    size_t inSize_, pos_ = 0;
    uint32_t bits_ = 0;
    int count_ = 0;
    uint8_t* out_;
    size_t outSize_, outPos_ = 0;
    Huffman lit_, dist_;
};

// Filled before main, so threads can share it (local statics aren't thread-safe in this build: /Zc:threadSafeInit-).
struct CrcTable {
    uint32_t t[256];
    CrcTable() {
        for (uint32_t i = 0; i < 256; ++i) {
            uint32_t v = i;
            for (int k = 0; k < 8; ++k) v = (v & 1) ? 0xEDB88320u ^ (v >> 1) : v >> 1;
            t[i] = v;
        }
    }
} const kCrc;

}  // namespace

uint32_t Crc32(const uint8_t* data, size_t size) {
    uint32_t c = 0xFFFFFFFFu;
    for (size_t i = 0; i < size; ++i) c = kCrc.t[(c ^ data[i]) & 0xFF] ^ (c >> 8);
    return c ^ 0xFFFFFFFFu;
}

bool Inflate(const uint8_t* in, size_t inSize, uint8_t* out, size_t outSize) {
    return Inflater(in, inSize, out, outSize).Run();
}

void PackDdsHeader(uint8_t hdr[128], uint32_t width, uint32_t height) {
    memset(hdr, 0, 128);
    memcpy(hdr, "DDS ", 4);
    Put32(hdr + 4, 124);                               // dwSize
    Put32(hdr + 8, 0x1 | 0x2 | 0x4 | 0x8 | 0x1000);    // CAPS | HEIGHT | WIDTH | PITCH | PIXELFORMAT
    Put32(hdr + 12, height);
    Put32(hdr + 16, width);
    Put32(hdr + 20, width * 4);                        // pitch
    Put32(hdr + 76, 32);                               // ddspf.dwSize
    Put32(hdr + 80, 0x40 | 0x1);                       // DDPF_RGB | DDPF_ALPHAPIXELS
    Put32(hdr + 88, 32);
    Put32(hdr + 92, 0x00FF0000);                       // R mask
    Put32(hdr + 96, 0x0000FF00);                       // G
    Put32(hdr + 100, 0x000000FF);                      // B
    Put32(hdr + 104, 0xFF000000);                      // A
    Put32(hdr + 108, 0x1000);                          // DDSCAPS_TEXTURE
}

std::string DataPack::Key(const std::string& name) {
    std::string s = name;
    for (char& c : s) c = c == '\\' ? '/' : (char)tolower((unsigned char)c);
    return s;
}

bool DataPack::Fail(const char* what) {
    error_ = what;
    Close();
    error_ = what;
    return false;
}

bool DataPack::Open(const std::string& path) {
    Close();
    HANDLE f = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                           FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) return Fail("can't open the file");
    fileHandle_ = f;
    LARGE_INTEGER size;
    if (!GetFileSizeEx(f, &size) || size.QuadPart < (LONGLONG)sizeof(PackHeader) || size.QuadPart > 0x7FFFFFFF)
        return Fail("bad file size");
    size_ = (size_t)size.QuadPart;
    mapping_ = CreateFileMappingA(f, nullptr, PAGE_READONLY, 0, 0, nullptr);
    if (!mapping_) return Fail("can't map the file");
    base_ = (const uint8_t*)MapViewOfFile(mapping_, FILE_MAP_READ, 0, 0, 0);
    if (!base_) return Fail("can't map the file");

    header_ = (const PackHeader*)base_;
    if (memcmp(header_->magic, "RPAK", 4) != 0) return Fail("not a data pack");
    if (header_->version != kPackVersion) return Fail("unsupported pack version");
    uint64_t toc = header_->tocOffset, files = header_->fileCount, blobs = header_->blobCount;
    uint64_t end = toc + files * sizeof(PackFile) + blobs * sizeof(PackBlob) + header_->namesSize;
    if (toc < sizeof(PackHeader) || end > size_ || toc % 4) return Fail("bad table of contents");
    files_ = (const PackFile*)(base_ + toc);
    blobs_ = (const PackBlob*)(files_ + files);
    names_ = (const char*)(blobs_ + blobs);
    if (header_->namesSize == 0 || names_[header_->namesSize - 1] != 0) return Fail("bad names");

    for (uint32_t i = 0; i < blobs; ++i) {
        const PackBlob& b = blobs_[i];
        if ((uint64_t)b.offset + b.packedSize > toc || b.codec > kPackDeflate ||
            (b.codec == kPackStored && b.packedSize != b.size))
            return Fail("bad blob");
    }
    for (uint32_t i = 0; i < files; ++i) {
        const PackFile& p = files_[i];
        if (p.name >= header_->namesSize || p.kind > kPackIndex4 || p.data >= blobs) return Fail("bad file entry");
        uint64_t pixels = (uint64_t)p.width * p.height, want = 0;
        switch (p.kind) {
        case kPackArgb: want = pixels * 4; break;
        case kPackIndex8: want = pixels; break;
        case kPackIndex4: want = (pixels + 1) / 2; break;
        }
        if (p.kind != kPackRaw) {
            uint32_t maxColors = p.kind == kPackIndex4 ? 16 : 256;
            if (p.width == 0 || p.height == 0 || blobs_[p.data].size != want) return Fail("bad image entry");
            if (p.kind != kPackArgb &&
                (p.palette >= blobs || p.colors == 0 || p.colors > maxColors || blobs_[p.palette].size != p.colors * 4))
                return Fail("bad palette");
        }
        index_[Key(names_ + p.name)] = i;
    }
    error_.clear();
    return true;
}

void DataPack::Close() {
    if (base_) UnmapViewOfFile(base_);
    if (mapping_) CloseHandle(mapping_);
    if (fileHandle_) CloseHandle(fileHandle_);
    fileHandle_ = mapping_ = nullptr;
    base_ = nullptr;
    size_ = 0;
    header_ = nullptr;
    files_ = nullptr;
    blobs_ = nullptr;
    names_ = nullptr;
    index_.clear();
}

const PackFile* DataPack::Find(const std::string& name) const {
    auto it = index_.find(Key(name));
    return it == index_.end() ? nullptr : &files_[it->second];
}

bool DataPack::ReadBlob(uint32_t index, std::vector<uint8_t>& out) const {
    if (index >= BlobCount()) return false;
    const PackBlob& b = blobs_[index];
    out.resize(b.size);
    if (b.codec == kPackStored) {
        if (b.size) memcpy(out.data(), base_ + b.offset, b.size);
    } else if (!Inflate(base_ + b.offset, b.packedSize, out.data(), out.size())) {
        return false;   // (the inflater keeps its state on the stack, so reads from several threads are fine)
    }
    return Crc32(out.data(), out.size()) == b.crc;
}

bool DataPack::Read(const PackFile& f, std::vector<uint8_t>& out) const {
    if (f.kind == kPackRaw) return ReadBlob(f.data, out);
    std::vector<uint32_t> pixels;
    int w, h;
    if (!ReadImage(f, pixels, w, h)) return false;
    out.resize(128 + pixels.size() * 4);
    PackDdsHeader(out.data(), (uint32_t)w, (uint32_t)h);
    memcpy(out.data() + 128, pixels.data(), pixels.size() * 4);
    return true;
}

bool DataPack::ReadImage(const PackFile& f, std::vector<uint32_t>& pixels, int& width, int& height) const {
    if (f.kind == kPackRaw) return false;
    std::vector<uint8_t> data, palette;
    if (!ReadBlob(f.data, data)) return false;
    size_t count = (size_t)f.width * f.height;
    pixels.resize(count);
    if (f.kind == kPackArgb) {
        memcpy(pixels.data(), data.data(), count * 4);
    } else {
        if (!ReadBlob(f.palette, palette)) return false;
        uint32_t colors[256] = {};
        memcpy(colors, palette.data(), palette.size());
        if (f.kind == kPackIndex8) {
            for (size_t i = 0; i < count; ++i) pixels[i] = colors[data[i]];
        } else {
            for (size_t i = 0; i < count; ++i) pixels[i] = colors[(data[i >> 1] >> ((i & 1) * 4)) & 15];
        }
    }
    width = (int)f.width;
    height = (int)f.height;
    return true;
}

bool Pack_Mount(const std::string& packPath, const std::string& dir) {
    std::unique_ptr<DataPack> pack(new DataPack);
    if (!pack->Open(packPath)) return false;
    std::string key = FullKey(dir);
    if (key.empty() || key.back() != '\\') key += '\\';
    for (Mount& m : Mounts()) {
        if (m.dir == key) {
            m.pack = std::move(pack);
            return true;
        }
    }
    Mounts().push_back({key, std::move(pack)});
    return true;
}

const DataPack* Pack_Find(const std::string& path, const PackFile** file) {
    if (Mounts().empty()) return nullptr;
    std::string key = FullKey(path);
    for (const Mount& m : Mounts()) {
        if (key.compare(0, m.dir.size(), m.dir) != 0) continue;
        const PackFile* f = m.pack->Find(key.substr(m.dir.size()));
        if (f) {
            *file = f;
            return m.pack.get();
        }
    }
    return nullptr;
}

bool Data_ReadFile(const std::string& path, std::vector<uint8_t>& out) {
    if (ReadLoose(path, out)) return true;
    const PackFile* f;
    const DataPack* pack = Pack_Find(path, &f);
    return pack && pack->Read(*f, out);
}

bool Data_ReadLines(const std::string& path, std::vector<std::string>& lines) {
    std::vector<uint8_t> text;
    if (!Data_ReadFile(path, text)) return false;
    size_t start = 0;
    for (size_t i = 0; i <= text.size(); ++i) {
        if (i < text.size() && text[i] != '\n') continue;
        size_t end = i > start && text[i - 1] == '\r' ? i - 1 : i;
        if (i < text.size() || end > start) lines.push_back(std::string((const char*)text.data() + start, end - start));
        start = i + 1;
    }
    return true;
}

bool Data_Exists(const std::string& path) {
    DWORD a = GetFileAttributesA(path.c_str());
    if (a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY)) return true;
    const PackFile* f;
    return Pack_Find(path, &f) != nullptr;
}

void Data_List(const std::string& dir, const char* ext, std::vector<std::string>& out) {
    std::vector<std::string> found;
    ListLoose(dir, "", ext, found);
    std::string key = FullKey(dir);
    if (key.empty() || key.back() != '\\') key += '\\';
    for (const Mount& m : Mounts()) {
        if (m.dir != key) continue;
        for (uint32_t i = 0; i < m.pack->FileCount(); ++i) {
            const char* name = m.pack->Name(m.pack->File(i));
            if (EndsWith(name, ext)) found.push_back(name);
        }
    }
    std::set<std::string> seen;
    for (const std::string& s : found)
        if (seen.insert(DataPack::Key(s)).second) out.push_back(s);
}

}  // namespace util
