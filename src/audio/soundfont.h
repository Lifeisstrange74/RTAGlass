// SoundFont 2 loader: 16-bit samples, presets flattened to playable zones. Enough of the format for the game's
// banks (made by tools/sndconv) and for edited or replacement banks from SF2 editors such as Polyphone:
// global zones, preset generators added to instrument ones, key/velocity ranges, sample offsets, loops, tuning,
// volume envelope, attenuation, pan and reverb send. Modulators, filters, LFOs and the modulation envelope are
// ignored.
#pragma once

#include <stdint.h>

#include <string>
#include <vector>

namespace audio {

struct Zone {
    int keyLo = 0, keyHi = 127, velLo = 0, velHi = 127;
    uint32_t start = 0, end = 0, loopStart = 0, loopEnd = 0;   // sample indices into SoundFont::Samples()
    bool loop = false;
    float rootKey = 60;     // the key that plays the sample at its own rate, with tuning folded in
    uint32_t rate = 48000;
    float gain = 1;         // from initialAttenuation
    float pan = 0;          // -0.5 (left) .. 0.5 (right)
    float reverb = 0;       // 0..1
    float attack = 0, hold = 0, decay = 0, release = 0;   // seconds; decay and release are per 100 dB
    float sustain = 1;      // amplitude
};

class SoundFont {
public:
    bool Load(const std::string& path);
    const std::string& Error() const { return error_; }

    // The zones that play `key` at `velocity` for the preset; empty if there is no such preset.
    void Find(int bank, int program, int key, int velocity, std::vector<const Zone*>& out) const;
    const int16_t* Samples() const { return samples_.data(); }

private:
    struct Preset {
        int bank, program;
        std::vector<Zone> zones;
    };
    std::vector<int16_t> samples_;
    std::vector<Preset> presets_;
    std::string error_;
};

}  // namespace audio
