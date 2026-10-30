#include "wav_file.h"

#include <string.h>

#include <algorithm>

namespace audio {

namespace {

uint16_t Rd16(const uint8_t* p) { return (uint16_t)(p[0] | (p[1] << 8)); }
uint32_t Rd32(const uint8_t* p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24); }

void Put16(std::vector<uint8_t>& v, int x) {
    v.push_back((uint8_t)x);
    v.push_back((uint8_t)(x >> 8));
}

void Put32(std::vector<uint8_t>& v, uint32_t x) {
    Put16(v, (int)(x & 0xFFFF));
    Put16(v, (int)(x >> 16));
}

void Tag(std::vector<uint8_t>& v, const char* t) { v.insert(v.end(), t, t + 4); }

}  // namespace

std::vector<uint8_t> Wav_Build(const std::vector<int16_t>& pcm, int channels, int rate) {
    uint32_t bytes = (uint32_t)pcm.size() * 2;
    std::vector<uint8_t> w;
    w.reserve(44 + bytes);
    Tag(w, "RIFF");
    Put32(w, 36 + bytes);
    Tag(w, "WAVE");
    Tag(w, "fmt ");
    Put32(w, 16);
    Put16(w, 1);   // PCM
    Put16(w, channels);
    Put32(w, (uint32_t)rate);
    Put32(w, (uint32_t)rate * channels * 2);
    Put16(w, channels * 2);
    Put16(w, 16);
    Tag(w, "data");
    Put32(w, bytes);
    for (int16_t s : pcm) Put16(w, s);
    return w;
}

bool Wav_Parse(const std::vector<uint8_t>& d, std::vector<int16_t>& pcm, int& channels, int& rate) {
    if (d.size() < 12 || memcmp(d.data(), "RIFF", 4) || memcmp(d.data() + 8, "WAVE", 4)) return false;
    int format = 0, bits = 0;
    channels = rate = 0;
    for (size_t at = 12; at + 8 <= d.size();) {
        uint32_t len = Rd32(d.data() + at + 4);
        if (!memcmp(d.data() + at, "fmt ", 4) && at + 24 <= d.size()) {
            format = Rd16(d.data() + at + 8);
            channels = Rd16(d.data() + at + 10);
            rate = (int)Rd32(d.data() + at + 12);
            bits = Rd16(d.data() + at + 22);
        } else if (!memcmp(d.data() + at, "data", 4)) {
            if (format != 1 || bits != 16 || (channels != 1 && channels != 2) || rate <= 0) return false;
            size_t n = std::min<size_t>(len, d.size() - at - 8) / 2 / channels * channels;
            pcm.resize(n);
            for (size_t i = 0; i < n; ++i) pcm[i] = (int16_t)Rd16(d.data() + at + 8 + i * 2);
            return true;
        }
        at += 8 + (size_t)len + (len & 1);
    }
    return false;
}

}  // namespace audio
