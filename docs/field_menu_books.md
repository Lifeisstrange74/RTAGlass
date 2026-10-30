# Notebook: Pictures (screen 9) and Results (screen 11) — spec

Reference for Notebook's Pictures and Results books: what the PS2 does, read from the asm (field_menu.md has the summary; the port is field_menu_screens.cpp). "GUESS" marks what wasn't read from
the code or seen on the PS2.

From the asm (reference/asm/nonmatchings/cod/*.s) and ELF tables (SLUS_203.98), checked against
`captures/adventure/menu_ps2/04_pictures.png` and `captures/adventure/menu_ps2/05_results.png`. Anything not read straight from code is marked **GUESS**.
Coordinates are in the 640x448 frame (x pixels, y frame lines) unless stated. "half-lines" = the original's
224-line y that its print functions double.

## 0. How they are reached (FieldMenu_Task, func_00215220 = Notebook)

The Notebook (screen 2) list: item 0 **Stamps**, 1 **Pictures**, 2 **Results** (01_notebook.png). On 0x12B
(Menu_UpdateList's "entered") func_00215220 does, by the item (+8 of its list state):

| item | loader job (func_0020DC50 via func_00204C48) | screen |
|---|---|---|
| 0 Stamps | 0x40017 = SYS/STAMP.GSL | 7 (func_00265B30, the stamp book; uses save +0x12E0) |
| 1 Pictures | 0x40018 = SYS/PUTI.GSL | 9 (func_002665B8) |
| 2 (and any other) Results | none (NOTE.GSL is already in: FieldMenu_Main preloads 0x340005 = NOTE.GSL) | 11 (func_00267248) |

Loader job 0x4nnnn = SYS file table D_0029AF58[nn] (12 bytes: handle, sector, sector count), read to
0x100000 and uploaded to the TBPs in the file: entry 5 NOTE.GSL (0x76 sectors), 0x17 STAMP.GSL (0x88), 0x18
PUTI.GSL (0x78), 0x19 PUTI.BIN (0x960). (Matched by sector counts against game/SYS.)

FieldMenu_Task, screens 7/9/8/11, on the screen's first frame (+8 == 0): clears two words of its state
(+0x80FC/+0x8100, +0x8104/+0x8108, +0x810C/+0x8110) and `Task_Create(menu task, fn, 0)`. It draws nothing
itself while the child runs; the child covers the whole frame. When the child ends it sets the menu task's
+0x24 = 1 (+4 = -1); FieldMenu_Task then sets screen = **2** (back to the Notebook; its cars drive in again).

- **Screen 8**: FieldMenu_Task runs func_00267248 for 8 exactly as for 11. Nothing in the field-menu code sets
  screen 8 (the Notebook only sets 7, 9 and 11). It is also run by func_0026B848's case 7 (0x0026BB58, root
  result 2's debug path), never in normal play. That it was meant for Memo (NOTE.GSL's label strip has a MEMO row)
  is UNDECIDABLE: only the label hints at it.
- **Screen 10**: not in the switch: it falls to `default`, FieldMenu_Warp. Nothing sets it. The picture viewer is
  **not** a screen; it is a child task of the Pictures task (func_0022BFD0, section 1.6).

Both tasks: arg0 = the task, arg1 = info (+0x0F loader busy, +0x30 u16 pad pressed, +0x34 u16 pad pressed +
repeat). Task fields used: +2 u16 page-turn angle, +8 the task's age in frames (the task system counts it),
+0x20 s32 turn speed, +0x24 child-done flag, +0x25 state, +0x26 u8 cursor, +0x27 flags (bit 1 draw, bit 0 done).
A new task has these zeroed. Done (bit 0): the task sets parent +0x24 = 1, parent +4 = -1 and deletes itself.

PS2 pad bits (info +0x30/+0x34): 0x40 Cross, 0x10 Triangle, 0x1000 Up, 0x4000 Down, 0x8000 Left, 0x2000 Right
(port: kCross, kTriangle, kUp, kDown, kLeft, kRight; +0x34 = Input::Repeated).

## Drawing primitives used (for reading the tables below)

All through the packet D_01762400: func_0024F558(pkt, 0x10000) opens it, func_0024F5C8(pkt, 0, 0) a header,
func_0024F578 sends it.

- **func_0024F640(pkt, table, idx, pal)**: select texture `table[idx]` (16 bytes: TEX0, second word), with
  `pal << 56` added = **CSA (palette)**. All calls here use palette 0.
- **func_0024F728(pkt, n)**: a GIF tag for the next n sprites (a count, not a depth).
- **func_0024F7C8(pkt, defs, n, ox, oy, oz)**: n sprites, def = 32 bytes `{x, y, u, v, w, h, z, flags}` (s32).
  Screen rect (x+ox, y+oy) size |w| x |h|, 1:1 texels from (u, v); w < 0 (or flags & 1) mirrors left-right,
  h < 0 (flags & 2) top-bottom. GS Z = 0xFFFFFF - (z + oz): **a smaller oz is nearer**.
- **func_0024F928(pkt, defs, n, ox, oy, oz)**: n sprites, def = 40 bytes `{x, y, u, v, w, h, rw, rh, z, flags}`.
  Screen size |w| x |h| at 1:1 texels from (u, v) (mirrored if w/h < 0), but the texture **repeats every rw x rh
  texels** from (u, v) (GS REGION_REPEAT, UMSK rw-1, UFIX u); rw/rh 0 = |w|/|h| (REGION_CLAMP, no repeat).
- **func_0024EB10(pkt, quad, 1, ox, oy, oz)**: a textured quad, struct `{x, y, u, v, w, h, z, ?, 4 x vec4 vertex
  (local, floats), vec4 rotation}`; texture rect (u, v, w, h) onto vertices v0 (u,v), v1 (u+w,v), v2 (u,v+h),
  v3 (u+w,v+h) (the order func_00207A28 writes the UVs at pkt +0x20/+0x40/+0x60/+0x80, beside vertices 0-3). Positions in 448 lines (it halves y itself).
- **func_0024F148(pkt, verts, n, ox, oy, oz)**: n vertices of the GIF tag D_0029CC60, PRIM 0x4A = **line strip,
  Gouraud, alpha blend on**; vertex = 32 bytes `{x, y, z, -, r, g, b, a}`.
- func_002063C8 / func_00206040 put x at x*16 + 0x6C00 and y at y*8 + 0x7900: x in pixels, y in 448 lines.

Texture table **D_002DBB50** (TEX0 decoded; picture = converted texture by its dbp):

| idx | TBP | PSM | size | CBP | picture |
|---|---|---|---|---|---|
| 0 | 0x1F2E | T8 | 256x256 (tw/th) | 0x1FDE | picture 0 of the last notebook GSL loaded: **PUTI.GSL/000** (Pictures), **NOTE.GSL/000** (Results), STAMP.GSL/000 (= PUTI's, same hash). 192x192: binder, page pieces, label strip, arrows, pencil tile |
| 1 | 0x1FE2 | T8 | 512x512 | 0x21C1 | NOTE.GSL/001 (368x319) — not used by these screens |
| 2 | 0x21C5 | T8 | 512x256 | 0x233D | NOTE.GSL/002 (512x160) — not used here |
| 3 | 0x1FE2 | T4 | 512x128 | 0x203A | **PUTI.GSL/001** (320x128): page header, photo frame, digits |
| 4 | 0x237A | T8 | 256x256 | 0x243A | the PUTI.BIN picture loaded by the viewer (256x184) |

Thumbnails: built on the stack per photo: T4 64x64, TBP 0x203B + 8i, CBP 0x2042 + 8i, palette 0 =
**PUTI.GSL/(i+2)** (64x47; dbp 0x203B + 8i in PUTI.GSL.txt), i = photo index 0-99.

Note: PUTI.GSL/000 has the Japanese labels (スタンプ / プチクラ / メモ メモ / セイセキ); NOTE.GSL/000 the English
(STAMPS / PICTURES / MEMO / RESULTS). The PS2 shows "プチクラ" on the Pictures screen (04_pictures.png) because
PUTI.GSL replaces NOTE.GSL's picture 0 at TBP 0x1F2E. **Port option:** use NOTE.GSL/000 for the labels to get
"PICTURES" (a localisation slip on the PS2? UNDECIDABLE: PUTI.GSL/000 and STAMP.GSL/000 are the same Japanese strip
and NOTE.GSL/000 the English one, but the intent can't be read from code).

## Shared: the album (func_00264FF0(pkt, kind)) and background

Drawn every frame the draw flag is set, texture D_002DBB50[0] palette 0:

1. **Background** D_002DBDA8 (func_0024F928, oz 0xFFFFF0, the farthest): (0, 0) 640 x 448, u 128 v 128 repeating
   64 x 64 = the pencil tile, 10 x 7.
2. **Album** D_002DBDD0, 0x1D pieces (func_0024F928, oz 2), `x y u v w h rw rh` (z 0, flags 0; w < 0 mirrored):

```
 0   8  32   0  48   32  32  0  0     binder corners / edges (yellow, u 0-64 v 48-132)
 1   8  64   0  80   32 320 32 16
 2   8 384   0  96   32  36  0  0
 3  40  32  32  48  232  32 16 32
 4  40 384  32  96  232  36 16 36
 5 272  32  48  48   48  32  0  0     the rings at the spine (top/bottom), mirrored at 320
 6 272 384  48  96   48  36  0  0
 7 320  32  48  48  -48  32  0  0
 8 320 384  48  96  -48  36  0  0
 9 368  32  32  48  232  32 16 32
10 368 384  32  96  232  36 16 36
11 600  32   0  48  -32  32  0  0
12 600  64   0  80  -32 320 32 16
13 600 384   0  96  -32  36  0  0
14 304  40  32   0   32  16  0  0     the spine (dark centre line)
15 304  56  32  16   32 336 32 16
16 304 392  32  32   32  16  0  0
17  40  40   0   0   16  16  0  0     left page: corners, edges, the light inside (u 0-96 v 0-48 cells of 16)
18  40  56   0  16   16 336 16 16
19  40 392   0  32   16  16  0  0
20  56  40  16   0  248  16 16 16
21  56  56  16  16  248 336 16 16
22  56 392  16  32  248  16 16 16
23 584  40  80   0   16  16  0  0     right page
24 584  56  80  16   16 336 16 16
25 584 392  80  32   16  16  0  0
26 336  40  64   0  248  16 16 16
27 336  56  64  16  248 336 16 16
28 336 392  64  32  248  16 16 16
```

3. **Labels** (func_0024F7C8, 12 sprites, oz 2): u 96, v 80 + 12·kind, 80 x 12, at x = 56 + 80j + 288·(i / 2),
   y = 42 + 352·(i % 2), i 0-3, j 0-2: three across the top and bottom of each page (y 42 and 394). kind 1
   Pictures (プチクラ / PICTURES), 3 Results (RESULTS); 0 Stamps, 2 Memo by the same table. On the Pictures screen
   the page headers cover the top two of each page, leaving one (05 vs 04 screenshots agree).

**Arrows** (when idle and task age & 0x20: 32 frames on, 32 off), texture still entry 0, oz 1:
left D_0032F0A0 (u 82 v 152, 22 x 40) at (30, 204); right D_0032F0C0 (u 104 v 152, 22 x 40) at (588, 204).

**The turning page, func_00264D50(pkt, angle)** (texture entry 0, oz 1: in front of the pages), two quads:

| quad | base (x, y) | texture u v w h | local vertices (x, y) |
|---|---|---|---|
| D_0032EFC0 | (320, 40) | 32 56 56 64 (plain green page) | (4,0) (276,0) (4,368) (276,368) |
| D_0032F030 | (320, 56) | 8 16 32 16 (the light inside) | (4,0) (268,0) (4,336) (268,336) |

So flat it is the right page (x 324-596 by 40-408; inside 324-588 by 56-392). With θ = (s16)angle × π / 32768
(D_003331F4 = π), c = cos θ (func_00274938), s = sin θ (func_00274A10), for each vertex with absolute X0, Y0:

```
X = 320 + (X0 - 320) * c
Y = 224 + (Y0 - 224) * (1 + (X0 - 320) * s * 0.2 / 320)      (0.2 = D_003331F8 / D_00333200)
```

The page swings about the spine (x 320); its outer edge grows by up to 20% mid-turn (perspective). Angle runs
0 → 0x8000 forward (right to left), 0x8000 → 0 backward, 0x800 a frame: **16 frames** a turn. The stepping (state
"turn" below): `angle += speed` (u16); when the result as u16 is > 0x8000 → angle 0, speed 0, back to idle.
(Forward: 0x800 .. 0x8000 then 0x8800 ends; backward: 0x8000, 0x7800 .. 0, then 0xF800 ends.)

## 1. Pictures — func_002665B8 (screen 9)

The photo album: 100 photos, 10 spreads of 2 pages x 5 frames. PUTI.GSL is loaded by the Notebook before the task
starts (loader job 0x40018).

### 1.1 Data

- Cursor: **save +0x12E2** (s16, 0-99; D_0177F760 + 0x12E2). save.md calls it the mail cursor: it is the photo
  album's. Photo_Take (func_0023C5C0) sets it to the photo just taken, so the album opens on the newest photo.
  The task keeps a copy in +0x26 (u8).
- Spread s = cursor / 10 (0-9); pages 2s (left) and 2s + 1 (right), page p shows photos 5p + 1 .. 5p + 5.
- **Photo taken:** Photo_Taken(n) = func_0023D8E8(n): bit n-1 of save +0x508 (two u64, 100 bits); n = 0: all 100.
- Photo record: save +0x54 + 12(n-1): the car's first 12 bytes when taken (paint u32, body, finish, wheel,
  billboard, part flags u16; save_data.h SavePhoto). Used by the viewer.
- How photos are got: dialogue end code 0x11 (p, n): "Say cheese!" at a **Quick-Pic Shop No. p** (message 4 of
  its table) runs Photo_Take(p) and the viewer in take mode (SE 0x306). Pre-code 0x05 tests a photo. All 100 →
  stamp 0x60 ("The picture album is full!") is queued and save +0x530 bit 31 set. From
  data/story/messages.txt, photo p ↔ table (town place):

```
 1-10  (1 18)..(1 27)      11 (21 0)    12 (20 0)
13-16  (11 0)..(11 3)    17-26 (2 19)..(2 28)    27 (17 0)     28-29 (9 21),(9 22)
30-35  (15 0)..(15 5)      36-47 (3 21)..(3 32)    48-51 (13 1)..(13 4)    52-54 (12 1)..(12 3)
55-57  (14 1)..(14 3)      58-59 (16 0),(16 1)     60-64 (4 12)..(4 16)    65-70 (5 5)..(5 10)
71-77  (6 20)..(6 26)      78-79 (18 1),(18 2)     80-83 (19 0)..(19 3)    84-86 (10 1)..(10 3)
87-96  (7 18)..(7 27)      97-100 (8 10)..(8 13)
```

  Photos 11 and 12 are tables (21 0) "Quick-Pic Shop No.11" and (20 0) "No.12": their "Say cheese!" writes the
  photo byte as `\x0B` / `\x0C`. Photo n's scene is PUTI.BIN picture n-1 and its
  thumbnail PUTI.GSL picture n+1 (100 different views).

### 1.2 Slot table D_002DBD30 (10 x 12 bytes: s8 Up, Down, Left, Right; s32 x, y)

Index = cursor % 10. x, y = the frame's top left. The moves are added to the cursor.

| slot | Up | Down | Left | Right | x | y | where |
|---|---|---|---|---|---|---|---|
| 0 | +4 | +2 | -5 | +5 | 186 | 90 | left page, top (right of the header circle) |
| 1 | +2 | +2 | -4 | +1 | 72 | 186 | left, middle left |
| 2 | -2 | +2 | -1 | +4 | 186 | 186 | left, middle right |
| 3 | -2 | -2 | -4 | +1 | 72 | 282 | left, bottom left |
| 4 | -2 | -4 | -1 | +4 | 186 | 282 | left, bottom right |
| 5 | +3 | +1 | -5 | +5 | 360 | 90 | right page, top (left of the circle) |
| 6 | -1 | +2 | -4 | +1 | 360 | 186 | right, middle left |
| 7 | +2 | +2 | -1 | +4 | 474 | 186 | right, middle right |
| 8 | -2 | -3 | -4 | +1 | 360 | 282 | right, bottom left |
| 9 | -2 | -2 | -1 | +4 | 474 | 282 | right, bottom right |

So Left from the left column and Right from the right column go to the previous/next spread (that is what starts a
page turn); Up/Down wrap within the page.

**Cursor move, func_00265FE8(info, cursor)**, every idle frame, from info **+0x34** (pressed + repeat), first of
Up, Down, Left, Right that is held: next = cursor + move. If next == cursor, or next is outside 0-99 (the test
`next != (next + 100) % 100`), the cursor stays (no sound). Otherwise **SE 0x1D** and the cursor moves.

### 1.3 States (+0x25)

| state | |
|---|---|
| 0, 1 | wait for the loader (info +0x0F == 0: PUTI.GSL in); then state 2, draw flag on. (The draw this frame still uses +0x26 = 0, cleared by Task_Create: spread 0 for one frame. Whether that shows is UNDECIDABLE from code: a single frame.) |
| 2 idle | old = save cursor; new = func_00265FE8; save +0x12E2 = new; +0x26 = new. If new/10 > old/10: turn forward (speed +0x800, angle 0), state 5. If new/10 < old/10: turn back (angle 0x8000, speed -0x800), state 5. Else the buttons (+0x30): **Cross**: photo +0x26 + 1 taken → **SE 0x1A**, start the viewer `Task_Create(task, func_0022BFD0, cursor)`, state 4; not taken → **SE 0x1C** (buzzer). Else **Triangle**: **SE 0x1B**, loader jobs **0x40005** (NOTE.GSL back) and **0xA0000 \| save +0x1A** (the meter part's graphics at TBP 0x237A: PUTI.BIN, job 6, loads there; PUTI.GSL ends at 0x235A), state 6. |
| 3 | wait for the loader, then state 4. Never entered: func_002665B8 stores only 2, 4, 5 and 6 to +0x25 (a leftover). |
| 4 viewer | the album isn't drawn (background only, under the viewer). When the viewer ends (task +0x24 set by the child): **SE 0x1A**, state 2. |
| 5 turn | angle += speed (u16); past 0x8000 → angle 0, speed 0, state 2. |
| 6+ (default) exit | draw flag off; when the loader is idle (info +0x0F == 0): done flag → the task ends (parent +0x24 = 1, +4 = -1): back to the Notebook. |

### 1.4 Drawing (draw flag set), back to front

1. Background, then if state != 4: the album (kind 1) — section "Shared".
2. Turning (speed > 0): func_00264D50(angle), page 2s - 2 (the old left page), page 2s + 1 (the new right page).
   Turning back (speed < 0): func_00264D50(angle), page 2s, page 2s + 3 (the old right page). The turning page
   has the nearer depth (oz 1), so it covers whichever page it is over. (Port: draw the two pages, then the turning
   page.)
3. Idle (speed 0): the arrows if age & 0x20 — left if 2s > 0, right if 2s < 18 (s < 9); the **red cursor box**:
   func_0024F148 with D_002DC3C0 at the slot's (x, y), oz 2: line strip (0,0) (93,0) (93,64) (0,64) (0,0), every
   vertex RGBA (255, 0, 0, 128) (= opaque red): a 94 x 65 outline exactly on the frame; then pages 2s and 2s + 1.

**Page p, func_002660F8(pkt, p)** (texture D_002DBB50[3] = PUTI.GSL/001, palette 0, all oz 2; even p = left
page, odd = right):

- Header (the corner circle and curve): u 0 v 0, 180 x 104 at y 42; x 40 (even), or x 420 mirrored (w -180, odd).
- Page number p + 1 (1-20) in 20 x 20 digits (u = 20·digit, v 108; func_0024F4A0 packs the digits,
  func_0024F4E0 counts them): right digit first at x0 + 10n - 10, each further digit 20 left (n digits), y 78;
  x0 = 88 (even) or 532 (odd): **centred on x 98 / 542** (the circle's centre).
- The 5 frames: slots (p & 1)·5 .. +4: D_0032F1A0 = u 180 v 0, 94 x 66 at the slot's (x, y).
- The photo numbers 5p + i + 1 under each frame, same digits: centred on x = slot.x + 47, top y = slot.y + 70.
- For each photo 5p + i + 1 taken (i 0-4): its thumbnail, D_0032F1C0 via func_0024EB10: texture PUTI.GSL/(5p+i+2)
  whole (u 0 v 0, 64 x 47) stretched to **94 x 66** at the slot's (x, y) — over the frame.

Checked against 04_pictures.png (no photos taken): frames at (186, 90) etc., numbers 1-10, page numbers 1 and 2 in
the circles, red box on slot 0.

### 1.5 Sounds

0x1D cursor move, 0x1A Cross on a taken photo, 0x1C Cross on an empty frame, 0x1A again when the viewer closes,
0x1B Triangle (leave).

### 1.6 The viewer — func_0022BFD0 (child task, arg = photo index 0-99; | 0x10000 = take mode from a message)

**First frame** (+8 == 0):
- Loader job **0x60000 | idx**: reads 0xC000 bytes at offset idx × 0xC000 of **SYS/PUTI.BIN** (file entry
  D_0029B084, sectors 0x3EF12, 0x960 long) into 0x100000 and relocates its GS uploads by +0x237A: the picture
  lands at TBP 0x237A, CLUT 0x243A (PUTI.BIN.txt dbp 0 / cbp 0xC0). That's how/when PUTI.BIN is loaded: one
  256 x 184 T8 picture, by the viewer, each time it opens.
- Album mode (no 0x10000): D_0177EA5C = &photo record (save +0x54 + 12 idx), loader job **0x70301** (the car
  from that record into car slot 3: func_0020CD10 takes the slot from job >> 8, the count from the low byte). Take mode: the current car, no load.
- The picture packet D_017523F0: TEX0 0x2004874E_2131237A (= D_002DBB50[4]: T8 256x256 at 0x237A, decal), one
  sprite u 0 v 0 256 x 184 drawn **2x: (64, 40) to (576, 408), 512 x 368** (func_00206650 + func_002075F0).
- The car: matrix D_017525D0 = identity, turned about y by D_0029F320 +0x4C (0.0), at D_0029F320 +0x40 (0, 0, 0)
  (D_0029F320 is 16-byte places {x, y, z, yaw}: +0x40 is place 4, the origin with yaw 0; the
  viewer's sceVu0RotMatrixY at 0x00272C38 takes +0x4C); car +0x194 = 0.85 (D_00332E20) if the record's part flags & 0x400 (big
  tires), else 0.

**Every later frame** (while info +0x0F, the loader, is idle): Light_Setup(1, 0); Camera_SetScreenCenter(2048,
2048) (the screen centre); Camera_BuildMatrices with camera D_0029F3B0 = eye (1.4, 2.0, -20.7), focal 1588, pitch
0x380, yaw 0x9880; Light_SetHighlight; sends the picture packet (12 qwords); func_002266E8, func_00223200 (the cars'
GS state and the light: no shadow), then func_002236D8(car, take mode ? 0 : 3): Car_Draw at distance 0 in colour set
0, so the close-up WHEEL.BIN wheels (func_00221570) unless the big tires. The album's car is the photo's record's
(loaded into car slot 3), the take mode's the save's car 0 (just copied into it). The port: story/photo.cpp
(Photo_DrawCar), from the album (FieldMenu) and from the room after a talk's code 0x11 (MessageTask::PhotoShown). Cross or
Triangle (+0x30 & 0x50): album mode → the task ends (parent +0x24 = 1, +4 = 1); take mode → loader 0xA0000 |
save +0x1A and goes on in func_0022C5F0. No sound of its own (the album plays 0x1A).

Background during the viewer: the album task still draws the pencil tile (state 4), so it frames the picture.

## 2. Results — func_00267248 (screens 11 and 8)

The record book, **14 spreads** (pages 0-27): pages 0-25 the Adventure's races, 26-27 the mini-games. Always
opens on spread 0 (+0x26 starts 0). Uses NOTE.GSL (texture entry 0 = NOTE.GSL/000).

### 2.1 States (+0x25)

| state | |
|---|---|
| 0, 1 | draw flag on, state 2, and straight on into state 2 the same frame |
| 2 idle | s = Menu_MoveCursor(info, s, 14, 1, 0, 0): 14 columns x 1 row, **Left/Right**; a press wraps (13 → 0, 0 → 13), a repeat stops at the ends; **SE 0x1D**. new > old: forward turn (speed 0x800, angle 0), state 3; new < old (also the wrap 0 → 13): back (angle 0x8000, speed -0x800), state 3. Else **Triangle**: **SE 0x1B**, draw flag off, done → the task ends at once (back to the Notebook). Cross does nothing. |
| 3 turn | as Pictures' state 5 (16 frames) |
| 4 | nothing (done already set) |

### 2.2 Drawing (draw flag set)

Sprites (one packet): background, album **kind 3** (RESULTS labels, all six on each page: no header here), then
turning → func_00264D50(angle); idle → arrows if age & 0x20: left if 2s > 0, right if **2s < 26** (none on the
last spread). Packet sent (func_0024F578). **Then the text** (Text_Print / Text_PrintAt, drawn after and over the
sprites, including over the turning page), so only the page the turning page isn't over is printed:

| | left side (x 50 / text col 2) | right side (x 330 or 350 / col 16) |
|---|---|---|
| idle | page 2s | page 2s + 1 |
| forward, angle < 0x4000 | page 2s - 2 (old left) | — |
| forward, angle ≥ 0x4000 | — | page 2s + 1 (new right) |
| back, angle > 0x4000 | — | page 2s + 3 (old right) |
| back, angle ≤ 0x4000 | page 2s (new left) | — |

A page < 26 is a race page, func_00238A48(page, side); 26 is the left mini-game page, 27 the right one.

### 2.3 Race page — func_00238A48(r, side) and func_00237680

Row r of the course table D_002BDDD0 (16 bytes: +0 course name index, +3 class). col c = 2 (left) or 16 (right).
Text_Print(col, row, str, palette): x = 20·col + 10, y = (12·row + 5)·2 (448 lines). All text **palette 3** (the
dark blue in 05_results.png).

- Course name D_002BE398[row +0] at (c, 2) → **(50 | 330, 58)**.
- Class D_0029FDB0[row +3] at (c + 7, 3) → **(190 | 470, 82)**: "  Rank C", "  Rank B", "  Rank A", "GrandPrix",
  "Special" (the rank strings start with two spaces).
- Then three blocks, k = 0 the player, 1 teammate 1, 2 teammate 2, at text row R = 4 + 4k (y 106, 202, 298), each
  only if the best place **save +0x0FF0 + 26k + r != 0xFF**:
  - the name at (c, R): k 0 the save's name (+0x62C); k 1/2 the teammate recorded for this race: (group, index)
    = s8 pair at save +0x33C0 + 4r + 2(k - 1); group 0 → no name (NULL string); else D_002C22C8[group][index]
    (16-byte rows) +0xC.
  - "Best Rank" (D_002EC4F0) at (c + 1, R + 1); the place D_0029FD30[place] (" 1st" .. "24th") at (c + 10, R + 1)
    → x 210 | 490.
  - "Best Lap" (D_002EC500) at (c + 1, R + 2); Time_Print(20c + 170 = 210 | 490, half-line 12(R+2) + 5 → y =
    24R + 58, **save +0x1040 + 4(26k + r)**, palette 3).
  - "Best Time" (D_002EC510) at (c + 1, R + 3); Time_Print(210 | 490, y = 24R + 82, **save +0x1178 + 4(26k + r)**).
  - (So x: name 50/330, labels 70/350, values 210/490; y of block k: name 106 + 96k, then +24, +48, +72.)

The 26 race rows (r: name, class):

```
 0 Peach Raceway Rank C        1 Peach Raceway Rank B         2 Peach Raceway II Rank A
 3 Ninja Temple Raceway Rank C 4 Temple Raceway Rank B        5 Temple Raceway Rank A
 6 Desert Raceway Rank C       7 Desert Raceway Rank B        8 Night Glow Raceway Rank B
 9 Night Glow Raceway Rank A  10 Miner 49er Raceway Rank C   11 Miner 49er Raceway Rank A
12 Lava Run Raceway Rank B    13 Lava Run Raceway Rank A     14 River Raceway Rank C
15 Slick Track Rank C         16 Oval Raceway Rank B         17 Slick Track Rank A
18 Snow Mountain Raceway B    19 Snow Mountain Raceway A     20 Sunny Beach Raceway B
21 Sunny Beach Raceway A      22 Lagoon Raceway B            23 Lagoon Raceway A
24 Endurance Run Special      25 Tin Raceway Special
```

(Spread 0 = Peach Raceway C | B, as 05_results.png shows with no results yet: only the two header lines.)

### 2.4 Mini-game pages (text via Text_PrintAt(x, y/2, ...): the code passes y/2; below in 448 lines)

Each record has a lock: while its **area bit is set** (save +0xBD8 areas: a place not yet found) it prints only
"      ????????" (D_0032F250) at its (x, y) but keeps its space. "%" strings are sprintf'd. Values are right-aligned
by an assumed character width w: x_value = x + 240 - w·strlen(text) (the trailing spaces in "%dm  " count).

**Page 26 (left), x = 50:**

| y | record | lock | lines |
|---|---|---|---|
| 58 | "Figure 8" (D_002BE3FC) | +0xBE4 & 0x40 | "Step%d    " i = 1-5 at (70, 58 + 24i); Time_Print(210, 58 + 24i, **save +0x12B4 + 4(i-1)**) |
| 250 | "Ski Jumping" (D_002BE424) | +0xBF0 & 0x80 | "Result" at (70, 274); "%dm  " of **save +0x12C8**, w 10, at y 274 |
| 346 | "Curling" (D_002BE418) | +0xBF0 & 0x40000 | "Result" at (70, 370); "%d P  " of **save +0x12CC**, w 10, at y 370 |

(Figure 8 takes 168 lines, Ski 72, then the next starts 24 lower: 58 → 250 → 346.)

**Page 27 (right), x = 350:**

| y | record | lock | lines |
|---|---|---|---|
| 58 | "Golf" (D_002BE3F4) | +0xBEC & 8 | rows i 0-9 at y 82 + 24i: label "Hole%d    " (i + 1) for i < 9, "Total" for i 9, at x 370; value s8 **save +0x12D0 + i** (+0x12D9 the total): 0x7F → "---", 0 → "0", else "%+d"; w 16, at x 590 - 16·len |
| 346 | "Q's Rally" (D_0032F260) | +0xBFC & 0x100000 | "Result" at (370, 370); **save +0x12B0**: 0x7FFFFFFF → "--:--", else "%d:%02d" of v / 9000, (v % 9000) / 150 (clock hours:minutes); w 16, at y 370 |

All palette 3.

### 2.5 Sounds

0x1D page (Menu_MoveCursor), 0x1B Triangle. No sound for the page turn itself.

## 3. Port notes

- Textures: PUTI.GSL/000 (or NOTE.GSL/000, see above) for background, album, arrows, turning page; PUTI.GSL/001
  for headers, frames, digits; PUTI.GSL/002-101 thumbnails; PUTI.BIN/000-099 the big pictures. NOTE.GSL/000 for
  Results.
- The PS2 reloads NOTE.GSL and the meter on leaving Pictures; the port needn't (texture sets stay loaded).
- The viewer needs the photo's car (record +0x54 + 12 idx: body, paint, parts flags) drawn with gfx::Model over
  the 512 x 368 picture, camera D_0029F3B0. The exact camera mapping is **not verified**; get a PCSX2 capture of a
  photo (a pnach that sets save +0x508 bits) to place the car.
- Save fields: +0x508/+0x510 (photo bits), +0x54 (records), +0x12E2 (album cursor), +0x0FF0/+0x1040/+0x1178
  (race records), +0x33C0 (teammates per race), +0x12B0-+0x12D9 (mini-games), +0xBD8 areas (locks). The port's
  SaveRecord has photoTaken[2] and mailCursor (rename it: it's the album cursor).
- names to add (docs/names.csv): func_002665B8 Pictures_Task, func_0022BFD0 Photo_ViewTask,
  func_00265FE8 Pictures_MoveCursor, func_002660F8 Pictures_DrawPage, func_00264FF0 Notebook_DrawAlbum,
  func_00264D50 Notebook_DrawTurningPage, func_00267248 Results_BookTask, func_00238A48 Results_DrawRacePage,
  func_00237680 Results_DrawRaceRecord, func_00266F10 Results_DrawFigure8, func_00266CE8 Results_DrawSkiJump,
  func_00266DF8 Results_DrawCurling, func_00267060 Results_DrawGolf, func_00266B78 Results_DrawQsRally,
  func_0024F928 Sprite_DrawRepeat, func_0024F7C8 Sprite_Draw, func_0024F640 Sprite_SetTexture,
  func_0024F148 Line_DrawStrip (suggestions; func_0023C5C0 Photo_Take and func_0023D8E8 Photo_Taken are in
  messages.md already).
- field_menu.cpp calls func_0024F728 a depth; it is a sprite count (GIF tag). Depth is the oz argument.
