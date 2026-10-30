# The Adventure's games

The games the Adventure's buildings start (buildings.md, races 35-55) that 2 Player doesn't have. Each runs as an
event (events.md): Race_Setup creates the row's task (D_002BE018 + 16 (race - 35), +8) in place of Race_Task, with
the event countdown (Race_CountdownForCourse's kind by course) and the HUD canvas, and Race_SetupTask goes back to
the talk with Race_Outcome as the task left it (no results screen).

The shared pieces, on top of events.md's:
- **The task's first frame** allocates the canvas (0x90C0 bytes), runs the course's canvas init (+0x8004 = 0, +0x8008
  = the course), loads the song and creates the countdown. The task's own state is at canvas +0x8CB0 on.
- **CLEAR** (Hud_SetClear, func_00234708): picture 1 (0, 0, 230 × 48, D_0029FA48) in palette 0, zooming in, in the
  player's WIN! slot (+0x86F0).
- **info+0x21** (the place in the town): a game can send the player on to another place than the building it
  started from.
- **The objects** are the course's (ACTION/Ann objK), placed in D_0177EB10 each frame (race.md).

## Barrel Dodging (race 52, course 0x21, ACTION/A15)

The cave in Peach Town (place 17): "You must dodge the barrels to get to Grandpa Tal's house. The white line is the
goal." One car, no opponents. Race_BarrelTask (func_0025E278) and a task per barrel (Barrel_Task, func_0025DF80).

- **The start:** HudCanvas_InitBarrel (func_002355A8, +0x8008 = 0x21), BGM_05 (0x0A100201), the countdown (kind 0).
  The car stays on the course's grid. The task's frame 1 copies model 0's place and TEX0 (D_0177F310 + 0x270,
  D_0177EA30) over models 1-3, so every matrix draws the barrel, then creates 32 barrel tasks.
- **A barrel** (0x20 bytes at +0x8CB0 + 0x20 i: position, +0xC its matrix's [3][3], +0x10 its fall speed, +0x18 its
  matrix i, +0x19 its state):
  - It starts at z 880 + 19 i, height 88.475, in lane D_002C4BF0[(D_002C4BE0[i & 15] × (info+0 + i)) % 5]. The
    lanes are x 744.4, 750, 755.6, 747.1, 752.9; D_002C4BE0 is 8, 1, 13, 4, 5, 11, 15, 9, 16, 6, 3, 10, 7, 14, 2, 12.
  - State 0, the ledge: z − 0.15 a frame; under z 1420 it falls (1).
  - State 1: its height − its fall speed (from 0.1, + 0.01 a frame), z − 0.1; once its bottom (1.575 under its
    centre) is at 78 or lower, 2.
  - State 2: on the ground (D_00335710, which func_002080D8(1) made Collision_Ground), its centre 1.575 above it, z −
    0.15. Under z 810 it's back on the ledge: z 1448.5, height 88.475, lane D_002C4BF0[info+0 % 5].
  - Within 3.3 of car 0's place (D_0177ACE0) is a hit: the parent's +4 = 1, and the barrel stops (state 3).
  - Its matrix: sceVu0UnitMatrix turned about x by (20 − frame % 40) π / 20 (not once it's hit or while paused),
    then its place. Its [3][3] goes to 1 once z ≥ 820 or on its task's odd frames, so from its second frame.
  - Under race flag 0x8000 (paused) it neither moves nor checks.
- **The task's states** (+0x90B4):
  - 1: car 0's z at 1350 or more is CLEAR, Race_Outcome 1 and info+0x21 = 0xD. Else a hit is SE 0x14 (the bump),
    MISS (Hud_SetMiss), Race_Outcome 0 and race flag 4 off, so the car can't drive on.
  - 2: race flag 0x100 off (no pause), car 0 braked (0x800); 3: a count up 1 a frame, the overlay at (count − 0x40)
    from 0x40, under the canvas; at 0xC0 the end.
- **After:** Peach Town's place 17 says "Oh... Too bad. Try it again!" for 0. CLEAR goes on to place 13, Grandpa
  Tal's house: "Hmm? You moved all my barrels to get here... Bah!!".

### The port

`race_barrels.cpp`: Race_BarrelTask and Barrel_Task; `CourseObjects::ShareModel0`; `RaceResult::scenePlace` (the
place the Adventure goes on to). The barrels' lane uses `Info_Frames`, which counts the races' frames where the PS2
counts every frame since the game started. The port's race doesn't run its tasks while paused at all, so the barrels
keep their turn under the pause instead of standing unturned.

Checked in the port (`--advrace 52`): the barrels roll off the ledge and down the cave, a hit is MISS with that barrel
stopped and the others rolling on, and CLEAR (`--place 750 120 1352 32768`) opens Grandpa Tal's house and his talk.
On the PS2 (`factory_menu.p2s`, any race picked while a PINE write holds info+0x20 at
race 52): the same cave, grid, barrels and lanes, and a barrel hitting the car idle on the grid is MISS.

## The games' common shape (race_games.cpp)

The Drag Race, Rock Climbing, the Volcano Course, Rainbow Jump, Beach Flag and the Treasure Hunting Maze are one
state machine each in the canvas's +0x8CB0 block, a fade count at the task's +2, and BGM_05 (0x0A100201). A game
played against the clock shows the TIME element (Hud_SetTime: the TIME heading at (40, 20), a running time row at
(40, 52)), counting while green and not paused. The banners are drawn after the fade's overlay, so they stay lit
over the black (but Rainbow Jump's, which hides the canvas first). The port: `Race::Race_GameStart` and a
`Race_*Task` each, the state in `Race::GameState`.

**Race_CountdownForCourse** (func_002357C0, jtbl_002EC3E0) gives course 0x16 (the Drag Race) and 0x23 (Ski
Jumping) the race's Race_CountdownTask with the start lights, on the event canvas, rather than the event countdown
(`Race::Race_StartLightsCountdown`).

**The frame's clear colour.** Main_Loop clears each frame to the background colour (pad +0x50; the pause's Retire
sets it black), which the light state keeps at +0x174. A race shows it wherever the course and its sky leave the frame
bare: Rainbow Jump's white below the cloud floor, which fades out at its edges. The port begins a race's frame with
it (`Race::ClearColor`).

## Drag Race (race 37, course 0x16, ACTION/A04)

Johnny's, in Sandpolis (place 10). Race_DragTask (func_00260638); HudCanvas_InitDrag (func_00234E50, +0x8008 =
0x16). The row's +0xC is Ai_Drive: Johnny (body 0x0D) drives AI path 0x16.
- The start (state 0): the player at (417.5, 24.5, 388), Johnny at (422.5, 24.5, 388), both facing +z; Johnny's
  +0x19A = 1 so he steers at once. The countdown is the start lights.
- State 1: the player out of the lane (x < 410 or > 430) or back under z 370 is COURSE OUT (Hud_SetCourseOut,
  func_00234AB0: picture 1 (0, 0, 256 × 46) in palette 11, rocking, in the MISS COURSE! slot) and Race_Outcome 0.
  Over z 790 first: WIN! and Race_Outcome 3 within 480 frames of green (0x1E1), else 2; Johnny over it first: LOSE,
  1. Either way the player's brake, no pause. Johnny brakes past the line.
- State 2: race flag 8; 3: the fade, 1 a frame to 0x80.
- The talk (Sandpolis 10, #8): 0 "Retire?", 1 #10, 2 #11 (stamp 0x1B), 3 #16 (stamps 0x1B and 0x1C, "within 8sec").

## Rock Climbing (race 42, course 0x15, ACTION/A03)

Chestnut Canyon's place 5. Race_RockClimbTask (func_0025FBF8); HudCanvas_InitRockClimb (func_00234E08). The car
starts on the grid (106.6, 1.5, 99.3) facing +x; the countdown is kind 2.
- Over the goal line D_002D9938 ((167, 215.3)-(162.6, 211.4), the side test ≥ 0): GOAL!! (Hud_SetGoalA03,
  func_002348D8: (0, 0, 232 × 48) in palette 3, zooming in), info+0x21 = 6 (on to Mason's), Race_Outcome 2 if the
  time is under 7200 frames and stamp 0x39 is earned, else 1. Back over D_002D9928 behind the start: 0. Either way
  the brake.
- States 2 and 3 as the Drag Race's.
- ACTION/A03.BIN (like A09, A12, A14 and A17) ends 8 bytes before its PMP's end word; modelconv now pads the PMP
  with zeros to its size (the disc's sector padding), so these courses have their collision.

## Volcano Course (race 50, course 0x1F, ACTION/A13)

Boss Rorke's, Chestnut Canyon place 7. Race_VolcanoTask (func_0025E630); HudCanvas_InitVolcano (func_00235418). The
car starts on the grid (208.75, 105, 200) facing +z; the countdown is kind 0; Course_ObjectsTask's A13_ScrollTask
runs as well.
- State 1: at height 169-171 inside D_002C4C40's square (306.5-311 × 282.2-286.5), the platform before the tool box:
  SE 0x14, race flags 0x104 off, the brake, CLEAR; Race_Outcome 1 the first time, and with stamp 0x37 earned 2
  under 4200 frames or 0 slower. Height 100 or under: MISS and 0. x 110 or under: 0, no banner.
- State 2: the lid's counter from 180 a degree a frame to 225, the wrench 0.03 a frame higher; 3: no green or pause;
  4: from 0x40 the fade, 2 a frame to 0xC0.
- The objects, every frame: A13's obj0 the box at (308.75, 170, 286.5), obj1 the lid at (308.75, 170.8, 286.5) turned
  about x by ((counter % 360) − 180) degrees, obj2 the wrench at (308.75, y, 285).
- The talk (#10): 0 #11, 1 #12 (stamp 0x37), 2 #13 (stamp 0x38).

## Rainbow Jump (race 47, course 0x1B, ACTION/A09)

Smiley's, Cloud Hill place 6. Race_RainbowTask (func_00254D20); HudCanvas_InitRainbow (func_002352D8). Its canvas is
at the task's +0x20 and 0x8CD0 bytes; its setup is state 0's.
- State 0: Car_ResetForRespawn, the car at (652.8, 400, 1198.5) facing +x (the grid's place), the countdown (kind 0).
- State 1: under height 275 MISS and Race_Outcome 1; under 350, at rest (car +0x90..+0x9F as last frame and no
  speed), CLEAR and 2. 2: no pause, the brake, the speed zeroed; 3: 129 frames, then Sound_FadeOutSong and race flag
  0x10000 (no meter or canvas); 4: the overlay at count × 2, to black over 64 frames. No Sound_MuteCars.
- While paused the canvas hides (HudCanvas_SetHidden, func_00233EF0).
- The jump is the cars' own physics: no boost and no ring collision (A09's five posts are on the start platform's
  edges). On the PS2 (race 0x2F forced over PINE, Cross held from the start) the save's car takes off at x 747, height 415,
  and falls past 275 at x 881, MISS: the port's trace matches it within a unit. Reaching the island (x 1211-1289,
  height 295.3) takes a faster car and the whole run-up.
- The talk (#7): 0 #8, 1 #12, 2 #9 (an item, stamp 0x5A).

## Beach Flag (race 39, course 0x25, ACTION/A19)

Micky's, Papaya Island place 13. Race_BeachFlagTask (func_002638C0); HudCanvas_InitBeachFlag (func_00235730). The
row's +0xC is Ai_Drive: Micky (body 0x33) drives AI path 0x25.
- State 0: the player at (354, 103.5, 769.2), Micky at (354, 103.8, 773.8), both facing −x, the flag behind them at
  (857.8, 103.21, 653.6); Micky steers at once. The countdown is kind 0.
- State 1: the player within 1.5 of the flag: WIN!, SE 0x302, Race_Outcome 2; Micky: LOSE, SE 0x302, 1 (both in
  one frame: both banners and 1). 2: no pause, race over, both braked; 3: from 0x40 the fade, 1 a frame to 0xC0.
- The flag is A19's obj0 (matrix 0), unturned, until someone reaches it.
- The talk (#7): 0 #8, 1 #9, 2 #10 (an item, stamp 78).

## Treasure Hunting Maze (race 41, course 0x12, ACTION/A00)

Fuji City's place 9. Race_MazeTask (func_0025F6A0); HudCanvas_InitMaze (func_00234D10). The car starts on the grid
(50, 20, 20) facing +z; the countdown is kind 0; the view is the mirrored one.
- State 1: inside D_002C4F50's square (48.5-54.8 × 346.4-353.6) SE 0x14, CLEAR, no pause, the brake; Race_Outcome 2
  under 10800 frames with stamp 14 earned, else 1. In the entrance (x 44-56, z 12 or under): 0. 2: the lid opens
  (180 to 225); 3: no pause; 4: from 0x40 the fade, 1 a frame to 0xC0. The end also sets race flag 0x400000.
- The chest, every frame: A00's obj0-2 turned −π/2 about y at (50, 20, 350), (50, 21.35, 350) and (50, 20, 350), the
  lid (1) also about z by its angle.
- The square lies behind the chest's front edge: a car driving straight in only gets there in the frame before the
  wall pushes it back. On the PS2 (race 0x29 forced over PINE, the car put at (51.5, 20, 335) facing +z with Cross held)
  it reaches z 346.598 at speed 6497 and CLEAR goes up; the port's wall is at the same place (both stop at 346.29),
  and a car as fast gets in the same way.
- The talk (Fuji City 9): the first visit gives stamp 13 and no game; the first win an item and stamp 14; a win in
  under 3 minutes stamp 15.

### Checked

In the port (`--advrace`, Cross held or `--place` near the end): the Drag Race (the lights, TIME, Johnny ahead, LOSE),
Rock Climbing (the climb on A03's new collision, GOAL!!), the Volcano Course (MISS in the lava, CLEAR with the lid and
wrench), Rainbow Jump, Beach Flag (Micky to the flag, LOSE) and the maze (CLEAR). On the PS2: Rainbow Jump and the
maze, as above.

## Curling (race 51, course 0x20, ACTION/A14)

White Mountain's place 18. Race_CurlingTask (func_002631A0); HudCanvas_InitCurling (func_00235460: +0x8008 =
0x20, the four points words −1). **The car is the stone:** it drives down the ice lane (−z), and over the line at
z 700 its brake goes on for good (0x800), so it slides on A14's ice (surface type 5) with the cars' own grip. No
countdown task: the task's banner and wait are the start, and there's no fade-in.
- State 0, each try: Car_Stop and Car_SetPlace at (875, 31.94, 792.5) facing −z, FIRST TRY / SECOND TRY / FINAL
  (Curling_SetTryBanner, func_00234618: picture 0 (0, 48 n, 256 × 48) in palette 0, zooming in over 10 frames in the
  countdown's slot), the HUD back (race flag 0x10000 off), Race_Outcome 0.
- State 1: after 120 frames the board (Curling_SetBoard), the banner gone, green and the pause (0x104), and on the
  first try the song (the task calls Sound_StartSong every frame until the line; the port starts it once). Over z
  700: the brake, state 2.
- State 2, once the car's speed across the ground is exactly 0: its distance from (875, 31.94, 525) under 2, 8, 15 or
  23 (D_002DA090) scores 100, 50, 20 or 10 (D_002DA0A0) and Race_Outcome 1 (Curling_SetPoints, func_002354F0: the
  try's points and the total of those over 0). Tries 1-2: the big score, state 3. The third: Curling_CheckRecord
  (func_00235570; save +0x12CC) and a win with a new record and stamp 70 earned, or 200 or more without it:
  Race_Outcome 2 and, 120 frames on, NEW RECORD (Hud_SetNewRecord, func_002347F0: picture 1 (0, 152, 252 × 40) in
  palette 12, zooming in). The win holds state 2 for those frames and scores again each frame, so with stamp 70
  earned the second frame's record check fails and it takes the loss (by the asm; not seen on the PS2). Else
  Race_Outcome 1, or 3 for a total of 300. No pause from here.
- State 3: 180 frames, then the HUD hidden, no green or pause; 4: the fade, 1 a frame to 0x80; 6: black, the car
  back at the start, the next try. After the third: 7 (race over, Sound_FadeOutSong) and 8 (the fade).
- **The board** (Curling_DrawBoard, func_00231208, picture 1): BEST RECORD (0, 216, 112 × 32) in palette 2 at (32,
  24), "Pts" small at (98, 65), the record in small digits (16 × 20 at (16 n, 194), palette 1) right to left from x
  82; the rows for the tries and the total in palette 3, the large digits (40 × 48 at ((n % 6) × 40, (n / 6) × 48 +
  56), 10 the dash) drawn 30 × 36 at (545 − 30 j, 32 + 36 k), three dashes for −1; the large "Pts" (200, 104, 56 × 48)
  drawn 45 × 36 at (575, 140); the 1, 2, 3 circles (112 + 24 k, 216, 24 × 24) in palette 11 at (458, 38 + 36 k). The
  big score: the try's points in 80 × 96 digits centred on x 320 at y 160.
- The talk (#7): 0 #8, 1 #9, 2 #10 (a new record; stamps 69 and 70 from the talk), 3 #19 ("Your first perfect
  score!").

## Ski Jumping (race 53, course 0x23, ACTION/A17)

White Mountain's place 7. Race_SkiJumpTask (func_00263E68); HudCanvas_InitSkiJump (func_00235638). The countdown
is the start lights.
- State 0: func_002080D8(1), the car at (790, 364, 1290) facing −z (the grid's place).
- State 1, the in-run (phase 0): z 1100 or less is the take-off; z 1280 or more with x 770 or less (off the start
  platform's left) is the quit, Race_Outcome 0. In the air (1): the jump is (int)(1100 − z) until the car's height is
  at or under the ground's below it (Collision_Ground), then no pause. The run-out (2): under 2 km/h across the ground
  the speed's x and z go to 0; each moves 0x98 towards 0 a frame; the brake. Stood still: SkiJump_CheckRecord
  (func_00235698; save +0x12C8), then NEW RECORD and Race_Outcome 2 for 140 metres or more with a new record or stamp
  0x42 yet to earn, else FINISH! and 1.
- State 2: race over; 3: the fade, 1 a frame to 0x80, the canvas lit over it.
- **The distance** (SkiJump_DrawDistance, func_002318B0, picture 1, from the first frame): BEST and the record as
  Curling's board has it, a small "M" (160, 194, 24 × 20) at (98, 65), a large "M" (200, 104, 48 × 48) in palette 4
  at (562, 24), and the jump in large digits in palette 4 at (522 − 37 k, 24) (0 while it's negative).
- The talk (#7): 0 #8 ("Are you quitting?"), 1 #9, 2 #10 (stamp 0x42 from #11).

## Fishing (race 54, course 0x24, ACTION/A18)

Shimisa's, Papaya Island place 15. Race_FishingTask (func_0025DBB8), a Fish_Task (func_0025D7C8) each for 30 fish;
HudCanvas_InitFishing (func_002356D0). The countdown is kind 1 (kind 0's numbers from picture 0). The car stays on
the grid (800, −38.5, 800), under the sea.
- State 0 (frame 0): func_002080D8(1); fish i starts at x = z = r % 160 + 720, heading r % 0xFFFF, with r = info+0 ×
  i × D_002C4BC0[i & 15] (Barrel Dodging's table).
- State 1: a catch (the task's +4 and +0x24, set by a fish) counts; 20 is FINISH! and Race_Outcome 1. 2: no pause,
  the brake; 3: from 0x40 the fade, 1 a frame to 0xC0, the count and FINISH! lit over it. The count (Fishing_SetCount,
  func_00235718) is set every frame.
- **A fish:** a unit over the sea floor (Collision_Ground). It turns 0x400 a frame towards its heading (snapping
  within 0x3FF), and when it faces it, picks a new one by the fence it's near (k = (info+0 & 0x7F) << 8: x under 720
  k + 0x4000, z under 720 k, x 880 or more k − 0x4000, z 880 or more k + 0x8000); while turning it's kept in the fence,
  and exactly opposite it takes (info+0 & 0xFF) << 8. It swims (−cos, 0, sin)(heading) × 0.08, 0.05, 0.2 or 0.15 by
  i & 3 (D_002C4BD0; sinf is func_00274A10, cosf func_00274938), nose first. The car within 2.5 after green catches
  it (SE 0x302), one a frame; after the 20th no more. Matrix i, A18's obj(i & 3) turned about y.
- **The count** (Fishing_DrawCount, func_00231C90, picture 1): the fish (160, 104, 56 × 38) and "×" (216, 104, 22 × 20)
  in palette 9 at (453, 40) and (512, 48), the count in large digits in palette 4 at (576 − 37 k, 34).
- The talk (#7): 0 #8, 1 #9 ("You caught 20 fish!", stamp 0x52).

### Checked in the port

Curling: the banner, the board, a slide to z 688.7 (0 points), the big score, the fade and SECOND TRY. Ski
Jumping: the lights, a 70 m jump (the take-off, the landing at z 1030, the run-out), the record and FINISH!.
Fishing: the fish, the count going up on a catch. Not compared with the PS2 yet.

## Which-way? Race (race 49, course 0x1E, ACTION/A12)

Dayan's, My City place 10. Race_WhichWayTask (func_00262678); HudCanvas_InitWhichWay (func_002353D0). Five rounds:
each puts the car on one of three stretches, past a colour plate, to a fork whose two signs show the plate's pattern
and a decoy; the sign on the right road shows the plate's. The countdown is kind 3 (kind 0's numbers on the seam,
y 224, even with one player), created a frame later than the other events' (the port doesn't keep that frame).
- State 1, a round: the stretch e = (info+0 >> 1) % 3; Car_ResetForRespawn and Car_SetPlace facing +z at
  D_002D9E70[e] ((99.9, 101, 55.8), (349.9, 101, 57), (700, 101, 55.9)), keeping the car's z speed. The right tile
  f = info+0 % count (+2 if it's last round's), the decoy (info+0 >> 8) % count (+2 if it's f), both then % count, so
  they can still be equal; the right road's side +1 (+x) on odd frames, else −1; the time 1800, 3600 or 1200 frames.
- State 2, playing: over the fork's line (D_002D9E40: z 534, 834, 434) no pause and the side line (D_002D9E10: x 100,
  350, 700) × the side ≥ 0 is the right road: round 5's is Race_Outcome 1, a wrong one 0 (and on to state 5). The
  time runs while green; at 0 TIME UP (Hud_SetTimeUp, func_00234B98: picture 0 (0, 0, 210 × 46) in palette 1,
  rocking, in the MISS COURSE! slot), Race_Outcome 0. TIME LIMIT shows the seconds (Hud_SetTimeLimit, func_002346F0).
  After the fifth, CLEAR at z 432 or more. After a miss: MISS, the car's speed z 0x23A0 and x 0, braked, until its
  height is 80 or under or z 100 or more.
- The plate and the signs: every frame of state 2 the task copies 64 × 64 tiles of the atlas (A12's 003, T8 512 ×
  256 at TBP 0x30DA) over A12's 000 (the plate), 001 (the −x sign) and 002 (the +x sign), with the course scrolls'
  copies (D_002DA030; `CourseObjects::CopyTile`): the right tile on the plate and on the right road's sign, the decoy
  on the other. The rounds' tiles (D_002D9F18): 4 plain colours, 6 halves, 12 of three colours (rounds 3 and 4), 8
  quarters.
- State 5: the round's fade out, 16 a frame; 7: black, then the next round (the pause back, no fade in); after the
  fifth the car (keeping its speed) at (860, 101, 406.8), after a miss at (875, 101, 35) rolling, the canvas cleared,
  and 6 fades in, 16 a frame. 3: race over; 4: the end's fade, 2 a frame.
- **TIME LIMIT** (func_00231F28, picture 0): the heading (0, 193, 124 × 20) in palette 7 at (258, 20), the seconds in
  picture 0's large digits (palette 3) drawn 30 × 36 from x 288 + 15 n right to left at y 40.
- The talk (#7): 0 #8 ("You're quitting already?"), 1 #9 (an item, stamp 47).

Checked in the port: the countdown on the seam, TIME LIMIT counting down, the plate's tile. Not compared with the
PS2 yet.

## Roulette (race 44, course 0x18, ACTION/A06)

Sandpolis' place 8. Race_RouletteTask (func_00260DC0), its canvas at the task's +0x20 (0x10D20 bytes). A betting
loop: on the board the player picks a bet and a wager, then drives the car off the hub of the spinning wheel in place
of the ball; the wheel catches it at its edge and carries it round, and the pocket it stops over pays. No countdown
and no pause (race flag 0x100 never comes). The song (BGM_05) starts at once and plays through the betting.
- **The main task:** 0, the song, the bet on BLACK (Roulette_InitBet, func_00262328) and the money to compare; 1,
  black, no green, Race_HideView (func_0024FB38: race flag 0x2000, no 3D view and the camera still, the cars' sounds
  muted), the board's task; 2, on its answer: quitting sets Race_Outcome by the money (Roulette_SetOutcome,
  func_00262458: 1 less than at the start, 2 more, 3 the same) and ends; a bet puts the car on the hub at (400, 5.25,
  400) facing +z (Car_ResetForRespawn, Car_SetPlace), clears the canvas (HudCanvas_InitRoulette, func_00235008) and
  sets the wheel's turn (info+0 % 20) × 0xCCC and speed ((info+0 >> 5) & 0xF) × 12 + 0x300. 3: 33 frames, then
  green. 4: off radius 11.7 the overhead camera (race flag 0x80000000), the HUD hidden and the brake. 5: the speed
  less 3 a frame, the car carried; under 0 the pocket under the car ((angle − turn + 0x666) / 0xCCC, the angle from
  +z towards +x) and its result (Roulette_SetResult, func_00262358). 7: 128 frames; 8: the fade over 64 frames, then
  the view off and the result's task; 9: its word: 0 LOSE, 1 WIN!, 2 both off, −1 black and the next round.
- **The wheel** (Roulette_Wheel, func_002609D0): the turn plus the speed each frame, matrices 0-3 all its turn about y
  at (400, 0, 400): A06's objects 0-3, the ring and the floor.
- **The ride** (Roulette_CarRide, func_00260B48): the car's radius about the hub drawn to 15 by 1/60 a frame, its
  angle turned by the wheel's speed ± 10 towards its pocket's middle; Car_SetPlace (func_0021C928: the position and
  yaw only) with its yaw turned by the speed too. Its own physics runs under it with the brake held.
- **The pockets** (D_002D9DB8): 0, 6, 8, 18, 10, 4, 12, 16, 14, 2, 0, 5, 7, 17, 9, 3, 11, 15, 13, 1 (0 is "0", "00" and
  the sliver at the end); black for an even pocket under 10 and an odd one from 10. A number pays 18 times, a sixth
  3, a colour 2.
- **The board** (Roulette_BetTask, func_00261AC8): SCOREA06 000 (640 × 448), the ball marker (001's top left 40 ×
  40) at the bet's place, blinking 16 frames in 32. The cursor (Roulette_MoveCursor, func_00261378) goes to the box's
  neighbour in D_002D9968 on a press, and on a repeat but where the box's no-repeat bit is set (SE 0x1D). Cross: the
  wager (SE 0x1A); Triangle: quit (SE 0x1B, the song fading). Under 10 money "You don't have the money to play."
  quits. The money box at (0x1B8, 0x1A) while the wager isn't up. Its end: the fade over 0x41 frames.
- **The wager** (Roulette_WagerTask, func_00261638): the amount a digit at a time (Left/Right the digit from the
  tens up; Up/Down by it, a fresh press wrapping between 0 and the most) up to the money or 10000; the window "How much
  do you wish to bet?", the amount and the currency, the caret, and the money after a win (Wallet_Draw). Cross: "Please
  make a bet." for 0, else "Start?" Yes (the bet paid, Money_Pay) or No; Triangle backs out.
- **The result** (Roulette_ResultTask, func_00261DE0): the board with the three winning bets' boxes from 001,
  blinking 32 frames in 64, and the marker; 128 frames (Cross or Triangle skips), then WIN!/LOSE; 128 more; the money
  after (Wallet_Draw), 192 frames, then paid (Money_Pay of minus the win, SE 0x305); the fade, and the next round.
  The canvas (WIN!, LOSE) is drawn over the board.
- **The overhead camera** (Camera_OverheadTask, func_0021ED90, under race flag 0x80000000): on course 0x18 the preset
  D_0029CCA0, (400, 50, 400) looking straight down, focal length 500.
- **The HUD's pictures** in SCOREA06 are its entries 2 and 3 (the uploads to TBP 0x2C36 and 0x2CB8), after the two
  boards; Golf's file has them at 11 and 12, Figure 8's at 7, 8 and 9 (`Hud_Picture`).
- The talk (#7): 1 #9 ("You lost some money"), 2 #10, 3 #14.

Checked in the port: a round from the board through the wager and Start?, the car caught on the wheel under the
overhead camera, the result board's lit boxes, WIN! and the money paid (990 to 1010 for 10 on BLACK). Not compared
with the PS2 yet.

## Figure 8 (race 45, course 0x19, ACTION/A07)

Sandpolis' place 6. Race_Figure8Task (func_0025A120), a Figure8_ConeTask (func_00259A08) per cone, the menu
(Figure8_SelectTask, func_0025AB28) and the results (Figure8_ResultTask, func_0025B0C0). A time trial round traffic
cones in five steps. The tables (the steps, their cones and gates, the 76 lines) are generated from the ELF:
`scripts/elf/gen_figure8.py` writes `race/figure8_tables.inc`.
- **The first frame** copies model 0 and its TEX0 over models 1-3 (every matrix draws the cone, A07's obj0).
- **State 0, the menu:** the block cleared, Figure8_InitMenu (func_0025B500: the cursor on the last step, the bests
  from the save +0x12B4, the goals D_002C3AF8: 20 to 60 seconds), the menu's task, Race_HideView (no 3D view), the
  car stopped. On the PS2 the map and the canvas are off over the first menu too (race flag 0x10000, though nothing
  in the task sets it then). **5:** on its answer, quit (the end), or the step: the jet fuel 0x40000, the car at its
  start (D_002C3A18) facing +z at height 1 (Car_SetPlace), the cones at their homes (D_002C3A00, height 1.5), the
  canvas cleared (HudCanvas_InitFigure8, func_00235050), BGM_05 and a new countdown (the race's start lights,
  Race_CountdownTask, from the next frame: the fade in, the lights, green at 240, the song and the pause at 300).
- **1, the run:** the time from the first gate (TIME). For the gate n the car's on (car +0x24A), gates[step][n] is
  {a, sign, b, sign, c, sign} of the lines D_002C3250: past a (the side test × its sign ≥ 0) is the next gate; past b
  or c is MISS COURSE! (its 0x19 sprite), Race_Outcome 0 and the time 0; back over the last gate's a is the last
  gate again. The step's gate count reached is FINISH!, Race_Outcome 1, and with no cone hit stamp 32 + step. A
  cone's signal: 1 (a touch) SE 0x314, a hit counted and PYLON TOUCH! for 128 frames; 2 only the sound.
- **2:** race over, the brake, the song fading; **3:** the fade over 128 frames, then the times recorded
  (Figure8_RecordTime, func_0025AA58: the time up to 360000, the hits up to 99, the total + 300 a hit; a total under
  the best is the save's), the results' task, MISS COURSE! and FINISH! off, the view and the HUD off; **4:** on their
  answer, green and the brake off, the menu again.
- **A cone** (0x40 bytes at canvas +0x8CB0 + 0x40 j): the car within 1.5 of it (in x, y and z) tells the task (1 if
  it was at rest, else 2) and, by the side the car came from (0.6 past its edges) and its speed: 1-14 km/h wobbling
  (±0x80 a frame about z or x, 16 frames) and sliding (the car's speed / 152 × 0.01 a frame, less 0.008 of it a frame
  till it turns); 15 on tipping (0x400 a frame till 0x4800: down for good), hopping (the speed × 0.003 up, less 0.008
  a frame, bouncing at half 8 times) and sliding. Settled with the car 1.5 from its home, it's back there, but still
  turned as it lay. Its matrix: Rz Rx at its place.
- **PYLON TOUCH!** (Hud_SetPylonTouch, func_00234C80): picture 1 (0, 96, 160 × 32) in palette 2, standing at (320,
  412), on while the count's bit 0x10 is set (16 frames in 32).
- **The map** (RaceMap_DrawStep, func_00242A88): the race's map with the step's view (D_002A42E0) and SCISSOR_2 x
  48-208, field lines 68-200 (D_002A4430; Golf's map too), the car and the cones as yellow markers.
- **The menu:** the cone tile (SCOREA07 006) repeated over the screen, the title (005) at (50, 34), the step's map
  (000-004) at (50, 100); a window of STEP1-STEP5 (style 2, at x 340, half-line 44, 260 wide) with each step's best;
  a window with the goal time, the step's description and "5 sec penalty / for pylon hit" in palette 6. Cross plays
  the step, Triangle quits (SE 0x1A, 0x1B); 17 frames on, the answer. **The results:** the same background, STEP n and
  "Course missed!", or the time, the cones hit ("%2dtimes/+%3dsec"), the total and best (blinking in palette 6 when
  new) and the goal time; Cross or Triangle.
- **SCOREA07**'s HUD pictures are its 7, 8 and 9.
- The talk (#7): 0 #8 ("Retire? That's too bad..."), 1 #9 and on by the stamps 32-36.

Checked: the first menu and a step's start against the PS2 (race 0x2D forced over PINE): the menu's places match, with no
map over it; the step's map, TIME and start match. In the port: a miss (MISS COURSE!, "Course missed!"), a cone hit
(knocked flying, PYLON TOUCH! blinking).

## Golf (race 43, course 0x17, ACTION/A05)

Jumbo's, Mushroom Road place 3. Race_GolfTask (func_00251B60). Nine holes where the car is the club: driving into
the ball hits it with the car's speed, the camera follows the ball till it stops, and the player drives on to it. Each
hole has a card first and a scorecard after; the round's score is the strokes against par (36). No countdown. The
tables (the holes, the surfaces' bounces, the cards' texts) are generated from the ELF: `scripts/elf/gen_golf.py`
writes `race/golf_tables.inc`.
- **Frame 0:** the view off with the cars muted, black. **Frame 1:** object 0's TEX0 becomes A05's picture 17 (the
  dimpled ball) in place of 18 (`CourseObjects::SetModel0Picture`). Object 1 (the ball's shadow) keeps 18, which
  modelconv's file has no material for (CourseObjects gives it object 0's picture on this course).
- **The holes** (D_002AC0B0, 64 bytes each): the car's start, the tee, the cup, the car's yaw, par. The yards are
  (the tee to the cup across the ground − 8.5856) / 0.9144.
- **The states:** 0 the card (Golf_InitCard, func_00252608: the bests from the save +0x12D0, +0x12D9); 2 the view off
  and the hole's card; 3 its answer; 10 the view on, the car and ball placed (Golf_PlaceHole, func_00250F00: the car
  stopped at its start with full jet fuel, the ball on the tee 0.45 up), the HUD off, no green or pause, ROOM_1, the
  panel's values (HudCanvas_InitGolf, func_00234E98); 11 black for 9 frames; 30 the fade in over 64 frames, then the
  HUD, green, the pause, the panel and the song; 31 32 frames; 33 green and no brake; 32 the shot (Golf_CheckHit:
  green off, the car coasts); 34 32 frames on the ball camera (race flag 0x80000000) and the brake, the car's nudges,
  the ball, and when it stops: in the cup (the yards to go at 0) no pause, the chase camera, SE 0x319 for a hole in
  one, the panel off, CUP IN and the result word (36), else 35: 66 frames, the stroke counted, the chase camera, 33
  again. 36: 193 frames; 37 the fade; 40 the rooms' song (0x0A0B0102), the hole recorded (Golf_RecordHole), the
  scorecard; 41 its answer, the next hole or the end.
- **The shot** (Golf_CheckHit, func_00250FE0): the car within 2 of the ball (in x, y and z) hits it with its speed:
  under 50 km/h (0.231481 a frame) a soft shot along it at 1.2 times plus 20 km/h (SE 0x318); else hard, 1.8 times,
  lifted by (the speed over 50, at most 200 km/h) × 180 / 0.925926 × 0.00272222 (SE 0x317). The sounds wait 10 frames
  apart.
- **The nudge** (Golf_CarTouchBall, func_00251190): once the car has been 2.2 from the ball since the shot, coming back
  within 2.2 of it while it isn't rising sets it off at 15 km/h at least along its way (or away from the car, or the
  car's back) with a hop of 0.108889; until the next shot it stops dead on landing (but on surface type 5).
- **The ball** (GolfBall_Step, func_00251440): moved by its speed, then the ground under it (Collision_Ground with the
  winning triangle's normal: `Collision::Collision_Ground`'s third argument). No ground: its speed across turned
  back. In the air (over the ground + 0.45): drag 0.993 across and gravity 0.00272222 (in water 0.993 × 0.99 and
  0.00266805). On the ground: pushed out along the normal, reflected off it, then the surface's (across, up) pair
  (D_002AC2F0: type 1 0.6/0.5, type 4 the flagsticks 0.1/0.1, else 0.97/0.7), × 0.99 in water. Landing after 16
  frames in the air: within 1.8 of the cup SE 0x31B, within 8.5856 SE 0x31A. The water's splash (0x31C) going in and
  out. Its two axes turn with its rolling. Under 4 km/h for 121 frames it's stopped; within 8.5856 of the cup it's in.
- **The ball camera** (Camera_OverheadTask, func_0021ED90, on course 0x17): the preset D_0029CC80 from the ball's
  place: 20 to −z and 2 up, looking +z pitched 0x1000 down, whatever the hole's direction.
- **The objects:** matrix 0 the ball (Matrix_Billboard of its axes) at its place; matrix 1 the shadow, turned to the
  ground's normal under the ball, 0.05 up.
- **The panel** (Golf_DrawPanel, func_00230520): at (40, 20) the ball (picture 0, palette 3), the hole, its yards and
  "Y" in picture 1's 20 × 30 digits (palette 1, (20 n, 145)), "PAR" (palette 4) and the par; at (522, 20) "LAST", the
  yards to go and "Y", "SHOT" and the stroke being played.
- **The banners:** CUP IN (Hud_SetCupIn, func_00234F68: picture 1 (0, 104, 180 × 40) in palette 3 at (320, 112)) and
  the result word (Golf_SetResultWord, func_00233338) at (320, 224): a hole in one's burst and HOLE IN ONE!, or by
  strokes − par ALBATROSS!, EAGLE, BIRDIE, PAR, BOGEY, DOUBLE BOGEY, TRIPLE BOGEY. They take the shared list's
  +0x82B4, +0x82E0 and +0x830C.
- **The map** (func_00242A88): course 0x17's own view, clipped as Figure 8's, the car and the ball.
- **The cards** (Golf_HoleIntroTask func_00252880, Golf_ScoreCardTask func_00252CB0): the golf tile (SCOREA05 010)
  repeated, the Golf bar (009) at (50, 34); the hole's map (000-008) at (78, 80), its number, yards, par and
  description; or the scorecard of each hole's par, score and best and the totals (0x7F "---"), new bests blinking.
  Cross or Triangle (SE 0x1A), then 17 frames.
- **The record** (Golf_RecordHole, func_00252738): the hole's score (strokes + 1 − par), the round's total, new bests
  into the save; after the ninth Race_Outcome 1 over par, 2 at or under.
- **SCOREA05**'s HUD pictures are its 11 and 12.
- The talk (#7): 0 #8, 1 #9 (stamp 93), 2 #10 (stamps 93 and 94).

Checked against the PS2 (race 0x2B forced over PINE): the first hole's card and its start (the panel, the map's strip, the
ball and the flag) match. In the port: a shot, the ball camera, the ball stopping at 132 yards to go, SHOT 2.
