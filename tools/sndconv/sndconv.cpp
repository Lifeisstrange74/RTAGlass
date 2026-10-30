// sndconv: converts the game's sound banks and sequences to standard PC formats for the data set (data/sound):
//   SOUND/<BANK>.TVB -> sound/<BANK>.sf2        SoundFont 2: one preset per program, 16-bit PCM samples with their
//                                                loops, and the SPU2 ADSR envelopes converted to SF2 envelopes.
//   SOUND/<SEQ>.TSQ  -> sound/<SEQ>/<nn>.mid    one Standard MIDI File per table entry (nn = entry, hex),
//                       sound/<SEQ>.txt         the manifest: which bank, and each entry's kind and priority.
//   SOUND/<n>CH_L/R.VAG -> sound/radio/<n>CH.adp  a radio station: both sides' PS-ADPCM blocks as they are,
//                                                interleaved, with seek points (src/audio/radio.h).
//
// The formats and the driver behaviour come from SNDMOD.IRX, the game's IOP sound driver, which still has its symbol
// table (docs/sound.md). In short:
//   TVB: u32 wave offset[256] (relative to 0x800), u32 ADSR[256] (stored inverted), then PS-ADPCM data at 0x800.
//   TSQ: a table of {u16 priority, u16 offset}, priority 0 = music. A sound effect is one channel of bytecode; a
//        song is a header of 36 channels {u8 priority, u8, u16 offset}. The bytecode runs once per vblank (60 Hz):
//        bytes < 0x80 wait that many ticks, 0x80-0xDF key a note on, 0xE0-0xF9 are commands, anything else ends.
//
// MIDI conventions (src/audio/synth.h reads them back the same way):
//   - 60 ticks per quarter note at 60 bpm, so one MIDI tick is one TSQ tick (1/60 s).
//   - One track per TSQ channel (spawned channels included), each on its own port/channel (15 per port, channel 10
//     is skipped). Key = TSQ note + 12, so note 48 (pitch 0x1000, the sample's own rate) is key 60, the root key.
//   - Exact pitches (detune, slides) are pitch bends with a +-24 semitone range, set by RPN 0 on every track.
//   - Volume: CC7 = 127*sqrt(vol/255). Pan: the SPU's separate left/right levels L, R (64 = full) become CC10 =
//     128*R/(L+R) and CC11 = 127*sqrt((L+R)/128); the engine pans linearly, 0 dB in the centre.
//   - Reverb on/off is CC91 127/0. Songs that loop have "loopStart" and "loopEnd" markers on track 0.
//
// Usage: sndconv GAMEDIR DATADIR [--force] [--wav DIR]   (e.g. sndconv ..\OriginalGameFiles data)
//   Existing .sf2/.mid/.adp files are kept unless --force is given, so edited files survive a reconversion.
//   --wav DIR also writes every sample as a WAV file (with its loop), and each radio station as radio_<n>CH.wav, to
//   look at or listen to. (tools/adpconv encodes a WAV to a radio file, e.g. a replacement station, and back.)
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>

#include <algorithm>
#include <map>
#include <string>
#include <vector>

#include "../../src/audio/adpcm.h"
#include "../../src/audio/wav_file.h"

namespace {

bool g_force = false;

bool ReadFile(const std::string& path, std::vector<uint8_t>& out) {
    FILE* fp = fopen(path.c_str(), "rb");
    if (!fp) return false;
    fseek(fp, 0, SEEK_END);
    long n = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    out.resize(n > 0 ? (size_t)n : 0);
    bool ok = fread(out.data(), 1, out.size(), fp) == out.size();
    fclose(fp);
    return ok;
}

bool Exists(const std::string& path) { return GetFileAttributesA(path.c_str()) != INVALID_FILE_ATTRIBUTES; }

// Writes a file unless it exists and --force is off. Returns false on a write error.
bool WriteOut(const std::string& path, const std::vector<uint8_t>& data, int& kept) {
    if (!g_force && Exists(path)) {
        ++kept;
        return true;
    }
    FILE* fp = fopen(path.c_str(), "wb");
    if (!fp) return false;
    bool ok = fwrite(data.data(), 1, data.size(), fp) == data.size();
    fclose(fp);
    return ok;
}

uint16_t Rd16(const uint8_t* p) { return (uint16_t)(p[0] | (p[1] << 8)); }
uint32_t Rd32(const uint8_t* p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24); }

// Little-endian byte writer for RIFF and MIDI chunks.
struct Bytes {
    std::vector<uint8_t> b;
    void U8(int v) { b.push_back((uint8_t)v); }
    void U16(int v) { U8(v); U8(v >> 8); }
    void U32(uint32_t v) { U16((int)(v & 0xFFFF)); U16((int)(v >> 16)); }
    void Str(const char* s, size_t n) {   // fixed-size, zero-padded
        for (size_t i = 0; i < n; ++i) U8(i < strlen(s) ? s[i] : 0);
    }
    void Tag(const char* t) { b.insert(b.end(), t, t + 4); }
    void Append(const std::vector<uint8_t>& o) { b.insert(b.end(), o.begin(), o.end()); }
};

// ---------------------------------------------------------------------------------------------------------------
// TVB: PS-ADPCM samples and SPU2 envelopes

struct Wave {
    std::vector<int16_t> pcm;
    int loopStart = -1;   // >= 0: the sample loops from here to its end
};

// Decodes one SPU2 ADPCM sample starting at `off`: 16-byte blocks of 28 samples. Flag bit 2 marks the loop start,
// bit 0 the end; at the end, bit 1 set means jump back to the loop start, clear means the voice stops.
void DecodeAdpcm(const uint8_t* d, size_t size, size_t off, Wave& w) {
    static const int kFilter[5][2] = {{0, 0}, {60, 0}, {115, -52}, {98, -55}, {122, -60}};
    int s1 = 0, s2 = 0, loop = -1;
    for (size_t pos = off; pos + 16 <= size; pos += 16) {
        int shift = d[pos] & 15, filter = std::min(d[pos] >> 4, 4), flags = d[pos + 1];
        if (flags & 4) loop = (int)w.pcm.size();
        for (int i = 0; i < 28; ++i) {
            int n = (d[pos + 2 + i / 2] >> ((i & 1) * 4)) & 15;
            if (n >= 8) n -= 16;
            int s = ((n << 12) >> shift) + ((s1 * kFilter[filter][0] + s2 * kFilter[filter][1] + 32) >> 6);
            s = std::max(-32768, std::min(32767, s));
            w.pcm.push_back((int16_t)s);
            s2 = s1;
            s1 = s;
        }
        if (flags & 1) {
            w.loopStart = (flags & 2) ? std::max(loop, 0) : -1;
            return;
        }
    }
}

// Runs one SPU2 envelope phase (rules from psx-spx) from `level` until it reaches `target`. Returns the time in
// seconds at 48 kHz, or -1 if it gets stuck or takes over a minute.
double EnvPhase(int& level, int shift, int step, bool exp, bool dec, int target) {
    long long samples = 0;
    while (dec ? level > target : level < target) {
        long long cycles = 1LL << std::max(0, shift - 11);
        int s = step << std::max(0, 11 - shift);
        if (exp && !dec && level > 0x6000) cycles *= 4;
        if (exp && dec) s = s * level / 0x8000;
        if (s == 0) return -1;
        samples += cycles;
        if (samples > 48000LL * 60) return -1;
        level = std::max(0, std::min(0x7FFF, level + s));
    }
    return samples / 48000.0;
}

int Timecents(double sec) {
    if (sec <= 0.001) return -12000;
    return std::max(-12000, std::min(8000, (int)lround(1200.0 * log2(sec))));
}

struct Sf2Env {
    int attack, decay, sustain, release;   // timecents, timecents, centibels of attenuation, timecents
};

// Converts the SPU2 ADSR registers to an SF2 volume envelope. SF2 decay and release fall at a fixed dB rate, timed
// for 100 dB; the SPU2 decays exponentially (also a fixed dB rate) but may keep falling during sustain, in which
// case the SF2 decay is timed so both reach -60 dB together and the sustain level is silence.
Sf2Env ConvertAdsr(uint16_t a1, uint16_t a2) {
    const int kQuiet = 33;   // -60 dB
    Sf2Env e;
    int level = 0;
    double t = EnvPhase(level, (a1 >> 10) & 0x1F, 7 - ((a1 >> 8) & 3), (a1 & 0x8000) != 0, false, 0x7FFF);
    e.attack = Timecents(t < 0 ? 60 : t);

    int sl = std::min(0x7FFF, ((a1 & 15) + 1) * 0x800);
    level = 0x7FFF;
    double td = EnvPhase(level, (a1 >> 4) & 15, -8, true, true, sl);
    if (td < 0) td = 60;
    bool sdec = (a2 & 0x4000) != 0;
    int sstep = sdec ? -8 + ((a2 >> 6) & 3) : 7 - ((a2 >> 6) & 3);
    double ts = sdec ? EnvPhase(level, (a2 >> 8) & 0x1F, sstep, (a2 & 0x8000) != 0, true, kQuiet) : -1;
    if (ts >= 0) {
        e.decay = Timecents((td + ts) * 100.0 / 60.0);
        e.sustain = 1000;
    } else {
        double drop = 20.0 * log10(32767.0 / std::max(level, 1));
        e.sustain = (int)lround(drop * 10.0);
        e.decay = Timecents(drop > 0.1 ? td * 100.0 / drop : 0.0);
    }

    level = 0x7FFF;
    double tr = EnvPhase(level, a2 & 0x1F, -8, (a2 & 0x20) != 0, true, kQuiet);
    e.release = Timecents((tr < 0 ? 60 : tr) * 100.0 / 60.0);
    return e;
}

std::vector<uint8_t> WavFile(const Wave& w) {
    Bytes f, smpl;
    smpl.Tag("smpl");
    smpl.U32(36 + (w.loopStart >= 0 ? 24 : 0));
    smpl.U32(0); smpl.U32(0); smpl.U32(1000000000 / 48000); smpl.U32(60); smpl.U32(0); smpl.U32(0); smpl.U32(0);
    smpl.U32(w.loopStart >= 0 ? 1 : 0);
    smpl.U32(0);
    if (w.loopStart >= 0) {
        smpl.U32(0); smpl.U32(0); smpl.U32(w.loopStart); smpl.U32((uint32_t)w.pcm.size() - 1); smpl.U32(0);
        smpl.U32(0);
    }
    uint32_t data = (uint32_t)w.pcm.size() * 2;
    f.Tag("RIFF");
    f.U32(4 + 24 + 8 + data + (uint32_t)smpl.b.size());
    f.Tag("WAVE");
    f.Tag("fmt ");
    f.U32(16); f.U16(1); f.U16(1); f.U32(48000); f.U32(96000); f.U16(2); f.U16(16);
    f.Tag("data");
    f.U32(data);
    for (int16_t s : w.pcm) f.U16(s);
    f.Append(smpl.b);
    return f.b;
}

// SF2 generator numbers.
enum {
    kGenPan = 17, kGenReverb = 16, kGenAttack = 34, kGenDecay = 36, kGenSustain = 37, kGenRelease = 38,
    kGenInstrument = 41, kGenSampleId = 53, kGenSampleModes = 54
};

Bytes Chunk(const char* tag, const Bytes& body) {
    Bytes c;
    c.Tag(tag);
    c.U32((uint32_t)body.b.size());
    c.Append(body.b);
    if (body.b.size() & 1) c.U8(0);
    return c;
}

Bytes List(const char* type, const std::vector<Bytes>& chunks) {
    Bytes body;
    body.Tag(type);
    for (const Bytes& c : chunks) body.Append(c.b);
    Bytes l;
    l.Tag("LIST");
    l.U32((uint32_t)body.b.size());
    l.Append(body.b);
    return l;
}

Bytes ZStr(const char* s) {
    Bytes b;
    b.b.assign(s, s + strlen(s) + 1);
    if (b.b.size() & 1) b.U8(0);
    return b;
}

// Converts one TVB to an SF2. Programs whose wave is empty or out of range are left out. Returns the program count.
int ConvertTvb(const std::string& path, const std::string& name, const std::string& out, const std::string& wavDir,
               int& kept) {
    std::vector<uint8_t> d;
    if (!ReadFile(path, d) || d.size() < 0x800) {
        printf("  %s: can't read\n", path.c_str());
        return -1;
    }
    const uint8_t* data = d.data() + 0x800;
    size_t size = d.size() - 0x800;

    // Decode each distinct wave once.
    std::map<uint32_t, int> sampleOf;   // wave offset -> sample index
    std::vector<Wave> samples;
    struct Prog { int program, sample; Sf2Env env; };
    std::vector<Prog> progs;
    for (int p = 0; p < 256; ++p) {
        uint32_t off = Rd32(&d[p * 4]);
        if (off >= size) continue;
        auto it = sampleOf.find(off);
        int si;
        if (it == sampleOf.end()) {
            Wave w;
            DecodeAdpcm(data, size, off, w);
            if (w.pcm.empty()) continue;
            si = (int)samples.size();
            samples.push_back(std::move(w));
            sampleOf[off] = si;
        } else {
            si = it->second;
        }
        uint32_t v = Rd32(&d[0x400 + p * 4]);
        uint32_t adsr = (~v & 0xDFFFFFF0u) | (v & 15);   // stored inverted, as load_tvbf undoes it
        progs.push_back({p, si, ConvertAdsr((uint16_t)adsr, (uint16_t)(adsr >> 16))});
    }

    if (!wavDir.empty()) {
        CreateDirectoryA(wavDir.c_str(), nullptr);
        for (const Prog& p : progs) {
            char fn[MAX_PATH];
            snprintf(fn, sizeof fn, "%s\\%s_%03d.wav", wavDir.c_str(), name.c_str(), p.program);
            int k = 0;
            bool force = g_force;
            g_force = true;
            WriteOut(fn, WavFile(samples[p.sample]), k);
            g_force = force;
        }
    }

    // sdta: every sample followed by 46 zero samples, as SF2 requires.
    Bytes smpl;
    std::vector<uint32_t> start(samples.size());
    for (size_t i = 0; i < samples.size(); ++i) {
        start[i] = (uint32_t)(smpl.b.size() / 2);
        for (int16_t s : samples[i].pcm) smpl.U16(s);
        for (int z = 0; z < 46; ++z) smpl.U16(0);
    }

    Bytes phdr, pbag, pmod, pgen, inst, ibag, imod, igen, shdr;
    char nm[32];
    for (size_t i = 0; i < progs.size(); ++i) {
        const Prog& p = progs[i];
        snprintf(nm, sizeof nm, "Tone %03d", p.program);
        phdr.Str(nm, 20);
        phdr.U16(p.program & 127);
        phdr.U16(p.program >> 7);
        phdr.U16((int)i);
        phdr.U32(0); phdr.U32(0); phdr.U32(0);
        pbag.U16((int)i);
        pbag.U16(0);
        pgen.U16(kGenInstrument);
        pgen.U16((int)i);

        inst.Str(nm, 20);
        inst.U16((int)i);
        ibag.U16((int)i * 6);
        ibag.U16(0);
        const Wave& w = samples[p.sample];
        igen.U16(kGenAttack); igen.U16(p.env.attack);
        igen.U16(kGenDecay); igen.U16(p.env.decay);
        igen.U16(kGenSustain); igen.U16(p.env.sustain);
        igen.U16(kGenRelease); igen.U16(p.env.release);
        igen.U16(kGenSampleModes); igen.U16(w.loopStart >= 0 ? 1 : 0);
        igen.U16(kGenSampleId); igen.U16(p.sample);
    }
    phdr.Str("EOP", 20); phdr.U16(0); phdr.U16(0); phdr.U16((int)progs.size()); phdr.U32(0); phdr.U32(0); phdr.U32(0);
    pbag.U16((int)progs.size()); pbag.U16(0);
    pmod.Str("", 10);
    pgen.U32(0);
    inst.Str("EOI", 20); inst.U16((int)progs.size());
    ibag.U16((int)progs.size() * 6); ibag.U16(0);
    imod.Str("", 10);
    igen.U32(0);
    for (size_t i = 0; i < samples.size(); ++i) {
        const Wave& w = samples[i];
        snprintf(nm, sizeof nm, "%s %03d", name.c_str(), (int)i);
        shdr.Str(nm, 20);
        uint32_t s = start[i], e = s + (uint32_t)w.pcm.size();
        shdr.U32(s);
        shdr.U32(e);
        shdr.U32(w.loopStart >= 0 ? s + w.loopStart : s);
        shdr.U32(w.loopStart >= 0 ? e : e);
        shdr.U32(48000);
        shdr.U8(60);
        shdr.U8(0);
        shdr.U16(0);
        shdr.U16(1);
    }
    shdr.Str("EOS", 20);
    for (int i = 0; i < 26; ++i) shdr.U8(0);

    Bytes ver;
    ver.U16(2);
    ver.U16(1);
    std::string title = name + " (Road Trip sound bank)";
    Bytes info = List("INFO", {Chunk("ifil", ver), Chunk("isng", ZStr("EMU8000")), Chunk("INAM", ZStr(title.c_str())),
                               Chunk("ISFT", ZStr("RTAGlass sndconv"))});
    Bytes sdta = List("sdta", {Chunk("smpl", smpl)});
    Bytes pdta = List("pdta", {Chunk("phdr", phdr), Chunk("pbag", pbag), Chunk("pmod", pmod), Chunk("pgen", pgen),
                               Chunk("inst", inst), Chunk("ibag", ibag), Chunk("imod", imod), Chunk("igen", igen),
                               Chunk("shdr", shdr)});
    Bytes body, file;
    body.Tag("sfbk");
    body.Append(info.b);
    body.Append(sdta.b);
    body.Append(pdta.b);
    file.Tag("RIFF");
    file.U32((uint32_t)body.b.size());
    file.Append(body.b);
    if (!WriteOut(out, file.b, kept)) {
        printf("  %s: can't write\n", out.c_str());
        return -1;
    }
    return (int)progs.size();
}

// ---------------------------------------------------------------------------------------------------------------
// The radio stations: VAG streams to the game's radio files (src/audio/adpcm.h has the layout)

// A VAG stream's ("VAGp", a 0x40-byte header, then PS-ADPCM blocks) blocks and rate.
bool ReadVag(const std::string& path, std::vector<uint8_t>& blocks, int& rate) {
    std::vector<uint8_t> d;
    if (!ReadFile(path, d) || d.size() < 0x40 || memcmp(d.data(), "VAGp", 4)) return false;
    rate = (d[0x10] << 24) | (d[0x11] << 16) | (d[0x12] << 8) | d[0x13];   // big-endian
    blocks.assign(d.begin() + 0x40, d.begin() + 0x40 + (d.size() - 0x40) / 16 * 16);
    return true;
}

// One station: <NAME>_L.VAG and <NAME>_R.VAG as the game's radio file, their blocks as they are (no re-encoding).
// With wavDir, also the station as a stereo WAV there. Returns false on an error; `seconds` is 0 when an existing
// file was kept.
bool ConvertRadio(const std::string& in, const std::string& name, const std::string& out, const std::string& wavDir,
                  int& kept, int& seconds) {
    seconds = 0;
    if (!g_force && Exists(out) && wavDir.empty()) {
        ++kept;
        return true;
    }
    std::vector<std::vector<uint8_t>> sides(2);
    int rate = 0, rate2 = 0;
    if (!ReadVag(in + "\\" + name + "_L.VAG", sides[0], rate) ||
        !ReadVag(in + "\\" + name + "_R.VAG", sides[1], rate2) || rate != rate2 || rate <= 0)
        return false;
    size_t blocks = std::min(sides[0].size(), sides[1].size()) / 16;
    for (std::vector<uint8_t>& s : sides) s.resize(blocks * 16);
    std::vector<uint8_t> file = audio::Radio_BuildFile(sides, rate);
    if (!wavDir.empty()) {
        std::vector<int16_t> pcm;
        int channels = 0, r = 0;
        CreateDirectoryA(wavDir.c_str(), nullptr);
        int k = 0;
        bool force = g_force;
        g_force = true;
        bool ok = audio::Radio_Decode(file, pcm, channels, r) &&
                  WriteOut(wavDir + "\radio_" + name + ".wav", audio::Wav_Build(pcm, channels, r), k);
        g_force = force;
        if (!ok) return false;
    }
    if (!g_force && Exists(out)) {
        ++kept;
        return true;
    }
    int k = 0;
    bool force = g_force;
    g_force = true;
    bool ok = WriteOut(out, file, k);
    g_force = force;
    if (ok) seconds = (int)(blocks * 28 / rate);
    return ok;
}

// ---------------------------------------------------------------------------------------------------------------
// TSQ: sequences to MIDI

// SNDMOD's freqtbl: SPU2 pitch per note, equal-tempered, 0x1000 at note 48. Notes past 72 read whatever follows the
// table in the driver; those values are kept so such notes sound (or don't) as they did.
const uint16_t kFreq[96] = {
    0x0100, 0x010F, 0x011F, 0x0130, 0x0142, 0x0155, 0x016A, 0x017F, 0x0196, 0x01AE, 0x01C8, 0x01E3,
    0x0200, 0x021E, 0x023E, 0x0260, 0x0285, 0x02AB, 0x02D4, 0x02FF, 0x032C, 0x035D, 0x0390, 0x03C6,
    0x0400, 0x043C, 0x047D, 0x04C1, 0x050A, 0x0556, 0x05A8, 0x05FE, 0x0659, 0x06BA, 0x0720, 0x078D,
    0x0800, 0x0879, 0x08FA, 0x0983, 0x0A14, 0x0AAD, 0x0B50, 0x0BFC, 0x0CB2, 0x0D74, 0x0E41, 0x0F1A,
    0x1000, 0x10F3, 0x11F5, 0x1306, 0x1428, 0x155B, 0x16A0, 0x17F9, 0x1965, 0x1AE8, 0x1C82, 0x1E34,
    0x2000, 0x21E7, 0x23EB, 0x260D, 0x2851, 0x2AB7, 0x2D41, 0x2FF2, 0x32CB, 0x35D1, 0x3904, 0x3C68,
    0x3FFF, 0x0000, 0x00FF, 0x0000, 0x0001, 0x0800, 0x0F00, 0x0000, 0x0001, 0x0980, 0x3FFF, 0x0000,
    0x0001, 0x0A80, 0x3FFF, 0x0000, 0x0001, 0x0B80, 0x7FFF, 0x0000, 0x0001, 0x0C80, 0x7FFF, 0x0000};

const int kBendRange = 24;
const uint32_t kNever = 0xFFFFFFFFu;

int ArgCount(int op) {
    switch (op) {
    case 0xE0: case 0xE2: case 0xE6: case 0xE7: case 0xF1: return 1;
    case 0xE1: case 0xE3: case 0xE4: case 0xE5: case 0xEA: case 0xF8: case 0xF9: return 2;
    case 0xE8: case 0xE9: case 0xF0: return 0;
    default: return -1;   // data_end
    }
}

// What a channel's future depends on; two equal snapshots at the same position mean the channel has looped.
struct ChanState {
    int vol = 200, panL = 64, panR = 64, tone = 0, cent = 0, freq = 0, key = -1;
    bool rev = false;
    bool operator==(const ChanState& o) const {
        return vol == o.vol && panL == o.panL && panR == o.panR && tone == o.tone && cent == o.cent &&
               freq == o.freq && key == o.key && rev == o.rev;
    }
};

struct MidiEvent {
    uint32_t tick;
    int seq;
    std::vector<uint8_t> bytes;
};

struct Chan {
    size_t pc;
    uint32_t time;
    ChanState st;
    bool done = false;
    int track;
    // Loop detection (pass 1)
    std::map<size_t, std::pair<uint32_t, ChanState>> visited;
    uint32_t loopStart = kNever, loopEnd = kNever, endTime = 0;
    // MIDI output (pass 2)
    int bend = 8192, cc7 = -1, cc10 = -1, cc11 = -1, cc91 = -1, prog = -1;
};

struct Song {
    const std::vector<uint8_t>* d;
    std::vector<Chan> chans;
    std::vector<std::vector<MidiEvent>> tracks;
    int seq = 0;
    bool emit = false;
    uint32_t horizon = kNever;
    int clamps = 0, bad = 0;
};

int MidiChannel(int track) { int c = track % 15; return c >= 9 ? c + 1 : c; }

void Emit(Song& s, Chan& c, std::initializer_list<int> bytes) {
    if (!s.emit) return;
    MidiEvent e{c.time, s.seq++, {}};
    for (int b : bytes) e.bytes.push_back((uint8_t)b);
    s.tracks[c.track].push_back(std::move(e));
}

void EmitCc(Song& s, Chan& c, int cc, int v, int& cache) {
    v = std::max(0, std::min(127, v));
    if (v == cache) return;
    cache = v;
    Emit(s, c, {0xB0 | MidiChannel(c.track), cc, v});
}

void EmitLevels(Song& s, Chan& c) {
    const ChanState& st = c.st;
    EmitCc(s, c, 7, (int)lround(127.0 * sqrt(st.vol / 255.0)), c.cc7);
    int sum = st.panL + st.panR;
    EmitCc(s, c, 10, sum ? (int)lround(128.0 * st.panR / sum) : 64, c.cc10);
    EmitCc(s, c, 11, (int)lround(127.0 * sqrt(std::min(1.0, sum / 128.0))), c.cc11);
}

double Semitones(int freq) { return 60.0 + 12.0 * log2(std::max(freq, 1) / 4096.0); }

void EmitBend(Song& s, Chan& c, double semis) {
    if (semis > kBendRange || semis < -kBendRange) ++s.clamps;
    int raw = std::max(0, std::min(16383, 8192 + (int)lround(semis / kBendRange * 8192.0)));
    if (raw == c.bend) return;
    c.bend = raw;
    Emit(s, c, {0xE0 | MidiChannel(c.track), raw & 127, raw >> 7});
}

void NoteOff(Song& s, Chan& c) {
    if (c.st.key < 0) return;
    Emit(s, c, {0x80 | MidiChannel(c.track), c.st.key, 0});
    c.st.key = -1;
}

int WithCent(int freq, int cent) { return std::min(0x3FFF, cent ? (int)(((uint32_t)freq * (uint32_t)cent) >> 12) : freq); }

void KeyOn(Song& s, Chan& c, int freq) {
    NoteOff(s, c);
    c.st.freq = freq;
    if (freq == 0) return;   // a zero pitch never advances the sample: silence
    EmitLevels(s, c);
    EmitCc(s, c, 91, c.st.rev ? 127 : 0, c.cc91);
    if (c.prog != (c.st.tone & 127)) {
        c.prog = c.st.tone & 127;
        Emit(s, c, {0xC0 | MidiChannel(c.track), c.prog});
    }
    double semis = Semitones(freq);
    int key = std::max(0, std::min(127, (int)lround(semis)));
    EmitBend(s, c, semis - key);
    c.st.key = key;
    Emit(s, c, {0x90 | MidiChannel(c.track), key, 127});
}

int NewChan(Song& s, size_t pc, uint32_t time, const ChanState* from) {
    Chan c;
    c.pc = pc;
    c.time = time;
    if (from) c.st = *from;
    c.st.key = -1;
    c.track = (int)s.chans.size();
    s.chans.push_back(c);
    if (s.emit) {
        s.tracks.resize(s.chans.size());
        Chan& n = s.chans.back();
        int port = n.track / 15, ch = MidiChannel(n.track);
        uint32_t t = n.time;
        n.time = 0;
        Emit(s, n, {0xFF, 0x21, port});   // MIDI port prefix (Track() writes the length)
        n.time = t;
        // RPN 0: pitch bend range.
        Emit(s, n, {0xB0 | ch, 101, 0});
        Emit(s, n, {0xB0 | ch, 100, 0});
        Emit(s, n, {0xB0 | ch, 6, kBendRange});
        Emit(s, n, {0xB0 | ch, 38, 0});
        Emit(s, n, {0xB0 | ch, 101, 127});
        Emit(s, n, {0xB0 | ch, 100, 127});
    }
    return s.chans.back().track;
}

// Runs channel i at its current time until it waits or ends.
void Step(Song& s, int i) {
    const std::vector<uint8_t>& d = *s.d;
    for (;;) {
        Chan& c = s.chans[i];
        if (c.pc >= d.size()) {
            ++s.bad;
            c.done = true;
            return;
        }
        if (!s.emit) {
            auto v = c.visited.find(c.pc);
            if (v == c.visited.end()) c.visited[c.pc] = {c.time, c.st};
        }
        size_t at = c.pc;
        int op = d[c.pc++];
        if (op < 0x80) {
            c.time += op ? op : 0x10000;   // a wait of 0 underflows the driver's counter: 65536 ticks
            return;
        }
        if (op < 0xE0) {
            KeyOn(s, c, WithCent(kFreq[op & 0x7F], c.st.cent));
            continue;
        }
        int n = ArgCount(op);
        if (n < 0 || c.pc + n > d.size()) {   // data_end
            NoteOff(s, c);
            c.done = true;
            c.endTime = c.time;
            return;
        }
        const uint8_t* a = &d[c.pc];
        c.pc += n;
        int16_t arg = n == 2 ? (int16_t)Rd16(a) : 0;
        switch (op) {
        case 0xE0: c.st.vol = a[0]; if (c.st.key >= 0) EmitLevels(s, c); break;
        case 0xE1: c.st.panL = a[0]; c.st.panR = a[1]; if (c.st.key >= 0) EmitLevels(s, c); break;
        case 0xE2: c.st.tone = (int8_t)a[0] & 0xFF; c.st.cent = 0; break;
        case 0xE4:
            c.st.freq = WithCent((uint16_t)arg, c.st.cent);
            if (c.st.key >= 0) EmitBend(s, c, Semitones(c.st.freq) - c.st.key);
            break;
        case 0xE5: KeyOn(s, c, WithCent((uint16_t)arg, c.st.cent)); break;
        case 0xE8: c.st.rev = true; if (c.st.key >= 0) EmitCc(s, c, 91, 127, c.cc91); break;
        case 0xE9: c.st.rev = false; if (c.st.key >= 0) EmitCc(s, c, 91, 0, c.cc91); break;
        case 0xEA: c.st.cent = (uint16_t)arg; break;
        case 0xF0: NoteOff(s, c); break;
        case 0xF8: {
            size_t target = c.pc + arg;
            if (!s.emit && target <= at) {
                auto v = c.visited.find(target);
                if (v != c.visited.end()) {
                    if (v->second.second == c.st) {   // same position, same state: a loop
                        c.loopStart = v->second.first;
                        c.loopEnd = c.time;
                        c.done = true;
                        return;
                    }
                    c.visited.clear();   // the first pass through set something up; compare the next one
                }
            }
            c.pc = target;
            break;
        }
        case 0xF9: {
            ChanState st = c.st;
            size_t pc = c.pc + arg;
            uint32_t t = c.time;
            NewChan(s, pc, t, &st);   // invalidates c
            break;
        }
        default: break;   // tempo, reverb mode/depth, priority: no effect on the MIDI
        }
    }
}

// Runs every channel in time order (ties in creation order) up to the horizon.
void Run(Song& s) {
    for (;;) {
        int best = -1;
        for (int i = 0; i < (int)s.chans.size(); ++i) {
            const Chan& c = s.chans[i];
            if (c.done || c.time >= s.horizon) continue;
            if (best < 0 || c.time < s.chans[best].time) best = i;
        }
        if (best < 0) return;
        Step(s, best);
    }
}

struct Converted {
    bool ok = false;
    uint32_t loopStart = kNever, loopEnd = kNever, length = 0;
    int tracks = 0, clamps = 0;
    std::vector<uint8_t> midi;
};

void Vlq(Bytes& b, uint32_t v) {
    uint8_t tmp[5];
    int n = 0;
    tmp[n++] = v & 127;
    while (v >>= 7) tmp[n++] = (uint8_t)(0x80 | (v & 127));
    while (n) b.U8(tmp[--n]);
}

Bytes Track(std::vector<MidiEvent>& ev, uint32_t endTick) {
    std::stable_sort(ev.begin(), ev.end(), [](const MidiEvent& a, const MidiEvent& b) {
        return a.tick != b.tick ? a.tick < b.tick : a.seq < b.seq;
    });
    Bytes body;
    uint32_t t = 0;
    for (const MidiEvent& e : ev) {
        Vlq(body, e.tick - t);
        t = e.tick;
        if (e.bytes[0] == 0xFF) {   // meta: FF type len data
            body.U8(0xFF);
            body.U8(e.bytes[1]);
            Vlq(body, (uint32_t)e.bytes.size() - 2);
            body.b.insert(body.b.end(), e.bytes.begin() + 2, e.bytes.end());
        } else {
            body.b.insert(body.b.end(), e.bytes.begin(), e.bytes.end());
        }
    }
    Vlq(body, std::max(endTick, t) - t);
    body.U8(0xFF); body.U8(0x2F); body.U8(0);
    Bytes chunk;
    chunk.b = {'M', 'T', 'r', 'k'};
    uint32_t n = (uint32_t)body.b.size();
    chunk.U8(n >> 24); chunk.U8(n >> 16); chunk.U8(n >> 8); chunk.U8(n);
    chunk.Append(body.b);
    return chunk;
}

MidiEvent Meta(uint32_t tick, int type, const std::string& text) {
    MidiEvent e{tick, 0, {0xFF, (uint8_t)type}};
    e.bytes.insert(e.bytes.end(), text.begin(), text.end());
    return e;
}

// Converts one sequence: a sound effect (one channel at `off`) or a song (36 channel header at `off`).
Converted ConvertSequence(const std::vector<uint8_t>& d, size_t off, bool music, const std::string& title) {
    Converted r;
    auto start = [&](Song& s) {
        if (music) {
            for (int ch = 0; ch < 36; ++ch) {
                uint16_t rel = Rd16(&d[off + ch * 4 + 2]);
                NewChan(s, off + rel, 0, nullptr);
            }
        } else {
            NewChan(s, off, 0, nullptr);
        }
    };

    // Pass 1: find each channel's loop or end.
    Song p1;
    p1.d = &d;
    start(p1);
    p1.horizon = 1u << 22;
    Run(p1);
    if (p1.bad) return r;
    uint32_t period = 0, loopStart = 0, end = 0;
    bool loops = false, mismatch = false;
    for (const Chan& c : p1.chans) {
        if (c.loopStart != kNever) {
            uint32_t p = c.loopEnd - c.loopStart;
            if (loops && p != period) mismatch = true;
            period = std::max(period, p);
            loopStart = std::max(loopStart, c.loopStart);
            loops = true;
        } else {
            end = std::max(end, c.endTime);
        }
    }
    if (mismatch) printf("    %s: channels loop with different periods; using the longest\n", title.c_str());

    // Pass 2: emit up to one steady period past the latest loop start.
    Song s;
    s.d = &d;
    s.emit = true;
    s.horizon = loops ? loopStart + period : kNever;
    start(s);
    Run(s);
    uint32_t length = loops ? loopStart + period : end;

    // Keep only tracks that play something.
    std::vector<std::vector<MidiEvent>> used;
    for (auto& t : s.tracks) {
        bool notes = false;
        for (const MidiEvent& e : t) notes |= (e.bytes[0] & 0xF0) == 0x90;
        if (notes) used.push_back(std::move(t));
    }
    if (used.empty()) return r;
    // Renumber channels and ports in the order kept.
    for (size_t i = 0; i < used.size(); ++i) {
        for (MidiEvent& e : used[i]) {
            if (e.bytes[0] == 0xFF && e.bytes[1] == 0x21) e.bytes[2] = (uint8_t)(i / 15);
            else if (e.bytes[0] < 0xF0) e.bytes[0] = (uint8_t)((e.bytes[0] & 0xF0) | MidiChannel((int)i));
        }
    }

    std::vector<MidiEvent> conductor;
    conductor.push_back(Meta(0, 0x03, title));
    conductor.push_back(MidiEvent{0, 1, {0xFF, 0x51, 0x0F, 0x42, 0x40}});   // 1,000,000 us per quarter
    if (loops) {
        conductor.push_back(Meta(loopStart, 0x06, "loopStart"));
        conductor.push_back(Meta(loopStart + period, 0x06, "loopEnd"));
    }
    for (size_t i = 0; i < conductor.size(); ++i) conductor[i].seq = (int)i;

    Bytes f;
    f.b = {'M', 'T', 'h', 'd', 0, 0, 0, 6, 0, 1};
    int nt = (int)used.size() + 1;
    f.U8(nt >> 8); f.U8(nt);
    f.U8(0); f.U8(60);   // 60 ticks per quarter
    f.Append(Track(conductor, length).b);
    for (auto& t : used) f.Append(Track(t, length).b);

    r.ok = true;
    r.loopStart = loops ? loopStart : kNever;
    r.loopEnd = loops ? loopStart + period : kNever;
    r.length = length;
    r.tracks = (int)used.size();
    r.clamps = s.clamps;
    r.midi = std::move(f.b);
    return r;
}

// Which bank each sequence file plays with: SNDMOD's bank slot k uses TVB slot k. The game loads CQ_MAIN into slot
// 0, ACTION into slot 3, and BGM.TVB into slots 1 and 2, which hold ROOM_1 and the current song.
std::string BankFor(const std::string& seq) {
    if (seq == "CQ_MAIN" || seq == "ACTION") return seq;
    return "BGM";
}

int ConvertTsq(const std::string& path, const std::string& name, const std::string& soundDir, int& kept,
               int& files) {
    std::vector<uint8_t> d;
    if (!ReadFile(path, d) || d.size() < 4) {
        printf("  %s: can't read\n", path.c_str());
        return -1;
    }
    std::string dir = soundDir + "\\" + name;
    CreateDirectoryA(dir.c_str(), nullptr);

    std::string manifest = "# " + name + ".TSQ, converted by sndconv. See data/README.md.\n";
    manifest += "bank " + BankFor(name) + ".sf2\n";
    manifest += "# entry kind priority file [loop START END] (ticks, 60 a second)\n";

    // The table runs up to the first sequence.
    size_t first = d.size();
    int converted = 0;
    for (size_t i = 0; i * 4 + 4 <= first; ++i) {
        uint16_t pri = Rd16(&d[i * 4]), off = Rd16(&d[i * 4 + 2]);
        if (off == 0) continue;
        first = std::min(first, (size_t)off);
        if (off + 2u > d.size() || (d[off] == 0xF0 && d[off + 1] == 0xFF)) continue;   // the empty stub
        bool music = pri == 0;
        if (music && off + 36u * 4 > d.size()) continue;
        char title[64], entry[16];
        snprintf(title, sizeof title, "%s %02X", name.c_str(), (unsigned)i);
        snprintf(entry, sizeof entry, "%02X.mid", (unsigned)i);
        std::string file = name + "/" + entry;
        Converted c = ConvertSequence(d, off, music, title);
        if (!c.ok) continue;
        if (!WriteOut(dir + "\\" + entry, c.midi, kept)) {
            printf("  %s: can't write\n", file.c_str());
            return -1;
        }
        ++files;
        char line[160];
        if (c.loopStart != kNever)
            snprintf(line, sizeof line, "%02X %s %d %s loop %u %u\n", (unsigned)i, music ? "music" : "se", pri,
                     file.c_str(), c.loopStart, c.loopEnd);
        else
            snprintf(line, sizeof line, "%02X %s %d %s\n", (unsigned)i, music ? "music" : "se", pri, file.c_str());
        manifest += line;
        if (c.clamps) printf("    %s: %d pitch bends past +-%d semitones were clamped\n", title, c.clamps, kBendRange);
        ++converted;
    }
    std::vector<uint8_t> m(manifest.begin(), manifest.end());
    bool force = g_force;
    g_force = true;   // the manifest always reflects this run
    int k = 0;
    bool ok = WriteOut(soundDir + "\\" + name + ".txt", m, k);
    g_force = force;
    return ok ? converted : -1;
}

}  // namespace

int main(int argc, char** argv) {
    std::vector<std::string> pos;
    std::string wavDir;
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--force")) g_force = true;
        else if (!strcmp(argv[i], "--wav") && i + 1 < argc) wavDir = argv[++i];
        else pos.push_back(argv[i]);
    }
    if (pos.size() != 2) {
        printf("usage: sndconv GAMEDIR DATADIR [--force] [--wav DIR]\n");
        return 1;
    }
    std::string in = pos[0] + "\\SOUND", soundDir = pos[1] + "\\sound";
    CreateDirectoryA(pos[1].c_str(), nullptr);
    CreateDirectoryA(soundDir.c_str(), nullptr);

    int kept = 0, banks = 0, seqs = 0, files = 0, errors = 0;
    WIN32_FIND_DATAA fd;
    for (const char* ext : {"TVB", "TSQ"}) {
        HANDLE h = FindFirstFileA((in + "\\*." + ext).c_str(), &fd);
        if (h == INVALID_HANDLE_VALUE) continue;
        std::vector<std::string> names;
        do names.push_back(fd.cFileName); while (FindNextFileA(h, &fd));
        FindClose(h);
        std::sort(names.begin(), names.end());
        for (const std::string& fn : names) {
            std::string base = fn.substr(0, fn.size() - 4);
            if (!strcmp(ext, "TVB")) {
                int n = ConvertTvb(in + "\\" + fn, base, soundDir + "\\" + base + ".sf2", wavDir, kept);
                if (n < 0) ++errors;
                else { printf("%-12s %3d programs -> sound\\%s.sf2\n", fn.c_str(), n, base.c_str()); ++banks; }
            } else {
                int n = ConvertTsq(in + "\\" + fn, base, soundDir, kept, files);
                if (n < 0) ++errors;
                else { printf("%-12s %3d sequences -> sound\\%s\\\n", fn.c_str(), n, base.c_str()); ++seqs; }
            }
        }
    }
    // The radio stations the game tunes to (Sound_Init's Sound_RadioFile (0x002575E0) calls: station 0 is 1CH, Peach FM; station 1
    // is 3CH, E-Radio; station 2 is 2CH, Q-Wave, which the PS2's radio has no button for: the Q-Wave extra adds one).
    std::string radioDir = soundDir + "\\radio";
    CreateDirectoryA(radioDir.c_str(), nullptr);
    for (const char* station : {"1CH", "3CH", "2CH"}) {
        int seconds = 0;
        if (!ConvertRadio(in, station, radioDir + "\\" + station + ".adp", wavDir, kept, seconds)) {
            printf("%s_L/R.VAG: failed\n", station);
            ++errors;
        } else if (seconds) {
            printf("%s_L/R.VAG  %d:%02d -> sound\\radio\\%s.adp\n", station, seconds / 60, seconds % 60, station);
        }
    }
    printf("%d banks, %d sequence files (%d MIDI files), %d existing files kept%s, %d errors\n", banks, seqs, files,
           kept, kept && !g_force ? " (--force rewrites them)" : "", errors);
    return errors ? 1 : 0;
}
