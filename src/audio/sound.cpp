#include "sound.h"

#include <windows.h>
#include <stdio.h>
#include <string.h>

#include <algorithm>
#include <map>
#include <memory>

#include "audio_out.h"
#include "midi_file.h"
#include "radio.h"
#include "soundfont.h"
#include "synth.h"

namespace audio {

namespace {

const int kRate = 48000;
// SNDMOD's thread runs once a vblank (thread_sddr); procFade steps the song's fade then.
const int kTick = kRate / 60;

// The game's sound file table (D_0029ADF0), by index. Only the sequence files matter here; the VAGs are the
// streamed radio stations and the TVBs come with the sequences (see the manifests).
const char* const kFiles[] = {
    "CQ_MAIN.TVB", "CQ_MAIN.TSQ", "ACTION.TVB", "ACTION.TSQ", "1CH_L.VAG", "1CH_R.VAG", "2CH_L.VAG", "2CH_R.VAG",
    "3CH_L.VAG", "3CH_R.VAG", "BGM.TVB", "ROOM_1.TSQ", "BGM_01.TSQ", "BGM_02.TSQ", "BGM_03.TSQ", "BGM_04.TSQ",
    "BGM_05.TSQ", "BGM_06.TSQ", "BGM_07.TSQ", "BGM_08.TSQ", "BGM_09.TSQ", "BGM_10.TSQ", "BGM_11.TSQ", "BGM_12.TSQ",
    "REPLAY.TSQ", "SELECT.TSQ", "ENDING.TSQ", "DEMO.TSQ", "FANFARE.TSQ"};

struct Entry {
    bool music = false;
    std::string file;
    std::unique_ptr<MidiFile> midi;   // loaded on first use
};

// A sequence file: its bank and table, from <name>.txt.
struct SeqFile {
    const SoundFont* bank = nullptr;
    std::map<int, Entry> entries;
};

// An engine's state (SNDMOD's sddrwork +0x22C, 0x24 bytes a player). The requests only change it; procEngine acts
// on it at the next vblank.
struct Engine {
    int left = 0x3FFF, right = 0x3FFF;   // the voices' levels (SPU2 volume, 0x3FFF = full)
    int state = 1;        // 1 to stop, 2 playing, 0 stopped (procEngine has keyed it off)
    int muted = 1;        // 2: stopped by Sound_EngineMute, for Sound_EngineLoud
    int type = 0, select = 0, rpm = 0, reverb = 0;
    int keyed = -1;       // the select last keyed on, -1 none (only Sound_EngineStop clears it)
    int voice = 0;        // its direct voice
};

// wave.168: each type's two samples (CQ_MAIN waves) for select 0 and 1.
const int kEngineWaves[8][2] = {{2, 1}, {0x28, 0x27}, {0x2A, 0x29}, {2, 1}, {2, 1}, {2, 1}, {2, 1}, {2, 1}};
// data.169: {rpm, pitch at that rpm, pitch at the next row's rpm}; the SPU2 pitch, 0x1000 = the sample's rate.
const int kEnginePitch[5][3] = {{0, 2000, 3000}, {2000, 3000, 9500}, {4000, 10000, 16000}, {8000, 16000, 16383},
                                {10000, 0, 0}};

// The radio (sddrwork +0x274). Sound_Init's Sound_RadioFile (0x002575E0) calls give the stations their files: 0 1CH, 1 3CH, 2 2CH.
const char* const kRadioFiles[3] = {"1CH", "3CH", "2CH"};
// A chunk (procRadio): 1800 frames of the clock, 0x32392 bytes of PS-ADPCM a side; 120 make the day. The frames of
// the day: the 120th chunk's start, (120 x 0x32392 + 0x40 & ~0xF) - 0x40 bytes at 28 samples a 16 bytes.
const int kRadioChunkFrames = 1800, kRadioChunkBytes = 0x32392, kRadioChunks = 0x78;
const int64_t kRadioDay = (((int64_t)kRadioChunks * kRadioChunkBytes + 0x40) & ~0xF) / 16 * 28 - 0x40 / 16 * 28;
const int kRadioRate = 12000;   // the VAGs' rate, which the chunks' frames count at

// The stream's frame for `clock`: load_radio_part reads the chunk from (chunk x 0x32392 + 0x40) & ~0xF, and procRadio
// keys its voice on (clock % 1800) x 0x72 bytes in (& ~0xF: a whole block).
int64_t RadioFrame(int clock) {
    int chunk = (clock / kRadioChunkFrames) % kRadioChunks, in = clock % kRadioChunkFrames;
    int64_t byte = (((int64_t)chunk * kRadioChunkBytes + 0x40) & ~0xF) + ((in * 0x72) & ~0xF);
    return (byte - 0x40) / 16 * 28;
}

struct Radio {
    int left = 0x3FFF, right = 0x3FFF;   // sddr_RadioInit's levels
    int station = 0, clock = 0;
    bool playing = false;
};

struct State {
    CRITICAL_SECTION lock;
    std::unique_ptr<AudioOut> out;
    Synth synth{kRate};
    std::string dir;
    std::map<std::string, std::unique_ptr<SoundFont>> banks;
    std::map<std::string, std::unique_ptr<SeqFile>> files;
    SeqFile* slots[16] = {};
    int song = -1;          // selected song: (slot << 8) | entry
    int songHandle = 0;
    int speaker = 0;
    Engine engines[2];
    Radio radio;
    RadioStream stations[3];   // opened on first tune
    int fade = 0;           // SNDMOD's fade_flag: the song's volume (0x100 = full) while it fades out, 0 none
    int fadeIn = -1;        // the volume while it fades back in (song command 6), -1 none
    int tick = 0;           // frames rendered into the current vblank
};

State* g_sound = nullptr;

struct Lock {
    Lock() { EnterCriticalSection(&g_sound->lock); }
    ~Lock() { LeaveCriticalSection(&g_sound->lock); }
};

// procFade, each vblank: a fading song loses 4/256 of its volume (fade_data), silent after 64 vblanks. It plays on
// silently.
void Sound_ProcFade() {
    State& s = *g_sound;
    if (s.fadeIn >= 0) {
        // Command 6's fade in: 4/256 more a vblank, full after 64.
        s.fadeIn += 4;
        if (s.fadeIn >= 0x100) s.fadeIn = -1;
        s.synth.SetVolume(s.songHandle, s.fadeIn < 0 ? 1.0f : s.fadeIn / 256.0f);
        return;
    }
    if (s.fade <= 0) return;
    s.fade -= 4;
    if (s.fade < 1) s.fade = 0;
    s.synth.SetVolume(s.songHandle, s.fade / 256.0f);
}

// data.169's pitch for `rpm`, capped at 10000.
int EnginePitch(int rpm) {
    rpm = std::min(rpm, kEnginePitch[4][0]);
    int i = 0;
    while (kEnginePitch[i + 1][0] < rpm) ++i;
    const int* r = kEnginePitch[i];
    return (rpm - r[0]) * (r[2] - r[1]) / (kEnginePitch[i + 1][0] - r[0]) + r[1];
}

// procEngine (SNDMOD, every vblank, before procFade), for one engine: a new select keys the type's sample for it on
// and the other off, otherwise only the pitch and the reverb change. Only the select is compared: a new type with the
// same select keeps the sample that sounds. So the type the first key-on takes is the one set by then: Car_Init
// starts the engine as type 0 and its Car_Sound makes it type 2 before the vblank, so the race keys type 2's.
void Sound_ProcEngine(Engine& e) {
    Synth& s = g_sound->synth;
    if (e.state == 1) {
        e.state = 0;
        s.DirectOff(e.voice);
        e.voice = 0;
        return;
    }
    if (e.state != 2) return;
    int pitch = EnginePitch(e.rpm);
    float send = e.reverb ? 1.f : 0.f, left = e.left / 16383.f, right = e.right / 16383.f;
    if (e.keyed != e.select || !e.voice) {
        s.DirectOff(e.voice);
        const SoundFont* bank = g_sound->slots[0] ? g_sound->slots[0]->bank : nullptr;
        e.voice = s.DirectOn(bank, kEngineWaves[e.type & 7][e.select & 1], pitch / 4096.f, left, right, send);
        e.keyed = e.select;
    } else {
        s.DirectSet(e.voice, pitch / 4096.f, left, right, send);
    }
}

void Fill(void*, int16_t* out, int frames) {
    // The lock is taken per 64 frames, not for the whole buffer: the game's requests (the engines' every frame)
    // wait for one short piece of synthesis at most, not a whole buffer's (a frame's time on a slow CPU).
    const int kPiece = 64;
    State& s = *g_sound;
    while (frames > 0) {
        Lock l;
        int n = std::min(std::min(frames, kTick - s.tick), kPiece);
        s.synth.Render(out, n);
        out += n * 2;
        frames -= n;
        if ((s.tick += n) == kTick) {
            s.tick = 0;
            for (Engine& e : s.engines) Sound_ProcEngine(e);
            Sound_ProcFade();
        }
    }
}

const SoundFont* Bank(const std::string& name) {
    auto it = g_sound->banks.find(name);
    if (it != g_sound->banks.end()) return it->second.get();
    std::unique_ptr<SoundFont> sf(new SoundFont);
    if (!sf->Load(g_sound->dir + "\\" + name)) {
        OutputDebugStringA(("sound: " + sf->Error() + "\n").c_str());
        sf.reset();
    }
    return (g_sound->banks[name] = std::move(sf)).get();
}

// Reads <name>.txt: "bank FILE.sf2", then "ENTRY KIND PRIORITY FILE ..." per sequence.
SeqFile* LoadSeqFile(const std::string& name) {
    auto it = g_sound->files.find(name);
    if (it != g_sound->files.end()) return it->second.get();
    std::unique_ptr<SeqFile> sf;
    FILE* fp = fopen((g_sound->dir + "\\" + name + ".txt").c_str(), "r");
    if (fp) {
        sf.reset(new SeqFile);
        char line[512];
        while (fgets(line, sizeof line, fp)) {
            char a[260], b[260], c[260], d[260];
            if (line[0] == '#') continue;
            int n = sscanf(line, "%259s %259s %259s %259s", a, b, c, d);
            if (n >= 2 && !strcmp(a, "bank")) {
                sf->bank = Bank(b);
            } else if (n >= 4) {
                Entry& e = sf->entries[(int)strtol(a, nullptr, 16)];
                e.music = !strcmp(b, "music");
                e.file = d;
            }
        }
        fclose(fp);
    } else {
        OutputDebugStringA(("sound: no manifest for " + name + "\n").c_str());
    }
    return (g_sound->files[name] = std::move(sf)).get();
}

Entry* Find(int id, const SoundFont** bank) {
    SeqFile* f = g_sound->slots[(id >> 8) & 15];
    if (!f || !f->bank) return nullptr;
    auto it = f->entries.find(id & 0xFF);
    if (it == f->entries.end()) return nullptr;
    Entry& e = it->second;
    if (!e.midi) {
        e.midi.reset(new MidiFile);
        if (!e.midi->Load(g_sound->dir + "\\" + e.file))
            OutputDebugStringA(("sound: " + e.midi->Error() + "\n").c_str());
    }
    if (e.midi->Events().empty()) return nullptr;
    *bank = f->bank;
    return &e;
}

void Sound_EngineStopVoice(Engine& e) {
    g_sound->synth.DirectOff(e.voice);
    e.voice = 0;
    e.keyed = -1;
}

Engine* GetEngine(int player) { return g_sound && player >= 0 && player < 2 ? &g_sound->engines[player] : nullptr; }

}  // namespace

bool Sound_Init(const std::string& dataDir, bool openDevice) {
    if (g_sound) return true;
    g_sound = new State;
    InitializeCriticalSection(&g_sound->lock);
    g_sound->dir = dataDir + "\\sound";
    g_sound->slots[0] = LoadSeqFile("CQ_MAIN");
    g_sound->slots[1] = LoadSeqFile("ROOM_1");
    g_sound->slots[3] = LoadSeqFile("ACTION");
    Sound_EngineInit();
    if (!openDevice) return true;
    g_sound->out.reset(CreateXAudio2Out());
    if (!g_sound->out->Start(kRate, Fill, nullptr)) {
        OutputDebugStringA("sound: no XAudio2 2.7 device (is the DirectX June 2010 runtime installed?)\n");
        Sound_Shutdown();
        return false;
    }
    return true;
}

void Sound_Render(int16_t* out, int frames) {
    if (g_sound && !g_sound->out) Fill(nullptr, out, frames);
}

void Sound_Shutdown() {
    if (!g_sound) return;
    if (g_sound->out) g_sound->out->Stop();
    g_sound->out.reset();
    g_sound->synth.SetStream(nullptr);
    DeleteCriticalSection(&g_sound->lock);
    delete g_sound;
    g_sound = nullptr;
}

void Sound_Request(uint32_t cmd) {
    if (!g_sound) return;
    Lock l;
    int id = cmd & 0xFFF;
    if (cmd & 0x8000) {
        g_sound->synth.StopTag(id);
        return;
    }
    const SoundFont* bank;
    Entry* e = Find(id, &bank);
    if (!e) return;
    if (e->music) {   // the driver queues a song entry; Sound_StartSong plays it
        g_sound->synth.Stop(g_sound->songHandle);
        g_sound->song = id;
        g_sound->fade = 0;
        return;
    }
    // The request's pan (cmd >> 16: right level in the low byte, left in the high) is the channel's starting pan;
    // the converted MIDI assumes the centre (0x40, 0x40), which is all the game asks for (Sound_PlaySe; the only
    // other sender, func_00258E70, has no callers).
    g_sound->synth.Play(e->midi.get(), bank, id);
}

void Sound_PlaySe(uint32_t id) { Sound_Request(id | 0x40400000); }

void Sound_LoadSong(uint32_t arg) {
    if (!g_sound) return;
    Sound_StopSong();
    int file = (arg >> 16) & 0xFF, slot = (arg >> 8) & 0xFF;
    Lock l;
    if (file < (int)(sizeof kFiles / sizeof kFiles[0]) && strstr(kFiles[file], ".TSQ")) {
        std::string name(kFiles[file], strlen(kFiles[file]) - 4);
        g_sound->slots[slot & 15] = LoadSeqFile(name);
    }
    g_sound->song = arg & 0xFFF;
    g_sound->fade = 0;
    g_sound->fadeIn = -1;
}

void Sound_StartSong() {
    if (!g_sound) return;
    Lock l;
    const SoundFont* bank;
    Entry* e = g_sound->song >= 0 ? Find(g_sound->song, &bank) : nullptr;
    if (!e || !e->music) return;
    g_sound->synth.Stop(g_sound->songHandle, true);
    g_sound->songHandle = g_sound->synth.Play(e->midi.get(), bank, g_sound->song);
}

void Sound_StopSong() {
    if (!g_sound) return;
    Lock l;
    g_sound->synth.Stop(g_sound->songHandle);
    g_sound->songHandle = 0;
}

// sddrSongData's 3 and 4 set the song channels' volume (set_volume_mode 0 / 0x100); the song keeps playing. A new
// song starts at full volume again (reqmus).
void Sound_MuteSong() {
    if (!g_sound) return;
    Lock l;
    g_sound->synth.SetVolume(g_sound->songHandle, 0);
}

void Sound_UnmuteSong() {
    if (!g_sound) return;
    Lock l;
    g_sound->synth.SetVolume(g_sound->songHandle, 1);
}

// sddrSongData's 5: the song fades out (fade_flag 0x100, fade_data 4), in procFade.
void Sound_FadeOutSong() {
    if (!g_sound) return;
    Lock l;
    g_sound->fadeIn = -1;
    g_sound->fade = 0x100;
}

// func_00258FD0, sddrSongData's 6: the song's volume from 0 back up, 4/256 a vblank (64 vblanks), after a fanfare
// muted it (Sound_MuteSong).
void Sound_FadeInSong() {
    if (!g_sound) return;
    Lock l;
    g_sound->fade = 0;
    g_sound->fadeIn = 0;
    g_sound->synth.SetVolume(g_sound->songHandle, 0);
}

// sddrStopMode -> set_cancel_mode(1): every sound effect's sequence ends; the song plays on.
void Sound_StopEffects() {
    if (!g_sound) return;
    Lock l;
    g_sound->synth.StopAllBut(g_sound->songHandle);
}

void Sound_SetVolume(uint32_t id, int volume) {
    if (!g_sound) return;
    Lock l;
    g_sound->synth.SetTagVolume(id & 0xFFF, volume / 256.f);
}

void Sound_EngineInit() {
    if (!g_sound) return;
    {
        Lock l;
        for (Engine& e : g_sound->engines) {
            Sound_EngineStopVoice(e);
            e = Engine();
        }
    }
    Sound_EngineVolume(0, 0x1CCC, 0x1CCC);
    Sound_EngineVolume(1, 0x1CCC, 0x1CCC);
}

void Sound_EngineVolume(int player, int left, int right) {
    Engine* e = GetEngine(player);
    if (!e) return;
    Lock l;
    e->left = left;
    e->right = right;
    // vol_Engine sets the voices' levels at once.
    if (e->voice)
        g_sound->synth.DirectSet(e->voice, EnginePitch(e->rpm) / 4096.f, left / 16383.f, right / 16383.f,
                                 e->reverb ? 1.f : 0.f);
}

void Sound_EnginePlay(int player) {
    Engine* e = GetEngine(player);
    if (!e) return;
    {
        Lock l;
        e->state = 2;
        e->muted = 1;
    }
    Sound_EngineRpms(player, 0, 1, 0, 0);
}

void Sound_EngineRpms(int player, int type, int select, int rpm, int reverb) {
    Engine* e = GetEngine(player);
    if (!e) return;
    Lock l;
    e->type = type;
    e->select = rpm == 0 ? 1 : select;   // (func_002586A0)
    e->rpm = rpm;
    e->reverb = reverb;
}

void Sound_EngineStop(int player) {
    Engine* e = GetEngine(player);
    if (!e) return;
    Lock l;
    e->state = 1;
    e->keyed = -1;
}

void Sound_EngineMute(int player) {
    Engine* e = GetEngine(player);
    if (!e || e->state != 2) return;
    {
        Lock l;
        e->muted = 2;
    }
    Sound_EngineStop(player);
}

void Sound_EngineLoud(int player) {
    Engine* e = GetEngine(player);
    if (!e || e->muted != 2) return;
    {
        Lock l;
        e->state = 2;   // (sddr_EnginePlay)
        e->muted = 1;
    }
}

// The driver keeps both settings on the IOP, so they last until the game resets.
void Sound_RadioVolume(int left, int right) {
    if (!g_sound) return;
    Lock l;
    Radio& r = g_sound->radio;
    r.left = left;
    r.right = right;
    // vol_Radio: the left file's voices on the left, the right's on the right (mono averages them, as Synth does).
    for (RadioStream& s : g_sound->stations) s.SetLevels(left / 16383.f, right / 16383.f);
}

void Sound_RadioTune(int station, int clock) {
    if (!g_sound || station < 0 || station > 2) return;
    RadioStream& s = g_sound->stations[station];
    // Opening reads only the header (and a radio file's seek points) and maps nothing yet; the audio thread doesn't
    // use a closed stream. A WAV of the station, if there is one, replaces the radio file (a mod).
    if (!s.IsOpen()) {
        Lock l;
        std::string base = g_sound->dir + "\\radio\\" + kRadioFiles[station];
        if (!s.Open(base + ".wav") && !s.Open(base + ".adp"))
            OutputDebugStringA(("sound: no radio station " + base + ".adp\n").c_str());
        s.SetLevels(g_sound->radio.left / 16383.f, g_sound->radio.right / 16383.f);
    }
    Lock l;
    g_sound->radio.station = station;
    g_sound->radio.clock = clock;
    // sddr_RadioTune clears the chunk playing (+0x2A0 = -1): a playing station moves to the new place.
    if (g_sound->radio.playing) Sound_RadioPlay();
}

void Sound_RadioPlay() {
    if (!g_sound) return;
    Lock l;
    Radio& r = g_sound->radio;
    for (RadioStream& s : g_sound->stations) s.Stop();
    RadioStream& s = g_sound->stations[r.station];
    s.Start(RadioFrame(r.clock), kRadioDay, kRadioRate);
    s.Prefetch();
    g_sound->synth.SetStream(&s);
    r.playing = true;
}

void Sound_RadioStop() {
    if (!g_sound) return;
    Lock l;
    for (RadioStream& s : g_sound->stations) s.Stop();
    g_sound->synth.SetStream(nullptr);
    g_sound->radio.playing = false;
}

void Sound_SetMasterVolume(int left, int right) {
    if (!g_sound) return;
    Lock l;
    g_sound->synth.SetMasterVolume((left + right) / (2.f * 0x3FFF));
}

void Sound_SetSpeaker(int mode) {
    if (!g_sound) return;
    Lock l;
    g_sound->speaker = mode;
    g_sound->synth.SetMono(mode != 0);
}

int Sound_GetSpeaker() { return g_sound ? g_sound->speaker : 0; }

}  // namespace audio
