# The opponents' AI

The race's opponents drive through the same input bits as a player's pad (car_physics.md, "Input"). Car_Input
(0x0021A990) runs **Ai_TrackZone** for every car that has a path, the player's included, then for a car with record
flag 0x80 it calls the AI through the pointer D_003356DC, which is **Ai_Drive** (0x002508E0). The physics then
treats the result exactly like the pad's. The code is the original's `car/ai` module; the port's is
`src/game/race/ai.cpp`.

There's no look-ahead, no racing line search and no rubber band. The AI holds the speed the course's table gives
for the stretch it's in, under a cap set by its grid slot, and steers for a gate two stretches ahead.

## The path (Ai_Init 0x0024FCD8)

Car_Init calls Ai_Init with the car's number and slot. It fills four record fields:

| field | what |
|---|---|
| +0x250 | the gates: D_002BD578[path].lines |
| +0x254 | the zones: D_002BD578[path].zones |
| +0x258 | the speed table, a byte per zone |
| +0x25C | the car's zone marks, D_01759D70 + 0x100 × car number (cleared) |

The path is the course id, except 0x0B uses 0x06's and 0x10 uses 0x0F's (the table holds the same pointers there
anyway). D_002BD578 has 0x18 entries: ids 0-17 are the race courses, and 19, 20 and 22 are used by other modes
(18, 21 and 23 are empty). On course −1 or 0x22 (the field and Cloud Hill's) the path comes from func_0025FBC0 (Ai_TownPath, the town
cars' loops: field_town_cars.h) instead.

**Gates** are 0x18 bytes: {x0, z0, x1, z1, split x, split z}. A gate is a short line across the racing line, 2-20
units wide (Peach Raceway's are mostly 2-6). The split point is 0 except where the path forks.

**Zones** are 8 bytes: {in gate, out gate, previous, previous alternative, next, next alternative, rank, 0}. A zone
is the stretch between its two gates. Where the course forks, the two next (or previous) zones differ, and the line
through the in and out gates' split points decides which branch a car is on. The rank orders the zones for the race
order; branches of a fork share ranks.

Peach Raceway has 35 gates and 35 zones, with one fork (zone 30, on the main straight). The biggest paths are C13's (224) and
C18/C19's (220). Every zone's gate and neighbour indices are in range on all 18 courses.

**Speeds.** In a Quick Race, and any race where pad +0x0A (the mode) isn't 0, the table is
D_002A9B80[D_002AC098[D_002AC080[path]]], 0x100 bytes per table and one byte per zone (`& 0x7F`). Units are 5 km/h,
and 0 means no limit. Peach Raceway runs from 11 (55 km/h, before the first corner) to 38 (190 km/h, the back
straight). Other modes use:
- with mode 0: D_002A9B80[race +0x20];
- for path 0x13: D_002ABE80;
- for paths from 0x14: D_002ABF80;
- for the Adventure's teammates (car flags 0x10, 0x20): their own copy in the save, D_01780A60 = record +0x1300
  (save.md): +0x1000 for flag 0x20, player 2's record (+0x3448) with flag 0x40, + 0x100 x D_002AC080[course].
  Ai_CopySpeedTables (0x00250DD0) fills it from the same 16 tables, and Ai_Drive changes it (step 4).

## Tracking (Ai_TrackZone 0x00250068)

Every line test is the same side test: `(z1 − z0)(px − x1) − (x1 − x0)(pz − z1) ≥ 0` means "on or past the line" the
way the path runs.

1. If the car is past its zone's in gate, step on (to the next zone, or at a fork to the branch the split line
   says) while it's past the out gate. Otherwise step back (to the previous zone, likewise) until it's past the in
   gate. The record starts at zone 0, so the first frame walks to the grid's zone.
2. +0x24B = the zone's rank.
3. The next zone: the alternative if the car is past the split line, otherwise the next. (They're the same zone
   unless the path forks.)
4. The course's heading: the angle of the out gate (x0 − x1, z0 − z1) − 0x4000, square to the gate. +0x248 counts
   frames where the car's heading is more than 0x5000 (112°) off it, and resets otherwise.
5. +0x24C = the distance from the car to the middle of the next zone's out gate, along that heading.
6. Returns the next zone. With the debug flag (pad +0x0B) it prints `ZONE n IN/OUT ...` and `TARGET LINE`.

Angles are 16-bit: atan2(dx, dz) × 0x8000/π (D_00332F9C = 10430.378), so 0 is +z and 0x4000 is +x. A car's
heading +0x1D4 uses the same scale.

## Driving (Ai_Drive 0x002508E0)

The car's speed is its driven wheels', +0x1B8 × 216 / 163840, in 5 km/h (the speedometer's 216 km/h per 0x8000).

1. **Throttle.** Accelerate (bit 1). If the zone has a speed, being 1 over it means coast (no bits), and 2 or more
   over means brake (bit 2).
2. **The cap.** If +0x246 is set, the speed in 2.5 km/h (× 216 / 81920) minus the cap overrides that: 3 or more
   over means brake, 1-2 over means coast.
3. **Steering.** Ai_TargetAngles (0x0024FED8) gives the angles from the car to two points of the next zone's out
   gate:
   - `first`: the gate's first end, or its middle in lane 1;
   - `second`: its second end, or its middle in lane 2.

   The lane is +0x194 & 3, so lanes 0 and 3 use the whole gate. With the car's heading h:
   - If second − h > 0, the target is to the right: steer right (0x2000). Beyond 0x4000 (behind the car) there's no
     steering bit; the car's heading turns 0x100 a frame on the spot instead, while race flags & 0xC == 4 (green).
   - If first − h < 0, the same to the left (0x8000, −0x100).
   - Otherwise the car points through the gate. Full throttle becomes 9 (adds bit 8, R2).

   The steering bits are only set while the lap zone +0x19A is non-zero. It's 0 on the grid, so the field drives
   straight until it reaches the line (hud.md, "The line").
4. **Marks.** The zone's mark in +0x25C becomes 0x80 if the car steered there or 0x40 if it didn't, OR the speed.
   The marks are only read back when Car_Input passes a non-zero last argument: flags & 0x30 (& 0x31 when the race's
   +0x0B is set) for an AI-driven car with flags 0x33. That is the teammates, always: Race_CreateCars gives teammate 1
   (save +0x674) flags 0x90 and teammate 2 (+0x676) 0xA0, and player 2's teammates (record 2) 0xD0 and 0xE0. Then
   Ai_Drive moves the zone's speed in their saved table towards what they drove there: one under after a steered
   (X) pass, one over after a clean (O) one, within 1-62. So the teammates' speeds change race by race and are
   saved. It isn't a mode or a setting; Ai_DrawZoneTable (0x00250738) is a debug view of the marks.

   In detail (Ai_Drive's start, before the throttle), with z the car's zone, n the next one (Ai_TrackZone's), S the
   table and M the marks, and a value that isn't 1-62 not counting:
   - S[n] set and M[n] marked: if S[n] is under M[n]'s speed − 2, M[n] loses 0x40, and S[z] (if 2 or more) comes
     down one when it's over M[z]'s speed − 1.
   - Then M[n] steered (0x80): S[z] = M[z]'s speed − 1 if S[z] was over that or 0. Or M[n] clean (0x40): S[z] =
     M[z]'s speed + 1 if S[z] was under that. Either way M[n] keeps only its speed.
   - The marking (after the steering): M[z] = 0x80 if the car steered (or turned on the spot), 0x40 if not, then
     OR its speed if that is 2-62. So a mark is the last frame the car spent in the zone.

   PCSX2 (`race_er.p2s`, 2 Player Race Right-Away on Endurance Run, both players idle): after about 184 s the four
   teammates' tables for the course (table 15, record +0x2200 and +0x3200) had changed in 38-48 of the first 96
   zones, by −3 to −4 on average, mostly down (`captures/versus/teammate_learning_ps2_port.txt`).

The AI doesn't use race flag 0x100 (countdown frame 300): it drives from green like everyone.

## The field's settings (Car_Init, Race_CreateCars)

- **Lane.** Race_CreateCars builds each car's Car_Init word. Bits 5-9 go to +0x194, and for a Quick Race's opponents
  they count up from 3: car 1 (slot 0) has 3, car 2 has 4, and so on. So the lanes cycle 3, 0, 1, 2 through the
  field. The player has 0.
- **The cap.** For a car with no flags 0x33 (not a player or teammate) and a parts record whose AI level (byte 0) is
  non-zero, Car_Init sets +0x246 = level + n − slot + 1. Here n counts D_0177F760 +0x674 and +0x676 (the player's
  two teammates, save.md: 0 in a Quick Race). The car in slot n gets no cap. A Quick Race's opponents all have the card's
  level (Peach Raceway: 0x36 = 54):
  - slot 0 runs free;
  - slot 1 is capped at 54 (135 km/h);
  - slot 22, at the back, at 33 (82 km/h).

  The field strings out from front to back, and the back of the grid never catches the front.

The Peach Raceway save states confirm all of this. Every opponent's +0x194, +0x246, +0x250, +0x258, zone, rank and
distance match the above (`race_peach_grid.p2s`, `race_peach_02_mid.p2s`).

## The race order

Race_RankCars' Obj_Before (hud.md) breaks ties by the zone's rank (+0x24B, higher is ahead), then by the distance
to the target gate (+0x24C, smaller is ahead).

## The port

- `tools/modelconv` writes `data/ai/Cnn.txt` per course, from the executable:
  - `line x0 z0 x1 z1 splitX splitZ` rows;
  - `zone in out prev prevAlt next nextAlt rank speed` rows, with the Quick Race's speed table folded in.

  Edit a speed or a gate to change how the opponents drive.
- The zone table editor (the Pause Items extra's Zone Table, debug.md, extras.md) saves the speeds it edits beside
  the path, without touching it: `data/ai/Cnn.speeds.txt` (the path's speeds; `AiPath::Load` reads it over them)
  or `data/ai/Gnn.speeds.txt` (Adventure race nn's own table; `Race::StartAdventure` reads it). One line a zone in
  the game's own `\t%d,\t/* zone %2d */` form, after a comment line; zones after the last line are 0. Delete the
  file for the original table.
- `data/ai/C16.txt` isn't the game's: the PC made it for the Test Course (C16, course 0x27; extras.md) with
  `scripts/course/make_c16.py`, from the road's shape. Only `--autodrive` (and the finish's AI) drive it.
- `game::AiPath` loads the path. It rejects any file whose indices are out of range, and then the opponents stay on
  the grid.
- `game::AiCar` holds a car's zone, rank, distance, wrong-way count, lane and cap.
- The reimplemented functions are Ai_Init, Ai_TrackZone, Ai_TargetAngles and Ai_Drive.
- Race::Update runs Ai_TrackZone for every car and Ai_Drive for the opponents before each Car_Update. It passes the
  lap zone as the steering gate and green as the on-the-spot turn's condition. Race_RankCars uses the rank and the
  distance.
- The marks are `AiCar::marks`, and a teammate the AI drives learns into its saved table (`AiCar::speeds`,
  `Ai_SpeedTable`, Ai_Drive's `learn`). The port's run of the same race moves the same zones by about as much: 37-46
  zones changed, −2.9 to −4.2 on average, against the PS2's −2.9 to −3.9.
- The wrong-way count (+0x248) drives WRONG WAY (hud.md). The events' paths and the Adventure teammates' tables
  are in.
- Mode 0's D_002A9B80[race +0x20] for the Adventure's cars not on a team is in (race/adventure_speeds.inc, from
  `scripts/elf/gen_adventure_speeds.py`): races 1, 6, 8, 10, 15, 18, 22 and 26 differ from their course's Quick
  Race table (race 1 much faster; 26, the president's race, 80/80/80 against 68/66/56 at the start).
- `RTAGlass.exe --race ID --cartrace FILE --tracecar N` traces opponent N (1-23, the game's record numbers),
  including its zone, rank and distance.

## Check

Starting from `race_peach_grid.p2s` with the player idle, a full lap of the port's car 1 (slot 0) against the PS2's:
- The first 2.7 s match exactly.
- After that, every zone is reached within 0.2 s and about 1 unit of the PS2's position, worst 2.9 at the start of
  lap 2. The PS2's times are PINE wall-clock times, and the cars brush their neighbours.

Car 23 (slot 22, held at its 82 km/h cap) stays within 0.64 units for the whole lap.

The leader keeps lapping on all eight Quick Race courses (90 s each) and never stalls.

## Not the race AI

The rest of `car/ai` belongs to other things. func_00250EB8 is called from u24E7D8. func_00250F00, func_00250FE0,
func_00251190 and func_00251440 belong to the golf minigame (func_00251B60).
