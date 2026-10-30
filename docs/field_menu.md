# The field's Start menu (Pause)

Start on the field (Adventure_Task state 0, pad +0x30 & 0x800) runs func_0022E718, Sound_MuteCars, Sound_PlaySe(0x1F)
and func_00258B30, stops the field's tasks (func_00204EE0) and starts **FieldMenu_Task (0x00217148)**, state 2. The
menu covers the whole frame. When the task ends, state 2 looks at its result (task +4):

- **-1:** state 0 again: func_00204F18 (the tasks on), Sound_LoudCars, func_002590B0(clock), Light_Setup.
- **A town (Warp):** info+0x21 = 0 (place 0), +0x23 = the town, race flags |= 0x400000 and &= ~0x1000 (Q's Rally
  ends), Save_ClearEventFlag(0x50, ...), and the clock + 9000 x ((task frames & 3) + 1): one to four hours. Then
  state 3: the loader and Room_SetupTask (the town's Q's Factory), or for town 0 func_0022D1E0 (My Garage).

## The task

FieldMenu_Task keeps its work in D_01727B90 (0x83A0 bytes, zeroed by FieldMenu_Open on the task's first frame):
+0 the screen, +4 the screen asked for, +8 the screen's frames, +0xC done, +0x10 the top row a list was left at.
A changed +4 starts that screen the next frame with +8 = 0 and +0x81A4..+0x8398 cleared (the cars, the road, the
arrows). The screens (the main screen's item ids):

| | Function | |
|---|---|---|
| 0 | FieldMenu_Main (0x00214AD0) | the main screen |
| 1 | FieldMenu_Warp (0x00214EF8) | Warp |
| 2 | FieldMenu_Notebook (0x00215220) | Notebook |
| 3 | FieldMenu_Radio (0x002156D0) | Radio |
| 4 | FieldMenu_Items (0x002161E0) | Items |
| 5 | FieldMenu_Settings (0x00216738) | Settings, the key setup (the PC has its Controls screen here: controls.md) |
| 6 | FieldMenu_Map (0x00216E00) | Map |
| 7, 9, 11 | Stamp_BookTask (0x00265B30), Pictures_Task (0x002665B8), Results_BookTask (0x00267248) | Notebook's books, child tasks; 8 runs Results too (nothing sets it), 10 falls to Warp |

**FieldMenu_Open (0x00216FF8)** makes two lists: the main screen's (labels D_0029CA30: 0 "Pause", then 1-6; 6 rows,
+0x64 = 1 so Start leaves it too) and Warp's (label 1 "Warp", then 12 + i for each town i whose bit in save +0x12DA,
D_01780A3A, is clear: My Garage, Peach Town ... My City).

## The list (Menu_UpdateList 0x00212AC8)

Each screen is a column of cars towing banners. **Menu_PlaceCars (0x00212A00)** puts them off to the right: the
title's at (0x2A2, 12), item i's at (0x2E4 + 20i, 0x50 + 0x38i); x in pixels, y in frame lines (448). The cursor's
car is lit (white banner, the label in its chosen colours, the lamps blinking) and 20 to the left of the column.
The list's states, a frame each:

1. **0, 1:** the cursor's car lit; the list scrolled to its top row from the last one's (+0x10), 4 steps a row.
2. **2:** the title's car drives in, 40 pixels a frame, 10 frames. **3:** all of them, 6 frames. **4:** the items',
   10 frames. So the title stops at x 34 and item i at 100 + 20i (the cursor's at 80 + 20i).
3. **0x64:** the scroll arrows (+0x81EC above, +0x8204 below: more rows that way). **0x65:** the pad:
   - Cross (0x40): Sound 0x1A, state 0xC8 (returned that frame).
   - Triangle (0x10), or Start with +0x64: Sound 0x1B, 0x12C.
   - Menu_MoveCursor (wraps; 0x1D): the new car lit, the old one not, 4 frames (0x66) of the new car 5 pixels left
     a frame and the old one 5 right. The list keeps two rows above and below the cursor when it can; each row is
     4 steps of (5, 14) up and left for the item cars and the road.
4. **After Cross:** the chosen car drives off left (10 frames), then all of them (6), then the rest (10); 0xCC
   saves the top row and returns **0x12B**.
5. **After Triangle:** all drive back off right, 16 frames, then **0x18F**.

FieldMenu_Main on 0x12B starts the chosen item's screen, on 0x18F ends the menu (-1). On 0xC8 it starts the loads of
Notebook's (func_0020DC50 0x340005), Settings' (0x3C0300, 0x34000E) and the Map's (0x340016) files.
FieldMenu_Warp on 0xC8 gives the parent the town at once (label - 12; My Garage 0): the warp doesn't wait for the
cars. Its 0x18F goes back to the main screen, where the cars drive in again with the cursor where it was.

## The picture

All of it is SYS/POSE.GSL, which the field has loaded at TBP 0x1D8E: picture 0 (T8) the panel and the sign,
picture 1 (T4, 256 x 256) the labels, 11 to a column of 128 x 22, picture 2 (T4) the cars, banners and road. Each
piece has a z, the sprite calls' last argument (func_0024F728 isn't a depth: it starts a batch of that many sprites,
Sprite_BeginBatch); visually, from the back:

- **Menu_DrawRoad (0x00212190)**, kinds 0 and 1, picture 2 palette 13, from the road's place (+0x81CC, +0x81D0;
  it scrolls with the list): 10 x 9 tiles (u 104, 80 x 80), 15 white bars down the left (0, 56 + 40k; u 0 v 68,
  100 x 20), 6 x 11 dashes (125 + 100c, 85 + 56r; u 104 v 80, 75 x 7). Kinds 2-5 are the other screens'.
- **Menu_DrawPanel (0x002119A8)**, picture 0: the title's dark panel (0-320 by 73, u 184-249) and the yellow
  borders with their lights, x 320-640 at the top and the whole bottom at y 0x1A0, 64 pixels a light.
- **Menu_DrawSign (0x00211F68)** at (0x1B8, 0xA0): D_0029C0C0, one corner mirrored four ways, the arrow and its
  post; four lines of D_0029C1A0[label] at (x + 30, y + 40 + 24k) in white. During Q's Rally Warp's labels read
  "Warning! / Warping / ends the / rally" (the first in colour 6).
- The scroll arrows (D_0029C300, picture 0 u 0 v 88, 38 x 22, the upper one flipped) at (40, 94) and (40, 374),
  shown while frame & 0x10.
- **Menu_DrawCars (0x00212720)**, each car: the banner (D_0029C340, (83, 4), u 0 v 88 156 x 40; palette 14 lit,
  15 not), the car (83 x 50, palette = its id: the title red, then blue, green, pink, yellow, grey, purple), the
  banner's lamps (palette 12, 6 x 24 at u 160 + 6f, at (94, 10) and (226, 8); f = frames >> 4 for a lit car, else
  0), the label (99, 10) in picture 1 palette 0 lit, 1 not, 2 the title.
- Help_Draw(2), "X button Enter / Triangle button Exit": the main screen only.

## Radio (FieldMenu_Radio 0x002156D0)

The car radio over a background of radio waves. Each frame Field_TickClock (0x002114E0) runs the clock (only this
screen does: the menu otherwise stops it), then **Radio_UpdateMenu (0x002137B0)**:

1. **0:** Radio_Start (sound.md): the save's station from the clock. The cursor (+8) and the button on (+0xC) are
   save +0x12DD (0 OFF, 1 Peach FM, 2 E-Radio), the volume (+0x10) +0x12DE (0-0x80).
2. **1, 2:** the title's car ("Radio", Menu_PlaceCars with label 3 alone) drives in, 40 a frame, 16 frames.
3. **0x65**, the pad:
   - Cross (SE 0x1A): the cursor's button on, if it isn't. The save takes it; a station is tuned to the clock and
     played (Sound_RadioTune, Sound_RadioPlay), OFF stops it.
   - Triangle (SE 0x1B): 0x12C, the title's car drives back out right, 16 frames, then 0x18F: the main screen.
   - Down held turns the volume down a step a frame, Up up (0-0x80, into the save at once); otherwise
     Menu_MoveCursor over the 3 buttons (Left and Right, wrapping). Sound_RadioVolume(volume x 0x3FFF / 128) every
     frame.

The screen's end stops the radio: the 0x18F branch falls through into OFF's Sound_RadioStop (at 0x00213A30; the m2c
draft hides it). So the menu is quiet but for the Radio screen: Start stops the radio (Adventure_Task), the Radio
screen plays it, leaving it stops it, and the menu's end tunes it again (Radio_Start). Checked by ear: a PCSX2
recording from `field_radio.p2s` (`field_radio_exit_ps2.wav`) has the station on the Radio screen and silence on the
main screen after it.

**The picture** (the radio's top left at (0xA5, 0xA0)): Menu_DrawPanel, the road kind 3 (POSE picture 3 palette 5:
10 x 7 tiles of 64 x 66 from u 192 v 48), the title's car, then:
- the body, picture 3 palette 0: its left end (96 x 128, the knob), 22 strips of 8 from u 96, its right end
  (D_002EAC48: x 272, u 104, 38 wide);
- the cursor, picture 0's orange arrow (u 0 v 88, 38 x 22) at (0x68 + 0x2E x cursor, -30);
- the clock (hours = clock / 9000, minutes = clock % 9000 / 150) as HH:MM in the display's digits (picture 0, 10 x 14
  at u 70 + 10 x digit, v 96; 10 the colon), right to left from x 250, y 58;
- the knob's mark: picture 0 u 71 v 97 (8 x 12, inside the "0" digit) on a 6 x 6 quad 13-19 above the knob's middle
  (54, 62), turned (volume x 4/3 / 128 - 2/3) x pi (D_00332B34, D_00332B38): 240 degrees, clockwise up;
- with a station on, its name (picture 0's strip, 72 x 18 at u 72 x (station - 1), v 110) at (112, 56), and the
  equaliser: 8 columns at x 108 + 20c of up to 7 bars (16 x 2, u 216 v 112) up from y 108, 4 apart; a column's
  bars are D_0029C458[station - 1][(frames >> 5) & 3][c] + frames % 3, kept to 0-7;
- the buttons at (100 + 46k, 8), 46 x 36: picture 3 palette 1, u 144 for the one on; palette 2, u 192 the others.
  Their labels in the same palette: OFF (D_002EAC68 / 88: (107, 20), 32 x 12 at u 144, v 96 on, 112 off), 1 and 2
  ((114 + 46k, 20), 16 x 12 at u 0x80 + 16k on, 0xB0 + 16k off, v 36).

No hint box and no sign. Checked against `field_radio_ps2.png` (`field_radio.gs`'s sprites) piece for piece.

## Notebook (FieldMenu_Notebook 0x00215220) and its books

Details: `field_menu_notebook.md` (Notebook, Stamps), `field_menu_books.md` (Pictures, Results).

**Notebook**: the title's car ("Notebook", label 2) over Menu_DrawRoad kind 2 (NOTE.GSL picture 0's pencils, 10 x 7
of 64), driven by **Notebook_Update (0x002135C8)**: in 16 frames; Up and Down over the three tabs (wrapping); Cross
(SE 0x1A) drives it off left (16) and gives 0x12B, Triangle (SE 0x1B) off right and 0x18F. The notebook is NOTE
picture 1 (120, 104), 216 x 270 from u 152, with a Class C card printed on it; a license (save +0x651 > 0) puts
its card over that one, (156, 136), 152 x 104 from v 104 x (license - 1). The tabs, NOTE picture 2: (336, 128 +
84i), 26 x 56, u 352 chosen, 384 not; their labels (336, 136 + 84i), 176 x 40 at v 40i, u 176 chosen, 0 not.
Stamps loads SYS/STAMP.GSL (0x40017) and Pictures SYS/PUTI.GSL (0x40018); Results needs nothing more. The tab's
cursor lasts while the menu's open.

The three books share one open book (func_00264FF0: 29 pieces repeated from their texels, GS REGION_REPEAT, and 12
labels of 80 x 12 from v 80 + 12 x kind), the pencil ground, the blinking arrows at (30, 204) and (588, 204), and
the turning page (func_00264D50): two quads hinged on x 320, the angle 0x800 a frame (16 frames), x = 320 + dx cos,
y = 224 + dy (1 + dx sin 0.2 / 320). Their picture 0 is whichever GSL loaded last: STAMP's and PUTI's labels are
Japanese (they overwrite NOTE's English ones in the GS), so the PS2 shows them, and so does the port.

- **Stamps (Stamp_BookTask)**: a stamp a page, 50 spreads, Left and Right (repeat, no wrap); the save keeps the
  left page (+0x12E0). Page n: its number, a frame, ruled lines, the stamp (STAMP picture n + 1, 128 x 128) once
  earned (+0x518), its text ('#' lines, D_002DBBA0) once shown (+0x528). Triangle back.
- **Pictures (Pictures_Task)**: 100 photos, 5 a page; the cursor (+0x12E2, which Photo_Take sets) moves by
  D_002DBD30's table and turns the page off the side. A photo taken (+0x508) shows its thumbnail (PUTI picture
  n + 1, 64 x 47, at 94 x 66); Cross on it (SE 0x1A) opens Photo_ViewTask (0x0022BFD0): PUTI.BIN's picture at 2x,
  (64, 40), with the car of the photo's record over it. Cross on an empty frame: SE 0x1C.
- **Results (Results_BookTask)**: 14 spreads, Left and Right wrapping: a page per Adventure race (the course and
  class, then the player and teammates' best place, lap and time: +0xFF0, +0x1040, +0x1178), then Figure 8, Ski
  Jumping and Curling, Golf and Q's Rally ("????????" while their place is hidden, +0xBD8). Text goes over the
  turning page, so only the page it isn't over is printed.

## Settings (FieldMenu_Settings 0x00216738)

Details: `field_menu_settings.md`. The title's car ("Settings", label 5) over Menu_DrawRoad kind 5 (OPTION2.GSL's
128 x 128 tile), **Settings_Update (0x00214470)**: Up and Down over 12 rows (wrapping), Left and Right through a
key row's buttons (D_0029C618's lists: 10, the Navigator 11 with SELECT). A button two rows share turns both red
(colour 6) with the help's "Warning!", and Apply (SE 0x1E) refuses. Initial settings (func_00214220) and Cancel
change (func_00213C40 again) change only the choices; Apply (func_00214078) writes save +0x12E8 (the horn's row
the lights too) and leaves. Triangle with changes opens Settings_CancelTask (0x002173D0), "Cancel? / Yes / No" on
No. The 3D pad is SYS/OPTION.BIN (loader 0x3C0300: model slot 3), lit by Light_Setup kind 2, seen from (0, 0, -2)
with focal 3564 about (470, 164); it leans towards the row's button 0x200 a frame (R1/R2 and L1/L2 tip it over),
and a red and yellow box (Prim_DrawLineStrip, 0x0024F148) turns round the button every 64 frames.

## Items (FieldMenu_Items 0x002161E0)

The title's car alone ("Items", label 4) over Menu_DrawRoad kind 4: 14 x 10 tiles of green palms, POSE picture 3
palette 5, 48 x 48 from u 144 v 48 (D_002EAB48). **Menu_UpdateTitle (0x00213A78)** runs it, the machine the
title-car screens share (Radio has its own):
- **0, 1, 2:** the task's window inactive (+0xE |= 0x80); the car in, 40 a frame, 16 frames.
- **0x64:** the window active.
- **0x65:** Triangle (SE 0x1B): the window inactive.
- **0x12C, 0x12D:** the car back out, 16 frames.
- **0x18F:** FieldMenu_Items sets +4 = 0, the main screen.

Cross isn't read. Window_UpdateList's result is dropped: the list's cursor only moves (Up and Down, no wrap).

The pieces, every frame:
- **The list:** the task's window (task + 0x28), made on the first frame by Window_SetList with 8 rows shown, then
  set to style 2 at (0x28, 0x2C), 0x104 x 0x60 (frame lines (40, 88), 260 x 192). Its rows are the set bits of the
  save's kind-15 items (+0xD5C, Part_NthOwned) as D_002F0810's names, ids 0-47 (45 is the Coin Radar). With none, a
  row "You have none".
- **The description:** the cursor's item's, split at '\n', a line every 24 from (40, 304) in white; with none,
  "Find items". It sits in Window_DrawFrame(0x28, 0x98, 0x104, 0x30, 0).
- **The right box:** Window_DrawFrame(0x154, 0x2C, 0x104, 0x9C, 0). In it:
  - "Choro Q Coins" at (360, 88), and 100 less the save's pickups still out (+0x548's bits) ending at x 580, y 90.
  - "Mileage" (360, 114), and the save's distance / 1609 with " Mile" ending at 580, y 134.
  - For each billboard owned (Options items 4-8, part kind 11), once, "Advertisement" (360, 156). Then the
    sponsor's name (D_0029C518: " Cafe", " Noodle cafe", " Bakery", " Wool shop", " Coconut shop") and its miles
    not yet paid (+0x65C + 4k), 44 a row.
- **The money:** func_0022E780 = Money_Draw(390, 42).

The list's rows (Window_UpdateList): an item line is palette 6 disabled, 5 the current setting, else white under
the cursor and the style's colour elsewhere (10, yellow, in style 2). Checked against `field_items_ps2.png`.

## Map (FieldMenu_Map 0x00216E00)

The world map alone, no cars or panel: SYS/WORLDMAP.GSL (loaded at Cross on the main screen, func_0020DC50
0x340016: the SYS table's entry 0x16, at TBP 0x1F2E; D_0029C030 entry 8), one 640 x 448 T4 picture over the frame.
The state goes 0 to 0x65 at once; Triangle (SE 0x1B) makes it 0x18F and the main screen comes back. Nothing else
reads the pad.

The car's mark, while the task's frames & 0x20 and the course isn't 0x22 (Cloud Hill, its own map), is
Text_PrintHalf (0x0024F530: Text_PrintAt with y halved) of D_00333580, Shift-JIS 8E A9, in palette 4:
- x = ((car x + 0x2000000) & 0xFFFFFFF) / 594240 + 0x82, the world's 8 blocks (0x2000000 each) wrapped and
  starting a block in;
- y = 0x1A2 - car z / 594240 (frame lines), from the car's fixed point (D_0177AC50 + 0xA0, + 0xA8).

Text_DrawGlyph sends two-byte codes over 0x889E to **Text_KanjiCell (0x002012F8)**: 11 cells of the font's kanji row
(v 0x140, 20 apart): 8E A9 is the last, u 0xC8, a little red car. Checked: Peach Town's start, the PS2 (window shot
`09_map.png`) and the port put it at (523, 132) and (524, 131).

## The port

`game::FieldMenu` (`src/game/field/field_menu`: the main screen, Warp and Radio; `field_menu_screens.cpp`:
Notebook and its books, Items, Settings and the Map; `field_menu_draw.h`: the pieces they share). `Field` opens it
on Start (the cars muted, SE 0x1F) and stops while it's up; Warp ends the field (`Field::kWarp`) and `NewGame`
moves the clock and loads the town's Q's Factory. The field starts the radio (Radio_Start) and stops it at Start
and at doors; the menu's end starts it again. Warp to My Garage opens it (town 0, place 0).

The port skips the loads (the textures are all at hand) and draws in back-to-front order instead of by z. The
Settings pad is `data/models/SYS/OPTION/0.obj` (modelconv's ConvertPad; `modelconv GAME DATA --pad` makes just it).
On the PS2 Settings' Apply writes the key setup the car reads (Car_ReadPad), so it takes effect at once. The PC
opens its Controls screen there instead (controls.md).

The town visited: the port clears the town's bit of +0x12DA as its Q's Factory opens (and for `--field N`).
Place_MarkFound (0x0023C7B0) does this for place 0 on the PS2: Message_Task's end code 0x14 (0x0023B5E4) and
Shop_SceneTask's car state 0xB (0x0022BB44) call it.

On PC, Enter is Start, so Enter leaves the main screen as on the PS2; Z or K choose.

**Checks** (`captures/adventure/field_menu.p2s`, `field_menu.gs`, `field_menu_ps2.png`): the menu just opened
in Peach Town (Barthou's talk over). The GS dump's sprites give every piece's
place, size, texture and palette; the port's main screen against `field_menu_ps2.png` matches piece for piece. Up
from Warp goes to Map. Warp in that save lists Peach Town alone (+0x12DA = 0x3FD).

## Not in yet

Nothing of the menu's own: Q's Rally's warning, My Garage, the photos (taken by the Quick-Pic shops' end code 0x11)
and the car over them are in.

**Checks** for the other screens (`field_items`, `field_settings`, `field_notebook`, `field_stamps`, `field_pictures`,
`field_results`: .p2s, .gs and _ps2.png in `captures/adventure`, and the window shots in
`captures/adventure/menu_ps2`): each screen of the port against the PS2's, the Map's mark, the Settings pad's
lean to R2 and the Cancel window.

Unused (the PC's Q-Wave extra adds it: extras.md): a third station, Q-Wave (`SOUND/2CH_L/R.VAG`, SNDMOD's station 2, given its files by Sound_Init), with its
name in POSE picture 0's strip (u 144, v 110) and a "3" among picture 3's button labels. The radio's cursor only
covers OFF, 1 and 2. It may be offered later as an option (not converted yet: sndconv takes 1CH and 3CH).
