# The pause menu

Start during a race opens a menu over the race: **Continue** and **Retire**, and in the Adventure **Ranking**, a list
of every car by its place with its driver and home town. The port has all three (`src/game/race/pause_menu.cpp`,
`race.cpp`).

## Opening it (Race_SetupTask 0x0020F3B8)

Each frame, unless the race is over (race flag 8), Race_SetupTask opens the pause when:
- race flag 0x100 is set: the countdown sets it at t = 300, when the song starts (hud.md), and it stays until the end,
  so the finish's fade can be paused too. The replay's flags (0x10044) don't have it, so Start there ends the replay
  instead (finish.md);
- the race isn't paused already (flag 0x8000);
- Start was pressed on pad 1, or on pad 2 in a 2 Player race (mode & 2). The menu then reads that pad.

It then:
1. **Sound_MuteCars** (0x00258FF0): both engines muted (Sound_EngineMute), every sound effect stopped (Sound_StopMode(1),
   RPC 0xF), and the skid loops forgotten (D_00335728), so Car_Sound starts them again afterwards.
2. Sound_PlaySe(0x1F), the pause sound.
3. **Sound_MuteSong** (0x00258F70): RPC 0xE command 3. SNDMOD's sddrSongData sets the song channels' volume to 0
   (set_volume_mode). The song plays on silently.
4. Race flag 0x8000, and the pause task as a child (state 2).

**Race flag 0x8000 stops the race.** Race_Task's clock and line, the countdown, the cars, the chase camera and the
particles' ageing (Effects_Draw) all check it. Cars_DrawEffects throws no spray or bow wave under it. The race is
still drawn every frame.

## The menu (Race_PauseTask 0x0026BC08)

- **Its first frame:** a window (WindowAlloc, centred on (320, 112)) with the lines "Continue", "Retire" (D_00335608) and,
  in the Adventure (pad +0x0A = 0) on courses below 0x12, "Ranking" (D_00335610), all items.
  - Mode 8 (9 for player 2: read pad 2): Start chooses too, and the cursor wraps.
  - State 3: the window doesn't fade in. It shows whole from the next frame, when Window_Update starts (state 0).
- **Every frame:** `Fade_SetOverlay(0x40000000)`. That is black at alpha 0x40, half brightness, over the whole race,
  HUD included; only the window is drawn over it. A PS2 frame paused and running measure exactly half (sky (113, 175,
  253) → (57, 88, 127), TOTALTIME's yellow (206, 236, 47) → (104, 118, 24)).
- **The choice** (Window_Update: 1 Cross or Start, −1 Triangle, which counts as Continue):
  - **Continue:** the task ends with result −1. Race_SetupTask clears flag 0x8000, Sound_LoudCars (the engines back)
    and **Sound_UnmuteSong** (0x00258F90, RPC 0xE command 4: volume 0x100). No sound. The window goes with the task,
    at once.
  - **Retire:** pad +0x50 = 0x80000000, race flag 0x400000, and the task ends with result 0. The next frame
    Race_SetupTask runs func_0023D8C8(0); in the Adventure, func_0022E5F8 adds the distance driven to the save's
    odometer; then race flags 0; on the World GPX courses (0x1C-0x22) Results_GrandPrixTask; otherwise the task ends
    with result 0 and the mode loads what's next. In a Quick Race that's the course select, through the loading
    screen over the last picture (the menu still on it).
  - **Ranking:** Race_RankingTask as a child (+0x25 = 1). Until it ends the window isn't updated, so it isn't drawn;
    it's back the frame after.
- Items 3-6 are never in the window: a debug build's (hide the map and meter, refuel, the AI drives your car, the
  AI zone table editor). See debug.md; the PC's Pause Items extra adds them (extras.md).

## Ranking (Race_RankingTask 0x00237878)

For every car with flags (+0x198), at y = (place × 24 − scroll + 128) / 2 half-lines, with Text_PrintAt in colour 0:

| x | what |
|---|---|
| 0x78 | its place (+0x247) as " 1st" ... "24th" (D_0029FD30) |
| 0xB4 | its driver: **Car_DriverName** (0x0022EA08). An opponent (D_01787008[i] +3 & 0xF0) has its record's name (D_002C22C8[+0] + 16 × [+1], +0xC); a player has the save's name (D_0177FD8C) |
| 0x17C | its home town: D_002BE438[D_01787008[i] +0], 8 bytes a town (its name, then its map and entry). A player's town is Peach Town, or My City (entry 9) once story flag 0x28 is set (func_0023D628) |

The towns are My Garage, Peach Town, Fuji City, Sandpolis, Chestnut Canyon, Mushroom Road, White Mountain, Papaya
Island, Cloud Hill and My City. D_01787008[i] +0 is the opponent's group in D_002C22C8, and the Quick Race fills it too.

- Down held scrolls by 2 a frame, while scroll < cars × 24 − 0x120; Up held scrolls back to 0.
- From its second frame, Cross or Triangle ends it (result 1).
- Nothing else is drawn: the race stays at half brightness under the list.

## The port

- `Race::Update` opens the pause on Start from frame 300 of the countdown (`kCountdownSong`) while racing or in the
  finish's fade. That frame is already still. `Race_Pause` and `Race_Resume` do Race_SetupTask's sound work, with
  `CarSounds::Mute` and `Loud` for Sound_MuteCars and Sound_LoudCars.
- While paused, `Race_Step` doesn't run and the particles aren't aged. The race and HUD are drawn, then the 0x40 black
  (in place of the finish's fade), then `PauseMenu`.
- `PauseMenu` is Race_PauseTask and Race_RankingTask. The Ranking's rows come from the race: each car's place, the
  opponents' names, the profile's name for the player (UnnamedRacer when there's none), and `Car_HomeTown`.
- Retire makes `Race::Update` return `kRetired`. QuickRace then shows the loading screen from the frame drawn, as
  the PS2 does, and returns to the course select on the same course.
- The Ranking item shows in the Adventure; the debug option `--ranking` adds it to the Quick Race.
- Sound: `Sound_MuteSong` and `Sound_UnmuteSong` set the song's volume (a new song starts at full volume, as reqmus
  does), and `Sound_StopEffects` (Sound_StopMode(1)) stops every sequence but the song.
- Start no longer restarts the race and Triangle no longer leaves it (both were development shortcuts); Retire
  replaces them.
- Not in the port: the debug items. The Adventure's odometer (func_0022E5F8) is added at every race end, Retire
  included (Race::Race_SaveDistance). (Player 2's pad pauses in 2 Player.)

## Check

`captures/race_peach_grid.p2s` in PCSX2 (load it by its full path), Start after the countdown; the port with `--race 0` and Start at frame
400.

- `captures/race/pause_compare.png`: the menu over the darkened race, then Retire's loading screen, PS2 left.
  The window, its place, the cursor and the overlay match.
- `ranking_compare.png`: the Ranking at the top and scrolled to the end. For the PS2 the Quick Race was turned into
  the Adventure by writing 0 to pad +0x0A (0x0033590A) over PINE. The PS2's list and the port's
  `--ranking` list match line for line: the same 24 drivers in the same places with the same towns.
- In PCSX2 the window seems to fade out after Continue. That is the frame blend: the scene is blended a little too.
- The port's sound (`--wav`): the pause sound, then silence but for the cursor's 0x1D, then the engine and the song
  together again on Continue.
