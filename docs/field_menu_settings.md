# Field Start menu: Settings (button configuration) — spec

**The PC no longer runs this screen:** the field menu's Settings item opens the PC's Controls screen
(controls.md), which binds actions straight to keys and controller buttons in the same style. This file stays as
the record of what the PS2 does (and of the parts the Controls screen reuses: the layout, the pad, the box).

Reference for the Settings screen (the key setup): what the PS2 does, read from the asm (field_menu.md has the summary; the port is field_menu_screens.cpp). "GUESS" marks what wasn't read from
the code or seen on the PS2.

Sources: `func_00216738` (asm `reference/asm/nonmatchings/cod/func_00216738.s`, checked instruction by
instruction), `func_00214470` (asm, read in full), `func_00213C40`, `func_00214078`, `func_00214220` (asm),
`func_002173D0` (byte-matched C), ELF tables dumped, `SYS/OPTION.BIN` parsed, the screenshot
`captures/adventure/menu_ps2/08_settings.png` measured (the game area in that window is x 49..1017, y 52..776: 1.5125 px/px across,
1.616 down, so it's 4:3-stretched; every position below was checked against it unless it's marked GUESS).

Coordinates: x in pixels, y in **frame lines (448)** unless I say "half-lines". The rules:
- `Window_DrawFrame` (0x00202350) takes y and h in half-lines (port: `Window_DrawFrame(d, x, yHalf, w, hHalf, style)`).
- `Text_PrintHalf` (0x0024F530) takes y in frame lines and calls `Text_PrintAt(x, y/2)`; the port's
  `Text_PrintAt(d, x, y)` takes frame lines, so use the y values below as they are.
- `func_0024FAD0` (bar + horn), the sprites (`func_0024F7C8`) and the line strip (`func_0024F148` ->
  `func_00207A88`: GS x = 0x6C0 + x, GS y = 0x790 + y/2) all take frame lines.

## 0. Names

| Address | Suggested name | What it does |
|---|---|---|
| 0x00216738 | `FieldMenu_Settings` | the screen: its first frame, the update, all the drawing |
| 0x00214470 | `Settings_Update` | the state machine and the pad; returns its state |
| 0x00213C40 | `Settings_ReadSave` | the save's key setup -> each row's choice (`sel[row]`) |
| 0x00214078 | `Settings_WriteSave` | each row's choice -> the save's key setup (Apply) |
| 0x00214220 | `Settings_Defaults` | "Initial settings": the choices set to the defaults; the save isn't changed |
| 0x002173D0 | `Settings_CancelTask` | the "Cancel? / Yes / No" window task |
| 0x002174E0 | `Settings_Task` | a standalone task that runs FieldMenu_Open, then screen 5 only, and ends when it leaves. Unused: nothing in either ELF segment points to it (no word, no `jal` 0x0C085D38, no lui/addiu pair) |
| 0x0024F148 | `Prim_DrawLineStrip` | n coloured vertices as one Gouraud line strip (PRIM 0x4A) |
| 0x0024F728 | `Sprite_BeginBatch` | a GIF tag for the next n sprites. **It isn't a depth**: field_menu.md calls it one, which is wrong |
| 0x00272A28 / 0x00272A38 | `sceVu0ITOF4Vector` / `sceVu0ITOF0Vector` | int to float, /16 and /1 |
| 0x00272A18 | `sceVu0FTOI0Vector` | |
| 0x00272988 | `sceVu0ScaleVector` | |
| 0x00272A48 | `sceVu0UnitMatrix` | |
| 0x00272AE8 | `sceVu0RotMatrixZ` | |
| 0x00272CE0 | `sceVu0RotMatrix` | Z, then Y (0x00272C38), then X: M = Rx·Ry·Rz·M |
| 0x00272728 | `sceVu0ApplyMatrix` | |
| 0x00273268 | `Vec3_Copy` | |
| 0x00223238 | `Model_DrawSlot(cam, matrix, slot)` | draws the MISC model in loader slot `slot` |

## 1. Data

### Work area (arg3 = D_01727B90 + 0x8130, 0x6C bytes; state 0 zeroes it)
| Off | |
|---|---|
| +0x00 | state |
| +0x04 | frame count for the title car's drive |
| +0x08 | row cursor, 0-11 |
| +0x0C | horn cursor shown (0/1) |
| +0x10 + 4r | `sel[r]` for r = 0-8: an index into row r's option list (not the option id) |
| +0x34 + 4r | `warn[r]` for r = 0-11. Rows 0-8: this row's button is also used by another row. Rows 9 and 10 are always 0. `warn[11]` (= +0x60) is "some row clashes" |
| +0x64..+0x6A | s16 pad rotation (x, y, z, w); only x, y, z are animated |

### D_0029C618 — the rows (12 x 0x3C): {s32 x, s32 y, s32 options[12] (ending in -1), char* label}
x is always 0. `D_0029C620` = `D_0029C618 + 8`, which is the options array (`D_0029C620[row*15 + i]`).

| r | y | label | options (option ids) | save field |
|---|---|---|---|---|
| 0 | 0 | Gas Pedal | 0 1 2 3 4 5 7 6 8 9 | +0x12E8 accel (buttons[0]) |
| 1 | 24 | Brake Pedal | same | +0x12EA brake [1] |
| 2 | 48 | Reverse | same | +0x12EC reverse [2] |
| 3 | 72 | Jet Fire | same | +0x12EE R2 action [3] |
| 4 | 96 | Change View | same | +0x12F6 camera [7] |
| 5 | 120 | Horn & Lights | same | +0x12F4 horn [6] **and** +0x12F8 lights [8] (written equal) |
| 6 | 144 | Wing Up | same | +0x12F0 L1 action [4] |
| 7 | 168 | Wing Down | same | +0x12F2 L2 action [5] |
| 8 | 192 | Navigator | 0-9 as above, then 10 | +0x12FA map [9] |
| 9 | 240 | Initial settings | (list 0..10, unused) | |
| 10 | 264 | Cancel change | (unused) | |
| 11 | 288 | Apply | (unused) | |

Rows 0-7 have 10 options (note L2 (7) comes before L1 (6)); Navigator also has 11, SELECT. Start is never one of
them.

### D_0029C538 — the options (12 x 0x14): {u16 padBit, u16 0, s32 boxDx, s32 boxDy, s16 rotX, rotY, rotZ, rotW}
| id | button | bit | box (dx, dy) | rotation target (x, y, z) | icon cell |
|---|---|---|---|---|---|
| 0 | Circle | 0x0020 | (98, 0) | 0,0,0 | u 128 v 0 |
| 1 | Cross | 0x0040 | (79, 18) | 0 | u 128 v 20 |
| 2 | Square | 0x0080 | (60, 0) | 0 | u 128 v 40 |
| 3 | Triangle | 0x0010 | (79, -18) | 0 | u 128 v 60 |
| 4 | R1 | 0x0008 | (78, -28) | -8192, 1024, 0 | u 128 v 80 |
| 5 | R2 | 0x0002 | (80, -32) | -12288, 1024, 0 | u 128 v 100 |
| 6 | L1 | 0x0004 | (-78, -28) | -8192, -1024, 0 | u 168 v 0 |
| 7 | L2 | 0x0001 | (-80, -32) | -12288, -1024, 0 | u 168 v 20 |
| 8 | Up (d-pad) | 0x1000 | (-79, -14) | 0 | u 168 v 40 |
| 9 | Down (d-pad) | 0x4000 | (-79, 14) | 0 | u 168 v 60 |
| 10 | SELECT | 0x0100 | (-24, 0) | 0 | u 168 v 80 |
| 11 | (none) | 0 | (0, 0) | 0, 0, 1 | not used by any list |

Icon cell: u = 0x80 + 40·(id / 6), v = 20·(id % 6), 40 x 20, in SYS/OPTION2.GSL picture 0 palette 0 (checked
against the texture: the left 128 x 128 is the background tile; the icons are two columns from u 128).

### D_0029C9B0 — the help text, `[row*2 + warn[row]]` -> a list of {char*, colour} ending in a null
- Rows 0-8, warn 0: "Change button" / "settings with" / "directional" / "buttons" (colour 0)
- Rows 0-8, warn 1: "      Warning!" (colour 6) / "Using the" / "same buttons"
- Rows 9 (both): "Changes back" / "to the" / "initial" / "setting"
- Rows 10 (both): "Cancels" / "changes made," / "and undo"
- Row 11, warn 0: "Apply and End"; warn 1: "      Warning!" (6) / "Using the" / "same buttons" / "Cannot apply"

(The leading spaces of "Warning!" are in the string.)

### Other constants
- D_0029CA10, the camera: pos (0, 0, -2, 1), focal +0x10 = 3564.0, angles 0, flags +0x1C = 0 (so the rotation
  comes from the angles: identity, looking along +z).
- D_00332B3C/40 = 2198.0, 2018.0: the 3D screen centre in GS coordinates, which is **(470, 164) in the frame**
  (2198 − 0x6C0, (2018 − 0x790)·2). That's (0x1D6, 0xA4), the same base the box uses.
- D_00332B44 = D_00332B48 = π/2048 (applied after ITOF4, so an s16 unit is π/32768: 0x10000 is a full turn).
- D_002EACC0, the box: 5 vertices {s32 x, y, z, w; s32 r, g, b, a}: (−10,−10) red (255,0,0,128); (−10,10) yellow
  (255,255,0,128); (10,10) red; (10,−10) yellow; (−10,−10) red. It's a closed 20 x 20 square.
- Strings D_00333558/60/68: "Cancel?", "Yes", "No".

## 2. The save, and who reads it

The record is `D_0177F760 + player·0x3448` (player = the task's +0x1C, which is pad 0 here), and `buttons[10]` sits
at +0x12E8 (already in `SaveRecord::buttons`, see save.md). Each entry holds **one PS2 pad bit** (D_0029C538's).
The mapping from rows to fields is in the table above. Defaults (Save_ResetButtons 0x00229F20, already ported):
Cross, Square, Circle, R2, L1, L2, R1, Triangle, R1, Select. They match the screenshot.

Readers (already in the port's race.cpp): `Car_ReadPad` (accel, brake, reverse, R2 = jet, L1/L2 = wings, lights =
entry 8), `Car_HornHeld` (6), `Car_CameraButton` (7), `Car_MapButton` (9, the field map zoom). They read the record
every call, so whatever Apply writes takes effect at once. Apply only writes RAM; the save is kept by the game's
usual saves. Up/Down d-pad can be chosen; the port's `Pad_Buttons` already maps 0x1000/0x4000.

## 3. State machine (`Settings_Update`, returns the state)

The pad is `info + 0x30 + 2·player` (pressed). `Menu_MoveCursor` (0x00206D28) has the arguments
`(pad, sel, cols, rows, player, wrap=1)`: sel is column-major (col = sel / rows, row = sel % rows). With wrap set,
a press moves and wraps; otherwise the repeat moves and stops at the ends. Each move plays SE 0x1D. A 1-long axis
doesn't move. In the port that's `Menu_MoveCursor(input, pad, sel, count, false)` (Up/Down) and
`Menu_MoveCursorGrid(input, pad, sel, count, 1, false)` (Left/Right).

- **0**: zero the work area (0x6C); `Settings_ReadSave`; then, in the same frame, **1**: count = 16, state 2;
  **2**: the title car's x (D_01727B90+0x8210, car 0) −= 40; when count reaches 0, go to 0x64. So the title drives
  in over 16 frames (x 674 → 34).
- **0x64**: +0xC = 1 (the horn shows), state 0x65, and the pad is read in the same frame.
- **0x65**, in this order:
  1. `row = Menu_MoveCursor(pad, row, 1, 12, player, 1)`: Up/Down over the 12 rows (wraps on a press).
  2. **Triangle (0x10)** (checked on the row just moved to): SE 0x1B; +0xC = 0; compare `sel[0..8]` with what the
     save holds (`Settings_ReadSave` into a copy; it's a memcmp of 0x24 bytes).
     - The same: state 0x12C (leave).
     - Different: start `Settings_CancelTask` as a child task (pad = player); row = 10 ("Cancel change"); state 0x66.
  3. Otherwise, by row:
     - **9, Initial settings**, on Cross (0x40): SE 0x1A, then `Settings_Defaults` (below).
     - **10, Cancel change**, on Cross: SE 0x1A; `Settings_ReadSave` (the edits are thrown away); recompute the warnings.
     - **11, Apply**, on Cross: if `warn[11]` (some row clashes), play **SE 0x1E** (refused) and nothing else.
       Otherwise SE 0x1A, `Settings_WriteSave`, +0xC = 0, state 0x12C.
     - **0-8**: `sel[row] = Menu_MoveCursor(pad, sel[row], n, 1, player, 1)`, where n = the length of the row's
       list (10, or 11 for Navigator). Left/Right steps through the options and wraps. Then recompute the
       warnings. Cross does nothing on these rows.
- **0x66**, the Cancel window: wait for the task's +0x24 (child finished; the task runner clears it each frame).
  Then the result (task +4) is 0 = Yes: SE 0x1A, state 0x12C (leave, the edits lost). Anything else (No, or
  Triangle in the window): SE 0x1B, state 0x64 (the horn comes back; the cursor stays on row 10).
- **0x12C**: count = 16, state 0x12D; **0x12D**: title car x += 40; at 0 → **0x18F**.
- Anything else: state 0, count 0.

`FieldMenu_Settings` on 0x18F: D_01727B90+4 = 0 (back to the main screen; its cars drive in again).

**Warnings (recomputed after Cancel, Initial, and every Left/Right on rows 0-8; *not* at state 0):**
```
any = 0
for i in 0..11: warn[i] = 0
    if i < 9: for j in 0..8, j != i:
        if opt(i) == opt(j): warn[i] = 1; any = 1; break      // opt(r) = D_0029C620[r*15 + sel[r]]
if any: warn[11] = 1                                          // warn[11] was just cleared by the loop
```
It compares option ids, not pad bits. Clashing rows' labels turn colour 6, and so does Apply when any clash. The
help text switches to the Warning! version.

**Settings_ReadSave**: for each row r of 0-8, k = the first option id whose padBit == the row's save field, then
`sel[r]` = the position of k in row r's list. It loops forever on a bit that isn't in the table or the list (a save
can't hold such a value through this screen; the port should clamp to 0). Rows 0-8 use fields 12E8, 12EA, 12EC,
12EE, 12F6, 12F4, 12F0, 12F2, 12FA.
**Settings_WriteSave**: field(r) = D_0029C538[opt(r)].padBit for rows 0-8; row 5 writes both +0x12F4 and +0x12F8.
**Settings_Defaults** (net effect): A = ReadSave(); Save_ResetButtons(); sel = ReadSave() (the defaults); recompute
the warnings; WriteSave(A) (the save gets its old setup back); sel = the defaults. So only the pending choices
change; Apply is still needed.

**Settings_CancelTask** (func_002173D0): on its first frame `WindowAlloc`; lines "Cancel?" (flags 0), "Yes" (0x10
item), "No" (0x10 item); `sel` (+0xD) = 1, so the cursor starts on **No**; pad (+0xC) = the task's arg (the player);
y = 0x9E (half-lines, 316 frame lines); x = 320 − w/2. The default style (from WindowAlloc). After that, each frame
`Window_Update`: a non-zero result ends it (−1, Triangle, forces sel = 1). Then parent +4 = sel, parent +0x24 = 1,
and the task deletes itself. The menu screen keeps drawing under it (the parent runs first, then the child).

Sounds: 0x1D cursor and option moves; 0x1A Cross on rows 9/10, Apply accepted, Yes; 0x1E Apply refused; 0x1B
Triangle, No/cancel. (The window's own sounds come from the port's Window.)

## 4. Drawing (`FieldMenu_Settings`, every frame)

The whole screen (update and drawing) is skipped while `info+0xF` is set. That's the loader's busy flag, which
func_0020DC50 sets while the preloaded files (SYS/OPTION.BIN, OPTION2.GSL) arrive. The PC doesn't need it (the Map
screen has the same check).

**First frame (+8 == 0):** state = count = 0; `Menu_PlaceCars(labels = {5}, items 0)`: the title car alone with
"Settings", as the Radio does with 3. The road entry (+0x81C8): on, x = y = 0, +0xC = 0xFFFFF0 (its z), kind 5. The
panel entry (+0x81A4): on, +0xC = 1, x = y = 0.

Then `Settings_Update`, and draw, in this order:

1. `Window_DrawFrame(40, 0x2C, 0x104, 0x9C, style 2)`: the list window (cyan, orange edges), inner box
   x 40..300, y 88..400.
2. `Window_DrawFrame(0x154, 0x8C, 0x104, 0x3C, style 0)`: the help window (blue), inner x 340..600, y 280..400.
3. `func_0024FAD0(40, 88 + rowY[cur], 0x104, +0xC)` = `Window_DrawBar(60, 88 + rowY, 240)`, and with +0xC
   `Window_DrawCursor(20, 88 + rowY)`. The port's Window_DrawBar and Window_DrawCursor take these frame-line
   values directly. On screen, row 0's bar runs from x 52 to 308 (with its caps) and from y 88 to 112.
4. The 12 labels: `Text_PrintAt(60, 90 + rowY[r], label, warn[r] ? 6 : 0)`. The y values are 90, 114 ... 282 for
   rows 0-8, then 330, 354, 378. (Colour 6 is the palette the port's Window uses for disabled lines: TEX2's CSA picks FONTE
   palette 6 (Text_PrintAt 0x00203FC8), a salmon pink face (240,136,144) on a black outline.)
5. The help lines of `D_0029C9B0[cur*2 + warn[cur]]`: `Text_PrintAt(360, 292 + 24k, str, colour)`.
6. **The 3D pad** (section 5).
7. The sprite list (on PS2 it goes into the work area +0x20; its z sorts it, so in the port draw it in this order):
   - `Menu_DrawPanel`, as the other screens do.
   - `Menu_DrawRoad` **kind 5** (new): OPTION2.GSL picture 0 palette 0, 5 columns x 4 rows of the 128 x 128 tile
     (u 0, v 0) at (x + 128c, y + 128r) from the road entry's (0, 0). That covers 640 x 512 and doesn't scroll. Its
     z is the farthest (0xFFFFFF − 0xFFFFF0 = 0xF), so the pad and everything else draw over it. **Draw it first.**
   - `Menu_DrawCars` (the title car).
   - The 9 icons (`Sprite_BeginBatch(9)`; the texture is D_0029C030 entry 4 = OPTION2.GSL, palette 0): for rows
     0-8, sprite (240, 90 + 24r), 40 x 20, at the option's cell (u = 128 + 40·(id/6), v = 20·(id%6)),
     id = opt(r).
8. **The box**, only while `cur < 9`. The 5 vertices of D_002EACC0, turned about z by
   `a = ((frame & 0x3F) − 32)·π/32`, where frame is D_01727B90+8 (the screen's frames). That's a full turn every 64
   frames, starting at −π. (The bit work, `(((f&63)−32) << 26) >> 16`, then ITOF4 and ×π/2048, gives exactly
   this.) Rounded with FTOI0, and offset to `(0x1D6 + boxDx, 0xA4 + boxDy)` of D_0029C538[opt(cur)]. Drawn as one
   **Gouraud line strip** (PRIM 0x4A = LINESTRIP | IIP | ABE; alpha 128 makes it opaque), 1 pixel wide, from red
   corners to yellow ones. It's at the nearest z, over the pad. The direction of the turn: sceVu0RotMatrixZ with
   y down on the screen comes out clockwise (sceVu0RotMatrixZ 0x00272AE8, then ApplyMatrix:
   x' = c·x − s·y, y' = s·x + c·y). The offsets are fixed per button: they don't follow the pad's tilt (and in the
   middle of a tilt the box is already at the new place). Checked: for Cross the box's centre measures (549, 183),
   and 470 + 79, 164 + 18 gives (549, 182).
9. The sprite list is flushed (func_0024F578).

## 5. The 3D pad

**File:** the main screen's Cross on any item runs `func_0020DC50(0x3C0300)`. That's loader kind 0xC
(func_0020C9D0, "MISC"): index 0x0300 & 0xFF = 0 of the table D_0029B428, i.e. **SYS/OPTION.BIN** (LSN 255526,
83 sectors, 169536 bytes), into slot 0x0300 >> 8 = **3** (D_003FBB80 + 3·0x34000, with
`D_0177F310[3]` = file + word 0). Its GS uploads are moved to TBP 0x243E + 0x44·slot.

**OPTION.BIN layout** (car-file-like):
- header {u32 0x10: the model section, u32 0x25130: the GS uploads (a GSL-style chain, uploaded by func_00207B50),
  u32 0x29640: the file's end};
- the section at 0x10: {0x250E0, 0x25100, 0, 0}: offsets of light chains A and B, both with count 0, so there are
  none;
- the main chain at **0x20**: one DMA cnt (qwc 0x250B), then ret at 0x250E0. It's all **program 0x20 lit strips**
  (STCYCL; UNPACK V4-32 x1, the GIF tag; UNPACK V3-32 x4N: position, normal, colour 0-255, STQ; MSCAL 4):
  **437 strips, 2906 vertices**, PRIMs 0x264/0x26C/0x274 (tristrips, some textured, some Gouraud, all with ABE and
  context 2). Bounds x ±0.0733, y −0.0571..+0.0349, z ±0.0281. Colours are greys (0-130) and red (255,0,0: the
  LED).
- The texture: one T8 128 x 128 (already converted: `data/textures/SYS/OPTION.BIN/000.dds`, dbp 0, cbp 0x40): the
  ○ ✕ △ □ symbols and "SELECT", "START", "ANALOG". Func_00223238's TEX0 is TBP 0x250A, TBW 2, PSMT8,
  TW = TH = 7, TCC 1, TFX modulate, CBP 0x254A (= file cbp 0x40), CLD 1. That's exactly modelconv's
  `FileTexture(0x13, 0x40)`. The GS state is the cars' D_0029D240 (alpha test GEQUAL, z test GEQUAL, bilinear,
  clamp, normal alpha).
- **The port:** modelconv's `ConvertPad` writes SYS/OPTION.BIN as one section (`WriteChain(b, 0x10 + 0x10, 0, 0,
  FileTexture(0x13, 0x40), ...)`) to `models/SYS/OPTION/0.obj` (field_menu.md).

**Per frame (func_00216738):**
1. `Light_Setup(2, 0)`: kind 2 takes its light directions from **D_0029E980**: light 1 from (3, 0.5, 1), light 2
   from (−1, −1, 0) (each negated and normalized by func_00272E08, as the port does for the courses). The colour
   sets are as for course 0: set 0 = D_0029D500[0] = light 1 0.6, light 2 0.25, highlight 0.6, ambient 0.4 (grey).
   The model is drawn with set 0 (func_00226868(D_0177F620) = D_0177F5E0 + 0x40). There's no time of day and no
   fog. The port's `Light_Setup(RaceLight&, course)` has no kind; it needs a kind-2 variant
   (`dir = −normalize(D_0029E980 rows)`, `sets[0] = kSets[0]`). Note that on the PS2 this overwrites the field's
   light, and Adventure_Task calls Light_Setup again when the menu closes.
2. `Camera_SetScreenCenter(2198, 2018)`, then `Camera_BuildMatrices(D_0177E6D0, D_0029CA10, I, I)`. The camera is
   at (0, 0, −2), looks along +z with up +y, with focal 3564. The y focal is ×0.47 on the PS2's 224-line field,
   which is **0.94 in frame lines**, as `gfx::Camera3D` uses elsewhere. The centre is (470, 164). Then
   `Light_SetHighlight` with the camera's forward (0, 0, 1).
3. The tilt toward the button: the target is D_0029C538[opt(cur)].rot (x, y, z) when cur < 9, otherwise (0,0,0).
   Each of +0x64/+0x66/+0x68 moves **±0x200 a frame** toward its target (targets are multiples of 0x400, so it
   lands exactly). The angle in radians is s16 · π/32768, so a frame's step is π/64. The targets come out as:
   R1/L1 x = −π/4, R2/L2 x = −3π/8, y = +π/32 for R1/R2 and −π/32 for L1/L2. A tilt to R1 takes 16 frames and to R2
   24. The other buttons go back to facing front.
4. The model matrix is `sceVu0RotMatrix(I, rot)`, i.e. M = Rx(rx)·Ry(ry)·Rz(rz) with v' = M·v (yaw in the model
   first, then pitch), and translation 0. Then `func_00223238(cam, M, slot 3)` draws it. Negative x brings the top
   edge, the shoulder buttons, toward the camera: sceVu0RotMatrixX (0x00272B90) is the standard Rx, so R1's −π/4
   takes (0, h, 0) to (0, 0.707h, −0.707h), towards the camera at z = −2.
5. Then the screen centre goes back to (2048, 2048) for the 2D.

Scale check: the pad's width is 0.1467 x 3564 / 2 ≈ 261 px, and it measures about 258. Its top is +0.035 (≈ 58
lines above 164) and its bottom −0.057 (≈ 95 below). That matches the screenshot's pad, about 107..265, so **model
+y is up on screen**, as in the port's other models.

Port drawing: `gfx::Model::Draw(d, cam, window = the whole frame, style)` with `style.transform` = M (3x4),
`style.lighting` = `Light_CarSet(kind-2 light, 0)` with paint white, and `clearDepth = true`. Draw it after the
road and before the box.

## 6. What the port needed (all in now: field_menu_screens.cpp, modelconv's ConvertPad, Light_SetupMenu)

1. A screen enum `kScreenSettings = 5` in `FieldMenu`. Today item 5 goes to kScreenNotYet. Also `Settings` state
   (the work area above) and `FieldMenu_Settings`, `Settings_Update`, `Settings_ReadSave`, `Settings_WriteSave`,
   `Settings_Defaults`, the warnings check, and `Settings_Draw`. Names go into names.csv.
2. `Menu_DrawRoad` kind 5 (the OPTION2.GSL tile).
3. The Cancel window: a `Window` with "Cancel?" / Yes / No, sel = 1, y 0x9E, centred x, pad 0. The port's
   `Window` already has the parts (see how CourseSelect or others run a child window).
4. modelconv: SYS/OPTION.BIN → models/SYS/OPTION/0.obj (section 5).
5. Light kind 2 (D_0029E980) for `Light_Setup`.
6. A **line primitive** in `gfx::Draw2D`: there's Sprite/Quad/Fill/Triangles, but no line. Build the 1-pixel
   Gouraud line strip from thin `Triangles` quads with per-vertex colours, or add a line call.
7. The SE 0x1E (refused) sound is probably already in the SE bank; I haven't checked whether the port plays it
   anywhere.
8. Pad input: the screen reads the raw pad (Cross/Triangle/arrows), not the remapped setup. Car_ReadPad and
   friends already read `buttons[]` live.
