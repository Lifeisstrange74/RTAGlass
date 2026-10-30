#include "image_write.h"

#include <stdio.h>
#include <string.h>

#include <algorithm>
#include <utility>
#include <vector>

namespace {

// ---- deflate (RFC 1951) ----
//
// Two passes. LZ77 first: a hash chain over the last 32 KB finds the longest earlier copy of the bytes ahead, with
// one step of lazy matching (a match is put off by a byte when the next byte starts a longer one). That gives a list
// of tokens, literals and (length, distance) copies. Then the tokens go out in blocks of up to kBlockTokens, each
// with the Huffman codes made from its own symbol counts (a dynamic block), or the standard codes (a fixed block)
// when that comes out smaller, as it can for a short block where the code tables would cost more than they save.

class BitWriter {
public:
    std::vector<uint8_t> out;
    void Put(uint32_t bits, int n) {   // LSB-first, n <= 24
        acc_ |= bits << count_;
        count_ += n;
        while (count_ >= 8) { out.push_back((uint8_t)acc_); acc_ >>= 8; count_ -= 8; }
    }
    void Flush() { if (count_) Put(0, 8 - count_); }
private:
    uint32_t acc_ = 0;
    int count_ = 0;
};

const int kLenBase[29] = {3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31,
                          35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
const int kLenExtra[29] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
const int kDistBase[30] = {1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769,
                           1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
const int kDistExtra[30] = {0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8,
                            9, 9, 10, 10, 11, 11, 12, 12, 13, 13};
// The code-length code's lengths go out in this order (the ones most often unused last, so they can be left off).
const uint8_t kClOrder[19] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};

// A match length (3-258) or distance (1-32768) to its code: filled before main, so threads can share them.
struct CodeTables {
    uint8_t len[259];
    uint8_t dist[512];   // distances 1-256 at dist - 1, longer ones at 256 + (dist - 1) / 128
    CodeTables() {
        for (int i = 0; i < 29; ++i)
            for (int l = kLenBase[i]; l < kLenBase[i] + (1 << kLenExtra[i]) && l <= 258; ++l) len[l] = (uint8_t)i;
        len[258] = 28;   // 258 has its own code, not 227 + 31 in code 27
        for (int i = 0; i < 30; ++i)
            for (int d = kDistBase[i]; d < kDistBase[i] + (1 << kDistExtra[i]); ++d)
                dist[d <= 256 ? d - 1 : 256 + ((d - 1) >> 7)] = (uint8_t)i;
    }
} const kCodes;

int DistCode(int dist) { return kCodes.dist[dist <= 256 ? dist - 1 : 256 + ((dist - 1) >> 7)]; }

struct Token {
    uint16_t len;    // the literal byte when dist is 0, else the match length
    uint16_t dist;
};

// LZ77: the input as literals and copies.
std::vector<Token> Match(const uint8_t* in, size_t size) {
    const int kWindow = 32768, kHashBits = 15, kMaxChain = 48;
    const int kGoodEnough = 32;   // a match this long isn't put off for a longer one at the next byte
    const int n = (int)size;
    std::vector<Token> tokens;
    tokens.reserve(size / 3 + 16);
    std::vector<int> head(1 << kHashBits, -1), prev(size, -1);
    int inserted = 0;   // positions below this are in the chains
    auto hash = [&](int p) {
        return ((in[p] << 10) ^ (in[p + 1] << 5) ^ in[p + 2]) & ((1 << kHashBits) - 1);
    };
    auto insertTo = [&](int end) {   // puts positions up to end (exclusive) in the chains
        for (; inserted < end; ++inserted) {
            if (inserted + 2 >= n) continue;
            int h = hash(inserted);
            prev[inserted] = head[h];
            head[h] = inserted;
        }
    };
    // The longest earlier copy of the bytes at p (bestLen 0 if none of at least 3).
    auto find = [&](int p, int& bestLen, int& bestDist) {
        bestLen = 0;
        bestDist = 0;
        if (p + 2 >= n) return;
        insertTo(p);
        int limit = n - p < 258 ? n - p : 258;
        int chain = kMaxChain;
        for (int c = head[hash(p)]; c >= 0 && p - c <= kWindow && chain--; c = prev[c]) {
            if (in[c + bestLen] != in[p + bestLen]) continue;
            int l = 0;
            while (l < limit && in[c + l] == in[p + l]) ++l;
            if (l > bestLen) { bestLen = l; bestDist = p - c; if (l == limit) break; }
        }
        if (bestLen < 3) bestLen = 0;
    };
    int len, dist;
    find(0, len, dist);
    for (int p = 0; p < n;) {
        if (len == 0) {
            tokens.push_back({in[p], 0});
            ++p;
            find(p, len, dist);
            continue;
        }
        int nextLen = 0, nextDist = 0;
        if (len < kGoodEnough && p + 1 < n) find(p + 1, nextLen, nextDist);
        if (nextLen > len) {   // a literal now and the longer match from the next byte
            tokens.push_back({in[p], 0});
            ++p;
            len = nextLen;
            dist = nextDist;
            continue;
        }
        tokens.push_back({(uint16_t)len, (uint16_t)dist});
        p += len;
        find(p, len, dist);
    }
    return tokens;
}

// Huffman code lengths for counts[0..n), none longer than maxLen: 0 for a symbol that isn't used. Gives a complete
// code: if fewer than two symbols are used, a second one gets a code too.
void CodeLengths(const uint32_t* counts, int n, int maxLen, uint8_t* lengths) {
    std::vector<uint32_t> weight(counts, counts + n);
    int used = 0;
    for (int i = 0; i < n; ++i) used += weight[i] != 0;
    for (int i = 0; used < 2 && i < n; ++i)
        if (!weight[i]) { weight[i] = 1; ++used; }
    for (;;) {
        // The usual tree: join the two lightest nodes until one is left. Nodes n.. are the joins.
        std::vector<int> parent(2 * n, -1);
        std::vector<std::pair<uint64_t, int>> heap;   // (weight, node), a min-heap through greater<>
        for (int i = 0; i < n; ++i)
            if (weight[i]) heap.push_back({weight[i], i});
        auto greater = [](const std::pair<uint64_t, int>& a, const std::pair<uint64_t, int>& b) { return a > b; };
        std::make_heap(heap.begin(), heap.end(), greater);
        int next = n;
        while (heap.size() > 1) {
            std::pop_heap(heap.begin(), heap.end(), greater);
            std::pair<uint64_t, int> a = heap.back();
            heap.pop_back();
            std::pop_heap(heap.begin(), heap.end(), greater);
            std::pair<uint64_t, int> b = heap.back();
            heap.pop_back();
            parent[a.second] = parent[b.second] = next;
            heap.push_back({a.first + b.first, next++});
            std::push_heap(heap.begin(), heap.end(), greater);
        }
        // Depths: a join's parent is always a later join, so one pass down from the root sets them all.
        std::vector<int> depth(next, 0);
        for (int i = next - 2; i >= 0; --i)
            if (parent[i] >= 0) depth[i] = depth[parent[i]] + 1;
        int longest = 0;
        for (int i = 0; i < n; ++i) {
            lengths[i] = weight[i] ? (uint8_t)depth[i] : 0;
            if (lengths[i] > longest) longest = lengths[i];
        }
        if (longest <= maxLen) return;
        // Too deep: flatten the counts and build again (rare; it costs a little compression, never correctness).
        for (int i = 0; i < n; ++i)
            if (weight[i]) weight[i] = (weight[i] >> 1) | 1;
    }
}

// The canonical codes for code lengths (RFC 1951 3.2.2), bit-reversed, as deflate sends Huffman codes MSB first.
void Codes(const uint8_t* lengths, int n, uint16_t* codes) {
    int count[16] = {}, next[16] = {};
    for (int i = 0; i < n; ++i) ++count[lengths[i]];
    count[0] = 0;
    for (int len = 1, code = 0; len < 16; ++len) {
        code = (code + count[len - 1]) << 1;
        next[len] = code;
    }
    for (int i = 0; i < n; ++i) {
        int len = lengths[i];
        if (!len) continue;
        int code = next[len]++, rev = 0;
        for (int b = 0; b < len; ++b) rev |= ((code >> b) & 1) << (len - 1 - b);
        codes[i] = (uint16_t)rev;
    }
}

// The literal/length and distance codes' lengths as code-length symbols: 0-15 a length, 16 the last length again
// 3-6 times, 17 3-10 zeros, 18 11-138 zeros (the count, less the minimum, in extra bits).
struct ClSymbol { uint8_t sym, extra; };
void RunLengths(const uint8_t* lengths, int n, std::vector<ClSymbol>& out) {
    for (int i = 0; i < n;) {
        int v = lengths[i], run = 1;
        while (i + run < n && lengths[i + run] == v) ++run;
        i += run;
        if (v == 0) {
            while (run >= 11) {
                int r = run < 138 ? run : 138;
                out.push_back({18, (uint8_t)(r - 11)});
                run -= r;
            }
            if (run >= 3) {
                out.push_back({17, (uint8_t)(run - 3)});
                run = 0;
            }
        } else {
            out.push_back({(uint8_t)v, 0});
            --run;
            while (run >= 3) {
                int r = run < 6 ? run : 6;
                out.push_back({16, (uint8_t)(r - 3)});
                run -= r;
            }
        }
        while (run-- > 0) out.push_back({(uint8_t)v, 0});
    }
}

void PutBlock(BitWriter& bw, const Token* tokens, size_t count, bool last) {
    // The symbol counts: literals 0-255, 256 the end of the block, 257-285 match lengths; distance codes 0-29.
    uint32_t litCount[286] = {}, distCount[30] = {};
    for (size_t i = 0; i < count; ++i) {
        if (tokens[i].dist == 0) {
            ++litCount[tokens[i].len];
        } else {
            ++litCount[257 + kCodes.len[tokens[i].len]];
            ++distCount[DistCode(tokens[i].dist)];
        }
    }
    litCount[256] = 1;

    // The dynamic codes and what their tables cost. Both sets of lengths are kept as [288 literal/length][30
    // distance]: the fixed code has 288 literal/length codes (286 and 287 are never sent), and the canonical codes of
    // the 9-bit literals come out right only when all 288 are counted.
    const int kLit = 288;
    uint8_t lengths[kLit + 30] = {};
    CodeLengths(litCount, 286, 15, lengths);
    CodeLengths(distCount, 30, 15, lengths + kLit);
    int nlit = 286, ndist = 30;
    while (nlit > 257 && lengths[nlit - 1] == 0) --nlit;
    while (ndist > 1 && lengths[kLit + ndist - 1] == 0) --ndist;
    uint8_t both[286 + 30];   // the two lists back to back, as they are sent (a run may cross from one to the other)
    memcpy(both, lengths, nlit);
    memcpy(both + nlit, lengths + kLit, ndist);
    std::vector<ClSymbol> cl;
    RunLengths(both, nlit + ndist, cl);
    uint32_t clCount[19] = {};
    for (const ClSymbol& s : cl) ++clCount[s.sym];
    uint8_t clLengths[19];
    CodeLengths(clCount, 19, 7, clLengths);
    int nclen = 19;
    while (nclen > 4 && clLengths[kClOrder[nclen - 1]] == 0) --nclen;

    // Which is smaller: the dynamic block with its tables, or the fixed one. (Extra bits cost the same in both.)
    static const int kClExtra[19] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 3, 7};
    uint8_t fixed[kLit + 30];
    for (int i = 0; i < kLit; ++i) fixed[i] = i < 144 ? 8 : i < 256 ? 9 : i < 280 ? 7 : 8;
    for (int i = 0; i < 30; ++i) fixed[kLit + i] = 5;
    uint64_t dynamicBits = 5 + 5 + 4 + 3 * nclen, fixedBits = 0;
    for (const ClSymbol& s : cl) dynamicBits += clLengths[s.sym] + kClExtra[s.sym];
    for (int i = 0; i < 286; ++i) {
        dynamicBits += (uint64_t)litCount[i] * lengths[i];
        fixedBits += (uint64_t)litCount[i] * fixed[i];
    }
    for (int i = 0; i < 30; ++i) {
        dynamicBits += (uint64_t)distCount[i] * lengths[kLit + i];
        fixedBits += (uint64_t)distCount[i] * fixed[kLit + i];
    }
    const bool dynamic = dynamicBits < fixedBits;
    const uint8_t* use = dynamic ? lengths : fixed;

    bw.Put(last ? 1 : 0, 1);   // BFINAL
    bw.Put(dynamic ? 2 : 1, 2);   // BTYPE: 2 dynamic, 1 fixed Huffman
    if (dynamic) {
        uint16_t clCodes[19] = {};
        Codes(clLengths, 19, clCodes);
        bw.Put(nlit - 257, 5);
        bw.Put(ndist - 1, 5);
        bw.Put(nclen - 4, 4);
        for (int i = 0; i < nclen; ++i) bw.Put(clLengths[kClOrder[i]], 3);
        for (const ClSymbol& s : cl) {
            bw.Put(clCodes[s.sym], clLengths[s.sym]);
            if (kClExtra[s.sym]) bw.Put(s.extra, kClExtra[s.sym]);
        }
    }
    uint16_t litCodes[kLit] = {}, distCodes[30] = {};
    Codes(use, kLit, litCodes);
    Codes(use + kLit, 30, distCodes);
    const uint8_t* litLen = use;
    const uint8_t* distLen = use + kLit;
    for (size_t i = 0; i < count; ++i) {
        const Token& t = tokens[i];
        if (t.dist == 0) {
            bw.Put(litCodes[t.len], litLen[t.len]);
            continue;
        }
        int lc = kCodes.len[t.len], dc = DistCode(t.dist);
        bw.Put(litCodes[257 + lc], litLen[257 + lc]);
        bw.Put(t.len - kLenBase[lc], kLenExtra[lc]);
        bw.Put(distCodes[dc], distLen[dc]);
        bw.Put(t.dist - kDistBase[dc], kDistExtra[dc]);
    }
    bw.Put(litCodes[256], litLen[256]);   // end of block
}

std::vector<uint8_t> Deflate(const uint8_t* in, size_t size) {
    const size_t kBlockTokens = 16384;
    std::vector<Token> tokens = Match(in, size);
    BitWriter bw;
    bw.out.reserve(size / 2 + 64);
    size_t i = 0;
    do {
        size_t count = tokens.size() - i < kBlockTokens ? tokens.size() - i : kBlockTokens;
        PutBlock(bw, tokens.data() + i, count, i + count == tokens.size());
        i += count;
    } while (i < tokens.size());
    bw.Flush();
    return bw.out;
}

// ---- PNG ----

uint32_t Crc(const uint8_t* p, size_t n) {
    static uint32_t table[256];
    static bool init = false;
    if (!init) {
        for (uint32_t i = 0; i < 256; ++i) {
            uint32_t v = i;
            for (int k = 0; k < 8; ++k) v = (v & 1) ? 0xEDB88320u ^ (v >> 1) : v >> 1;
            table[i] = v;
        }
        init = true;
    }
    uint32_t c = 0xFFFFFFFFu;
    for (size_t i = 0; i < n; ++i) c = table[(c ^ p[i]) & 0xFF] ^ (c >> 8);
    return c ^ 0xFFFFFFFFu;
}

void Put32(std::vector<uint8_t>& v, uint32_t x) {
    v.push_back((uint8_t)(x >> 24)); v.push_back((uint8_t)(x >> 16));
    v.push_back((uint8_t)(x >> 8)); v.push_back((uint8_t)x);
}

void Chunk(FILE* fp, const char* type, const std::vector<uint8_t>& data) {
    std::vector<uint8_t> c;
    Put32(c, (uint32_t)data.size());
    c.insert(c.end(), type, type + 4);
    c.insert(c.end(), data.begin(), data.end());
    Put32(c, Crc(c.data() + 4, c.size() - 4));
    fwrite(c.data(), 1, c.size(), fp);
}

void Put32Le(uint8_t* p, uint32_t v) {
    p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
}

}  // namespace

std::vector<uint8_t> DeflateRaw(const uint8_t* data, size_t size) { return Deflate(data, size); }

bool WritePng(const char* path, const uint32_t* rgba, int w, int h) {
    // Filter: Sub on every row (cheap, and helps the LZ77 on gradients and flat runs).
    std::vector<uint8_t> raw;
    raw.reserve((size_t)(w * 4 + 1) * h);
    for (int y = 0; y < h; ++y) {
        raw.push_back(1);
        const uint8_t* row = (const uint8_t*)(rgba + (size_t)y * w);
        for (int i = 0; i < w * 4; ++i) raw.push_back((uint8_t)(row[i] - (i >= 4 ? row[i - 4] : 0)));
    }
    std::vector<uint8_t> z = {0x78, 0x01};
    std::vector<uint8_t> body = Deflate(raw.data(), raw.size());
    z.insert(z.end(), body.begin(), body.end());
    uint32_t s1 = 1, s2 = 0;
    for (uint8_t b : raw) { s1 = (s1 + b) % 65521; s2 = (s2 + s1) % 65521; }
    Put32(z, (s2 << 16) | s1);

    FILE* fp = fopen(path, "wb");
    if (!fp) return false;
    static const uint8_t sig[8] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    fwrite(sig, 1, 8, fp);
    std::vector<uint8_t> ihdr;
    Put32(ihdr, (uint32_t)w); Put32(ihdr, (uint32_t)h);
    ihdr.push_back(8); ihdr.push_back(6); ihdr.push_back(0); ihdr.push_back(0); ihdr.push_back(0);
    Chunk(fp, "IHDR", ihdr);
    Chunk(fp, "IDAT", z);
    Chunk(fp, "IEND", std::vector<uint8_t>());
    fclose(fp);
    return true;
}

bool WriteDds(const char* path, const uint32_t* rgba, int w, int h) {
    uint8_t hdr[128];
    memset(hdr, 0, sizeof(hdr));
    memcpy(hdr, "DDS ", 4);
    Put32Le(hdr + 4, 124);                          // dwSize
    Put32Le(hdr + 8, 0x1 | 0x2 | 0x4 | 0x8 | 0x1000);   // CAPS | HEIGHT | WIDTH | PITCH | PIXELFORMAT
    Put32Le(hdr + 12, (uint32_t)h);
    Put32Le(hdr + 16, (uint32_t)w);
    Put32Le(hdr + 20, (uint32_t)w * 4);             // pitch
    Put32Le(hdr + 76, 32);                          // ddspf.dwSize
    Put32Le(hdr + 80, 0x40 | 0x1);                  // DDPF_RGB | DDPF_ALPHAPIXELS
    Put32Le(hdr + 88, 32);
    Put32Le(hdr + 92, 0x00FF0000);                  // R mask
    Put32Le(hdr + 96, 0x0000FF00);                  // G
    Put32Le(hdr + 100, 0x000000FF);                 // B
    Put32Le(hdr + 104, 0xFF000000);                 // A
    Put32Le(hdr + 108, 0x1000);                     // DDSCAPS_TEXTURE
    FILE* fp = fopen(path, "wb");
    if (!fp) return false;
    fwrite(hdr, 1, sizeof(hdr), fp);
    std::vector<uint32_t> bgra((size_t)w * h);
    for (size_t i = 0; i < bgra.size(); ++i) {
        uint32_t c = rgba[i];   // memory R,G,B,A -> B,G,R,A
        bgra[i] = (c & 0xFF00FF00u) | ((c & 0xFF) << 16) | ((c >> 16) & 0xFF);
    }
    fwrite(bgra.data(), 4, bgra.size(), fp);
    fclose(fp);
    return true;
}
