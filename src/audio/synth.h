// Software synthesizer: plays MidiFiles with SoundFont instruments and mixes everything to 16-bit stereo.
// Any number of sequences play at once (a song plus sound effects); each has its own 64 channels.
//
// Conventions shared with tools/sndconv (see its header comment):
//   - Pan is linear and 0 dB in the centre: left = 2*(1-q), right = 2*q with q = CC10/128 (+ the zone's pan).
//   - CC7 and CC11 are squared, as in General MIDI. RPN 0 sets the pitch bend range.
//   - CC91 is the reverb send.
// Not thread-safe: the caller locks (see sound.cpp).
#pragma once

#include <stdint.h>

#include <vector>

#include "midi_file.h"
#include "soundfont.h"

namespace audio {

// A sound mixed in with the synth's voices (the radio's stream): adds `frames` stereo frames at `rate` to `mix`.
class SynthStream {
public:
    virtual ~SynthStream() = default;
    virtual void Mix(float* mix, int frames, int rate) = 0;
};

class Synth {
public:
    explicit Synth(int rate = 48000);

    // Starts a sequence. `tag` is the caller's id for it (used by StopTag). Returns a handle, never 0.
    int Play(const MidiFile* seq, const SoundFont* font, uint32_t tag, float volume = 1.f);
    // Stops a sequence; its notes release (or are cut, if `now`).
    void Stop(int handle, bool now = false);
    void StopTag(uint32_t tag, bool now = false);
    void StopAll(bool now = false);
    void StopAllBut(int keep, bool now = false);   // every sequence but `keep` (a handle, 0 none)
    bool Playing(int handle) const;
    void SetVolume(int handle, float volume);
    void SetTagVolume(uint32_t tag, float volume);

    // Direct voices (SNDMOD's diwork, played by procDirect): a program's sample keyed on outside any sequence, with
    // its pitch (playback rate over the sample's own, the SPU2's pitch / 0x1000), levels (1 = full) and reverb send
    // set by the caller. The envelope is SNDMOD's for them (ADSR 0x13FF / 0x5FC5): a 1.3 ms attack, full sustain and
    // a short release. The engines use them (sound.cpp). Returns a handle, 0 if the program is missing.
    int DirectOn(const SoundFont* font, int program, float pitch, float left, float right, float send);
    void DirectSet(int handle, float pitch, float left, float right, float send);
    void DirectOff(int handle);
    // Output level for everything (1 = full) and mono output (both sides get the average).
    void SetMasterVolume(float volume) { master_ = volume; }
    void SetMono(bool mono) { mono_ = mono; }
    // A stream mixed in after the voices (dry, no reverb), or null.
    void SetStream(SynthStream* stream) { stream_ = stream; }

    // Mixes `frames` stereo frames.
    void Render(int16_t* out, int frames);

    int ActiveVoices() const;

private:
    struct Channel {
        int program = 0, bank = 0, rpn = 0x3FFF, bendRange = 2;
        float volume = 100 / 127.f, expression = 1, pan = 64, reverb = 0, bend = 0;
        void Reset() { *this = Channel(); }
    };
    struct Player {
        int handle = 0;
        uint32_t tag = 0;
        const MidiFile* seq = nullptr;
        const SoundFont* font = nullptr;
        size_t next = 0;
        double tick = 0, ticksPerFrame = 0;
        float volume = 1;
        bool stopped = false;   // no more events; ends when its voices are done
        Channel ch[64];
    };
    enum Stage { kOff, kAttack, kHold, kDecay, kSustain, kRelease };
    struct Voice {
        Stage stage = kOff;
        int player = 0, channel = 0, key = 0;
        uint32_t age = 0;
        const Zone* zone = nullptr;
        const int16_t* data = nullptr;
        double pos = 0, step = 0;
        float env = 0, stageTime = 0, decayMul = 1, releaseMul = 1;
        float velocity = 1;   // gain from the note-on velocity
        float gainL = 0, gainR = 0, send = 0;
        int direct = 0;     // a direct voice's handle (it has no player)
        Zone own;           // a direct voice's zone: the program's sample with SNDMOD's envelope
    };

    Player* Find(int handle);
    const Player* Find(int handle) const;
    void Dispatch(Player& p, const MidiEvent& e);
    void NoteOn(Player& p, int ch, int key, int velocity);
    void NoteOff(Player& p, int ch, int key);
    void UpdateVoice(Voice& v);   // pitch and gains from the channel
    Voice* FreeVoice();
    Voice* FindDirect(int handle);
    void Advance(Player& p, int frames);
    void MixVoice(Voice& v, float* mix, float* rev, int frames);
    void Reverb(const float* in, float* mix, int frames);

    int rate_;
    float master_ = 1;
    bool mono_ = false;
    SynthStream* stream_ = nullptr;
    int nextHandle_ = 1;
    int nextDirect_ = 1;
    uint32_t clock_ = 0;
    std::vector<Player> players_;
    std::vector<Voice> voices_;
    std::vector<const Zone*> zones_;
    std::vector<float> mix_, rev_;
    // Reverb: four combs and two allpasses per side (Freeverb's tunings).
    std::vector<float> comb_[8], allpass_[4];
    int combPos_[8] = {}, allpassPos_[4] = {};
    float combStore_[8] = {};
};

}  // namespace audio
