#include "radio.h"

#include <string.h>

#include <algorithm>

#include "adpcm.h"

namespace audio {

namespace {

const int64_t kViewSize = 2 << 20;   // the mapped window: 2 MB, 43 seconds of the disc's stations
const float kLevel = 0.5f;           // Synth's kMaster: an SPU2 voice at full level

uint32_t Rd32(const uint8_t* p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24); }
uint16_t Rd16(const uint8_t* p) { return (uint16_t)(p[0] | (p[1] << 8)); }

}  // namespace

bool RadioStream::Open(const std::string& path) {
    Close();
    file_ = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_FLAG_RANDOM_ACCESS,
                        nullptr);
    if (file_ == INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER size;
    bool ok = GetFileSizeEx(file_, &size) && (OpenAdpcm(size.QuadPart) || OpenWav(size.QuadPart));
    if (ok) {
        mapping_ = CreateFileMappingA(file_, nullptr, PAGE_READONLY, 0, 0, nullptr);
        ok = mapping_ && frames_ > 0;
    }
    if (!ok) Close();
    return ok;
}

bool RadioStream::OpenAdpcm(int64_t fileSize) {
    uint8_t raw0[kRadioHeaderSize];
    DWORD got = 0;
    LARGE_INTEGER zero = {};
    RadioHeader h;
    if (!SetFilePointerEx(file_, zero, nullptr, FILE_BEGIN) || !ReadFile(file_, raw0, sizeof raw0, &got, nullptr) ||
        got != sizeof raw0 || !Radio_ParseHeader(raw0, fileSize, h))
        return false;
    rate_ = h.rate;
    channels_ = h.channels;
    blocks_ = h.blocks;
    seekBlocks_ = h.seekBlocks;
    segFrames_ = (int64_t)seekBlocks_ * kAdpcmBlockSamples;
    // The seek points are small (a few KB an hour): read once.
    std::vector<uint8_t> raw(h.seekCount * channels_ * 4);
    if (!ReadFile(file_, raw.data(), (DWORD)raw.size(), &got, nullptr) || got != raw.size()) return false;
    seeks_.resize(h.seekCount * channels_ * 2);
    for (size_t i = 0; i < seeks_.size(); ++i) seeks_[i] = (int16_t)Rd16(raw.data() + i * 2);
    for (Decoded& d : decoded_) {
        d.segment = -1;
        d.pcm.assign((size_t)segFrames_ * channels_, 0);
    }
    data_ = h.dataOffset;
    dataSize_ = blocks_ * kAdpcmBlockBytes * channels_;
    frames_ = blocks_ * kAdpcmBlockSamples;
    adpcm_ = true;
    return true;
}

bool RadioStream::OpenWav(int64_t fileSize) {
    // The chunks: "fmt " for the format, "data" for the samples.
    uint8_t head[12];
    DWORD got = 0;
    LARGE_INTEGER zero = {};
    if (!SetFilePointerEx(file_, zero, nullptr, FILE_BEGIN) || !ReadFile(file_, head, 12, &got, nullptr) ||
        got != 12 || memcmp(head, "RIFF", 4) || memcmp(head + 8, "WAVE", 4))
        return false;
    int64_t at = 12;
    int format = 0, bits = 0;
    while (at + 8 <= fileSize && !data_) {
        uint8_t c[24];
        LARGE_INTEGER to;
        to.QuadPart = at;
        if (!SetFilePointerEx(file_, to, nullptr, FILE_BEGIN) || !ReadFile(file_, c, 24, &got, nullptr) || got < 8)
            break;
        uint32_t len = Rd32(c + 4);
        if (!memcmp(c, "fmt ", 4) && got >= 24) {
            format = Rd16(c + 8);
            channels_ = Rd16(c + 10);
            rate_ = (int)Rd32(c + 12);
            bits = Rd16(c + 22);
        } else if (!memcmp(c, "data", 4)) {
            data_ = at + 8;
            dataSize_ = std::min<int64_t>(len, fileSize - data_);
        }
        at += 8 + len + (len & 1);
    }
    if (format != 1 || bits != 16 || (channels_ != 1 && channels_ != 2) || rate_ <= 0 || !data_) {
        data_ = 0;
        return false;
    }
    frames_ = dataSize_ / (2 * channels_);
    adpcm_ = false;
    return true;
}

void RadioStream::Close() {
    if (view_) UnmapViewOfFile(view_);
    view_ = nullptr;
    viewStart_ = viewSize_ = 0;
    if (mapping_) CloseHandle(mapping_);
    mapping_ = 0;
    if (file_ != INVALID_HANDLE_VALUE) CloseHandle(file_);
    file_ = INVALID_HANDLE_VALUE;
    data_ = dataSize_ = frames_ = 0;
    playing_ = false;
    adpcm_ = false;
    segPcm_ = nullptr;
    seeks_.clear();
    for (Decoded& d : decoded_) {
        d.segment = -1;
        d.pcm.clear();
    }
}

void RadioStream::Start(int64_t frame, int64_t loop, int rate) {
    if (!IsOpen() || rate <= 0) return;
    frame = frame * rate_ / rate;
    loop = loop * rate_ / rate;
    loop_ = loop > 0 && loop <= frames_ ? loop : frames_;
    pos_ = ((frame % loop_) + loop_) % loop_;
    frac_ = 0;
    playing_ = true;
}

const uint8_t* RadioStream::Window(int64_t byte, int size) {
    if (!view_ || byte < viewStart_ || byte + size > viewStart_ + viewSize_) {
        if (view_) UnmapViewOfFile(view_);
        view_ = nullptr;
        // Views start on the allocation granularity (64 KB).
        SYSTEM_INFO si;
        GetSystemInfo(&si);
        int64_t grain = si.dwAllocationGranularity, end = data_ + dataSize_;
        viewStart_ = byte / grain * grain;
        viewSize_ = std::min(kViewSize, end - viewStart_);
        if (viewSize_ < size) return nullptr;
        view_ = (const uint8_t*)MapViewOfFile(mapping_, FILE_MAP_READ, (DWORD)(viewStart_ >> 32),
                                              (DWORD)(viewStart_ & 0xFFFFFFFF), (SIZE_T)viewSize_);
        if (!view_) return nullptr;
    }
    return view_ + (byte - viewStart_);
}

int64_t RadioStream::ByteOf(int64_t frame) const {
    return adpcm_ ? data_ + frame / kAdpcmBlockSamples * kAdpcmBlockBytes * channels_ : data_ + frame * 2 * channels_;
}

const int16_t* RadioStream::Segment(int64_t i) {
    int64_t segment = i / segFrames_;
    for (int k = 0; k < 2; ++k) {
        if (decoded_[k].segment == segment) {
            lastDecoded_ = k;
            return decoded_[k].pcm.data();
        }
    }
    // Decode it into the slot not used last, from its seek point.
    Decoded& d = decoded_[lastDecoded_ ^ 1];
    d.segment = -1;
    int64_t first = segment * seekBlocks_;
    int count = (int)std::min<int64_t>(seekBlocks_, blocks_ - first);
    const int bytes = count * kAdpcmBlockBytes * channels_;
    const uint8_t* p = count > 0 ? Window(data_ + first * kAdpcmBlockBytes * channels_, bytes) : nullptr;
    if (!p) return nullptr;
    for (int c = 0; c < channels_; ++c) {
        const int16_t* seek = seeks_.data() + (size_t)(segment * channels_ + c) * 2;
        int s1 = seek[0], s2 = seek[1];
        for (int b = 0; b < count; ++b)
            Adpcm_DecodeBlock(p + (b * channels_ + c) * kAdpcmBlockBytes, s1, s2,
                              d.pcm.data() + b * kAdpcmBlockSamples * channels_ + c, channels_);
    }
    d.segment = segment;
    lastDecoded_ ^= 1;
    return d.pcm.data();
}

void RadioStream::Frame(int64_t i, float* l, float* r) {
    // Mix asks for frames at most two loops on: subtracting is much cheaper than a 64-bit % on x86.
    while (i >= loop_) i -= loop_;
    if (adpcm_) {
        int64_t at = i - segStart_;
        if (!segPcm_ || at < 0 || at >= segFrames_) {
            segPcm_ = Segment(i);
            segStart_ = i / segFrames_ * segFrames_;
            at = i - segStart_;
        }
        if (!segPcm_) {
            *l = *r = 0;
            return;
        }
        const int16_t* f = segPcm_ + at * channels_;
        *l = f[0] * (1.f / 32768.f);
        *r = channels_ == 2 ? f[1] * (1.f / 32768.f) : *l;
        return;
    }
    int stride = 2 * channels_;
    const uint8_t* p = Window(data_ + i * stride, stride);
    if (!p) {
        *l = *r = 0;
        return;
    }
    *l = (int16_t)Rd16(p) * (1.f / 32768.f);
    *r = channels_ == 2 ? (int16_t)Rd16(p + 2) * (1.f / 32768.f) : *l;
}

void RadioStream::Prefetch() {
    if (!IsOpen() || !playing_) return;
    // A second's worth, a byte a page.
    volatile uint8_t sink = 0;
    for (int64_t i = 0, last = -1; i < rate_; i += kAdpcmBlockSamples) {
        int64_t page = ByteOf((pos_ + i) % loop_) / 4096;
        if (page == last) continue;
        last = page;
        if (const uint8_t* p = Window(page * 4096 < data_ ? data_ : page * 4096, 1)) sink = *p;
    }
    (void)sink;
}

void RadioStream::Mix(float* mix, int frames, int rate) {
    if (!playing_ || !IsOpen()) return;
    // Catmull-Rom between the file's frames (the SPU2 smooths its 12 kHz voices with its own 4-point filter).
    double step = (double)rate_ / rate;
    float gl = kLevel * left_, gr = kLevel * right_;
    for (int n = 0; n < frames; ++n) {
        float l[4], r[4];
        for (int k = 0; k < 4; ++k) Frame(pos_ + loop_ - 1 + k, &l[k], &r[k]);
        float t = (float)frac_;
        auto Cubic = [t](const float* s) {
            return s[1] + 0.5f * t * (s[2] - s[0] + t * (2 * s[0] - 5 * s[1] + 4 * s[2] - s[3] +
                                                       t * (3 * (s[1] - s[2]) + s[3] - s[0])));
        };
        mix[n * 2] += Cubic(l) * gl;
        mix[n * 2 + 1] += Cubic(r) * gr;
        frac_ += step;
        while (frac_ >= 1) {
            frac_ -= 1;
            if (++pos_ >= loop_) pos_ = 0;
        }
    }
}

}  // namespace audio
