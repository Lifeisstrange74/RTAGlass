# Race cars: who races, where they start, how they are drawn

The port puts the Quick Race field on the grid exactly as the game does and draws the cars the way the game
draws them: the body lit and painted by VU1 program 0x20, the tires, and the shadow. The cars move with the game's
physics (car_physics.md); the AI drives the opponents (ai.md). Code: `src/game/race/car.cpp` (tables, Race_CreateCars, Cars),
`race.cpp` (grid and camera), `tools/modelconv` (the models). Reference: `captures/race_peach_grid.p2s`, a
Quick Race on Peach Raceway during the countdown, and `captures/race/peach_grid_*.png`.

## Who races (Quick Race)

QuickRace_Task (0x00211170), once a card is confirmed:
- `D_0177F760 +4` (the player's body) = `D_0029C028[card]`, and `+0` (the paint) = `D_0177F760[0xD70 + 4 body]`,
  which func_00229FD8 fills from `D_002ED720[body] + 4`. That is, **the player's car is fixed by the card**, in its
  body's own paint. Peach Raceway: body 26 in 0x888888 (grey).
- Race_Setup (0x0020F950) takes the course table row `D_002BE158[card]` (16 bytes: course id, cars 0x18, laps 3,
  flags 3, opponent list pointer, two task pointers). The opponent list is `cars - 1` (group, index) byte pairs,
  each pointing at a 16-byte record `D_002C22C8[group] + 16 index` = {paint, body, 0, name}. The pointers go to
  `D_0177EA50[3..]` (0 is the player's record, 1 and 2 are the story rivals).
- Race_CreateCars (0x0020ED30, with flags 3 and no rivals): Car_Init for the player with slot `cars - 1`, record 0,
  then for list entry i: record 3 + i in slot i, except that **entries 3 and 6, and 9 and 12, trade slots**. The
  Car_Init argument is `slot << 10 | record << 5 | car index | type << 16` (type 0x02 player, 0x80 computer).
  The rivals (`D_0177F760 +0x674/0x676`, slot 0 onward) are empty in a Quick Race: func_002277B0 clears the state.
- All eight courses share their first 17 opponents and the last (Klien); entries 17-21 are the course's own.

Checked against the save state: all 24 cars have the record, body, paint and slot the port computes, and the grid
positions (Car_Init, race.md) agree to the digit.

## Car files (CARn/Qnn.BIN)

Body n is `CAR{n / 30}/Qn.BIN`, 150 is `CARS/Q150.BIN` (the loader's table D_0029B438 has the sizes; all 151
match). Loader_LoadCar (0x0020C7B8) loads record r's body:

- u32 offsets {six sections, GS uploads, file end}. The part before the uploads is kept; `D_0177F310 + 0x18 r`
  points at the six sections. The uploads (one T8 128x128 at block 0, CLUT at 0x40, in every body) are moved to
  TBP `0x243E + 0x44 r`.
- A section: {u32 offset A, offset B, count A, count B}, then its main chain at +0x10 (cnt ... ret), then chains A
  and B. The main chain is program 0x20 strips (MSCAL 0x04): V4 header (x = count, y = GIF word 1 with PRIM, w =
  **paint**), then 4 qwords per vertex: position, normal, colour (0-255), (s, t, specular). A and B are program
  0x50 (MSCAL 0x0A) and are drawn by Car_DrawLights (func_00222360) only when the car's lights are on (car +0x1FC:
  headlights by time, +0x1FE: brake lights). Its flags: bit 0 draws chain A of section n (n = the body Car_Draw drew,
  0 near, 1 far; every body has A in both), bit 1 chains B of sections 0 and 2 (every body has B in section 0; three
  in section 2). It sets TEX0_2 to the car's texture and calls the chains; Cars_Draw (and the room scene) first set
  vf17 = (1, 1, 0.5, 1), vf18 = 0 (D_0029D2D0, func_00223490) and ALPHA_2 = 0x48 (D_0029D2F0: Cs x As + Cd,
  additive), after every car is drawn. modelconv writes the chains as `<section>_head.obj` and `<section>_brake.obj`;
  the port draws them in Cars::Car_DrawLights by RaceCar::lights (a driven car's headlights, its physics +0x1FC;
  the brake lights +0x1FE aren't set in races yet).
- **The headlights by the clock.** Car_Update sets every car's +0x1FC from pad +0x14 (a race's course clock, the
  field's time of day): on outside 0xE484-0x278D0 (6:30-18:00), and the lights button held (Car_Input bit 0x10)
  turns them the other way. The port's Car_SetClock gives Car_Update the clock.
- Sections: 0 the body near, 1 far (about half the triangles), 2 or 3 by part flag 0x1000 (2 is usually a stub),
  4 and 5 by flags 0x2000/0x4000 (the jet turbines; 0x4000's sticker panels). Sections 4 and 5 aren't in the
  car's texture: Car_Draw writes TEX0 as T8 at 0x1BC2 and then TEX2 as T4 with its CLUT at 0x1BE2 and CSA 10 or
  11: CARS/PARTS.BIN's T4 (at 0, CLUT at 0x20 with 16 palettes) loaded at 0x1BC2. modelconv writes them with
  `textures/CARS/PARTS.BIN/000` in palette 10 (the turbine's fan) or 11, and the port draws them by
  `RaceCar::partFlags`.
- No GS state packets in the chains: Car_Draw sets TEX0 itself (T8, TBW 2, 128x128, TCC, CBP + 0x40).
- The texture's bottom right quarter is the car's shadow.

CARS/TIRE.BIN has the same layout (its uploads go to 0x1C44: T4 128x128, CLUT 0x20 with 16 palettes). Sections:
0 front left, 1 front right, 2 the rear pair, 3 all four (far), 4 and 5 another set (part flag 0x400).
CARS/PARTS.BIN (func_0020AEF0 step 3): u32 offsets of 12 models in the car file's section layout, then at +0x30
the GS uploads (T4 128x128 with 16 palettes, and five T8 64x32 pictures), loaded at 0x1BC2. D_0177EAC8[n] points at
model n. modelconv writes them as `models/CARS/PARTS/<n>.obj`. WHEEL.BIN (16, D_0177AB30) is for close-ups:
`models/CARS/WHEEL/<n>.obj` (parts.md).

**The parts.** Car_Draw (asm 0x00221A90) loops over part flag bits 0-8. For a set bit n it draws model n in the
body's frame (car matrix times the body's, +0x40) moved to (0, y, z), with TEX2's CSA = n (the T4's palette n):
- (y, z) is the body's place for the part, D_002ED720 + 0x44 body: +0x08 for parts 0 and 1, +0x10 for 2 and 3,
  +0x18 for 4, +0x20 for 5 and 9, +0x28 for 6, +0x30 for 8 (`car_parts.inc`). Part 7 has none (jtbl_002EAEF0
  skips it), so it sits where the last part drawn did.
- Part 6 (0x40, the propellers) is model 6 (a bar across the back) and four of model 7 (a propeller) at x -0.63
  (D_00332CC8) 0.42 apart (D_00332CC4), each turned about the car's length (sceVu0RotMatrixZ, func_00272AE8) by
  minus the rear wheels' angle (+0x88). Far away (Car_Draw's far body) model 11 instead.
- Part 9 (0x200), the billboard, is model 9 at part 5's place (+0x20), drawn after parts 0-8, with TEX0 T8 64x32
  at TBP 0x1BE4 + 10 n, CLUT 0x1BEC + 10 n (n the record's +7, the sponsor): PARTS.BIN's picture 1 + n. The port
  draws it in Car_DrawParts; checked in its factory room for sponsors 0, 3 and 4 (coffee, wool, coconut).
- **Part 10 (0x400), the big tires**, replace the tires at every distance. Car_Draw first draws model 10 (the lift
  between body and tires: untextured, in paint 1) in the car's frame, not the body's. The reflection's draw
  (Car_DrawReflection, func_00221F70) leaves the lift out. Then it draws TIRE.BIN's section 4 (left) and 5 (right)
  at D_0029CB20: (-0.4, 0.55, 0.68), (0.4, 0.55, 0.68), (-0.4, 0.55, -0.66), (0.4, 0.55, -0.66). The front pair
  turns by +0x80 and +0x84, the rear pair by +0x88. Their TEX0 is TBP 0x1C44 itself (the tread and hub tile at
  (96, 96)) with CLUT +5.
- The rest of part flag 0x400:
  - The suspension's reach becomes 0xA3D7 (Car_Suspension).
  - The body sits 0.85 higher (+0x74: Car_Init, and Car_Update each frame, D_00332C18). Standing cars get the same 0.85: the rooms (Shop_SceneTask), the 2 Player cards (Versus_DrawCar), the garage (func_0022CD38) and the photos.
  - The wheels spin at speed / 0.55 instead of / 0.33 and / 0.38, at most 0.2 a frame instead of 0.8 (Car_SpinWheels).
  - The chase camera takes views 4-7 (race.md).
  - Car_Ground's water test is 1.35 under the level instead of 0.5. It reads D_0177EA50[0]'s flags, the player's, for every car.
  - Checked in PCSX2 against the port's room (`captures/adventure/tires_big_ps2_port.png`).
PCSX2 (Lagoon and Tin Raceway grids): the player's propellers, the opponents' propellers, turbines and section 3
match the port's (`captures/race/lagoon_propellers_ps2_port.png`, `tin_parts_ps2_port.png`).

## How a car is drawn

Cars_Draw (0x002228A8), called by Race_DrawView between the field and block 64:

1. Every car's **shadow**: D_00290570, a 2 x 3 quad (program 0x50) with ST in the car texture's bottom right
   quarter. The matrix is the car's with its y column cleared; program 0x50 takes each corner's height from vf18,
   which holds the ground heights under the car's four corners (car +0x140..+0x170: the corner probes, which
   Car_Ground fills with the ground found under them, or the corner's own height where none is found or the ground
   is a wall). MR32 turns vf18 once per vertex, so the quad's four vertices (front left, front right, rear left, rear
   right) take x, y, z, w. The shadow therefore stays on the ground when the car jumps, and a corner over a drop
   reaches down to the lower ground. GS state D_0029D290: TEST alpha >= 0x80 with RGB-only fail (no depth), alpha
   blend. vf17 = (1, 1, 1, 0.5).
2. Every car in view (depth / focal >= 0.003): Car_Draw (0x00221788).

Car_Draw for record r (`car +0x194`):
- Paint: `record +0` holds six 4-bit levels; `D_0029D150[n] = 0.1 + 0.05 n`. Nibbles 0-2 are paint 1 (r, g, b),
  3-5 paint 2. They go to M[24..25] = vf17/vf18.
- Lights: set `(car +0x19C >> 12) & 3` of D_0177F620, bits the ground's surface flags carry (every car in the Peach save state uses 2; race.md, "Lighting"), scaled by
  Car_LightsUnderWater (func_00226058) when the car is more than 2 below the water level at its front wheels (+0x11C;
  effects.md). The light matrix is D_0177F5E0's first three rows.
- Body: section 0 if depth / focal < 0.03 (15 units) and fewer than six near bodies were drawn, else section 1;
  then section 2 or 3, sections 4/5 and the parts by flags, the tires by distance (Car_DrawTires 0x002213E8 within
  depth / focal 0.05, TIRE.BIN 0-2 at D_0029CAF0 (-0.74, 0.33, 0.68), (0.74, 0.33, 0.68), (0, 0.38, -0.66), turned
  by the wheel angles at car +0x80; beyond that TIRE section 3). The hi-detail wheels (WHEEL.BIN, func_00221570)
  are only for distance 0. Tire texture TBP 0x1C44 + 2 `record[6]`, palette `record[5]` (the wheel colour,
  paint.md; every driver record's is 0). `record[6]` is the Wheels part w (0-14). PSMT4 keeps a page's 32 blocks
  of 32×16 in the order 0 2 8 10 / 1 3 9 11 / 4 6 12 14 / 5 7 13 15 / and the same + 16 below. The tires' UVs
  cover only the 32×32 corner (blocks 0 and 1), so starting 2w blocks on shows the hub tile at blocks 2w and 2w + 1.
  That puts Normal at (0, 0), Mesh at (32, 0), Spoke 1 at (0, 32) and so on, ending with Spoke 666's skull at (64, 96).
  The port re-addresses the picture the same way (car.cpp's TirePicture), with every block moved. PCSX2 shows the
  Mesh and the skull (`captures/adventure/tires_skull_mesh_ps2.png`, the port's `tires_mesh_skull_port.png`).
- GS state D_0029D240: TEST alpha >= 1, depth GEQUAL, alpha blend, clamp, bilinear.

### Program 0x20 (from the microcode at 0x1620)

```
paint = w == 0 ? (1, 1, 1) : w odd ? vf17 : vf18
n' = light matrix x (model rotation x normal); d = max(n', 0)
colour = min(colour x paint x (vf09 d.x + vf10 d.y + vf12) + vf11 x specular x d.z^8, 255)
alpha = min(fog value, vf12.w = 128); STQ = (s, t, 1) / w; fog as the field's (vf08 = (128, 255, 0.5, 800))
```

With the save state's values: lights (-0.7071, 0.7071, 0) colour (0.8, 0.8, 0.7), (0.4472, 0.4472, -0.7746)
colour 0.3, highlight direction (-0.9066, 0.422, 0) colour (0.65, 0.65, 0.6), ambient 0.5. The light matrix
is in world space (A, uploaded by func_00226B18, is the car's world matrix; the camera is in B).

## The port

- modelconv writes `models/CARn/Qnn/<section>.obj` + `shadow.obj` and `models/CARS/TIRE/<section>.obj`. New OBJ
  extensions: `vs s` (per-vertex specular) and the material line `paint N` (data/README.md). The shadow's ST are
  pulled in half a texel, or the opaque neighbouring quarter shows as a line along its edge.
- `gfx::DrawStyle` gained `transform` (3x4 model matrix), `lighting` (gfx::Lighting, program 0x20) and `alpha`.
- `game::Race_CreateCars` builds the field from tables copied from the executable; `game::Cars` loads each body
  once and draws shadows (each corner on the ground under the car's corner probe: `DrawStyle::cornerY`, program
  0x50's vf18; `captures/race/shadow_jump.png`, a jump on course 6), then bodies (section 0 always: no LOD on the PC), section 2/3 and the tires.
- `--race ID` on a course outside the Quick Race borrows Peach Raceway's field.

**Check:** the port's Peach grid overlaid on the PCSX2 frame at the same camera lines up with no ghosting
(`captures/race/peach_grid_overlay.png`, close-up `peach_grid_cars_compare.png`); paint and lighting match
closely. All eight grids: `quickrace_grids_cars.png`.

## Next

Parts 0x200 and 0x400, lights (chains A/B), the far LOD if ever needed. Done since: the parts (flags 0x1FF and the
jet turbines), per-course lighting
(each car takes the colour set its ground picks, the course's light directions and FOGCOL; race.md, "Lighting"),
the shadow on the ground under the corners, the AI (ai.md), the body's tilt and height on the
suspension, the tires rolling and steering (from the physics).
