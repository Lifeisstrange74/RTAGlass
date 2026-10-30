# The events

The events are the mini-games 2 Player offers on rows 5-9 (two_player.md), which the Adventure also uses: Highway
Race, Tunnel Race, Sliding Door Race, Obstacle Course and Soccer. Their courses are the ACTION files, course ids
0x12-0x26. This doc covers what they share and each event: the Tunnel Race, the Sliding Door Race, the Obstacle
Course, the Highway Race and Soccer, all ported.

## The event courses (ACTION/Ann.BIN)

- **The files.** The loader's table D_0029BE50 (`{handle, LBA, sectors}` by course id) puts ids 0-0x11 on COURSE/C00-C14
  and C18-C20, and ids 0x12-0x26 on ACTION/A00-A20 in order (checked by LBA and size: 0x14 is A02, 0x1C is A10).
- **The layout** is the race courses': u32 offsets {GS uploads, field, collision (PMP), map, objects...}.
  Loader_LoadCourse reads a map only for ids below 0x12 and 0x13, 0x14, 0x16, 0x17, 0x19, 0x1A, 0x1C, 0x25 and
  0x26. On the others the objects start a word earlier. `tools/modelconv` converts all 21 (`models/ACTION/Ann/`,
  `collision/Ann.pmp`).
- **The executable's tables run over all 0x27 ids:**
  - the grid (D_002A7F28) and the line (D_002A8198);
  - the clock (D_002A88E8), the lights (D_0029E200, 0x30 bytes each), the fog (D_0029EA10) and the car colour
    sets (D_0029EC90, `{id, set 2, set 3}` entries looked up by id);
  - the map's view (D_002A4050) and the trackside cameras (D_002A2A50; every event course has the default list
    D_002A2A10, but 0x18).

  The port's tables now have every id. The AI paths (D_002BD578) exist for 0x13, 0x14, 0x16, 0x1C, 0x25 and 0x26.
  Path 0x13's speeds are D_002ABE80 and the later paths' D_002ABF80 (Ai_Init). They're converted to `ai/Ann.txt`.
- **The grid** (Car_Init): only courses below 0x12 put the slots in rows 5 apart. On an event course every car
  stands on the first row, the odd slots 7.5 to the side (PCSX2: the Tunnel Race's two cars at x 536.5 and 544, both
  at z 693).
- **The song** (Race_Task's first frame): the race courses' table D_0029F5B0 below 0x12; course 0x1C has BGM_11
  (0x0A160201); the other event courses ROOM_1 (0x0A0D0201); the Adventure's row 0x1A (Endurance Run against one car)
  has BGM_12 (0x0A170201).
- **The HUD's colours.** Race_Setup's loader request (the switch on the course id: 0x4000D by default) loads the HUD
  file from the SYS table D_0029AF58. Entry 0x0D is SCORE.GSL. An event course gets entry 0x29, 0x07, 0x1A-0x1D,
  0x12, 0x10, 0x11, 0x1E-0x28 by id, which are SYS/SCOREA00-A20.GSL in order (checked by LBA where the files'
  first bytes differ). A16 (0x22) has none and keeps SCORE.GSL.
  - The files have SCORE.GSL's pictures with other palettes. A10's palette 0 is green where SCORE.GSL's is pink, so
    the Tunnel Race's places are green (`ps2_tunnel_start.png`). PCSX2's GS memory at CBP 0x2CB6 holds A10's colours
    during that race.
  - The port picks the file in Race_Setup (`Hud_SetCourse`, `hud_parts.cpp`).
- **The view.** RaceView_Init gives courses 0x12, 0x1C and 0x20 RaceView_DrawMirrored (func_00220FE8). It is
  Race_DrawView without the sky (0x20 still draws it) and with Cars_DrawMirrored (func_00222B70) for the cars.
  - Cars_DrawMirrored draws each car first upside down: the axes' heights are negated and the height is mirrored
    about the ground under the first corner probe (+0x144). The light matrix's second row (the lights' heights,
    PCSX2: the matrix is stored transposed) is negated too. It draws at distance 0.03 (the far body, separate tires).
    The see-through floor shows it as a reflection.
  - The port: `Course::MirrorsCars`, `Cars_Draw(..., mirrored)`.
- **No course objects** on 0x1C (Course_ObjectsTask's table has nothing for it).

## Tunnel Race (course 0x1C, ACTION/A10)

2 Player's row 6: two cars, two laps, flags 7 ("Best Total Time" on the card). The Adventure's row 0x26 (D_002BE018
+ 3 rows): two cars, one lap, against an opponent (list D_002BDD78) driven by Ai_Drive on path 0x1C. Both run
Race_Task, the ordinary race.

- **Car_Crash's tunnel branch** (the port's Car_CrashTunnel). On course 0x1C, Car_Crash runs its own branch every
  frame, hit or not, after the usual step back for a hit:
  - The car's velocity in its own frame gets a set bounce by the corners hit (jtbl_002EAE20):
    - front left 0x8E3 sideways and the heading + 0x200; front right the opposite;
    - front left with the side (7): also 0x11C7 back;
    - a rear corner or a side: 0x11C7 sideways and the forward speed halved;
    - the whole front or back: 0x11C7 back or forward;
    - all four corners: race flag 0x200.
  - Back in the world, it moves the car once more by 16/25 of it and becomes its speed across the ground. So in the
    tunnel the cars move twice as far a frame. PCSX2's trace (over PINE) shows two moves
    of velocity × 16/25 each frame, and a wall at 7.18 s: the step back, the bounce, the turn.
  - A hit plays ACTION 0x11 (Sound_PlaySe 0x8311, then 0x311) for a player's car (flags & 3, not with race flags
    0x48), at most every 0x20 frames (+0x212 counts down in Car_Update). The pad's shake is the usual one.
- **In the Adventure,** Car_Init gives the player (mode 0 on course 0x1C) the car of D_0177EA5C (D_0177EA50 entry 3,
  record lane 3) with the parts of the course row's list. In the port Race_CreateAdventureCars and
  Race::StartAdventure do this (Akiban's record (9, 18), body 12).
- **2 Player's results:** Race_SetupTask runs Versus_ResultsTask after every 2 Player race. Rows 5-9 have no screen:
  it runs Results_UpdateRecords at once (row 6 decides the winner by place), sets the overlay black and race flag
  0x400000, and returns. PCSX2 (`ps2_tunnel_finish.png`): FINISH! in both halves, the fade, NOW LOADING over the
  darkened race, the course select on the Tunnel card.

### The port

`game/race` runs it as a 2 Player race (`Versus_RowIsRace`). The pieces:
- the course from ACTION/A10 and the tables above;
- the side-by-side grid;
- BGM_11;
- SCOREA10's HUD;
- the mirrored cars without the sky;
- Car_CrashTunnel with its sound.

After the finish, Versus_SaveRecords and the loader (`versus.cpp`).

Checked against PCSX2 (`captures/events/`):
- the grid's frame (`ps2_tunnel_start.png`, `port_tunnel_start.png`): the places, the colours, the cars' places,
  the lights;
- the doubled move and the bounce in the trace;
- the end (`ps2_tunnel_finish.png`, `port_tunnel_finish.png`);
- the card's records after it.

## The event tasks

The Sliding Door Race, the Obstacle Course and the Highway Race don't run Race_Task: Race_Setup creates the course
row's own task
(+8) in its place, after the cars' tasks, Cars_Collide and Meter_Draw. So there are no laps, clock, places, map or
replay. What they share:

- **The countdown.** Race_Setup's countdown task (func_002357C0, Race_CountdownForCourse) picks
  Race_EventCountdownTask (func_00233A80) for the event courses, with a kind by course: 0x13 kind 0, 0x1D kind 2.
  Courses 0x12, 0x1B, 0x1F, 0x21 and 0x25 also have kind 0, 0x18 kind 0 without the pause or the song, 0x20 and
  0x24 kind 1, 0x14, 0x15 and 0x1D kind 2, and 0x1E kind 3.
  - It fades in over 0x40 frames, beeps (CQ_MAIN 0x2D) at 60, and gives green (race flag 4) at 240.
  - Every 60 frames it puts up a number at the canvas's +0x80A4: the sprite list [t / 60 − 1] of "3", "2", "1",
    "GO!". Each zooms in over 10 frames to twice its size, centred on (320, 112), or (320, 224) on the seam in
    2 Player.
  - Kinds 0, 1 and 3 use D_0029F7E8: picture 1 at v 48-56, palette 1 (kind 1: picture 0; kind 3 always on the
    seam). Kind 2 uses D_0029F868: picture 0 at v 208-216, palette 13.
  - At 300 the number goes, the song starts and race flags 0x104 are set (the pause comes with 0x100).
- **The HUD canvas** is cleared by the event's own init: HudCanvas_InitSlidingDoor (func_00234D58,
  +0x8008 = 0x13), HudCanvas_InitObstacle (func_00235378, 0x1D) or HudCanvas_InitHighway (func_00234DB0, 0x14). HudCanvas_Draw then draws only what the event
  sets. Each player has a WIN! element at +0x86F0 + 0x408 p and a LOSE / MISS element at +0x871C + 0x408 p.
  They're centred at x 320, y 168 with one player, 112 and 336 with two. Hud_DrawElements draws them:
  - **WIN!** (Hud_SetWin, func_00232D68): picture 1 (0, 144, 160 × 48), palette 12. It is type 3: it zooms in over
    30 frames to twice its size.
  - **LOSE** (Hud_SetLose, func_00232F08): on 0x13 picture 1 (0, 0, 176 × 48) in palette 2; elsewhere
    (0, 192, 168 × 48) in palette 13. Soccer's (0x1A) is (0, 48) in palette 1.
  - **MISS** (Hud_SetMiss, func_002349C8): (0, 192, 176 × 50), palette 13.
  - **MISS COURSE!** (Hud_SetMissCourse, func_00233158) is a third element per player, at +0x8748 + 0x408 p: on
    0x14 (0, 0, 256 × 36) in palette 3, on 0x19 (0, 56, 256 × 36) in palette 1, elsewhere (0, 48, 256 × 50) in
    palette 1. Type 2, rocking.
  - LOSE and MISS are type 2: they rock by ±15 × 0.002 radians over 60 frames. PCSX2 shows them twice their size, as
    big as WIN!.
  - The pictures are the course's own SCOREAnn.GSL (`a01_p1`: LOSE, 3-2-1 GO!, START, WIN!, MISS; A11: FINISH!,
    WRONG WAY, FINAL LAP, WIN!, LOSE).
- **Meter_Draw** still draws each player's meter (routine 10 in 2 Player).
- **Car flag 0x800** makes Car_Input keep the steering and hold the brake (input & 0xFF00 | 2). The events put it
  on a car that's done.
- **The end.** The task sets the race's flag 8 and returns −1 to Race_SetupTask, which goes to the results (2 Player:
  Versus_ResultsTask, no screen on rows 5-9) with Race_Outcome as the event set it.
- **The fade and the banners.** Each task calls Fade_SetOverlay and HudCanvas_Draw itself, and the later call draws
  on top. The Sliding Door task draws the canvas first, so its fade covers WIN! and LOSE (`ps2_sliding_win.png`). The
  Obstacle Course and the Highway Race set the overlay first, so their banners stay lit over the black
  (`ps2_obstacle_goal.png`, `highway_miss_ps2_port.png`). The meters (Meter_Draw, earlier) are under it either way.

## Sliding Door Race (course 0x13, ACTION/A01)

2 Player's row 7: two cars, one lap (none counted), flags 0. The card says "Come to a complete stop on the goal. You
fail if you stop before the goal, or fall into the pond." The Adventure's row 0x23 has the same task, against an
opponent on AI path 0x13. Race_SlidingDoorTask (func_002595F0):

- **The start.** The first frame puts car 0 at (720, 6, 97.5) and car 1 at (720, 6, 102.5), facing −x
  (Car_SetPlace, func_0021C928). Car 1's +0x19A is set to 1, so the AI opponent steers from the start. The song is
  ROOM_1 (0x0A0D0201).
- **Each frame** SlidingDoor_CheckCar (func_00259498) checks each car that hasn't failed, until there's a winner:
  - x < 175 or y < 0 (the pond) fails.
  - Moving (its velocity +0xF0..+0xF8 not exactly 0): nothing.
  - Standing still inside the goal (222.5 < x < 235.75, 83 < z < 117) wins: +0x25 = car + 1, if no one has yet.
  - Standing still short of the goal, past x 710, fails (+0x27 |= 1 << car). At x 710 or more it's still at the
    start: nothing.
  - A car that won or failed gets flag 0x800 (the brake).
- **Phase 1** goes on until there's a winner, or with one player the player failed, or both failed. Each player who
  failed shows MISS in their half.
  - With one player, the opponent is stopped dead past x 231 (in the goal): velocity 0 and flag 0x800.
- **Phase 2** (one frame):
  - Race_Outcome = the failures, then, with a winner, winner ^ 3 (2: player 1 won, 1: player 2).
  - Two players: the winner's WIN!. If the loser hadn't failed, their LOSE and their brake.
  - One player: WIN! if the player won; LOSE if the opponent won and the player hadn't failed.
  - The pause goes (race flag 0x100 cleared).
- **Phase 3:** a count from 0 holds 0x40 frames, then fades to black over 0x80 (Fade_SetOverlay, over everything).
  Then the end.
- Both missing ends with Race_Outcome 3: nobody wins (Results_UpdateRecords' win needs 1 or 2).

## Obstacle Course (course 0x1D, ACTION/A11)

2 Player's row 8: two cars, one lap, flags 1 (the track length on the card). The Adventure's row 0x30 has the same
task alone. Race_ObstacleTask (func_0025EB70) runs a state machine for each player (+4, +6), each with its own fade
count (+0, +2):

- **Start (state 0):** player 1 at x 162.5 and player 2 at 167.5 (x 165 alone), y 134, z 102.5, facing +z. The car's
  +0x24A (the AI zone byte; the course has no path) is its gate, 0. The song is BGM_05 (0x0A100201).
- **Racing (2):**
  - The gates are lines D_002C4C50[0..14] ({x0, z0, x1, z1}). On gate n, being past line n + 1 (the cross product
    ≥ 0) moves the car to n + 1, and being back over line n moves it back.
  - At or under the height D_002C4F00[n] (bytes 0, 100, 60, 60, 0, 10, 0, 0, 0, 50 × 5) the car fell. It gets flag
    0x800 and state 5, or 3 alone; the pause goes. Player 1's fall also sets Race_Outcome 0.
  - Line 14 is the goal:
    - Two players: the winner's WIN!, the other's LOSE, both braked, both to state 3, Race_Outcome 2 for player 1
      or 1 for player 2.
    - One player: FINISH!, Race_Outcome 1, or 2 if the time (+0xC, counting from the first gate while green) is
      7200 frames or under.
  - Alone, going back over D_002C4F10 on gate 0 ends it (Race_Outcome 0).
- **3:** no pause (player 1's also fades the song out, Sound_FadeOutSong), then 4.
- **4:** the count goes up 2 a frame. Past 0x40 it darkens the player's half (Fade_SetTopOverlay func_0022EA78,
  Fade_SetBottomOverlay func_0022EAA0; alone, Fade_SetOverlay) by count − 0x40. At 0xC0 it's the end.
- **5:** the same fade, then **1:** the car is put back:
  - Car_ResetForRespawn (func_0024FB80, Car_Stop func_0021C998), then Car_SetPlace at D_002C4D40 (player 1) or
    D_002C4E20 (player 2) by its gate, 4 higher, its yaw the entry's 4th float;
  - the brake comes off;
  - **6** fades back in (the count down 2 a frame from 0xC0), then racing and the pause again.
- **Two players**, each frame: a car still on gate 0 that's past x 238.5 goes back two moves, its engine at 0 (a
  fence at the start).
- PCSX2 (`ps2_obstacle_goal.png`): the half overlays are under the canvas. WIN! and LOSE stay lit while the view
  fades; Fade_SetOverlay covers everything.

### The port

`race_events.cpp`: Race_TaskFor picks the task by course, so the Adventure's rows will get it too. Race_EventStart,
Race_EventCountdownTask, Race_SlidingDoorTask (SlidingDoor_CheckCar) and Race_ObstacleTask (Obstacle_PlayerTask)
run as above.
- The race's per-car `braked_` is flag 0x800.
- The elements are `HudElement`s (`hud_parts.cpp`, Hud_DrawElement), and the HUD is `RaceHud::DrawEventMeters` and
  `DrawEventElements`.
- `RaceResult::outcome` carries Race_Outcome to Versus_SaveRecords.
- `Car_Reset` is the respawn's Car_ResetForRespawn and Car_SetPlace.
- One player's TIME (Hud_SetTime, func_00233FC8: the TIME heading at (40, 20) over a running time row at (40, 52))
  and FINISH! (Hud_SetFinish, func_00234128: picture 1 (0, 0, 210 × 56) in palette 0 at +0x8590, zooming in to its
  own size at y 112) are `Race::Event_SetTime` and `Event_SetFinish` (the Adventure's race 48: `--advrace 48 --gate
  13 --place 1461 65 370 32768` reaches the goal).

Development: `--place X Y Z YAW` now also wins over an event's start, and `--gate N` puts player 1 on gate N of the
Obstacle Course (on the Highway Race, in AI zone N). On the PS2, moving the car and writing +0x24A over PINE does the same.

Checked against PCSX2 (`captures/events/`):
- **Sliding Door Race:** the countdown (`ps2_sliding_countdown.png`); player 1 put at rest on the goal: WIN! and
  LOSE, the fade, the loader (`ps2_sliding_win.png` / `port_events_win.png`); player 1 at rest at x 500: MISS, player
  2 racing on (`ps2_sliding_miss.png`); both cars into the pond (`port_sliding_miss.png`).
- **Obstacle Course:** driving off the first stretch: the fall, the half's fade, the car back at the gate's point
  (`ps2_obstacle_fall.png` / `port_obstacle_fall.png`); the goal from gate 13 (`ps2_obstacle_goal.png`); the card
  after it, one attempt each and player 1's score (`obstacle_card_ps2_port.png`).
- `race_sliding.p2s` and `race_obstacle.p2s` are 2 Player Race Right-Away at each event's countdown.
- **The courses' own parts:** the Sliding Door Race's seven doors (SlidingDoor_DoorTask, race.md
  "Course objects"; `sliding_doors_ps2_port.png`), the Obstacle Course's scrolling textures and 2 Player fence
  (`port_obstacle_fence.png`), and its cars' jet turbines: 2 Player's row 8 has part flags 0x2000, so both cars
  carry them and R2 fires them (Car_Jet and Car_DrawJet, car_physics.md and effects.md;
  `obstacle_jet_ps2_port.png`). Quick Race's Tin Raceway card has them too (`port_tin_jet.png`).

## Highway Race (course 0x14, ACTION/A02)

2 Player's row 5: two cars, one lap, flags 3 (the length and the laps on the card). Night, on an elevated highway
with two ways round: the AI path forks after zone 0x14, and the two forks meet again before the start. The Adventure
has the same task with an opponent (car 1, driven by Ai_Drive on path 0x14). Race_HighwayTask (func_0025FF00):

- **The first frame:** the canvas (HudCanvas_InitHighway), BGM_05 (0x0A100201), the countdown task (kind 2, as the
  Obstacle Course's: D_0029F868), the 0x20-byte state at +0x8CB0 cleared, player 2's state at −1 with one player, and
  car 1's +0x19A (D_0177B05A) at 1 so the opponent steers from the start.
- **State 0** (player 1's) puts car 0 at (412, 53, 871) and car 1 at (412, 53, 875.2), both facing −x (yaw 0xC000).
  Player 2's state 0 puts car 1 there again. Both start in AI zone 0x55.
- **State 1, racing.** The car's AI zone (+0x24A; Ai_TrackZone runs for the players' cars too) and z (+0x98):
  - **Off the course:** z under 806 in zones 0x13 and 0x54, or 994 or more in zones 0x14, 0x40 and 0x41 (past the
    highway's edge where it runs along those lines). MISS COURSE! for that player. Two players: the other's WIN!,
    Race_Outcome 1 if player 1 missed, 2 if player 2 did. One player: Race_Outcome 0.
  - **The goal:** zone 0x55 with the car's bit in +0x10 set. The bit is set in zone 0x14 and cleared in any zone
    below it, so the car must have come round. Two players: the winner's WIN!, the other's LOSE, Race_Outcome 2
    (player 1) or 1 (player 2). One player: WIN! and Race_Outcome 3 if the time (+0xC) is 3000 frames or under,
    else 2. If the opponent got there first (player 2's slot at 5, set by player 1's part when car 1 reaches zone
    0x55 with its bit), LOSE and Race_Outcome 1.
  - Either ends both players' states at 2. Player 1's part runs first, so it wins a tie.
  - One player's time counts while green and not paused (Hud_SetTime, func_00233FC8, shows it).
- **State 2:** race flag 8 on and the pause (0x100) off, both cars braked (flag 0x800), then 3.
- **State 3:** the count (+0 player 1, +2 player 2) goes up 2 a frame. Past 0x40 Fade_SetOverlay((count − 0x40) <<
  24), over the whole screen. At 0xC0 the end: Sound_MuteCars, black, race flag 8.

### The port

`race_events.cpp`: Race_HighwayTask and Highway_PlayerTask as above, Event_SetMissCourse (a third element per player).
`--gate N` on this course starts player 1 in AI zone N: `--place 470 30 1000 3515 --gate 20` is off the course in
zone 0x14 at once.

Checked against PCSX2 (`captures/events/`):
- **The start:** the countdown's numbers on the seam and the cars' places (`highway_start_ps2_port.png`).
- **MISS COURSE!:** player 1 put at (470, 24, 1000) with zone 0x14 over PINE (the position,
  then +0x24A): MISS COURSE! for player 1, WIN! for player 2, the fade with the banners lit, the loader
  (`highway_miss_ps2_port.png`).
- **The card after it** (`highway_card_ps2_port.png`): one attempt each, player 2's course and total score 1.
- **The goal**, port only: both cars on `--autodrive 0`. Player 2 took the other fork and reached zone 0x55 first
  (about 3600 frames): WIN! for player 2, LOSE for player 1.

`race_highway.p2s` is a Race Right-Away Highway Race just after green, both cars still on the start.

## Soccer (course 0x1A, ACTION/A08)

2 Player's row 9: six cars (both players and their teammates, so only when both have a second teammate), flags 0.
The card says "3-on-3 Scoccer game. Whoever scores 3 points first wins." The Adventure has it with one player.
Race_SoccerTask (func_00254430) runs in place of Race_Task, with no countdown task of its own: its kick-offs do the
countdown's work. The cars the AI drives use the row's AI routine, Soccer_AiRoutine (func_00254CC8), not Ai_Drive.

- **The block** (canvas +0x8CB0, car +0x258 points at it): +0 the state, +4 its frame count, +8 done, +0xC the pause
  hid the canvas, +0x10 the team that scored last, +0x14 and +0x18 the goals (team A, team B), +0x20 the ball (0x50
  bytes), +0x70 a 12-byte AI slot per place {mode, input bits, gear}.
- **The places** (D_002AC480, 64 bytes each; car +0x24A is its place): the kick-off heading and spot (+0, +0x10),
  then for the AI a home spot (+0x20) and an x range and reach (+0x30, +0x34, +0x38):

  | place | kick-off | home | x range, reach |
  |---|---|---|---|
  | 0 | (180, 205) facing +x | (200, 200) | 80-320, 25 |
  | 1 | (180, 195) facing +x | (260, 200) | 140-320, 25 |
  | 2 | (100, 200) facing +x | (100, 200) | 80-170, 10: team A's keeper |
  | 3 | (220, 195) facing −x | (200, 200) | 80-320, 25 |
  | 4 | (220, 205) facing −x | (140, 200) | 80-260, 25 |
  | 5 | (300, 200) facing −x | (300, 200) | 230-320, 10: team B's keeper |

  With one player the cars take places 0-5 in order (the player in place 0). In 2 Player car i takes i / 2 + 3 (i &
  1): player 1, player 2, then the teammates in turn, so player 1's team is A and player 2's B.
- **The states**, each counting its frames and going on to the next on the same frame:
  - 0: race flag 4 off and 0x10000 on (no map, meters or canvas); every car Car_ResetForRespawn and Car_SetPlace'd to
    its place, braked (0x800); the ball at (200, 2, 200) at rest, its axes up and +z.
  - 1: 8 frames black (Fade_SetOverlay). 2: fading in over 0x40 frames ((0x40 − n) << 25), then 0x10000 off and the
    score small at the top (Hud_SetScore mode 1). 3: 0x20 frames, then KICKOFF! and its whistle (ACTION 0x0C,
    0x30C). 4: 0x80 frames, then race flags 0x104 (green and the pause), the brakes off, KICKOFF! gone, and the song
    (ROOM_1, loaded on the first frame) the first time only.
  - 5: play. The ball's x past 320 is team A's goal, under 80 team B's: no pause (0x100 off), ACTION 0x0F (0x30F),
    the goal counted, GOAL!!, the score off.
  - 6: 0x80 frames, then GOAL!! off and the score large in the middle (mode 2). 7: 0x80 frames, then the score off.
    At three goals: the winner's WIN! and the loser's LOSE (2 Player: in each half; one player: the player's) and
    Race_Outcome 2 (team A) or 1 (team B), then 9. Otherwise 0x10000 on, every car braked, and 8.
  - 8: fading to black over 0x80 (n << 24), then 0 again. Green stays on until then, so the cars can drive while
    GOAL!! shows.
  - 9: 0x100 frames, then the banners off, 0x10000 on and the pause off, every car braked, Sound_FadeOutSong. 10:
    fading to black over 0x80, Sound_StopSong, and the end (race flag 8).
  - While the race is paused (0x8000) the task hides the canvas (HudCanvas_SetHidden) and does nothing else.
- **The ball**, every frame it isn't paused:
  - **Ball_HitCars** (func_00253B80), each car: within 2.5 of the ball's centre, the ball takes the direction
    from the car's centre at 0.7 of its speed across the ground, plus the car's velocity (fixed point) × 3.5e-5.
    If the car's centre is within 2 across the ground, the push is at least 0.0926 and the ball hops (0.1089 up);
    a car over 50 km/h also makes it hop. A new touch sets the kick flag and shakes the pad of a car whose flags
    (+0x198) are 0 or 2: player 1's car only (PCSX2: 0x802 braked and 0x843 for player 2's).
  - The drag: × 0.993 above height 2, × 0.97 on the ground (x and z). Then the move.
  - **Ball_HitWalls** (func_00253E90): the ends at x 82 and 318, open between z 182 and 218 back to the nets at 62
    and 338; the sides at z 122 and 278. A wall turns that speed back at 0.7; in a corner the ball goes out the
    shorter way. Above height 2 it falls 0.0027 a frame faster; at 2 it stays there and bounces at 0.6.
  - **Ball_Spin** (func_00254290): its two axes turn about z by −vx and about x by vz (radians, wrapped), so it rolls.
  - The sound: a wall or a new touch plays ACTION 0x0A (0x30A), at most every 10 frames.
  - Its matrix (Matrix_Billboard on its axes, at its centre) is D_0177EB10[0], which Course_DrawObjects draws as
    A08's object 0, the ball.
- **The HUD** (unless 0x10000): the meters (Meter_Draw), the map (RaceMap_Draw, RaceMap_DrawCars) with the ball as
  a yellow marker (RaceMap_DrawObjects, func_00243398: D_002A43A0), and the canvas. The canvas's file is
  SCOREA08.GSL: picture 0 holds only 40 × 48 digits in two colours, picture 1 WIN!, LOSE, KICKOFF! and GOAL!!.
  - The score (Hud_DrawScore, element type 0x00230C20 at +0x8438): team A's goals in palette 1 (red), a dash in
    palette 2, team B's in palette 0 (blue). Mode 1 is at (320, 44), or (560, 44) in 2 Player, digits 40 either side;
    mode 2 is at (320, 224) at twice the size, 160 either side.
  - KICKOFF! (Hud_SetKickoff, +0x82B4: (0, 96, 232 × 54) palette 2) and GOAL!! (Hud_SetGoal, +0x82E0: (0, 152, 232 ×
    48) palette 3) zoom in to twice their size at (320, 224). WIN! is (0, 0, 160 × 48) in palette 0, LOSE (0, 48,
    176 × 48) in palette 1 (Hud_SetWin's and Hud_SetLose's 0x1A branches).
- **The AI** (Soccer_AiRoutine, func_00254CC8): no input before green; else Soccer_CarAi (func_00253680) on the car's
  slot, which returns the slot's input bits:
  - The ball is "in play" for a car while its x is inside the place's x range, or it's within the place's reach.
  - Mode 0 goes for it (Soccer_Attack, func_00253448). The goal it attacks is x 320 for team A, 80 for team B, at z
    200. It aims 2.3 behind the ball on the line from that goal. Facing along the ball's line to the goal (within
    0x800) it drives at up to 100 km/h, else flat out from further than 20 and 45 km/h nearer. Out of play: mode 1.
  - Mode 1 drives back to the home spot, 10 km/h within 20 of it, braking there to a stop; then mode 2 turns to the
    kick-off heading at 10 km/h and stops. The ball in play sends either back to 0.
  - Soccer_Steer (func_00253230) works the slot's gear. Gear 0 brakes until the car is stopped, then picks forward
    (the angle within a quarter turn) or reverse. Gear 1 accelerates, steering while the angle is over 0x80, and goes
    back to 0 past 0x6000. Gear 2 reverses, steering the other way while the angle is over 0x2000, and goes back to 0
    within it. Over the cap it only steers.
- **The camera** (race.md): course 0x1A has its own chase views, further back (D_0029CE00 8 and 9).
- **2 Player's records:** no results screen; Versus_ResultsTask's records with Race_Outcome, as the other events.

### The port

`race_soccer.cpp`: Race_SoccerTask, the ball (Ball_Step: Ball_HitCars, Ball_HitWalls, Ball_Spin), the AI
(Soccer_CarAi, Soccer_Attack, Soccer_Steer) and the canvas's setters. The ball goes to the course objects as object
0 (`CourseObjects::SetObject`). The HUD adds Hud_DrawScore and the map's ball marker (`MapMarker::kBall`), and the
chase camera has views 8 and 9.

Checked against PCSX2 (`captures/events/`):
- **The kick-off** (`soccer_kickoff_ps2_port.png`): the cars and the ball in their places, KICKOFF!, the score, the map
  and its ball, the camera.
- **The state timings** over PINE (setting the state to 0, so a kick-off starts on the next
  frame; `ps2_soccer_kickoff_trace.csv`): 8, 64, 32 and 128 frames, then green.
- **The AI** from that kick-off, both players idle (`soccer_ai_kickoff_ps2_port.png`): the four AI cars' paths in the
  port lie on the PS2's, within about a unit for the first 3 seconds, through their first touches of the ball.
- **The end:** both teams set to 2 and the ball put past x 320 over PINE: GOAL!!, the large 3 − 2, WIN! and LOSE in
  the two halves with the map and meters up, the fade with the HUD off, the loader (`soccer_win_ps2_port.png`); then
  the card (`soccer_card_ps2_port.png`): one attempt each, player 1's course score.
- With both players idle, the AI teams play on their own and score now and then: a goal after about 7000 frames on
  the PS2 and 8700 in the port.
