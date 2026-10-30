# The cars' sounds, water and effects

What the race does around a car besides moving it: the player's engine and skid sounds, the horn, the bumps and
the splash, the particles at the wheels (smoke, dust, snow, and in water the splashes, the bow wave and the bubble
rings), and the green light on a car under water. The port has all of it (`src/game/race/car_effects.cpp`,
the engine voices in `src/audio`). The water's physics is in car_physics.md ("Car_Ground"); the sound
driver's engine side is in sound.md ("Engines").

## The effect count (Car_Sound's first half)

Car_Drive calls **Car_Sound** (0x00217C68) for every car with Car_Engine's squeal. It keeps the car's effect count
(+0x208) by the class of the ground under the front wheels (surface bits 8-10):

- The count goes up while the squeal is over 0x40; or while the driven tires turn (+0x1BC) with a rear wheel down
  (+0x1E0 or +0x1E4) on classes 1 (dirt), 2 (snow) or 6 (a river bed); or, on a river bed, while the car is under
  water (+0x213 = 1).
- Otherwise a running count is negated. The effects read a count of 0 or less as dying away and clear it.

The rest of Car_Sound is for the players' cars only (flags & 3), below.

## Sounds

| Sound | When | Code |
|---|---|---|
| Engine | Always, for a player's car (SNDMOD's engine 0 or 1, sound.md) | Car_Sound |
| Skid loop | CQ_MAIN 6, 8, 0xA or 0xC (+1 for player 2) by the ground's sound bits 4-6 (D_0029CBA8), while the squeal is 8 or more | Car_Sound |
| Horn | +0x245 (0x3C + 2 x the parts' sound byte, D_002F00AC; a player adds 1) while key 6 (R1) is held | Car_ReadPad |
| Splash | CQ_MAIN 0x28, a player's car going from dry to wading, not with race flags 0x48 | Car_Ground |
| Bump | CQ_MAIN 0x14, Cars_Collide's pair with a player's car, unless the other car bumped in the last 8 frames (+0x210) | Cars_BumpSound 0x00217830 |
| Crash | ACTION 0x11 (0x8311 first), only on course 0x1C (Car_Crash's variant there, events.md): a player's car, at most every 0x20 frames; walls are silent elsewhere | Car_Crash |

- **The skid loop's volume** follows the squeal: 8-0x1F gives 0x40, 0x20-0x3F gives 0x80, 0x40-0x5F gives 0x100, and above
  that the squeal itself, capped at 0x100. Sound_SetVolume (0x00257C68, RPC 9: the voices' levels × volume / 256)
  sets it on the loop. A change of ground stops the old loop and starts the new one.
- **The engine:** each frame, Sound_EngineRpms(player, type 2, select, rpm +0x1D0, reverb = surface bit 16). Select is
  1 (the throttle sample) while Cross is held and the car isn't under water.
- **A finished car** (flags 0xA00) turns its skid loop down to 0 and idles its engine (rpm 0). In the replay (race
  flag 0x40) Car_Sound changes nothing, so the engine keeps the finish's idle.
- **The horn** stops at the player's finish (Race_CompleteLap sends +0x245 + 0x8000). finish.md's "0x3C" is the horn.
- The bump also shakes the pad, as do landings and wall hits (below).

## The pad's vibration

**Pad_Vibrate** (func_0020ACB8, pad state, pad, pattern, strength) starts one of eight patterns (D_0029AD60) on a
pad, replacing what was playing, if the pad's mode (+0x0C + pad) ANDed with the Options' setting (+0x0E: 4 On,
2 ANALOG mode button, 0 Off) is non-zero and the strength isn't 0. The pad reader (func_0020A8A8) sets the mode from
func_00273FA8(port, 0, 2, 0), which looks like libpad's scePadInfoMode(InfoModeCurExID); by libpad's ids that is 7
for analog and 4 for digital, which fits the settings (not checked on the PS2). A strength over 0xFF is 0xFF. It sets the pad's pattern pointer (D_00333358), wait
(D_00335698) to 0 and strength (D_003356A0).

**Pad_VibrationTask** (func_0020A660) runs first in the root task (func_00205088), before the pad reader and the game,
so a shake starts on the next frame. For each pad with a pattern it counts the wait down and, at 0, runs commands:

| Byte | Command |
|---|---|
| 0x80 / 0x81 | the small motor on / off |
| 0x82 n | the large motor at n |
| 0x83 | the large motor off |
| 0x84 | the large motor at the strength |
| 0xFF | both off, the end |
| other | wait that many frames |

The motors' bytes (D_003356A8, small then large per pad) go to scePadSetActDirect. A pad whose motors have run for
180 frames in a row (D_003356B8) is held off until the count reaches 360, then the count starts over.

| # | Bytes | Caller |
|---|---|---|
| 0 | FF | |
| 1 | 80 84 02 81 06 FF | Car_Suspension: a wheel landing at spring speed 0xB3 or more, strength speed / 16 |
| 2 | 80 84 04 FF | none found |
| 3 | 80 01 81 82 FF 02 FF | Car_Crash: hits 2, 8, 10 (front right, rear right, the right side) |
| 4 | 82 FF 01 80 01 FF | Car_Crash: hits 1, 4, 5 (the left) |
| 5 | 80 82 FF 02 FF | Car_Crash: every other hit but 0 (jtbl_002EAE60) |
| 6 | 84 04 80 02 81 02 FF | Cars_BumpSound: a player's car and another, strength = |their velocities' difference| / 32768 x 1000; the golf (0x40) |
| 7 | 82 7F 1E FF | Options_VibrationTask's TEST, both pads |

Every race caller skips a car with flags 0xFB0 (an AI-driven or finished car) and race flag 0x40 (the replay). The
pad is the car's flags & 1.

**The port** (`game/pad/vibration`): the same patterns and task, a pad's mode always 7 (an XInput controller is
analog). The task runs at the top of the frame and `platform::Input::SetMotors` sends pad 0's motors to the
XInput controller: the small motor as the right (high-frequency) motor at full, the large one as the left at
strength x 0x101. Car_Suspension and Car_Crash leave the shake in `CarBody::shake`; Race_Step passes the player's
on (not finished, autodriven or in the replay), and Cars_BumpSound shakes with the bump sound.

**Check:** a scripted Peach Raceway run (hold Cross, steer into the walls) logged the motors each frame: a scrape on
the right gives the small motor, then the large at 255, again every frame of contact; a landing gives both at the
strength (23 there) for 2 frames, then the large alone for 6.

## The particles (Cars_DrawEffects)

Race_DrawView's last step, **Cars_DrawEffects** (0x00225120), goes over the cars, then draws the particles.

**The pool.** D_0172FF70 holds 64 particles. Each is a pair of points (two vec4s), one per rear wheel or a pair of
drops: +0xC is the alpha and +0x1C the size. A ring keeps its size in +0xC and in +0x1C the height it rises to.
D_01730770 holds the types and D_017307F0 the drops' age and kind. A free particle is the first with +0xC ≤ 0.

**Effect_DrawPuff** (0x00223B30) draws one point pair:
- a quad (D_00290770: x −0.5..0.5, y 0..1, standing on the point) at each point;
- turned about the vertical to face the eye (Matrix_Billboard 0x00207F88);
- scaled by the size, at the alpha (1 = the texture's own; vertex colour 0x80);
- the texture is SYS/EFFECT.GSL, by type, with the animation frame (frame + 6 × car) >> 2 & 3:

  | type | picture | TBP | EFFECT.GSL |
  |---|---|---|---|
  | 1 smoke | white-blue cloud | 0x3F4A + 3n | 7-10 |
  | 2 dust | brown cloud | 0x3F7A + 3n | 23-26 |
  | 3 snow, and the drops (8) | white speckle | 0x3F68 + 3n | 17-20 |
  | 7 ring | a bubble | 0x3F29 | 0 |

  EFFECT.GSL 1-6 (flames) are the jet's: **Car_DrawJet** (func_002238A0), drawn by Cars_DrawEffects before the
  car's effect for cars 0-2 (D_0177AC50 order) while their jet count (+0x20E) isn't 0:
  - n = the count; alpha 0.5 (D_0029D370's w, the colour multiplier), or for n < 0 alpha n x -0.15 and n = 20 - n;
  - picture 1 + n % 6 (TBP 0x3F2C + 5 (n % 6), T4 64x32), GS state as the puffs';
  - the quad D_00290670: x -0.375..0.375, from (y 0.15, z 0) at the nozzle to (y -0.05, z -1.5), s along it;
  - scaled by n / 16 + 0.5 across and n / 32 + 0.75 along for n < 8 (it grows);
  - turned about z (sceVu0RotMatrixZ) by k x 3.14 / 8 for k = -8, -6 ... 6 (odd k on odd frames): eight quads;
  - at the body's nozzle D_002ED720 + 0x44 body, +0x38 (`car_jets.inc`), and again at -x, in the car's body frame
    (car +0 times +0x40).
  PCSX2 (the Obstacle Course, R2 held): a bright burst at each turbine, as the port draws it.
  Cars_DrawEffects is called from the field's view too (func_00220938), so the flames show when the jet fires on
  the field (field.cpp draws them before the car's effect).

**By ground class**, while the count runs:

| class | effect | at the rear wheels (x ±0.74) | left behind |
|---|---|---|---|
| 0 road | **Car_EmitSmoke** (0x00224068) | count 1-32, then round from 16; z = −1.2 − 0.02 n, size n/32 + 0.4, alpha n/64; a second, older puff from 17 on | every 2 units: alpha 1/4, size 0.4 |
| 1 dirt | **Car_EmitDust** (0x002243E0) | count 1-16; alpha n/32 | every unit in view: alpha 1/2 |
| 2 snow | **Car_EmitSnow** (0x002246C0) | count 1-16; alpha n/64 | every 1.25 units in view: alpha 1/2 |
| 6 river bed, under water | **Car_EmitBubbles** (0x00224EC0) | a ring grows, size (n + 1)/128, at z = −0.8 − 0.04 n | at 16, in view: the ring, rising to the water level (+0x11C) − 0.5 |
| 6 river bed, wading (+0x1F8 & 0x40) | every 4th frame, 4 × **Car_EmitSplash** (0x002249B8); every frame **Car_EmitBow** (0x00224CD0) | | drops (type 8) |
| other | the count is cleared | | |

A dying count (n ≤ 0) gives the puff that's left alpha −n/64 and size −n/32 + 0.4.

**Effects_Draw** (0x00223D58) draws and ages each particle every frame (not while paused, race flag 0x8000):
- smoke: alpha −1/64;
- dust: alpha −1/128, dead at 1/8;
- snow: alpha −1/128;
- all three: size +1/16;
- rings: size +1/128, rising 1/16 a frame. A ring is gone at size 1 or at its height.
- drops: **Drop_Update** (0x00225390).
  - A splash (kind 1) is a pair of drops beside the car at (−0.4 − s/16, 0.5, 1.5 − 0.375 s′) and its mirror, with s from the seed (frame + 0, 2, 4, 6). It starts at alpha speed/300 + 1/4 (speed = Vec_SpeedKmh 0x0021D358, capped at 1) and size 1/2, then loses 1/32 of alpha and gains 1/4 of size a frame.
  - The bow wave (kind 2) starts at the nose (0, 0.2, 1.75) at alpha speed/300 + 1/2. It follows the car: on a 16-frame cycle its drops part to ±0.094 n, rise n/64 and fall back 0.075 n. It loses 1/16 of alpha and gains 1/16 of size a frame.

## Under water (Car_LightsUnderWater)

Car_Draw passes its light set (D_0177F620[surface bits 12-13]) and the car's height above the water level (the front
probe's, +0x11C; 0 where there is none) to **Car_LightsUnderWater** (0x00226058). More than 2 units under, every
colour of the set (both lights, the highlight and the ambient) is scaled:
- red by 0.5 + 0.01 d, at least 0;
- green and blue by 1 + 0.01 d, at least 1/4.

The car turns green. The river bed and the water surface are the course's own geometry; the surface is in block 64,
drawn after the cars.

## The port

- `CarBody` gained the count (+0x208), the latest particle (+0x20C), the bump timer (+0x210) and the squeal.
  `Car_EffectTime` in car_physics.cpp is Car_Sound's first half, run from Car_Drive for every car. `Cars_Collide`
  returns the pairs it pushed apart.
- `game::CarSounds` is the player's half of Car_Sound, the horn, and the start and stop.
  - The engine starts as Car_Init does it. Sound_EnginePlay asks for type 0 at rest. Car_Init's last call is
    Car_Update, which runs Car_Drive and so Car_Sound: type 2, rpm 0, select 1. SNDMOD keys on only at its next
    vblank (procEngine), and by then the type is 2, so the grid sounds wave 0x29 at pitch 2000. Had a vblank come
    between the two, type 0's wave 1 would stay: procEngine reloads only on a new select, never on a new type.
  - The PS2's grid (`race_peach_grid.p2s`): the engine block says type 2, select 1, last keyed 1. SNDMOD's diwork
    says waves 0x2A and 0x29. The SPU2's own voice state (SPU2.bin) has start 0xEA960 and loop 0xEAECE, playing at
    +0xC8, all inside wave 0x29 (SPU byte 0x1D52C0, from the wave table at SNDMOD +0xC988). Wave 0x2A's start is on
    the other voice. Nothing points into wave 1.
  - The port's grid keys the same: type 2, select 1, wave 0x29 at rpm 0, on the first race and on a rerun. Releasing
    Cross at speed keys 0x2A.
  - The engine and the skid loop stop when the race is left or over.
- `game::CarEffects` is the pool, the emitters and Effects_Draw, drawn after Race_DrawView. Its puffs use
  `gfx::DrawWorldTriangles` (model.h): depth tested, no depth write, unfogged.
- `Race_Step` plays the splash and the bumps (Cars_BumpSound). Car_Draw applies Car_LightsUnderWater.
- The boost flames are Car_DrawJet (car_effects.cpp; above).

## Check

- **The engine.** PCSX2 and the port on Temple Raceway, Cross held from rest. The engine's line runs at about 350 Hz in
  first gear and drops to about 260 Hz at each shift, with the shifts at the same times (`captures/race/engine_spectrogram.png`;
  the audio is `engine_temple_pcsx2.wav` and `engine_temple_port.wav`).
  - A save state mid-run gave SNDMOD's own state: voice 1 on wave 0x29 at pitch 16112 for rpm 8586, which is data.169's line, and the level 0x1CCC. Engine 1 is stopped.
  - The engine's share of the mix is within a dB of the PS2's.
- **Water.** The same run teleported over PINE to Temple Raceway's pool.
  - On both, the car drops in at y = 84, the water level, drives along the bed with the bubble rings rising from the rear wheels, and turns green (`water_compare.png`).
  - The PS2's colours differed by the course's lights and fog, which the port didn't have then (race.md, "Lighting").
  - Lagoon Raceway's shallow water in the port: the spray and the bow wave while wading, the dust on the sand, the bubbles and the green under deeper water (`lagoon_effects.png`). Temple's drop into the pool is in `temple_underwater.png`.
- **Smoke.** On Peach Raceway the rear wheels smoke once the squeal passes 0x40 in a hard turn (`peach_smoke.png`).
