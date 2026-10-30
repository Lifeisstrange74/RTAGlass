# The field: driving around the world

What the Adventure does between the buildings, from the decomp, the ELF and PCSX2, and how `src/game/field`
reimplements it. Functions are named in `docs/names.csv`. PCSX2 states: `captures/adventure/field_peach.p2s`
(Peach Town, just out of Q's Factory) and `field_peach_ps2.png`.

## The world

The world is 64 blocks, `FLD/<abc>.BIN`, each 1600 x 1600 units and in a race course's file layout: u32 offsets
{GS uploads, field (a model table of 65: 64 cells of 200 x 200, then block 64, drawn after the cars), PMP collision, map,
objects} (race.md). Positions in a block's models and collision are the block's own, 0-1600.

- **Block ids** (Field_BlockAt, 0x0021C8D0): from a car's fixed point (0x2000000 a block, 20971.52 a unit), row = z /
  0x2000000; id = (row & 6) x 8 + ((x / 0x1000000 - (row & 1)) & 0xE) + (row & 1). So there are eight rows of eight
  blocks, the odd rows half a block (800) along x, and every block touches six others. The ids wrap both ways (the
  physics keeps the fixed point to 0x0FFFFFFF), so the world wraps too.
- **A block's corner** (Field_BlockOrigin, 0x0021C6A0): x = column x 0x2000000 (+ 0x1000000 on an odd row), z = row x
  0x2000000. A car's float position (+0x90) is in its own block: x = (fixed x + (z & 0x2000000) / 2) & 0x1FFFFFF.
- **Files**: block id n is `FLD/` + digits n / 16, (n / 4) & 3 and n & 3 with its two bits swapped (Field_StreamBlocks'
  debug print `[%d%d%d]`; the loader's table D_0029BB50 lists the files in id order: the sector counts match). Ids 64-69
  in the table are other files.
- **Towns** (D_002BE438, 8 bytes, towns 0-21): name, +4 the town's block, +6 its places, +7 its cars on the roads. The
  places' people are the town's first driver records (D_002C22C8[town]), its cars the ones after. Peach Town is block 43
  (`FLD/223.BIN`), 28 places, 11 cars.

| Town | Block | Places | Cars |
|---|---|---|---|
| 0 My Garage | 11 | 1 | 0 |
| 1 Peach Town | 43 | 28 | 11 |
| 2 Fuji City | 23 | 29 | 11 |
| 3 Sandpolis | 7 | 33 | 9 |
| 4 Chestnut Canyon | 19 | 17 | 11 |
| 5 Mushroom Road | 36 | 11 | 0 |
| 6 White Mountain | 35 | 27 | 10 |
| 7 Papaya Island | 47 | 28 | 10 |
| 8 Cloud Hill | (64: course 0x22) | 14 | 11 |
| 9 My City | 11 | 23 | 6 |
| 10-21 | the stops on the way (Bridge, UFO, Ruins, LightHouse ...) | | |

Cloud Hill (town 8, and entry 0x16) has no world block: Field_Setup gives it course 0x22 (`ACTION/A16`, loader 0x350000)
and the race's ground query instead. It's a map of its own, in the clouds (section "Cloud Hill" below). Town-table rows
22-31 are the ending's ten scenes (func_0026CA48 runs Field_Setup(scene + 0x16) with fixed cameras); 0x16 is Cloud
Hill's. My City (9) shares block 11 with My Garage,
whose file also holds 0x34 more model tables and 0x1A more PMPs: the city's buildings, drawn and collided by
func_0023DE30's flags (Field_DrawBlocks, Field_Ground, Loader_LoadField's block-0xB branch).

## Loading and streaming

Field_Setup (0x00210158), on the factory's "Drive around town" (result 10):
- course (info+0x22) -1, or 0x22 for towns 8 and 0x16 (func_002080D8 picks the ground query: Field_Ground, or
  Collision_Ground for 0x22);
- the slots info+0x60..0x6C to -1;
- the loader's jobs: the town's cars (0x370400 | count, D_0177EA50 + 0x10 the records, or the teammate's in the save),
  `SYS/SORA.GSL` (0x340008: the sky's picture at TBP 0x3CFE), and the town's block (0x390000 | block) or course 0x22.

Loader_LoadField (0x0020BAC8), job 0x39 | id: the block's file into slot id & 3 (D_00707B80 + 0x3A8000 x slot; slot
3 may be 0x528000): its uploads, its model table (D_0177AB20[slot]), PMP (info+0x70 + 4 slot), map (D_0177AB70) and
misc objects (D_0177F580); info+0x60 + 4 slot = id.

Field_StreamBlocks (0x002201C0), from the view each frame, with the camera car's place in its block (local x, z):
x <= 1400 and z <= 600: the block below on the left (corner offset (-800, -1600)); z > 1000: above on the left
(-800, 1600); x > 200: likewise on the right ((800, -1600), (800, 1600)); x <= 600: the left one (-1600, 0); x > 1000:
the right one (1600, 0). A neighbour not in its slot (id & 3) is loaded there; each slot's place among the seven
(D_0029D080) goes to D_003335B8, the car's own block being 3. Four slots are enough because a block and the three
neighbours it can be near never share id & 3. The game expects the car's own block to be in already: a car teleported
two blocks away (PINE) drives on nothing.

**In the port** (`field_world.cpp`): the PC keeps every block that comes within 1400 of the car's position (its
nearest edge), read on loading threads of its own (one a processor bar the main thread's, up to three; the nearest
block first) and built a few milliseconds a frame (each picture, then the cells as one vertex buffer, block 64, the
collision data). A block goes only once the car is 2000 from it, so driving to and fro over an edge doesn't read it
again. The block the car is in is built at once if it isn't in yet, which only happens if the threads haven't kept
up. The field's loading screen starts the reads on its first frame and builds what's read during its 80 frames, so
the field starts with everything near loaded (`Field::Preload`, then `Start` waits for anything left). On the VM a block
takes 35-110 ms to read (OBJ meshes, their DDS pictures, the PMP; three at once), and the main thread about a
millisecond a picture and 1-3 for the cells' buffer, though the VM's graphics driver now and then stalls 5-20 ms on
one of them, whatever its size. The loading threads run below normal priority, so on a PC with fewer processors the
game's own thread goes first. Nothing further from the eye than the distance fade can reach (alpha 0 at a view depth of 800,
so about 1000 at the frame's corners) is drawn: each cell's bounds are checked, so a model that reaches out of its
cell still shows while any of it can.

**Draw calls** (from a `--perf` log on an XP test machine: the field was held up by the processor, about
7 ms a frame plus 0.015 ms a draw call, with 670-800 calls in Peach Town; the card waited). Three changes:
- A block's cells are merged when read (`gfx::Model::MergeSections`): the parts whose order doesn't matter grouped by
  picture across the 64 cells, then the translucent ones cell by cell. `DrawSections` draws each group from its first
  cell in view to its last as one call; the cells between that are out of view go to the card, which clips them
  (about 20% more vertices). The translucent parts now come after every cell's opaque ones instead of after their own
  cell's: in a far, faded spot of the Fuji road 227 pixels changed, by up to a third.
- The GS alpha test's second pass (the failing pixels, colour only) is left out for opaque parts wholly nearer than
  where the distance cue starts (view depth 544), where no pixel can fail (`Draw3DState::alphaPasses`).
- The objects (FieldObjects) are drawn on the CPU (`DrawStyle::cpu`), so all of a block's fronds or sails join as one
  call.
Same scripted drives: Peach Town 671 calls a frame on average before, 327 after; the palm road 598 at most before, 236
after. The frames are the same but for the far faded spot above.

The XP machine's log driving Island Bridge (15-22 ms a frame before Present at 450-500 calls, ~0.03 ms a
call): the worst spot is past the bridge's south end at the Fuji City gate, where the view is long. Culling had only
looked at the screen's edges and the near plane, so sections wholly past the distance cue's end (view depth 800) still
cost a call or two each while drawing nothing: their alpha is 0, so no pixel passes the GS alpha test, and the
failing pixels' colour-only pass blends to no change. `gfx::Model::Visible` now leaves them out (with the GS alpha
test and the cue on). Scripted southbound drive: 489 calls to 403 at the Fuji gate, 364 to 204 mid-bridge; frames
pixel-identical, races 0 and 13 too. Of the 403, about 150 are still second passes for sections in the fade band
(544-800); one wholly past 544 whose texels' alpha is at most 0x80 could skip its first pass (needs each picture's
greatest alpha). Reproduce with `--field 1 --place -2200 200 -500 32768 --press 150:x:3000`.

## The ground across blocks

Field_Ground (0x00207280), the field's D_00335710, takes a point in the car's block and its slot: a point past the
block's z edge moves half a block along x and into the next row's block (slot +-1), past an x edge into the block
beside (slot ^ 2); then that slot's PMP as Collision_Ground does (16 x 16 cells of 100). The port works the same from
the world's fixed point, for any point. The car's slot is the one it's in when the query is made: the physics moves the
car's fixed point before Car_Ground's probes, so a car crossing an edge asks the next block. (Using the block from before
the update stopped the car dead at the edge: its probes landed on the far side of its old block.)

## The start: Car_FieldStart, the drive-out and the first camera

Field_CreateTasks (0x00210430) sets info+0x28 to 0x404 (0x400: the doors are shut), Light_Setup(0, course), the view
(RaceView_Init with Field_DrawView), the map (FieldMap_Task 0x00242FE8, hud.md), Car_Init 0x20000 for the player,
Field_TownCarsTask, Cars_Collide, Meter_Draw, the coins (Field_CheckCoins) and Field_CheckDoors. Adventure_Task state 6 waits for the loads, then Fade_InTask.

- **Car_FieldStart** (0x002182B0), from Car_Init for slot 0 on the field: halfway along the door's way out (corners 2
  and 3 of D_002C0698[town] + 32 x place), y -20 then dropped onto the ground, yaw = atan2(dx, dz) x 0x8000 / pi -
  0x4000 (facing out across the door); the fixed point is the block's corner (D_002BE438 +4) plus the position. Town 7's
  place 0x1F (not a door) starts at (400, 200, 600). Peach Town's Q's Factory: (508.14, 434.41), yaw 0xF67D.
- **The drive-out**: while info+0x28 has 0x400, Car_Update gives the player's car flag 0x100 (and starts it over at the
  door once). With 0x100, Car_Input keeps only the pad's steering; for the car task's first 0x40 frames it accelerates
  while the wheels' speed (+0x1B8) is below 0x1000 and brakes above 0x1800; then it coasts. So the car rolls about 7.9
  units out of the door and stops. Checked in PCSX2 (a trace from the factory menu, no input): the port's
  velocities match frame for frame and the car stops at (0xBA20603, 0xA8D75F0) against the PS2's (0xBA2060B,
  0xA8D75E9).
- **Camera_FieldStartTask** (0x0021EC58), the camera's first task: for 0x40 frames it looks at the car from the front
  (swing -0x8000), then swings round 0x200 a frame; at 0x80 it clears 0x400 (the doors open, the car's 0x100 goes) and
  becomes Camera_ChaseTask.

## Cloud Hill

Field_Setup for town 8: the ground query Collision_Ground on the course's PMP (loader slot 2, info+0x78), info+0x22 =
0x22, info+0x54 = 0x40 (block 64, which the doors and coins go by: only town 8's doors, no coins) and the block corner
(0, 0): positions are the course's own, 0-1600, no wrapping. Field_CreateTasks: Light_Setup(0, 0x22), the radio,
Course_ObjectsTask's A16_GateTask (not the field's objects) and RaceView_Init's func_00220B20 (Cloud_DrawView): the sky
tinted by the background colour, the clouds ring always (by the night weight) and no band below the horizon, blocks
0-63, the cars, the gate, Field_DrawNight, block 64. No coins, field objects or streaming. FieldMap_Task draws the
empty frame, FieldMenu_Map no car mark. Field_TownCarsTask's state 3: all 11 cars at once, never gone.
- **Arriving:** Pollepolle (Papaya Island 17), "To Cloudhill then!": pre-code 0x14 (8, 7), the car at door 7's way out
  (832, 250, 600) facing north. Door 7 ("EXIT ONLY") and door 9 (Forest's, no quad) can't be driven into.
- **Leaving:** Adventure_Task's state 0 off the world (0x00210DE8): the car below y 200 (the roads are at 250-350 over a
  floor at 50) lands in Pollepolle's building (7, 0x11). Or Warp once its Q's Factory is found.
- **The gate** before the Secretary's mansion (place 8) opens with event flag 0x5A, which the Gate man's talk sets once
  the Grand Prix is won (flag 0x57); it holds the car's brake while it opens.
- The port: FieldWorld's course mode (Field_SetCourse, Course::Cloud_DrawView, the course's Collision),
  Field::TownBlock, the fall in Field::Update, CourseObjects for the gate, `models/ACTION/A16/clouds.obj` (A16's own
  ring picture at 0x3DFA: the loader runs its jobs in request order, so it is uploaded after SORA.GSL's and is the
  one seen; whether anything uploads SORA's again later wasn't traced).

## Doors

Field_CheckDoors (0x00259138), each frame unless 0x400: for each town whose block is the car's (info+0x54; town 0 only
once bit 0 of D_01780A3A, save +0x12DA, is clear: My Garage's scene clears it) and each of its places, the car inside the door's quad (D_002C0698: the four edges' cross
products all > 0) and no higher than 1 above the ground at the door's way out: info+0x23 = town, +0x21 = place and
the field ends with result 1 (Adventure_Task: that building's scene; driving out of it starts the field at that door).
My City's places count only once opened (func_0023DEB8); the UFO's place 0 only by night (clock outside
0xE483-0x278CF) and not while event flag 0x50 (Q's Rally) is on (`bnel` at 0x00259264: the flag *shuts* it); during
the rally (race flag 0x1000) only the destination's Q's Factory (Rally_AtDestination), the others printing "During the
rally, you can only enter" / "Q's Factory at the destination." at (130, 113) and (130, 125) half-lines, colour 6, each
frame the car sits in them. The car's start point is on the door's edge, so without the drive-out
a car that sat still would go back in (PCSX2 does, if the car is put back there).

## The view

Field_DrawView (0x00220938): Field_StreamBlocks, Sky_Draw (the race's sky with SORA.GSL's picture), Field_DrawBlocks
(each placed slot's cells in view, offset by its place, D_0029D080), the cars and objects, then Field_DrawBlocks64. The
lighting is the race's (Course_WorldStyle): Light_Setup with course -1 (the port takes course 0's lights: not checked),
and each frame Light_TimeOfDay from the clock (info+0x14, a frame at a time round the hour) and the car's height, so
under water (the sea east of Peach Town, block 45) the fog closes in green-blue as in a race. The frame shows the
background colour (pad +0x50) past the blocks.

## The time of day

The clock (info+0x14) counts frames round a day of 0x34BC0 (an hour of play; 9000 frames a game hour, 150 a
minute). New Game and Continue set 9:00 (0x13C68).
- **Who runs it.** Adventure_Task's state 0 (the field) and state 8 (a talk) tick it, and so does state 4 (a
  building's scene). The Radio screen ticks it too (Field_TickClock). A warp moves it on 1-4 hours. After an
  Adventure race, state 9 sets it to the course's end time (D_002A88EC, the second word of each D_002A88E8 row),
  or, where that's 0, to the clock as the race began plus an hour.
- **The sky.** Sky_Draw tints the sky by the background colour on course -1 (Light_SkyTint), so it darkens through
  dusk to black. Sky_DrawRings adds the ring of clouds and stars by the night weight, and the band below the
  horizon by the day's (Endurance Run's rings: the ELF's D_002962E0 and D_00298A50).
- **The meter's clock** (hud.md), the map's colour (FieldMap_DrawBlocks), the town cars that are out
  (Town_Absent), the radio's place in its stream, the race list's night race (0x19 becomes 0x1B outside
  6:00-18:00) and pre-code 0x18 all read it.
- **The UFO's door** (town 12, place 0) opens only by night, outside 0xE484-0x278CF, and not during Q's Rally (flag 0x50).
- **Headlights.** Car_Update turns every car's on outside 6:30-18:00 (cars.md).
- **The night** (Field_DrawNight, func_002207D0, after the objects and before block 64 and the cars' effects).
  Nothing happens from 6:00 to 18:00 (0xD2F0-0x278D0). Otherwise:
  1. With the player's headlights on, Car_DrawBeams (func_002224F8) draws a cone from each headlight,
     (+-0.5, 0.6, 1) in the body's frame, to a ring of 21 points (angles -pi to pi) of radius r around
     (+-(0.5 + s), y0) at z ahead. The Lights part (the parts record's +8) sizes them: 1 (s 6.5, z 29, r 34.9,
     y0 -31.8), 2 (4.7, 103, 22.9, -16.1), the rest (4, 50, 26, -22). The vertices are program 0x40's
     (D_00299E54, D_00299458). Both of their alphas are 0 (colour 2.x and STQ.z, which the code zeroes), and TEST_2
     0x7180B keeps them from writing depth. So a beam changes no colour; it leaves the frame's alpha at 0 wherever
     it is nearer than the scene.
  2. A full-screen sprite (D_0029D0F0) of (v, v, v) with ALPHA_2 0x52, (0 - Cs) x Ad + Cd, and TEST_2 0x7D80B,
     whose DATE passes only where the frame's alpha is 0x80 or more. It darkens the scene by v but where the beams
     are, which is what lights the road. v is 0x20, fading in over the 0x800 frames after 18:00 and out over those
     before 6:00.
- **Checked in PCSX2.** `field_peach.p2s` with info+0x14 (0x00335914) poked over PINE:
  - The weights (0, 0, 1.1), fog (300, 1, 0x000008) and background are the port's.
  - The town's vertex colours at night are about 0.75 of the day's, as the OBJs' colour sets give.
  - The night dump has the (32, 32, 32) sprite, and the beams show once Car_Update turns the lights on.
  - The frame blend (Frame_BlendPrevious) sets no RGBAQ, but its TEX0 is TFX DECAL, so the colour the last sprite
    left doesn't matter (gs_sprites prints it, e.g. a talk's text colour, 58 or 143 as it pulses).
  - With the talk ended (`field_peach.p2s`, the car teleported to the Paint Shop's door mid-talk, then Cross until
    it ends), the car's shadow is darkened like the road. Its TEST_2 (Cars_Draw's D_0029D290, 0x5380B) fails every
    pixel under 0x80 with AFAIL RGB_ONLY: it writes colour but leaves the frame's alpha alone. The world's TEST_2
    (0x537FB) does the same for its translucent faces.

In the port (`Field::Field_DrawNight`, `Car_DrawBeams`, `Draw2D::Darken`, `--clock N` for the start time):
- **The beams** draw alpha only (Draw3DState::alphaOnly).
- **The darkening.** D3D9 has no destination alpha test, so Darken samples the frame (the backend copies it) and
  alpha-tests the sampled alpha >= 0x80. HIGHLIGHT2 with a black vertex of alpha v gives the colour v / 128, and
  FIX 64 takes off v / 255. One copy and one draw, by night only.
- **The shadows** take the GS alpha test (`gsAlphaTest`, RGB only on failure) as the world does, so they keep the
  ground's alpha and are darkened with it.
- **Island Bridge by night.** The far carriageway drew black in patches, and the towers and cables
  faded into the dark well before the PS2's. Two things the world's programs do, now in the port (renderer.md, "Fog
  and the guard band"): the flagged strips fog by the distance cue's numbers, not the night's 300, and triangles are
  clipped to the guard band, so a big triangle's fog doesn't come from corners far off the screen. Checked against
  PCSX2 at 22:00 on the bridge (the car teleported over PINE in hops from `field_peach.p2s`, so the blocks stream in,
  and the clock poked at info+0x14): the PS2 shows the far carriageway grey and the towers lit, as the port now does.

## Objects

A block's file has one objects section (Loader_LoadField): u32 offsets of up to six models ("MISC OBJ COUNT OVER !"),
each a program 0x04 chain at +0x10 (lit, as the car bodies), then the offset of a GS upload sent at once, then 0. The
models go to D_0177F580 + 0x18 x slot + 4n, and the upload puts their picture where the file says (block 39's at
TBP 0x3CB8, CBP 0x3CF8). modelconv writes model n as `models/FLD/<abc>/objN.obj` with the upload's first picture (block
11's three each with their own: its upload has three, at TBP 0x3C30, 0x3C74 and 0x3CB8).

**My City's buildings** (block 11, `FLD/023.BIN`, which always loads into slot 3, the only one allowed more than
0x3A8000 bytes). Its header has 0x34 more model tables (words 4-0x37) and 0x1A more PMPs (words 0x38-0x51) between the
map and the objects (word 0x52); Loader_LoadField's block-0xB branch (0x0020BE78) puts the tables in D_0177AB80 and the
PMPs and their posts at info+0x80 and info+0xF8. Table 2k is My City's resident k's lot before they move in, 2k + 1
their building, each with one cell's model (in block coordinates) and maybe an entry 64. Field_DrawBlocks (0x0021FEEC)
draws table `2k + Resident_MovedIn(k)` for each visible cell after block 11's own, Field_DrawBlocks64 (0x0022013C) their
entries 64; Field_Ground (0x00207458) runs the query over the block's PMP and each moved-in k's in turn with one result,
and Car_HitPosts (0x00219FEC) ORs in their posts. Only save +0x670 decides: town 0 and town 9 see the same city. The
port: modelconv writes `models/FLD/023/city_II.obj`, `city_II_64.obj` and `collision/FLD/023/city_KK.pmp`;
FieldWorld loads them with the block (one merged model of 52 sections, drawn with DrawSections' `exact`, so a lot and
its building never draw together) and `Collision::Collision_GroundAll`/`Collision_PostsAll`. Resident 12's Big Tower
is high enough that Roberts's climb (pre-code 0x14 to My City's way out 29, the tower's top) leaves the car on its roof.

Field_DrawView draws them after the cars: Course_DrawObjects (the objects the block tasks place, with the cars' light
matrix and colour set 0, D_0029D240), the coins (func_00241D38), then **Field_DrawObjects** (0x0025D028). On the view
task's frame 1 that sets the palms going; after that it runs the handler of D_002C4AD0 (14 of block and function) for
each block that is loaded. Each handler sets its TEX0, builds a matrix (sceVu0RotMatrixX/Z) at a place in its block
moved by Field_SlotPlace (0x00221288, the slot's place from the car's block; nothing if the slot isn't placed) and
calls the model:

- **Windmill_DrawSails** (0x0025D5F8, block 39, two blocks west of Peach Town towards White Mountain): the 22 wind
  turbines' sails (D_002C4250, object 0, the hub at the place) turn about z by the view's frame count:
  (frame % 360 - 180) degrees for sails with i & 3 = 0, (frame % 1440 - 720) / 4 for 1, (frame % 720 - 360) / 2 for 2
  and 3. The towers are in the block's cells.
- **Palms_Draw40** (0x0025CC18, block 40) and **Palms_Draw42** (0x0025CE20, block 42), on the way to Fuji City: 107
  and 6 palms (D_002C43C0, D_002C4A70), whose trunks are in the cells and whose three fronds are objects 0-2 (TEX0
  0x2C38 and 0x3476, 64 x 32). Frond j turns about x, down a thirtieth of D_002C43B0[j] (8, 8, 3 degrees) a frame to
  -D_002C43B0[j] and back up to 0, the turn in the same frame (D_017608D0/D_01760DD8 the angles, D_01760E20/D_01760F68
  bit 0x80 the way back up), except while race flag 0x8000 is up. The start (Field_DrawObjects, frame 1): odd palms
  going back up, each at -(|(D_002C4B40[k & 31] << 12) + k| mod the count) / D_002C43B0[j], which can be past the end.
- The rest are things to pick up and Peach Town's big peach (Peach_Draw 0x0025BE98 with Field_ObjectsTask's
  Peach_FallTask 0x002406F8, below). A thing to pick up is drawn (object n of its block at its place)
  while the player has neither the item (kind 15) nor the stamp that ends its story, and the car's place in its own
  block within a radius of the thing's in its block gives it: SE 0x302, Item_Give, no window.
  - Jones's gems, until stamp 9: 0x20 block 5 (700, 90.48, 1000), 0x21 block 33 (540, -9.5, 980), 0x22 block 6
    (130, -37.11, 1530; the ruby in the coral by Sandpolis), 0x23 block 38 (1360, 3.5, 1260), 0x24 block 47 (765.77,
    40.63, 1207.7), 0x25 and 0x26 block 35 (1040, 162.28, 110 and 400, 354, 1240). Within 1.5, 2.0 for 0x25 and 2.5
    for 0x24. Blocks 33's and 38's are course objects 1 and 2 (D_0177EB50, D_0177EB90), which Course_DrawObjects
    draws as object 0 of the block in slot 1 or 2: a block is loaded into slot id & 3.
  - The stories' items: Benji's fountain pen 0x28 (block 5, stamp 39), the Papu Flower 0x2A (block 47, stamp 64),
    the Fluffy Mushroom 0x2B (block 36, stamp 80), the love letter 0x2C (block 47, stamp 84, within 2.5) and Tim's
    wallet 0x17 (Peach_Draw: block 0x2B's object 2 at (437.5, 30.2, 535) on the lawn outside Peach FM (place 6, whose door is at about (437, 509)), stamp 5).
  - Block 6's object 1 at (537.5, 46.12, 1499) turns about y a degree a frame outside 4:00-20:00.
  - My City's objects (MyCity_DrawObjects 0x0025C088), unturned: object 0, Ms. Flower's flower bed, with stamp 41
    at (1180.5, 26, 699.5); object 1, Q's Rally's GOAL banner, while event flag 0x50 is on, at (879.1, 30, 966.4);
    object 2, the gold statue, with stamp 50 at (1238.9, 27.65, 1035.9).
  - Fuji City's offering box in front of the shrine (func_0025D2C8: course object 3, block 23's object 0 at
    D_002A2BD0 (542.5, 48, z)), over Otomi's door (Fuji City place 10). Its block task (MoneyBox_Task 0x00240CD8,
    "MONEY_BOX_DETACH"/"ATTACH") keeps the cars out of the box's place (throwing them back: speed x 0.75, place back by
    twice its step), pushes the player's car out of the strip in front by day and puts it back in front from inside the
    box. Until stamp 17 (Otomi saved), by night (outside 0xD2F0-0x3056F) and not during Q's Rally, the player's car in
    front of it (543.5-545.75, 1013-1014.5) below y 50 is stopped and held (car +0x198 bit 0x800) while the box slides
    from z 1003 to 994, 0.02 a frame; then her door is open. The Guard's car stands there until 23:00 with flag 22.
    The task also writes car slot 1's position (+0xA0, D_0177AC50 + 0x310: the Guard, T02_00) to (0x7AE3D70,
    0xF5BF5, 0x74428F4) on its first frame or with stamp 17, and every frame by day, with stamp 17 or in Q's Rally
    while the player is in block 23, so he can't be pushed off his post. The port: FieldObjects::MoneyBox_Step.
  - Block 22's UFO (town 12): Ufo_Task (0x00240228, state D_00335738) and its draw func_0025D328. The model is block
    22's only object, all in part 1 (header {0x30, 0x50, 0, 0x57}: 87 batches of program 0x50, unlit, FLD/111's
    picture 35; drawn with vf17 = (1, 1, 1, 1), blended, so opaque but where its picture's alpha is under 0x80), which modelconv now writes as obj0. At 17:00 it comes down from y 100 over (900, 300) at 0.1 a
    frame, slides to z 229.32, circles (two 0-360 counters a quarter apart: a circle of about 71 in 720 frames)
    until 17:57, flies back and hangs over its door all night, and rises at 6:30; its first frame picks the state
    from the clock, and it starts over each time the block loads. Never turned, not drawn while away or during
    Q's Rally. Its door is field.cpp's night rule, not the task's. Checked in the port at 19:00 (over the door) and
    17:06 (circling); not against the PS2. The port: FieldObjects::Ufo_Step.
  The port: `FieldObjects` (field_objects.cpp, kPickups).

**The big peach** (Peach_FallTask, a block task of block 0x2B; object 0 of `FLD/223`, a peach with a leaf, texture 145):
- State 0: nothing drawn (the tree's peaches are scenery). The player's car in block 0x2B within 4.3 of the tree
  (559.9, 28, 1501.7: the big peach tree in the north of the town, which a Quick-Pic shop line mentions) at 10 km/h or
  more (Vec_SpeedKmh): the bump SE 0x14, the peach at (562.4, 44.04, 1496.4), state 1.
- State 1: it falls, 0.2 a frame and 0.005 faster each frame, drifting 0.15 a frame to -z; at the ground (the query at
  D_002A2BC0) less 0.5: SE 0x31C, state 2.
- State 2: it floats off down the water along +x at 0.2 a frame, bobbing 0-0.3 (a counter 0-60 and back, x 0.005), and
  zigzags across z (0.01 a frame, turning back where the ground under it isn't water, surfaces 0x100651/0x101651). The
  player's car within 1.5 catches it: SE 0x302 and item 15/0x2F, "Peach: It's very big. Looks delicious"
  (func_0023D2C0), state 0. At x 1920 (past the block's east edge, into the sea) state 4: it drifts on, sinks 0.01 a
  frame and blinks (drawn every other frame), and once down at the tree's ground height it's gone (state 0), so the
  tree can be bumped again. Nothing here checks whether the item is already held.
- State 5 prints "PEACH_DETACH" and ends the task.
The Peach item is James's (Peach Town 28): his talk takes it and gives stamp 8 ("Gave a big peach to James!").
Peach_Draw draws the peach (block 0x2B's object 0 at D_002A2BC0) in states 1-3, and in state 4 on odd frames.
The ground query's fourth word is the water's top: that is what the fall stops at and the float rides on.
The tree's trunk keeps the car about 4.4 from the tree's point on its west side; from the south the car gets within
4.3 (checked in the port with `--field 1 --place 548 30 1490 8192` and Cross held).

Field_ObjectsTask (0x002412E0) is Course_ObjectsTask's task on the field: it starts the block tasks of D_002A3920/30
(blocks 0x16, 0x2B and 0x17) as their blocks load, steps Lagoon_TideStep, and runs the 14 texture scrolls of
D_002A3800 (block, function, the race's GS copies: water and the like) for the loaded blocks: 34 copy rows over
blocks 43, 23, 19, 35, 47, 39, 36, 40, 22, 38, 42, 37, 41 and 45 (kFieldCopies), most by the task's frame count, the
beaches' surf (blocks 47, 40, 38, 42) by the tide, block 22's running lights and glow switched to their blank halves
during Q's Rally. Block 47's handler also moves the love letter: x = 451.14 + tide × 0.004. Every block that writes a
picture writes it alike, so the port keeps one copy per picture, run while any of those blocks is loaded
(FieldObjects::Field_Scrolls, with the race's copy
code in race/tex_anim); the pictures are shared with the races' (A17, A19, A11, C14, C05), so leaving the field puts
them back (Field::Leave). The ending's fields run them too.

**In the port** (`field_objects.cpp`): FieldWorld loads each block's objN.obj with its cells, and FieldObjects is
Field_DrawObjects with the sails and the palms, in Field_DrawView's cars step; an object further from the eye than the
distance fade can reach is left out. The angle signs follow libvu0's RotMatrixZ/X (the asm); the PS2's sails haven't
been compared on screen for their turning way. `--place X Y Z YAW` starts the field's car anywhere (in its start
block's units, past its edges too) and `--eye X Y Z YAW PITCH` looks from a fixed point (in the car's block's units):
`--field 1 --place -2300 400 700 0 --eye 900 230 250 0 0x800` shows the turbines, `--field 1 --place -1500 100 -600 0
--eye 700 30 1080 0x2000 0x400` the palms.

## Coins

The field has 100 ChoroQ coins. D_002A3970 holds 16 bytes for each: {f32 x, y, z, s32 block}. The position is in that world block's units, and the block is a FieldWorld id.
- **Field_CheckCoins** (0x00241BA8) is a task of Field_CreateTasks. Each frame it checks every coin not taken yet (Pickup_Taken).
  - A coin counts only if it is in the car's block (+0x54) and within 1.4 (D_00332EEC) of the car's position.
  - Taking a coin plays SE 0x29 and clears bit i of save +0x548. Pickups_Left counts the set bits.
  - The coin that leaves 70 sends mail 0x7E (Mail_Send).
  - The task also runs the field's 64-frame fade overlay.
- **Coins_Draw** (0x00241D38) runs in Field_DrawView after the cars. It draws each coin not yet taken in a loaded block, placed with Field_SlotPlace.
  - The model is SYS/COIN.BIN, which Loader_BootFiles (0x0020B348) loads at boot to D_003FA780. D_00335718 points at its section. It is a disc 0.5 across standing from y 0 to 1.
  - The texture is TEX0 0x2003AA0599405D48: a T4 64 x 64 picture at 0x1D48, CLUT 0x1D50.
  - Every coin turns about the vertical by RotMatrixY(((view frame & 0xFF) - 0x80) x pi / 128), so all of them make one turn every 256 frames.
  - The lighting is the objects' (Course_DrawObjects').
- **Coins_MapDots** (0x00241E90) draws the coins on the field map. FieldMap_Task calls it when the save has the Coin Radar (Save_HasItem(15, 0x2D)) and the map task's frame & 0x20 is set, so the dots blink 32 frames on, 32 off. Each coin not yet taken in a loaded block gets a yellow square (D_002A43A0, FieldMap_DrawSquare 0x00243338).
- **The scripts.** Pre-code 0x0D (IF_PICKUP_TAKEN i, Pickup_Taken(i), 1-based) and 0x1A (at least c coins found: 100 - Pickups_Left() >= c) read the same bits. The bars use them for their hints, and Coine in My City (9, 13) uses them for her rewards.

**In the port** (`field_coins.cpp`):
- The table is `field_coins.inc`, from the ELF.
- modelconv writes the model to `models/SYS/COIN/0.obj`, along with the Settings pad (`--pad` makes both).
- The coins are drawn on the CPU path, so all of them go in one draw call.
- The Items screen's coin count reads Pickups_Left.
- `--coinradar` (with `--factory` or `--field N`) gives the save the Coin Radar.

Checked:
- `--field 1 --place 430 40 1020 0 --eye 446.3 33 1040 0 0x200` shows coin 78 behind Kinsera's house, where the bar's hint puts it.
- `--place 446.3 35 1045.6 0` drives over it, and it is taken.

## Bars

Seven towns have a bar. Each is a building whose bartender tells where the coins not yet found in or around that town are:

| Town | Place | Speaker |
|---|---|---|
| Peach Town | 4 | Bartender |
| Fuji City | 4 | Barkeeper (a juice bar) |
| Sandpolis | 4 | Bartender |
| Chestnut Canyon | 1 | Bartender |
| Mushroom Road | 2 | Bartender (a juice bar) |
| White Mountain | 4 | Bartender |
| Papaya Island | 3 | Bartender |

How a bar's script runs:
1. #1: IF_PLACE_NOT_FOUND (pre-code 4, Place_Hidden on save +0xBD8) goes to the longer first-visit talk.
2. Both lead to "So, do you want a hint?" (YES_NO).
3. Yes walks the hints in order. Each hint's IF_PICKUP_TAKEN skips it if that coin is already taken. The first coin still out is told, and the bar ends.
4. With none left: "I guess there are no more coins around here."

The scene is Shop_SceneTask's ordinary place:
- The room picture is the place's 640 x 384 picture in SHOP/Tnn.BIN.
- The car drives in on path 6 and stands (car state 0xE/0xF).
- The person is the place's driver record.
- The song is 0x0A0B0102.
- At the end (results 4 and 5) it calls Place_MarkFound, so the second visit gets the short talk.

**In the port:**
- `RoomScene::Room_Open` lets the field's doors into Q's Factory and the bars.
- Driving out of a bar starts the field at its door.
- Checked with `--field 1 --place 831.35 35 415 0`, Peach Town's bar door: the first-visit talk, then with Cross presses "behind Kinsera's house".

## Town cars

**Field_TownCarsTask** (0x0020E708) keeps one town's cars out at a time. Its states (+0x25):
- **First frame:** My Garage's town 0 counts as My City (9). The town's cars are made at once, because Field_Setup's loader job already holds their bodies.
- **4:** wait for the town's block.
- **2:** the cars are out. When the block isn't loaded any more, all of them go ("kill block %d") and the task goes to 0.
- **0:** look for the first town (1-21) with cars whose block is loaded ("new block %d"). For a town other than the last:
  - fill D_0177EA50[4+i] with the records;
  - start loader job 0x370400 | count, which loads the bodies into buffers 4 and up at TBP 0x243E + 0x44 x idx;
  - go to 1.
- **1:** when the loader is done, run Car_Init for each car and go back to 2.
- **3:** Cloud Hill. All its cars come out at once and never go (no case handles state 3).

So the cars are back at their starts every time their block comes in.

Which cars, and how they start:
- **The cars:** D_002BE438 +7 gives the town's count. Car i's driver record is D_002C22C8[town] + 16 x (places + i): {paint, body, u16 part flags at +8, name}. A car that is the player's teammate (Team_Find) is the save's own car instead.
- **Who is out** (Town_Absent, 0x0023CD58):
  - A My City resident (D_002A2B70) who has moved in (+0x670) has left their town.
  - My City's own six cars are out only once their residents have moved in.
  - Fuji City's Guard leaves after 23:00 with flag 22.
  - Shirley leaves with flag 63.
  - Benji (Ruins) is out only at night and not after flag 85.
  - A few people are out only at some hours.
- **Car_Init** (arg town << 10 | (i + 4) << 5 | (i + 1) | 0x800000):
  - It runs in car slot i + 1, with flags 0x80 (AI) and +0x247 = the town.
  - The parts record is D_0029CBA0: tires 7 and steering 2, the rest the first.
  - The start is D_002C2E78[town][i], {x, y, z, u16 yaw}, in the town's block. There is no drop to the ground.
  - The speed cap is 0x14 (50 km/h), or 0x28 with part 0x100 (the water ski).
  - Steering is on from the start.
  - Town cars have no engine sound.
  - The headlights are on outside the clock's 0xE484-0x278D0.
- **Ai_Init and Ai_TownPath** (0x0025FBC0): the car's path is D_002D98A8[town][i] {gates, zones}, with no zone speeds (D_002ABF80).
  - It uses the race path format (ai.md) in block units, with as many zones as gates.
  - Every path is a closed loop with no forks.
  - Some cars share one: Peach Town's 1 and 7, and 2 and 8.
  - Some cars have none and stand: Fuji's Guard, and Cloud Hill's Yumyum, Williams and Gate man.
- **Ai_Drive** gives nothing while the car's bump timer +0x210 runs. The cars don't avoid or approach the player.

**The talk** (Cars_BumpSound's field branch, 0x00217830):
1. **The bump.** When the player's car bumps town car j and that car's +0x210 is 0:
   - info+0x23 = its town and info+0x21 = places + j - 1 (its record);
   - the Adventure task gets result 2;
   - Car_Stop runs on both cars.
2. **The timer.** Every bump sets +0x210 to 0xB4, so the car stands 3 seconds and a bump within that time doesn't talk again.
3. **State 0** reads the result only once the doors are open (flag 0x400 down). It goes to state 8: Message_Task on D_002A7ED0[town][place], with info+0x28 |= 0x50000 and the cars quiet.
   - 0x40000 stops Car_Update and Cars_Collide.
   - 0x10000 hides the map and the meter.
   - The chase camera keeps running. No camera of the talk's own was found, and PCSX2 agrees (`field_peach.p2s`,
     mid-way through Barthou's talk): the plain chase view behind the stopped car, unchanged over 8 seconds.
4. **State 8.** The clock runs.
   - Result 11 (code 0x14, e.g. Kevin's "take me home") goes to that place's scene.
   - Any other result goes to func_0026D008, which shows the stamps a talk queued, then state 10. State 10 clears the flags and goes back to state 0.
5. No town car's talk starts a race.

**FieldMap_Task** draws a green square (D_002A4390, alpha 0.5) for every town car out, wherever it is.

Example: Barthou is Peach Town car 7, place 35, table 0x2A5698. He drives Gonzo's loop round the town's west end. His talk is the teammate pitch, then the Island Bridge tip.

**In the port** (`field_town_cars.cpp`):
- **Data.** modelconv writes each town car's start, record, part flags and loop to `data/ai/FLD/Tnn_cc.txt` (`modelconv GAME DATA --ai` makes just the paths). Zone links past a loop's end, a slip in the data (Papaya Island's car 1), become the loop's own links.
- **Models.** `Cars::Cars_Request` reads the bodies on a thread, and `Cars_Stream` puts them on the card about 3 ms a frame. A town's cars come out once that is done.
  - The first town's bodies are read during the field's loading screen.
  - The PS2's first-frame cars and its state 4 are one state here: the bodies come first.
- **Driving.** The AI gets the car's position in the town block's units, wherever the car is.
- **Collisions.** Cars_Collide takes the player's car and the town cars.
- **The talk.** `Field::Update` runs state 8 with the story's MessageTask over the field.

Message codes added for the talks:
- Pre-codes 3 (has item), 4 (place not found), 5 (photo), 7 (room on the team), 8 (teammate / not beaten), 0x0D, 0x11 (take an item), 0x16, 0x17 and 0x19 (My City's residents, Resident_MoveIn's mail), 0x18 (night, from the field's clock), 0x1A, 0x1B and 0x1D; 0x0E and 0x13 (Q's Rally) and 0x14 (leave to another place's way out). Only 0x12 (an early Field_Setup) is skipped.
- The talk windows (messages.md, "The other end codes' windows"), among them the teammate menu, Get, and the stamp book that follows a talk (state 10).
- Checked: `--field 1 --beaten --allparts --place 350 31 514 32768` bumps Barthou with a full team. "Who will you trade?" lists Flower, Casa and Quit, then comes "Do you want to remove Flower from your team?", then "Barthou is now in your team.". The next bump gives his teammate line.

Checked:
- `--field 1 --eye 840 40 370 0x2000 0x300` shows Kevin and the next car driving their loops.
- `--place 848 32 392 16384` bumps Kevin: "Mommy? Huh? Who are...", with the map and meter hidden.
- Kevin's "take me home" goes to Kevin's mom's house (1, 8), which opens as any building does.
- About 30 draw calls more in Peach Town (356 a frame against 327).

Every building opens (`RoomScene::Room_Open`: every place with a talk, and each town's Q's Factory); My City's by MyCity_PlaceOpen. `--room T P` starts in building P of town T (`--room 0 0` My Garage; `--mycity HEX` sets save +0x670).

## Not in yet

- Under water the PS2 shows teal where the port shows the day's white past the blocks (the clear colour there isn't
  traced).
