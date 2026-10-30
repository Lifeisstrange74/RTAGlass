#include "midi_file.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include <algorithm>

namespace audio {

namespace {

uint32_t Be32(const uint8_t* p) { return ((uint32_t)p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3]; }

bool Vlq(const uint8_t*& p, const uint8_t* end, uint32_t& v) {
    v = 0;
    for (int i = 0; i < 4 && p < end; ++i) {
        uint8_t b = *p++;
        v = (v << 7) | (b & 127);
        if (!(b & 128)) return true;
    }
    return false;
}

bool Matches(const uint8_t* p, uint32_t len, const char* text) {
    size_t n = strlen(text);
    if (len != n) return false;
    for (size_t i = 0; i < n; ++i)
        if (tolower(p[i]) != tolower((unsigned char)text[i])) return false;
    return true;
}

}  // namespace

bool MidiFile::Load(const std::string& path) {
    events_.clear();
    length_ = loopStart_ = loopEnd_ = 0;
    FILE* fp = fopen(path.c_str(), "rb");
    if (!fp) {
        error_ = "can't open " + path;
        return false;
    }
    fseek(fp, 0, SEEK_END);
    long n = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    std::vector<uint8_t> d(n > 0 ? (size_t)n : 0);
    bool read = fread(d.data(), 1, d.size(), fp) == d.size();
    fclose(fp);
    if (!read || d.size() < 14 || memcmp(&d[0], "MThd", 4)) {
        error_ = path + " is not a MIDI file";
        return false;
    }
    division_ = (d[12] << 8) | d[13];
    if (division_ & 0x8000 || division_ == 0) {
        error_ = path + ": SMPTE time is not supported";
        return false;
    }

    // Tracks are read one after another; a stable sort by tick then keeps track order for equal ticks.
    std::vector<MidiEvent> all;
    bool haveStart = false, haveEnd = false;
    for (size_t pos = 8 + Be32(&d[4]); pos + 8 <= d.size();) {
        uint32_t size = Be32(&d[pos + 4]);
        const uint8_t* p = &d[pos + 8];
        const uint8_t* end = &d[0] + std::min(d.size(), pos + 8 + size);
        bool track = !memcmp(&d[pos], "MTrk", 4);
        pos += 8 + size;
        if (!track) continue;
        uint32_t tick = 0;
        uint8_t running = 0, port = 0;
        while (p < end) {
            uint32_t delta;
            if (!Vlq(p, end, delta)) break;
            tick += delta;
            if (p >= end) break;
            uint8_t st = *p;
            if (st == 0xFF) {
                if (p + 2 > end) break;
                uint8_t type = p[1];
                p += 2;
                uint32_t len;
                if (!Vlq(p, end, len) || p + len > end) break;
                if (type == 0x2F) break;
                if (type == 0x21 && len >= 1) port = p[0] & 3;
                if (type == 0x51 && len == 3) {
                    MidiEvent e = {tick, 0xFF, 0, 0, 0, ((uint32_t)p[0] << 16) | (p[1] << 8) | p[2]};
                    all.push_back(e);
                }
                if (type == 0x06 || type == 0x01) {
                    if (Matches(p, len, "loopStart")) loopStart_ = tick, haveStart = true;
                    if (Matches(p, len, "loopEnd")) loopEnd_ = tick, haveEnd = true;
                }
                p += len;
                length_ = std::max(length_, tick);
                continue;
            }
            if (st == 0xF0 || st == 0xF7) {   // sysex: skip
                ++p;
                uint32_t len;
                if (!Vlq(p, end, len) || p + len > end) break;
                p += len;
                continue;
            }
            if (st & 0x80) {
                running = st;
                ++p;
            }
            if (!(running & 0x80)) break;
            int args = (running & 0xF0) == 0xC0 || (running & 0xF0) == 0xD0 ? 1 : 2;
            if (p + args > end) break;
            MidiEvent e = {tick, (uint8_t)(running & 0xF0), (uint8_t)(port * 16 + (running & 15)), p[0],
                           args == 2 ? p[1] : (uint8_t)0, 0};
            p += args;
            all.push_back(e);
            length_ = std::max(length_, tick);
        }
    }
    std::stable_sort(all.begin(), all.end(), [](const MidiEvent& x, const MidiEvent& y) { return x.tick < y.tick; });
    events_ = std::move(all);
    if (!haveEnd) loopEnd_ = haveStart ? length_ : 0;
    if (!haveStart) loopStart_ = 0;
    if (!haveStart && !haveEnd) loopStart_ = loopEnd_ = 0;
    return true;
}

}  // namespace audio
