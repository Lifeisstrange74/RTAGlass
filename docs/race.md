# Race: the course and the camera

Course select → course information → Cross: QuickRace_Task (0x00211170) loads the race. The PC version
draws the course as the race does, from the player's place on the grid, with the race camera, and the field on the
grid (cars.md) for the Quick Race, the Adventure, 2 Player and the events, runs the countdown, races the opponents by the game's AI (ai.md), counts laps at the
line, shows the HUD (hud.md) and finishes with FINISH!, the trackside cameras and the replay (finish.md). The
course objects and animated textures are in ("Course objects" below). The code is in `src/game/race`: `course.cpp` holds the world, `race.cpp` the
grid and camera. Reference: `captures/race_peach_01.p2s` / `.png` / `race_peach_01.gs`, a Quick Race on Peach
Raceway with the player stopped on the grid. `captures/race/` holds the port's frame, an overlay on the PS2
frame, and all eight Quick Race grids.

## Course files (COURSE/Cnn.BIN)

The loader is Loader_LoadCourse (0x0020C188), from func_0020D120 case 0x50000. Course id `pad+0x22` indexes the file
table D_0029BE50: ids 0-14 are C00-C14, ids 15-17 are C18-C20. C16 is on the disc but in no table; the PC races it
as course 0x27 (Course::kTestCourse, the Test Course extra), with a grid, line, path, map, cameras, light and song of its
own or borrowed (extras.md, "Test Course").

Header: u32 offsets {0x20, field, PMP, map, objects} and the file size.
- `[0x20, field)`: the GS texture uploads (a GSL chain, sent at once). texconv writes them to
  `textures/COURSE/Cnn.BIN`. TBPs are absolute.
- `[field, end)` is read to D_00707B80 + slot × 0x3A8000 (the race uses slot 2). It is relocated in place:
  - **field**: 65 u32 offsets, the same model table as SYS/S00.E3D. The pointer goes to D_0177AB20[slot].
  - **PMP** (race +0x78): the collision data, a 16x16 grid of strips (car_physics.md). modelconv writes it to
    `data/collision/Cnn.pmp`.
  - **map** (D_0177AB70 + slot): for ids < 0x12 and some others: the HUD's course map (RaceMap_Draw, hud.md).
    modelconv writes it to `data/models/COURSE/Cnn/map.obj`.
  - **objects**: header words 4 on, up to the first zero, each bound one section ("Course objects" below).

### The field

Entries 0-63 are the blocks of an 8x8 grid of 200-unit cells: block n covers x 200 (n & 7) .. +200,
z 200 (n / 8) .. +200, y up. Empty cells are 0x20-byte stubs. Entry 64 is drawn after the cars. It holds what is
seen through, such as fences, banners and trees, and spans the whole course. Positions are world coordinates.
Each entry is a DMA chain (cnt ... ret) of VU1 batches:

| MSCAL | program | input | used for |
|---|---|---|---|
| 0x08 | 0x40 | header (V4 x1 to top+0), V3 x5N to top+1: position, colour 1-3, STQ | everything else |
| 0x06 | 0x30 | the same header, V3 x(1 + 5N) to top+1: an anchor, then the vertices relative to it | billboards: the program turns them about the vertical to face the camera (block 64's trees) |

Per batch an A+D packet to top+0x192 sets TEX0_2/CLAMP_2/TEX1_2. PRIM is 0x27C (textured) or 0x26C, a fogged,
blended tristrip. The three colours are the day, dusk and night sets ("Lighting" below). The day alpha (colour
2's x) is 1 almost everywhere; water and glass have 0.4-0.75, and some faces have 0. Those show only at night, by
the night alpha (STQ's z): Endurance Run's (course 0x11) overpass lamps, their light beams and glows (C20 block 64,
textures 018, 040, 067), and some fittings on Peach Raceway, Lagoon and others. modelconv writes the alpha as a
seventh `v` value when it isn't 1. Dropping it shows the beams as solid beige wedges.

Size: 15-40 thousand vertices per course (C00: 23 646 vertices, 14 410 triangles, 71 billboards).

## Race_DrawView (0x00220D40)

This draws one view of a race (`arg0+0x28` = the camera's matrix block, `arg0+0x1C` = the split-screen mode). For
the world:

1. **Sky_Draw** (0x00225608). The sky is a model in the executable, D_00290A40 (program 0x60: header as two
   qwords, then position, colour and STQ per vertex). It has a flat ceiling at y 900 from -800 to 800 (ST 0,0)
   and a ring of radius 800 from y 10 to 935, with s going once round and t from 0 at the top to 0.8 at
   the horizon. Its translation is the camera's eye with y = 0, so it follows the camera across the ground but
   not up and down.
   - GS state D_0029D4A0: TEST_2 0x3001A (no alpha test, depth ALWAYS), TEX1_2 linear, CLAMP_2 0x17C00000009
     (s clamped, t region-clamped to rows 0-95), TEX0_2 0x2007BEC5E5323CFE: T8 512x128 at TBP 0x3CFE / CBP 0x3DF6.
     That is the course file's sky picture, 512x96.
   - Untouched by fog. Opaque (PRIM 0x21C has no ABE).
   - Program 0x60 multiplies the vertex colour by vf31.xyz and the alpha by vf31.w (Sky_Draw: Light_SkyTint, 0.9).
     Light_SkyTint (func_002262D8) is the background colour (light state +0x174) / 255 on courses -1, 0x11 and
     0x22, white elsewhere: Endurance Run's sky darkens through dusk to black at night.
   - Course 0x11 also gets Sky_DrawRings (func_002256A8): a ring of clouds and stars D_002962E0 (SYS/SORA.GSL's
     1024x128 T4 at TBP 0x3DFA, radius 800, y 195-935) with alpha the night weight, and a band below the horizon,
     D_00298A50 (untextured, colour 0/127/127, y -10 to -100) with alpha the day weight (not on course 0x22). Both
     blend (PRIM ABE). modelconv writes them as C20's `clouds.obj` and `horizon.obj`.
2. Camera, lighting (Vu1Packet_UploadLighting D_0177F720 → vf09-11), FOGCOL (func_0021DB48 into D_0172FF50, from
   D_0177F720+0x30), and the blocks' GS state D_0029CFC0 (TEST_2 0x537FB).
3. Blocks 0-63 whose cell passes Camera_CellVisible (0x0021FA38), a test of the cell's centre against the frustum
   lines.
4. Course objects and cars (func_00222F08 for courses 0x19/0x1A, Cars_Draw 0x002228A8, Course_DrawObjects
   0x002232E8). Both are in the port (cars.md; "Course objects" below).
5. Camera again, GS state D_0029CFF0 (TEST_2 0x737FB: depth GREATER), block 64 (`D_0177AB20[2] + 0x100`).
6. Cars_DrawEffects (0x00225120): each car's tire smoke, dust, snow and water effects, then the particles
   (effects.md). The boost flames for the first three cars (func_002238A0) are the port's Car_DrawJet.

Other views use their own variants: func_00220938, func_00220B20 (always with the cloud ring) and func_00220FE8.

### Lighting, fog and the distance fade

The light state is D_0177F5E0 (0x180 bytes); D_0177F720 is its +0x140:

| Offset | What |
|---|---|
| +0x00 | the cars' light matrix: columns light 1, light 2, highlight; row 3 (128, 255, 0.5, 800) is program 0x20's vf08 |
| +0x40 (D_0177F620) | four car colour sets of 4 rows (light 1, light 2, highlight, ambient; w 128). A car uses set (car +0x19C >> 12) & 3, bits its ground's surface flags carry |
| +0x140 | vf09: the weights of the field's three vertex colour sets (day, dusk, night) |
| +0x150 | vf10: the field's fog (128, 255, scale, end) |
| +0x160 | vf11: the alpha fade (128, 255, 0.5, 800), never changed |
| +0x170 | FOGCOL (RGBA, red low). The cars and shadows fog towards it too |
| +0x174 | the background colour (pad +0x50); Endurance Run's sky is tinted by it |

**Program 0x40's colour** (0x30 does the same). The vertex has colours 1-3 and STQ. As the code takes the lanes
(mr32 puts colour 2.x in colour 1's w; one madda adds colour 1's x and w again for the dusk weight):
- day = colour 1, alpha colour 2.x;
- dusk = (colour 1.x, colour 2.y, colour 2.z), alpha colour 2.x;
- night = colour 3, alpha STQ.z (the program then puts 1 in STQ.z: the course vertices have no Q).

colour = min(sum of weight x set, 255) and alpha = min(sum, 1). Then:
- alpha x= clamp((800 - w) x 0.5, 0, 128) / 128 (vf11): the world fades out from 544 to 800 units away.
- F = clamp((end - w) x scale, 0, 255) (vf10, or vf11 with header flag 2).
- The GS then tests alpha >= 0x7F. Pixels that fail are still drawn and blended, but write no depth (AFAIL = RGB).

So the faces with colour 2.x = 0 (Endurance Run's lamp beams and glows, some fittings) show only at night, by their
STQ.z. modelconv used to divide the UVs by STQ.z as if it were Q, which stretched the textures of translucent faces
(water, glass) on some courses.

**Who fills it.**
- Race_Setup: **Light_Setup** (func_00225790, arg 0 and the course id):
  - The light matrix comes from D_0029E200's two directions, each turned round and normalized (func_00272E08). Most courses have (1, -1, 0) and (-0.5, -0.5, 0.866).
  - Sets 0 and 1 start from D_0029D500 entries 0 and 3. Sets 2 and 3 are the entries D_0029EC90 names for the course (Peach: 19, 19).
  - Pad +0x14 = the course's clock, D_002A88E8 (first word of 8-byte rows).
- Race_SetupTask, every frame:
  - Endurance Run (0x11): the clock goes up a frame at a time and wraps at 0x34BC0. Then **Light_TimeOfDay**(clock, the player's car's height).
  - Every other course: **Light_TimeOfDay**(clock, 0), then **Light_CourseFog** (func_00226248). That replaces the background, FOGCOL, fog scale and fog end with the course's row of D_0029EA10.
- Race_DrawView: **Light_SetHighlight** (func_002261A0) with the camera's forward axis (camera +0x180). The highlight is normalize(light 1 - forward).

**Light_TimeOfDay** (func_00225988). The clock counts frames of a 1-hour day (0x34BC0 frames). Weights are (day, dusk, night):

| clock | weights | fog above water (end, scale, colour) |
|---|---|---|
| < 0xC15C or > 0x28A63: night | (0, 0, 1.1) | 300, 1, 0x000008 (whatever the height) |
| 0xC15C-0xE483: dawn, d = (t - 0xC15C) / 9000 | (d, 0, 1.1 - d) | max(300, 500 d + 300 - y), 0.5 night + 0.5, (255 d, 255 d, d < 0.5 ? 511 d : 255) |
| 0xE484-0x255A7: day | (1.1, 0, 0) | 800 - y, 0.5, white |
| 0x255A8-0x278CF: dusk, k = (t - 0x255A8) / 9000 | (1.1 - k, k, 0) | 800 - y, 0.5, (255, 255 - 94 k, 255 - 128 k) |
| 0x278D0-0x28A63: evening, n = (t - 0x278D0) / 4500 + 0.1 | (0, 1.1 - n, n) | max(300, 500 (1.1 - n) + 300 - y), 0.5 n + 0.5, (255, 161, 127) x (1.1 - n) |

- **Height.** y is the player's car's height, at most 300. At y <= -2.5 (under water) the fog is 300 and 1, and green-blue: day 0x7F7F00, dusk (0, 127 - 47 k, 127 - 64 k), and so on.
- **Background.** It follows the fog's colour: day white, night black.
- **Car sets.** Sets 0 and 1 are the weights times D_0029D500 entries 0-2 and 3-5 (each set's day, dusk and night).
- **Constants.** D_00332DA8.. are 1.1, 9000, 1.1, 1.1, 9000, 1.1, 4500, 0.1, 1.1.

**By course** (ids 0-17):
- **Clocks:** day 0x1A5E0 on most courses. Sunset 0x278D0 on 2 (Temple) and 10. Night on 1, 5 (Night Glow), 8, 11, 12 and 16. Endurance Run (0x11) starts at 0x23280: dusk begins 9000 frames (2.5 minutes) in and night at 22500, so a 3-lap Quick Race ends early in the dusk, and the Adventure's longer race goes on into the night.
- **Fog rows (D_0029EA10):** white at 0.5/800 on the day courses. Temple and 10: 0x285AD2. Night: black. Course 1: 0x404040 at 0.75/500. Course 4: 0xA5CBE9 at 1/400.

**Check** (the state read over PINE). On the PS2:
- Peach: (1.1, 0, 0), white, sets 2/3 = entry 19.
- Temple: (0, 1, 0.1), 0x285AD2, set 0 (0.63, 0.41, 0.23), sets 6 and 7.
- Night Glow: (0, 0, 1.1), black, sets 7 and 8.
- Endurance Run with pad +0x14 set to 175000: (0, 0, 1.1), fog 1/300, 0x080000, sets 24 and 25.

- Endurance Run's tunnel: its surfaces carry light sets 1 and 2 by turns (flags 0x11000 between the lamps,
  0x12000 under them), so a car's lighting switches set every 20-25 frames at racing speed. Logged on the PS2 over
  PINE while driving through, the same runs of 0x11000 and 0x12000. Car_Draw takes the set as it is, with no easing,
  and on the PS2 the car changes as abruptly. `endurance_tunnel_compare.png` shows both at the same two spots (the
  port with `--place`, the PS2 with the car moved there over PINE). Two opponents bumped the parked PS2 car and turned it a little.

All four are what the port computes. The frames are in `captures/race/lighting_compare.png` (PS2 left). Endurance Run at midday, dusk, sunset and night in the port is in `endurance_times.png`.

## The camera

The race camera block in the save state (D_0177E6D0, uploaded by Vu1Packet_UploadCamera) has focal length 500
(+0x18C) and the screen centre at (2048, 2048). The projection is the course select's: x = 320 + 500 x/z,
y = 224 − 0.47 × 500 y/z on the field (0.94 on the 448-line frame). That is a horizontal field of view of 65°.
The frustum table D_0029CF40 has depths 1.5 and 700.

The player was stopped on the grid at (457.2, 1.1, 568.8), facing +x. Solving the view matrix (+0x40, its w column)
puts the eye at (450.4, 3.69, 568.8), looking along (0.9963, −0.0858, 0). The chase camera at rest is therefore
6.8 behind the car, 2.6 above the ground under it, and pitched 4.9° down: view 0 below, (0, 2, −7) turned 0x380 down.

### Camera_ChaseTask (0x0021E690)

The race's camera task, one per player, works on the camera D_0177E9F0 (0x20 bytes: eye in the car's frame, focal
+0x10, pitch +0x14, swing +0x16, turn +0x1A, view +0x1C). Race_Setup creates it before the cars' tasks, so each
frame it frames the car where the last frame left it.

- **Views** (D_0029CE00, 0x20 bytes each; Camera_SetView 0x0021FAE8 copies one):

  | | eye | focal | pitch | |
  |---|---|---|---|---|
  | 0 | (0, 2, −7) | 500 | 0x380 | the chase view |
  | 1 | (0, 1.5, −0.25) | 500 | 0 | the driver's view (+0x1C = 1) |
  | 2, 3 | (0, 1.2, −7), as 1 | 461 | 0x600 | split screen |
  | 4-7 | (0, 2.5, −7) ... | | 0x580 ... | part flag 0x400 |
  | 8, 9 | (0, 2, −9.3), (0, 1.2, −10.8) | 500, 461 | 0xD80, 0x880 | course 0x1A |

  The view is the camera button's choice plus the task's +0x1C & 2 (2 Player), | 4 with part flag 0x400. On course
  0x1A (Soccer) the chase view is 8 + (+0x1C & 2) / 2 instead, and the button's other view is 1 even in 2 Player.
  The camera button switches 0 and 1 (not with race flags 0x48008). It is the 8th entry of the button setup
  D_01780A48 (+0xE): 0x10, Triangle. Switching also starts the tilt follower from the car's tilt as it is.
- Each frame, unless paused: the swing goes back towards 0 by 0x80; in the chase view the pitch moves to the view's
  by at most 0x80 (+0x500 under water with part flag 0x100). The view it moves to is func_0021FB20's: the ordinary
  chase view (0, or 2 split), but 8 on course 0x1A in mode 0 (the Adventure). So 2 Player's soccer camera starts at
  view 9's pitch, 0x880, and settles to 0x600 (PCSX2: D_0177E9F0 +0x14 reads 0x600 with view 9's eye). The turn is −slide (+0x1D6) in the chase view, 0 in the
  driver's: the chase camera looks where the car goes, not where it points.
- **Camera_FollowTilt** (func_0021DC18): the car's up vector (+0x10) in its heading's frame. Its forward tilt (the
  slope) is followed by D_0172FF30: each frame a step towards it that grows by 0.001 while it keeps going the same
  way, never past it. The chase view replaces the side tilt with the body's lean on its springs, −(+0x50); the
  driver's view follows the side tilt too and adds the lean (and +0x58 forward). That up vector (y = 1 in the chase
  view) back in the world, Matrix_FromUp, turned by the car's yaw, is the camera's frame, at the car's position.
- Camera_BuildMatrices (Camera_RotationFromAngles): in that frame, Ry(swing + turn) Rx(pitch); the eye is the view's
  place turned the same way.
- **Camera_KeepOutOfWalls** (func_0021E070), chase view only, not paused: the near plane's bottom corners (D_0029CF40's
  first two points, (±0.96, −0.715, 1.5) from the eye). While the left one has no ground under it or ground above it
  (a wall), the swing goes −0x80 and the view is rebuilt; the right one +0x80; at most half a turn.
- Race flag 0x400 hands over to func_0021EC58 (the Adventure's free driving), car flag 0x200 (finished) to
  Camera_FinishTask.

**Check (PCSX2, the camera traced over PINE, Peach Raceway driven through a slide and along the walls):** the
follower's value, replayed from the logged car with the step above, matches the PS2's on every logged frame (the
misses are frames the log skipped). The turn is −slide, the same or one frame late (the camera runs before the car).
The swing jumps by 0x80 steps at walls and wears off. Triangle switches to view 1 (pitch 0) and back; the player's
car isn't seen in it, since it is all closer than the near plane at 1.5.

## The grid (Car_Init 0x00218458)

D_002A7F28 holds 16 bytes per course id: {x, y, z, s16 dir, s16 side}. Car slot k (`car+0x247`; the player is last
in a Quick Race, k = 23) stands at:

| dir (heading dir × 0x4000) | x | z |
|---|---|---|
| 0 (+z) | x ± 7.5 (k & 1) | z − 5k |
| 1 (+x) | x − 5k | z ± 7.5 (k & 1) |
| 2 (−z) | x ∓ 7.5 (k & 1) | z + 5k |
| 3 (−x) | x + 5k | z ± 7.5 (k & 1) |

The signs flip when `side` is set. Peach Raceway: slot 23 is (572.2 − 115, 561.3 + 7.5), which matches the save
state. The rows (5k) are only for courses below 0x12; on the event courses every slot is on the first row
(events.md). Then D_00335710 drops the car onto the ground. The table's y is the ground on most courses, but Oval
Raceway's grid is 30 below its track and Lagoon Raceway's 100 below, so the port lands the car on the surface
nearest that height. The car's position is on the ground (Peach: y 1.1 is the track's height there).

## The port

- `tools/modelconv` writes `data/models/COURSE/Cnn/<block>.obj` (00-64) and `sky.obj`, the executable's sky with
  that course's texture. TEX0's power-of-two size is rescaled to the image's real size (512x96 of 512x128).
  Billboards are stored with an OBJ extension, `billboard x y z` (data/README.md). Without that line every
  billboard stands still facing -z (the field's trees show it most).
- `gfx::Model` draws with a `DrawStyle`: colour weight, distance cue (alpha fade and fog), fog colour, the GS alpha
  test and depth, and an offset (the sky's). Culling is by bounding box. Billboards turn to face the camera's
  heading.
- `game::Course::Race_DrawView`: the sky (no depth; Endurance Run's tinted, with its two rings), then blocks 0-64
  with the race style. `game::RaceLight` (race_light.cpp) is the light state: Light_Setup when the race starts,
  `Race::Race_UpdateLight` each frame (Race_SetupTask's part), Light_SetHighlight each view. The field draws with
  `DrawStyle::weights` over the OBJ's three colour sets, the cars with their surface's set and the course's FOGCOL.
  `--clock N` starts a race at another time of day (the debug byte's L1/L2 clock scrub on course 0x11), and
  `--place X Y Z YAW` starts the player's car anywhere (to match a PCSX2 view).
- `game::ChaseCamera` (chase_camera.cpp) is Camera_ChaseTask for the player, before the cars' update each frame;
  in the driver's view `Cars::Cars_Draw` leaves the player's car out.
- `game::Race`: the field on its grid slots, the chase camera, and the game's car physics (car_physics.md): the
  player's car drives from the pad on the course's collision data, the others by the AI along the course's path
  (ai.md).
  Start pauses the race: Continue, Retire (back to the course select on the same card) and the Adventure's
  Ranking (pause.md).
- The countdown and the course's song (hud.md, "The countdown").
- `RTAGlass.exe --race ID` starts there directly. `--press N:K:M` holds a button for M frames.

**Check:** the port's Peach frame overlaid on PCSX2's lines up with no visible ghosting
(`captures/race/peach_overlay.png`). All eight Quick Race courses render from their grids
(`quickrace_grids.png`). A frame takes about 2 ms.

## Course objects

Three things make a course move besides the cars: the objects its course task turns, the textures the task
animates, and the posts the cars hit (car_physics.md, "Posts"). The objects and textures are only on Temple, Night
Glow and Lagoon among the Quick Race courses.

**The sections.** Loader_LoadCourse reads the course file's header words from 4 on (5 on for courses without a
map), up to the first zero; each pair of neighbours bounds a section. The count goes to pad +0x26. A section is
`{u32 model, u32 textures, u32 next}`. The model pointer goes to D_0177F580 + 0x18 n (up to 6 per section,
"MISC OBJ COUNT OVER !"; every course file has one each). The textures are a GS upload chain sent at once, with
each BITBLTBUF's DBP moved by 0x2B26 + 0x44 n. Every section's upload is at 0 / CBP 0x40 in the file.
- **C02** (Temple): a water wheel, 212 strips, 6.5 wide and 8 across.
- **C05** (Night Glow): three sections. The first is a fairground ride, a post with a ringed planet on top and
  four rocket cars on arms. The second and third are the two rings of the carousel's horses and panels, 35 and 26
  across.
- **C20** (Endurance Run): one quad. No task places it, so it never shows.
- **A01** (the Sliding Door Race, course 0x13): two halves of a sliding door (shoji), 5 wide each. **A11** (the
  Obstacle Course, 0x1D): the fence across the track behind the start.
- **C03 and C13**: objects too, but neither is a Quick Race course. C03 (Ninja Temple Raceway; not Temple Raceway,
  which is C02)'s are A01's two door halves (objects 0, 1)
  and the last door's two brown slatted halves (objects 2, 3, door 5's matrices 22 and 23). Those two upload a
  128 x 128 T4 with its CLUT at 0x20, not 0x40, and func_002423B8's TEX0s for them say so (T4 128 x 128, CBP TBP +
  0x20); modelconv takes either.

The model is a program 0x04 chain, as the car bodies are (lit, with normals), at the pointer + 0x10. modelconv
writes section n as `models/COURSE/Cnn/objN.obj`, with the n-th texture at 0 / 0x40 in the course's manifest.

**Course_ObjectsTask** (func_00241490) is a task Race_Setup (via Race_CreateViewTasks, func_0020EBE8) creates. On its first frame it runs:
- **Course_ClearObjects** (func_00223410): D_0177EB10 (32 matrices) zeroed, and D_0177EA30[k] = the TEX0 of
  object k (T8 128x128, TBP 0x2B26 + 0x44 k, CBP 0x2B66 + 0x44 k, CLD 1);
- then a switch on course + 1 (jtbl_002EC990) that gives the task its function: course 2
  **Temple_ObjectsTask** (func_0023E140), 5 **NightGlow_ObjectsTask** (func_0023E508), 14 **Lagoon_WaterTask**
  (func_0023ECA0), 0x13 an empty task (func_00241BA0) with seven **SlidingDoor_DoorTask** children (func_00241F68,
  doors 6-12), 0x1D **Obstacle_ObjectsTask** (func_0023EE70). The Adventure's courses and events: 3
  **C03_ScrollTask** (func_00241688) with doors 0-5, 4 **C04_ScrollTask** (func_00241760), 7 an empty task
  (func_00241838), 10 **C10_ScrollTask** (func_0023E7E0), 12 **C12_ScrollTask** (func_0023E8D0), 13
  **C13_ObjectsTask** (func_0023E9C0), 0x14 **Highway_ScrollTask** (func_00241840), 0x1F **A13_ScrollTask**
  (func_0023EFB0), 0x22 **A16_GateTask** (func_0023F0A0), 0x25 **A19_WaterTask** (func_0023F398). 3 and 0x13 also
  run func_002423B8, which sets the four objects' TEX0s to T4 256x128 (as their pictures are). Course -1 (the
  Adventure's field) gets func_002412E0, which starts and stops tasks by the field's keys: in the port, FieldObjects'
  scrolls and tide (field.md). The other Quick Race courses get none.

The task counts its frames (+8). It runs while the race is paused: in PCSX2 the water wheel turns on under race
flag 0x8000. Race_Setup creates a new one for every race (Race_CreateViewTasks, 0x0020EBE8), so a race on the same course again starts
over: frame 0, nothing placed, and Lagoon's tide out. The port loads the models and pictures once per course
(`CourseObjects::Load`) and runs `Course_ClearObjects` for every race. That also puts the animated textures back to
the course file's pictures. Whether the PS2 uploads the course's textures again for a rerun isn't checked; the
task's first frame copies over them either way. Checked in the port with a scripted rerun on Temple and Lagoon:
after 461 frames the second race starts at frame 0 with the tide at 0 (it had reached 46) and all copies restored.

**The events' objects:**
- **SlidingDoor_DoorTask** (func_00241F68), one per door at D_002A3FB0[n] (doors 6-12 at x 295-661, 61 apart, y 5,
  z 100). The nearest car in x (any car in the race, within 20 in z) gives t = (distance - 5) / 15, 0 to 1. Matrix
  4 n - 24 (object 0) sits at z - 5 - 5 (1 - t), matrix 4 n - 23 (object 1) at z + 5 + 5 (1 - t): closed when no
  car is within 20, wide open within 5. The task's +0x26 is for its sounds only: a player's car (car 0; in 2 Player
  car 1 too) at 61 km/h or more (Vec_SpeedKmh) within 20 in z sets 1 within 20 in x (ACTION 7, 0x307) and 2 within
  5 (ACTION 8, 0x308), and 0 again past 20; not with race flags 0x48. The doors don't collide: they're only seen.
- **Obstacle_ObjectsTask** (func_0023EE70): three copies scroll (D_002A2F30: 0x30CD to 0x309E, 0x30C4 to 0x30BF,
  0x3117 to 0x310E), and in 2 Player (pad +0x0A & 2) matrix 0 is the identity: the fence (A11 object 0) at x
  240-241.5, z 95.5-111.5, where Race_ObstacleTask turns the cars back before the first gate (events.md).
- PCSX2 (`race_sliding.p2s`, Cross held): the doors part ahead of the car and slide past either side; the port's
  2 Player run shows the same.

**The Adventure's objects:**
- **C13_ObjectsTask**: three cloths hung on lines over the pier (C13's objects 0-2, flat, hanging 9-11 down) swing.
  Each angle (D_017526F0, degrees) starts at 180 on frame 1 and moves D_002A3770 {0.4, 0.5, 0.6} a frame, up while
  frame % D_002A3760[k] {60, 40, 30} is under D_002A3760[k + 3] {30, 20, 15} and down for the rest. Matrix k is
  turned about x by ((s16) angle % 360 - 180) degrees at (1035.5, 118.18, 767.21) (D_002A3780 + 12 k), matrix k + 4
  turned a quarter about y (D_00332E6C, -pi / 2) and then about z by the same angle at (590.59, 118.18, 820.44)
  (+0x24): the same cloths on a second line. Then a copy (D_002A2DF0, the sea's surf) follows the tide, as
  Lagoon's does (Lagoon_TideStep on the task's +2, reset on frame 1). PCSX2 (C13 by the course cheat below, the car
  put under the line, `race_c13_cloths.p2s`): the cloths hang and swing as the port's
  (`captures/race/c13_cloths_ps2_port.png`).
- **A16_GateTask**: a gate of two halves (A16's objects 0 and 1) at D_002A37E0 (1323.8, 300, 855.3) and
  (1323.8, 300, 839.7). With event flag 0x5A (func_0023D778: bit n of D_0177FC98, the save record's +0x538) on
  frame 1 the task's +2 starts at 360, open; without it at 0. While the flag is clear, each of the first 12 cars
  whose x (+0x90) is 1322 (D_00332E70) or more gets rpm 0 (+0x1D0), its velocity x 0.75 (truncated) and its
  position (+0xA0) moved back twice this frame's move ((v << 5) / 25 each); past 1322.2 (D_00332E74) x goes back
  0x3E80 more. With the flag, +2 counts up to 360 while car 0 gets flag 0x800 (held braking; cleared otherwise).
  Matrix 0 is turned about y by ((0x2D0 - n) % 1440 - 0x2D0) pi / 720, matrix 1 by ((n + 0x2D0) % 1440 - 0x2D0) pi
  / 720: closed at 0, a quarter turn each way at 360. Not checked in PCSX2: the Quick Race course cheat doesn't
  start course 0x22 (the course select stalls).
- **The scrolls** (C03, C04, C10, C12, the Highway Race, A13) copy by the frame, as Night Glow's do; A19_WaterTask's
  first copy by the frame, its second by the tide. C03 runs door tasks 0-5 as well (D_002A3FB0: x 409.5 down to 260,
  30 apart, y 10, z 95; matrices 4 n, door 5 22).
- **Shared pictures.** texconv stores an image once, so a course that uploads the same picture to several DBPs has
  them share a DDS. C12's two lava destinations (0x30CD, 0x30EE, scrolled at -16 and -4 from the same source) and
  its three spark destinations (0x3150, 0x3159, 0x3162, each from its own 64x512 strip of 16 frames), and A13's
  three, are such pictures. modelconv gives each upload of a shared picture its own copy, `u<DBP>.dds` in the
  course's texture folder (16 files over A08, A13, C02, C03, C05 and C12), and the course's models use it; the
  copies write to it when it exists.
- **The course cheat.** In PCSX2 the Quick Race's first card starts whatever course id is at 0x002BE158 (D_002BE158
  row 0's id), with Peach Raceway's field: write it on the course select, then pick the card. C12 and C13 start
  this way; 0x22 doesn't. C12 (`c12_lava_ps2_port.png`): the PS2's lava under the bridge is brighter and yellower
  than the port's in every frame sampled; the copies scroll in both, so this looks like the course's lighting
  (Adventure courses' light setups haven't been compared yet).

**Course_DrawObjects** (func_002232E8), from Race_DrawView after Cars_Draw:
- each D_0177EB10[i] whose [3][3] is not 0 is drawn as model i & 3, with TEX0 D_0177EA30[i & 3];
- the lighting is the cars' light matrix (D_0177F5E0) and colour set 0 (+0x40), with the GS state D_0029D240.

**The objects.** The tasks build the matrices from sceVu0UnitMatrix and a rotation, then the position:
- Temple: matrix 0 is turned about x by (frame % 360 - 180) degrees, at (399, 70.8, 23.8). That is the wheel by
  the mill, a degree a frame.
- Night Glow:
  - Matrix 0, the ride: turned about y by ((frame & 0xFF) - 0x80) pi / 128, at (560, 15.77, 910).
  - Matrices 1 and 2, the carousel's rings: turned about y by (frame % 0x160 - 0xB0) pi / 176 and
    (frame % 0x180 - 0xC0) pi / 192, at (588.39, y, 143.71).
  - The rings' y bobs on D_002A36E8, 30 heights from 7.5 to 8.9 and back, one step every 4 frames. Ring 1 is
    15 steps ahead.

**The textures.** Each task fills A+D packets (D_002A3490, 3 qwords per copy at D_002A34B0 + 0x40 i) for the GS's
local-to-local copy and sends them (func_00207D80, func_00207B50). Each copy has a 0x20-byte row:

| Offset | Field |
|---|---|
| +0x00 | SBP |
| +0x04 | DBP |
| +0x08 | the buffer width (both) |
| +0x0C | the PSM (both) |
| +0x10, +0x14 | TRXREG (w, h) |
| +0x18, +0x1A | x and y speed (s16) |
| +0x1C, +0x1E | x and y mask |

TRXPOS is the source corner ((frame x speed) / 16 & mask), and the copy lands at the destination's (0, 0).
- **Sources and destinations.** The sources are pictures twice as tall as their destinations, whose top and bottom
  halves join, so a moving window scrolls them.
- **Temple** (D_002A2BF0, 3 rows): the pool's caustics, 024.dds.
  - Two of the copies flip through four 64x64 frames, one every 8 frames.
  - The frames are stacked in the 64x256 026.dds. The copy reads the 256-wide buffer, so x 128 is the
    picture's second page, 128 rows down.
  - The step tables are D_002A36D8 (x: 0, 0, 32, 32) and D_002A36E0 (y: 0, 16, 0, 16), each times speed 64 / 16.
  - The third copy scrolls the water by the wheel (027.dds) 4 rows a frame.
- **Night Glow** (D_002A2C90, 3 rows): three scrolls.
- **Lagoon** (D_002A2E10, 8 rows): the sea, the waterfalls and streams.
  - Copy 5 is the surf on the beach (ACTION/A19.BIN/005.dds from 006.dds). It follows the tide (task +2) instead
    of the frame.
  - **Lagoon_TideStep** (func_0023E018) moves the tide from 0 to 512 and back. It goes faster in the middle,
    by the edges D_002A36B0 {0, 64, 384, 480, 512, 496, 400, 208, 80, 32, 0} and the speeds D_002A36C8
    {4, 8, 4, 1, 1, 4, 8, 6, 2, 1}. D_00335708 is the direction.
  - The surf slides 0-32 rows, in for about 2 seconds and out for about 2.

**The port.** `game::CourseObjects` (course_objects.cpp) does all of this:
- **Load** is Course_ClearObjects. It loads the objN.obj models, and reads the course's texture manifest to find
  each copy's source and destination DDS by their DBPs.
- **Update** runs the course's task every frame, the pause included.
- **Course_DrawObjects** draws the placed objects in Race_DrawView's `cars` step: lit like a car body, with
  `Light_CarSet(light, 0)` and the cars' distance cue.
- **The copies.** Copy takes the window from the source's pixels, with the page remap for a buffer wider than the
  picture, and writes it over the destination's pixels. `Draw2D::WriteFile` then replaces the texture that the
  field's models use (`Renderer::WriteTexture`; Draw2D's file cache now normalizes paths, so the models' "..\..\.."
  paths and the manifest's meet).
- **Colours.** The pictures are decoded RGBA, not indices. Where a source's palette differs a little from its
  destination's (Temple's 024/026 and Night Glow's 085/086, a mean difference of 1-2 out of 255), the copied
  frames take the source's colours.

`--eye X Y Z YAW PITCH` watches the race from a fixed point, to look at scenery.

**Check:**
- **The water wheel.** PCSX2 and the port from the same spot (the port's `--cartrace` state, teleported):
  `captures/race/temple_wheel_compare.png`. The wheel is there and turning on both, and the PS2's matrix read
  over PINE turns a degree a frame.
- **Night Glow.** The ride and the carousel, frames 30 and 60: `nightglow_objects.png`.
- **The textures.** The caustics at frames 100 and 108, Lagoon's surf at 100 and 132, and the waterfalls at 100
  and 132: `course_texanim.png`.

## Next

Nothing is left on the Quick Race list. The last item was the engine sound on the grid. It's type 2's wave 0x29, keyed
by Car_Init's own Car_Update before SNDMOD's next vblank (effects.md, sound.md "Engines").

2 Player is its own mode (its own course rows, split screen, Save&Exit): see two_player.md. The pad's vibration is in (effects.md). A
rerun on the same course resets the course objects ("Course objects"). The
replay's camera switching needs no work in a Quick Race (finish.md). The course records and the save data (save.md), the chase camera, the lighting per course, the time of day and course 0x11's sky are in. The HUD, the lap counting,
the countdown, the course's song (hud.md), the opponents' AI (ai.md), the finish and the replay (finish.md) are
in.
