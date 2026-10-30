# New Game: Adventure menu, name and currency entry

What happens between the main menu and the story, as worked out from the decomp and PCSX2 captures (in
`captures/menu`), and how `src/game/flow` reimplements it. Functions are named in
`docs/names.csv`. The window system they use is described in `title.md`.

## How the title hands over

The main menu doesn't start anything itself. It returns the chosen item to `TitleSeq_Task`, which passes it to
the root task. The root task's update (`func_00205088`) then starts the next task:

| Main menu result | Next |
|---|---|
| 0 Adventure | `Title_AdventureMenuTask` (0x00209520) |
| 1 Quick Race | `QuickRace_Task` (0x00211170): NOW LOADING, then the course select (see `course_select.md`) |
| 2 2 Player | `func_00209848`: Race Right-Away / Random Race / Custom Race |

When that submenu finishes, the root acts on its result:
- 0 → `NewGame_Setup` (0x0026D438), the New Game flow.
- 1 → `NewGame_Continue` (0x0026D370), continuing a loaded save.
- 2 → `func_0026B848`.
- 3 → `func_00211320`.
- −1 → `TitleSeq_Task` with argument 2: the main menu again, built fresh.

## Adventure menu

`Title_AdventureMenuTask` (0x00209520) has two items, New Game and Continue (`D_0029ACE0`, two per language).
- It draws the title picture, darkened by a black overlay that rises to alpha 0x40 over 32 frames.
- The window is placed at 8 (lower half), then moved to x 0x190. Help mode 1 (Enter, Back).
- New Game returns 0. Triangle returns −1 (0x1B).
- Continue runs the memory card screens (`Mc_LoadSlotWindowTask`, 0x00228D10). A loaded game returns 1.

## New Game

`NewGame_Setup` (0x0026D438) initialises the save block at 0x0177F760. It includes a random pick from 6 starting
values (`func_00229F70`), probably the starting car. It also starts loading, then hands over to `NewGame_Task`:
- **`NewGame_Task` (0x0026D550):** starts `NameEntry_Task` with mode 0, and two loader jobs for the opening scene.
- **`NewGame_AfterName` (0x0026D5E0):** when the name is done, starts `NameEntry_Task` with mode 1.
- **`NewGame_AfterCurrency` (0x0026D628):** when the currency unit is done, starts the opening scene
  (`func_00211708`). The scene is the company president's office, with dialogue boxes.

## Name entry (`ui/nameentry`, 0x0025B5A0-0x0025BE98)

The tree sorter had put these under `race/timetrial`; they now have their own module.

**Modes** (`D_002C40D0`: prompt, buffer, length):

| Mode | Prompt | Buffer | Length |
|---|---|---|---|
| 0 | Enter your name. | 0x0177FD8C | 9 |
| 1 | Enter currency unit. | 0x0177FD9F | 4 |
| 2 | Enter greeting. | 0x0177FDA6 | 5 |

Nothing found uses mode 2 (the test menu's Name Entry, `func_0026B848`, starts mode 0); it puts the keyboard higher up. When the name is applied,
its first five characters are also packed into the save's ID at +0x3438.

**`NameEntry_Task` (0x0025BCF0):**
- For the first 16 frames it only fades in (alpha age × 8).
- After that, each frame it runs `NameEntry_MoveCursor`, then `NameEntry_Input`, then draws the keyboard, the
  prompt and help mode 1.
- On Apply it joins the typed keys into the mode's buffer and switches to `NameEntry_CloseTask` (0x0025BE30).
  That task fades everything out over 16 frames, without the help box, then tells the parent.

**Keyboard** (`D_002C3B10`, 16 bytes per key): label pointer, four moves (up, down, left, right), position.
- 92 keys: A-Z, a-z, then `0-9!"#$%&'()*+,-.:;`, then `<=>?@[\]_{|}`. The font draws `\` as ¥.
- Rows of 26; Cancel (key 90) and Apply (91) sit at column 28 of rows 0 and 1.
- A key with all-zero moves uses its grid neighbours, wrapping at the edges. Others spell their moves out, for
  example M-Y and `#`-`;`, which link the letter rows to the symbol rows.
- A move with bit 0x8000 wraps. It's taken only on a fresh press, not on the pad's repeat. Each move plays 0x1D.

**`NameEntry_Input` (0x0025B798):**
- Cross on a character types it (0x22). When the text is full, the cursor jumps to Apply.
- Cross on Cancel, or Triangle, deletes the last character (0x1B).
- Cross on Apply finishes if anything has been typed (0x1A).
- Anything not allowed buzzes (0x1C). There's no way back to the menu.

**Drawing:**
- **`NameEntry_DrawKeyboard` (0x0025B5A0):** a style-2 frame at (0x29, base/2, 0x22E, 0x30), with base 0x138
  (0xA8 for the greeting).
  - Each key is centred on x = col × 18 + 50, at y = base + row × 24, in palette 10.
  - `Window_DrawBar` highlights the current key, at the key's own width.
- **`NameEntry_DrawPrompt` (0x0025BB08):** a style-0 frame centred at y 0x24, as wide as the larger of the prompt
  and 18 × length, plus 18.
  - The prompt is at y 72, palette 0.
  - One 18-pixel slot per character at y 96: the typed character in palette 10, or `*` in palette 0.

## In the port

- `game/title/title_menu` has the Adventure menu. Continue loads a save (AdventureLoad in save_screens; save.md) and
  `game/flow/new_game` goes on as NewGame_Continue (0x0026D370) does: NOW LOADING, then the Q's Factory of the town
  the save was made in (`+0x12DC`). The clock it sets (0x13C68, 9:00) is the time of day the Adventure goes on at; NewGame_Setup sets the same.
- `game/flow/new_game` runs the name, then the currency unit.
  - The entries go to the save record (`Save_Current()`, save_data.h; save1/2.json).
  - Then the opening scene (below), then NewGame_AfterOpening: Q's Factory and the Adventure.
- `game/flow/name_entry` is the text entry, as above. On PC, Start (Enter) also finishes the entry, as Apply does.
- The loading screen (Loading_Draw) and the saves (save_file.cpp, save_screens.cpp) are in.

## The opening scene

Reproduced in PCSX2: `captures/adventure/` (`currency_entry.p2s` just before the currency is applied,
`office_01.gs/.p2s` in the office, `factory_menu.p2s` at Q's Factory's menu). The dialogue is in messages.md.
- The scene is the building scene func_0022AEB0 with town 8, place 9. The room is SHOP/T07.BIN chunk 9 (0x3F000 bytes
  a room; D_0029B290[town] gives the file: town 0 GARAGE.BIN, town n T(n-1)). Its picture is the 10th 640x384 texture of
  T07 (021.dds). The picture's top 225 rows are a sprite at the top of the screen, blended by alpha. The floor is a
  20x20 quad (D_0029F1B0) repeating the picture's 64x64 tile at (512, 320) (CLAMP region repeat). Two decal quads
  (D_0029F230, which turns, and D_0029F1F0) sit at (1.2, 0, 5.2). The cars are drawn over all of it.
- The camera is D_0029F190: eye (2.2, 1.1, -45) in the turned frame, focal 3564, pitch 0x1100, yaw -0x1D80, centred on
  the origin. Checked: the floor's corners project to the GS dump's values exactly.
- Car slot 0 (D_0177EA50[0]) is the Secretary (group 8 record 8, body 147). It starts at (9.06, 0, 1.47) with speed
  2100 and heading 0xC271 (D_00333680), using the port's yaw convention. Slot 3 is Forest (record 9, body 148), at
  (-1.75, 0, 2.1), yaw 1.963. Lights come from Light_Setup(1).
- The car state is scene +0x27 (func_0022A6B8, func_0022ABA0); paths are D_0029F300 (func_0022A408: steps of
  {frames, accel sign, steer}, speed ±0x23 a frame, moving speed/12800 along the heading). The office starts in state
  0xE (path 6: the Secretary drives in) and goes to 0xF. Message result 5 then sets +2 = 4, which gives state 0x10
  (SE 0x34, path 7: drives out) and then 0xB. The scene ends with func_0022C418: the frame held under a 0x08 black
  overlay for 64 frames, then the song stops.
- Song 0x0A0B0104 (ROOM_1 entry 4); SE 0x32 at the start. The message starts at scene age 0x41.
- Next comes NOW LOADING, then Q's Factory (func_0026D678: D_0177EA50 becomes the save's cars, town 1 place 0).

In the port: `game/story/room_scene` (Room_Start, Shop_SceneTask, Room_CarPath, the car states 0xE/0xF/0x10/0xB,
Room_EndTask) and `game/story/message_task` (Message_Task with its codes, the box, the name tab, the choice and Yes/No
windows), with the tables in `data/story/messages.txt` (modelconv). Checked against PCSX2: the office, the floor, the
box and both cars line up with `office_01`, Forest's matrix and the Secretary's end position (3.57, 1.80) match the
save state's D_017524B0, and the scene plays through to the Secretary driving out as on the PS2.
The speaker's headlights flash with the typing blips (info+0x18, Car_DrawLights; cars.md), and a car's brake lights
are on while its path slows it (the step's accel byte & 2, sp40). Every end code (0x00-0x18,
message_task.cpp) and the garage's turntable states (2-7, room_scene.cpp) are in.

## Q's Factory

Every town (1-9) has a Q's Factory as its place 0, with the shared message table 0x2A4450 (messages.md). My City
starts without one (MyCity_PlaceOpen(0): resident 0 moves in by a talk's pre-code 0x17; buildings.md "My City"). After the opening,
NewGame_AfterOpening (func_0026D678) makes D_0177EA50 the save's three cars, runs the loader (job 0x70001, NOW
LOADING) and opens town 1, place 0 through func_00211808 (Room_Start). PCSX2 state: `factory_menu.p2s`.
- Shop_SceneTask takes a place 0 of a town below 10 as the factory: D_00335720 is filled with 0xFF (8 bytes; what it
  is: not checked), the car state +0x27 starts at 0 and the song is 0x0A0B0101. The cars stand where the opening's
  do (D_0029F320; the person is "Q's Factory Staff", group 1 record 0, body 28), and car 0 starts with the same
  speed and heading (D_00333680).
- State 0: car 0 drives in on path 0 (slowing, then a right turn onto the turntable), then state 0xC and SE 0x35
  (the turntable's motor). 0xC turns the car in place: each frame its heading's high byte steps once towards 0x80
  (facing the camera) and the turntable's angle +0x26 with it; when it faces the camera, state 0xD, SE 0x8035 (the
  motor stops) and the age restarts. The talk starts at age 0x41 regardless, while the car is still driving in
  (once: the scene's +0x25 becomes 1).
- A plain end of the talk (result 5) tells the parent 10 (on to the town) and asks the cars to move (+0x02 = 4):
  from 0xD the car goes to 9 (SE 0x34), which turns it to 0x80 (already there, so at once) and gives 0xA; 0xA drives
  off on path 3, then 0xB ends the scene (func_0022C418). "Drive around town" (#11) is this end, its pre-code 0x12
  telling the parent 10 first. Result 6 ("Quit game", #10's code 0x0F) tells the parent 6 and ends the scene at once,
  without the car moving.
- Car 0's part flags (+8): flag 0x8 (the Flight Wing out, car_physics.md) is cleared and 0x4 (folded) set in its
  place, in Shop_SceneTask's setup (0x0022B2CC; Garage_SceneTask's too, 0x0022CE54), on the save's car: Part_Fit's
  options mask (0x0324) doesn't clear 0x8. The port: RoomScene::Start and GarageScene. Car 0's and
  the teammates' +0x74/+0x194/+0x104 come from part flag 0x400 (big tires; Room_Draw sets the height for all three cars).

In the port: `game/story/room_scene` (state 0, the turntable states in Room_TurnCar, 0xA, Room_CarRequest)
and `game/flow/new_game` (the loading screen and the factory with the save's cars). Checked against PCSX2's
`factory_menu` screenshot: the turntable, the staff's truck and the player's car on the turntable sit in the same
places. Drive around town drives out onto the field around the town (field.md); Quit game returns to the title.
Debug: `--factory` starts here with a new save; `--field N` drives straight out of town N's factory;
`--allparts` adds every part, eight bodies, the Super A license and two teammates.

The menu's windows (messages.md, "What Message_Task returns"):
- **Change parts** (#5, end code 4): Message_PickCarTask lists the player's name, the teammates there are and Quit.
  A car closes the box and gives the scene 9 | car << 8: state 2 opens the parts screen (Parts_WindowTask; parts.md)
  and asks that car onto the turntable (+0x02 = car + 1). A teammate's: from 0xD, state 2 turns the player's car
  to face the camera (it does already), 3 drives it off on path 1 while the teammate's car (slot 2, placed at
  D_0029F320 + 0x20 = (7.8, 0, 5.2) with D_00333688's speed 0x992 and heading -0x4000) backs on on path 4 30 frames
  later, 4 turns it to face the camera (SE 0x35, 0x8035) and 5 holds it. Leaving the parts screen restarts the talk
  (#1 goes to "Whose parts" while flag 0 is set) and asks for the player's car (+0x02 = 1): from 4/5, 6 turns the
  teammate's car to 0x40, 7 drives it off on path 5 while the player's car backs on on path 2, then 0xC turns it.
  Slot 2 is drawn in states 3-7. A body change rebuilds the car (the PS2 reloads it, func_0020AD18 with +4 & 0xFF).
- **Race** (#7, end code 8): Message_RaceListTask, the town's races (race_list.h; Peach Town: Peach Raceway RANK C,
  Peach Raceway RANK B, Peach Raceway II RANK A); "B/A/Super A license is required." over it when locked; a race
  goes to #15 "$R , right? / Have fun!", whose code 3 ends the talk with result 4: the scene tells its parent 4 and
  is gone at once. Its parent, **Adventure_Task** (0x002105B8, state 4), takes result 4 to Race_Setup with
  info+0x20 (the race) behind the loading screen and goes to state 9, which after the race (Retire or the results,
  finish.md) starts the scene again: the car drives in once more and Q's #1 (flag 3, set by #15) goes to #16, the
  talk about how it went. On the PS2 (factory_menu.p2s, Peach Raceway RANK C won by cheat) Cross on the results goes
  straight back to the factory and "How was the race?". State 9 also moves the clock to the course's end time
  (D_002A88EC[course]; 0 there: the clock as the race began, Adventure_Task +0x20, + 9000). The port does both.
  The race (`Race::StartAdventure`, Race_Setup in mode 0): the row's course and laps; the player's car in the last
  slot with its own parts (the save car's +0xC, part flags +8), the teammates in slots 0 and 1 with theirs (lanes 1 and
  2, their own speed tables, the teammate grip), and the row's opponents with the row's parts record, the list's last
  six from the back and the rest from the front (`Race_CreateAdventureCars`); the lap to beat is the race's record;
  the pause menu has the Ranking. Shop_SceneTask also calls Place_MarkFound (0x0023C7B0) on this end; the port
  doesn't (the stamps). Debug: `--advrace N` goes straight into race N (with event flag 3 set) and back to its
  town's factory.
- **Save data** (#6, end code 0x0E): the save's +0x12DC = the town, then Save_AdventureSaveTask (slot 1 / slot 2,
  then the 2 Player save flow on that slot with record 0: "There is no data. Do you want to create new saved
  data?" or the overwrite question, then "Saved."). The box and its name tab hide meanwhile (Message_Task state 6);
  then #4 again. Checked in PCSX2: the slot window, and the box hidden under it.
- The stick turns the turntable while the parts window or the paint screen is open (scene state 2; paint.md, "The stick").
