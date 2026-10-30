# Car physics

How the game moves a car, and the port's reimplementation (`src/game/race/car_physics.cpp`,
`collision.cpp`). The port follows the assembly function by function in the game's own integer arithmetic. Driven
from the grid with the same inputs, it reproduces a PCSX2 trace frame for frame (see "Check").

## Units and state

- Positions are fixed point: 2^21 / 100 = **20971.52 per world unit** (car +0xA0..+0xA8), wrapping at 0x1FFFFFF
  (1600 units, 16 collision cells). x takes bit 0x2000000 of z as half a wrap (the town's 2x2 regions).
- Velocity (+0xF0) is in the same units; a frame moves the car by **v x 16 / 25**.
- Angles are 16 bits (0x8000 = pi). Yaw +0x1D4: forward = (sin, 0, cos).
- The orientation is a float matrix at +0x00 whose rows are the car's axes (right, up, forward) in the world; +0xB0
  is its inverse. Fixed-point vectors are turned by them through 1/32768 floats (Fixed_Rotate 0x0021D2D8).
- Suspension heights are 1/32768 of a unit.

The fields the physics uses are named in `CarBody` (car_physics.h) with their offsets.

## The parts (Car_SetParts 0x002180C0)

The parts record (car +0x180, 16 bytes) is {AI level, tires, engine, weight, gearbox, steering, brakes, ..., sound
(+0xD), flags (+0xE)}. (In the player's save the record is the car's `+0x0C`, and +0xE is the meter; see save.md.) Each byte indexes a table in the executable:

| part | table | gives |
|---|---|---|
| tires | D_002ECA70 (0x1C each), +0xC | grip by surface type 0-5 (+0x21C); +0x1A adds 5 to the mass |
| engine | D_002ECED0 (0x18) | +0xC torque (+0x214), +0x10 fuel use (+0x240) |
| weight | D_002ED204 (0x10) | mass (+0x218): 25, 22, 20, 18, 15 |
| gearbox | D_002ED334 (0x1C) | 8 ratios (+0x22C), reverse first: -95, 116, 162, ... |
| steering | D_002ED4F4 (0x10) | +0x242: 64, 96, 128, 160 |
| brakes | D_002ED5DC (0x2C) | 32 force steps by frames held (+0x200 points at it) |

Quick Race: the player's record is `D_002BE318[card]`, the opponents' the course table row's (+4, +4). Player-2
cars (flags 0x30) get a sixteenth more grip and a quarter less mass.

## Input (Car_Input 0x0021A990, Car_ReadPad 0x0021A820)

Bits: 1 accelerate (Cross), 2 brake (Square), 4 reverse (Circle gives 5), 8 R2, 0x10 R1 held (lights), 0x20 / 0x40
L1 / L2 pressed, 0x8000 / 0x2000 steer left / right (the d-pad, or the stick's sign in analog mode). The key setup is
D_01780A48 (per player, 0x3448 apart). Other sources: replays (race flags 0x40 / 0x20) and the AI
(D_003356DC = Ai_Drive, ai.md). Car flag 0x100 is an autodrive (keeps the wheel speed between 0x1000 and 0x1800),
0x800 brakes.

## One frame (Car_Update 0x0021BA70)

1. `accel` = velocity - last frame's, in the car's frame. Gravity: v.y -= 0x59. Remember v.
2. `local` = v in the car's frame; local.w = gravity in the car's frame.
3. **Drag:** q = local.z / 0x600 (0x300 wading, 0x180 under water), local.z -= sign q x q^2 / mass; sideways
   s = local.x / 1024: local.x -= sign s x s^2 / mass; the springs' speeds alike.
4. **Car_Drive** (0x0021A310):
   - gear = Car_AutoGear (0x0021CE48, the race's D_003356D8): Circle -> reverse; first from reverse on Cross;
     up when speed x 128 / ratio >= 0x2009 (8201) unless braking; down from third up when < 5000 and the lower gear
     gives < 8000; braking shifts down under 8000.
   - **Steering:** the angle (+0x1CE) steps by 1 a frame to +-0x20 while held, back by 4 a frame when released.
     turnSpeed (+0x1D8) is the lowest speed since the steering started (reset when released or reversed). Radius
     = turnSpeed x 0x28BE / (steering x angle / 32), at least 0x18000; turn (+0x1CC) = 0x1B4C14B6 / radius.
   - **Brake:** force = brakes[frames held - 1] x 10000 / 32.
   - grip = 0x59 x tires[surface type] / 256 when a rear wheel bears weight (else 0).
   - **Car_Engine** (0x00218EE0): ratio = gears[gear]. Engine force = torque x 128 / ratio below the rev limit
     (9000 accelerating, 3000 out of fuel; fuel 0x40000, minus fuel use a frame), -256000 / ratio above it (engine
     braking, also off the throttle). The tires couple engine and car: slip = local.z - rpm x ratio / 128, force =
     slip x mass. The brake holds the engine back. The sideways force is -local.x x mass. If the two together exceed
     grip x mass (a friction circle), the car slides: forward force capped at grip x mass, sideways at twice it, and
     the excess is the squeal (skid sounds). New rpm (+0x1D0, 0-10000) and local velocity. Before the start (race
     flag 4) the car is held still.
   - The driven wheels (+0x1B8) follow local.z while loaded (vertical accel x grip), slowed by the brake.
   - **Yaw** += wheel x turn / 43909 (when the front or rear-left wheel is loaded).
   - v = the new local velocity turned to the world (its vertical part is dropped: height is the suspension's).
   - **Car_Slide** (0x0021A088): when the turn's lateral force exceeds the grip, the slide rate (+0x1D2) builds to
     a third of the excess (8 a frame, at most 0x200) and adds to the yaw and the slide angle (+0x1D6); it settles
     to an eighth off the brake; the angle wears off by 0x1EC000 / wheel speed a frame (at most 0x400). The chase
     camera reads the slide angle.
5. **Move:** fixed += v x 16 / 25 (x and z wrap).
6. **Car_Ground** (0x0021B3D0): 7 probes (D_0029CA80: front axle middle (0, 0, 0.68), rear wheels (+-0.74, 0,
   -0.66), corners (+-0.9, 0, +-1.5)) through the matrix, each against the collision (Collision_Ground). A probe with
   no ground is a hit bit. **Car_Suspension** (0x0021AF28):
   - ground more than the clearance (0x51EB = 0.64; 0xA3D7 = 1.28 with part flag 0x400, the big tires: cars.md) above a probe is a wall: the probe
     keeps its height and sets its hit bit; a ceiling under a unit above is a roof (0x100);
   - the three wheels are springs: above the ground a wheel falls (its speed grows by local gravity + local.z /
     2048 a frame); on it the spring's step gets +0x59 + weight transfer (braking/accelerating front-back, turning
     left-right, from `accel`) - load x 0x59 / 4096, damped to 13/16 (15/16 when negative), load kept in 0..0x2000.
   - Height = (front x 33 + rear average x 34) / 67. Water below the front probe's water level wades (-1) or sinks
     (1: surfaces 0x100651, drag x4). Wading sets event 0x40 every frame. Going from dry to wading splashes (CQ_MAIN
     0x28, a player's car). The bubbles, spray and the green under water are in effects.md.
   - Up = the cross product of the three wheels' plane (rear right - front, rear left - rear right). Airborne for 64
     frames (or hitting with the corners) levels it by 1/16 a frame (Car_LevelInAir 0x002194B8). The matrix =
     Matrix_FromUp(up) turned by the yaw (Matrix_TurnY).
7. **Car_Crash** (0x00219660), from the corner hit bits (with the posts', Car_HitPosts below): the car goes back
   two moves with its old yaw, then by corners (FL, FR, RL, RR = 1, 2, 4, 8): a front corner turns it 0x200 away,
   takes a quarter of its sideways and a sixteenth of its forward speed and pushes it 0x800 back and aside; a rear
   corner halves the sideways speed and takes an eighth of the forward; both front (or both rear) corners stop it;
   one whole side pushes it aside. All four: stuck (race flag 0x200).
8. Body tilt (+0x40) and height (+0x74) from the loads; odometer; wheel angles (Car_SpinWheels 0x0021ACD0).

**The Flight Wing** (part flag 4 folded, 8 out; PARTS.BIN models 2 and 3): before the fins, Car_Update
(0x0021C114) puts it out when Vec_SpeedKmh (func_0021D358: the speed across the ground, |v.xz|) passes 300 km/h and
folds it under 300, in the car's parts record (+0x184: the save's car in the Adventure and on the field). While
it's out Car_Ground (0x0021B5A8) gives Car_Suspension the fins' climb alone as the wheels' reach, without gravity
(+0xC), so a car off the ground glides and the fins lift it ("Fly at 190mph. Use with jet turbine."; the jets'
cap is 350 km/h). The rooms fold it again (new_game.md). The port: Car_Update and Car_Ground (`wingOut`), the
callers passing the flags on to the drawn car and the save. Checked in the port with the threshold lowered to 150
(the wing comes out and draws as model 3); a real flight (a 300 km/h car off a jump) not tested. Debug:
`--partflags 3004`.

**The Endurance Run's pit** (race 24, Car_Input 0x0021A9D8): a car in zone 102 (0x66, the pit lane off zone 99 on
C20) with its lap zone set, short of its last lap, gets car flag 0x400; the AI drives it (input bit 8, the jet,
cleared) and its fuel fills 0x200 a frame up to 0x40000. For player 1's car (flags exactly 2) the horn stops and SE
0x309 plays on the way in, 0x8309 on the way out (flags 0x402). Race_Task shows PITTING meanwhile and GO! in zone
103 (hud.md). No AI path takes the branch. The port: Race::Race_PitStop; checked by placing the car in the pit lane
(the AI takes over and PITTING shows), with the lap-zone test lifted for the check.

Then, as a separate task, **Cars_Collide** (0x002179D8): for every pair less than 1.8 apart (in 1/32768 fixed
units, 2.8 world units) with a bumper corner of one inside the other's box (Car_Overlaps 0x002175D0: 0.9 either
side, 1.5 front and back), both are pushed apart along the line between them to 0.9 (at least by 1%) and both take
the average of their x and z speeds. Exactly in-line cars can slip through: their corners sit on the box's edge.

## Collision data (PMP)

The course file's third section, written as-is by modelconv to `data/collision/Cnn.pmp`. A 16x16 grid of 100-unit
cells: u32 offset[256] (from the PMP's start) and s16 count[256] at 0x400, u32 data end at 0x600, surfaces from
0x608. A surface is a triangle strip: u32 n (low 15 bits), two unused words, u32 flags; n vertices (x, y, z, 1);
n - 2 unit normals (x, y, z, 1). Collision_Ground (0x002084D0, the race's D_00335710) takes the highest surface
under (x, z) that is below the lowest **layer** surface (flag bit 31) above the point; layers count as ground too.
Height = v0.y - (n.x dx + n.z dz) (the game leaves out the division by n.y). Bit 28 marks water (its height is the
water level). The flags' low 3 bits are the surface type for the tires' grip; the car keeps each probe's flags
(+0x19C..+0x1B4; 0x2000 none).

## The port

- **Posts.** Car_Update calls **Car_HitPosts** (func_00219ED8) after the ground, with the car's float position
  (+0x90, from the fixed one), and ORs what it returns into the corner bits for Car_Crash.
  - It runs **Collision_Posts** (func_00219CB0) on the PMP's posts for the race's slot. The PMP is "0x608" when its
    +0x600 is 0x608: then +0x600 is an offset to the list, and +0x604 is the count of (x, y, z, bottom) floats.
  - The Adventure's fields (course -1) also try 26 other lists.
  - A post counts while the car's height + 1 is at least its bottom and the post's y is not below the car.
  - Turned into the car's frame (sceVu0RotMatrixY by -yaw), a post within 0 to 1.5 ahead is a front corner and one
    within -1.5 to 0 behind is a rear corner: left for x in [-0.9, 0), right for x in [0, 0.9].
  - Posts are trees, lamp posts and pillars: Peach and Oval have none, Tin 12, Endurance Run 16, Temple 19, Lagoon
    76, Night Glow 84 and Snow Mountain 239.
  - The port: `Collision::Collision_Posts`, called from `Car_Update` for every car.
  - **Check:** on Snow Mountain, a car driven at the posts at (589.7, 11, 600.6) is turned away 0x200 at a time.
    PCSX2's car from about the same state was too.
- `game::Collision` loads the PMP and answers Collision_Ground; `game::CarBody` / `CarParts` / `Car_Update` /
  `Cars_Collide` are the physics; `game::Race` gives every car a body, drives car 0 from the pad
  (`Car_ReadPad`), updates them all and collides them, and draws them from their state (orientation, the body on its
  suspension, wheels rolling and steering).
- Car_Drive also runs Car_Sound (0x00217C68) with the squeal: every car's effect count and a player's engine and
  skid sounds (effects.md). The port splits it: the count in `Car_EffectTime`, the sounds in `CarSounds`.
- **The jet turbines (part flag 0x2000).** Car_Init gives a car its part flags (the parts copy at +0x184, +8):
  in a Quick Race (mode 1) and 2 Player's modes 2 and 3 the parts record's +0xE (asm 0x002185F0: D_002BE318[card]
  or D_002BE278[row]), in Custom Race the save car's own. Quick Race card 6 (Tin Raceway, course 0x10) and 2
  Player's Obstacle Course (row 8) have 0x2000; card 5 (Lagoon) has 0x40, the propellers. With 0x2000, Car_Update
  calls **Car_Jet (func_00217F10)** after Car_Input and adds its push to the forward speed (sp8, before the drag),
  then holds that to at most 0xCF69:
  - only while race flags & 0xC is 4 (green, not over); otherwise the count (+0x20E) goes to 0 and its sound stops;
  - R2 (input 8) with fuel (+0x23C, 0x40000 at the start) of 100 or more burns 100 and pushes 0xB2. The count goes
    up; under 0x7530 fuel the push is 0x59 and the count runs round 8 frames, under 0x2EE0 0x2C and 4 frames;
  - the roar is CQ_MAIN 0x10 + (car flags & 1), a loop, for a player's car (flags & 3): it starts when the count
    leaves 0 (not in the replay, race flag 0x40) and stops (0x8010 + pad) when it comes back to 0, so on low fuel
    it sputters;
  - let go, a count above 0 goes to -2, then counts up to 0 (the flame dies out), with the sound stopped;
  - Car_Stop (func_0021C998, the Obstacle Course's respawn) zeroes the count and stops the roar.
  The port: `Car_Jet`, `CarParts::flags`, `CarBody::jet` and `jetSound` (played by `CarSounds::Car_Jet`).
  PCSX2: on the Obstacle Course R2 fires both turbines' flames and the car pulls away; the port's run matches.
- **The opponents' part flags.** For a car that isn't a player's (flags & 0x33 clear) on a course other than -1 and
  0x22, Car_Init copies the course table's +4 record (the opponents' parts record at +4) and takes +0x190 from its
  +0x12, the record's +0xE: the same field as the player's. The Quick Race's: card 5 (Lagoon) 0x1040, the propellers
  and the fins; card 6 (Tin Raceway) 0x3000, the jet turbines and the fins; the others none. Ai_Drive already
  presses R2 with Cross at full throttle (its 9), so the opponents fire their jets. PCSX2 (Tin Raceway, opponent 1
  traced from the grid): its speed at each AI zone is within 1% of the port's all the way to zone 26.
- **The propellers (part flag 0x40).** In Car_Update after Car_Jet (asm 0x0021BE94): in water (+0x213 not 0)
  Circle (input 4) takes 0x59 off the forward speed, else Cross (input 1) adds 0x59. PCSX2 (Lagoon, Cross held from
  the grid, `race_lagoon_grid.p2s`): into the water at x 818 at 22,482, out at 701 at about 21,000; the port enters
  at 22,488 and keeps 18,000-22,700 to the same place (without the push it falls to 14,000). The rest of the gap is
  the car bouncing on the river bed, which the push doesn't change (the PS2's front load stays 250-500, the port's
  swings to 5,000): not looked into yet.
- **The fins (part flag 0x1000).** Car_Update after the drag: L1 (input 0x20, pressed) steps the setting (+0x244,
  0 from Car_SetParts) down from 0 or 1, L2 (0x40) up from 0 or -1, so it is -1, 0 or 1; and left / right (0x8000 /
  0x2000) add -+ forward speed / 512 to the yaw (+0x1D4) on top of the steering, on the ground or in the air. The
  setting goes into Car_Ground's lift ("climb", the load the suspension takes): (setting + 1) x speed / 2048, under
  water setting x speed / 256. Car_Draw draws the body's section 3 in place of 2.
- **2 Player's drag.** With mode & 2 (asm 0x0021BED8: race +0x0A & 2), a car whose +0x247 isn't 0 divides by
  0x500 instead of 0x600: more drag. +0x247 is the car's place, which Race_RankCars (func_0022D7B8) writes every
  frame (0 = leading; Car_Init's grid slot until then, Race_CompleteLap's finish order after), so in 2 Player every
  car but the leader has the extra drag. PCSX2 (`versus/race_er.p2s`, both players behind their teammates, Cross
  held on both pads): the speed gained a frame at 21,000-23,000 is 24.2, and 32.6 with the mode byte (0x0033590A)
  set to 1; the port's 2 Player run gains 27.3 (36.4 / 19.9 at 18,000 / 23,000 against the PS2's 36.6 / 20.0).
- The autodrive (`--autodrive`) and the replays are in (finish.md). The field's race flag 0x400 (set by
  func_00210430 as the car drives out of a building; never in a race) is in field.cpp: the car's flag 0x100 holds
  its wheels' speed and keeps only the steering, and Car_FieldStart (func_002182B0) puts it at the door. The countdown holds the cars until green (race flag 4,
  hud.md).
- Development: `RTAGlass.exe --race ID --cartrace FILE` writes the player's state each frame (`--tracecar N` an
  opponent's), for comparing against the same fields read from PCSX2.

## Check

`captures/race_peach_grid.p2s` (Peach Raceway's grid during the countdown), Cross held from the start, then
into the first corner's wall:

- Every straight-line velocity matches the PS2's exactly, through all five gears (5th at 20382), and the
  suspension loads match (PINE reads land mid-frame, so the PS2's loads lag a frame half the time).
- The wall hit and the bounce follow the same path; the port hits one frame earlier (the PS2's FPU truncates where
  x86 rounds, which moves the rear-right load by a few units on the kerb).
- A steering test (right, left, brake) tracks the PS2 within a unit of turn rate (the radius' speed lands on a
  rounding boundary); braking and the downshifts match.
