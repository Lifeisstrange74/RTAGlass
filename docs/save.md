# Save data

Road Trip keeps two player records in memory, `D_0177F760 + 0x3448 p` (player 1 and 2), with `D_0177EA50` pointing
at the current one. A memory card save is **one record, byte for byte**: `/BASLUS-20398/BASLUS-20398`, 0x3448 bytes
(func_00228708 writes it after `icon.sys`, D_0029ED20 0x3C4 bytes, and `choroQ.ico`, 0x8158 bytes from 0x100000;
func_00228510 reads it back). There is no checksum, magic or version; func_002283E0 only checks that 0x3448 bytes
were read. Options (the key setup, the radio) are part of the record; there is no separate settings file. The race
reads each player's key setup from their record (Car_ReadPad, the camera and horn keys). The PC keeps its own control setup
apart, in controls.json (controls.md); the record's key setup is kept for the card but not read.

The PC port keeps the same record (`src/game/save`) and writes it as JSON next to the executable,
`save1.json` / `save2.json` (the two card slots). The JSON's sections and keys are the fields below. Bytes the game
never uses are written only when they aren't zero, so a record goes through the JSON and back unchanged. During
development the format may change without keeping old saves readable.

How the map was made: a static scan of every load and store into the record (per function, registers holding the record's address, through `D_0177EA50` too), a sweep of the symbols
and immediates in the range, and the asm of each function that touches a field. **V** = read in the asm and clear
from it; **I** = inferred (plausible, not proven). "Never accessed" means none of those found a use.

## When the game saves

- **Adventure:** the save point (dialogue in func_0023B168) sets `+0x12DC` (the town) and runs func_00229538: pick
  slot 1 or 2, then func_002291C8.
- **2 Player:** the course select's Save&Exit card (id 0x1F) opens func_00248BF0: "Save Player 1(MEMORY CARD slot
  1)", "Save Player 2(MEMORY CARD slot 2)", "End Game". Each runs func_00229758(p): the other record is copied to
  0x110000, func_002291C8(p | p << 8) writes record p to slot p whole, and the copy is put back (step 2 below loads
  the card's file over the other record).
- **func_002291C8, Save_SaveTask** (jump table D_002EB860; its low byte is the slot, the next the record): card check
  (func_002282C0: not inserted / unformatted → format / under 60 KB free); read the card's file into the *other*
  record (func_00228510); then by the read's result: an existing save → Mc_OverwriteTask (func_002280D8): "Do you
  want to overwrite?" over func_00227F90's summary (name, town, play distance, money), with "This is not your saved
  data." and No first when the two `+0x3438` IDs differ; result 0 → message 22 + p "There is no data. Do you want to
  create new saved data?"; −3 → 28 + p "Load failed. Do you want to newly create saved data?" (No first); others →
  18 + p "Load failed." Yes writes (func_00228708, or func_0022A168 after the overwrite question): "Saved." (30 + p)
  or "Save failed." (20 + p). Mc_MessageTask's messages from 12 come in pairs, + p, each on player p's pad. Which
  read result means "no file" and which "unreadable" is from the messages' wording **(I)**.
- **Continue** (the Adventure menu's second item) loads: Mc_LoadSlotWindowTask (0x00228D10) is the same "slot 1" /
  "slot 2" window, then Mc_LoadSlotTask (0x00228B10) on the slot picked: the card check (func_002282C0), the read
  (Mc_ReadRecordTask) straight into record 0, then by the read's result: none → message 26 "There is no data.",
  unreadable → 18 "Load failed." (both come back to the window), a save → Mc_OverwriteTask with 0x100: "This saved
  data?" (D_002EB4C0) Yes / No from Yes, over the summary, the license clamped to 3. Yes plays 0x303 and the
  Adventure menu ends with 1: the root runs NewGame_Continue (0x0026D370): the loader's jobs, D_0177EA50 the record's
  three cars, the clock 0x13C68 and Room_Start on `+0x12DC`, place 0 (that town's Q's Factory). No, or Triangle on
  the window, ends it; Triangle on the window ends the Adventure menu too (back to the main menu). Checked in PCSX2
  (a save made at `factory_menu.p2s`, then Continue after a reboot): the window, the summary at row 4 with the
  question under it, then NOW LOADING and the factory's "Okay, let's get started." A card that was unformatted asks to
  format and then saves without the "create new" question.
- **Quick Race never saves.** QuickRace_Task's first frame runs func_00229FD8, a new record for both players, and
  the Quick Race card only has Exit. Checked in PCSX2: a Peach Raceway finish set `+0xB78` (388, 1592, 1 attempt,
  1st); back at the title it was still there; entering Quick Race again reset it.
- New records: **func_002277B0** (Save_Init) zeroes one and fills the defaults below; **func_00229FD8**
  (Save_InitBoth) does both, then player 1 gets money 1000, the paint table from `D_002ED720 + 4 + 0x44 body` and
  `+0` = body 0's paint. **NewGame_Setup** (0x0026D438) runs it and gives player 1 one of six starting bodies (0x2B,
  0, 0x75, 0x8C, 0x72, 0x5D) by a value from the pad state modulo 6 (func_00229F70; which value hasn't been traced).
- Save_InitComplete (func_00227C68, from the test menu Debug_TestMenuTask 0x0026B848, Adventure menu result 2) fills
  a "complete" record 0 after Save_InitBoth: Alec, euro, license 3, money 10000, bank 100000, the bodies and parts,
  the teammates, the photos, stamp texts, mails and beaten opponents (V: the asm; the Test Menu extra, extras.md). The
  PC keeps it from silently replacing a save: Save_SetTestRecord.

Both fresh PCSX2 dumps equal what the port's Save_InitBoth makes, byte for byte.

## The record

### 0x0000 Cars: 3 × 0x1C (V)

Slot 0 the player's car, 1 and 2 the teammates' (func_0023DA58 fills them; Car_Init takes the slot from car flags
`(flags & 0x30) >> 4`, flags 0x40 the second record).

| off | type | field |
|---|---|---|
| +0x00 | u32 | paint: six 4-bit indices into D_0029D150 (two RGB colours, func_00223150); bits 24-31 copy +0x05 |
| +0x04 | u8 | body (CARn/Qnn.BIN) |
| +0x05 | u8 | the wheels' colour, 0-11 (V: the Paint Shop's Wheel panel, paint.md): TIRE.BIN's CLUT and D_0029D1A0's levels for WHEEL.BIN; JSON `wheelColor` |
| +0x06 | u8 | wheels (a copy of part 7) |
| +0x07 | u8 | the billboard's sponsor, 0-4 (Options part, `D_002F03E0 + 0x10`) |
| +0x08 | u16 | part flags (func_0023C418): 0x1/0x2 engine type, 0x4 Flight Wing, 0x8 (runtime, wing out: I), 0x10 lights, 0x20 police light, 0x40 propeller, 0x100 water ski, 0x200 billboard, 0x400 big tires, 0x1000 wing set, 0x2000 jet turbine, 0x4000 sticker |
| +0x0A | 2 | never accessed (copied with the first 12 bytes) |
| +0x0C+t | u8[16] | the part fitted in category t: 0 the starting body, 1 tires, 2 engine, 3 chassis, 4 transmission, 5 steering, 6 brakes, 7 wheels, 8 lights, 9 wing set, 10 special, 11 option, 12 sticker, 13 horn (sound D_002F00AC), 14 meter (loaded as 0xA0000 \| meter); 15 never accessed |

### 0x0054 Photo album: 100 × 12 (V)

func_0023C5C0(n) copies slot 0's +0..+0xB into entry n-1 ("Do you want to take a picture?"); func_0022BFD0 draws
one, func_002665B8 browses. 0x504-0x507 never accessed.

### 0x0508 Bit sets: u64 pairs (100 or 128 bits)

| off | init | field |
|---|---|---|
| 0x508 | 0 | photo n taken (V; all 100 → stamp 0x60 "The picture album is full!") |
| 0x518 | 0 | stamp n earned (V; func_0023C8A8 queues, func_0023D6E0 sets, func_0023D9E0 counts) |
| 0x528 | 0 | stamp n's text shown in the book (func_002652B8 prints it only then) (mechanics V, meaning I) |
| 0x538 | 0 | event script flags, 128 (V: func_0023D778/7A8/7E0 from the script interpreter 23AD00; bits unnamed) |
| 0x548 | all set (100) | field pickups `D_002A3970[i]` not yet collected (V: 241BA8 clears near the car, SE 0x29; mail 0x7E at 70 left); what they are is unknown |
| 0x558 | 0 | u64[10]: opponents placed behind in the Adventure, word = group (13 → 9, else group-1), bit = index in D_002C22C8[group] (V: 237C48 → 23DCF8) |

### 0x05A8 Mail (V)

u8[128] the mail box, oldest first, bits 0-6 the mail (D_002DB948), bit 7 read; full: the oldest drops.
0x628 s32 the count. Mail screen func_00264498.

### 0x062C Player

| off | type | init | field |
|---|---|---|---|
| 0x62C | char[19] | "UnnamedRacer" | name (name entry: 9 characters) (V) |
| 0x63F | char[5] | "gold" | currency unit (V) |
| 0x644 | 2 | | never accessed |
| 0x646 | char[6] | | greeting (name entry mode 2; read by the text expander 23A8E8) (V) |
| 0x64C | 5 | | never accessed |
| 0x651 | s8 | 0 | **license**: 0 none, 1 B, 2 A, 3 Super A (V: Results_Task, Results_DrawRace; load clamps to 3) |
| 0x652 | u8 | 0 | home interior, the garage's Redecorate (V) |
| 0x653 | 1 | | never accessed |
| 0x654 | s32 | 0 (P1 1000) | money, up to 999999 (V: func_0023DC58) |
| 0x658 | s32 | 0 | distance driven, metres (car +0x204 >> 15; shown /1609 as miles) (V) |
| 0x65C | s32[5] | 0 | distance with each billboard sponsor, paid per 1609 by func_0023A140 (V) |
| 0x670 | u32 | 0x100 | My City: bit k the resident D_002A2B70[k] has moved in (V: func_0023CB68, mails "Now Open!"; complete at 0x3FFFFFF, stamp 0x33) |
| 0x674 | s8[2][2] | 0 | teammates 1 and 2: (group, index) in D_002C22C8; 0 none (V: func_0023DA58) |

### 0x0678 2 Player rivals: 20 × 0x30 (V)

Other saves met in 2 Player (func_00237AC0 finds or adds the other record's `+0x3438`): +0x00 u64 its ID, +0x08
char[20] its name (empty: free; when all are taken entry 19 becomes "ANYBODY"), +0x1C u8[10] wins against it by
course row, +0x26 u8[10] races against it (both saturate at 255).

### 0x0A38 2 Player course records: 10 × 0x20, by row of D_002BE1D8 (V)

+0x00 u32[3] the top three best laps, +0x0C u32[3] the top three totals: time in bits 0-23 (frames), bits 24-31 whose
(0xFF this record's player, else a rival index); 0 empty. +0x18 s32 this player's best lap, +0x1C best total (init
0x7FFFFFFF). Written by func_00237C48's (mode & 2) branch into both records; shown by func_00269F48 and
func_00247E38.

### 0x0B78 Quick Race records: 8 × 0xC, by course card (V)

| off | type | init | field |
|---|---|---|---|
| +0x0 | s32 | 0x7FFFFFFF | best lap (frames) |
| +0x4 | s32 | 0x7FFFFFFF | best total, green to the line (frames) |
| +0x8 | s16 | 0 | attempts (races finished) |
| +0xA | s16 | 0xFF | best place, 0 = 1st |

The index is the card (row of D_002BE158: setup +0x20), not the course id. func_00237C48 mode 1 keeps the minimum
of each against D_01787074 (the race's best lap: Race_CompleteLap keeps a 0x10-byte slot per player at D_01787068,
the last three laps then the best), D_01786FF0 (the clock at the finish) and D_0178700A (the place), and adds one
attempt. Race_SetupTask calls it at 0x0020F404 (after the replay) and 0x0020F724 (no replay); Retire and falling
off don't. **Race_InitTimes** (0x0022D288) takes the card's best lap as D_00335700, the lap to beat for the
brighter chime (SE 0x2F). **CourseInfo_DrawRecords** (0x00247BF0) prints player 1's name, the attempts at (0x226,
0x95), " Best Total Time" +0x4 at (0x1D6, 0xA1), " Best Lap Time" +0x0 at (0x1D6, 0xAD), and " Your Best Result"
`D_002A8ED0[place]` (" 1ST" .. "24TH") or " --" at cell (24, 15).

### 0x0BD8 Areas: u32[22] (mechanics V, meaning I)

A word per area, a bit per place. Init: 0, 0x3DFFF, 0x7FBFF, 0x1FFFFF, 0xFBF, 0xF, 0xFFFFF, 0x3FFFF, 0x17F, 0x7FFFDF,
0xF, 0xF, 0xF, 0x1F, 7, 0x3F, 3, 1, 1, 0xF, 1, 1. func_0023C7B0 clears a bit (an event's +0x21 bit in word +0x23)
and, for slots 1-9, sets a stamp-shown bit; func_0023D5F8 tests. The minigame record screens show "????????" while
their bit is set (Q's Rally: +0xBFC bit 20). So: a place in that area not yet found.

### 0x0C30 Items: 16 kinds × 5 u32 (V)

func_0023D2C0 / 0023D390 / 0023D488 / 0023D548 take (kind, bit, player). Kinds 0 and 15 are plain 160-bit sets
(bit n in word n >> 5): kind 0 the bodies owned, kind 15 miscellaneous flags (I). Kinds 1-14 are the part
categories (as +0x0C+t; tables D_002ECA80 tires, D_002ECEDC engines, ... D_002F00AC horns): 0-5 of each item,
in unary: copy k sets the item's bit in word k. Init: word 0 of kinds 1-14 = 1 (one of each kind's first part).

### 0x0D70 Body paints: u32[151] (V)

Each body's paint, kept while another body is fitted (the parts shop, func_0022BB88, swaps them through `+0`).
Player 1's start as `D_002ED720 + 4 + 0x44 body`; player 2's stay 0. Quick Race paints the card's body from it.
0xFCC-0xFEF never accessed.

### 0x0FF0 Adventure course records: 3 rows × 26 courses (V)

Rows: the player, teammate 1, teammate 2. Course index = row of the course table, minus 2 for rows 0x1A-0x1B.

| off | type | init | field |
|---|---|---|---|
| 0x0FF0 | u8[3][26] | 0xFF | best place, 0 = 1st (Results_Task's license check needs < 6 on every race of the class) |
| 0x103E | 2 | | never accessed |
| 0x1040 | s32[3][26] | 0x7FFFFFFF | best lap |
| 0x1178 | s32[3][26] | 0x7FFFFFFF | best total |
| 0x33C0 | u8[26][2][2] | 0 | the teammates' (group, index) each row was set with; a new teammate's better result replaces the whole entry |

Race_InitTimes takes `[0][course]` from 0x1040 as the lap to beat (rows 0x1C-0x1F read 0x1038 + 4 row, landing in
teammate 1's courses 0-3).

### 0x12B0 Minigame records (V)

0x12B0 Q's Rally best time (0x7FFFFFFF); 0x12B4 s32[5] Figure 8 steps 1-5; 0x12C8 Ski Jumping best metres (0);
0x12CC Curling best points (0); 0x12D0 s8[9] Golf strokes over par by hole (0x7F none); 0x12D9 Golf total.

### 0x12DA Adventure state and options

| off | type | init | field |
|---|---|---|---|
| 0x12DA | u16 | 0x3FF | a bit per town (D_002BE438), cleared by events; "not yet visited" is I |
| 0x12DC | s8 | 0 | the town it was saved in (V) |
| 0x12DD | s8 | 2 | radio: 0 off, n station n-1: 1 Peach FM, 2 E-Radio (V; Radio_UpdateMenu, sound.md) |
| 0x12DE | s16 | 0x5A | radio volume 0-0x80 (V) |
| 0x12E0 | s16 | 0 | the stamp screen's cursor (V) |
| 0x12E2 | s16 | 0 | the photo album's cursor, 0-99: Photo_Take (func_0023C5C0) sets it to the photo taken (Pictures_Task, field_menu.md) |
| 0x12E4 | 4 | | never accessed |
| 0x12E8 | u16[10] | | **key setup**, pad bits (Car_ReadPad): accelerate Cross 0x40, brake Square 0x80, reverse Circle 0x20, R2 0x02, L1 0x04, L2 0x01, horn R1 0x08, camera Triangle 0x10, lights 0x08 (the options screen keeps it equal to the horn), map Select 0x100 (V; func_00229F20 resets) |
| 0x12FC | 4 | | never accessed |

### 0x1300 Teammates' speed tables: 2 × 16 × 256 bytes (V)

A byte per AI zone (5 km/h; 0 none), 16 tables (course D_002AC080[course]) per teammate. Ai_CopySpeedTables
(0x00250DD0) copies `D_002A9B80 + 0x100 D_002AC098[i]`; Ai_Drive changes them as the teammates drive, so what they
learn is saved.

### 0x3300 Stamps, bank, World GPX (V)

0x3300 s32 stamps waiting (up to 8, "Stamp QUE Overflow !!"), 0x3304 u8[8] their ids; 0x330C s32 bank savings;
0x3310 s32 World GPX stages done (0-6); 0x3314 s8[7][24] GPX points [stage][car] (0x1E − place); 0x33BC-0x33BF
never accessed; 0x33C0 see Adventure; 0x3428-0x3437 never accessed.

### 0x3438 ID (V)

u64, written by NameEntry_Task (0x0025BCF0, name mode): bytes 0-2 from a value in the shared task state (not
identified), bytes 3-7 the name's characters 4, 3, 2, 1, 0. 2 Player and the overwrite check compare it.
0x3440-0x3447 never accessed.

## Corrections this made to other notes

- The license is `+0x651`, not `+0x4` (finish.md); `+0x4` is the body.
- `+0x674` / `+0x676` are the player's **teammates** (ai.md called them other players' cars).
- The parts record (car +0x180, the save car's `+0x0C`): byte +0xE is the meter in the save's cars; car_physics.md's
  "flags" there is an opponent record's (its u16 goes to car +0x190). The save car's part flags are the u16 at `+0x08`.
- The Quick Race record's first word is the best lap and the second the best total (finish.md had them the other way).

## The port

- `game/save/save_data.h`: `SaveRecord`, the record with every field at its offset (static_asserts on the size and
  key offsets), `Save_Init`, `Save_InitBoth`, `Save_SetStartingCar`, `Save_ResetButtons`, `Results_UpdateQuickRace`
  (func_00237C48 mode 1). `save_tables.inc` holds the body paints and the default speed tables, generated from the
  executable and checked against the PCSX2 dump.
- `game/save/save_file.h`: the JSON (a table of fields covers every byte once: `Save_CheckFields`), the raw PS2 file,
  and the slot paths.
- `game/save/save_screens.h`: Mc_MessageTask's messages (`Mc_MessageWindow`), Save_DrawSummary, and 2 Player's save
  (`VersusSave`: Save_VersusSaveTask and Save_SaveTask for files). A PC slot is always inserted and formatted, so the
  card checks never fail: a missing file asks to create it, an unreadable one says "Load failed." and asks the same,
  an existing one gets the overwrite question. The file is read into a spare record, so the other player's record
  is never touched. The Adventure's saving will use the same pieces.
- `Versus_FindRivals` (func_00237AC0, `save_data.h`): Versus_Task's rival entries.
- QuickRace starts a new save (both records) like QuickRace_Task, paints the card's car from it, gives the race the
  card's best lap (Race_InitTimes), updates the card after a finish, and the course card shows the records.
- New Game runs NewGame_Setup (the random pick is a PC random number). Name entry writes the name, currency and
  greeting into player 1's record.
- `RTAGlass.exe --save-convert IN OUT` converts between `.json` and a PS2 card file (any other name);
  `--save-new P1 P2` writes the two new records.
