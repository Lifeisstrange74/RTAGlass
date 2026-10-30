# The message script system (building scenes, the opening)

How the Adventure's building/cutscene dialogue works, as read from the decomp drafts
the asm in
`reference/asm/nonmatchings/cod`, and the data in `game/SLUS_203.98`. Anything inferred instead of
read is marked **(inferred)**.

The decoder and dumps used for this research were throwaway scripts and aren't kept; the script format
below is enough to write one again.

---

## 1. Where the data is

### ELF layout

Program headers of SLUS_203.98:

| Segment | File offset | vaddr | filesz | memsz |
|---|---|---|---|---|
| text (R X) | 0x1000 | 0x200000 | 0x9A848 | 0x9A848 |
| data (R W) | 0x9B880 | 0x29A880 | 0x9ADFB | 0x14ECC60 (bss to 0x17874E0) |

Both segments are at **file offset = vaddr − 0x1FF000**. So `D_002A7ED0` is at file offset **0xA8ED0**.

### The tables

`D_002A7ED0` is `MsgEntry **D_002A7ED0[group]`. The scene runner indexes it with `info+0x23` (the **town**, s8) and
`info+0x21` (the **place in the town**, u8): `D_002A7ED0[town][place]`. (`info` = the scene's arg1, the game state
struct; town 8 is Cloud Hill: Q's script #59 says "the president's mansion is in Cloudhill" and sends you to town 8.)

Group pointers (`D_002A7ED0[g]`): 0 → NULL, 1 → 0x2A5740, 2 → 0x2A5CC8, 3 → 0x2A62B0, 4 → 0x2A66F0, 5 → 0x2A6808,
6 → 0x2A6D50, 7 → 0x2A72C0, 8 → 0x2A7768, 9 → 0x2A7C50, 10 → 0x2A7D00 ... 19 → 0x2A7EA8.

Each `D_002A7ED0[town][place]` points at a **message table**: an array of `char *`:
- **[0] = the speaker's name** (drawn in the name tab), e.g. "Forest", "Q's Factory", "George".
- **[1] = the first message**; **[n]** = message n. Every "go to message n" in the script is an index into this
  array. There is no count; the scripts only use indexes that exist.

Examples:
- `D_002A7ED0[8][9]` = **0x2A7460** (president's office): 16 pointers, name "Forest" at 0x3342E0, messages at
  0x31C7C0, 0x31C6B8 ... 0x31C1B0 (the compiler laid the strings out in reverse).
- `D_002A7ED0[1..9][0]` = **0x2A4450**: the shared **Q's Factory** table (74 pointers, name "Q's Factory" at
  0x2F2C38). Every town's place 0 is Q's Factory.
- The scene runner's other use: `D_002A7EF4` = `&D_002A7ED0[9]`, `+0x20` → `D_002A7ED0[9][8]` = My City Cinemas
  ("George").

### Message byte code

A message is a byte string:

```
[pre-control codes...] text [0x0A text ...] END-CODE [args...]
```

- **Bytes ≥ 0x20** are text. Bytes ≥ 0x80 start a two-byte (Shift-JIS) character (button glyphs etc.); the rest is
  the FONTE font (title.md, `Text_DrawGlyph`), with its accent escapes.
- **0x0A** is a new line.
- **'$' + letter** is an expansion (jump table `jtbl_002EC6C0`, letters A-U; any other letter draws nothing):

  | Escape | Draws |
  |---|---|
  | `$N` | the player's name, D_0177FD8C (save +0x62C) |
  | `$U` | the currency unit, D_0177FD9F (save +0x63F, "gold") |
  | `$G` | the greeting, D_0177FDA6 (save +0x646) |
  | `$R` | the race name `D_002BE398[*(u8 *)info+0x10]` (info+0x10 points at the chosen race's D_002BDDD0 row) |
  | `$S` | the number D_01782A6C (func_00204460, palette 10) (what it holds: unknown) |
  | `$A` c | `D_0177FCFC[c] / 1000` with "%d" (c is the next raw byte; D_0177FCFC = save +0x59C) |
  | `$T` c | a time "%d:%02d" = `v / 9000`, `(v % 9000) / 150`, v = `D_01787008[c]` (s32); `$T7` uses D_01780A10. The Q's Rally text uses `$T0`..`$T6`, so c is the raw byte '0' (0x30) etc., which indexes `D_01787008[0x30..]` |

- **Other bytes < 0x20** are control codes: the **pre-control codes** at the very start of a message, run by
  `func_0023AD00`; and one **end code** after the text, run by `func_0023B168`.

#### Pre-control codes (func_0023AD00, jump table jtbl_002EC750, code − 1, 0x1D entries)

Run in a loop while the byte is < 0x20, before any text is drawn. "→ n" means *jump*: the current message becomes
message n of the table, and its own pre-codes run straight away (same loop). If the test fails, the codes' bytes
are skipped and the next code runs. Unknown codes (0, 9, 10, 12, ≥ 30) print "Unknown pre-control code %d in town
T%02d shop %d" and skip the following run of bytes < 0x20.

| Code | Args | Meaning |
|---|---|---|
| 0x01 | f, n | if event flag f (Save_EventFlag, save +0x538 bit f) → n |
| 0x02 | f, n | if not event flag f → n |
| 0x03 | k, i, n | if the player has item (kind k, number i) (func_0023D488, items at save +0xC30) → n |
| 0x04 | n | if this place (town, place) is not found yet (func_0023D5F8, save +0xBD8 areas) → n |
| 0x05 | p, n | if photo p is taken (func_0023D8E8, save +0x508; p = 0: all) → n |
| 0x06 | s, n | if stamp s is earned (func_0023D628, save +0x518, bit s−1; s = 0: all) → n. Stamp 100 = you are president (inferred from the texts) |
| 0x07 | n | if the team isn't full (func_0023DBC8: 1 no teammates, 2 one, 0 full) → n |
| 0x08 | a, b | if the person here (town, place) is a teammate (func_0023DB80) → a; else if not beaten (func_0023DCC0, save +0x558) → b; else go on |
| 0x0B | v, n | if Race_Outcome() (D_00335704) == v → n (after a race: 0 retired, else what Results_Task told: 1 below 6th, 2 below 6th with a teammate's prize, 3 2nd-6th, 4 won, 5 a license; 6/7 the Grand Prix lost/won; the president's race 2 won, 1 lost; finish.md) |
| 0x0D | i, n | if field pickup i is taken (func_0023DBF0, save +0x548) → n |
| 0x0E | 6 indexes | if event flag 80 (the Q's Rally is on): → the index chosen by the next rally checkpoint (func_0023DDB8, 0-5); else skip 6 |
| 0x0F | f | set event flag f (func_0023D7A8) |
| 0x10 | f | clear event flag f (func_0023D7E0) |
| 0x11 | k, i | take item (k, i) away (func_0023D390) |
| 0x12 | — | tell the scene result **10** now (parent +4 = 10, +0x24 = 1); the message goes on |
| 0x13 | c | Q's Rally checkpoint c: store the split time (func_0023DD30) |
| 0x14 | t, p | tell the scene result **12** now; mark this place found (func_0023C7B0); then info town = t, place = p |
| 0x15 | s | mark stamp s's text as shown (func_0023D730, save +0x528) |
| 0x16 | n | if this place's My City resident has moved in (func_0023DE50) → n |
| 0x17 | r | My City resident r moves in (func_0023CB68: mail "Now Open!", stamp 0x33 when complete) |
| 0x18 | n | if the clock info+0x14 is **outside** 54000..198000 → n. (info+0x14 is the time of day; 9000 a unit, so 6:00-22:00 — inferred) |
| 0x19 | r, n | if My City resident r has moved in (func_0023DE30, save +0x670) → n |
| 0x1A | c, n | if pickups taken (100 − func_0023D968(0)) ≥ c → n |
| 0x1B | n | if the license (save +0x651) is Super A and (stamp 100, or not event flag 87) → n |
| 0x1C | t, n | if the current town == t → n |
| 0x1D | s, p, n | if the player's car has part p in slot s (func_0023D5E0: record +0xC+s) → n |

#### End codes (func_0023B168's switch on the byte after the text)

"Waits" = waits for Cross or Triangle (pad `info+0x30 & 0x50`: 0x40 Cross, 0x10 Triangle). "Arrow" = the blinking
arrow is drawn while waiting and the press plays SE 0x1A. "Closes" = SE 0x26, the box fades out over 16 frames, then
the result goes to the scene. Children are tasks created with Task_Create(message task, fn, args), arg = pointer to
the bytes after the code.

| Code | Args | Meaning |
|---|---|---|
| 0x00 | — | waits (no arrow), closes; **result 5** |
| 0x01 | yes, no | Yes/No window (func_00239260, cursor on Yes); → yes/no message. Triangle = No |
| 0x02 | yes, no | the same with the cursor on No (func_0023BC90) |
| 0x03 | r | waits, closes; **result 4** (start a race). If r ≠ 0, info+0x20 = r first (the race number: the president's is 26 = 0x1A; the Grand Prix stages 28-34) |
| 0x04 | n | pick a car of the team (func_00239900: you, teammates, "Quit"); a car → **result 9 \| (car << 8)** (change parts); Quit/Triangle → message n |
| 0x05 | a, y, x | number 0-99 with up/down (func_0023A320); Cross: equals a → y, else → x |
| 0x06 | n | the same car pick → **result 8 \| (car << 8)** (paint) |
| 0x07 | k, i, n | arrow, wait; then "Get Body "name"" window (func_00239F68): gives item (k, i) (func_0023D2C0), mutes the song, SE 0x302; Cross → n |
| 0x08 | a, q | the town's race list (func_00239A80); a race → sets info+0x20 / +0x10 → message a; Quit → q. Shows "B/A/Super A license is required." (func_0023BB00) when locked |
| 0x09 | strings, 0x09, targets | **choice window** (func_0023BCD0): one item per 0-terminated string until a 0x09 byte, then one target byte per item. Triangle picks the last item |
| 0x0B | i, n | func_0023BE88: runs func_002083C0 with D_002F00AC[i] (i = 0x10: save +0x779) for 64 frames, then → n (what it shows: not checked) |
| 0x0C | n | arrow, wait; → message n in the same box (the box stays, the text restarts) |
| 0x0D | ids..., 0 | waits, closes (result 5); on the press, queues each stamp id (func_0023C8A8) |
| 0x0E | n | no wait: saves (Save_AdventureSaveTask, D_01780A3C = town); then → n |
| 0x0F | — | waits, closes; **result 6** (quit the game) |
| 0x10 | a, q | teammate menu (func_00239748: teammates 1, 2, "Quit"; func_00239378/func_00239508) → a / Quit q |
| 0x11 | p, n | arrow, wait; takes photo p (func_0023C5C0) and runs the photo view func_0022BFD0; SE 0x306; → n after the photo |
| 0x12 | n | name entry mode 2 (the greeting); → n |
| 0x13 | n | arrow, wait; runs Shop_ScreenTask (func_00269E88: the shop by Room_SceneKind, func_0022AD70); → n after it |
| 0x14 | t, p | waits, closes; marks this place found; info town = t, place = p; **result 11** (go there) |
| 0x15 | s, p, n | arrow, wait; fits part p in slot s (func_0023C418), SE 0x303; → n |
| 0x16 | k, n | arrow, wait; "Earned N gold by driving M miles." (func_0023A140: odometer / 1609 miles, paid (k*10+10) a mile); → n |
| 0x17 | w, z, ok, short, x | bank amount entry (func_0023A460, savings at save +0x330C); w ≠ 0 withdraws |
| 0x18 | — | waits, closes; **result 15** (the ending) |

### The opening: group 8, place 9 (table 0x2A7460, "Forest")

The opening is the president's office before you ever met Q (event flag 1 is set by Q's Factory's first message), so
#1 jumps to #10.

```
#0 name "Forest"
#1 (0x31C7C0)   pre: IF_NOT_FLAG 1 -> #10; IF_FLAG 3 -> #6; STAMP_TEXT_SHOWN 100; IF_NOT_FLAG 88 -> #2
   <5 lines>
   [0C] NEXT -> #3
#2 (0x31C6B8)   pre: SET_FLAG 88
   <11 lines>
   [0C] NEXT -> #3
#3 (0x31C668)   <1 line>
   [09] CHOICE item 1 -> #4; item 2 -> #5
#4 (0x31C658)   pre: SET_FLAG 3
   <1 line>   [03] race 26, result 4
#5 (0x31C610)   <3 lines>   [00] result 5
#6 (0x31C5B8)   pre: CLEAR_FLAG 3; IF_RACE_OUTCOME==0 -> #7; ==1 -> #8; ==2 -> #9
   <3 lines>   [0C] NEXT -> #3
#7 (0x31C570)   <3 lines>   [0C] NEXT -> #3
#8 (0x31C4B0)   <9 lines>   [00] result 5
#9 (0x31C3A0)   <11 lines>
   [07] GET_ITEM kind 0 (body) id 148 -> #11
#10 (0x31C360)  <3 lines>
   [09] CHOICE item 1 -> #12
#11 (0x31C330)  <2 lines>   [18] result 15 (the ending)
#12 (0x31C2C0)  <3 lines>
   [09] CHOICE item 1 -> #13
#13 (0x31C228)  <7 lines>
   [09] CHOICE item 1 -> #14
#14 (0x31C1C8)  <3 lines>
   [09] CHOICE item 1 -> #15
#15 (0x31C1B0)  <1 line>   [00] result 5
```

So the opening plays #10 → #12 → #13 → #14 → #15 and ends with result 5. The "top box with the trumpet" is the
**code 0x09 choice window with a single item**; the trumpet is the window cursor (the horn, `Window_DrawCursor`).
A trailing 0x0A before the code (#10, #13) gives an empty last line, which matters for the scrolling (below): in
the screenshots #13's choice opens after one more scroll, with its two lines before the empty one left in the box.
The raw bytes of #10: its text lines, each ending in 0x0A, then `09`, the one item's text in quotes, `00 09 0C`.

### Q's Factory (table 0x2A4450, place 0 of towns 1-9)

The first visit is #1 (event flag 1 not set). Full dump in `g8_0.txt`; the start:

```
#0 name "Q's Factory"
#1 (0x2F2AC0)  pre: IF_FLAG 80 (rally on) -> #[29,30,31,32,33,34][next checkpoint]; IF_FLAG 0 -> #5;
               IF_FLAG 3 -> #16; IF_FLAG 1 -> #2; SET_FLAG 1
   <11 lines>
   [02] YES/NO (cursor on No) yes -> #13, no -> #14
#2  pre: IF_FLAG 2 -> #3; IF_STAMP 100 -> #12      <1 line>   NEXT -> #4
#3  <1 line>   NEXT -> #4
#4  pre: CLEAR_FLAG 3; CLEAR_FLAG 0; IF_SUPER_A_AND(...) -> #45
    <1 line>
    [09] CHOICE item 1 -> #5 (parts); 2 -> #7 (race); 3 -> #6 (save); 4 -> #8 (quit); 5 -> #11 (drive)
#5  pre: CLEAR_FLAG 2; SET_FLAG 0     <2 lines>   [04] PICK_CAR cancel -> #4
#6  pre: SET_FLAG 2   <1 line>   [0E] SAVE -> #4
#7  <1 line>   [08] RACE_LIST pick -> #15, quit -> #4
#8  pre: IF_NOT_FLAG 2 -> #9   <1 line>   [02] yes -> #10, no -> #4
#9  <2 lines>   [02] yes -> #10, no -> #4
#10 <1 line>   [0F] result 6
#11 pre: PARENT_RESULT 10; CLEAR_FLAG 2   <1 line>   [00] result 5
#13 <3 lines>   NEXT -> #4
#14 (the license explanation, 13 lines) ... <1 line>   [01] YES/NO yes -> #13, no -> #14
#15 pre: CLEAR_FLAG 2; SET_FLAG 3   <2 lines>   [03] result 4 (race already chosen)
#16 after a race: Race_Outcome 6/7/0/1/2/3/4 -> #58/#59/#17/#18/#19/#20/#21; else (5) <3 lines>   NEXT -> #4
#17 <2 lines>   NEXT -> #4
#18-#21 <1 line> [09] CHOICE (#18 without the first answer): each answer -> one of #22-#28 (Q's reply), then #4
```

Event flags as these two scripts use them (inferred from the texts): 0 = in the parts screen (so the script comes
back to #5 after one), 1 = met Q, 2 = saved since the last change, 3 = a race was started from here, 80 =
Q's Rally on, 87 = Grand Prix won, 88 = met the president, 101-106 = Grand Prix stages done.

The 5-item menu is #4's code 0x09 choice; #45 adds "World Grand Prix" once you have the Super A license. #35's first
item is "\tChange parts": the string starts with a 0x09 byte, a data quirk (it goes to Window_AddLine as it is).

---

## 2. How it runs

### The scene: func_0022AEB0 (proposed Shop_SceneTask)

- The song: (town, place) = (8, 9) loads 0x0A0B0104; others 0x0A0B0101/0102/0103.
- At scene age 0x41 (65 frames), unless scene +0x1C & 2, it starts `Task_Create(scene, func_0023B168,
  D_002A7ED0[info+0x23][info+0x21])` and goes to state 1.
- It draws the room and the cars itself. The car movement in the cutscene is the scene's own state machine
  (func_0022A6B8 / func_0022ABA0, on scene +0x27), **not** script commands. The scripts have no movement or animation
  codes. The only link is **info+0x18**: the message task sets it to 2 on every typing blip, and the scene draws car
  slot 3 (D_017524B0+0x90) with `func_00222360(car, 3, info+0x18 != 0, 0)` and counts it down. Bit 0 of that argument
  draws the car's headlights (chain A, cars.md), so the speaker's lights flash while it talks: 2 frames on from each
  blip, which comes every 4 frames. The port draws them (Car_DrawLights).

### The message task: func_0023B168 (Message_Task)

Task fields (task struct per title.md): +0x02 s16 **chars shown**; +0x08 age; +0x1C arg = the table; +0x20 the table
(`char **`); +0x25 state; +0x26 u8 **scroll** (12 units a line); +0x27 the **result code**; +0x28 the **current
message pointer**.

On creation: +0x20 = table, +0x28 = table[1], **SE 0x25**.

| State | Does |
|---|---|
| 0 open | 16 frames: fade alpha = age × 8 (TEXA, func_00203990); the box; the name tab grows to its width × age / 16. No text. At age 16 → 1 |
| 1 run | pre-codes; box; text; name tab and name; then the end code / typing / page handling below |
| 2 scroll | +0x26 += 2 a frame (box and text drawn); every 12 → back to 1. A line takes 6 frames |
| 3 close | 16 frames (age reset to 0): alpha 0x80 − age × 8, box and full tab, no text. At 16: info+0x28 \|= 0x400 (cleared instead for result 4); if +0x27 ≠ 0xFF, tells the scene **+0x27, or 5 if it's 0**; deletes itself |
| 4 child | box and text drawn under the child window; when the child ends (+0x24), message = table[child result], +0x26 = +0x02 = 0, → 1 |
| 5 car pick | as 4, but a child result ≠ −1 goes straight to the scene (result 9/8 \| car << 8), +0x27 = 0xFF, SE 0x26, → 3; −1 → message +0x27 |
| 6 wait | waits for the child (photo, save, dialog), then → 1 |

Every frame of states 1, 2, 4, 5 draws, in this order: `func_0023AD00` (pre-codes, advances +0x28 to the text) →
`func_002028B0(0x158)` (the box) → `func_0023A8E8` (the text) → `func_00202B70(0x158, Text_Width(name))` (the tab) →
`Text_PrintAt(0x23E − width, 0xA0, name, 0)`.

### Text, typing and scrolling: func_0023A8E8 (Message_DrawText)

- It skips `+0x26 / 12` lines from the message start, counting their characters (a two-byte character counts 1).
- Text palette **10** (func_002039E8(0xA)). Each line starts at x **0x46 (70)**. Line 1 is at y **0xAD** half-lines
  (346 px in the 448-line frame), then +12 half-lines (24 px): 346, 370, 394.
- **Scroll offset:** f = `+0x26 % 12`; the lines move up f half-lines (2f px). The first line is clipped at the top
  once f > 4: it's drawn at `y = 0xA9` with its top `(f − 4)` half-lines cut (Text_DrawGlyph's clip argument). So
  the text area's top is y 0xA9 (338 px).
- **Typing:** it stops at the character whose count equals +0x02 and returns a pointer to it.
- **End of page:** after 3 lines it returns NULL. At a page boundary (`+0x26 == 0` or `(+0x26/12) % 3 == 0`), if
  +0x02 is 0x7FFF (skipped), +0x02 = the count so far.
- **End code:** it returns a pointer to the code byte.

func_0023B168 then (state 1):
- **A text character** (still typing): on even ages +0x02 += 1 → **1 character every 2 frames**. Every 4 frames:
  **SE 0x23** (the blip) and info+0x18 = 2. **Cross/Triangle sets +0x02 = 0x7FFF**: the rest of the visible lines
  appear at once. Also the next lines while it scrolls, until the next page boundary.
- **NULL (3 lines full):**
  - at a page boundary: blinking arrow, waits for Cross/Triangle (**SE 0x1A**) → state 2;
  - otherwise → state 2 at once.

  So a page of 3 lines waits, then the box scrolls one line, types it, scrolls, types, scrolls, types (3 new lines),
  and waits again. Empty lines count as lines. ($X escapes count as 2 characters when skipped but 1 when drawn, so a
  scrolled-past `$R` line makes typing pause one step longer. Harmless.)
- **An end code:** the table in section 1.
  - Codes that close (0, 3, 13, 15, 20, 24) wait without the arrow and play **SE 0x26**.
  - Codes that go on (0x0C, 7, 0x11, 0x13, 0x15, 0x16) draw the arrow and play **SE 0x1A**.
  - Window codes (1, 2, 4, 5, 6, 8, 9, 0x0B, 0x10, 0x12, 0x17) open their child **at once, without waiting**, as soon
    as the text before them is fully typed. The text stays in the box under the window.

### Box, tab and arrow (sprites from SYS/FONTE.GSL)

`func_002063C8(pkt, x, y, u, v, w, h)` (Sprite_Emit) is a textured sprite; x in pixels (0-640), y in **448-line
pixels**. The box and tab set TEX2 to palette (CSA) **7** (the style-2 orange/olive, as in title.md). Box with
Y = 0x158 (344):

| Piece | x, y | u, v | w × h |
|---|---|---|---|
| top left | 32, Y−16 (328) | 0x00, 0x100 | 40 × 56 |
| bottom left | 32, Y+40 (384) | 0x78, 0x100 | 40 × 48 |
| top middle | 72, 328 | strip u 0x50-0x57 repeated (CLAMP REGION_REPEAT UMSK 7 UFIX 0x50; v clamped 0x100-0x137) | 496 × 56 |
| bottom middle | 72, 384 | strip u 0xC8-0xCF repeated (v 0x100-0x12F) | 496 × 48 |
| top right | 568, 328 | 0x28, 0x100 | 40 × 56 |
| bottom right | 568, 384 | 0xA0, 0x100 | 40 × 48 |
| bottom centre knob | 304 (0x130), Y+72 (416) | 0x58, 0x100 | 32 × 24 |

So the box spans x 32-608, y 328-432.

**Name tab**, func_00202B70(Y, w), w = Text_Width(name), at y = Y − 0x1C = 316, all 28 tall:
- left cap: x 0x232 − w (562 − w), uv (0x58, 0x118), 8 wide;
- middle: x 0x23A − w, uv strip u 0x60-0x67 v 0x118 (REGION_REPEAT UFIX 0x60), w + 4 wide;
- right cap: x 0x23E (574), uv (0xD0, 0x100), 24 wide.

The name is printed right-aligned to x 574 at y 0xA0 half-lines (320 px), palette 0. In state 0 the tab's w is
scaled by age / 16.

**Arrow:** func_00203E70(0x136, 0x1A4, 7): icon 7 of the 20 × 20 row at v 0xB4 (u = 7 × 20 + 120 = 0x104), at (310,
420), palette 0. It's drawn while `age & 0x10` (16 frames on, 16 off).

**Fade:** func_00203990(a) writes TEXA (TA0 = a, TA1 = a / 2), scaling the sprites' alpha (title.md). The text isn't
drawn during the open/close fades.

### Choice and Yes/No windows

They use the ordinary window system (title.md):
- `WindowAlloc`; one `Window_AddLine(w, str, 0x10)` per item;
- w+0x5F = 2: style 2 (palette 7 frame, text palette 10);
- `Window_Place(w, 5)`: left at x 0x34, top at 0x18;
- w+0x0E = 1: opens over 8 frames. The cursor is the horn (the "trumpet").

`Window_Update` returns:
- Cross: SE 0x1A; the parent's +4 = the target for the cursor's item, +0x24 = 1; the window closes (+0x0E = 2,
  16 frames) while the message task already goes on;
- Triangle (−1): the last item (code 9), or No (codes 1/2).

The task deletes itself on −2 (closed). The Yes/No strings are D_002A2AF0[language] = "Yes"/"No".

### What Message_Task returns to the scene (func_0022AEB0 state 1)

Set as scene +4 with +0x24 = 1:

| Value | From | The scene does |
|---|---|---|
| 9 \| car<<8 | code 4 | func_0022BB88(car): the change-parts window for that car (state 2), +0x02 = car + 1 (that car onto the turntable). When it ends (-1) it restarts Message_Task on the same table (Q's #1 → flag 0 → "Whose parts") and +0x02 = 1. A child's +4 with bit 0x8000 (a body changed): func_0020AD18(+4 & 0xFF, 0) |
| 8 \| car<<8 | code 6 | PaintShop_Task (func_00256958) for that car (state 2), +0x02 = car + 1; when it ends (-1), as for 9 (paint.md) |
| 6 | code 0x0F | quit the game: Sound_FadeOutSong (func_00258FB0); info+0x28 \|= 0x400000; tells its parent 6; becomes func_0022C418 (overlay 0x08000000, after 64 frames tells its parent 5) |
| 12 | pre-code 0x14 | state 4: passes 12 up; when the message task ends, fades out (func_0022C418) and marks the place found |
| 10 | pre-code 0x12 | passes 10 up (e.g. "Drive around town"); the message goes on |
| 4 | code 3 | start the race in info+0x20: func_0023C7B0, then as 11 |
| 11 | code 0x14 | leave to the place set in info (town/place): passes the value up, info+0x28 \|= 0x400000, deletes itself |
| 5 | code 0 / 0x0D / closed | plain end: passes **10** up and sets scene +2 = 4 |
| 15 | code 0x18 | passes 15 up, fades the song, state 3, info+0x28 \|= 0x400000; town 8: func_0026CB60 (the ending: song 0x0A1C0201, then after 0x34C frames 0x0A1A0201 and func_0026A928); other towns (My City Cinemas): func_0026D2C0 (straight to song 0x0A1A0201 and func_0026A928, the ending replay); plus Fade_OutTask |

After 15 (state 3):
- once you're president (stamp 100): runs the cinema's script `D_002A7ED0[9][8]` (town 9, place 8);
- otherwise: queues stamp 100 (func_0023C8A8(0x64)), sets town 8 place 8, clock info+0x14 = 54000, and tells its
  parent 5.

### The end codes' windows (Q's Factory's menu)

How Message_Task runs them (func_0023B168, states 4-6):
- **Code 4 / 6** (state 5): Message_PickCarTask (func_00239900). A car: the task passes 9 (or 8) | car << 8 to the
  scene at once, +0x27 = 0xFF (no result of its own) and closes (state 3, SE 0x26). Quit or Triangle (-1): message
  arg 0 (+0x27 holds it), the box staying. The window plays SE 0x1A either way.
- **Code 8** (state 4): Message_RaceListTask (func_00239A80) with the arg pointer; its pick or Quit gives the next
  message. The race list's rows: "*" (the player's best place in that race below 6th: save +0xFF0 + race), the race
  name (0x90: same line, item), the rank (0x98, or 0x99 red when the license +0x651 is below it) right-aligned to the
  window's edge (func_00201C80 at x + w), then "Quit" padded with spaces. Locked: Notice_Task (func_0023BB00) with
  "B license is required." / "A ..." / "Super A ...", the list inactive until a press; race 0x19 (Tin Raceway)
  says "Under construction." until stamp 100. A race sets info+0x20 (0x19 becomes 0x1B when the clock info+0x14 is
  outside 54000-162000) and info+0x10 = its row.
- **Code 0x0E**: D_01780A3C (save +0x12DC) = the town, Save_AdventureSaveTask, and the message pointer moves to arg 0
  at once; state 6 draws nothing (the box, the text and the name tab are hidden) until the save task tells it
  (+0x24), then state 1 types that message.

The race table D_002BDDD0 (16 bytes: course, cars, laps, rank; then the race's setup and two functions) and the towns'
ranges D_002BE000 are in the port's `game/story/race_list.cpp`; rows 0-27 with their setups (the opponents and their
parts record) are generated into `adventure_races.inc` by `scripts/elf/gen_adventure_races.py`. Rows 28-34 are the
World Grand Prix's (rank 3, group 10's drivers) and 35-54 the events'; all 56 rows are in `adventure_races.inc`. The ranks' texts D_002A2B08: " RANK C", " RANK B",
" RANK A", "GrandPrix", "" (rank 4: Endurance Run, Tin Raceway and the president's race).

### The other end codes' windows (from the asm)

When the window opens:
- **As soon as the text is typed (state 4, the box drawn under):** 5, 0x0B, 0x10, 0x12, 0x17. Code 6 is code 4's car pick with type 8.
- **After the arrow and a press (SE 0x1A):** 7, 0x11, 0x13, 0x15, 0x16. Codes 0x11 and 0x13 then run in state 6, with the box, text and name tab hidden.
- **After a press with no arrow, closing the box (SE 0x26, result 5):** 0x0D.

Each code:
- **0x05 Message_NumberTask (a, y, x).**
  - A number 0-99 starting at 1, in Window_DrawFrame(0x40, 0x20, 0x3C, 0xC, style 2).
  - The up and down arrows (icons 2 and 3) blink at (88, 40) and (88, 88). The number is right-aligned to x 112 in palette 10.
  - Up takes one off and Down adds one, with repeat. They stop at the ends and make no sound.
  - Cross only: SE 0x1A, then y if the number is a, else x.
  - Used once: Laz's windmill quiz (10, 0), answer 22.
- **0x06:** the car pick, then 8 | car << 8 to the scene, which runs the paint screen (PaintShop_Task, paint.md).
- **0x07 Message_GetItemTask (k, i, n).**
  - Item_Give(k, i), then a centred style-0 notice, open at once: `Get Body "name"`, `Get "name Tires"`. The suffix goes by kind: Tires, Engine, Chassis, Gear, Steering, (brakes none), Wheels.
  - Kind 15's names are D_002F0810 + 8 n.
  - Sound_MuteSong and SE 0x302. A press plays SE 0x1A, goes to message n, and Sound_FadeInSong (song command 6) brings the song back over 64 vblanks.
- **0x0B Message_HornTask (i, n):** SE 0x3C + 2 i (0x10: the player's horn), then message n 64 frames later.
- **0x0D:** the ids up to a 0 are queued by Stamp_Queue on the closing press:
  - The queue is save +0x3300 / +0x3304, 8 at most, first in first out. A stamp already earned or already queued is skipped.
  - Stamps 5, 17, 30, 50 and 100 send mail 0x7B, 0x7C, 0x7D, 0x1B and 0x7F.
  - Queuing doesn't earn the stamp. Stamp_GetBookTask (func_00265568, run by Stamp_ShowTask 0x0026D008) does, when it shows the queue:
    - after a field talk: Adventure state 10, arg 1;
    - after a building's end: state 7, arg 0, once Town_FoundStamp has queued the town's stamp if every place in the town is found.
  - The book:
    - It opens at the stamp's spread (save +0x12E0).
    - After 12 frames the stamp lands over 10 frames: STAMP picture n + 1, half-size (level / 5 + 1) x 64 about (176 + 288 side, 166), with its text. As it lands, Stamp_Earn (+0x518 and +0x528) and SE 0x21.
    - A press (SE 0x1A) turns to the next stamp's spread, with the Notebook's page turn, or stays on this spread.
    - After the last stamp, 10 frames; then arg 0 darkens the held picture for 129 frames (a 0x06000000 overlay).
- **0x10 Message_TeammateTask (a, q).**
  - **Room on the team** (Team_State: the first empty slot): Message_TeamJoinTask runs at once.
    1. Team_Join: the slot's car takes the speaker's paint and body and keeps its parts; +0x674 + 2j = (town, place); the speed tables are reset.
    2. Item_Give of the car's parts 1-7.
    3. A centred style-2 notice, "X is now in your team.", with Sound_MuteSong and SE 0x303.
    4. A press: SE 0x1A, message a, Sound_FadeInSong.
  - **A full team:** a top-left menu: teammate 1, teammate 2, Quit. Quit or Triangle goes to q, the menu fading.
    - Picking a teammate opens Message_TeamLeaveTask, centred and open at once: "Do you want to remove X from your team?" Yes/No.
    - Yes takes the slot car's parts 1-7 back out, clears the group byte, plays SE 0x1A and goes to message a. That message's 0x10 then finds the room and does the join.
    - No plays SE 0x1B and brings the menu back.
- **0x11 (p, n):** Photo_Take(p) copies the car's first 12 bytes into the album and sets the bit. All 100 photos queue stamp 96. It also always sets +0x530 bit 31. Then:
  - SE 0x306;
  - Photo_ViewTask's take mode shows the picture, with the box hidden;
  - Cross or Triangle, with no SE, goes to message n.
- **0x12 Message_NameEntryTask (n):** NameEntry mode 2, the greeting (+0x646), then n.
- **0x13 (n):** Shop_ScreenTask (func_00269E88) runs the shop screen by Room_SceneKind: 1 the Parts Shop, 2 the Body Shop, 4 My City's Recycle Shop. Then n. See parts.md, "7. The shops".
- **0x15 (s, p, n):** Part_Fit(cars[0], s, p), SE 0x303, and message n typed in the same box. Used for the sponsors' billboards (Options 4-8) and for taking off the Flight Wing.
- **0x16 Message_MileagePayTask (k, n).**
  - miles = sponsorDistance[k] / 1609. The remaining metres stay; nothing else is reset.
  - Pay is 10 (k + 1) a mile: Sound_MuteSong, then Money_Pay(-pay), which plays SE 0x305.
  - The window is cells 8-22 by 6-10, style 1: "Earned" with the pay and unit, and "by driving" with the miles and "miles.".
  - Cross: SE 0x1A, message n, Sound_FadeInSong. With no miles it goes to n at once.
  - The distances come from Field_SaveDistance (func_0022E718), whenever the field hands over: the car's odometer / 0x8000 metres goes into +0x658 and, with a billboard, into its sponsor's distance.
- **0x17 Message_BankTask (w, z, ok, short, x).**
  - The amount is in thousands, from 1000. Left and Right pick the thousands, ten thousands or hundred thousands digit (SE 0x1D). Up and Down change it with repeat, wrapping 9 to 0 only on a fresh press.
  - The window is cells 10-19 by 7-9. Below the scene's money box, a second box shows the money after the deal, with a bobbing arrow.
  - Cross:
    - withdraw (w set): cash would pass 999999 → x; savings short → short; otherwise ok, or z for 0;
    - deposit: cash short → short; otherwise ok, or z.
  - Triangle: z, SE 0x1B.
  - The savings are save +0x330C, shown by `$S` in the narrow digits with commas.
- **The money box:** the scene shows it at (0x1B8, 0x1A) in the shops and the bank (Room_SceneKind 1-5).

In the port (`message_task.cpp`): all of the above.
- The paint pick hands the car to `RoomScene`, which runs `PaintScreen` (paint_screen.cpp) and restarts the talk after.
- 0x13 returns 0x13 to the scene, which runs the shop (`PartsScreen::StartShop`) while the talk waits with the box hidden (`kChildShop`); `Message_ShopDone` goes on to n.
- `RoomScene` and `Field` redraw the cars after 0x15 and 0x10.
- `StampBook` (`field_menu_screens.cpp`) is the stamp-get book.
- The Options table's billboard sponsor is fixed (`gen_parts_tables.py` read the +0xC word's high half: billboards 9-13 instead of 0-4).

Checked in the port:
- Peach Town's cafe Owner (1, 12): Get "Billboard", fitted, then "For every mile ..." on the next visit.
- Kinsera (1, 7): Get "Local Peach Wine", then the stamp book landing stamp 2 after the scene.
- Quick-Pic Shop No.1 (1, 18): the photo.
- Barthou's trade (field.md).

Not checked live: the number, horn, greeting, mileage-with-miles and bank windows (their places are outside Peach Town or need save edits).

### Sounds (Sound_PlaySe ids)

| Id | When |
|---|---|
| 0x25 | the box opens |
| 0x23 | the typing blip, every 4 frames |
| 0x1A | arrow advance, window choices |
| 0x26 | the box closes |
| 0x1E | the one-line notice (func_0023BB00) |
| 0x302 | the item-get window (with Sound_MuteSong) |
| 0x303 | a part fitted (code 0x15) |
| 0x306 | a photo (code 0x11) |
| 0x1D / 0x1B | number and bank entry moves / back |

---

## 3. Function names (proposed, names.csv style)

```
0x0022AEB0,Shop_SceneTask,,"a building scene (town info+0x23, place info+0x21): room, cars, song (0x0A0B0104 for the president's office 8/9); at age 0x41 starts Message_Task with D_002A7ED0[town][place]; acts on its result (9 parts, 8 paint, 6 quit, 4/11 leave, 10/12 pass up, 5 end, 15 the ending)"
0x0023B168,Message_Task,,"the dialogue box: opens (16 frames, SE 0x25), runs a message table (entry 0 the speaker, n message n): pre-codes, typing 1 char / 2 frames (SE 0x23 every 4), 3 lines a page, scroll 6 frames a line, end codes 0-0x18; closes (SE 0x26) and returns its result code"
0x0023AD00,Message_RunPreCodes,,"runs a message's leading control codes 0x01-0x1D (flag/stamp/item/team/town tests that jump to message n, set/clear flags, scene results 10/12); returns the pointer to the text"
0x0023A8E8,Message_DrawText,,"draws 3 lines from line +0x26/12 at x 70, y 0xAD half-lines (+12 a line), scrolled and top-clipped by +0x26%12; expands $N $U $G $R $S $A $T; stops at character +0x02 (returns it), at an end code (returns it) or after 3 lines (NULL)"
0x002028B0,Message_DrawBox,,"the dialogue box from FONTE palette 7: 6-piece frame x 32-608, y Y-16..Y+88 (middle rows tiled from 8-texel strips at u 0x50/0xC8) and the knob at (304, Y+72)"
0x00202B70,Message_DrawNameTab,,"the speaker's name tab right-aligned to x 0x23E, y Y-0x1C, w+36 wide (caps u 0x58/0xD0, strip u 0x60)"
0x002039E8,Text_SetPalette,,"TEX2 with the font's palette (CSA) if it changed (D_00335688)"
0x00203990,Gfx_SetTexAlpha,,"TEXA TA0 = a, TA1 = a/2: fades the 2D sprites"
0x00203E70,Icon_Draw,,"20x20 icon n from FONTE row v 0xB4 (u n*20+120), palette 0; n 7 = the message arrow"
0x002063C8,Sprite_Emit,,"one 2D sprite packet: x pixels, y in 448-line pixels, uv, size; CLAMP region clamp to the uv rect"
0x00204230,Text_PrintClipped,,"(inferred from calls) a string at x, y half-lines with a top clip; returns the end x"
0x00204460,Number_PrintClipped,,"(inferred from calls) a number for the $S escape"
0x0023BCD0,Message_ChoiceTask,,"end code 9: window of 0-terminated items up to a 0x09 byte, then a target per item; style 2, top left; Triangle = last item; result = target"
0x00239260,Message_YesNoTask,,"end code 1: Yes/No window (style 2, top left), result arg[0]/arg[1]; Triangle = No"
0x0023BC90,Message_YesNoNoTask,,"end code 2: Message_YesNoTask with the cursor on No"
0x00239900,Message_PickCarTask,,"end codes 4/6: you, teammates, Quit; result type | (car << 8), Quit/Triangle -1"
0x00239A80,Message_RaceListTask,,"end code 8: the town's races (license marks, locked ones say which license), Quit; sets info+0x20 and +0x10"
0x00239F68,Message_GetItemTask,,"end code 7: 'Get Body \"name\"' window, gives the item (func_0023D2C0), SE 0x302"
0x0023A140,Message_MileagePayTask,,"end code 0x16: 'Earned N gold by driving M miles.'"
0x0023A320,Message_NumberTask,,"end code 5: a number 0-99 (up/down), compared with arg[0]"
0x0023A460,Message_BankTask,,"end code 0x17: amount entry for the bank savings (save +0x330C)"
0x00239748,Message_TeammateTask,,"end code 0x10: teammates menu (func_00239378 / func_00239508)"
0x0023BE18,Message_NameEntryTask,,"end code 0x12: NameEntry_Task mode 2 (the greeting), then message arg[0]"
0x0023BE88,Message_PictureTask,,"end code 0x0B: func_002083C0 with D_002F00AC[arg0] for 64 frames (what it shows unknown)"
0x0023BB00,Message_NoticeTask,,"a one-line window (SE 0x1E), result 0: 'B license is required.' etc."
0x0023C8A8,Stamp_Queue,,"queue stamp n (save +0x3300/+0x3304, 8 max) unless earned; some stamps add mail"
0x0023C7B0,Area_MarkFound,,"clears the place's 'not found' bit (save +0xBD8 word town, bit place); slots 1-9 set a stamp-shown bit"
0x0023C5C0,Photo_Take,,"photo n taken (save +0x508); all → stamp 0x60"
0x0023C418,Car_SetPart,,"record part slot s = p (+0xC+s) and its flags"
0x0023D7A8,Save_SetEventFlag,,"sets event flag n (save +0x538)"
0x0023D7E0,Save_ClearEventFlag,,"clears event flag n"
0x0023D628,Stamp_Earned,,"stamp n-1 earned (save +0x518); 0: all"
0x0023D730,Stamp_SetTextShown,,"stamp n-1's text shown (save +0x528)"
0x0023D8E8,Photo_Taken,,"photo n-1 taken (save +0x508); 0: all"
0x0023D488,Item_Count,,"item (kind, n) held (save +0xC30; kinds 0/15 one bit, others count of 5 words)"
0x0023D2C0,Item_Give,,"adds item (kind, n)"
0x0023D390,Item_Take,,"removes item (kind, n)"
0x0023D5F8,Area_NotFound,,"place (town, place) not found yet (save +0xBD8)"
0x0023D5E0,Car_HasPart,,"record +0xC+slot == part"
0x0023DBC8,Team_State,,"1 no teammates, 2 one, 0 full"
0x0023DB80,Team_Find,,"1/2 if (group, index) is teammate 1/2"
0x0023DCC0,Opponent_Beaten,,"opponent (group, index) placed behind (save +0x558)"
0x0023DBF0,Pickup_Taken,,"field pickup n-1 collected (save +0x548 bit clear)"
0x0023D968,Pickup_CountLeft,,"pickups still out (bits set in save +0x548)"
0x0023DD30,Rally_Checkpoint,,"Q's Rally: stores the split for checkpoint n, the total at 6"
0x0023DDB8,Rally_NextCheckpoint,,"index 0-5 of the next Q's Rally checkpoint"
0x0023CB68,Resident_MoveIn,,"My City resident n moves in (save +0x670; mail; stamp 0x33 when complete)"
0x0023DE30,Resident_MovedIn,,"My City resident n has moved in"
0x0023DE50,Resident_HereMovedIn,,"the resident of (town, place) has moved in"
0x0022BB88,Shop_PartsTask,,"change parts window for a car (scene result 9)"
0x00256958,PaintShop_Task,paint_screen.cpp,"paint a car (scene result 8; paint.md)"
0x0026CB60,Ending_Task,,"(inferred) the ending after beating the president: song 0x0A1C0201, then 0x0A1A0201 and func_0026A928"
0x0026D2C0,Ending_ReplayTask,,"(inferred) the cinema's ending replay: song 0x0A1A0201 and func_0026A928"
0x00258FB0,Sound_FadeOutSong,,"(already in sound.md) song command 5"
```

---

## Notes and caveats

- The pre-code numbers are the jump table index + 1, checked against the asm (`jtbl_002EC750`: codes 9, 10 and 12 go
  to the "unknown" path). The $-escape table (`jtbl_002EC6C0`) was checked too.
- Several helpers (func_0023D488, the stamp/photo functions) are taken from drafts whose files hold several
  functions; their meanings match save.md.
- Some messages are only pre-code jumps followed by a 0 (e.g. Q's #39: `0E 28 28 29 2A 2B 2C 00`). They rely on the
  jump always being taken; otherwise the 0 is an "unknown pre-control code" and the interpreter runs on into the
  next string.
- `all_scripts.txt` guesses each table's length from the next table's address, so a few tables run on into
  unrelated pointer data at their end (the garbage entries are obvious). Group 8's entries 0 and 9 are exact.
- Not checked: func_00204230 / func_00204460's bodies; func_002083C0 (code 0x0B); info+0x28 bit 0x400's use; what
  info+0x20 = r selects beyond being the race index (the race list also sets it).
