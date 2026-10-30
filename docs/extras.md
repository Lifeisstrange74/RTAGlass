# Extras (PC)

Switches for what the original game has but never lets the player reach (unused content), and for its leftover
debug features (debug.md), and the PC's own cheats (below). A PC feature: all are off by default, and off is the
game as it shipped. Code:
`src/game/extras.{h,cpp}` (the setting and extras.json), `src/game/ui/extras_screen.{h,cpp}` (the screen),
`src/game/debug_text.{h,cpp}` (the debug font), and each switch where it acts (below).

## The menus

- **Title:** Options is Vibration, Speaker, Sound Volume, Controls, Display (display.md), **Extras**, (with
  `"cheats": true`) **Cheats**, Exit. Extras and Cheats run over OPTION2's background with their name on the panel, as Controls
  does.
- **Field menu:** Settings (the title car with "Settings" drives in as the PS2's) shows a list window (style 2, at
  (40, 88)) of **Controls**, Display, **Extras**, (with `"cheats": true`) **Cheats** and **Other**, with a help window at the PS2 Settings' help place (340,
  280). Controls and Extras open their screens over it; Other opens a window at the top right with the title
  Options' Vibration, Speaker and Sound Volume, which open the same setting windows as on the title
  (`ui/option_windows`: Options_VibrationTask, Options_SpeakerTask, Options_VolumeTask). Triangle on the list
  drives the car back out.
- **The Extras screen** has the Controls screen's look and workings: the list window with the bar and horn
  cursor, a help window (here the full height, where Controls has its pad), Initial settings (this page off) /
  Cancel change / Apply, and "Cancel?" (No first) on Triangle with changes. Two pages on the first row, `< Unused
  Content >` and `< Debug >` (Left/Right). On a switch's row Left, Right or Cross turns it On (palette 5, green) or
  Off. Apply sets the extras in use and writes extras.json.

## extras.json

Next to the executable, written by Apply (through a temporary file), read at start; scripted runs (`--shot`)
neither read nor write it. `--extra NAME[,NAME...]` turns switches on for one run without writing it.

```
{ "format": "RTAGlass extras", "version": 1, "notes": "...",
  "lifeMeter": false, "qWave": false, "mail5": false, "testCourse": false, "body149": false, "demoSong": false,
  "duckQuiz": false, "language": false,
  "pauseItems": false, "readouts": false, "freeCamera": false, "heightKeys": false, "clockScrub": false,
  "testMenu": false, "mapTuner": false,
  "cheats": false, "uncappedSpeed": false, "infiniteFuel": false, "powerBoost": false, "superGrip": false,
  "reduceDrag": false, "lowGravity": false, "slowRivals": false, "maxMoney": false, "allParts": false }
```

A missing file or entry is off. `"cheats"` has no row on any screen: it's changed in the file by hand (Apply keeps
what the file had).

## Unused content

| Switch | What it does | Where |
|---|---|---|
| Life Meter (`lifeMeter`) | My City's Parts Shop stocks meter 6, the Life Meter, once stamp 47 ("Cleared the Which-way? Race!") is earned. MyCity_MeterStockAdd (0x002679C8) asks Stamp_Earned(47) at 0x00267A2C and drops the answer, so no shop stocks it | `MyCity_ShopStockAdd`, story/parts_screen.cpp |
| Q-Wave (`qWave`) | The radio gets a fourth button, "3" (the unused digit among POSE picture 3's labels), for SNDMOD's station 2, `SOUND/2CH_L/R.VAG` (sndconv now converts it: `data/sound/radio/2CH.adp`), with "③ Q-WAVE" from POSE picture 0's name strip (u 144) and the first station's equaliser table. A save tuned to it plays nothing once the switch is off (Radio_Start puts it back to OFF) | `Radio_Buttons`, field/field_menu.cpp |
| Endurance Mail (`mail5`) | Wonder Realty's mail 5 ("Now Open!": "You can now play Endurance Run! Go to Q'sFactory if you wish to play.") is sent when Gonzo (resident 0) moves into My City, right after his own mail 3, which says his Q's Factory is open with the Endurance Run. **The moment is an inference:** nothing in the game sends mail 5 but the debug complete-record fill (func_00227C68) | `Resident_MoveIn`, save/save_data.cpp |
| Body 149 (`body149`) | Body 149 (CAR4/Q149.BIN: model, lights and brake lights, texture) is a complete body in no Body Shop row, talk, driver record or race table, and its name in D_002ED720 is empty. The switch puts it in My City's Body Shop, (9, 2), always, as body 28 is in its own row, at 500 like every body (Part_Price). The PC names it **Q Remote**: it's a box on wheels with two sticks, and its texture has a battery cover with the Choro Q logo and "TAKARA" (what it was meant to be is a GUESS). Its paint, jet nozzle and part places are in the per-body tables like any body's; it draws in the shop, the factory, the field and races. A save that has it keeps it, and its name, with the switch off | `MyCity_ShopStockAdd`, `Part_Name`, story/parts_screen.cpp |
| Demo Song (`demoSong`) | The title's attract demo plays DEMO.TSQ's song (sound file 0x1B, entry 1: `Sound_LoadSong(0x0A1B0201)`) in place of REPLAY.TSQ's (0x0A180201). Nothing in the game loads DEMO (no 0x0A1B.... song id in the code). Checked: the scripted demo's sound matches `--soundtest 1B0201` | `Title_AttractDemoTask`, title/title_sequence.cpp |
| Duck Quiz (`duckQuiz`) | Cloud Hill's duck, (8, 5): questions 2 and 3 (#9, #14) start with "if event flag 75, go to #20", "Quack, quack! You're gonna try again? I think it's too hard for you." (#20 clears 75 and goes back to #1, then on to the question), but nothing sets flag 75. **The moment is the PC's (a GUESS at the intent):** flag 75 is set when #5 ("You're not gonna try?!", No) or #6 ("Wrong!") comes up while flag 73 (question 1 won) or 74 (question 2 won) is set, so coming back after giving up on question 2 or 3 opens with #20. (After question 1 the duck already has #1's "Huh? You came again?".) A save keeps the flag | `DuckQuiz_Entered`, story/message_task.cpp |
| Language (`language`) | The title's Options get the Language item where D_0029AC90 has it (Vibration, Speaker, Sound Volume, **Language**, then the PC's Controls and Extras, and Exit; the US Options_MenuTask adds items 0-2 and 4 and has no task for it). It opens the PC's window: the item's name, then English, Francais (written "Fran~cais", the font's c cedilla) and Deutsch (the PC's names), the one in use green; Cross sets it (SE 0x1A), Triangle leaves (0x1B). The language is D_00333284 (0 in .sdata; nothing writes it). It picks the rows of the tables that have French and German: PRESS START (func_00208A88: D_002EA090 / D_002EA0A8 / D_002EA0C8), the main menu (D_0029AD20), Adventure (D_0029ACE0), 2 Player (D_0029ACF8), Options (D_0029AC90; French has "Volume" for Speaker), the help box (D_0029A8E0; French keeps "Apply" and "Select") and the Vibration window (D_0029AC40). Their fourth language rows are empty. The accents are the font's escapes ("R/esultats", "Zur~uck"), which the port draws. The menus are made again in the new language at once. **PC choices:** it's for this run only (not in extras.json or the saves), English with the switch off, and only the title switches: the other tables the game indexes by D_00333284 (the Yes/No windows D_002A2AF0, the memory card messages D_0029F0E8 of the save screens) stay English | `Options_MenuTask`, `Options_LanguageTask`, `Title_Language`, title/title_menu.cpp; `Help_Draw`, ui/window.cpp; `OptionWindows::Open`, ui/option_windows.cpp |
| Test Course (`testCourse`) | COURSE/C16.BIN as a Quick Race card after the eight courses: a time trial, the player's car alone for 3 laps. Below | quickrace/course_select.cpp, race/race.cpp |

### Test Course

**The file.** COURSE/C16.BIN is in none of the ELF's file tables: not the course table D_0029BE50 (race.md), not the
debug test menu's course list. Dated Aug 2001 (the other courses Oct 2001-Jun 2002). Header {0x20, field 0x4800, PMP
0x11B730, end}: textures, field and collision, no map section and no objects. One texture (000, a 128x128 blue noise),
15 blocks (no block 64), about 17k vertices; modelconv already wrote `data/models/COURSE/C16` and
`data/collision/C16.pmp`. Seen in the port:
- The road is a **closed tube** about 30 across, a loop 2554 units long over x 20-800, z 20-865, its floor from 7 to
  about 120 high. Coloured rings (pink at the start, green further on) are painted round the inside.
- The tube's two ends meet over the **start box**, an untextured flat quad at y 20.4 (x 410-460, z 808-864) under the
  tube, with a **gap** in the tube at x 427-431: the floor is 27.1 on the +x side and 24.0 on the -x. The upper half of
  the tube is layer surfaces in the PMP (ceilings for what's under them; car_physics.md).
- No sky picture (the sky's material wants TBP 0x3CFE; the PS2 would show what another course left there).

**The PC's own data** (none of it the game's; course id 0x27, Course::kTestCourse, one past the game's ids):
- Driving direction **-x** at the start, the way the seam is a 3-unit step down (driven +x it's a 3-unit wall over a
  gap, and the car drops into the box). Grid {420, 40, 836, dir 3}: on the box, past the gap, dropped onto the floor.
- The line (kLapZones): strips x 404-410, z 815-857, at the box's -x edge.
- `data/ai/C16.txt`, `data/models/COURSE/C16/map.obj` (the HUD map, map view {197, 2006, 40, 0}) and
  `data/camera/C16.txt` (only the follow camera every course ends with) by `scripts/course/make_c16.py`, from the
  blocks: the tube's middle traced on a top view, a 6-wide gate every 15 units (170 zones; gate 0 at the line),
  speeds from the bend (55-170 km/h, eased for braking). With `--autodrive 0` the car laps in about 1'42".
- Peach Raceway's light (midday, sets 19/19, white fog), sky (C00's sky.obj) and song (BGM_01), and its Quick Race
  player car and parts.
- The card: the name strip "Test Course" cut from the strip's own letters (T from Tunnel Race, the rest from Obstacle
  Cource); the window shows C16's own blocks through the miniature camera 9 times as far off (the whole loop, still;
  the chosen card's tilt and turn as the others). Track 1.616 mile (2554 units at the ~1580 units a mile the Quick
  Race courses' paths measure), 3 laps, "Entry 1 car" (" car": the PC's).
- Records: the save has the eight courses' cards only (+0xB78), so the Test Course's are CourseSelect's own, cleared
  with the Quick Race's session as the save's are; they never touch the save. The lap to beat for the brighter chime
  is the card's best (Race::SetBestLap).
- Put back: a car more than 3 under the tube's floor at both of its zone's gates (on the box after dropping into the
  gap, or fallen outside) is put back on the floor at the zone's far gate, stopped and facing along the road
  (`Race::TestCourse_PutBack`, Car_Reset as the Obstacle Course's checkpoints). The clock runs on. In three AI laps a
  car is never more than 0.15 under.
- `--race 39` races it without the switch.

## Debug

The debug byte (pad +0x0B) turns all of the readouts, the free camera, the ride height and the clock scrub on at
once; here each has its own switch. Read from the asm.

**Pause Items** (`pauseItems`): Race_PauseTask's items 3-6 (its jump table jtbl_0032FA00), after Continue, Retire
(and Ranking), in a race for one player. The retail ELF has no English text for them; the labels of 3 and 4 are
the unused Japanese pause menu's (func_0026B018: "meetaa shoukyo" / "meetaa hyouji", "mantan"), AI Drive and Zone
Table the PC's own.
- **Hide Meter / Show Meter** (item 3): race flag 0x10000, SE 0x1A; no map and no meters (the event courses'
  meters and canvas too). The line says Show Meter while they're hidden.
- **Full Tank** (item 4): the player's car's fuel (+0x23C) = 0x40000, SE 0x1A; then the line greys out (window
  +0x54 |= 1) until the next pause.
- **AI Drive** (item 5): toggles the player's car flag 0x80, so the AI drives it (Race::AiDriven, as
  `--autodrive`); no sound.
- **Zone Table** (item 6): the AI zone table editor (Ai_ZoneTableTask 0x0026D020, `race/race_zone_table.cpp`;
  debug.md has the original's details), in the menu's place, which waits undrawn. An opaque olive overlay
  (0x80006666) and, in the debug font, the speed table the player's car drives by, 20 zones a column: each zone's
  last mark (O clean, X steered, blank none), its speed and the mark's speed in km/h; `>` at the car's zone. Up and
  Down (pressed) change that zone's speed by 5 km/h; the Navigator (Select; Tab, the controller's Back) saves;
  Cross or Triangle (Confirm, Back) returns to the menu. With AI Drive the car drives the course and marks the
  zones, so you can tune a table as the developers did.
  - The table is the one every car not on a team shares: the course path's speeds (a Quick Race, an event), or
    an Adventure race's own (adventure_speeds.inc). The editor makes it the race's own copy (`Ai_EditZoneTable`)
    and points every car that shared it at the copy, so an edit changes the opponents too, as on the PS2. The PS2
    edited its data for the rest of the session; the PC's copy lasts for the race.
  - **Saving** (PC choice): the game wrote `host:ai_g%02d.c` on the development PC, C source to build in. The PC
    writes the same lines (`\t%d,\t/* zone %2d */`, up to the last non-zero zone, after a comment line) to
    `data/ai/Cnn.speeds.txt` beside the path it belongs to (`Ann` for an event's path), or `data/ai/Gnn.speeds.txt`
    for Adventure race nn's own table, through a temporary file. The shipped `Cnn.txt` is never written.
    `AiPath::Load` reads `Cnn.speeds.txt` over the path's speeds and `StartAdventure` reads `Gnn.speeds.txt`, with
    the switch on or off; delete the file for the original table. The game printed `AI Save %s` and `done` (or
    `File open error`, `write error!`) on the development PC's console; the PC shows them over the table for 2
    seconds.
  - The game's `>` stays at its column's unwrapped x, off the screen from zone 160 (columns 8-12 are drawn over
    0-4's x); the PC wraps it with its column. Zones past 255 aren't read (the PS2's last column reads 4 bytes past
    the table).
  - Up and Down change the byte as the game does: Down from 0 makes 255, which the AI reads as 127 (& 0x7F).

**Car Readouts** (`readouts`): the developers' text, in their 8 x 8 font (func_00203A50: FONTE u 448 + (c & 7) x 8,
v 224 + (c & 0x78), palette 15; 8 pixels a character, each 8 x 16 on the 448-line frame; `DebugText_Printf`). The
font has no lower case, so on the PS2 `%x`'s a-f don't show; the PC draws them as capitals. Positions below are
(x, y) with y in half-lines.
- Car_Update (player 1's car driven from the pad, car flags exactly 2; not in the field), `"%s %6d%6d%6d %d"`
  printing z, x, y, w at x 300: y 16 LAST (prevVel before the frame), 48 LACC (accel rotated into the car), 24
  WSPD (vel after gravity), 32 LSPD (local velocity), 40 LSPD (after drag, damping and fins; w the local gravity),
  64 LACC (Car_Drive's accel), 80 WSPD (after Car_Drive), 88 WSPD (after the ground and crashes); `AIR: %d` (the
  drag) at (220, 48).
- Car_Engine: `FX %d %d` (the drive force after the brake, the slip x mass) at (300, 120); `TIRE %d RPM %d` at
  (300, 136); `SQUEAL %d` at (300, 152).
- Debug_PrintCar (0x0026D700; the field too): at x 32, `BLOCK %d%d%d` (Field_BlockAt in the block-name digits)
  y 24, `X:` `Y:` `Z:%4.2f` y 32-48, `ATR:%08x` (the front probe's surface) y 56, `WARN:%02x` (the frame's
  events) y 64. In the field the vector lines are off (race flag 0x400 sets car flag 0x100).
- Ai_TrackZone, for each player's car: `ZONE %d %s %s` at (16, 152) (the zone; LEFT/RIGHT/EITHER for the fork
  ahead, and behind where the way ahead doesn't fork); for player 1's driven car `TARGET LINE %d %X` (the next
  zone's far gate, the course heading), `MYROT  %X` (the yaw), `DIST  %d` (the distance x 100) at y 160-176, and
  `REVERSE` at (300, 96) while the car faces the wrong way.
- **Not in:** Field_StreamBlocks' slot readout (func_002201C0: the loader's four slots and the block origin at
  y 24-40, x 400 on). The port streams blocks on threads without the PS2's slots, so there's nothing to show.

**Free Camera** (`freeCamera`): Camera_DebugMove (0x0021F198), in a race for one player. Controller 2's held
buttons move player 1's chase camera instead of its swing and pitch settling back, and Camera_KeepOutOfWalls is
skipped: the d-pad moves the eye 0.1 a frame up/down and across (in the car's frame), L2/L1 (the PC's Wing
Down/Up) along, Triangle zooms in (focal x 1.01162 a frame), Square (the PC's Brake) out while the focal length is
over 500; both controllers' right sticks swing and pitch it (`<< 7`; the PC adds a dead zone). func_0026D880
prints CX/CY/CZ, `ZM:%.0f(%.0f)` and the angles at x 16, y 80-120 (the angle +0x18 isn't in the port: 0).

**Height Keys** (`heightKeys`): Car_Ground (0x0021B3D0), for the players' cars, in races and the field: the
d-pad's Up held puts the car 0x1000 (0.2 units) below where its suspension holds it, Down 0x51EB (1 unit) above,
unless a probe finds a roof. In the port Down keeps lifting the car about a unit a frame: the probes start from
the car's height, so a car held off the ground isn't pulled back. The physics follows the asm's integers, so the
PS2 probably does the same (not checked in PCSX2).

**Clock Scrub** (`clockScrub`): Race_SetupTask on the Endurance Run (course 0x11): pad 1's L2 held moves the clock
on 0x168 frames (6 seconds) a frame, L1 back (from under 0: + 0x34A58), before the usual frame. On the PC, L2 and L1
are Wing Down and Wing Up. The same in the field: Adventure_Task's state 0 (0x00210CF4, "the debug clock nudge";
not in a talk, state 8, and only while race flag 0x8000 is clear) moves the field's clock (info +0x14) the same way
before its tick (`Field::Update`); L2 then L1, each on its own, so both held cancel out unless one wraps. The sky,
the light and the HUD's clock follow (checked: 150 frames of L2 from 12:00 show 18:01, then 50 of L1 16:02).

**Map Tuner** (`mapTuner`): RaceMap_Draw (0x002427D0) and RaceMap_DrawStep (0x00242A88) with the debug byte change
the map's view as they draw it: D_002A4050[course] (x, y, scale, angle; hud.md), or on Figure 8 (course 0x19) its
step's D_002A42E0[step] (Golf's is its course's). Controller 2, in a race for one player (PC choice: in 2 Player
pad 2 drives): held Right/Left x +- 1.0 and Up/Down y +- 1.0 (field lines: Up moves the map down the screen),
R2 (the PC's Jet) scale x 1.01162 a frame, R1 (Horn) scale / 1.01162; pressed L1 (Wing Up) angle + pi/2, L2 (Wing
Down) - pi/2. func_0026DE80 then prints `X:%f Y:%f Z:%f W:%f` (x, y, scale, angle) at (16, 208) in the debug font.
`RaceMap_Tune` (hud/minimap.cpp) does it as the map is drawn, also while paused, as the game's map task does. The
views are kept for the run (the game's data too), not saved. The Free Camera reads the same d-pad: with both on,
both move at once, as with the debug byte.

**Test Menu** (`testMenu`): the developers' test menu, Debug_TestMenuTask (0x0026B848), read from the asm.
Title_AdventureMenuTask (0x00209520) ends with result 2 for item 2 (race flags 0x400000, as New Game) and
the root task (func_00205088) then starts the menu, but the retail window only has New Game and Continue (D_0029ACE0
has two strings a language). With the switch the window gets a third line, "Test Menu" (the PC's text: the original's
isn't in the ELF). Code: `src/game/flow/test_menu.{h,cpp}`, run from the New Game flow (`flow/new_game.cpp`).
- **Opening:** the clock (info+0x14) at 0x13C68, Save_InitBoth, then Save_InitComplete (func_00227C68) makes record 0
  "complete": Alec in euro, the Super A license, 10000 money and 100000 in the bank, every town visited, every photo
  taken and every stamp's text shown (not the stamps themselves: +0x518 isn't set), every opponent beaten, every body
  but 149 and the parts (three copies of kinds 1-5's, one or more of the rest), parts fitted on the three cars, teammates
  (6, 8) and (7, 8) in bodies 0x37 and 0x6E, one more of part 0 of kinds 1-7, all 128 mails, event flags 1, 4 and 90.
  D_0177EA50 points at record 0's three cars.
- **The menu:** a window (WindowAlloc's: centred on (320, 112)) on a green ground (info+0x50 = 0x8033CC66 each frame)
  of Go Town, Race Course, Race Solo, World GrandPrix, Action Stage, Stamp Note, PetitPhotoBook, Result Note, Name
  Entry, Show Ending, To Shop and Go Back Title (jtbl_0032F9C0). Cross (SE 0x1A) starts the item; Triangle does nothing.
  The window isn't drawn while an item runs, and comes back with its cursor where it was.
- **Go Town** (Debug_TownListTask 0x0026C460 with 0): a scroll list of D_002BE438's 22 towns (11 shown; Triangle back).
  My Garage goes straight in; any other town lists its buildings (D_002BE43E, 15 shown): the speakers' names of its
  message tables, green for a place found, grey for a person away at 9:00 (Town_Absent) or a My City place not open.
  A place starts the Adventure there (func_00211808: Room_Start, then Adventure_Task in state 4), for good: the menu
  doesn't come back. On the PC a place without a talk starts on the field at its door.
- **Race Course / Race Solo** (Debug_RaceListTask 0x0026B1B8 with 0 / 1): two cell frames of the 28 town races
  (rows 0-27), each row's course file (D_002DCC78, C00-C14, C18-C20), name and rank (D_002DCCC0: C, B, A, S, X),
  the long names running over the next column (the original's positions and font). Left/Right change column; Cross (SE 0x1A)
  runs the race (Race_Setup with the row; then its results), Triangle (SE 0x1B) goes back; after a race the list comes
  back. Race Solo sets race flag 0x800 first, which Race_Setup keeps: Race_CreateCars with no setup (the player's car
  alone, grid slot 0), race flags 0x104 at once (the clock runs and the car may go during the countdown, which still
  plays; the pause works from the start), and Race_CompleteLap returns after a lap's time and count, so there's no
  final lap or finish: laps count on until Retire.
- **World GrandPrix** (Debug_GrandPrixListTask 0x0026B470): a window of the seven stages (rows 0x1C-0x22); Cross runs
  one (the GPX results after it, also after Retire), Triangle back.
- **Action Stage** (Debug_ActionListTask 0x0026B5C8): two cell frames of the 20 events (rows 0x23-0x36) with their
  stage files (D_002DCCD8: A01, A02, A04, A10, A19, A20, A00, A03, A05-A09, A11-A15, A17, A18) and names; Cross runs one,
  Triangle back.
- **Stamp Note / PetitPhotoBook / Result Note:** Stamp_BookTask, Pictures_Task and Results_BookTask on their own
  (`FieldMenu::Book_Open`); Triangle closes them, back to the menu.
- **Name Entry:** NameEntry_Task with argument 0, "Enter your name." (not mode 2, as new_game.md had it); Apply writes the
  name into record 0.
- **Show Ending** (Debug_EndingMenuTask 0x0026CCD0): a window of lines that are Japanese in the ELF ("endingu 1" to "10",
  "endingu furu", "shiataa"), shown translated: Ending 1-10 run the ending's field scene 0-9 (Ending_FieldScene) until
  Start, Ending Full is Ending_Task (the fanfare scene, then scenes 1-9 under the staff roll), Theater the cinema's
  replay (Ending_ReplayTask). Start ends any of them (Sound_MuteCars, Sound_MuteSong: the song plays on silent) and the
  window comes back, as when the last two end. Triangle goes back to the menu, without a sound.
- **To Shop** (Debug_TownListTask with 1): the same town list; a town lists every message table, its buildings and then
  its field talks. A building runs its scene alone (Room_Start, func_00211708) and its end comes back to the list:
  quitting (result 6) to the towns, a race (4) opens the scene again (no race), a talk sending the car elsewhere (11)
  opens that building, driving out (5) shows the stamps it queued (Stamp_ShowTask) and then (the PC; the PS2's next
  step isn't clear) the list. My Garage runs the garage, and its end goes back to the towns. **Not in:** a field
  talk (Team_Find's car and Room_SetupTask with 1, a scene without its room) buzzes (SE 0x1C).
- **Go Back Title:** the menu ends with -1 and the root goes back to the title (TitleSeq_Task with 3).

**The saves.** Nothing in the menu writes a save; the record stays in memory. The only way to write it is the
Adventure's save point (Go Town, then a Q's Factory's Save): the slot window, then, over a save in that slot, the
overwrite question. The PC marks the complete record (Save_SetTestRecord, until Save_InitBoth or the title's main menu
opens again) and while it's marked that question treats any save in the slot as another player's: "This is not your
saved data." with No first. So save1.json and save2.json are only written if the player picks Save, a slot and Yes.

## Cheats

The PC's own, not the original's. Hidden unless extras.json has `"cheats": true` (put in by hand, so they're not
one menu away): then Options and Settings offer **Cheats**, the Extras screen with one page, "Cheats" (no
Left/Right), and its switches. With `"cheats"` false (or missing) every cheat is off whatever its own entry says
(`Extras_On`), and the screen isn't offered. Scripted runs take `--extra cheats,NAME...`.

The car cheats are `Car_SetCheats` bits (car_physics.h), set around the players' cars only, as the Height Keys are:
the race's (and its replay's, which drives the same physics from the recorded pad; not the attract demo's) and the
field's. `Car_PlayerCheats` reads them from the switches.

| Switch | Name | What it does |
|---|---|---|
| Uncapped Speed | uncappedSpeed | In top gear (the next gear's ratio 0) Car_Engine's limiter, and the throttle's fuel cut-off at 0x251D, move up to 32000 rpm (the rpm is worked out without the 16-bit wrap, and kept under 32000); the drag sets the top speed. The jet's push isn't held to kJetTop. The engine's pitch stays capped at 10000 (EnginePitch), the meters clamp too. |
| Infinite Fuel | infiniteFuel | The fuel back to 0x40000 at the start of every Car_Update, so the jet keeps its full push. |
| Power Boost | powerBoost | Car_Engine's torque doubled. |
| Super Grip | superGrip | The tires' grip on the surface under the front probe doubled (the engine's grip and the brakes' cap). |
| Reduce Drag | reduceDrag | Car_Update's forward drag divisor doubled (0x600 to 0xC00, and the slowed, wading and underwater ones too): a quarter of the drag. The sideways damping stays. |
| Low Gravity | lowGravity | Gravity a third (0x59 / 3) in the fall, the wheels' reach and the suspension; the tires' load (Car_Ground's `loaded`) keeps the full value, so the grip doesn't drop. |
| Slow Rivals | slowRivals | Ai_Drive aims for three quarters of the zone's speed (`Ai_SetSlow`), for the cars no player drives that aren't Adventure teammates; not the attract demo. Soccer's AI is its own. |
| Max Money | maxMoney | In the field the money is set to 999999 (Money_Pay's most) every frame. |
| All Parts | allParts | Every shop in every town sells everything from the start, story unlocks or not: each Parts Shop every part of its category that has a price, each Body Shop every body the player doesn't have (0-148; 149 stays the extra's), added to the shop's own stock (and My City's additions). Only the priceless items are left out: each category's stock part (Normal, None) and the five sponsors' Billboards. Priced prizes (the Life Meter, say) are sold too. |

Checked: on the Drag Race strip (course 0x16), Cross held with Power Boost, the car levels at 8996 rpm in fifth
without Uncapped Speed and passes 9116 with it before the strip ends; Power Boost alone takes Peach Raceway's first
straight from 121 to 154 km/h; Slow Rivals takes car 1's best on Peach Raceway from 159 to 104 km/h; Max Money's
999999 shows in Items. All Parts isn't checked in a shop yet.
Reduce Drag, on the Drag Race strip (Quick Race's car, Cross held from frame 30, the best speed before the
finish): 162 to 176 km/h; with Uncapped Speed and Power Boost, 210 to 228.

## Found, not made a switch

The sweep also found these, which have nothing for a player:
- SYS/FONT.GSL, in no file table: the Japanese kana font (with "(c)TAKARA CO.,LTD." and the JP buttons).
- Two skeleton event tasks with no end: func_00263710 (course 0x22's canvas, one car, BGM_05, a countdown) and
  func_002642B8 (course 0x1C's canvas, two cars; GUESS: the Tunnel Race's own task before Race_Task took it over).
- Functions nothing calls: readouts the debug byte doesn't reach (func_0026AD48 VX/VY/VZ, func_0026AE88 "FOG %.0f
  %X", func_0026DA90-0026E1F8 the replay camera's and texture coordinates), func_0020E500 "Screen Save" (a dump to
  the development PC), func_0026CF20 (a two-page records viewer for races 0-23, before the Results book),
  Settings_Task (field_menu_settings.md), and the unused sound RPCs Sound_RadioMute/Loud and Sound_EngineStop.
- The Yes/No windows' and the memory card messages' French and German (D_002A2AF0, D_0029F0E8): the Language
  extra leaves them English.
