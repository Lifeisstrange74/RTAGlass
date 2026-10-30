# Q's Factory "Change parts": research notes

Sources: asm in reference/asm/nonmatchings/cod (func_0022BB88, func_00246250, func_0023BF28, func_0023C150,
func_0023C418, func_00243C50, func_00244F60, func_0022E960, func_00205350), table contents dumped from the ELF
(SLUS_203.98; scripts/gen_parts_tables.py writes the port's parts_tables.inc) and PCSX2 (factory_menu.p2s, with
the save's part bits poked through PINE to see full carousels). "m2c" = taken from the m2c draft without a full asm
check. "GUESS" = guess.

**In the port:** `game/story/parts_screen` (PartsScreen: the list, the carousel, Part_Fit = func_0023C418,
Part_Name = func_00243468), the scroll list in `game/ui/window` (Window::SetList, Icon_Draw = func_00203E70), the
cards' scaled names (Text_PrintScaled = func_002045D0) and the cars on the cards (Cars::Car_DrawCard). The room
scene (`game/story/room_scene`) opens it for the car the message's car pick (end code 4) gives and swaps a
teammate's car onto the turntable (func_0022ABA0 / func_0022A6B8 states 2-7). While the screen is open a stick turns
the turntable (paint.md, "The stick": pad 1's left stick X on the PS2, the right stick on PC). The Wheel card
(category 7) shows WHEEL.BIN's hub of the item alone, face on (camera D_002A8CF0[7]: eye (0, 0, −15 × 256 / size),
yaw 0x4000, focal 3564), in the edited car's wheel colour (Car_DrawWheelCard); modelconv writes CARS/WHEEL.BIN as
models/CARS/WHEEL/<n>.obj (0 the tire, 1 + n the hub of item n). The meter's load (job 0x3A0000 | item) has no PC step: meters.cpp reads each meter's ITEM/METERn.GSL. The shops, My City's Recycle Shop and 2 Player's Change Parts and Trade Items are in (7). Debug:
`--factory --allparts`.

Names used below: `P` = the parts window task (func_0022BB88), `C` = its carousel child (func_00246250),
`S` = C's big work buffer D_01752780 (C's +0x28 points at it), `info` = the second task argument (arg1: the pads
at +0x30/+0x32 pressed, +0x34/+0x36 repeat; +0x0A mode flags; +0x0F loader busy; +0x21/+0x23 town/place).
Sound_PlaySe is func_00258E50.

## 1. The category list window (func_0022BB88, first frame)

D_0029F370 is 16 string pointers (the 16th is "None", never used):

| row | text | category (row+1, row 14 -> 0) |
|---|---|---|
| 0 | Tires | 1 |
| 1 | Engine | 2 |
| 2 | Chassis | 3 |
| 3 | Transmission | 4 |
| 4 | Steering | 5 |
| 5 | Brake | 6 |
| 6 | Wheel | 7 |
| 7 | Lights | 8 |
| 8 | Wing Set | 9 |
| 9 | Special Parts | 10 |
| 10 | Options | 11 |
| 11 | Sticker | 12 |
| 12 | Horn | 13 |
| 13 | Meter | 14 |
| 14 | Body | 0 |

- P's arg (+0x1C) = (side << 4) | car (car 0 the player, 1-2 teammates; side = which save record, 0 in the
  Adventure). The car is `D_0177F760 + side*0x3448 + car*0x1C` (the SaveCar).
- `w = WindowAlloc(); func_00205DA0(w, D_0029F370, car ? 12 : 15, 6); Window_Place(w, 5)` (left, top);
  `w+0x0E = 1` (opening), `w+0x5F = 1` (style 1, cyan with orange edges), `w+0x0C |= side` (the pad).
- func_00205DA0 (scroll-list setup): the first min(count, max) strings are added as items (Window_AddLine flag 0x10);
  for each further string `w+0x11++` (lines hidden below), and the window is widened (about its centre) to fit the
  widest of them (Text_Width + 20; the indent +0x08 kept at most (width - textwidth)/2). Finally `w+0x0C = 4`, so
  **noWrap**: the cursor stops at the first/last row (Menu_MoveCursor's wrap flag = ((+0xC >> 2) ^ 1) & 1).
- Window_UpdateList(w, info, D_0029F370) scrolling (asm 0x00205350): the cursor (+0x0D) runs over all rows
  (+0x0B items + +0x11 hidden). +0x10 = first shown row: when the cursor goes above it, +0x10-- ; at or below
  +0x10 + 6, +0x10++; each time the 6 shown text pointers (+0x14..) are recopied from the list starting at +0x10.
  Bar/cursor y use (+0x0A + cursor - +0x10) * 12. Blinking scroll arrows while active (+0x0E == 0) on frames with
  (+0x12 counter & 0x10): func_00203E70 icon 2 (up) at (x + w/2 - 10, y*2 - 20) if +0x10 != 0, icon 3 (down) at
  (x + w/2 - 10, (y + h)*2) if +0x10 < +0x11. The returned 1/-1 (Cross/Triangle) is **ignored** by P: the window
  never closes on its own here.
- Port: Window::SetList and the scrolling in Window::Update; Icon_Draw for the arrows.

## 2. P each frame (func_0022BB88)

- If `P+0x24` (child reported): r = `P+4`.
  - r < 0 (C quit): P's parent (`P+0x18`) gets +0x24 = 1, +4 = -1, and P deletes itself (func_00204DE8(P, -1)).
    The screen is over.
  - else by category `cat = (P+0x27 == 0xE) ? 0 : P+0x27 + 1` (P+0x27 = the row the child was made for):
    - **0 (body)**: r is a body slot 0-7 (see 4). `bodyPaint[side][car.body] = car.paint` (save +0xD70 + body*4);
      `car.body = D_00335720[r]`; `car.paint = bodyPaint[side][newBody]`; P's parent gets +4 = r + 0x800F,
      +0x24 = 1: the room scene copies model slot 0xF + r, textures included, into slot 0 (func_0020AD18).
    - **13 (horn)**: starts a task func_002083C0 with arg `D_002F00AC[r*0x10]` (the horn table's +0xC, SE
      0x3C + 2r): it plays that SE on its first frame and deletes itself at frame 64 (the horn preview); then
      func_0023C418(car, 13, r).
    - **14 (meter)**: starts func_0020DC50 (a loader request) with arg `r | 0x3A0000` (job 0x3A loads
      ITEM/METERr.GSL; func_0020DC50 sets info+0x0F = 1 while the loader is busy), then as default.
    - **default (1-12, 14)**: func_0023C418(car, cat, r). (Then two debug prints "Acc: %d" / "Mue: %d"; ignore.)
    - In every fitting case `P+2 = 0`, which makes the code below remake the child, so the carousel restarts
      with the new part as fitted (and centred).
  - (+0x24 isn't cleared in this function.)
- `Window_UpdateList(w, info, D_0029F370)`.
- Remake the child when `P+2 == 0 && w+0x0E == 0` (first time, after a fit), or when the window's cursor row
  != `P+0x27`: delete the old child (P+0x20) if any, `P+0x27 = row`, `P+2 = 1`, `P+0x20 = NULL`; then unless
  skipped, `P+0x20 = new task func_00246250(arg)`, `w+0x0E |= 0x80` (window inactive while the child has input).
  - arg = `cat | (car.parts[cat] << 8) | (P+0x1C << 16)`, or for body `(car.body << 8) | (side << 20)`.
  - **Skipped** (no child; nothing below the window): cat 7 (Wheel) when car.partFlags & 0x400 (Big tires),
    and cat 14 (Meter) when info+0x0A & 2 (2 Player: modes 2, 3 and 6). (For a teammate cat 14 / 0 can't be reached anyway:
    12 rows.)
- The child clears `w+0x0E & 0x80` on every frame its frame counter (+8) isn't 0 (see 4), so **the window's
  Up/Down stay live while the carousel runs**; moving the row remakes the child at once.

## 3. Which items go in the carousel (func_0023C150 / func_0023BF28, asm-checked)

Owned bits: `items[k][0..4]` at save +0xC30 + side*0x3448 + k*0x14.

- **Bodies (kind 0, and 15)**: one 160-bit mask (words 0-4, bit b of word b>>5). Entries = set bits in
  ascending body number. No other-car logic.
- **Parts (kinds 1-14)**: for item i = 0..15 (from kind 8 on, item 0, the default Headlights / None / Normal Horn /
  Normal Meter, skips the counting below: all its copies are listed whatever the other cars have; checked in
  PCSX2, where Lights shows the Headlights card on a new save):
  - `used` = how many of the *other* cars have item i fitted: mask bit 1 car 0 (parts at +0x0C), bit 2 car 1
    (+0x28; only if save+0x674 != 0, teammate 1 exists), bit 4 car 2 (+0x44; only if +0x676 != 0). In the factory
    mask = `~(1 << car) & 7` (C sets it from arg byte +0x1E & 3).
  - then for each copy word j = used..4 with bit i set, **one entry** (so two spare copies give two cards).
  - mask bit 8 (shop mode 4 only): only items with a price (func_00243670 != 0).
- func_0023BF28(kind, side, n, mask) = the n-th entry's item (-1 past the end); func_0023C150(kind, item, side,
  mask) = the index of the first entry with that item, or the count when item == -1 (-1 if not listed).
- C's setup (factory, Room_SceneKind 0): `S+0x5400 = cat`, `+0x5404 = fitted`, `+0x540C = side`, `+0x5408 = 0`,
  `+0x5410 = car + side*3`, `+0x5414 = mask`, `+0x541C = index of fitted`, `+0x5418 = count`.
  - **count == 0: C deletes itself and reports -1, which (2) closes the whole parts screen.** (No SE.) Every row
    has its item 0 in a new save, so this needs the other cars to use up a kind's copies (not seen in PCSX2).
  - selection `C+0x27 = index & 0xF`. The car's own fitted copy is never counted against it, so the fitted item is
    listed whenever it's owned (an index of -1 would give 15; the port takes the first card then).
  - count <= 16: entries 0..count-1 in `S+0x5420[]` (s16). count >= 17 (bodies): a 16-slot ring around
    `+0x541C` (8 ahead, 8 behind, slot = index & 15), refilled when the base moves; `C+0x20 = 0xFFFF`.
  - `C+0x20` = card mask ((1 << count) - 1, max 0xFFFF), `C+0x26 = n` = Bits_Count of it (cards), card k shows
    entry k (k-th set bit; with this mask it's k).
- Then func_00243C50(S, (info+0x0A >> 1) & 1) sets up the 16 picture TEX0s (see 5).

## 4. Carousel: input, placement, drawing (func_00246250)

**Input** (only while `C+0x25 == 0`; nothing in this function sets +0x25, so always): pad = side's (info+0x30 +
side*2 pressed, +0x34 + side*2 repeat; mode 4 with info+0x0A & 2 ORs both pads).
- Only if n >= 2 and the scroll `C+2 == 0`: repeat Right (0x2000): `sel = (sel + 1) % n`, scroll = +8, dir +1;
  else repeat Left (0x8000): `sel = sel ? sel - 1 : n - 1`, scroll = -8, dir -1. Either plays **SE 0x24**.
- While scroll != 0: `C+8 = 0` (the frame counter), and scroll steps 1 towards 0 per frame (8 frames a turn).
- count >= 17 and a turn: `+0x541C += dir` (wrapped), ring refilled.
- Body category, idle (scroll 0, no turn this frame): if `D_00335720[sel & 7] != item(sel)` and item != fitted and
  info+0x0F == 0 (loader idle): start func_0020DC50 with `item | (sel << 8) | 0x3B0000` (load that body's car into
  body slot sel & 7). D_00335720 = the 8 slots' loaded body ids (0xFF when the factory starts; new_game.md).
- At the end of the frame, only if `C+8 != 0` (not on the first frame or while turning):
  - `P's window +0x0E &= 0x7F` (window active again).
  - **Cross** (pressed 0x40): if item(sel) != fitted: body: only if `D_00335720[sel & 7] == item(sel)` (loaded):
    P+0x24 = 1, P+4 = sel & 7, **SE 0x1A**; other categories: P+0x24 = 1, P+4 = item(sel), SE 0x1A unless
    cat 13 (the horn preview plays instead). C keeps running; P remakes it next frame. Cross on the fitted part:
    nothing.
  - else **Triangle** (pressed 0x10), only if info+0x0F == 0: **SE 0x1B**, C deletes itself, P+0x24 = 1,
    P+4 = -1 -> P ends the screen (2).
- C doesn't draw Help/OK/BACK plates.

**Placement** (card k = 0..n-1, asm 0x00246BE8-0x002470A8):
- `a = (s16)((((k + n - sel) % n) * 8 + scroll) << 13) / n) * pi / 32768` (cos = func_00274938, sin = func_00274A10).
- `size = 160 + (int)(cos a * (n + 32.0)) - n`, **+1 if a > 0** (256 = full scale).
- `x = 240 + (int)(sin a * 112.0)`.
- `y = 240 + (int)(cos a * (n + 24.0)) - n` (the card's top).
- Front card (a = 0): x 240, y 264, size 192. (Course select differs: 320/192, 128/64, 192/64.)
- Grey: all parts `(int)(func_00274810(a) * 16) + 0x60` (port: cos), 0x80 exactly at the front (PlaceCard and
  func_002446A8 both).
- **Frame** (CourseSelect_PlaceCard(S+0x60, k, x, y, size, a)): rect `x +- 128*size/256, y .. y + 216*size/256`.
  Its texture is **the room picture** (TEX0 0x20045DC669329F2E: TBP 0x1F2E T8, TBW 10, 1024x512, CBP 0x22EE = the
  room image's CLUT), vertex UVs D_002A8A60: (0, 0.4375)-(0.1875, 0.75) = texels **(0,224)-(192,384)** of the
  640x384 room picture: the orange name tab + green panel card art (`data/textures/SHOP/T00.BIN/000.dds` for
  Peach Town's factory; verified by eye). Stretched 192x160 -> 256x216.
- **Name**: `Text_Width(name) * 5/4 * size/256` wide, centred on x; stored at S+0x5380 + k*8: x0 = x - half,
  y0 = y + size*14/256, size, size*5/4. Drawn by func_002045D0(x0, y0, z = size, name, palette, scale = size*5/4):
  scaled font glyphs (each glyph w*scale/256, 24*scale/256 high; space 10*scale/256), **palette 5 (green) if
  item == fitted, else 0 (white)**. Port: Text_PrintScaled.
- **Picture** (func_002446A8(S+0x2B80 + k*0xF0, x, y, size, a)): the square `x +- size/4, y + size/4 .. y +
  size*3/4`, UV 0-1 (D_002A8AE0). (The 6th argument, C+8, isn't used: nothing animates.)
- **Model camera** (3D categories): D_002A8CF0[cat] (0x20 each) copied, eye z scaled `z * 256 / size`:
  all categories eye (0, 1, -60), focal 3564.0, pitch (+0x14) 0x0C00, yaw (+0x16) 0x6280; **Wheel (7)**: eye
  (0, 0, -15), pitch 0, yaw 0x4000. Camera_SetScreenCenter centre = (x, y + size/2) (the D_00333288 != 0 branch,
  x*4/5 + 0x700, is never taken: D_00333288 is never written). Camera_BuildMatrices into S+0x3A80 + k*0x190,
  Light_SetHighlight(+0x180). Lights: Light_Setup(3, 0) once per frame (course 3's light set).
- **Draw order**: placement for all cards first, then GS setup packets (S+0, 6 qwords; D_002A8B20: TEST_2
  0x537FB, ALPHA_2 (Cs-Cd)*As+Cd), then cards back to front: for k = n/2 down to 0: card (n + sel - k) % n, then
  (sel + k) % n if different. Each card (func_00244F60): frame, name, then picture or model.

**Picture or model per category** (func_00244F60's jump table 0x00327560, asm-checked mapping; the model
details are m2c):
- 1-6, 10, 13, 14: the 2D picture (TEX0 slot item, quad of card k).
- 0 Body: if body slot `k & 7` holds this body (D_00335720): car record 15 + (k&7) (D_0177F310 + 0x168/0x170:
  sections 0 and 2) with this body's saved paint (bodyPaint[side][body]); else if it's the fitted body: the
  player's current car (D_0177EA50[S+0x5410], record S+0x5410 sections 0 and 2); else nothing (not loaded yet).
- 7 Wheel: WHEEL.BIN model D_0177AB34[item] (the side view camera above), func_002230C8(car+5) first (both paints
  from the car's wheel colour, save car +5).
- 8 Lights, 11 Options: the current car body (sections 0 and 2, light set D_002A8BB0), then if item != 0 the
  PARTS.BIN model func_00243A00(cat, item) placed with D_002ED720[car.body] (the body's part places), TEX0 from the
  slot. Model numbers: lights table +0xE (Fog/Beam 4), options +0xE (Water Ski 8, Flight Wing 2, Police Light 5,
  Billboards 9, with the sponsor picture TEX0 from func_00223810(options +0x10)).
- 9 Wing Set: body section 0, plus section +8 (item 0) or +0xC (the wing).
- 12 Sticker: body sections 0 and 2, plus section +0x14 if item != 0.

## 5. Tables (dumped from the ELF)

Per category (func_00243468 name, func_00243848 description, func_00243670 price): name at +0, description
pointer at +4, price at +8 (+4 for 7, 12, 13, 14 have no description; their +4 points at "").

**Descriptions are fixed strings in the ELF**, not formatted: a run of 0-terminated lines.
func_0022E960(cat, item) asks func_00243848 for lines 0-3 (NULL for 0, 7, 12, 13, 14 -> no box): frame
Window_DrawCellFrame(0x11, 0xC, 0x1E, 0x11, style 1), line i at Text_Print(0x12, 0xD + i, s, **palette 0xA**).
Always 4 lines (it only stops on NULL), so an empty first line (engines) leaves row 0xD blank, and lines past
the end are the padding's empty strings. "Standard" is simply item 0's first line. Lines start with a space where
shown (" Roads ***").

| cat | table, stride | items |
|---|---|---|
| 1 Tires | 0x2ECA70, 0x1C | 0 Normal 200 {Standard / Roads *** / Off-Road **}; 1 Sports 1000 {All-around / Roads **** / Off-Road ***}; 2 Semi-Racing 2000 {Road racing type / Roads ***** / Off-Road **}; 3 Racing 5000 {Medium Tread / Roads ****** / Off-Road *}; 4 HG Racing 10000 {Great on raceway / Roads ****** / Off-Road **}; 5 Wet 2000 {Water Durable / Roads **** / Off-Road *** / Wet Roads ***}; 6 HG Wet 3000 {Super Water Durable / Roads **** / Off-Road *** / Wet Roads ****}; 7 Off-Road 500 {Off-road type / Roads *** / Off-Road *** / Wet Roads **}; 8 HG Off-Road 3000 {Mountain ride / Roads **** / Off-Road **** / Wet Roads **}; 9 Studless 1000 {Ice Durable / Roads *** / Off-Road *** / Wet Roads **}; 10 HG Studless 3000 {Ice and Land Durable / Roads **** / Off-Road *** / Wet Roads ***}; 11 Big 5000 {Drive everywhere / Roads **** / Off-Road **** / Wet Roads ****} (+0x1A = 1: Big tires); 12 Devil 200000 {Unimaginable!} |
| 2 Engine | 0x2ECED0, 0x18 | first line empty except item 0: 0 Normal 200 {Standard / Power 150 / Energy Use 3}; 1 Panther 500 {/ Power 180 / Energy Use 4}; 2 Blue MAX 1000 {220, 5}; 3 Blue MAX V2 1500 {260, 6}; 4 MAD 2000 {290, 7}; 5 MAD V2 4000 {330, 8}; 6 Long MAD 8000 {360, 6}; 7 Black MAX 12000 {390, 8}; 8 RS Magnum 16000 {420, 12}; 9 Speed MAX 20000 {450, 16}; 10 Hyper MAX 80000 {600, 20} (type 1); 11 Devil Engine 160000 {3000, 0} (type 2). (+0xC power x10, +0x10 energy use, +0x14 type.) |
| 3 Chassis | 0x2ED1F8, 0x10 | 0 Normal 200 {Standard}; 1 Light 500 {Good top-speed / and acceleration / Weight ****}; 2 Feather 1000 {Better ... / Weight ***}; 3 Phantom 2000 {Best ... / Weight **}; 4 Hyper 4000 {Super ... / Weight *}. (+0xC weight 25/22/20/18/15.) |
| 4 Transmission | 0x2ED328, 0x1C | 0 Normal 200 {Standard}; 1 Sports 1000 {Good engine, / better top-speed / Gear Ratio**}; 2 Power 2000 {... Gear Ratio***}; 3 Speed 4000 {****}; 4 Wide 7000 {*****}; 5 Hyper 10000 {******} |
| 5 Steering | 0x2ED4E8, 0x10 | 0 Normal 200 {Standard}; 1 Quick 500 {Steer quickly / 1.5 times normal}; 2 X2 Quick 1000 {Steer quickly / 2 times normal}; 3 X3 Quick 2000 {Steer quickly / 3 times normal / Might spin!} |
| 6 Brake | 0x2ED5D0, 0x2C | 0 Normal Pad 500 {Standard}; 1 Soft Pad 1000 {Good for / quick braking}; 2 Hard Pad 1500 {Helps for all around / cornering}; 3 Metal Pad 2000 {Stops on a dime} |
| 7 Wheel | 0x2EFFD8, 0xC | 0 Normal, 1 Mesh, 2 Spoke 1, 3 Spoke 2, 4 Flush 1, 5 Spoke 3, 6 Flush 2, 7 Spoke 4, 8 Spoke 5, 9 Spoke 6, 10 Flush 3, 11 Flush 4, 12 Flush 5, 13 Spoke 7 (500 each), 14 Spoke 666 (5000); no description |
| 8 Lights | 0x2EFF40, 0x10 | 0 Headlights 0 {Standard lights}; 1 Fog Lights 500 {Wide beam}; 2 Beam Lights 500 {Hi-Beam} |
| 9 Wing Set | 0x2F0280, 0xC | 0 None {Wing Set}; 1 Wing Set 3000 {Turn while / gliding / Longer duration / while in the air.} |
| 10 Special | 0x2F02F8, 0x10 | 0 None {Propulsion device}; 1 Propeller 3000 {Works with engine / Drive underwater / Reverses also!}; 2 Jet Turbine 10000 {Speeds up even / while in the air. / Uses energy fast / so beware} |
| 11 Options | 0x2F03E0, 0x14 | 0 None {Optional parts}; 1 Water Ski 3000 {Drive on water. / Sinks at a dead stop. / Need a propeller / or jet turbine.}; 2 Flight Wing 50000 {Fly at 190mph. / Use with / jet turbine.}; 3 Police Light 1000 {Has no effect. / Improves the car's / look}; 4-8 Billboard 0 (sponsor 0-4: cafe in Peach Town, noodle cafe in Fuji City, bakery in Sandpolis, wool shop in White Mountain, coconut shop in Papaya Island; "Post it up to / receive money at / ...") |
| 12 Sticker | 0x2F07F8, 0xC | 0 None, 1 Sticker 10000; no description |
| 13 Horn | 0x2F00A0, 0x10 | 0 Normal Horn 0, then 1000 each: Air Horn, Echo Air Horn, Bus Horn, Bicycle Bell, Venus Horn, Chicken Horn, Fantasy Horn, Trumpet Horn, Christmas Horn, Duck Horn, Space Horn, Horse Horn, Baby Horn, Train Horn (14); +0xC = SE 0x3C + 2i |
| 14 Meter | 0x2F06C0, 0xC | 0 Normal Meter 0, then 100 each: Chronometer, Rainbow, Space, Triangle, Love Sick, Life, Cherry, Duck, Devil, Digital Meter (10) |
| 0 Body | 0x2ED720, 0x44 | name at +0; price always 500; no description |

Item counts: 13, 12, 5, 6, 4, 4, 15, 3, 2, 3, 9, 2, 15, 11 for categories 1-14 (from where each table's names end).

**Pictures** (func_00243C50; table D_002A8C00 by category: s16 TBP, CBP offset, stride, PSM; D_002A8C78 for
shops when info+0x0A & 2). ITEM/PARTS.GSL is loaded at **TBP 0x2C7E** (loader job 0x80000, file entry
D_0029B150, 135 sectors = PARTS.GSL's size, relocated +0x2C7E in func_0020D120). So, in PARTS.GSL's manifest
(`data/textures/ITEM/PARTS.GSL.txt`):

| cat | PARTS.GSL entries |
|---|---|
| 1 Tires | 0-12 (T4 64x64) |
| 2 Engine | 13-24 (T8 128x64) |
| 3 Chassis | 25, palette = item (CT16, CSA) |
| 4 Transmission | 30, palette = item |
| 5 Steering | 26-29 (T8 64x64) |
| 6 Brake | 31, palette = item |
| 10 Special | 47-49 |
| 13 Horn | 32-46 |
| 14 Meter | 50-60 (T8 64x64) |

Picture = base + item*stride, so for 10, 13, 14 entry "first" is item 0 (None / Normal Horn / Normal Meter):
Propeller 48, Jet Turbine 49; Air Horn 33 ...; Chronometer 51 ... (Special Parts' None, 47, shows an empty panel
in PCSX2.)
The engine pictures are 128x64 drawn into the square quad (UV 0-1): verify the aspect in PCSX2.
Body slots: 16 TEX0s at 0x283A + slot*0x44 (T8 128x128, CLUT +0x40), slot 8 = the current car's texture
(0x243E + 0x44*record, Loader_LoadCar's). 7/8/9/11/12 use PARTS.BIN's texture (0x1BC2) or the car's.

## 6. func_0023C418(car, cat, item): fitting (asm-checked, jump table 0x002EC850)

Always `car.parts[cat] = item` (+0x0C + cat), then:

| cat | also |
|---|---|
| 1 Tires | partFlags 0x400 set if tire +0x1A (Big = 1) else cleared |
| 2 Engine | partFlags &= ~3, then \|= 1 if engine +0x14 == 1 (Hyper MAX), \|= 2 if == 2 (Devil) |
| 3-6 | nothing |
| 7 Wheel | car.wheel (+6) = item |
| 8 Lights | 0x10 set if item != 0 else cleared |
| 9 Wing Set | 0x1000 set if item != 0 else cleared |
| 10 Special | partFlags = (flags & 0xDFBF) \| D_002F0304[item] (0, 0x40 propeller, 0x2000 jet) |
| 11 Options | car.billboard (+7) = table +0x10 (sponsor); flags = (flags & 0xFCDB) \| table +0xC (0, 0x100 water ski, 0x4 flight wing, 0x20 police light, 0x200 billboard) |
| 12 Sticker | 0x4000 set if item != 0 else cleared |
| 13, 14, 0 | nothing else (body: P does the swap, see 2) |

## 7. The shops (end code 0x13)

Sources: asm/m2c of Shop_ScreenTask (func_00269E88), PartsShop_Task (func_00269568), BodyShop_Task (func_00269900),
Shop_NoticeTask (func_00268428), Shop_BuyTask (func_00268890), BodyShop_SoldOutTask (func_00269D50), Shop_StockNth /
Shop_StockIndex (func_00267F48 / func_002681A8), Part_Price (func_00243670), Save_ItemFull (func_0023D548),
Money_DrawAfter (func_0022E8C0) and ItemCarousel_Task's shop branches; the tables dumped from the ELF.

- **Shop_ScreenTask** picks the screen by Room_SceneKind: 1 PartsShop_Task, 2 BodyShop_Task, 4 RecycleShop_Task
  (func_00268E40, My City's (9, 5)). The message waits in state 6 with its box hidden, then goes to n.
- **Stock** (by town, info+0x23): parts D_002DC528[town x 16 + kind] (one word, items 0-31); bodies the 160-bit
  mask D_002DC460 + 0x14 x town less the bodies owned (Part_NthOwned's list, after My City's additions). Town 9 adds
  MyCity_ShopStockAdd (func_00267C30): the stock of each town 0-8 whose Parts (Body) Shop place (D_002DC7A8: 1 and
  2, none in towns 0 and 4, no Body Shop in 5) is found (save +0xBD8), then by kind: Big tires (flag 0x60), Hyper MAX
  engine (stamp 46), Hyper chassis (0x63) and transmission (0x62), X3 Quick steering (0x61), Metal Pad brakes (0x64),
  the Wing Set and Propeller (the sea temple, area 18 place 0, found), the Jet Turbine (stamp 81), the Police Light
  (My City place 4 found), 13 bodies (func_00267A80), 7 horns (func_002678E0) and 5 meters (func_002679C8, which
  asks for stamp 47 and drops the answer: the Life Meter is never stocked). Everything is ORed into one mask: an item
  shows once. The port has the tables in `parts_tables.inc` (`kShopParts`, `kShopBodies`; `gen_parts_tables.py`)
  and the unlocks as `kMyCityUnlocks` (parts_screen.cpp).
- **Prices:** Part_Price = the item's table entry +8; every body is 500.
- **PartsShop_Task:** the category list D_002DC8E0 (Tires ... Meter, 14 rows, 6 shown; style 1, top left, opening).
  A new row (and every return to state 0) makes ItemCarousel_Task of that row's stock, starting at entry +0x27 (0 on
  a row change). A category with nothing in stock has no carousel (its -1 is ignored). Triangle on the list leaves.
- **BodyShop_Task:** no list; the carousel of the bodies, or BodyShop_SoldOutTask ("We don't have any more" over
  "   bodies to sell.", Yes; SE 0x1A) when there are none, which leaves. Triangle on the carousel leaves (SE 0x1B).
- **The carousel in a shop:** fitted = 0xFF, so every name is white and Cross works on any card (SE 0x1A, none for
  a horn; the horn isn't played). Under the cards, cells (0x11, 9)-(0x1E, 0xC) style 2: "You have it." (Body Shop) /
  "You have N" (copies, at 0x186, 0x7D) / "You don't have it."; "Price" at (0x13, 0xB), the price right-aligned at
  0x226 (palette 6 if the money is short) and the currency at (0x1B, 0xB); Money_DrawAfter(price), the money after it
  below the scene's money box with a bobbing arrow. The description as in the factory.
- **Cross** (state 1): the carousel is deleted and the list made inactive; +0x27 = the item's stock index. Money
  short: Shop_NoticeTask mode 2 "Need more money!". Else Save_ItemFull (a body owned, a part's fifth copy word):
  mode 3 "Cannot carry anymore!". Else Shop_BuyTask mode 1. A notice's answer (any, SE 0x1A) goes back to state 0.
- **Shop_BuyTask:** lines name (Part_Name), "    Buy ?     ", two empty, then Yes / No (from No), each line centred
  with spaces on the widest; centred on (320, 112). From its second frame the price (digits right-aligned at x +
  (w - (16 digits + currency width + 2)) / 2 + 16 digits, pixel y 2y + 0x30) and the currency 2 pixels on, on the third
  line. PartsShop/BodyShop draw Wallet_Draw(price) meanwhile (the money now and after at the bottom right). Yes:
  SE 0x1A, Item_Give, Money_Pay (SE 0x305); a body: +0x27 one back if it was the last entry. No or Triangle: SE 0x1B.
  Then state 0 (the carousel again on +0x27). The windows go at once (Task_Delete), no fade.
- **RecycleShop_Task** (kind 4): the list D_002DC870 (the same 14 categories) and the carousel of the player's own
  parts with a price, a copy each, those on the team's three cars not counted (Part_NthOwned's mask 0xF). Its box
  (cells 0x11-0x1E, rows 0xA-0xC): "Selling" with half the price and the currency; Money_DrawAfter(-price / 2).
  Cross: +0x27 = Part_OwnedIndex of the item; no price: notice mode 0 "Cannot buy this!"; not listed: mode 1
  "Installing!" (neither can happen from the list); else Shop_BuyTask mode 0 ("    Sell ?    ", half the price).
  Yes: Save_TakeItem, Money_Pay(-price / 2); +0x27 one back if it was the last entry.
- **2 Player's trading** (TradeShop_Start, func_00269E50: RecycleShop_Task with +0x1C bit 1; bit 0 the seller). The
  list on either pad and 0x18 half-lines lower; the carousel on both pads. Prices: Trade_Price (func_00243A78),
  d = Save_Score (func_0023D058) of the player less the other's; selling Part_Price x (3000 + d) / 4000, buying
  x (2000 - d) / 2000. The box: the buyer's name, "You have N" / "You don't have it." of theirs, "Buying" with their
  price (palette 6 if they're short) in rows 8-0xC, and above it (rows 5-8) the seller's name and "Selling" with
  theirs. Cross: no price mode 4 "Cannot sell this!", not listed mode 5 "Installing!" (both with the seller's name);
  the buyer short mode 6 "Need more money!" (their name, the buying price below); their fifth copy mode 7 "Cannot
  carry anymore!" (their name, the part); else Shop_BuyTask modes 2 (the seller: name, part, "Sell ?", the selling
  price on the fourth line) and 3 (the buyer's), each on its player's pad, 150 pixels to that player's side; +0x20 = 2,
  each Yes takes one off, a No makes it -1. Meanwhile both wallets: player 1's at x 0x28, player 2's at 0x1B8 (money
  at 0x152, after at 0x182, the arrow at x + 0x4C). Both Yes: the seller loses the copy and gets the selling price,
  the buyer gets it (Item_Give) and pays the buying price. The notices' and windows' answers (any) need their
  owner's pad.
- Not in the port: info+0x0F (the loader busy) holding Triangle, and the 2 Player pictures' TEX0 table D_002A8C78 (the
  same pictures on screen).
- **Checked in the port** (Peach Town, a new save, 1000 gold): the Parts Shop's Tires (Sports 1000, Off-Road), Yes
  buys them ("You have 1", 0 gold), the next try says "Need more money!", Triangle leaves to "Come again!". The Body
  Shop's 20 bodies (Q001 first, 500 each): Q001 bought, the ring then starts at Q002. 213 draw calls with the full
  ring of cars. Not compared with PCSX2 yet.
- **Checked in PCSX2** (Race Right-Away with the mode cheat set to 6, PINE giving parts and money; captures/versus
  ps2_parts_list/_window.png, ps2_trade_*.png): Change Parts' list and car, the parts screen over the card, Trade
  Items' menu, the trade's box (PLAYER 1 Selling 150 / PLAYER 2 You have 5, Buying 200 in red), mode 7's notice,
  the two windows and wallets, and the trade's result (1150 and 4800). The port shows the same (its test saves:
  201, from Player 1's teammate). The Recycle Shop itself was checked in the port only (Peach Town's Parts Shop
  opened as kind 4 for the test).

## 8. Seen in PCSX2 (factory_menu.p2s)

- The cards of Lights, Wing Set, Options and Sticker show the car as a black silhouette (the light set
  D_002A8BB0); the port draws them so (Car_DrawCard's silhouette) with the parts in colour. The body card shows
  the body in its own paint, smaller in the panel; its name is the Q number (Q141 for body 140).
- Horn and Meter have pictures and no description; Special Parts' None has an empty panel.
- Not seen there: a save with teammates (the car pick and the swap follow the decomp; checked in the port only).
