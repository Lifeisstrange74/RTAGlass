#include "soundfont.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include <algorithm>

namespace audio {

namespace {

uint16_t Rd16(const uint8_t* p) { return (uint16_t)(p[0] | (p[1] << 8)); }
uint32_t Rd32(const uint8_t* p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24); }

// Generator numbers used here.
enum {
    kStartOffset = 0, kEndOffset = 1, kStartLoopOffset = 2, kEndLoopOffset = 3, kStartCoarse = 4,
    kEndCoarse = 12, kReverb = 16, kPan = 17, kHoldVol = 35, kAttackVol = 34, kDecayVol = 36, kSustainVol = 37,
    kReleaseVol = 38, kInstrument = 41, kKeyRange = 43, kVelRange = 44, kStartLoopCoarse = 45, kAttenuation = 48,
    kEndLoopCoarse = 50, kCoarseTune = 51, kFineTune = 52, kSampleId = 53, kSampleModes = 54, kScaleTuning = 56,
    kRootKey = 58, kGenCount = 61
};

struct Gens {
    int v[kGenCount];
    bool set[kGenCount];
    Gens() {
        memset(set, 0, sizeof set);
        memset(v, 0, sizeof v);
    }
    static Gens Defaults() {
        Gens g;
        static const int kTimes[] = {kHoldVol, kAttackVol, kDecayVol, kReleaseVol, 33 /* delayVolEnv */};
        for (int i : kTimes) g.v[i] = -12000;
        g.v[kKeyRange] = 127 << 8;
        g.v[kVelRange] = 127 << 8;
        g.v[kScaleTuning] = 100;
        g.v[kRootKey] = -1;
        return g;
    }
};

struct Chunk {
    const uint8_t* p = nullptr;
    uint32_t size = 0;
};

// The zones of a bag list as generator sets: [first, last) bags of one preset or instrument.
void ReadZones(const Chunk& bag, const Chunk& gen, int first, int last, std::vector<Gens>& out,
               const Gens& base) {
    for (int b = first; b < last; ++b) {
        if ((uint32_t)(b + 1) * 4 + 4 > bag.size) break;
        int g0 = Rd16(bag.p + b * 4), g1 = Rd16(bag.p + (b + 1) * 4);
        Gens z = base;
        for (int g = g0; g < g1 && (uint32_t)g * 4 + 4 <= gen.size; ++g) {
            int op = Rd16(gen.p + g * 4);
            int amount = (int16_t)Rd16(gen.p + g * 4 + 2);
            if (op == kKeyRange || op == kVelRange) amount = Rd16(gen.p + g * 4 + 2);
            if (op < kGenCount) {
                z.v[op] = amount;
                z.set[op] = true;
            }
        }
        out.push_back(z);
    }
}

float Seconds(int timecents) { return timecents <= -12000 ? 0.f : (float)pow(2.0, timecents / 1200.0); }

}  // namespace

bool SoundFont::Load(const std::string& path) {
    samples_.clear();
    presets_.clear();
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
    if (!read || d.size() < 12 || memcmp(&d[0], "RIFF", 4) || memcmp(&d[8], "sfbk", 4)) {
        error_ = path + " is not a SoundFont 2 file";
        return false;
    }

    // Find the chunks inside the three LISTs.
    Chunk smpl, phdr, pbag, pgen, inst, ibag, igen, shdr;
    for (size_t pos = 12; pos + 12 <= d.size();) {
        uint32_t size = Rd32(&d[pos + 4]);
        if (!memcmp(&d[pos], "LIST", 4)) {
            size_t end = std::min(d.size(), pos + 8 + size);
            for (size_t q = pos + 12; q + 8 <= end;) {
                uint32_t cs = Rd32(&d[q + 4]);
                Chunk c{&d[q + 8], (uint32_t)std::min<size_t>(cs, end - q - 8)};
                const char* t = (const char*)&d[q];
                if (!memcmp(t, "smpl", 4)) smpl = c;
                else if (!memcmp(t, "phdr", 4)) phdr = c;
                else if (!memcmp(t, "pbag", 4)) pbag = c;
                else if (!memcmp(t, "pgen", 4)) pgen = c;
                else if (!memcmp(t, "inst", 4)) inst = c;
                else if (!memcmp(t, "ibag", 4)) ibag = c;
                else if (!memcmp(t, "igen", 4)) igen = c;
                else if (!memcmp(t, "shdr", 4)) shdr = c;
                q += 8 + cs + (cs & 1);
            }
        }
        pos += 8 + size + (size & 1);
    }
    if (!smpl.p || phdr.size < 76 || inst.size < 44 || shdr.size < 92) {
        error_ = path + " is missing SoundFont chunks";
        return false;
    }
    samples_.resize(smpl.size / 2);
    memcpy(samples_.data(), smpl.p, samples_.size() * 2);
    uint32_t nsamples = shdr.size / 46 - 1, ninst = inst.size / 22 - 1;

    for (uint32_t p = 0; p + 1 < phdr.size / 38; ++p) {
        const uint8_t* h = phdr.p + p * 38;
        Preset pr;
        pr.program = Rd16(h + 20);
        pr.bank = Rd16(h + 22);
        std::vector<Gens> pzones;
        ReadZones(pbag, pgen, Rd16(h + 24), Rd16(h + 38 + 24), pzones, Gens());
        Gens pglobal;
        if (!pzones.empty() && !pzones[0].set[kInstrument]) {
            pglobal = pzones[0];
            pzones.erase(pzones.begin());
        }
        for (Gens pz : pzones) {
            if (!pz.set[kInstrument] || (uint32_t)pz.v[kInstrument] >= ninst) continue;
            for (int i = 0; i < kGenCount; ++i)
                if (!pz.set[i] && pglobal.set[i]) pz.v[i] = pglobal.v[i], pz.set[i] = true;
            const uint8_t* ih = inst.p + pz.v[kInstrument] * 22;
            std::vector<Gens> izones;
            ReadZones(ibag, igen, Rd16(ih + 20), Rd16(ih + 22 + 20), izones, Gens::Defaults());
            Gens iglobal = Gens::Defaults();
            if (!izones.empty() && !izones[0].set[kSampleId]) {
                iglobal = izones[0];
                izones.erase(izones.begin());
            }
            for (const Gens& izr : izones) {
                if (!izr.set[kSampleId] || (uint32_t)izr.v[kSampleId] >= nsamples) continue;
                Gens g = izr;
                for (int i = 0; i < kGenCount; ++i)
                    if (!izr.set[i] && iglobal.set[i]) g.v[i] = iglobal.v[i];
                // Preset generators add to instrument ones, except ranges, which intersect.
                for (int i = 0; i < kGenCount; ++i) {
                    if (!pz.set[i] || i == kInstrument || i == kSampleId || i == kSampleModes || i == kRootKey ||
                        (i >= kStartOffset && i <= kEndLoopOffset) || i == kStartCoarse || i == kEndCoarse ||
                        i == kStartLoopCoarse || i == kEndLoopCoarse)
                        continue;
                    if (i == kKeyRange || i == kVelRange) {
                        int lo = std::max(g.v[i] & 255, pz.v[i] & 255), hi = std::min(g.v[i] >> 8, pz.v[i] >> 8);
                        g.v[i] = lo | (hi << 8);
                    } else {
                        g.v[i] += pz.v[i];
                    }
                }
                const uint8_t* s = shdr.p + g.v[kSampleId] * 46;
                Zone z;
                z.keyLo = g.v[kKeyRange] & 255;
                z.keyHi = g.v[kKeyRange] >> 8;
                z.velLo = g.v[kVelRange] & 255;
                z.velHi = g.v[kVelRange] >> 8;
                z.start = Rd32(s + 20) + g.v[kStartOffset] + g.v[kStartCoarse] * 32768;
                z.end = Rd32(s + 24) + g.v[kEndOffset] + g.v[kEndCoarse] * 32768;
                z.loopStart = Rd32(s + 28) + g.v[kStartLoopOffset] + g.v[kStartLoopCoarse] * 32768;
                z.loopEnd = Rd32(s + 32) + g.v[kEndLoopOffset] + g.v[kEndLoopCoarse] * 32768;
                z.end = std::min<uint32_t>(z.end, (uint32_t)samples_.size());
                if (z.start >= z.end) continue;
                z.loop = (g.v[kSampleModes] & 1) && z.loopStart < z.loopEnd && z.loopEnd <= z.end &&
                         z.loopStart >= z.start;
                z.rate = std::max<uint32_t>(Rd32(s + 36), 1);
                int root = g.v[kRootKey] >= 0 ? g.v[kRootKey] : s[40];
                z.rootKey = root - g.v[kCoarseTune] - (g.v[kFineTune] + (int8_t)s[41]) / 100.f;
                z.gain = (float)pow(10.0, -std::max(0, g.v[kAttenuation]) / 200.0);
                z.pan = std::max(-500, std::min(500, g.v[kPan])) / 1000.f;
                z.reverb = std::max(0, std::min(1000, g.v[kReverb])) / 1000.f;
                z.attack = Seconds(g.v[kAttackVol]);
                z.hold = Seconds(g.v[kHoldVol]);
                z.decay = Seconds(g.v[kDecayVol]);
                z.release = Seconds(g.v[kReleaseVol]);
                z.sustain = (float)pow(10.0, -std::max(0, std::min(1440, g.v[kSustainVol])) / 200.0);
                pr.zones.push_back(z);
            }
        }
        presets_.push_back(std::move(pr));
    }
    return true;
}

void SoundFont::Find(int bank, int program, int key, int velocity, std::vector<const Zone*>& out) const {
    out.clear();
    for (const Preset& p : presets_) {
        if (p.bank != bank || p.program != program) continue;
        for (const Zone& z : p.zones)
            if (key >= z.keyLo && key <= z.keyHi && velocity >= z.velLo && velocity <= z.velHi) out.push_back(&z);
        return;
    }
}

}  // namespace audio
