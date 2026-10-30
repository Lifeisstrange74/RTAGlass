#include "adpcm.h"

#include <math.h>
#include <string.h>

#include <algorithm>

namespace audio {

namespace {

const int kFilter[5][2] = {{0, 0}, {60, 0}, {115, -52}, {98, -55}, {122, -60}};

uint32_t Rd32(const uint8_t* p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24); }

void Put16(std::vector<uint8_t>& v, int x) {
    v.push_back((uint8_t)x);
    v.push_back((uint8_t)(x >> 8));
}

void Put32(std::vector<uint8_t>& v, uint32_t x) {
    Put16(v, (int)(x & 0xFFFF));
    Put16(v, (int)(x >> 16));
}

}  // namespace

void Adpcm_DecodeBlock(const uint8_t* b, int& s1, int& s2, int16_t* out, int stride) {
    int shift = b[0] & 15, filter = std::min(b[0] >> 4, 4);
    const int f0 = kFilter[filter][0], f1 = kFilter[filter][1];
    for (int i = 0; i < kAdpcmBlockSamples; ++i) {
        int n = (b[2 + i / 2] >> ((i & 1) * 4)) & 15;
        if (n >= 8) n -= 16;
        int v = ((n << 12) >> shift) + ((s1 * f0 + s2 * f1 + 32) >> 6);
        v = std::max(-32768, std::min(32767, v));
        out[i * stride] = (int16_t)v;
        s2 = s1;
        s1 = v;
    }
}

void Adpcm_EncodeBlock(const int16_t* in, int stride, int& s1, int& s2, uint8_t* out) {
    int64_t bestErr = INT64_MAX;
    int bestShift = 0, bestFilter = 0;
    int8_t best[kAdpcmBlockSamples] = {};
    for (int filter = 0; filter < 5; ++filter) {
        for (int shift = 0; shift <= 12; ++shift) {
            int8_t nib[kAdpcmBlockSamples];
            int p1 = s1, p2 = s2;
            const int step = 4096 >> shift;
            int64_t err = 0;
            for (int i = 0; i < kAdpcmBlockSamples && err < bestErr; ++i) {
                int x = in[i * stride];
                int pred = (p1 * kFilter[filter][0] + p2 * kFilter[filter][1] + 32) >> 6;
                // The value whose step lands nearest; then the decoder's own result from it.
                int r = x - pred;
                int n = std::max(-8, std::min(7, (r >= 0 ? r + step / 2 : r - step / 2) / step));
                int v = std::max(-32768, std::min(32767, ((n << 12) >> shift) + pred));
                err += (int64_t)(v - x) * (v - x);
                nib[i] = (int8_t)n;
                p2 = p1;
                p1 = v;
            }
            if (err < bestErr) {
                bestErr = err;
                bestShift = shift;
                bestFilter = filter;
                memcpy(best, nib, sizeof best);
            }
        }
    }
    out[0] = (uint8_t)(bestFilter << 4 | bestShift);
    out[1] = 0;   // no flags
    for (int i = 0; i < 14; ++i) out[2 + i] = (uint8_t)((best[i * 2] & 15) | (best[i * 2 + 1] & 15) << 4);
    int16_t pcm[kAdpcmBlockSamples];
    Adpcm_DecodeBlock(out, s1, s2, pcm);
}

bool Radio_ParseHeader(const uint8_t* p, int64_t fileSize, RadioHeader& h) {
    if (fileSize < kRadioHeaderSize || memcmp(p, "RADP", 4) || Rd32(p + 4) != 1) return false;
    h.rate = (int)Rd32(p + 8);
    h.channels = (int)Rd32(p + 12);
    h.blocks = Rd32(p + 16);
    h.seekBlocks = (int)Rd32(p + 20);
    h.seekCount = Rd32(p + 24);
    h.dataOffset = Rd32(p + 28);
    return h.rate > 0 && (h.channels == 1 || h.channels == 2) && h.blocks > 0 && h.seekBlocks > 0 &&
           h.seekBlocks <= 1 << 16 && h.seekCount == (h.blocks + h.seekBlocks - 1) / h.seekBlocks &&
           h.dataOffset >= kRadioHeaderSize + (int64_t)h.seekCount * h.channels * 4 &&
           h.dataOffset + h.blocks * kAdpcmBlockBytes * h.channels <= fileSize;
}

std::vector<uint8_t> Radio_BuildFile(const std::vector<std::vector<uint8_t>>& sides, int rate) {
    const uint32_t channels = (uint32_t)sides.size(), blocks = (uint32_t)(sides[0].size() / kAdpcmBlockBytes);
    const uint32_t seeks = (blocks + kRadioSeekBlocks - 1) / kRadioSeekBlocks;
    const uint32_t dataOffset = (kRadioHeaderSize + seeks * channels * 4 + 15) & ~15u;
    std::vector<uint8_t> f;
    f.reserve(dataOffset + (size_t)blocks * kAdpcmBlockBytes * channels);
    f.insert(f.end(), {'R', 'A', 'D', 'P'});
    Put32(f, 1);   // version
    Put32(f, (uint32_t)rate);
    Put32(f, channels);
    Put32(f, blocks);
    Put32(f, kRadioSeekBlocks);
    Put32(f, seeks);
    Put32(f, dataOffset);
    // The seek points: the two samples before each kRadioSeekBlocks'th block, found by decoding everything once.
    std::vector<int> s1(channels, 0), s2(channels, 0);
    int16_t pcm[kAdpcmBlockSamples];
    for (uint32_t b = 0; b < blocks; ++b) {
        for (uint32_t c = 0; c < channels; ++c) {
            if (b % kRadioSeekBlocks == 0) {
                Put16(f, s1[c]);
                Put16(f, s2[c]);
            }
            Adpcm_DecodeBlock(sides[c].data() + (size_t)b * kAdpcmBlockBytes, s1[c], s2[c], pcm);
        }
    }
    f.resize(dataOffset, 0);
    for (uint32_t b = 0; b < blocks; ++b)
        for (uint32_t c = 0; c < channels; ++c)
            f.insert(f.end(), sides[c].begin() + (size_t)b * kAdpcmBlockBytes,
                     sides[c].begin() + (size_t)(b + 1) * kAdpcmBlockBytes);
    return f;
}

std::vector<std::vector<uint8_t>> Radio_Encode(const std::vector<int16_t>& pcm, int channels, double* snr) {
    size_t frames = pcm.size() / channels, blocks = (frames + kAdpcmBlockSamples - 1) / kAdpcmBlockSamples;
    std::vector<std::vector<uint8_t>> sides(channels, std::vector<uint8_t>(blocks * kAdpcmBlockBytes));
    double signal = 0, noise = 0;
    for (int c = 0; c < channels; ++c) {
        int s1 = 0, s2 = 0;
        int16_t in[kAdpcmBlockSamples], out[kAdpcmBlockSamples];
        for (size_t b = 0; b < blocks; ++b) {
            for (int i = 0; i < kAdpcmBlockSamples; ++i) {
                size_t f = b * kAdpcmBlockSamples + i;
                in[i] = f < frames ? pcm[f * channels + c] : 0;
            }
            int d1 = s1, d2 = s2;
            uint8_t* block = sides[c].data() + b * kAdpcmBlockBytes;
            Adpcm_EncodeBlock(in, 1, s1, s2, block);
            Adpcm_DecodeBlock(block, d1, d2, out);
            for (int i = 0; i < kAdpcmBlockSamples; ++i) {
                signal += (double)in[i] * in[i];
                noise += (double)(out[i] - in[i]) * (out[i] - in[i]);
            }
        }
    }
    if (snr) *snr = noise > 0 ? 10 * log10(signal / noise) : 99.0;
    return sides;
}

bool Radio_Decode(const std::vector<uint8_t>& file, std::vector<int16_t>& pcm, int& channels, int& rate) {
    RadioHeader h;
    if (file.size() < (size_t)kRadioHeaderSize || !Radio_ParseHeader(file.data(), (int64_t)file.size(), h))
        return false;
    channels = h.channels;
    rate = h.rate;
    pcm.assign((size_t)h.blocks * kAdpcmBlockSamples * channels, 0);
    std::vector<int> s1(channels, 0), s2(channels, 0);
    const uint8_t* data = file.data() + h.dataOffset;
    for (int64_t b = 0; b < h.blocks; ++b)
        for (int c = 0; c < channels; ++c)
            Adpcm_DecodeBlock(data + (b * channels + c) * kAdpcmBlockBytes, s1[c], s2[c],
                              pcm.data() + b * kAdpcmBlockSamples * channels + c, channels);
    return true;
}

}  // namespace audio
