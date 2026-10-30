# The HUD

In a race the HUD has six parts:
- TOTALTIME and the race clock, top left.
- The player's place under the clock.
- The course map on the left.
- LAPTIME with the last laps and the lap being driven, top right.
- The speedometer, bottom right.
- The start lights, top middle, during the countdown.

All of them are in the port. The code is in `src/game/hud`:
- `hud_parts` holds the pieces.
- `speedometer` and `minimap` are the meter and the map.
- `race_hud` is the race's layout of them.

Reference: `captures/race_peach_01.gs` frame 1 (driving) and `captures/race/peach_grid_pcsx2.png` (the grid, during the countdown).

## Who draws what

Race_Task (0x0022DDD0) is the race's task (Quick Race course table D_002BE158 +8). It sets the HUD up on its first frame. After that, each frame it does the following:

- **The clock.** Task +0x20 counts while race flags & 0x44 == 4. Race_CountdownTask sets flag 4 at frame 240 of the countdown.
- **The line.** Each racing car goes through Race_CheckLapZones (0x0022D620), and its lap time (+0x260) counts once its zone (+0x19A) is set.
- **The order.** Race_RankCars (0x0022D7B8) ranks the cars.
- **The HUD canvas's values.** Hud_SetTotalTime (0x00233F10), Race_SetLapRows (0x0022D8C8) with Hud_SetLapRow, and Hud_SetPosition (0x00234080).
- **The map.** RaceMap_Draw (0x002427D0) and RaceMap_DrawCars (0x00242D20).
- **The canvas.** HudCanvas_Draw (0x00233860).

Meter_Draw (0x002490A0) is called separately each frame.

### The HUD canvas

The HUD canvas is a 0x8CB0-byte block. HudCanvas_Init (0x002321B8) clears it and copies element templates into it, which the setters then fill. The templates are D_002EC070 to D_002EC1D8, for one player and for the split screen. Player 2's LAPTIME and lap rows are 214 lines lower, the position 312 and the
meter 224 (two_player.md).

HudCanvas_Draw draws them. Most of the drawing is in element-type functions 0x0022EE70 to 0x00231F28 (the old "u22EE70" module).

The textures are SYS/SCORE.GSL (TEX0 table D_0029F5F8), or on an event course its own SYS/SCOREAnn.GSL with the
same pictures in other colours (events.md):
- **Picture 0** holds the headings, the digits and the bars, in 16 palettes.
- **Picture 1** holds FINISH!, WRONG WAY and its arrow, FINAL LAP, PITTING, REPLAY, the lap badge and GO! (the banners: finish.md).

The sprites address one texel more than they draw: a 16-pixel digit has UV 0 to 17. The port uses the drawn size.

One-player layout, in 640x448 frame coordinates:

| Element | Where | Source |
|---|---|---|
| TOTALTIME heading | (20, 20) | D_002EC1A8 (TIME alone for time trials: D_002EC1C0) |
| Race clock | (60, 52) | Hud_SetTotalTime |
| LAPTIME heading | (490, 20) | D_002EC070 |
| Lap rows | (494, 52), (494, 76), (494, 100) | D_002EC088 to D_002EC0B8 |
| Position | (40, 88) | +0x84FC |

Each heading is a large first letter at (0, 0) plus the rest of the word at (32, 12), palette 1.

**Time row (Hud_DrawTimeRow 0x0022F750):**
- **Bar.** Palette 1, 132 by 24: an 18-pixel left end (UV 0,176), 12 middle pieces of 8 (UV 18,176), and an 18-pixel right end (UV 26,176).
- **Marks.** ' at +32 (UV 160,48) and " at +74 (UV 176,48).
- **Digits.** Six digits, 16 by 16 at UV (16 d, 64), at +4, 18, 46, 60, 88 and 102, y +4. Digit 10 is the dash.
- **Value.** The time is turned to BCD MM SS CC (centiseconds are frames × 100 / 60). A time of 0 or 0x7FFFFFFF shows as dashes, and more than 0x57E3F shows 99'59"99.
- **Colour.** The digits' palette is D_0029FCA8[style]: 0 for the best lap (pink), 1 for a running time (yellow), 12 for other laps.
- **Lap badge.** A circle at -26 (picture 1 palette 8, UV 232,168, 24 by 24), and the lap number at -18,+5 in picture 1 palette 7 (UV 8 n, 200, 8 by 16). Laps of 10 or more put their two digits at -22 and -14. Lap 0 shows the BEST plate (D_002EBE20).

**Lap rows (Race_SetLapRows):** the last two finished laps, then the lap being driven, which uses the running style. After the finish the rows are the last three laps.

The lap time counts from the car's first crossing. A Quick Race starts on the grid behind the line, so the first row shows dashes until the line is crossed, as the grid capture shows.

**Position (Hud_DrawPosition 0x0022FE50).** The value is the order, counting from 0.
- **Place.** Large 28 by 30 digits of order + 1, picture 0 palette 0. The units are at +23 and the tens at +0. The atlas has 0 to 8 on its top row and 9 below.
- **Suffix.** st, nd, rd or th at +51 (UV 32 + 22 n, 31, 22 by 16).
- **Car count.** With the car count, a circle at +51 (UV 160,105, 32 by 32), then the count in small 12 by 16 digits at +71,+16 and +61,+16 (UV 32 + 12 d, 48, palette 1).

## The countdown

Race_Task's first frame loads the course's song (D_0029F5B0[course], Sound_LoadSong: BGM_01-BGM_10 into slot 2) and
starts **Race_CountdownTask (0x0022DAE8)**. By its frame t:
- t < 0x40: a black overlay at alpha (0x40 - t) x 2 (Fade_SetOverlay): the race fades in.
- t < 300: Hud_SetStartLights(t / 60 + 2), steps 2 to 6. From 300: step 6.
- t = 60: sound 0x2D (CQ_MAIN): beeps at frames 60, 120 and 180 and a long tone at 240, one per lamp change.
- t = 240: race flag 4. That is green: the clock counts (flags & 0x44 == 4) and the cars may go (Car_Drive passes
  flags & 4 to Car_Engine, which holds the car without it; car_physics.md).
- t = 300: Sound_StartSong and race flag 0x100. (Not the AI's start: the AI drives from green, ai.md.)
- t = 360: step 0 (the lights go), flags 0x114, and the task ends. With D_0177ADEA set it ends at 300 instead.
Race flag 0x4000 holds t at 0.

**Hud_SetStartLights (0x002329A0)** sets the canvas element at +0x800C when the step changes: centred at (320, 64)
(224 lower in the split screen or on course 0x1E), then 8 lamps (4 red over 4 green), each with a palette, a glow
fade and a glow alpha:

| Step | Red lamps (palette 6 lit, 7 off) | Green lamps (8 lit, 9 off) | Glow |
|---|---|---|---|
| 1 | all off | all off | none |
| 2 | all lit | off | red fades in |
| 3, 4, 5 | the first 1, 2, 3 off | off | the lamp just off fades out from 0x80, the others steady at 0x80 |
| 6 | all off | all lit | red lamp 4 fades out, green fades in |
| 0 | hidden | | |

**Hud_DrawStartLights (0x0022F338)** draws it, picture 0, from 88 left and 44 above the centre:
- **Panel.** Palette 6: the left end (D_002EBCE0: UV 152,168, 16 by 88), the right end at +160 (D_002EBD00: UV
  176,168) and 18 middle pieces of 8 from +16 (D_002EBD20: UV 168,168).
- **Lamps.** At (8 + 40 col, 8 + 36 row), UV (112, 184), 40 by 36, in the lamp's palette.
- **Glow.** Picture 2 (64x64 T8), UV (0, 0), 50 by 50 at (2 + 40 col, 1 + 36 row), coloured 0x4334EC (red) or
  0x50E800 (green) at the glow's alpha. The drawer moves the alpha each frame: fading in +0x40 up to 0x80, fading
  out -0x20 down to 0.

The port samples the lamps and glows half a texel inside their rectangles, because filtering would otherwise pick up
the neighbouring panel texels and the row under the glow.

**Check:** the port's frame 66 (`captures/race/countdown_peach_compare.png`, right) against the PCSX2 grid
capture (left) at the same step: the panel, lamps and glows line up.

## The speedometer (Meter_Draw 0x002490A0)

Meter_Draw runs one of eleven meter routines (D_002A8FB0). It picks the routine by the player's meter part; a split screen uses meter 10. Each routine draws its own ITEM/METERn.GSL, which the loader puts at TBP 0x237A and whose TEX0s it relocates.

The routine also gets a "field" flag, set when the course id is -1 or 0x22. With it, the routine draws the Adventure clock: Meter_ClockIcon picks moon, sun or evening, and Meter_ClockDigits gives the time as BCD. Meter_Draw0 draws it after the speed, relative to the meter's top left:
- the frame D_00327890, picture 1 in palette 12, (0, 0) 94 x 38 at (20, 100);
- the icon, picture 0, 24 x 24 from (140 + 24 n, 49) at (22, 113): 0 the moon until 0xE483 and after 0x278D0, 1 the
  sun until 0x2327F, 2 the evening;
- the digits, picture 1 in palette 12, 8 x 8 from (8 d, 40), right to left from x 91 at y 122, 9 apart: minutes,
  tens of minutes, glyph 10 (the colon), hours, tens of hours. A minute is 150 frames, an hour 9000.

The port draws meter 0's clock on the field (`MeterReadout::clock`).

Meter_Draw0 (0x002496C8) is the meter at D_002A8FF0, which is (484, 294) for one player. Its inputs:
- `Meter_Rpm`: car +0x1D0, clamped to 10000.
- `Meter_Fuel`: car +0x23C, clamped to 0x40000, which is a full tank.
- `Meter_Gear`: car +0x1FF, where 0 is reverse.
- `Meter_Speed`: 216 × |velocity.xz| / 32768 (km/h; func_0021D358 drops y), divided by 1.609 (D_00332F48) and turned to BCD.

It draws, in order:
1. **Fuel strip.** The strip at pivot (70, 70), corners (46..66, −30..30), picture 1 UV (40, 48) 20 by 60. Its palette is 0, or 3 while blinking: fuel ≤ 0x17FFF and frame & 0x20.
2. **Turned fuel strip.** The same with UV (64, 48), turned by −fuel / 0x40000 × 0.2778 half turns (up to 50°). Its palette is 2, 4 while blinking, and 5 when empty. The dial hides the turned strip's lower end, so the fuel shows as an arc.
3. **Dial.** Picture 0, 140 by 128.
4. **Gear.** At (97, 60), 20 by 14: UV (140 + 20 (g % 5), 80 + 16 (g / 5)), with R at g = 0.
5. **Speed in mph.** Up to three 16 by 23 digits from x 89 leftwards, every 17, at y 94: UV (140 + 16 (d % 7), 23 (d / 7)).
6. **Needle.** Pivot (70, 68), corners (−4..4, −59..0), picture 1 palette 12, UV (3, 53) 8 by 59. It is turned (rpm / 10000 − 0.7) half turns: 0 rpm points down left, 10000 up right.
7. **Hub.** (64, 64), 12 by 12, UV (16, 80).

A turn of a half turns maps a corner (x, y) to (x cos aπ − y sin aπ, x sin aπ + y cos aπ) in frame pixels.

The needle (−126° at 0 rpm) and the fuel strip (−50° when full) match the dump's triangles to a tenth of a pixel.

With the fins (part flag 0x1000), after the clock and before the needle, D_003278B0: picture 1 at (29, 59), UV (16, 48)
16 by 27, both arrows in one image: palette 12 neither lit, 14 for fins −1, 13 for +1. (Not the slope, as once
thought.)

## The meters

Meter_Draw (0x002490A0) skips everything under race flags 0x12000. For each player whose car hasn't finished it runs
`D_002A8FB0[two players ? 10 : player record +0x1A]`: the routine of **the save's car 0's meter part** (parts[14]), in
every mode (a Quick Race reads the save too). The routines read Meter_Rpm and Meter_Fuel (clamped, and with a scale
`(v × scale + max − 1) / max`, rounding up), Meter_Mph (func_00249458, |v.xz| km/h / 1.609), Meter_Gear (0 R),
Meter_Fins, the time of day (Meter_TimeOfDay, func_002495E8: info+0x14, in races too) and the task's frame count. A
per-player count at buf+0x1000 (cleared on the task's first frame) turns meters 1 and 6's dials (a float) and cycles
3, 5 and 7's (an int); race flag 0x8000 (the pause) holds it. Each routine draws its own ITEM/METERn.GSL in DECAL (the
one exception below); func_0024EB10 turns a sprite's corners about a pivot by rot.z × π.

| part | name | routine | top left | what it shows |
|---|---|---|---|---|
| 0 | Normal | 0x002496C8 | (484, 294) | above; the field's clock |
| 1 | Chronometer | 0x00249FA0 | (484, 291) | a watch: hands for the speed (270° to 186 mph), fuel, the rpm's sub-dial counting up, and the hour (a turn in 12 game hours); 3 speed digits, gear, fins |
| 2 | Rainbow | 0x0024A6A8 | (474, 304) | an arc of 10 rpm segments, 3 speed digits, gear, the fins' needle, the fuel bar cut by palette |
| 3 | Space | 0x0024AB38 | (492, 304) | by night (Meter_ClockIcon's moon) its backdrop modulated 0x80FFFFFF (TFX MODULATE); the fuel arc turned, a planet cycling 9 palettes by the rpm, gear, speed, the field's clock (digits only, (51 − 8i, 84)), fins |
| 4 | Triangle | 0x0024B3A0 | (424, 244) | the fuel column and 10 rpm bars round a triangle frame, speed, the fins' hub (always) and arrows, gear in a circle |
| 5 | Love Sick | 0x0024BA78 | (472, 292) | a heart: the wings in the rpm level's palette, at level 7 a heart swinging (2 frames a pose, jtbl_00327CA0), speed, gear, fins, fuel |
| 6 | Life | 0x0024C320 | (492, 304) | the fuel arc to 145°, the tank full or empty, a wheel turning with the rpm (back in reverse), gear pips, the speed's digits round a ring, fins |
| 7 | Cherry | 0x0024CB70 | (456, 304) | a cherry turning with the rpm, the fuel in 16 strips (EMPTY blinking), speed, gear, both fins arrows always |
| 8 | Duck | 0x0024D3E0 | (441, 304) | the fuel under the dial, EMPTY, speed, a stack for the gear, the fins cycling colours, rpm dots with a blinking head |
| 9 | Devil | 0x0024DA50 | (470, 300) | speed and gear in a glyph sheet ("46 / 3"), the rpm and fuel bar columns (the fuel flickering at none), the devil on top |
| 10 | Digital | 0x0024E1A0 | (492, 336) | also every split screen's; below |

Only meters 0 and 3 show the field's clock; meter 1's hour hand and meter 3's night backdrop read the time of day
everywhere. The Life Meter (6) is drawable but no shop or talk gives it (the stamp-47 check, parts.md).

**In the port** (`game/hud/meters.cpp`, Meter_Draw in `speedometer.cpp`): the tables are the ELF's sprite records;
the race passes each player's part, time of day (`dayClock_`) and pause, and a MeterState per player; the field
its own. Debug: `--meter N`. Checked in the port with all eleven in a Quick Race (`--race 0 --autodrive 0 --meter
N`) and meter 3 on the field by night; none against the PS2. Meter 6's digits are 12 × 16 texels on a 16 × 12 quad,
not turned, at 0.105i + 0.05 round the ring; a race's info+0x14 is its time of day (Race_Setup stores the course's
clock there, 0x0020FA80).

## The course map

**RaceMap_Draw (0x002427D0)** draws the map model D_0177AB78. That model is the course file's map section (offset 3 of the header), {u32 offset 0, offset 1, count 0, count 1}. The race draws part 1: program 0x50 strips (V4 header, then 4 qwords per vertex) of the track's outline, in world coordinates at y 0, white. The colour is vf17 = D_002A4350, (0.2, 0.2, 0.6) at alpha 0.7. Overlaps come out darker. modelconv writes it to `models/COURSE/Cnn/map.obj`.

The camera comes from D_002A4050[course] = (x, y, scale, angle). func_0021F920 builds a projection with focal length `scale` around GS (x + 1728, y). The world is moved by D_002A43E0 = (−800, −200, −800), turned by `angle` about y, and turned −90° about x, so the map is seen straight down from 200 above. On the 640x448 frame, with a = wx − 800, b = wz − 800, c = cos angle and s = sin angle:

```
sx = x + scale/200 × (a c + b s)
sy = (y − 2048) × 2 + 224 − 0.94 × scale/200 × (b c − a s)
```

Checked on Peach Raceway (angle −π/2): the map's bounds in the dump are x 49.9 to 112.4 and y 132 to 272. The formula gives the same.

With the debug byte, pad 2 changes the view before it's used (the PC's Map Tuner extra, `RaceMap_Tune`; debug.md, extras.md).

**RaceMap_DrawCars (0x00242D20)** draws the cars with program 0x50 models from the executable, scaled 50 / scale so they keep their size:
- **The player.** The arrow D_00290870, a triangle (0, 20), (−15, −20), (15, −20) turned by the car's yaw, in red (D_002A4370).
- **Every other car.** The 14-unit square D_00290940 in green at alpha 0.5 (D_002A4390). On the grid this is a green streak.
- **Teammates.** Cars with flags 0x30 (the Adventure's teammates) are squares in red (D_002A4370), or in cyan
  (D_002A4380) with 0x40 (player 2's teammates in 2 Player; asm 0x00242F44). A Quick Race has none. In 2 Player
  player 2's arrow is cyan too (two_player.md).

The port projects the map and the markers itself and draws them as 2D triangles: `game::MiniMap` with a `MapView`.

## The field map

In the Adventure's field the map sits in the bottom left, in a frame, and follows the player's car. **FieldMap_Task
(0x00242FE8)** is the field HUD's task. Field_CreateTasks makes it each time the Adventure goes out onto the field,
and Task_Create clears +0x25, so each field starts in the near view:

1. **The frame.** FieldMap_DrawFrame (0x00249208) draws three sprites at (30, 304) from SYS/POSE.GSL picture 2 (TEX0
   D_002A8FE0[0], TBP 0x1E94: POSE.GSL is loaded at 0x1D8E in the field), palette 10:
   - the left side, u 192, 16 x 120 (D_003277B0);
   - the right side at x + 128, u 216 (D_003277D0);
   - between them an 8-texel strip at u 208 stretched to 112 x 120 (D_003277F0, func_0024F928): the map's
     see-through lilac background.

   Palette 11 lights the side lamps (three a side, dark green to bright green). It is chosen when the map task's
   age has bit 0x20 (32 frames of each 64), stamp 40 (My Garage) is earned and a mail in record 0's box isn't read
   (func_0023DFE0, Mail_IsRead: bit 7 clear, for i below the box's count +0x628). Reading the mail puts them out.
   The PS2 capture has palette 10. The port: FieldMap::Draw's `frame` (field.cpp's frame_).
2. **The zoom.** The map key, key setup entry 9 (D_01780A5A; Select by default), flips task +0x25 between the near
   and the far view, D_002A42C0[2] = (302, 89, scale 50) and (202, 136, scale 25).
3. **The maps** (course -1 only: not on Cloud Hill). FieldMap_DrawBlocks (0x00242438) draws part 1 of each loaded
   slot's map section (D_0177AB70 to D_0177AB7C, the block file's map, in its own vertex colours), offset by the
   slot's place (func_00221288). The camera is the race map's (func_0021F920, the world moved by D_002A43E0 and
   turned -90 degrees about x), but north up (no turn about y) and following the car: x = 302 + 1728 - car x x k,
   y = 89 + 1936 + car z x k x 0.47, with k = scale / 200. So the car is always at (102, 366) in the frame.
   - **Colour.** vf17 is D_002A4340, (0.5, 0.5, 0.5), by day (the clock in 0xE484-0x278D0, the hours the UFO's door
     is shut), and D_002A4330, (1, 1, 1), at night. The capture's roads are (43, 43, 114) against the model's
     (88, 88, 231).
   - **Clip.** D_002A43F0 sets SCISSOR_2 to x 45-158 and field lines 156-207 (frame y 312-415): the frame's inside.
     The markers keep it.
4. **The markers.** The car's arrow (D_00290870, red), turned by its yaw and scaled 50 / scale. Then a green
   square (D_00290940, alpha 0.5) for each of the 23 town cars that are on and whose block is loaded.
5. **The coins.** With the Coin Radar (func_0023D488(0xF, 0x2D, 0), bought in White Mountain) and HUD flag 0x20,
   func_00241E90 draws the ChoroQ coins still to be found, in yellow (D_002A43A0). Field_CheckCoins (0x00241BA8)
   picks them up: 100 coins (D_002A3970, a position and a town each), SE 0x29, and bit i of D_0177F768 + 0x540
   cleared.
6. **Q's Rally** (race flag 0x1000): "+++ Q's Rally +++", DESTINATION and the town, and the time left
   (D_00335730 / 9000 minutes, / 150 seconds).

The port has the frame, both views, the maps, the arrow, the town cars' squares and the coins' (field.md, "Coins"
and "Town cars"): `game::FieldMap` (`game/field/field_map`), with
`FieldWorld::Field_DrawMaps` drawing each loaded block's `map.obj` through `MiniMap::DrawShape`. The maps are read
with the blocks on the loading threads. Tab, or Back on a controller, is Select.

**Check:** the port's start in Peach Town (`--field 1`, frame 290) against `captures/adventure/field_peach_ps2.png`:
same frame, background, scale, roads, buildings and arrow (from before the town cars were in).

## Laps and places

**The line (Race_CheckLapZones).** D_002A8198[course] holds three 2-unit strips across the track in driving order, as {x0, z0, x1, z1}:
- In strip 1: a lap completes if the zone was 4, and then the zone becomes 2.
- In strip 2: the zone becomes 3.
- In strip 3: a zone of 3 becomes 4.
- Outside all three, a zone of 2 becomes 1, because the car backed out.

A car therefore counts a lap only after driving through all three strips and coming round to the first again.

**A lap done (Race_CompleteLap 0x0022D390).** For players:
- The lap time goes into the last three (D_01787068).
- Sound 0x2E plays. Sound 0x2F plays instead when the lap beats the course's best lap D_00335700, which Race_InitTimes takes from the save.

For every car, the lap time resets and +0x19B (laps) goes up by one. At the course's laps (3 in every Quick Race), the car gets its finish order in +0x247 and flags 0x280. One lap before that the player gets race flag 0x10, which is FINAL LAP.

**The order (Race_RankCars).** The cars still racing are ranked after the finished ones, using these keys in turn:
1. Laps, minus one if the zone is under 2.
2. The zone.
3. The rank of the AI path zone (+0x24B; higher is ahead).
4. The distance to the gate the AI aims for (+0x24C; smaller is ahead).

Ai_TrackZone keeps the last two keys up to date for every car, the player's too (ai.md).

## Not in yet

- **Endurance Run's banners** are in: func_00234378 (PITTING, D_0029F7A8: 0,128 160×24, palette 3, while the
  player's car pits) and func_00234450 (GO!, D_0029F7C8: 0,216 128×40, palette 10, in zone 103 facing the right
  way), type 3 at scale 2 on (320, 112), race 0x18 only (Race_Task tests the pad's +0x20, the race row, at
  0x0022E224). GO!'s test also needs Race_Task's +0x25 bit 0 and +0x27 clear: not traced, left out. FINISH!, FINAL LAP, WRONG WAY and REPLAY are in (finish.md).

**Check:** the port's Peach Raceway grid against PCSX2's (`captures/race/hud_peach_grid_compare.png`) shows the same layout to the pixel. All eight Quick Race courses are in `quickrace_grids_hud.png`, and a run across the line is in `hud_drive.png`.
