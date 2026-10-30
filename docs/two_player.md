# 2 Player

Notes for the 2 Player mode: what the PS2 does, from the main menu to the results, and a plan for the port. The
menu, Versus_Task's flow, the course select, the split-screen race (rows 0-4) and its results, and the Tunnel Race,
the Sliding Door Race, the Obstacle Course (rows 6-8, events.md), the Highway Race (row 5) and Soccer (row 9) are in
the port (see "The port so far").
Everything is from the decomp (m2c drafts, checked against the asm where unclear) and the ELF's
data, except where it says PCSX2. **(G)** marks a guess; everything else was read in
the code.

The race mode is pad +0x0A: 2 = Race Right-Away, 3 = Random Race, 6 = Custom Race (mode & 2 is "2 Player" throughout
the race code).

## Front end

### The menu: Title_VersusMenuTask (func_00209848)

Picking 2 Player in the main menu (Title_MainMenuTask, title.md) opens a window with Race Right-Away, Random Race and
Custom Race (strings D_0029ACF8). Triangle plays 0x1B and goes back to the main menu (TitleSeq_Task(2)).

- **Race Right-Away (mode 2).** Save_InitBoth, then Save_InitVersusPreset (func_00227B28) fills both records with a
  preset. There is no memory card and no name entry. It returns 3.
  - Names "PLAYER 1" / "PLAYER 2" (D_002EB480 / D_002EB490).
  - Bodies: player 1 0x2D, their teammates 0x49 and 0x68; player 2 0, their teammates 0x69 and 0x8D. Paints are from
    D_002ED720.
  - Teammates (+0x674..+0x677): player 1 (1, 0x21) and (7, 0x0B); player 2 (7, 0x1F) and (4, 0x1A). Both players
    have a second teammate, so Race Right-Away always has Soccer (row 9) and its races are 6-car team battles.
  - Mode 2 never saves: its course select has no Save&Exit card.
- **Random Race (mode 3) and Custom Race (mode 6).** Each player brings their Adventure save.
  - Mc_MessageTask (func_00227380(2)) shows "Insert memory card… into MEMORY CARD slot 1 for player 1, and into
    MEMORY CARD slot 2 for player 2." with OK / No. No goes back to the menu.
  - Mc_LoadPlayerTask (func_00228F40) loads record 0 from slot 1, then record 1 from slot 2.
  - Any failure shows a message and goes back to the menu: "not inserted into slot 1/2", "no data saved … slot 1 /
    2 / 1 and 2", "no memory card in slot 1 and 2", "Load failed in Player 1/2". There are no default cars.
  - When both loads work: SE 0x303, then Versus_LoadedTask (func_0022A050) shows "Data has been loaded." with each
    save's summary (Save_DrawSummary, func_00227F90). It returns 3.
  - **A PS2 bug:** Versus_LoadedTask clamps the license (+0x651) to 3 for player 2, then again at an address past
    both records (0x6EE1). Player 1's license is never clamped. The port doesn't need to copy this.
- **Random vs Custom.** The only difference found is the course select's cards: Custom adds the parts card (0x19).
  Nothing randomises the course, laps or cars. **(G)** The English names are loose; "Custom" fits the parts card.

Result 3 makes the root (func_00205088) start **Versus_Task (func_00211320)**, the 2 Player task.

### Versus_Task (func_00211320)

On its first frame:
1. Versus_FindRivals (func_00237AC0) finds or adds each player's rival entry for the other (save +0x678, keyed by
   +0x3438). The indexes go to pad +0x1C (player 1's list) and +0x1E (player 2's).
2. D_0177EA50[0..5] point at the six cars: player 1, teammate 1, teammate 2, then the same for player 2. The loader
   (0x70006) loads them.
3. It starts the course select on card +0x26.

After a race it reloads (loader 5) and goes back to the course select on the same card. Exit or End Game plays
QuickRace_ExitFadeTask, and the root starts TitleSeq_Task(3).

- Versus_FindRivals: in each record, the first entry whose name is empty, or whose id is the other record's
  `+0x3438`, whichever comes first. A new entry gets the id and the other's name (`+0x62C`). With all 20 taken it's
  entry 19, renamed "ANYBODY" (D_00333870, 8 bytes) and nothing else changed.
- Loader 5 has bit 0 set, so the miniatures come from SYS/S01.E3D (the Quick Race's loader 4 reads S00.E3D;
  course_select.md).

### The course select

This is the Quick Race's CourseSelect_Task (course_select.md) with another table and other cards.

| Mode | Card mask | Cards |
|---|---|---|
| 2 | 0x43FF | rows 0-9, Exit (0x1E) |
| 3 | 0xA3FF | rows 0-9, Sell parts (0x1B), Save&Exit (0x1F) |
| 6 | 0xB3FF | rows 0-9, the parts card (0x19), Sell parts (0x1B), Save&Exit (0x1F) |

- In all three modes, row 9 (Soccer) is dropped unless both players have a second teammate (record 1's +0x676 and
  record 2's, +0x3ABE, both non-zero; CourseSelect_Task). Race Right-Away's preset sets both (7 and 4, func_00227B28),
  so it always has Soccer. With Random and Custom it depends on the two saves.
- Both pads move the carousel and pick a card. CourseInfo_Task takes either pad's Cross or Triangle; so do
  CourseSelect_QuitTask and its Dialog_YesNoTask (+0x0C = 2 with mode & 2).
- The slots and cards are the Quick Race's: bit n of the mask is slot n; slots under 10 are rows; 12 is id 0x19,
  13 is 0x1B, 14 Exit (0x1E) and 15 Save&Exit (0x1F). The child by slot: 12 CourseSelect_VersusPartsTask, 13
  CourseSelect_SellPartsTask, 14 CourseSelect_QuitTask, 15 CourseSelect_SaveExitTask, else CourseInfo_Task. The
  result to Versus_Task is the slot (+0x26), so it comes back to the same card.

**The rows: D_002BE1D8**, 10 × 16 bytes, laid out like the Quick Race's D_002BE158. Each row has the course id, cars,
laps, flags, an opponent list (always 0 here), the race task and the AI routine. The course names are D_002BE398
(`char*` by course id).

| Row | Id | Course | Cars | Laps | Flags | Race task / AI |
|---|---|---|---|---|---|---|
| 0 | 0x11 | Endurance Run | 6 | 2 | 0xF | Race_Task / Ai_Drive |
| 1 | 0x04 | Desert Raceway | 6 | 2 | 0xF | Race_Task / Ai_Drive |
| 2 | 0x05 | Night Glow Raceway | 6 | 2 | 0xF | Race_Task / Ai_Drive |
| 3 | 0x0A | River Raceway | 6 | 2 | 0xF | Race_Task / Ai_Drive |
| 4 | 0x0D | Sunny Beach Raceway | 6 | 2 | 0xF | Race_Task / Ai_Drive |
| 5 | 0x14 | Highway Race | 2 | 1 | 3 | 0x0025FF00 / none |
| 6 | 0x1C | Tunnel Race | 2 | 2 | 7 | Race_Task / none |
| 7 | 0x13 | Sliding Door Race | 2 | 1 | 0 | 0x002595F0 / none |
| 8 | 0x1D | Obstacle Course | 2 | 1 | 1 | 0x0025EB70 / none |
| 9 | 0x1A | Soccer | 6 | 1 | 0 | 0x00254430 / 0x00254CC8 |

- **The flags:** 1 shows the length, 2 the laps, 8 TEAM BATTLE (course_select.md), and 4 makes the card say " Best
  Total Time".
- Rows 0-4 have course miniatures (S01.E3D) like the Quick Race. Rows 5-9 have picture cards.
- Rows 5-9 are the 2 Player events (events.md). Row 6 (Tunnel Race) uses the ordinary Race_Task on course 0x1C,
  the course with its own crash physics and sound (events.md, ported). The others have their own tasks: Highway Race
  0x0025FF00, Sliding Door 0x002595F0 and Obstacle Course 0x0025EB70 (both ported, events.md), and Soccer 0x00254430
  with its AI 0x00254CC8. The Highway Race and Soccer are ported too (events.md).

**The course box, CourseInfo_DrawCourse in 2 Player.** No "Entry n cars" line (mode 1 only). TEAM BATTLE (flag 8,
when both players have a second teammate) at cell (0x13, 6), palette 0 or 6 by bit 4 of the frame counter (pad +0),
so it flashes. Courses 0x13, 0x1A and 0x1D print up to five lines in green from cell (0x10, 4): D_002A8F70 ("Come to
a complete / stop on the goal. / You fail if you stop / before the goal, / or fall into the pond."), D_002A8F84
("3-on-3 Scoccer game. / Whoever scores / 3 points first wins.", the game's spelling) and D_002A8F98 (one empty
line).

**The card's info, CourseInfo_DrawVersusRecords (func_00247E38).** A frame of cells (1, 10)-(29, 17); player 1's
numbers end at x 0x96 and their times start at x 0x5A, player 2's at 0x1FE and 0x1C2.
- Both players' names (func_0023DB00).
- Attempts: each player's races on the row, summed over the 20 rival entries (+0x678 + 0x26).
- Each player's best total (+0xA38 + 0x20 row + 0x1C) and best lap (+0x18, only when there are 2 laps or more).
- "Course Score": the wins on this row against the current rival (+0x1C + row). "Total Score": the wins on all 10
  rows.

**The other cards.**
- **0x19, the parts card (Custom), CourseSelect_VersusPartsTask (func_00248580).** A list of the six cars, with ""
  for a missing teammate. Pad 1's Cross picks items 0-2 and pad 2's items 3-5 (passed as (n − 3) | 0x10). It opens
  the parts shop (func_0022BB88) for that car, then func_0020AD18 reloads the car. The window is at (0x40, 0x84),
  on either pad, from the task's 17th frame; the car the cursor was last on (if it's there) stands on the right
  (Versus_DrawCar, func_0022C280). In 2 Player the parts screen's Meter row has no carousel. Triangle on either pad
  goes back. (PCSX2: captures/versus/ps2_parts_list.png, ps2_parts_window.png.)
- **0x1B, Sell parts, CourseSelect_SellPartsTask (func_00248398).** "Sell Player 1 parts." / "Sell Player 2 parts.",
  then the selling shop (TradeShop_Start, func_00269E50, mode flag 2): that player sells their parts to the other
  (parts.md, "7. The shops"). The card in PS2's English is "Trade Items"; both players' money shows at the top
  (Versus_DrawMoney, func_0022EDB8). (PCSX2: ps2_trade_*.png.)
- **0x1F, Save&Exit, CourseSelect_SaveExitTask (func_00248BF0).** Save Player 1 (slot 1), Save Player 2 (slot 2) and
  End Game (save.md), in a window at the top right (Window_Place 6) on either pad.
  - A save item needs that player's own pad's Cross; the other pad's buzzes 0x1C. Each save runs
    Save_VersusSaveTask (func_00229758(p)).
  - End Game asks "Quit Games" (Dialog_YesNoTask). Yes (SE 0x1A) leaves the course select.
  - When a save or the dialog ends, the window takes the input back with SE 0x1B (even after "Saved."), and after a
    save the cursor moves down one. Triangle (either pad) goes back to the carousel.

## The race

### Setup

- **Race_Setup with mode & 2:** the row is D_002BE1D8[card]. It calls Race_CreateViewTasks(…, 1), the split screen,
  then Race_CreateCars(row, teams, 1). Teams means both players have a second teammate.
- **Race_CreateCars in 2 Player.** There are **no course opponents**: the opponent list is skipped (asm 0x0020F014).
  The Car_Init word is: bits 0-4 the car's index, 5-9 its D_0177EA50 entry (also +0x194, the AI lane), 10-14 the
  slot (+0x247), 16-23 the flags.
  - Player 1: flags 0x02, entry 0. Player 2: flags 0x43, entry 3. Player 2's car is D_0177AEC0.
  - Without teams (or on a 2-car row) the race has two cars: player 1 in slot 1 (0x020400), player 2 in slot 0
    (0x430061).
  - With teams, on a 6-car row: player 1 0x021400 (slot 5), player 2 0x431061 (slot 4), then player 1's teammate 1
    (0x900022, entry 1), player 2's teammate 1 (0xD00483, entry 4), player 1's teammate 2 (0xA00844, entry 2) and
    player 2's teammate 2 (0xE00CA5, entry 5). The teammates' slots count up from 0, skipping one that's missing
    (teammate 1 needs +0x674 / +0x3ABC).
  - The car flags: 0x01 is pad 2, 0x40 is player 2's side, 0x02 is a human, 0x10/0x20 teammate 1/2. None of the six
    has a cap (the cap code needs !(flags & 0x33)).
- **Which save car and which parts (Car_Init, asm 0x002184CC-0x0021863C).** The save car comes from the flags: car
  (flags & 0x30) >> 4 of record (flags & 0x40 ? 1 : 0). Its first 12 bytes (paint, body ...) are copied to car
  +0x188. The physics parts record (+0x180):
  - modes 2 and 3: **D_002BE278 + 0x10 x row**, the same for all six cars. Every row is `00 tt 04 01 03 02` (tires
    2, 7, 1, 6, 6, 2, 0, 3 for rows 0-7, 2 for row 9); row 8 is `00 08 05 02 03 02` with part flags 0x2000. So
    Race Right-Away and Random Race don't use the saves' parts at all;
  - mode 6 (Custom): the save car's own parts (+0xC);
  - (mode 1, the Quick Race: D_002BE318 + 0x10 x card.)
  - Car_SetParts gives a teammate (flags & 0x30, player 2's too) grip + grip/16 and mass - mass/4.
  - The horn is the parts' +0xD (0 in D_002BE278) + (flags & 1).
- **The teammates' speed tables** (Ai_Init): record +0x1300 (+0x3448 for flag 0x40), +0x1000 for teammate 2, +
  0x100 x D_002AC080[course] (0x11: 15, 4: 4, 5: 5, 0xA: 10, 0xD: 12). Nothing copies the defaults in at race start
  (Save_Init and recruiting a teammate do). Ai_Drive's learning (ai.md, "Marks") runs in 2 Player too, so both
  players' teammates learn into their records; modes 3 and 6 save that through Save&Exit.
- The laps are the row's. Race_InitTimes sets D_00335700 = 0: no lap is ever the brighter chime's (0x2F) or a
  highlighted lap row.

### Two views

Race_CreateViewTasks mode 1 creates Course_ObjectsTask, Camera_ChaseTask(2), Camera_ChaseTask(3), RaceView_Init(1)
and RaceView_Init(2). One player uses Camera_ChaseTask(0) and RaceView_Init(0).

- **RaceView_Init (func_00221300)**, the draw-view task's init. Its argument (+0x1C) does two things.
  - It picks the camera's matrix block, D_0177E6D0 + 0x190 × max(arg − 1, 0).
  - It is the split mode Race_DrawView reads: 0 full screen, 1 top, 2 bottom.
  - It also picks the drawer by course: −1 func_00220938, 0x22 func_00220B20, 0x12/0x1C/0x20
    RaceView_DrawMirrored (func_00220FE8, events.md), else Race_DrawView.
- **Camera_ChaseTask's argument.** `arg & 1` is the player: it selects the camera D_0177E9F0 + 0x20 i, the matrix
  block, the car D_0177AC50 + 0x270 i, the tilt follower D_0172FF30 + 0x10 i, and the camera button from that
  player's key setup on pad i. `arg & 2` adds 2 to the view index.
  - Both players get view 2 (chase) and view 3 (driver's), or 6/7 with part flag 0x400. Triangle switches each
    player's view separately.
  - After the finish the camera keeps chasing. The switch to Camera_FinishTask needs `!(mode & 2)`.
- **The split is top and bottom.** D_0029D020 holds three SCISSOR_2 packets in field lines: mode 0 is x 0-639, y
  0-223; mode 1 is y 0-111; mode 2 is y 112-223. That's two 640 × 224 halves of the 448-line frame. After view 2,
  Race_DrawView puts back entry 0. Both views go out in one VU1 packet.
- **The projection.** Camera_SetScreenCenter keeps x at 2048. y is 1992 for view index 2 and 2104 for 3 (D_00332C4C),
  so ±56 field lines: the centres are (320, 112) and (320, 336) on the frame.
  - Each chase view is eye (0, 1.2, −7), focal 461, pitch 0x600. The driver's view is (0, 1.5, −0.25), focal 500.
  - The y scale, the clip and the frustum are as for one player (Camera_BuildMatrices has no split input). Only the
    scissor crops, so each half shows about half the vertical field of view. A PCSX2 frame of the race matches the
    port's drawn this way (`captures/versus/race_er.p2s`).
- **Per view:** the sky, the highlight (Light_SetHighlight with that camera), the field cells culled by that camera,
  the cars, Course_DrawObjects, block 64 and Cars_DrawEffects. Lighting and fog are shared. The cars'
  LOD (view z over that camera's focal, against D_00332CF4) and the far clip are as for one player.

### The HUD

One canvas, drawn once over the whole screen. Each player has a block of 0x408 bytes (player 2's is at +0x408), and
HudCanvas_Draw always draws both.

| Element | Player 1 | Player 2 |
|---|---|---|
| LAPTIME heading | (490, 20) | (490, 234) |
| Lap rows | y 52, 76, 100 | y 266, 290, 314 |
| Best lap pointer (unused: the asm never reads it) | D_017807A0 | D_01783BE8 |
| Position | (40, 88) | (40, 400) |
| Teammate positions (hidden until set) | (128, 88), (128, 104) | (128, 400), (128, 416) |
| WRONG WAY / its arrow | (320, 135) / (320, 92) | (320, 359) / (320, 316) |
| FINISH! and the other banners | y 112 | y 336 |
| Meter | routine 10 at (492, 112) | routine 10 at (492, 336) (D_002A9AF0[split + player]) |

- Hud_SetPosition maps car flags to slots: 0x02 (0, 0), 0x10 (0, 1), 0x20 (0, 2), 0x43 (1, 0), 0x50 (1, 1), 0x60
  (1, 2).
- **Shared:** the TOTALTIME heading (20, 20) and the clock (60, 52), one, in the top half; one set of start lights at
  (320, 224), on the seam; one minimap, at the one-player place.
- **The minimap:** player 1's arrow is red (D_002A4370) and player 2's cyan (D_002A4380). Teammates are squares,
  cyan with car flag 0x40 (player 2's) and red without (player 1's). The port's minimap draws every teammate cyan; fix
  it with the 2 Player work.
- **Meter routine 10 (func_0024E1A0, Meter_Draw10)** is used in the split screen whatever the meter part
  (ITEM/METER10.GSL), with its top left at D_002A9AF0[split + player]. A finished car's meter isn't drawn. In its
  drawing order:
  - the fuel bar's track (picture 2 palette 0, 100 x 18 at +7, +76), then the bar, picture 1 in palette 15 - f
    (each palette a shorter cut of it), f = ceil(fuel x 15 / 0x40000); at f = 0 EMPTY (picture 2 at +30, +82, uv 0,
    24, 58 x 8) in palette (frame >> 3) % 3 + 1;
  - the plate, picture 0 (132 x 96), whose fuel frame has a window the bar shows through;
  - the rpm: n = ceil(rpm x 16 / 10000) slices of 4 x 60 at +34 + 6i from u 136 + 6i (each holds its part of the
    arc);
  - the mph (km/h / 1.609), up to three digits of 20 x 26 (u 20 d, v 102) at +78, +59, +40, y +49;
  - with the fins (part flag 0x1000, which Meter_Draw passes as the routine's last argument), an arrow by their
    setting (Meter_Fins, func_002495C0, of +0x244): -1 the up arrow (picture 0, u 240 v 0, 16 x 16, D_00327FB0) at
    +9, +12; 1 the down arrow (u 240 v 24, D_00327FD0) at +9, +36; 0 none. (This doc had it as a "gears part".)
  - the gear in the white circle: picture 2 palette 4 at +10, +62, cell g + (g != 0) of 16 x 10 at v 32 (R, then
    1st on; what looks like "7" at the start is the italic 1).
- **The teammates' places** are Hud_DrawPosition's small form (cars 0): the suffix at +30 and 16-pixel pink digits
  (v 64) at +14 (tens at +0), SCORE.GSL picture 0 palette 0. Hidden until Hud_SetPosition sets them.
- **FINAL LAP is never shown in 2 Player:** Race_CompleteLap only sets it with `!(mode & 2)`.
- **FINISH!** goes up in each player's half at their own finish (Hud_SetFinish(0) / (1)).

### Player 2's car

- Car_ReadPad reads pad `flags & 1` with the key setup D_01780A48 + 0x3448 (flags & 1): player 2 uses their own
  record's key setup.
- Car_Sound uses engine `flags & 1` (SNDMOD's voices 2 and 3), the skid sound id + 1 and skid slot D_00335728[1].
- The vibration goes to pad `flags & 1` (effects.md).
- The pause opens from either pad's Start (pad +0x30 or, with mode & 2, +0x32 & 0x800). Race_PauseTask's window
  reads only the pad that pressed it (mode 8 or 9; pad 2 if both pressed on the frame). Its items are Continue and
  Retire.
- Car_ReadPad's key setup is record (flags & 1)'s (+0x12E8); the camera button (buttons[7]) is read by
  Camera_ChaseTask on pad (arg & 1).
- Cars_BumpSound plays 0x14 and shakes the pad for any human car not finished (flags & 0xFB0), player 2's too; in a
  bump between the two players both get it.

### The finish and the results

- Race_Task's +0x25 bits: player 1 bit 0, their teammates bits 1-2, player 2 bit 3, their teammates bits 4-5.
- At each car's finish: its place (+0x247), its total time (D_01786FF0[3 x side + member]), the horn stopped, and
  the AI drives it on held to 100 km/h (+0x246 = 0x28, flags 0x280).
- **The race ends when both players have finished** ((+0x25 & 9) == 9), after a 0x80-frame fade. It sets +0x25 to
  0x3F at once, which stops the clock and ends the teammates' race too. There's no skip and no timeout. **(G)**
  Nothing else ends it except Retire.
- Race_SetupTask in 2 Player mutes the cars and ends the race tasks, then runs **Versus_ResultsTask (func_00235B58)**.
  There is no replay. Race flag 0x10 (Replay_Save) still goes up on player 1's last lap, but nothing plays it.
- **Versus_ResultsTask, rows 0-4.**
  - The course name at (230, 27), then the columns ENTRY NAME / RANK / PTS / TIME / Bestlap.
  - Versus_DrawRow (func_002359E8) draws each row: the name, the place " 1st".."6th" or "retire", 6 − place points,
    the total time (D_01786FF0) and the best lap. A row is at y = 0x41 + 12 (member + 3 × player).
  - **With teams** (TEAM BATTLE and both second teammates): all six rows, then "TOTAL POINTS": player 1's team gets
    18 − the sum of its three places (from 0), "vs", player 2's team 21 − that.
  - **Without teams:** 21 to player 1 if they placed ahead of player 2, else 0.
  - 11 or more: "PLAYER 1 WIN!", else "PLAYER 2 WIN!", flashing palette 0/6 every 16 frames. Race_Outcome
    (func_0023D8C8) is set to 2 for player 1, 1 for player 2.
  - Either pad's Cross saves the records (below), sets race flag 0x400000 and returns the outcome. There's no sound.
    Versus_Task's loader (request 5) then darkens the results under NOW LOADING and the course select comes back
    on the same card.
  - The places are D_01787008[car] +2, car = member x 2 + side (the creation order). The teammates still racing
    when the race ends keep the place they had then. Their total time is D_01786FF0[3 x side + member], which only
    the finish sets, so theirs show "--'--"--" (and their best lap too, with no lap done): `ps2_results.png`.
  - The best lap is D_01787068 +0xC [3 x side + member]: Race_CompleteLap keeps it for the players and teammates
    (car flags 0x73), the lowest of the race's laps.
  - **The backdrop** (func_00235880 builds the packet, sent each frame): the Takara checker, SCORE.GSL picture 1's
    64 x 64 corner at (192, 192) in palette 6, as a 320 x 228 sprite repeating it (TEX0 reads T8, but the packet's
    TEX2 makes it T4 with CSA 6). PCSX2 shows it over the whole frame, each tile 128 x 125.75 pixels. Then the
    banner from picture 0 palette 2: its left end (u 0, v 0xD0, 64 x 48) at (140, 40), the 8-texel middle column
    (u 0x40) repeated over 280 at x 204, the right end (u 0x48, 16 x 48) at x 484. The screen cuts in after the
    race's fade, with no fade of its own.
- **Rows 5-9** have no results screen. Versus_ResultsTask saves the records at once, with the outcome the event
  left in D_00335704 (the Tunnel Race's go by place), and keeps the screen black (PCSX2: the loader comes over the
  darkened race).

### The records: Results_UpdateRecords (func_00237C48), mode & 2, row r

- Each player's best lap and best total (+0xA38 + 0x20 r + 0x18 / + 0x1C) keep the minimum.
- Both records' top-3 lap and total lists are updated, player 1's time first, then player 2's. An entry is tagged
  0xFF for the record's own player, else the rival index (pad +0x1C in record 1, +0x1E in record 2). A time goes in
  before the first entry that's empty (24 bits 0) or slower, the rest moving down; a tie goes after.
- On each side's rival entry: races + 1 (+0x26 + r), and the winner's wins + 1 (+0x1C + r), both capped at 255.
- Row 6 decides the winner by place; the other rows by Race_Outcome (2 = player 1).
- Saving them to the cards is only through Save&Exit (modes 3 and 6). Mode 2's records live in memory only.

## The port so far

- **The menu** (`title/title_menu.cpp`): Title_VersusMenuTask with the three items at x 0x17C, over the darkened
  title as the Adventure menu is. Triangle goes back to the main menu.
  - Race Right-Away: Save_InitBoth and Save_InitVersusPreset (`save/save_data.cpp`).
  - Random and Custom: Mc_MessageTask's card question (OK / No, Triangle is No), then both records are read from
    `save1.json` and `save2.json` (Mc_LoadPlayers, after Mc_LoadPlayerTask). A missing file is "no data" in that
    slot, an unreadable one "Load failed". As on the PS2, with no `save1.json` it also tries slot 2, to pick "slot 1"
    or "slot 1 and 2". The messages keep the game's wording.
  - Then SE 0x303 and Versus_LoadedTask: "Data has been loaded." with Save_DrawSummary for each player. The port
    clamps both players' license to 3 (the PS2's player 1 is missed, a bug).
  - Then the root's Versus_Task (`game/versus/versus.cpp`) takes over with the race mode.
  - The windows read either pad (+0x0C = 2), as on the PS2.
- **Two pads** (`platform/input.cpp`): pad 1 is the keyboard and XInput controller 0, pad 2 is XInput controller 1,
  each with its own d-pad repeat and motors. `Input::Held/Pressed/Repeated(pad)` take 0, 1 or `kEitherPad`; a
  Window's `pad` is +0x0C's bits 0-1. Pad 2's vibration goes to its own controller. `--press N:2x` presses on pad 2.
  - Which windows read which pad (WindowAlloc clears +0x0C to 0, pad 1): the main menu, Options and its three
    windows, the 2 Player menu, Mc_MessageTask's messages 0-9 and Versus_LoadedTask's set 2 (either). Messages 12 on
    come in pairs and read their player's pad (number & 1). The Adventure menu stays on pad 1.
  - Title_MainMenuTask: Adventure, Quick Race and Results check pad 1's Cross (+0x30 & 0x40); from pad 2 they buzz
    (SE 0x1C) and the menu stays. 2 Player and Options take either. The port counts pad 1's Start too (Start
    confirms on PC).
  - Everything a single player does (the press-start screen, the course select, name entry, the race and its
    pause) reads pad 1, as before.
- **Versus_Task** (`game/versus`): Versus_FindRivals (`save_data.cpp`), the loading screen, the course select,
  Exit's fade back to the title. A course row (0-9) loads its race; after it (or Retire) the
  course select loads again on the same card. Every row is in.
- **The split-screen race** (`game/race`, `Race::StartVersus`):
  - Race_CreateVersusCars (`car.cpp`) makes the field above from the two records: players first, then the
    teammates (when both players have a second one), each with its record's body and paint.
  - The parts: D_002BE278's row in modes 2 and 3, the cars' own in mode 6, with the teammates' grip and weight
    (`Car_SetParts`). The teammates drive by their saved speed tables (`Ai_SpeedTable`) and learn into them as
    they drive (Ai_Drive's marks, ai.md), which Save&Exit saves.
  - Each player has their own pad (`Car_ReadPad(input, pad)` through their record's key setup, `buttons`), chase
    camera (views 2 and 3, centred in their half; their camera key on their own pad), engine and skid loops
    (`CarSounds` with the player's number), horn (their horn key), splash, bumps and vibration on their own pad.
  - The views: `Draw2D::SetView3D` is SCISSOR_2, each half drawn with its own camera and highlight, the effects
    per view as on the PS2.
  - Laps, chimes (0x2E only), each player's FINISH!, the clock until both have finished, then the fade; no replay,
    no FINAL LAP. Start on either pad pauses, and that pad works the menu (Continue, Retire).
  - The HUD (`RaceHud::DrawVersus`): the layout above, Meter_Draw10, the teammates' places, one map with player 2's
    arrow and teammates cyan (the map's teammates are now red for player 1's side, fixing the old all-cyan).
  - Meter 10 shows the fins' arrow for a car with the fins (only Custom Race's own parts can have them).
  - Checked: frames of Race Right-Away on Endurance Run against PCSX2 (`race_er.p2s`, both cars driving): the halves,
    the cameras, the HUD, meter 10 and the places match. By scripted runs: pad 2 drives, both finish (FINISH! in
    each half, the fade, back to the course select), pad 2's pause with pad 1 locked out, Retire.
- **The results** (`game/results/versus_results.cpp`, `VersusResults`): Versus_ResultsTask and Versus_DrawRow as
  above, on Results_DrawBackdrop (func_00235880's packet). `RaceResult` now carries each car's total and best lap
  (`Lap::total`, `Lap::best`), name and each side's cars. Cross runs Results_UpdateVersus (`save_data.cpp`, the
  2 Player branch of Results_UpdateRecords), then the loader. The teammates' names now come from the whole driver
  table (D_002C22C8, `race/driver_names.inc` generated from the ELF; Car_DriverRecordName): the Quick Race's
  opponent table missed Flower and Kuwano.
  - Checked against PCSX2 (both players finished at their next pass of the line over PINE):
    `ps2_results.png` against `port_results.png` (the same layout; the times differ with the race), and the course
    card after it (`ps2_results_card.png`): one attempt each, the bests, the winner's course and total score.
- **The Tunnel Race** (row 6; events.md): the race on ACTION/A10 with the event courses' tables, grid, song and HUD
  file, the mirrored cars, Car_Crash's tunnel branch and its sound. No results screen: the records straight away
  (`VersusResults::Versus_SaveRecords`), then the loader.
- **The Sliding Door Race, the Obstacle Course, the Highway Race and Soccer** (rows 7, 8, 5 and 9; events.md): their
  tasks in place of Race_Task, with their countdown (Soccer: its kick-offs), banners and fades; Soccer's ball and AI;
  the records at once with the Race_Outcome they set.
- **The course select** (`game/quickrace/course_select.cpp`, shared with the Quick Race): the race mode's card mask
  and rows, S01's miniatures, both pads, the course box (TEAM BATTLE, the events' texts),
  CourseInfo_DrawVersusRecords, Exit, and Save&Exit with its saves (`save/save_screens.cpp`, save.md: the PC's
  messages for a file). The parts and selling cards open the Adventure's shops (CourseSelect_VersusPartsTask,
  CourseSelect_SellPartsTask).
- **The font:** Text_DrawGlyph's accent escapes are in (`text.cpp`). The card messages' "PlayStation~R2" has the
  registered sign, and é, ü and the rest draw as on the PS2.
- **Checked** by scripted runs: the menu, Random Race with no saves (message 7), with only `save1.json` (message 5),
  and with both (the summaries); the course select on pad 2 (moving, picking, Exit to the title); Save&Exit's
  overwrite question, "Saved.", the cursor moving on and pad 1's Cross on Save Player 2 buzzing.
- **Checked in PCSX2** (pad 2 on the keyboard): Race Right-Away's
  carousel, the Endurance Run and Soccer cards picked with pad 2's Cross. The port's frames match.
- **Save&Exit in PCSX2.** A cheat gave Race Right-Away the Random Race's cards: hold pad +0x0A (0x0033590A) at 3 over
  PINE while Versus_Task loads, so CourseSelect_Task builds mask 0xA3FF. Then both players saved from it, which put
  saves on both (blank) cards; Random Race loads them since (`select_random_cards.p2s`). Seen and matched by the port:
  the Save&Exit window's place; "Saved."; the overwrite question over the summary, and with player 1's `+0x3438` poked,
  "This is not your saved data." with No first; the cursor moving down one after every save, No included; pad 1's
  Cross on Save Player 2 doing nothing, and pad 1's d-pad not moving player 2's message; End Game, Yes to the title;
  "Data has been loaded." with both summaries; Versus_FindRivals' entries (an existing one found, a new one made for
  a changed id). Blank cards first ask "is unformatted. Do you want to format?" (message 12 + p), Yes formats and
  saves; the PC never asks.
- **A fix from it:** Save_DrawSummary's frame is style 1 (olive, orange edges), not the menus' blue. The original's
  Window_DrawCellFrame passes a fifth argument through to Window_DrawFrame in `$t0` untouched (the matched C shows
  four), so it's the style; the port's Window_DrawCellFrame now takes it.

## The port: a plan

### Decisions for the PC version

- **Player 2's controls** (decided): pad 1 is the keyboard plus the first XInput controller, pad 2 the second
  XInput controller.
- **The memory cards.** Slot 1 and slot 2 map onto `save1.json` and `save2.json` (save.md). Random and Custom need
  both files, as the PS2 needs both cards. The messages can stay, worded for files. **(To decide:** whether to keep
  the "insert the cards" step or load straight away.)
- 2 Player is always shown in the main menu (title.md).

### Steps, in order

1. ~~**Input:** two pads (the decision above). Pad 2's buttons through the same `Button` bits.~~ (done)
2. **The front end:** ~~Title_VersusMenuTask with the three items; Race Right-Away's preset
   (Save_InitVersusPreset); loading both saves for Random/Custom with the messages and Versus_LoadedTask's summary~~
   ~~Versus_Task's flow (the rivals, back to the course select after a race)~~ (done).
3. ~~**The course select in 2 Player:** D_002BE1D8's rows, the card masks, both pads, CourseInfo_DrawVersusRecords,
   Exit and Save&Exit.~~ (done).
4. ~~**The split-screen race on rows 0-4:** two cameras and two views with the scissor and centres above; player
   2's car, pad, engine, skid slot and vibration; the teams; no opponents; both must finish; the pause from either
   pad.~~ (done, with each record's key setup and the teammates' learning)
5. ~~**The split HUD:** both blocks, the shared clock and lights, meter routine 10, the minimap's colours (and the
   teammate fix), no FINAL LAP.~~ (done, with meter 10's fins arrows)
6. ~~**The results and the records:** Versus_ResultsTask and Results_UpdateRecords' 2 Player branch, then Save&Exit
   writing each player's file.~~ (done; rows 5-9 have no results screen and update the records at once, as the
   port does: the files are written only by Save&Exit)
7. ~~**The parts and selling cards** (0x19, 0x1B).~~ (done with the Adventure's shops: parts.md "7. The
   shops"; CourseSelect_VersusPartsTask and CourseSelect_SellPartsTask in `course_select.cpp`)
8. ~~**The events, rows 5-9:** Highway Race, Tunnel Race, Sliding Door, Obstacle Course and Soccer.~~ (done,
   events.md) Each has its own task and needs its own analysis, like the Quick Race's did.

### To check in PCSX2

- ~~A 2 Player race's frame: the halves, the centres, the HUD and meter 10, against the port.~~ (matches)
- ~~The two engines and skid sounds together.~~ Both run on the PS2 (a state saved with both players
  accelerating): engines 0 and 1 playing, type 2, select 1, levels 0x1CCC / 0x1CCC each (no pan by
  player), voices 0-1 and 2-3 keyed with each car's rpm. Skidding together, D_00335728 held 6 and 7 at once (player
  2's skid loop is the next entry), as the port's.
- ~~The results screen, and the course card's records after a race.~~ (match)
- ~~The teammates' speed tables changing over races (Ai_Drive's marks).~~ They change on the PS2, and the port's
  change the same way (`captures/versus/teammate_learning_ps2_port.txt`, ai.md).

## Doc corrections found on the way

- hud.md had the teammates' minimap colours the wrong way round (fixed there).
- hud.md's "214 lines lower" is true only of LAPTIME and the lap rows. The position is 312 lower and the meter 224.
- This doc had the Car_Init word's bits 5-7 as "the car record index": it's the D_0177EA50 entry (bits 5-9); the
  save car comes from the flags. It also had the HUD's best lap from the records (the pointer is never read) and
  the parts from the saves (only in mode 6).
- 0x00233EF0 isn't viewport code. It's the HUD canvas setters (HudCanvas_SetHidden, the
  banners at 0x002342C0 and after).
