# The finish, the replay and the results

What a race does from the final lap to the end, and what follows it in each mode. The race part is in the port
(`src/game/race`: `race.cpp`, `race_camera.cpp`; the banners in `game/hud`). The Adventure's results
screen is in `game/results` (adventure_results.cpp, grand_prix_results). Lap counting and the race order are in hud.md, "Laps and places".

## The final lap

Race_CompleteLap gives the player race flag 0x10 when its lap count reaches the course's laps − 1, and sets Race_Task
+0x27 = 0x80. Race_Task counts +0x27 down each frame and lights FINAL LAP while it has bit 3 (Hud_SetFinalLap
0x002341F8): it blinks 8 frames on, 8 off, for 128 frames.

Race_SetupTask (0x0020F3B8), the race's parent task, sees flag 0x10. It swaps it for 0x20 (recording) and calls
**Replay_Save** (0x0021CBC8), which copies all 24 car records (0x3A80 bytes) to 0x100000 and starts three run-length
input lists (race +0x44 to +0x4C). From then on Car_Input appends each player car's input bits: a {count, bits}
entry that counts up while the bits don't change. Only the players' and teammates' cars (flags 0x33) are recorded. The
AI cars run their AI again in the replay, and since the AI depends only on the car records, they drive the same
race.

## The finish

When a car completes its last lap, Race_CompleteLap:
- gives it its place (+0x247, from Race_Task +0x26);
- gives it flags 0x280: finished, and the AI drives it from then on;
- caps it at 0x28 (100 km/h);
- for a player, sends sound +0x245 + 0x8000 (0x803C). That stops the horn (+0x245, key 6, effects.md) in case it's
  held. The engine goes on idling: Car_Sound gives a finished car rpm 0.

The player's finish completes Race_Task +0x25, the set of players still racing. With one player that's the player's
own bit, so the finish is immediate. From that frame:
- **Race_Task** stops its own work: no clock, no lap zones, no ranking. The opponents still racing keep driving but
  never finish.
- **FINISH!** (Hud_SetFinish 0x00234128) is element type 3 of the HUD canvas. It is picture 1's sprite (UV 0,0,
  210 by 56, palette 0) centred on (320, 112) and zooms from nothing to full size: scale t / 30 for its first 30
  frames.
- **The meter** goes: Meter_Draw skips a car with flag 0x200. The map, the times and the place stay.
- **The fade.** Race_Task draws Fade_SetOverlay(t << 24) for t = 0 to 0x80, which fades to black over 128 frames.
  When t reaches 0x80 it copies every car's place to D_01787008 (+2), tells its parent it's done (result −1) and
  becomes Race_ReplayTaskInit (0x0022ECB8). That starts the queued song, clears the canvas to REPLAY alone
  (HudCanvas_InitReplay 0x00234528) and runs Race_ReplayTask (0x0022EB78) from then on.
- **The camera.** Camera_ChaseTask (0x0021E690) sees flag 0x200 on the player's car and hands the view to
  Camera_FinishTask (0x0021E4D8). That task starts **Camera_TrackSide** (0x00238B18) on the car.

### The trackside cameras (Camera_TrackSide)

D_002A2A50[course id] points to a list of 0x20-byte entries {f32 x, y, z, radius, zoom; s16 a, b, c, d; u32 flags}.
The last entry has flag 0x8000. Courses have 9 (Peach Raceway) to 39 entries. Each frame the camera takes the first
entry whose sphere holds the point 1 above the car (or the last entry), then:

- **flags 0x1000** (almost every entry; Peach Raceway's are all 0x1022): the eye is (a, b, c), absolute with flag
  0x20 or else from (x, y, z). It is used while the car is within d = 999 horizontally; otherwise the eye is (x, y,
  z). The view looks at the car's point from the eye, with up = +y. With flag 2 the focal length is the distance ×
  zoom (75), clamped to 500 to 5333 (D_00332E38). The car keeps its size until the lens can't zoom further.
- **flags 0xC**, the last entry (0x801C everywhere): a follow camera d = 7 behind the car, pitched a = 0x1000 down,
  turned with the car's heading (0x10), focal length 400. Flags 4 and 8 centre it on the car's x and z.
- Otherwise, a fixed view from (x, y, z) at the angles (a, b).

Checked in PCSX2 after the finish on Peach Raceway: the camera block D_0177E9F0 holds the eye (665, 8, 560) and focal
lengths 5333 → 2641 as the car comes closer, which is distance × 75 to within a frame. modelconv writes the tables
to `data/camera/Cnn.txt`.

## After the fade (Race_SetupTask)

With race flag 8 and the race task's result, Race_SetupTask picks what follows, by the course table row (pad +0x20)
and the player's place (D_0178700A, 0 = first):

| row | place < 6 | otherwise |
|---|---|---|
| 2 Player (mode & 2) | func_00235B58 (the versus results) | the same |
| < 0x1C (Quick Race, Adventure races) | the replay (not on courses 0x0F and 0x10), then Results_Task | Results_Task |
| 0x1C-0x22 (World GPX stages) | the replay, then Results_GrandPrixTask | Results_GrandPrixTask |
| ≥ 0x23 | func_00238950 | the same |

## Falling off the course (Race_CheckFall, Race_FallOutTask)

Race_Task creates **Race_CheckFall** (func_0022DC68) as a child on courses 0x0F and 0x10 (Tin Raceway: its track is
at 110-290, over a floor at -50, C19's collision). On course 0x26 it creates func_00263C38 instead. From green (race
flag 4), each frame it checks every car still racing (+0x198 != 0, not finished 0x200). A car whose height (+0x94)
is below **110** has fallen off:
- A car with player bits (+0x198 & 0x73) sets its bit in Race_Task +0x25, as a finish does.
- **An opponent** is out: +0x198 = 0 and place (+0x247) -1. Car_Update, Cars_Draw, Cars_DrawEffects, Cars_Collide,
  the line, Race_RankCars and RaceMap_DrawCars all skip a car with +0x198 = 0.
- **The player's car** (car 0) ends the race:
  - **Sound_FadeOutSong** (func_00258FB0) runs RPC 0xE command 5. procFade takes the song's volume from 0x100 down 4 a vblank, so it is silent after 64.
  - Sound_MuteCars runs.
  - Race flags become |= 0x10000 (no map or meter) and &= ~0x100 (Start can't pause).
  - Race_Task's function becomes **Race_FallOutTask** (func_0022EAC8), and the check deletes itself.

**Race_FallOutTask** draws Fade_SetOverlay(h << 25) for h = 0 to 63, a black fade over 64 frames. It keeps no clock or places, and the HUD canvas isn't drawn. The cars' tasks run on, so the player's car keeps falling with the chase camera behind it. Then:
- the player's car is out (+0x247 -1, +0x198 0);
- Race_SetupTask +0x24 = 1, result -1;
- pad +0x50 and the overlay are black.

Race_SetupTask's default branch sees +0x24: the screen goes black and the sounds and song are muted. Outside the World GPX (rows 0x1C-0x22), the race ends with result 0, the same as Retire. A Quick Race goes back to the course select. There is no replay and no results.

**The port.** `Race::Race_CheckFall` and the `kFallOut` phase do this; `RaceCar::out` is +0x198 = 0. Check: placed
off the road (`--race 16 --place 711 180 1236 16384`) the car lands on the floor at -50. At green the HUD goes, the
screen fades, and the course select loads at frame 305. The AI driving Tin Raceway (`--autodrive 0`) stays above
139 for 4000 frames.

### The replay

The replay is Sound_LoadSong(0x0A180201) (REPLAY.TSQ), race flags 0x10044 (green, replay, no map), and **Replay_Restore**
(0x0021CCB8), which copies the saved records back. The replay therefore plays the final lap again:
- The player's car reads its recorded bits; when they run out it gets flag 0x200.
- The AI cars drive by their AI as before.
- Race_ReplayTask watches the line but keeps no times or places (flag 0x40). The clock doesn't run.
- The HUD canvas shows only **REPLAY**: picture 1, UV (0, 152), 136 by 48, palette 4, centred on (100, 44). There is
  no map or meter (flag 0x10000).
- Camera_ReplayTask (0x0021E328) follows its group's car with Camera_TrackSide: 0 the player (car flags & 0x27E
  == 2), 1 and 2 the teammates (0x10, 0x20). Triangle on pad 1 (or the followed car gone) moves on one group, or two
  if that one has no car; in a Quick Race it stays on the player. The port: Race::Update (Replay_GroupCar).

The replay ends when the player's car crosses the line again (+0x25 complete), or on Start. Race_ReplayFadeOut
(0x0022EE00) then fades to black over 128 frames and tells the parent.

### Results_Task (0x00237508)

- **Quick Race (mode 1):** no screen. The screen stays black and the parent is told the outcome: 1, or 2 for a win.
  QuickRace_Task then loads the course select again. Race_SetupTask first calls func_00237C48 (Results_UpdateRecords),
  which in a Quick Race keeps the card's best lap, best total and best place and counts an attempt (save +0xB78,
  save.md). The Adventure's records are save +0xFF0 best place, +0x1040 best lap, +0x1178 best total
  (func_00237680 prints them).
- **Adventure (mode 0):** the race results screen, **Results_DrawRace** (0x002364B0), over the results' backdrop
  (Results_DrawBackdrop). Race 26 (against the president) has none: black, and the parent is told 2 (won) or 1.
  - Title: the race's name (D_002BE398[course row]), at (230, 27).
  - A window (cells 2,3 to 28,11) with the headings ENTRY NAME, RANK, TIME and Bestlap, and a row for the player and
    each teammate (**Results_DrawRow**, 0x00235F10): y = member × 0x18 + 0x41 half-lines; the name at x 0x46, the
    place at 0x118 (palette 6 in the first six; "retire" at 0x104), the total at 0x168 and the best lap at 0x1E0.
    A time that beats its record blinks palette 6, 16 frames in 32, offset 4 frames a row. 12 half-lines below:
    " RECORD" (cell 8) and the records in palette 5 in brackets: best place (" --" for none), total, lap. The
    records are by race (race 26 and 27 use 24's and 25's) and member (+0xFF0, +0x1178, +0x1040, 0x1A apart).
  - With prize money (+0x20 ≠ 0): a "Congratulations" window (cells 10,11 to 20,17) with each player's place and
    PRIZE (Results_DrawPrizes 0x00236328).
  - The license messages, from +0x27: "got a B-License", "got a A-License" or "Super A License".
  - Results_DrawPrizes: for each place 1st-6th one of the team took, a line from row 0xD: the place (cell 0xB),
    " PRIZE" (cell 0xD; the PS2 prints them run together, "1stPRIZE") and the amount (small digits ending at 0x19A).
  - **Wallet_Draw** (0x0022E7F0): the money now in a money box at (0x1B8, 0x152) and the money with the prize at
    (0x1B8, 0x182), icon 7 bobbing between at (0x204, 0x171 + (frame >> 1 & 7)). **Money_Draw** (0x00202D10) is the
    box: FONTE palette 9, caps at u 0x1AB and 0x1C8 (5 wide), two middles repeating an 8-texel column (REGION_REPEAT,
    u 0x1B0 for 0x60 and u 0x1C0 for 0x40), v 0xDD, 32 high; the figure in the large digits ending at x + 0x60, top
    y + 6, and the currency at x + 0x62.
  - Cross (from the second frame) pays the prize (**Money_Pay** 0x0023DC58, SE 0x305), sets the license, records the
    race (Results_UpdateRecords), and on race 0x18 (Endurance Run) sets story flags 0x31 and 5 (func_0023C8A8,
    func_0023CB68). A place in the first six with stamp 0x28 earned brings mail. It tells the parent 5 with a
    license won, else 4 first, 3 second to sixth, 2 lower with a teammate's prize, 1 lower. Race_SetupTask keeps it
    as Race_Outcome (0 after Retire), which Q's Factory's talk branches on (messages.md, pre-code 0x0B).
- **Results_UpdateRecords in mode 0:** for the player and each teammate (stopping at the first missing one): the best
  place, total and lap of the race's record, each kept if better. A teammate's record also remembers who set it
  (+0x33C0); another teammate's race replaces all three, and the name, when it's better at any of them. Then every car
  placed behind the player is beaten: **Save_SetBeaten** (0x0023DCF8) sets bit index of +0x558[group - 1] (group
  13: the tenth word).
- **The prize money (Results_PrizeTotal 0x00236238):** D_0029FCB8[course row's prize class (+3)][place], for places 1-6:

  | class | 1st | 2nd | 3rd | 4th | 5th | 6th |
  |---|---|---|---|---|---|---|
  | 0 | 800 | 500 | 400 | 300 | 200 | 100 |
  | 1 | 1500 | 1200 | 1000 | 800 | 600 | 500 |
  | 2 | 2500 | 2000 | 1600 | 1200 | 1000 | 800 |
  | 3 | 80000 | 60000 | 40000 | 30000 | 20000 | 10000 |
  | 4 | 10000 | 7500 | 5000 | 3000 | 2000 | 1000 |

- **The license** (+0x27, worked out at the start of Results_Task, races 0-23): the license (save +0x651) goes up one
  when every race of the license's class (race row +3) has a best place of 6th or better, counting this race as placed
  if it was placed now. With license 3 it doesn't. It's set on Cross.
- **Results_GrandPrixTask (0x00237388)**, "World GPX stage": every car scores 0x1E − place points into the save
  (+0x3314 + 0x18 × stage; an out car 0). It then shows the standings (func_00237080, func_00236D28, func_00236858).
  See "World Grand Prix" below.

## World Grand Prix

Races 28-34 (rows 0x1C-0x22): seven stages of 24 cars and 3 laps, one a visit to that town's Q's Factory (Peach
Town, Fuji City, Sandpolis, White Mountain, Chestnut Canyon, Papaya Island, Mushroom Road), entered from Q's menu
(#45, with the Super A license and a full team, #47; flags 101-106 mark each stage entered, #71 clears them).
- **The field** (Race_CreateCars' rank-3 branch, 0x0020F01C): the player (slot 23) and both teammates (slots 0, 1),
  then group 10's drivers in list order as cars 3 on (lane 3 + k, slot 2 + k with the Quick Race's 3/6 and 9/12
  swaps), so team t is cars 3t..3t+2: "Team " + car 3t's driver. Port: Race_CreateAdventureCars' rank-3 branch.
- **The end:** Race_SetupTask runs Results_GrandPrixTask after the replay (first six) or at once, and also after a
  retire or a fall, once Gpx_RetireTeam (func_0020F330) has put the player's team out (no points) and moved the others
  up past it. Never Results_Task: no records, license or prize but the final's.
- **The screens** (GrandPrixResults, grand_prix_results.cpp), over the results' backdrop and "World GPX stage n":
  Gpx_DrawStageOrder (two columns of 12, a row every 4 frames with SE 0x22, the player's team blinking in palette 6,
  Cross from frame 0x60); Gpx_DrawTeamStage (each team's places, then its points "a + b + c" and the sum); and
  Gpx_DrawStandings (the totals over the stages so far sorted down, a tie keeping the player's team ahead, a row every
  8 frames; from frame 0x40 Cross). On the final the team's place pays 80000-10000 for 1st-6th with the wallet (the
  prize frame is drawn empty for 7th and 8th).
- **The outcome** (Race_Outcome): 3 a stage run, 0 retired (stages 1-6); after the final 7 won (Q's #59: flags 87
  and 108, to Cloud Hill) or 6 lost (#58). +0x3310 counts the stages, back to 0 after the final either way.
- Not in: Race_InitTimes' quirk (teammate 1's lap of races 0-6 as the lap to beat; the port's index now follows the
  asm, so this may be stale). Debug: `--gpstage N` (with `--factory`/`--advrace`).

## The ending

After the president's race, Forest's (8, 9) "Well, I'm sure you'll be a great President!" ends with code 0x18 (talk
result 15). My City Cinemas' (9, 8) talk ends with it too, once president. All from the asm and the ELF; the full spec
the port was written from is summed up here.
- **Shop_SceneTask's state 3** (asm 0x0022B788): the room's song fades, Fade_OutTask darkens the room (still drawn
  while the state's age is under 0x41), and the task runs Ending_Task (0x0026CB60) in town 8, else
  Ending_ReplayTask (func_0026D2C0). Adventure_Task stays in state 4: the clock runs on underneath.
- **Ending_Task:** scene 0 (Cloud Hill) and the fanfare (0x0A1C0201, FANFARE.TSQ) loading; once loaded the song
  starts and a timer runs; at 0x30C Fade_OutTask and the song's fade; at 0x34C scene 0 goes, the clear is black and
  scene 1 and the ending song (0x0A1A0201, ENDING.TSQ) load; once loaded the task becomes the sequencer
  (Ending_SceneTask, func_0026D1C0), the staff roll (func_0026A928) starts and the song with it. The replay starts
  straight at scene 1 with the ending song.
- **The sequencer:** scene n ends at frame D_002DCD50[n] of the roll (cumulative: 1260, 2520 ... 10080, 12033),
  Fade_OutTask 0x40 frames before; the next scene is set up at once and fades in (Fade_InTask) when loaded. After
  scene 9 Shop_SceneTask is told. Nothing reads the pad: nothing skips the ending.
- **The scenes** (Ending_FieldScene, func_0026CA48): Field_Setup(0x16 + scene) and Field_CreateTasks(arg 1), the
  towns 22-31: Cloud Hill (course 0x22), Papaya Island (block 47), White Mountain (35), the Ruins (5), Chestnut Canyon
  (19), Peach Town (43), the Bridge (40), Fuji City (23), the Highway (9), Sandpolis (7), at 8:00, 9:00, 12:00, 13:00,
  14:00, 15:00, 18:00, 18:06, 19:00 and 21:00 (D_002DCD28). Arg 1: no radio, meter, coins or door check, the map
  hidden (0x10000), Camera_FixedTask instead of the chase camera, and the player's car (slot 0, the save's car 0) a
  town car (Car_Init town << 10 | 0x800000) that the AI drives: flag 0x80000 makes Ai_Init take a car's path by its
  slot, and only slot 0 has one. Every car starts on its slot's row of D_002C2E78[town] (no drop to the ground), the
  cap 5 on Cloud Hill, else 0x0F. The town's cars (group D_002C22C8[town]) stand; on Cloud Hill the two teammates
  (slots 1, 2) too, and its last two town cars would read past its table into Peach Town's.
- **Camera_FixedTask** (0x0021F098), D_0029CCC0[town - 0x16]: scene 0's eye is absolute, (1308.5, 309, 848) yaw
  0x4000, the focal 5000 divided by 1.01162 a frame down to 500, the pitch 0x380 + 5 a frame to 0x880; the others'
  eye is the car's position plus the table's eye turned by Ry(yaw) Rx(pitch), focal 500: the car followed at a fixed
  angle.
- **The staff roll** (Ending_StaffRoll, func_0026A928): D_002DC938, 69 entries {x, delay, text, palette}; an entry
  spawns into one of 24 slots on the frame its delays add up to, at y 448, and rises 0.5 a frame, drawn at
  Text_PrintAt(x, (int)y / 2) in palette 0; the first entry whose text starts with a space (palette 0xFFFF) ends the
  table, and once it has risen above 200 the roll stops: "Thank You for Playing!" held mid-screen from frame 11777
  until scene 9's fade (11969) and the end (12033).
- **After it** (asm 0x0022B560): once president (stamp 100), the cinema's talk from its start, the room back without
  a fade. The first time: stamp 100 queued, the car at the Secretary's door (8, 8) at 6:00 (54000), result 5 (the
  stamp book, then the field) without a drive-out.
- **Port:** story/ending.cpp (Ending, StaffRoll), field/field_ending.cpp (Field::Ending_Start, Ending_Update,
  Ending_Camera), RoomScene::kEnding and Room_AfterEnding, NewGame's kEnding. modelconv writes the ending towns' cars
  as data/ai/FLD/T22_00.txt-T31_nn.txt by slot (RECORD -1 the save's car 0, -2/-3 its cars 1/2); the driver records
  now cover groups 1-31. The PC streams each scene in behind the black, so scenes show longer than on the PS2, whose
  load time eats into each scene's window. Debug: `--ending 0` (the whole ending, from the president's office; add
  `--flag 90` or the closed gate turns the cars back), `--ending 1` (the replay; with `--stamp 100` the cinema's talk
  follows).
- The roll's text is drawn over the fades (Main_Loop sends Text_PrintAt's list after every task's drawing,
  0x0020053C). The ending's player car has no engine sound (Car_Init and Car_Sound need car +0x198 & 3; it has 0x80).
  Cloud Hill's slots 1 and 2 are always made from save cars 1 and 2 (0x0020E7A4; the port skips an empty seat, which
  the World Grand Prix's full team rules out). Cloud Hill's two overrun cars (town 27's rows) would stand about 900
  units off, below the cloud floor; the port leaves them out (inferred, not compared).

## Captures

- `captures/race/finish_compare.png`: the PS2 (left) and the port (right) during FINISH!'s fade, under the
  trackside camera. FINISH! and the HUD sit in the same places. The PS2 run finished 1st (by cheat) and the port's 8th.
- `replay_compare.png`: REPLAY in the corner, and the trackside cameras.
- `wrongway_compare.png`: WRONG WAY and its arrow (Hud_SetWrongWay): text palette 1 at (320, 135), the U-turn arrow
  palette 5 at (320, 92). Race_Task lights them while the player's +0x248 (Ai_TrackZone's wrong-way frames) is
  0x3D or more and the car hasn't finished, on bit 4 of the frame.

The PS2 references came from cheats over PINE:
- Keeping the player's lap zone at 4 and its laps one short, so the next pass of the line finishes the race.
- Writing car +0x247 during the fade sets the place: 9 gives the no-replay path.
- Poking +0x1D4 += 0x8000 turns the car round, for WRONG WAY.

## The port

- `game::Race` runs the finish as above:
  - Race_CompleteLap sets the place, the cap and FINAL LAP, and marks Replay_Save for the end of the frame.
  - The player's finish starts the 128-frame fade.
  - The AI drives the finished player's car.
  - Camera_TrackSide takes over the view.
- `Race_AfterFinish` is Race_SetupTask's choice. It fills a `RaceResult` (place, times, every car's place) for the
  mode, then plays the replay or ends.
- The replay restores the saved race (cars, physics, AI, laps) and feeds the player's recorded bits. The port's
  physics is deterministic, so the field drives the same race: a replayed finish matches the original's frame.
- `Race::Update` returns `kOver` once everything is black. QuickRace then goes back to the course select, which is
  Results_Task's Quick Race branch.
- The Adventure's results are `game::AdventureResults` (`game/results/adventure_results.cpp`), from the
  `RaceResult`; `RaceResult::versusCar[0]` gives the team's cars there too, and `group`/`index` each car's driver.
- `game::TrackCameras` (`race_camera.cpp`) loads `data/camera/Cnn.txt` and implements Camera_TrackSide.
- `--autodrive N` lets the AI drive the player's car (capped at N × 2.5 km/h, 0 for none), as the attract demo does,
  for checking the finish without driving.
- The replay's camera switching is in (above).

**Check:** with `--autodrive 0` on Peach Raceway the player finishes 8th: FINISH!, the fade, then the course
select. With the replay forced, the replay plays the final lap from the line with REPLAY and the trackside cameras,
ends at the player's finish with the same cars beside it as in the race, and fades back to the course select.
FINAL LAP blinks at the line, and WRONG WAY shows after a U-turn, as on the PS2.

**Check (the Adventure's results):** `captures/adventure/results_ps2.png` is the PS2 after Peach Raceway RANK C
from `factory_menu.p2s` (finished by that cheat, 1st place written to +0x247 in the fade, the replay skipped):
one row, 1st PRIZE 800, 1000 → 1800. `results_port.png` is the port's `--advrace 0 --allparts --autodrive 0`: the
player 18th, the two teammates 1st and 2nd, 1st and 2nd PRIZE, 1000 → 2300. The title, the window, the rows, the
records, the prizes, the money boxes and the arrow sit in the same places. Cross on the PS2 then goes straight to
Q's Factory, where #16 asks "How was the race?" (Great / OK / Not good / Terrible); the port does the same for its
outcome 2.
