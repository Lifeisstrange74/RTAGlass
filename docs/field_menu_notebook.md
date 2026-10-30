# Spec: the field Start menu's Notebook (screen 2) and Stamps (screen 7)

Reference for Notebook and its Stamps book: what the PS2 does, read from the asm (field_menu.md has the summary; the port is field_menu_screens.cpp). "GUESS" marks what wasn't read from
the code or seen on the PS2.

Sources: asm `reference/asm/nonmatchings/cod/func_00215220.s`, `func_002135C8.s`, `func_00212190.s`
(Menu_DrawRoad), `func_00217148.s` (FieldMenu_Task), `func_00265B30.s`, `func_002652B8.s`, `func_00264D50.s`,
`func_00264FF0.s`, `func_00206D28.s` (Menu_MoveCursor), `func_0020DC50.s`; the reference decomp\'s best-attempt C for
func_0024F7C8, func_0024F928, func_00206040, func_00264FF0, func_00265128, func_0024EA00, func_0023D628. Tables
dumped from the ELF (the texts are src/game/field/field_stamps.inc). Checked against `captures/adventure/menu_ps2/01_notebook.png`,
`02_stamps.png`, `02_stamps_r.png` (game area in those window captures: x 50..1016, y 52..776).

All coordinates are in the 640 x 448 frame (x pixels, y frame lines). Sprite calls (func_0024F7C8 /
func_0024F928 / func_0024EB10) take frame-line y; the GS packet halves it itself (func_00206040: y*8 + 0x7900, x*16 +
0x6C00). Text goes through Text_PrintHalf (func_0024F530) = Text_PrintAt(x, y/2, ...), so its y is also frame lines;
the port's `Text_PrintAt(d, x, y, ...)` takes frame lines, so pass the values below as they are.

## 0. Helpers worth knowing (verified)

- **func_0024F7C8(pkt, SprDef *s, n, ox, oy, oz)**: SprDef is 8 words: x, y, u, v, w, h, z, flags. Sprite at
  (s.x+ox, s.y+oy), size w x h, texels u..u+w, v..v+h (1:1). flags bit 0 mirrors left-right, bit 1 top-bottom (a
  negative w/h does the same). Prim z = 0xFFFFFF - (s.z + oz): bigger oz = farther.
- **func_0024F928(pkt, Spr *s, n, ox, oy, oz)**: Spr is 10 words: x, y, u, v, w, h, sw, sh, z, flags. Screen size
  w x h with texels 1:1, but if sw (or sh) is nonzero and differs from |w| (|h|) the GS CLAMP is REGION_REPEAT: the
  tile u..u+sw-1 (v..v+sh-1) repeats across the w x h rectangle (func_00206040). I.e. **tiling, not stretching**.
  Negative w/h = mirrored.
- **func_0024F640(pkt, table, i, csa)**: selects TEX0 pair i of a 16-byte-per-entry table; csa goes into TEX0 bits
  56+ (CSA = the palette).
- **func_0024F728(pkt, n)**: NOT a depth. It opens a batch of n sprites (every call is followed by exactly n sprite
  prims: 0xAB = 90+15+66 road pieces, 0x46 = 70 tiles, 0x1D = 29 book pieces, 0x30 = 48 line tiles, 6 = 3 tabs + 3
  labels). (Strong inference from the counts.) So draw order = call order; z (from oz) decides overlap.
  (field_menu.cpp's comment calling 0xAB/0x7B/7 depths is wrong but harmless.)
- **Menu_MoveCursor func_00206D28(pad, sel, nCols, nRows, padIndex, wrap)**: col = sel / nRows, row = sel % nRows;
  Right/Left (0x2000/0x8000) change the column, Up/Down (0x1000/0x4000) the row; result col*nRows + row.
  wrap != 0: a *press* (+0x30) wraps, then repeats (+0x34) stop at the ends; wrap == 0: repeats only, stop at the
  ends. SE 0x1D on a change. Port: `Menu_MoveCursorGrid(input, 0, sel, cols, rows, noWrap = !wrap)`.
- **Task_Create func_00204C48(task, fn, arg)**: child task. **func_0020DC50** (the loader request): sets info+0x0F = 1
  (loader busy until the load ends), hands `arg` to the loader, and if arg lacks bit 0x100000 sets info+0x28 |=
  0x400000 (the display keeps the last picture, course_select.md). Deletes itself at once.
- Pad: info+0x30 u16 pressed (0x40 Cross, 0x10 Triangle, 0x1000 Up, 0x2000 Right, 0x4000 Down, 0x8000 Left), +0x34
  repeated. Port: platform::kCross/kTriangle/kUp/... in `src/platform/input.h`.

## 1. FieldMenu_Task's screens 7-11 (jtbl_002EAD60, verified)

| screen | entry |
|---|---|
| 7 | Stamps: child task **func_00265B30** (book of stamps) |
| 8 | child task **func_00267248** (same as 11; nothing in Notebook sets 8: unused here; the other caller is func_0026B848's case 7, root result 2's debug path) |
| 9 | Pictures: child task **func_002665B8** (the photo album; save.md: "func_002665B8 browses") |
| 10 | not handled: falls into the default = FieldMenu_Warp (0x00217358). Unused. |
| 11 | Results: child task **func_00267248** (minigame/results; uses the same book: func_00264FF0, func_00264D50, D_0032F0A0/C0 arrows) |

For 7, 9 and 8/11: on the screen's first frame (+8 == 0) clear two words (7: +0x80FC/+0x8100; 9: +0x8104/+0x8108;
8/11: +0x810C/+0x8110) and Task_Create(menuTask, fn, 0). Every frame (also the first): if menuTask+0x24 != 0 (the
child finished: it sets parent+0x24 = 1, parent+4 = -1 and deletes itself), screen asked for (+4) = 2: **back to the
Notebook**. The menu task itself draws nothing on these screens; the child draws everything.

## 2. Notebook (screen 2): func_00215220, update func_002135C8

Arguments: arg0 menu task, arg1 info, arg2 = D_01727B90 (work), arg3 = D_01727B90 + 0x80F0 (state: +0 state,
+4 counter, +8 cursor 0..2). Cursor +8 is not cleared on entry (only +0, +4), so it stays where it was when coming
back from Stamps/Pictures/Results; FieldMenu_Open zeroes the whole work area, so it is 0 (Stamps) each time the menu
opens.

### First frame (work +8 == 0)
- state = 0, counter = 0.
- Menu_PlaceCars(work, cars +0x820C, labels {2}, items 0, 0): only the title car, label 2 "Notebook" (POSE picture 1),
  at (0x2A2, 12), as in field_menu.md (title palette etc.).
- Road struct (+0x81C8): flag |= 1, x = 0, y = 0, z = 0xFFFFF0 (farthest), kind (+0x10) = **2**.
- Panel struct (+0x81A4): flag |= 1, x = 0, y = 0, z (+0xC) = 1.

### Every frame: only if info+0x0F == 0 (loader idle). Otherwise NOTHING (no update, no draw) that frame.

**Update func_002135C8 (arg3 state):**

| state | action |
|---|---|
| 0, 1 | counter = 16, state = 2, and fall into 2 the same frame |
| 2 | title car x (+0x8210) -= 40; if --counter == 0: state 0x64, counter 0. (16 frames: 674 -> 34) |
| 0x64 | state = 0x65, fall into 0x65 the same frame |
| 0x65 | Cross (0x40): Sound_PlaySe(0x1A), state 0xC8. Else Triangle (0x10): Sound_PlaySe(0x1B), state 0x12C. Else cursor = Menu_MoveCursor(pad, cursor, 1, 3, 0, 1): **Up/Down over the 3 tabs, a press wraps**, SE 0x1D. No animation on a move: only the highlight changes. |
| 0xC8 | state 0xC9, counter 16, fall into 0xC9 |
| 0xC9 | title car x -= 40; when --counter == 0: state **0x12B**, counter 0 (the car drives off LEFT, 16 frames) |
| 0x12C | state 0x12D, counter 16, fall in |
| 0x12D | title car x += 40; when --counter == 0: state **0x18F** (back off right, 16 frames) |
| other | state 0, counter 0 |

Returns the state. Start, Select and Left/Right do nothing.

**After the update (func_00215220):**
- 0x12B: cursor 0 (Stamps): Task_Create(func_0020DC50, 0x40017) (load SYS/STAMP.GSL) and screen = 7. Cursor 1
  (Pictures): load 0x40018 (SYS/PUTI.GSL), screen = 9. Cursor 2 (Results): **no load**, screen = 0xB (11).
  Neither 0x40017 nor 0x40018 has bit 0x100000, so the display holds the last Notebook picture while loading.
- 0x18F: screen = 0 (main screen; its cars drive in again, cursor where it was: "Notebook").
- The Notebook is still drawn on the frame that returns 0x12B / 0x18F (the switch happens next frame).

### Draw (every frame the loader is idle), in call order
1. func_0024F558(pkt, 0x8000); func_0024F5C8(pkt, 0, 0) (begin).
2. **Menu_DrawPanel** (panel struct, count 1): exactly the main screen's panel and borders (field_menu.cpp
   Menu_DrawPanel). Screenshot agrees.
3. **Menu_DrawRoad, kind 2** (road struct at (0,0), z far): texture D_0029C030 entry 5 (palette 0), a batch of 0x46
   sprites, template D_002EAB08 = {x 0, y 0, u 128, v 128, w 64, h 64}: **10 columns x 7 rows of 64 x 64 tiles at
   (64c, 64r)**, c 0..9, r 0..6 (640 x 448). Entry 5 = **NOTE.GSL picture 0** (192 x 192; tbp 0x1F2E cbp 0x1FDE);
   u128 v128 is its blue pencil tile. (For the record: kind 3 = POSE pic 3 pal 5 D_002EAB28 10x7 of 64x66 (Radio);
   kind 4 = entry 3 pal 5 D_002EAB48 {u144 v48 48x48} 14 x 10 tiles of 48 (0x8C sprites); kind 5 = entry 4 pal 0,
   zeroed template with w = h = 0x80, 5 x 4 tiles of 128 x 128 at u0 v0 (0x14 sprites); kinds 0, 1 and >= 6 the
   road. jtbl_002EABD0 = {road, road, 2, 3, 4, 5}.)
4. **Menu_DrawCars** (cars +0x820C, count 11): only the title car is on (label 2 "Notebook", title palettes). No
   sign, no Help_Draw, no scroll arrows.
5. Texture D_0029C030 **entry 6** (palette 0) = **NOTE.GSL picture 1** (368 x 319 converted; tbp 0x1FE2 cbp 0x21C1,
   T8 512 x 512 on the GS). Batch of 1: **the notebook** D_002EAC08 = {0, 0, u 152, v 0, w 216, h 270} at
   ox 0x78, oy 0x68: **(120, 104), 216 x 270, u 152 v 0**. This picture already shows the "LICENSE ... CLASS C"
   card printed on the cover.
6. If **license = save +0x651 (s8, D_0177FDB1) > 0**: batch of 1, sprite {x 0x24, y 0x20, u 0, v (license-1)*104,
   w 0x98, h 0x68} at (0x78, 0x68): **(156, 136), 152 x 104, u 0, v 0 / 104 / 208** for 1 = Class B, 2 = Class A,
   3 = Super A (cards stacked in picture 1's left column). Covers the printed Class C card. License 0 = Class C
   (nothing extra drawn).
7. Texture D_0029C030 **entry 7** (palette 0) = **NOTE.GSL picture 2** (512 x 160; tbp 0x21C5 cbp 0x233D). Batch of
   6; for i = 0..2 (0 Stamps, 1 Pictures, 2 Results), all at ox 0x78, oy 0x68:
   - the tab: {x 0xD8, y 0x18 + 0x54 i, u (i == cursor ? 0x160 : 0x180), v 0, w 0x1A, h 0x38} =
     **(336, 128 + 84 i), 26 x 56, u 352 (light green, chosen) / 384 (dark green)**, v 0;
   - its label: {x 0xD8, y 0x20 + 0x54 i, u (i == cursor ? 0xB0 : 0), v 40 i, w 0xB0, h 0x28} =
     **(336, 136 + 84 i), 176 x 40, u 176 (yellow text, white box, the pin and its line to the tab) / u 0 (green
     text on grey)**, v 0 / 40 / 80. (Picture 2 also has a 4th label "Memo" at v 120; never drawn.)
8. func_0024F578(pkt) (end).
z: road farthest (0xFFFFF0), the rest oz 0. Port order: road, panel, car, notebook, license, tabs + labels.

Checked against 01_notebook.png: notebook left edge 120, tab x 336, "Pictures" grey box at ~376 (u 0 + 40), cards
and tab rows at 128/212/296 match; the Stamps label shows the pin. No sound other than 0x1A/0x1B/0x1D.

## 3. Stamps (screen 7): task func_00265B30

A child task of the menu task (arg0 = task: +8 age, +0x18 parent, +0x24, +0x25 state, +0x26 u8 old page, +0x27 u8
flags (bit 1 draw, bit 0 end), +0x02 u16 turn angle, +0x20 s32 turn speed; arg1 = info). Draws into its own packet
D_01762400. Textures: table **D_002DBB50** entry 0 = tbp 0x1F2E cbp 0x1FDE T8 256 x 256 = **STAMP.GSL picture 0**
(192 x 192; same image as PUTI.GSL/000.dds: the book pieces, page corner circle, Japanese headers スタンプ /
プチクラ / メモ メモ / セイセキ at v 80 + 12k, line tile, arrow diamond, pencil tile). Note: NOTE.GSL picture 0 at the
same address has English headers (STAMPS/PICTURES/MEMO/RESULTS), but loading STAMP.GSL overwrites it, so the PS2
shows **スタンプ** (screenshot agrees). Stamp pictures: STAMP.GSL pictures 1..100 (T4 64 x 64, picture k at tbp
0x1FE2 + 9(k-1), cbp +8).

Save fields: **+0x12E0 s16 stamp cursor** (D_01780A40; the left page, always even 0..98; read and written: the book
reopens at the last spread, persisting in the save), **+0x518** stamp earned bits, **+0x528** stamp text shown bits
(stamp s 1-based = bit s-1; page n (0-based, 0..99) shows stamp n+1).

### State machine (+0x25), every frame
- First frame (age 0): +0x26 = (u8) cursor.
- **0, 1**: if info+0x0F == 0 (STAMP.GSL loaded): state 2, flags |= 2 (drawing on). Until then nothing is drawn
  (the display holds the Notebook).
- **2**: spread = cursor / 2. new = Menu_MoveCursor(pad, spread, 50, 1, 0, 0): **Left/Right repeat (held repeats
  too), stops at spreads 0 and 49, no wrap**, SE 0x1D. cursor (save) = new * 2.
  - new > spread: speed = 0x800, angle = 0, state 3 (page turns forward, right to left).
  - new < spread: angle = 0x8000, speed = -0x800, state 3 (turns back).
  - else if Triangle pressed: Sound_PlaySe(0x1B), Task_Create(func_0020DC50, **0x40005**) (reload NOTE.GSL: D_0029AF58[5] is LBA
    0x3DC49, /SYS/NOTE.GSL on the disc), state 4.
  - Cross, Start, Up/Down do nothing.
- **3**: angle (u16) += speed; if angle > 0x8000 (unsigned; backwards it wraps 0 - 0x800 = 0xF800): speed 0, state 2,
  +0x26 = cursor, angle 0.
- **4 and above**: flags &= ~2 (no drawing; display frozen since 0x40005 lacks 0x100000); when info+0x0F == 0:
  flags |= 1 -> parent+0x24 = 1, parent+4 = -1, Task_Delete. The menu then returns to the Notebook (its title car
  drives in again; cursor still on Stamps).

Turn timing: the state-2 frame that starts the turn is drawn already in state 3 (forward angle 0, back 0x8000);
then 16 more frames (forward 0x800..0x8000, back 0x7800..0); the next update ends it and that frame draws the new
spread normally. So **17 frames of turning page**, then the normal spread.

### Draw (if flags & 2), in call order
1. func_0024F558(D_01762400, 0x10000); func_0024F5C8(0, 0); texture D_002DBB50 entry 0 palette 0.
2. **Background** D_002DBDA8 via func_0024F928, oz 0xFFFFF0 (farthest): {x 0, y 0, u 128, v 128, w 640, h 448,
   sw 64, sh 64} = the pencil tile u128 v128 64 x 64 **tiled** over 640 x 448 (same look as Menu_DrawRoad kind 2).
3. **func_00264FF0(pkt, 0)** - the open book (arg = header row: 0 Stamps; Pictures/Results presumably 1/3):
   - 29 pieces D_002DBDD0 (func_0024F928, oz 2). Format x, y, u, v, w, h, sw, sh (sw/sh 0 = no tiling; negative w =
     mirrored):
     ```
     cover:  ( 8, 32) u0  v48  32x32          ( 8, 64) u0  v80  32x320 tile 32x16   ( 8,384) u0  v96  32x36
             (40, 32) u32 v48 232x32 tile16x32 (40,384) u32 v96 232x36 tile 16x36
             (272,32) u48 v48  48x32          (272,384) u48 v96 48x36
             (320,32) u48 v48 -48x32 (mirror) (320,384) u48 v96 -48x36 (mirror)
             (368,32) u32 v48 232x32 tile16x32 (368,384) u32 v96 232x36 tile 16x36
             (600,32) u0 v48 -32x32 (mirror)  (600,64) u0 v80 -32x320 tile 32x16 (mirror)  (600,384) u0 v96 -32x36 (mirror)
     spine:  (304,40) u32 v0 32x16   (304,56) u32 v16 32x336 tile 32x16   (304,392) u32 v32 32x16
     left page:  (40,40) u0 v0 16x16   (40,56) u0 v16 16x336 tile16x16   (40,392) u0 v32 16x16
                 (56,40) u16 v0 248x16 tile16x16  (56,56) u16 v16 248x336 tile 16x16  (56,392) u16 v32 248x16 tile16x16
     right page: (584,40) u80 v0 16x16 (584,56) u80 v16 16x336 tile16x16 (584,392) u80 v32 16x16
                 (336,40) u64 v0 248x16 tile16x16 (336,56) u64 v16 248x336 tile16x16 (336,392) u64 v32 248x16 tile16x16
     ```
     The book covers x 8..632, y 32..420 (screenshot agrees).
   - 12 headers, func_0024F7C8 oz 2: sprite {u 0x60, v 0x50 + 12*arg, w 80, h 12} at
     x = 56 + 288*(i/2) + 80j, y = 42 + 352*(i%2) for i 0..3, j 0..2: **x 56, 136, 216, 344, 424, 504; y 42 and
     394**, u 96 v 80 (スタンプ), 80 x 12.
4. If state 3: **func_00264D50(pkt, angle)** (the turning page, below), then ONE page's contents:
   - forward (speed > 0): angle < 0x4000: page (+0x26 & ~1) (the old left page); else page (cursor | 1) (the new
     right page).
   - back (speed <= 0): angle >= 0x4001: page (+0x26 | 1) (the old right page); else page cursor (the new left page).
   (The other half shows the bare book. No arrows while turning.)
5. Else (not turning):
   - if task age & 0x20 (32 frames on, 32 off): left arrow if cursor > 0: D_0032F0A0 {0,0, u 82, v 152, 22, 40} at
     (0x1E, 0xCC) = **(30, 204)**; right arrow if cursor < 0x62 (98): D_0032F0C0 {u 104, v 152, 22 x 40} at
     (0x24C, 0xCC) = **(588, 204)**; oz 1. (The two halves of the green diamond.)
   - page cursor (left) and page cursor | 1 (right).
6. func_0024F578 (end).

### One page: func_002652B8(pkt, n), n 0..99; side = n & 1 (0 left, 1 right)
1. Texture D_002DBB50 entry 0. **Page-number corner**: {x side ? 0x210 : 0x30, y 0x38, u 0, v 0x88, w side ? -64 :
   64, h 0x38}, oz 2: **(48, 56) or (528, 56), 64 x 56, u 0 v 136, mirrored on the right page**.
2. **Page number**: sprintf("%d", n + 1); Text_PrintHalf(x = (side ? 0x242 : 0x54) - 10 * strlen, y 0x44, colour 3):
   right-aligned to x 84 (left) / 578 (right), **y 68**, colour 3 (dark blue). Screenshot: "1" at x ~74, "2" at ~568.
3. **Stamp frame**, 9 pieces D_002DC258 (func_0024F928) at ox side ? 0x120 : 0, oz 2:
   ```
   ( 80, 80) u96  v0  32x32    (112, 80) u128 v0  128x32 tile 16 wide    (240, 80) u144 v0  32x32
   ( 80,112) u96  v32 32x112 tile 16 tall  (112,112) u128 v32 128x112 tile 16x16  (240,112) u144 v32 32x112 tile 16 tall
   ( 80,224) u96  v48 32x32    (112,224) u128 v48 128x32 tile 16 wide    (240,224) u144 v48 32x32
   ```
   (the rounded frame, x 80..272 + 288 side, y 80..256).
4. **Text lines' rules**: sprite {u 0x60, v 0x80, w 0x14, h 0x18} (48 sprites, oz 2) at x (side ? 0x158 : 0x38) +
   20k, y 0x110 + 24r, k 0..11, r 0..3: **x 56 / 344 .. +240, y 272, 296, 320, 344**, tile u 96 v 128 20 x 24.
5. If Stamp_Earned(n + 1) (save +0x518 bit n): **func_00265128(pkt, n, 0)**: TEX0 STAMP picture n+1 (T4 64 x 64,
   tbp 0x1FE2 + 9n, cbp 0x1FEA + 9n), a quad (func_0024EB10, oz 0) centred on **(176 + 288*side, 166)**, half-size s
   = (lv/5 + 1) * 64 = 64 for lv 0: **128 x 128 from (112 + 288 side, 102)**, the whole 64 x 64 picture (2x).
   (lv > 0 is the stamp-earned pop-up's growth, not used here.) func_0024EB10 maps u,v,w,h onto the quad's
   corners TL, TR, BL, BR (field_menu_books.md; func_00265128 sets the vertices (-s,-s), (s,-s), (-s,s), (s,s)).
6. If the text-shown bit (save +0x528 bit n, func_0023D6A0(n+1)): the text D_002DBBA0[n] split on '#'
   (func_0024EA00: line i = the field after the (i+1)-th '#'; every string starts with '#'), each line
   Text_PrintHalf(x **60 + 288*side**, y **272 + 24 i**, colour 3) until no more fields. Earned and shown are
   separate: 02_stamps.png shows stamp 1's text without its picture (shown set by message code 0x15, the stamp
   still in the queue).

### The turning page: func_00264D50(pkt, angle)
theta = angle * pi / 32768 (D_003331F4 = pi, x 2^-15); c = cos(theta), s = sin(theta) (func_00274938 = cosf (its
pi/4 kernel split), func_00274A10 = sinf: fdlibm's quadrant table k_sin, k_cos, −k_sin, −k_cos). Two quads (func_0024EB10, count 1, oz 1,
i.e. in front of the page contents), current texture (D_002DBB50 entry 0):

| quad | origin (x, y) | uv rect | vertices (vx, vy), TL TR BL BR |
|---|---|---|---|
| D_0032EFC0 | (320, 40) | u 32 v 56, 56 x 64 (the dark green page board) | (4,0) (276,0) (4,368) (276,368) |
| D_0032F030 | (320, 56) | u 8 v 16, 32 x 16 (light paper) | (4,0) (268,0) (4,336) (268,336) |

Each vertex, with X0 = x - 320 (= 0) and Y0 = y - 224 (-184, -168):
```
fx  = vx + X0
vx' = fx * c - X0
vy' = (vy + Y0) * (fx * s * 0.2 / 320 + 1) - Y0      (D_003331F8 = D_00333200 = 0.2)
screen = (x + vx', y + vy')
```
So the page hinges on the spine x = 320: at angle 0 it lies on the right half (324..596), at 0x4000 it is edge-on,
at 0x8000 it lies mirrored on the left (44..316); while lifted its outer edge grows taller about the frame's middle
line y 224 (up to x1.17 at 90 degrees). The uv rects are stretched over the quads (no tiling). No sound for the
turn other than the cursor's 0x1D. Draw order for the port: background, book, headers, page contents, turning page,
arrows, text on top.

## 4. Stamp texts (D_002DBBA0[0..99], '#' = line break; index = stamp id - 1 = page)

```
 1 Visited/all the houses/in Peach Town!        51 My City is now/complete!
 2 Got/a local Peach Wine!                      52 Visited/all the houses/in Chestnut Canyon!
 3 Got/a Peach doll!                            53 Maya Carton/gave me her/painting!
 4 Took Kevin home!                             54 Got/a Model Train!
 5 Delivered/a wallet to/a police station!      55 Completed the/Volcano Course!
 6 Cleared/Barrel Dodging!                      56 Completed the/Volcano Course/within 1min 10sec!
 7 Gave/a magazine!                             57 Completed/Rock Climbing!
 8 Gave/a big peach/to James!                   58 Completed/Rock Climbing/within 2min!
 9 Collected/7 gemstones!                       59 Came up with/a new greeting!
10 Visited/all the houses/in Fuji City!         60 Visited/all the houses/in White Mountain!
11 Got/a Gold Ornament!                         61 Got/a Christmas Tree!
12 Got/a Policemen's Club!                      62 Got/Arctic Pattern!
13 Found/the location of/Treasure Hunting/Maze! 63 Solved the/ghost mystery!
14 Found/the treasure in/Treasure Hunting/Maze! 64 Found/a Papu Flower!
15 Completed/Treasure Hunting/Maze within 3min! 65 Delivered/a package/to Jousset!
16 Beat the King/in the/Sliding Door Race!      66 Jumped over 140m/in the/Ski Jumping!
17 Saved Otomi/of Dumpling Cake/Shop!           67 Got a present/from Santa Claus!
18 Met Iwasuke!                                 68 Opened/millionaire/Keitel's safe!
19 Listened to/Hakosuke's/trumpet!              69 Played Curling!
20 Had my fortune/told!                         70 Scored over/200points/in Curling!
21 Beat Natsuo/in the/Highway Race!             71 Fulfilled/White Mountain/policeman's wish!
22 Completed/the Highway Race/in less than 50sec!  72 Visited/all the houses/in Papaya Island!
23 Visited/all the houses/in Sandpolis!         73 Got/an Unbabo Doll!
24 Got/a Mini-Tower!                            74 Got/a Papaya Ukulele!
25 Got/a toy gun!                               75 Ran through the/Obstacle Course!
26 Impressed Lisalisa!                          76 Completed the/Obstacle Course/within 2min!
27 Beat Johnny in/the Drag Race!                77 Listened to/Papu Tree's story!
28 Completed/the Drag Race/within 8sec!         78 Beat Micky in/Beach Flag!
29 Played/Roulette!                             79 Mayor is/motivated!
30 Caught Butch!                                80 Delivered/a Fluffy Mushroom/to Shirley!
31 Delivered/a soccerball/to Mr.King!           81 Found Kerori!
32 Figure 8 step 1/Did not hit any/traffic cones!   82 Caught 20 fish!
33 Figure 8 step 2/Did not hit any/traffic cones!   83 Woke sleepy/Casa up!
34 Figure 8 step 3/Did not hit any/traffic cones!   84 Gave/the love letter/to Minerva!
35 Figure 8 step 4/Did not hit any/traffic cones!   85 Visited/all the houses/in Cloud Hill!
36 Figure 8 step 5/Did not hit any/traffic cones!   86 Got/Angel's Wings!
37 Played Soccer!                               87 Got/God's Rod!
38 Won the/soccer game!                         88 Beat Travis/in the Single/Lap Race!
39 Found Benji's/fountain pen!                  89 Answered/all questions/correct!
40 Got my own/garage!                           90 Cleared the/Rainbow Jump!
41 Ms.Flower's/flower bloomed!                  91 Visited/all the houses/in Mushroom Road!
42 Sleepy Brian/gave me a body!                 92 A goddess gave/me a horn!
43 Played the/Tunnel Race!                      93 Played/all holes in/Golf!
44 Beat Akiban/in the/Tunnel Race!              94 Played Golf/below 36!
45 Climbed to/the top of/the Big Tower!         95 We now have/a fan club!
46 Fulfilled/Coine's wish!                      96 The picture/album is full!
47 Cleared the/Which-way? Race!                 97 Guessed the/correct number/of windmills!
48 Cleared/Q's Rally!                           98 Met the/Black Bus Orpheus!
49 Participated/in the/Endurance Race!          99 Made a first/contact!
50 A statue is/built!                          100 Became/the President!
```
(Exact strings incl. trailing spaces: src/game/field/field_stamps.inc; e.g. the Figure 8 ones end in "! ".)

### What gives each stamp (func_0023C8A8 queues a stamp id; it is set as earned later, func_0023D6E0)
Callers of func_0023C8A8 in the ELF (verified call sites; conditions only partly traced):
- **Message_Task (0x0023B168), message code 0x0D**: queues the listed ids on the press. Most stamps come from the
  towns' dialogue scripts this way (ids live in the message data, not in a table here). The text-shown bit comes from
  message code **0x15** (Stamp_SetTextShown), also script-driven (messages.md).
- func_0022D1E0 (My Garage, first frame with the loader idle): **40** "Got my own garage!" if not earned.
- Results_DrawRace (0x002364B0): **49** "Endurance Race" when race id (+0x20) == 0x18; **95** "fan club" when
  D_01787008+2 < 6 and stamp 40 earned: car 0's place (0-based) in the prize places, with My Garage.
- func_0023C5C0 (taking a picture): **96** when all 100 photo bits (+0x508) are set.
- func_0023CB68 (My City resident moves in): **51** when +0x670 becomes 0x3FFFFFF.
- func_0022AEB0: **100** "Became the President!" (messages.md: the cinema path).
- The items some talks wait for come from the field: Tim's wallet (stamps 5, then 3 by the Voucher the police give)
  and the big peach (stamp 8) from Peach_Draw and Peach_FallTask (field.md, "Objects").
- func_0025A120 (the Figure 8 minigame: event table D_002BE018's course 0x19 row): **32..36** from D_002C3A40 = {32, 33, 34, 35, 36} by step.

## 5. Port notes
- Notebook: reuse Menu_PlaceCars / Menu_DrawPanel / Menu_DrawCars; add Menu_DrawRoad kind 2 (NOTE pic 0 pal 0, 10 x 7
  of 64 at u128 v128). Title car only, label 2. No loads needed on PC: switch screens directly (the PS2's frozen
  frame during loads can be skipped).
- Stamps: book pieces from **STAMP.GSL picture 0** (Japanese headers, as on the PS2) or NOTE picture 0 (English
  headers) - choice; the PS2 shows Japanese. Stamp k = STAMP.GSL picture k.
- The book (func_00264FF0, func_00264D50, the arrows D_0032F0A0/C0) is shared with Results (func_00267248) and
  presumably Pictures (func_002665B8): write it once.
- Save: read/write `stampCursor` (+0x12E0, even), read `stampEarned`, `stampShown`, `license` (+0x651).
