# Quick Race: course select

Main menu → Quick Race. The PC version is `src/game/quickrace/course_select.cpp` (with
`game/flow/loading.cpp` for the loading screen). Reference captures are
in `captures/course/`: frame-by-frame PCSX2 shots of each step (`sheet_*.png` are contact sheets), a save state
on the carousel (`captures/course_select.p2s`) and a 1.5 s GS dump of it (`captures/course_01.gs`).

## Flow

```
Title_MainMenuTask: Quick Race (Cross, SE 0x1A)
  -> TitleSeq ends; the root task (func_00205088) starts QuickRace_Task (0x00211170)
     loader tasks (func_0020DC50) read the course select's files; Loading_DrawTask (0x0020E140) shows NOW LOADING
  -> CourseSelect_Task (0x002455C0)                 the carousel; song SELECT.TSQ (Sound_LoadSong 0x0A190201)
       Cross on a course -> CourseInfo_Task (0x00248180)
            Cross (after 17 frames) -> result 1: QuickRace_Task loads the race    (PC: game/race, see race.md)
            Triangle -> result -1: back to the carousel
       Cross on Exit   -> CourseSelect_QuitTask (0x00248A70) -> Dialog_YesNoTask (0x00239150) "Quit Games"
            Yes -> result 2: func_00258FB0 (song off), QuickRace_Task gets -1 -> QuickRace_ExitFadeTask (0x00211948)
            No / Triangle -> result -1: back to the carousel
  QuickRace_ExitFadeTask: black overlay, alpha t*2 over 64 frames; then result 0 to the root -> title (PRESS START)
```

Child tasks report to their parent with `+0x04` (result) and `+0x24` (flag); the parent acts on it on its next
frame. Every task's `+0x08` counts its frames and is reset to 0 on each change of state.

## Loading screen (Loading_DrawTask)

Frame flag 0x400000 makes the display keep the last picture and new drawing go over it. For the first 64 frames
the task sets it and adds a black overlay of alpha 6 (out of 128), so the frozen title darkens (and softens) to
almost black; after that the screen is cleared black. `NOW LOADING...` is at text cell (12, 10), palette 0, all the
time. On the PS2 the menu → carousel load takes about 68 frames of darkening plus about 2 s of black; leaving the
course select shows about 15 frames of black. The PC shows 64 + 16 and 15.

## The carousel (CourseSelect_Task)

Which cards appear is a bit mask by mode (`pad+0x0A`): Quick Race (mode 1) 0x40FF, mode 6 0xB3FF, mode 3 0xA3FF, others
0x43FF (bit 9 needs both players' second teammates; 2 Player's cards are in two_player.md). Card k is the k-th set bit
(Bits_Count 0x00207B20 counts them; Bits_Rank 0x002081D0 turns the start slot into its card). Quick Race cards: bits 0-7
are rows of the course table `D_002BE158` (16 bytes: course id, cars, laps, flags; 1 = show the length, 2 = show laps, 8
= TEAM BATTLE), bit 14 is Exit. Ids from the table; for bits 10 and up the id is 0x19 (bit 12), 0x1B (bit 13) or bit +
0x10, so Exit is 0x1E.

| card | id | name | length |
|---|---|---|---|
| 0 | 0 | Peach Raceway | 0.745 mile |
| 1 | 2 | Temple Raceway | 1.118 |
| 2 | 5 | Night Glow Raceway | 1.522 |
| 3 | 6 | Snow Mountain Raceway | 1.367 |
| 4 | 9 | Oval Raceway | 1.118 |
| 5 | 14 | Lagoon Raceway | 1.864 |
| 6 | 16 | Tin Raceway | 2.113 |
| 7 | 17 | Endurance Run | 1.864 |
| 8 | 0x1E | Exit | |

All Quick Race courses: 24 cars, 3 laps. Lengths are `D_002A8F30[id]` in thousandths of a mile.

**(PC) The Test Course extra** (extras.md) sets mask bit 8: a card "Test Course" (id 0x27, COURSE/C16.BIN) between
Endurance Run and Exit, 1 car, 3 laps, 1.616 mile. Its name strip is put together from the strip's own letters, its
window shows C16's blocks through the miniature camera 9 times as far off, and its records are kept apart from the
save's eight cards (`CourseSelect::TestRecord`). Off, the carousel is the game's.

**Textures** (`SYS/SELECT.GSL`, loaded at 0x1CC6): 0 the card frame, 256x216 (the window is olive and gets the
picture drawn over it); 1 the name strip, 192 wide, 32 rows of 28 by id (row 30 Exit, 31 Save&Exit); 2 the cloud
tile, 200x150, opaque sky. Picture cards (id >= 0x12) use `SYS/S02.GSL` (nine 220x156 T8 pictures at 0x285E, 184
blocks apart), picture `D_002A8BF0[id - 0x12]`; Exit is 7, the car in a tunnel.

**Clouds**: three rows of five tiles; row r at y = 150r, x = 200i - (frame + 64r) % 200 (frame = the global frame
counter), so the sky moves left a pixel a frame.

**Ring**: card at position p = (k - sel) mod n has angle a = pi * s16(((8p + scroll) << 13) / n) / 32768 (cos is
func_00274938, sin func_00274A10). x = 320 + 192 sin a, top y = 128 + 64 cos a, size z = 192 + 64 cos a
(256 = 1.0). Each part scales by z/256 about x:

| part | rect | function |
|---|---|---|
| frame | x +-128, y .. y+216 | CourseSelect_PlaceCard 0x002443D0 |
| name | x +-96, y+12 .. y+40 | CourseSelect_PlaceName 0x00244800 |
| picture card | x +-110, y+50 .. y+206 (220x156) | CourseSelect_PlacePicture 0x00244538 |
| course view | the same window, about 221x158 | CourseSelect_PlaceCourseView 0x00244988 |

Colour (all parts) is grey `(int)(cos a * 16) + 0x60` (0x50-0x70), and 0x80 for the card exactly at the front.
CourseSelect_DrawCard (0x00244D60) draws back to front: for k = n/2 down to 0, card sel-k then sel+k.

**Course pictures are 3D.** For a course the window shows a live render of a miniature of the course
(`D_0177AB24[id]`, from SYS/S00.E3D / S01.E3D, placed at (-200 (id & 7), -200 (id / 8))). The view doesn't scale with
the card: a smaller card shows less of it, roughly the middle of its top part. See "Course miniatures" below.

**Input** (`+0x34` repeat bits, both pads in 2P): Right turns to the next card (scroll = +8), Left the previous
(-8), SE 0x24; only when scroll is 0. Scroll counts back to 0 one a frame (so a turn takes 8 frames) and keeps
`+0x08` at 0. Cross (pressed) when `+0x08` != 0 and scroll is 0 picks the front card: SE 0x1A, mode (`+0x25`) 1.
There is no Triangle on the carousel; Exit is the way out.

**Chosen card**: in modes 1 and 2 only the front card is drawn, at full size and brightness. Mode 1 moves it from
(320, 192) by -10, -10 a frame for 16 frames to (160, 32); mode 2 (back) moves it back and returns to mode 0.

## Course information (CourseInfo_Task)

Fades in (TEXA alpha t*8) over 16 frames; Triangle fades it out over 16 while the card moves back. The BACK
(u 0x100) and OK (u 0x154) plates, 84x32 from FONTE palette 2 at v 0x118, are at (0x1A4, 0x20) and (0x1FE, 0x20)
while it's open (Help_DrawButton 0x00200DC0).

- CourseInfo_DrawCourse (0x002477A8): frame around cells (15, 2)-(29, 9). "Track" (16, 3); the length's fraction
  as three digits ending at x 0x212 (Number_PrintDigits 0x002046C0), "." 10 pixels before them, the whole miles
  right-aligned to that (Number_PrintRight 0x00201E48), " mile" (26, 3). Laps at 0x212 / " laps" (26, 4) and, in
  the Quick Race, "Entry" (16, 5), cars, " cars" (26, 5). Digits are the large 16x20 set (0-3 at (0x1B8, 0xC8),
  4-9 at (0, 0xDC)).
- CourseInfo_DrawRecords (0x00247BF0): frame (10, 10)-(29, 16). The player's name in green (11, 11),
  " Challenge attempts" (11, 12) with the count at 0x226, " Best Total Time" and " Best Lap Time" (11, 13-14)
  with Time_Print (0x00203B78) at x 0x1D6, " Your Best Result" (11, 15) with " 1ST" .. "24TH" or " --" at (24, 15).
  Time_Print uses Text_PrintSmall (0x002008C0), the narrow 10x20 font: `M'SS"CC`, `--'--"--` for none.

## Exit

CourseSelect_QuitTask makes Dialog_YesNoTask: a default window (centred on (320, 224)) with "Quit Games" and the
items Yes and No, No selected. The dialog starts a frame after the quit task and its window a frame after that, so
it shows 3 frames after Cross. Yes: SE 0x1A, result 2. No: result -1. Triangle: SE 0x1B, result -1.

## Course miniatures

**Data.** The loader (func_0020B5E8, states 6-7) reads SYS/S00.E3D or S01.E3D by bit 0 of the loader request: the
Quick Race's request 4 reads S00, Versus_Task's request 5 S01 (D_0029AFE8 and D_0029B048, whose sector counts match
the two files) to D_00AAFB80 and relocates its first 64 words into pointers: `D_0177AB24[id]` is course id's model.
Entries of 0x20 bytes are empty stubs (S00 holds ids 0, 2, 5, 6, 9, 14, 16 and 17; S01 4, 5, 7, 10, 11, 13, 17 and
18). A model is a DMA chain (cnt ... ret) that CourseSelect_DrawCard calls with Vu1PacketAddCall. Its VIF stream
repeats:

- `UNPACK V3-32` to top+0x192: an A+D packet the microprogram kicks before its strips: TEX0_2, CLAMP_2 and TEX1_2
  (linear filtering). The textures are SYS/S00.GSL's (or S01.GSL's), at the GS addresses the GSL gives them, so TEX0's
  TBP/CBP find them in the texture set's manifest directly. For direct-colour formats (CT24) CBP is leftover junk.
- `UNPACK V4-32 x1` to top+0: the header, a GIF tag (x = vertex count, y = word 1 with PRIM). PRIM is a tristrip,
  0x27C textured or 0x26C untextured, both alpha blended and fogged in context 2. w holds the same with PRIM 0x27D/
  0x26D (a fan), which only the clipper uses.
- `UNPACK V3-32 x5N` to top+1, `MSCAL 0x08`: program 0x40, five qwords per vertex: position, colours 1-3 and STQ
  (Q is always 1).

**Camera.** CourseSelect_DrawCard uploads, per card: 3 qwords to M[0x13] (vf08-10: colour weights (1, 0, 0), so
colour 1 is the vertex colour, and fog (128, 255, 0.5, 800), which never fogs at these distances), the camera
matrices to M[4..0x12], setup 0x00, and the course's offset to M[0x1A]. The camera comes from
CourseSelect_PlaceCourseView:

- D_002A8B50 = {pos (0, h, -260), focal (+0x10), pitch (+0x14), yaw (+0x16)} and the target matrix D_002A8B70, a
  translation to (100, 0, 100), the centre of the course's cell. The eye is target + Ry(yaw) Rx(pitch) pos, looking
  along the rotated z (func_0021F340, func_0021F450, func_0021F698).
- Still (the carousel, and the chosen card moving back): pitch 0x4000 (straight down), yaw 0, h 0, focal 440. So
  the camera is 260 above the course's centre.
- Chosen, at frame t of the card's timer (`+0x08`): focal 440 + 13t (1024 from t = 44), pitch 0x4000 - 256t (0x1300
  from t = 45), yaw 32t, h = min(t, 20).
- Projection: x = cx + focal * x/z, y = cy - 0.47 * focal * y/z on the 224-line field (0.94 in frame lines). The
  centre (func_0021F910) is the card's (x, y + size / 2); the scissor is the picture window. Checked against a replay
  of a PCSX2 save state's VU1 list (`captures/course_select.p2s`): M[8..15] match these numbers for every card.

**PC.** `tools/modelconv` writes each model to `data/models/SYS/S00/<id>.obj` + `.mtl`, S01's to `SYS/S01`
(moved to the origin, colour 1 as the vertex colour, a material per texture and wrap mode). The course select draws them
with `gfx::Model` (CPU transform, near-plane clipping, perspective-correct STQ) through the camera above. A port frame
of Temple Raceway 23 frames after it was picked lines up with PCSX2's `captures/course/11_pick_23.png`.
