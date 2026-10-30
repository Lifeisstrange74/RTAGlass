// The game's sound interface: a high-level reimplementation of the EE side of the sound driver (sound/driver,
// 0x00256AF8-0x00259100), which sent requests over SIF RPC to SNDMOD.IRX on the IOP. Here the requests play the
// converted data in data/sound (SoundFonts and MIDI files, made by tools/sndconv) through audio::Synth.
//
// Sounds are addressed as the game does: (slot << 8) | entry, where a slot holds one sequence file (TSQ) and the
// entry is a row of its table. The game loads CQ_MAIN into slot 0, ROOM_1 into slot 1 and ACTION into slot 3 at
// boot; slot 2 holds the current song file. See docs/sound.md.
//
// Everything works without sound: if the device or the data is missing, requests do nothing.
#pragma once

#include <stdint.h>

#include <string>

namespace audio {

// Sound_Init (0x00256AF8): opens the device and loads the boot-time slots from dataDir/sound. Without a device
// (openDevice false, for tests) the mix is pulled with Sound_Render. Returns false, with sound off, on failure.
bool Sound_Init(const std::string& dataDir, bool openDevice = true);
void Sound_Shutdown();
// Mixes 48 kHz 16-bit stereo frames; only for Sound_Init(..., false).
void Sound_Render(int16_t* out, int frames);

// Sound_Request (0x00258308): `cmd & 0xFFF` is the sound. Bit 15 stops it instead; otherwise a sound effect
// starts, or a song entry becomes the song to start (Sound_StartSong).
void Sound_Request(uint32_t cmd);
// Sound_PlaySe (0x00258E50): Sound_Request with the default (centre) pan.
void Sound_PlaySe(uint32_t id);
// Sound_LoadSong (0x00258EC8): `arg` = (file << 16) | (slot << 8) | entry. Stops the song, loads file number `file`
// of the game's sound file table into the slot and selects the entry as the song.
void Sound_LoadSong(uint32_t arg);
// Sound_StartSong (0x00258F30) and Sound_StopSong (0x00258F50).
void Sound_StartSong();
void Sound_StopSong();
// Sound_MuteSong (func_00258F70) and Sound_UnmuteSong (func_00258F90): RPC 0xE commands 3 and 4, the playing song's
// volume to 0 and back; it plays on silently in between (the race's pause).
void Sound_MuteSong();
void Sound_UnmuteSong();
// func_00258FB0, RPC 0xE command 5: the song fades out over 64 vblanks (about a second) and plays on silently; the
// next song starts at full volume.
void Sound_FadeOutSong();
// func_00258FD0 (song command 6): the song comes back in from silence over 64 vblanks.
void Sound_FadeInSong();
// Sound_StopEffects (func_00257FD8(1), RPC 0xF): every sound effect playing stops; the song doesn't.
void Sound_StopEffects();

// Sound_SetVolume (func_00257C68, RPC 9 / sddrCtrlData 0): the volume of a playing sound, 0x100 = as written
// (SNDMOD's set_volume: the voices' levels times volume / 256). The skid loops follow the squeal with it.
void Sound_SetVolume(uint32_t id, int volume);

// The engines: SNDMOD's sddr_Engine* (RPCs 0x20-0x26), one per player (0, 1). An engine is two looped samples, one
// for the throttle and one off it, of the pair its type picks (wave.168: type 2 is the race's CQ_MAIN waves 0x2A
// and 0x29). The rpm (0-10000) sets the pitch through a table of straight segments (data.169). See sound.md.
// Sound_EngineInit (func_00258578, at boot; Sound_Init runs it): RPC 0x20 resets both engines, then both get the
// level 0x1CCC (a little under half).
void Sound_EngineInit();
// Sound_EngineVolume (func_002585F0, RPC 0x21): an engine's left and right levels, 0-0x3FFF.
void Sound_EngineVolume(int player, int left, int right);
// Sound_EnginePlay (func_002574A0, RPCs 0x23 and 0x22): starts the engine at rest (type 0, rpm 0).
void Sound_EnginePlay(int player);
// Sound_EngineRpms (func_002586A0, RPC 0x22): `select` 0 plays the type's first sample, 1 its second (rpm 0
// always takes the second); `reverb` sends the engine to the reverb.
void Sound_EngineRpms(int player, int type, int select, int rpm, int reverb);
// Sound_EngineStop (func_00258768, RPC 0x24).
void Sound_EngineStop(int player);
// Sound_EngineMute and Sound_EngineLoud (func_00258810 and func_002588B8, RPCs 0x25 and 0x26): stop a playing
// engine, then start it again (the pause menu).
void Sound_EngineMute(int player);
void Sound_EngineLoud(int player);

// The car radio: SNDMOD's sddr_Radio* (RPCs 0x30-0x37). A station is a stereo stream an in-game day long (216000
// frames, an hour), which plays from where the clock is when it's tuned: its 120 chunks of 1800 frames are the day's
// half minutes (procRadio). Station 0 is Peach FM (1CH), 1 E-Radio (3CH), 2 Q-Wave (2CH, not on the radio's
// buttons and not converted). See sound.md.
// Sound_RadioVolume (0x002589A0, RPC 0x31): the levels, left and right, 0-0x3FFF.
void Sound_RadioVolume(int left, int right);
// Sound_RadioTune (0x00258A48, RPC 0x33): the station and the clock (info+0x14) to play it from.
void Sound_RadioTune(int station, int clock);
// Sound_RadioPlay (0x00258AF0, RPC 0x34) and Sound_RadioStop (0x00258B30, RPC 0x35). Playing again after a stop
// starts from the tuned clock again.
void Sound_RadioPlay();
void Sound_RadioStop();

// Sound_SetMasterVolume (0x002583F0): the SPU2 master volume, left and right, 0-0x3FFF.
void Sound_SetMasterVolume(int left, int right);
// Sound_SetSpeaker (0x00257B00) and Sound_GetSpeaker (0x00257BA8): 0 stereo, 1 mono.
void Sound_SetSpeaker(int mode);
int Sound_GetSpeaker();

}  // namespace audio
