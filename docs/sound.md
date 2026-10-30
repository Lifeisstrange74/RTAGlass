# Sound: the driver, its formats and the PC version

Road Trip's sound runs on the IOP in `SNDMOD.IRX`, a sequencer written for this game. The module still has its
symbol table and stabs, so every function and struct has its original name. The EE side (`sound/driver`) only
sends it RPC requests. The PC port replaces both with standard files and a software synthesizer.

## Files (game/SOUND)

| File | What it is |
|---|---|
| `CQ_MAIN.TVB/.TSQ` | Menu and system sounds. Bank slot 0. |
| `ACTION.TVB/.TSQ` | Race and event sounds, and the TAKARA jingle. Bank slot 3. |
| `BGM.TVB` | Instruments for every song. Loaded into slots 1 and 2. |
| `ROOM_1.TSQ` | Garage songs (slot 1). |
| `BGM_01-12, DEMO, ENDING, FANFARE, REPLAY, SELECT .TSQ` | One song each, loaded into slot 2 when needed. |
| `1CH-3CH_L/R.VAG` | The car radio stations: streamed stereo ADPCM (about 24 MB per side). 1CH and 3CH are `data/sound/radio/<n>CH.adp` (the same blocks, interleaved), streamed (audio/radio.h); 2CH (Q-Wave) isn't converted. |
| `SNDMOD.IRX` | The driver (also on the disc root). |

The game's file table at `D_0029ADF0` lists these by index (LBA, sectors). `Sound_LoadSong` takes an index:
0 CQ_MAIN.TVB, 1 CQ_MAIN.TSQ, 2 ACTION.TVB, 3 ACTION.TSQ, 4-9 the radio VAGs, 0x0A BGM.TVB, 0x0B ROOM_1,
0x0C-0x17 BGM_01-12, 0x18 REPLAY, 0x19 SELECT, 0x1A ENDING, 0x1B DEMO, 0x1C FANFARE.

### TVB: a sample bank

- `0x000`: u32 wave offset[256], relative to the data at 0x800 (the SPU2 address is slot base + offset).
- `0x400`: u32 ADSR[256], stored inverted. `load_tvbf` takes `adsr = (~v & 0xDFFFFFF0) | (v & 0xF)`; the low
  half is ADSR1, the high half ADSR2.
- `0x800`: PS-ADPCM. A wave runs until a block with flag bit 0. Bit 1 on that block means loop back to the last
  block with bit 2; without it the voice stops.

A program (`tone`) is (slot << 8) | entry: a sequence plays waves from the bank in its own slot.

### TSQ: sequences

A table of `{u16 priority, u16 offset}` runs up to the first sequence. Priority 0 means a song: at the offset is a
header of 36 channels `{u8 priority, u8, u16 offset}`, offsets relative to the header. Any other priority is a
sound effect: one channel at the offset. Unused rows point at a shared stub, `F0 FF` (key off, end).

The bytecode runs once per vblank (`thread_sddr`, 60 Hz):

| Byte | Name in SNDMOD | Meaning |
|---|---|---|
| 00-7F | step_t | wait that many ticks (0 underflows to 65536: hold) |
| 80-DF | key_on | note `b & 0x7F`; pitch from `freqtbl` (0x1000 at note 48, equal-tempered, 73 entries; higher notes read past the table) |
| E0 v | volume | channel volume 0-255 (200 at start) |
| E1 l r | panpot | left and right levels, 64 = full (both 64 at start) |
| E2 p | tone_chg | program p; resets the detune |
| E3 xx | tempo | no effect |
| E4 f | pitch | set the SPU pitch (u16) without retriggering: slides, vibrato |
| E5 f | key_pitch | key on at pitch f |
| E6 m, E7 d | set_rev_mode/depth | reverb type and depth (used once each) |
| E8, E9 | | reverb send on, off |
| EA c | pitch_cent | detune: pitch = pitch * c >> 12 for later notes |
| F0 | key_off | release |
| F1 p | pri_chg | priority |
| F8 o | data_jump | jump by s16 o from after the operand (loops) |
| F9 o | ext_chan | start a copy of this channel at +o on another voice |
| other | data_end | end the channel |

Voices: 40 hardware voices (8-47 of the SPU2's 48), each with a music channel and a sound-effect channel. A voice
belongs to whichever has the higher priority. Channel volume to the SPU: `vol * pan * main >> 8`, main = 0x100.

## Requests (EE side)

`Sound_Request` (0x00258308) sends RPC 0xD with a 32-bit command:
- bits 0-11: the sequence, `(slot << 8) | entry`;
- bit 15: stop that sequence instead;
- bits 16-23 and 24-31: the right and left starting pan.

A song entry is only queued: `Sound_StartSong` (0x00258F30) starts it. `Sound_PlaySe` (0x00258E50) is
`Sound_Request(id | 0x40400000)`, which gives centre pan.

| Where | Sound |
|---|---|
| `Logo_TakaraTask` | 0x328: the TAKARA jingle. Tone 24 centred, then tones 25 and 26 hard left and right after 65 ticks; about 3 s in all. |
| `TitleSeq_Task` | 0x1F when the logos are skipped and when Start opens the menu, after `Sound_StopSong`. |
| `Title_MainMenuTask` | 0x1A and 0x1C (menu moves and choices). |
| `Title_AttractDemoTask` | `Sound_LoadSong(0x0A180201)`: REPLAY.TSQ into slot 2, entry 1. |

Songs and effects also have two small RPCs:
- 0xE, `Sound_SongCommand` (0x00257F30, SNDMOD's sddrSongData): 0 queues a song entry, 1 starts it (Sound_StartSong),
  2 stops it (Sound_StopSong), 3 and 4 set the song channels' volume to 0 and back to 0x100 (Sound_MuteSong,
  Sound_UnmuteSong: the race's pause; the song plays on), 5 and 6 fade (procFade, each vblank: 5 takes the volume
  from 0x100 down 4 a vblank, 6 up from 0 the same way; Sound_FadeOutSong, func_00258FB0, is 5: falling off Tin
  Raceway). A new song starts at full volume (reqmus).
- 0xF, `Sound_StopMode` (0x00257FD8, sddrStopMode): set_cancel_mode, with 1 every sound effect's sequence ends.

The radio has its own RPCs, below the engines.

## Engines

Each player's engine is two of SNDMOD's *direct voices* (`diwork`, played by `procDirect` outside any sequence):
voices 0 and 1 for player 0, 2 and 3 for player 1. `procEngine` runs every vblank with each engine's block
(sddrwork +0x22C, 0x24 bytes):
- {left, right level; state 1 stopped / 2 playing; mute; type; select; the select last keyed (-1 none); rpm; reverb}.

| RPC | EE side | Does |
|---|---|---|
| 0x20 | Sound_EngineReset (func_002585B0) | `sddr_EngineInit`: both engines stopped, levels 0x3FFF |
| 0x21 | Sound_EngineVolume (func_002585F0) | an engine's levels. Sound_EngineInit (func_00258578, at boot) sets both to 0x1CCC |
| 0x22 | Sound_EngineRpms (func_002586A0) | type, select, rpm, reverb. The EE side makes select 1 when rpm is 0 |
| 0x23 | Sound_EnginePlay (func_002574A0, with 0x22: type 0, select 1, rpm 0) | state 2 |
| 0x24-0x26 | Sound_EngineStop, Sound_EngineMute, Sound_EngineLoud | stop; stop a playing one and mark it; restart a marked one |

**procEngine:**
- **A new select** (or a fresh start: Sound_EngineStop sets "last keyed" to -1, Sound_EnginePlay doesn't) loads both voices with the type's pair of waves (`wave.168`: type 0 = waves 2
  and 1, type 1 = 0x28 and 0x27, type 2 = 0x2A and 0x29, all in slot 0, CQ_MAIN.TVB). It keys on the select's voice
  and keys off the other, with ADSR 0x13FF / 0x5FC5 (a 64-sample attack, full sustain, a fast release).
- **Otherwise** only the pitch and the reverb send (vmixe) change: diwork flags 0xA0, no start address. The new type's
  waves are still written into diwork, so diwork can name waves that aren't sounding. The SPU2's voice addresses
  tell which one is.
- **The pitch** comes from the rpm through `data.169`, straight lines between {rpm, pitch there, pitch at the next row}:
  0-2000 rpm gives 2000-3000, 2000-4000 gives 3000-9500, 4000-8000 gives 10000-16000, 8000-10000 gives 16000-16383. The rpm is capped at 10000.

The race uses type 2: select 0 (wave 0x2A) off the throttle, 1 (0x29) on it (effects.md). Checked in a PCSX2 save
state: rpm 8586 had voice 1 on wave 0x29 at pitch 16112, both levels 0x1CCC. In 2 Player both engines play at once
(a save state with both players accelerating): engine 1 is player 2's, on voices 2 and 3, with the same
levels; neither is panned to a side.

## The radio

The car radio plays one of three stereo streams, `1CH`, `2CH` and `3CH` (`_L.VAG` and `_R.VAG`: PS-ADPCM at 12 kHz,
mono each, a "VAGp" header of 0x40 bytes; 1CH's header names it `us_cq`, a US recording from April 2002, the others
are from October 2001). Sound_Init gives SNDMOD the files with Sound_RadioFile (0x002575E0, RPC 0x32): **station 0
is 1CH (Peach FM), 1 is 3CH (E-Radio), 2 is 2CH (Q-Wave)**. The radio's buttons reach stations 0 and 1 only
(save +0x12DD: 0 off, 1 Peach FM, 2 E-Radio; the names are POSE picture 0's strip "1 PEACH FM 2 E-RADIO 3 Q-WAVE").

- **Sound_RadioVolume** (0x002589A0, RPC 0x31 sddr_RadioVols): the levels (0-0x3FFF). vol_Radio sets the L
  file's voices to the left level on the left only, the R file's to the right; mono averages them.
- **Sound_RadioTune** (0x00258A48, RPC 0x33): the station (+0x290) and the clock (+0x28C, info+0x14), and +0x2A0
  (the chunk playing) = -1.
- **Sound_RadioPlay** (0x00258AF0, 0x34): state 2. **Sound_RadioStop** (0x00258B30, 0x35): state 1, keyed off.
  Mute and Loud (0x36, 0x37: stop a playing radio and start it again) have no callers.
- **procRadio**, each vblank: a station is 120 chunks of 1800 frames of the clock, 0x32392 bytes a side, so a day
  (0x34BC0 frames, an hour) is its whole stream. The chunk playing is clock / 1800; load_radio (the CD thread)
  reads it from (chunk x 0x32392 + 0x40) & ~0xF into one of two 0x40000-byte buffers a side, and the voices key on
  (clock % 1800) x 0x72 & ~0xF bytes in (0x72 bytes = 199.5 samples a frame, 12 kHz), pitch 0x400, ADSR
  0x35FF/0x5FC5. The next chunk follows in the other buffer. So the radio plays from where the clock is: the same
  hour of the day always plays the same part of the station.

The streams have no ADPCM flags but on their last block, so they decode as one sample. 3CH runs about 10 s past
the 120 chunks; 1CH about 3 s.

**The game's calls:** Radio_Start (0x002590B0): Sound_RadioVolume(save +0x12DE x 0x3FFF / 128) and, with a
station, Sound_RadioTune(station - 1, clock) and Sound_RadioPlay. Field_CreateTasks calls it for the field, and
Adventure_Task's state 2 after the menu (and state 7). Adventure_Task stops it (Sound_RadioStop) on Start (before
the menu), at a door and when a talk ends in a building. The Radio screen (field_menu.md) runs the clock and
tunes as it changes station.

**Check** (`captures/adventure/field_radio.p2s`, the Radio screen with E-Radio on): PCSX2's sound
(`field_radio_ps2.wav`, 10 s from clock 92656; the radio tuned at 92229 as the screen opened) against
the 3CH stream: correlation 0.95 at 0.3 s from the place the formula gives (the clock was read over
PINE as the recording started), and 0.07 against 1CH.

## The PC version

`tools/sndconv` converts the data into `data/sound`:
- each TVB becomes a SoundFont 2: one preset per program, 16-bit samples at 48 kHz with loops, root key 60;
- each TSQ entry becomes a Standard MIDI File, plus a manifest per TSQ.

`src/audio` plays them: `Synth` mixes in software into one XAudio2 2.7 voice.
- **Timing:** one MIDI tick is one TSQ tick. That is 60 ticks per quarter note at 60 bpm.
- **Channels:** each TSQ channel, spawned ones included, becomes its own track. Tracks are spread over MIDI ports 0-3, 15 channels each, skipping channel 10. Songs use up to 36 channels.
- **Pitch:** key = note + 12. Exact pitches (detune, the `E4` slides) are pitch bends with a ±24-semitone range set by RPN 0 on every track. One bend in CQ_MAIN 32 goes past that range and is clamped.
- **Levels:**
  - CC7 = 127·√(vol/255).
  - Pan becomes CC10 = 128·R/(L+R) and CC11 = 127·√((L+R)/128).
  - The engine pans linearly with 0 dB at the centre, so (64, 64) and (64, 0) both come out at the original level.
  - CC91 is the reverb send.
- **Loops:** a converted song repeats from its `loopStart` marker to its `loopEnd` marker on track 0.
  - The converter runs every channel until the channel comes back to a position in the same state. The steady loop starts at the latest channel's loop start.
  - Every song's channels share one loop period, except one BGM_12 channel (4074 ticks against 4112), which uses the longest.
  - Where the first pass through a loop differs from later ones (ROOM_1), the file covers two passes.
- **Envelopes:** ADSR is simulated with the SPU2 rules and fitted to SF2's attack, decay, sustain and release.
  - When the SPU2 sustain keeps falling, SF2 can't express it. The decay is then timed to reach −60 dB at the same moment, with silent sustain.
- **Engines:** `Synth` has direct voices too: a program's sample keyed on outside any sequence, with its pitch,
  levels and reverb send set by the caller. They are never stolen for notes. As on the PS2, `Sound_EngineRpms` and the
  other requests only change the engine's block, and `Sound_ProcEngine` (procEngine) acts on it once a vblank. The
  audio thread runs it every 800 samples, before procFade. The levels (vol_Engine) apply at once. The timing
  matters: the type a key-on takes is whichever was set by that vblank (effects.md, "The port").
- **The radio:** sndconv writes each station the radio reaches as `data/sound/radio/<n>CH.adp`: the two VAGs'
  PS-ADPCM blocks as they are, interleaved, with a seek point (the two samples before the block) every 512 blocks,
  about 1.2 s (an hour, 50 MB). The stream decodes as one sample
  from its start, so `RadioStream` (audio/radio.cpp) decodes a 512-block segment at a time from its seek point and
  keeps two (the interpolation reaches across a segment's end); the samples are the same as decoding the VAGs.
  Checked against the old WAVs: both whole hours and 500 random 48 kHz starts mix identically, and the field's
  recorded sound (`--field 1 --wav`, E-Radio on) is byte for byte the same. Mixing a minute costs about 41 ms on
  the VM (the WAV took 88 ms, mostly in 64-bit divisions now gone from both). A 16-bit WAV of the station
  (`<n>CH.wav`, any rate, mono or stereo) is played instead if there is one. `tools/adpconv` (a modding tool, in
  the player folder's `tools`) encodes a WAV to a radio file and decodes one back. The encoder tries every filter
  and shift per block, decoded as the SPU2 does, and keeps the closest: 71.7 dB SNR re-encoding 1CH's decode,
  58 dB on a test tone, 35.7 dB on BGM_01 rendered at 48 kHz; an hour of the stations in about 6 s. The codec and
  the file layout are `src/audio/adpcm.{h,cpp}`, shared by the game, sndconv and adpconv; adpconv's decode gives
  exactly what the game plays. IMA ADPCM was measured at 25 dB SNR against the PS2's
  decode, an audible hiss, and wasn't used. It reads the file through a 2 MB mapped window and mixes it
  into `Synth`'s output after the reverb (dry), at the SPU2 voice level (kMaster) times the radio's levels, with
  Catmull-Rom interpolation to 48 kHz. `Sound_RadioPlay` starts it at the frame procRadio would key: chunk and
  offset as above, wrapping after the 120 chunks (43 199 940 frames). It plays straight on from there, as the
  PS2's chunks follow each other.
- **Not carried over:**
  - per-request pan (the converted files assume the centre);
  - voice priorities and stealing (96 voices instead of 40);
  - reverb modes (one Freeverb-style room for all);
  - the SPU2's Gaussian interpolation (linear here).

`RTAGlass.exe --soundtest 328 5 out.wav` renders requests offline, which is useful for checking a conversion.
