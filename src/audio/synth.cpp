#include "synth.h"

#include <math.h>

#include <algorithm>

namespace audio {

namespace {

const int kVoices = 96;
const int kBlock = 64;            // frames mixed between event checks (1.3 ms at 48 kHz)
const float kSilent = 1e-5f;      // -100 dB: a releasing voice ends here
const float kMaster = 0.5f;       // headroom for many voices; the SPU2 mixes at 0 dB but rarely has 40 notes up

}  // namespace

Synth::Synth(int rate) : rate_(rate), voices_(kVoices), mix_(kBlock * 2), rev_(kBlock) {
    static const int kComb[4] = {1116, 1188, 1277, 1356}, kAllpass[2] = {556, 441};
    for (int i = 0; i < 8; ++i) comb_[i].assign((kComb[i & 3] + (i >> 2) * 23) * rate / 44100, 0.f);
    for (int i = 0; i < 4; ++i) allpass_[i].assign((kAllpass[i & 1] + (i >> 1) * 23) * rate / 44100, 0.f);
}

Synth::Player* Synth::Find(int handle) {
    for (Player& p : players_)
        if (p.handle == handle) return &p;
    return nullptr;
}

const Synth::Player* Synth::Find(int handle) const {
    for (const Player& p : players_)
        if (p.handle == handle) return &p;
    return nullptr;
}

int Synth::Play(const MidiFile* seq, const SoundFont* font, uint32_t tag, float volume) {
    if (!seq || !font) return 0;
    Player p;
    p.handle = nextHandle_++;
    if (nextHandle_ <= 0) nextHandle_ = 1;
    p.tag = tag;
    p.seq = seq;
    p.font = font;
    p.volume = volume;
    p.ticksPerFrame = seq->Division() / (0.5 * rate_);   // 120 bpm until a tempo event
    players_.push_back(p);
    return p.handle;
}

void Synth::Stop(int handle, bool now) {
    Player* p = Find(handle);
    if (!p) return;
    p->stopped = true;
    for (Voice& v : voices_) {
        if (v.stage == kOff || v.player != handle) continue;
        if (now) v.stage = kOff;
        else if (v.stage != kRelease) v.stage = kRelease;
    }
}

void Synth::StopTag(uint32_t tag, bool now) {
    for (Player& p : players_)
        if (p.tag == tag) Stop(p.handle, now);
}

void Synth::StopAll(bool now) {
    for (Player& p : players_) Stop(p.handle, now);
}

void Synth::StopAllBut(int keep, bool now) {
    for (Player& p : players_)
        if (p.handle != keep) Stop(p.handle, now);
}

bool Synth::Playing(int handle) const { return Find(handle) != nullptr; }

void Synth::SetVolume(int handle, float volume) {
    Player* p = Find(handle);
    if (!p) return;
    p->volume = volume;
    for (Voice& v : voices_)
        if (v.stage != kOff && v.player == handle) UpdateVoice(v);
}

void Synth::SetTagVolume(uint32_t tag, float volume) {
    for (Player& p : players_)
        if (p.tag == tag) SetVolume(p.handle, volume);
}

int Synth::ActiveVoices() const {
    int n = 0;
    for (const Voice& v : voices_) n += v.stage != kOff;
    return n;
}

void Synth::UpdateVoice(Voice& v) {
    const Player* p = Find(v.player);
    if (!p) return;
    const Channel& c = p->ch[v.channel];
    const Zone& z = *v.zone;
    double semis = v.key - z.rootKey + c.bend * c.bendRange;
    v.step = (double)z.rate / rate_ * pow(2.0, semis / 12.0);
    float q = std::max(0.f, std::min(1.f, (c.pan >= 127 ? 1.f : c.pan / 128.f) + z.pan));
    float amp = kMaster * p->volume * v.velocity * z.gain * c.volume * c.volume * c.expression * c.expression;
    v.gainL = amp * 2 * (1 - q);
    v.gainR = amp * 2 * q;
    v.send = std::min(1.f, c.reverb + z.reverb);
}

// A free voice, else the oldest releasing one, else the oldest. Direct voices (the engines) aren't taken.
Synth::Voice* Synth::FreeVoice() {
    Voice* best = nullptr;
    for (Voice& v : voices_) {
        if (v.stage == kOff) return &v;
        if (v.direct && v.stage != kRelease) continue;
        if (!best || (v.stage == kRelease) > (best->stage == kRelease) ||
            ((v.stage == kRelease) == (best->stage == kRelease) && v.age < best->age))
            best = &v;
    }
    return best;
}

Synth::Voice* Synth::FindDirect(int handle) {
    if (!handle) return nullptr;
    for (Voice& v : voices_)
        if (v.stage != kOff && v.direct == handle) return &v;
    return nullptr;
}

int Synth::DirectOn(const SoundFont* font, int program, float pitch, float left, float right, float send) {
    if (!font) return 0;
    font->Find(program >> 7, program & 127, 60, 127, zones_);
    if (zones_.empty()) return 0;
    Voice* free = FreeVoice();
    if (!free) return 0;
    Voice& v = *free;
    v = Voice();
    v.direct = nextDirect_++;
    if (nextDirect_ <= 0) nextDirect_ = 1;
    v.own = *zones_[0];
    // ADSR1 0x13FF: linear attack at rate 0x13 (64 samples), sustain level 15. ADSR2 0x5FC5: sustain held,
    // linear release at rate 5 (64 samples); a little longer here, as the exponential release only nears silence.
    v.own.attack = 64.0f / 48000;
    v.own.hold = v.own.decay = 0;
    v.own.sustain = 1;
    v.own.release = 0.01f;
    v.zone = &v.own;
    v.age = clock_++;
    v.data = font->Samples();
    v.pos = v.own.start;
    v.stage = kAttack;
    v.env = 0;
    v.releaseMul = (float)pow(10.0, -5.0 / (v.own.release * rate_));
    v.velocity = 1;
    DirectSet(v.direct, pitch, left, right, send);
    return v.direct;
}

void Synth::DirectSet(int handle, float pitch, float left, float right, float send) {
    Voice* v = FindDirect(handle);
    if (!v) return;
    v->step = (double)v->own.rate / rate_ * pitch;
    v->gainL = kMaster * v->own.gain * left;
    v->gainR = kMaster * v->own.gain * right;
    v->send = std::min(1.f, send);
}

void Synth::DirectOff(int handle) {
    Voice* v = FindDirect(handle);
    if (v && v->stage != kRelease) v->stage = kRelease;
}

void Synth::NoteOn(Player& p, int ch, int key, int velocity) {
    if (velocity == 0) {
        NoteOff(p, ch, key);
        return;
    }
    const Channel& c = p.ch[ch];
    p.font->Find(c.bank, c.program, key, velocity, zones_);
    for (const Zone* z : zones_) {
        Voice* best = FreeVoice();
        if (!best) return;
        Voice& v = *best;
        v = Voice();
        v.player = p.handle;
        v.channel = ch;
        v.key = key;
        v.age = clock_++;
        v.zone = z;
        v.data = p.font->Samples();
        v.pos = z->start;
        v.stage = z->attack > 0 ? kAttack : (z->hold > 0 ? kHold : kDecay);
        v.env = v.stage == kAttack ? 0.f : 1.f;
        // Decay and release fall 100 dB over their time.
        v.decayMul = z->decay > 0 ? (float)pow(10.0, -5.0 / (z->decay * rate_)) : 0.f;
        v.releaseMul = z->release > 0 ? (float)pow(10.0, -5.0 / (z->release * rate_)) : 0.f;
        v.velocity = (velocity / 127.f) * (velocity / 127.f);
        UpdateVoice(v);
    }
}

void Synth::NoteOff(Player& p, int ch, int key) {
    for (Voice& v : voices_)
        if (v.stage != kOff && v.stage != kRelease && v.player == p.handle && v.channel == ch && v.key == key)
            v.stage = kRelease;
}

void Synth::Dispatch(Player& p, const MidiEvent& e) {
    if (e.status == 0xFF) {
        p.ticksPerFrame = p.seq->Division() * 1e6 / ((double)std::max<uint32_t>(e.tempo, 1) * rate_);
        return;
    }
    Channel& c = p.ch[e.channel & 63];
    bool refresh = false;
    switch (e.status) {
    case 0x80: NoteOff(p, e.channel, e.a); break;
    case 0x90: NoteOn(p, e.channel, e.a, e.b); break;
    case 0xC0: c.program = e.a; break;
    case 0xE0:
        c.bend = (((e.b << 7) | e.a) - 8192) / 8192.f;
        refresh = true;
        break;
    case 0xB0:
        switch (e.a) {
        case 0: c.bank = e.b; break;
        case 7: c.volume = e.b / 127.f; refresh = true; break;
        case 10: c.pan = e.b; refresh = true; break;
        case 11: c.expression = e.b / 127.f; refresh = true; break;
        case 91: c.reverb = e.b / 127.f; refresh = true; break;
        case 101: c.rpn = (c.rpn & 0x7F) | (e.b << 7); break;
        case 100: c.rpn = (c.rpn & 0x3F80) | e.b; break;
        case 6: if (c.rpn == 0) c.bendRange = e.b; break;
        case 120: case 123:   // all sound off, all notes off
            for (Voice& v : voices_)
                if (v.stage != kOff && v.player == p.handle && v.channel == e.channel)
                    v.stage = e.a == 120 ? kOff : kRelease;
            break;
        case 121: {
            int range = c.bendRange;
            c.Reset();
            c.bendRange = range;
            refresh = true;
            break;
        }
        }
        break;
    }
    if (refresh)
        for (Voice& v : voices_)
            if (v.stage != kOff && v.player == p.handle && v.channel == e.channel) UpdateVoice(v);
}

// Plays the player's events up to its position after `frames` more frames, looping at the loop end.
void Synth::Advance(Player& p, int frames) {
    if (p.stopped) return;
    const std::vector<MidiEvent>& ev = p.seq->Events();
    p.tick += frames * p.ticksPerFrame;
    for (;;) {
        bool loops = p.seq->Loops();
        double limit = loops ? std::min<double>(p.tick, p.seq->LoopEnd()) : p.tick;
        while (p.next < ev.size() && ev[p.next].tick <= limit && (!loops || ev[p.next].tick < p.seq->LoopEnd()))
            Dispatch(p, ev[p.next++]);
        if (loops && p.tick >= p.seq->LoopEnd()) {
            p.tick -= p.seq->LoopEnd() - p.seq->LoopStart();
            p.next = std::lower_bound(ev.begin(), ev.end(), p.seq->LoopStart(),
                                      [](const MidiEvent& e, uint32_t t) { return e.tick < t; }) - ev.begin();
            continue;
        }
        break;
    }
    if (!p.seq->Loops() && p.next >= ev.size() && p.tick >= p.seq->Length()) p.stopped = true;
}

void Synth::MixVoice(Voice& v, float* mix, float* rev, int frames) {
    const Zone& z = *v.zone;
    for (int i = 0; i < frames; ++i) {
        // Envelope
        switch (v.stage) {
        case kAttack:
            v.env += 1.f / (z.attack * rate_);
            if (v.env >= 1) {
                v.env = 1;
                v.stage = z.hold > 0 ? kHold : kDecay;
                v.stageTime = 0;
            }
            break;
        case kHold:
            v.stageTime += 1.f / rate_;
            if (v.stageTime >= z.hold) v.stage = kDecay;
            break;
        case kDecay:
            v.env *= v.decayMul;
            if (v.env <= z.sustain) {
                v.env = z.sustain;
                v.stage = kSustain;
            }
            break;
        case kRelease:
            v.env *= v.releaseMul;
            break;
        default: break;
        }
        if (v.env <= kSilent && (v.stage == kRelease || v.stage == kSustain)) {
            v.stage = kOff;
            return;
        }
        // Sample, linearly interpolated
        uint32_t ip = (uint32_t)v.pos;
        float frac = (float)(v.pos - ip);
        uint32_t ip1 = ip + 1;
        if (z.loop && ip1 >= z.loopEnd) ip1 -= z.loopEnd - z.loopStart;
        float s0 = v.data[ip], s1 = ip1 < z.end ? v.data[ip1] : 0.f;
        float s = (s0 + (s1 - s0) * frac) * (1.f / 32768.f) * v.env;
        mix[i * 2] += s * v.gainL;
        mix[i * 2 + 1] += s * v.gainR;
        rev[i] += s * (v.gainL + v.gainR) * 0.5f * v.send;
        v.pos += v.step;
        if (z.loop) {
            while (v.pos >= z.loopEnd) v.pos -= z.loopEnd - z.loopStart;
        } else if (v.pos >= z.end) {
            v.stage = kOff;
            return;
        }
    }
}

void Synth::Reverb(const float* in, float* mix, int frames) {
    const float kFeedback = 0.82f, kDamp = 0.25f, kWet = 0.35f;
    for (int i = 0; i < frames; ++i) {
        float x = in[i] * 0.015f;
        for (int side = 0; side < 2; ++side) {
            float out = 0;
            for (int k = side * 4; k < side * 4 + 4; ++k) {
                std::vector<float>& buf = comb_[k];
                float y = buf[combPos_[k]];
                combStore_[k] = y * (1 - kDamp) + combStore_[k] * kDamp;
                buf[combPos_[k]] = x + combStore_[k] * kFeedback;
                if (++combPos_[k] >= (int)buf.size()) combPos_[k] = 0;
                out += y;
            }
            for (int k = side * 2; k < side * 2 + 2; ++k) {
                std::vector<float>& buf = allpass_[k];
                float b = buf[allpassPos_[k]];
                buf[allpassPos_[k]] = out + b * 0.5f;
                out = b - out;
                if (++allpassPos_[k] >= (int)buf.size()) allpassPos_[k] = 0;
            }
            mix[i * 2 + side] += out * kWet;
        }
    }
}

void Synth::Render(int16_t* out, int frames) {
    while (frames > 0) {
        int n = std::min(frames, kBlock);
        for (Player& p : players_) Advance(p, n);
        std::fill(mix_.begin(), mix_.begin() + n * 2, 0.f);
        std::fill(rev_.begin(), rev_.begin() + n, 0.f);
        for (Voice& v : voices_)
            if (v.stage != kOff) MixVoice(v, mix_.data(), rev_.data(), n);
        Reverb(rev_.data(), mix_.data(), n);
        if (stream_) stream_->Mix(mix_.data(), n, rate_);
        if (mono_)
            for (int i = 0; i < n * 2; i += 2) mix_[i] = mix_[i + 1] = (mix_[i] + mix_[i + 1]) * 0.5f;
        for (int i = 0; i < n * 2; ++i) {
            float s = mix_[i] * master_ * 32767.f;
            out[i] = (int16_t)std::max(-32768.f, std::min(32767.f, s));
        }
        out += n * 2;
        frames -= n;
        // Drop players that are done: stopped, with no voices left.
        players_.erase(std::remove_if(players_.begin(), players_.end(),
                                      [&](const Player& p) {
                                          if (!p.stopped) return false;
                                          for (const Voice& v : voices_)
                                              if (v.stage != kOff && v.player == p.handle) return false;
                                          return true;
                                      }),
                       players_.end());
    }
}

}  // namespace audio
