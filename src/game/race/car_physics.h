// The cars' physics: a reimplementation of the game's per-frame car update, Car_Update (0x0021BA70), and what it
// calls, in the game's own fixed-point arithmetic. See docs/car_physics.md for the model.
//
// Units: positions are fixed point, 2^21 / 100 = 20971.52 per world unit, wrapping every 1600 units; velocities are
// in the same units per frame times 25/16 (a frame moves a car by velocity x 16 / 25). Angles are 16-bit
// (0x8000 = pi). Heights in the suspension are 1/32768 of a unit.
#pragma once

#include <stdint.h>

#include "collision.h"

namespace game {

// A car's parts as the physics uses them (Car_SetParts 0x002180C0 fills them from a parts record).
struct CarParts {
    int32_t torque;          // +0x214: the engine's force
    int32_t mass;            // +0x218
    int16_t grip[8];         // +0x21C: the tires' grip by surface type (types 6 and 7 have none)
    int16_t gears[8];        // +0x22C: reverse, then the gears' ratios (0 ends them)
    int16_t fuelUse;         // +0x240: the fuel each frame of full throttle burns
    int16_t steer;           // +0x242: steering strength
    const uint8_t* brakes;   // +0x200: the brake's force by frames held (32 steps)
    uint16_t flags;          // the part flags (the car's parts copy at +0x184, +8), which Car_Init sets, not
                             // Car_SetParts: its parts record's +0xE in a Quick Race and 2 Player's modes 2 and 3
};

// Part flags. 0x40: the propellers (PARTS.BIN models 6 and 7), which push the car in water; 0x400: the big tires
// (PARTS.BIN model 10 and TIRE.BIN sections 4 and 5), with a longer suspension reach, the body 0.85 higher and wheels
// of radius 0.55; 0x1000: the fins (the body's section 3 in place of 2), which steer and lift the car by its speed;
// 0x2000: the jet turbines (a body's model section 4, Car_Jet).
// 0x4 / 0x8: the Flight Wing folded (PARTS.BIN model 2) / out (model 3): Car_Update puts it out over 300 km/h and
// folds it under, and while it's out Car_Ground leaves gravity out of the wheels' reach, so the fins' lift flies the car.
const uint16_t kPartWingFolded = 0x4, kPartWingOut = 0x8;
const uint16_t kPartPropellers = 0x40, kPartBigTires = 0x400, kPartFins = 0x1000, kPartJet = 0x2000;
// The body's lift on the big tires (D_00332C18, and Car_Init's, the rooms' and the cards' D_00332B7C/E14-E28).
const float kBigTiresLift = 0.85f;
// D_0177EA50[0]'s part flags: the player's car (player 1's), which Car_Ground's water test reads for every car.
void Car_SetPlayerFlags(uint16_t flags);
// The time of day Car_Update's headlights go by (pad +0x14: a race's course clock, the field's info+0x14).
void Car_SetClock(int32_t clock);
// A 16-byte parts record's part flags (+0xE): D_002BE318's for the Quick Race's player, D_002BE278's in 2 Player.
// Quick Race card 6 (course 0x10) and 2 Player's Obstacle Course (row 8) have the jet turbines, card 5 (Lagoon) the
// propellers.
uint16_t Car_PartFlags(const uint8_t* record);

// Car_SetParts (0x002180C0): the parts of a parts record {AI level, tires, engine, weight, gears, steering, brakes}
// (the Quick Race's: D_002BE318 by card for the player, the course table's for its opponents). An Adventure
// teammate's car (flags 0x30) grips a sixteenth better and weighs a quarter less.
CarParts Car_SetParts(const uint8_t* record, bool teammate = false);
// The Quick Race's parts records: the player's by card, and the opponents' by card.
const uint8_t* QuickRace_PlayerParts(int card);
const uint8_t* QuickRace_OpponentParts(int card);
// 2 Player's parts record for course row `row` in modes 2 and 3 (D_002BE278); 16 bytes.
const uint8_t* Versus_RowParts(int row);

// The pad as the car code reads it (Car_ReadPad 0x0021A820 with the game's default key setup).
enum CarInput : uint32_t {
    kCarAccel = 0x1,      // Cross
    kCarBrake = 0x2,      // Square
    kCarReverse = 0x4,    // Circle (with 0x1)
    kCarR2 = 0x8,
    kCarHeadlights = 0x10,   // R1 held
    kCarL1 = 0x20,        // L1 pressed
    kCarL2 = 0x40,        // L2 pressed
    kCarRight = 0x2000,
    kCarLeft = 0x8000,
};

// One car's physical state (the fields of D_0177AC50's 0x270-byte records that the physics uses).
struct CarBody {
    float m[3][4];          // +0x00: orientation, rows = the car's x (right), y (up), z (forward) in the world
    float inv[3][4];        // +0xB0: its inverse (transpose)
    float body[3][4];       // +0x40: the body's tilt on the suspension, in the car's frame
    float bodyHeight;       // +0x74
    float wheelAngle[3];    // +0x80: front wheels' spin, steering angle, rear wheels' spin (radians)
    float pos[4];           // +0x90: position in world units
    int32_t fixed[3];       // +0xA0: position, fixed point
    int32_t vel[4];         // +0xF0: velocity
    int32_t prevVel[4];     // +0x100: last frame's velocity (after gravity)
    float probe[7][4];      // +0x110: the ground probes in the world (front middle, rear left, rear right, corners)
    int32_t surface[7];     // +0x19C: each probe's surface flags (0x2000 = none found)
    int32_t wheel;          // +0x1B8: the driven wheels' speed
    int32_t tire;           // +0x1BC: the engine's speed at the tires
    int32_t spring[3];      // +0x1C0: suspension speed: front, rear left, rear right
    int16_t turn;           // +0x1CC: turning rate from the steering
    int16_t steerAngle;     // +0x1CE: -0x20..0x20
    int16_t rpm;            // +0x1D0: 0..10000
    int16_t slideRate;      // +0x1D2
    uint16_t yaw;           // +0x1D4
    int16_t slide;          // +0x1D6: the angle between the car and where it goes (sliding)
    int32_t turnSpeed;      // +0x1D8: the speed the radius of the turn is worked out from
    int32_t load[3];        // +0x1DC: suspension compression, 0x1000 at rest
    int32_t springStep[3];  // +0x1E8
    int32_t airTime;        // +0x1F4
    uint32_t events;        // +0x1F8: this frame's events (1 airborne, 2 engine, 0x20 slide, 0x40 water)
    uint8_t lights;         // +0x1FC
    uint8_t brakeTime;      // +0x1FE
    int8_t gear;            // +0x1FF: 0 reverse, 1..
    int32_t odometer;       // +0x204
    int32_t effect;         // +0x208: the effect's frames: counts up while the tires squeal or kick up the ground (or
                            // the car is under water), negated when that stops (car_effects.h)
    int16_t particle;       // +0x20C: the effect's latest particle
    int16_t bump;           // +0x210: frames before another car's bump sounds again
    int8_t crashTime;       // +0x212: frames before a wall in the Tunnel Race sounds again
    int8_t water;           // +0x213: 0 dry, -1 wading, 1 under water
    int16_t jet;            // +0x20E: the jet's frames firing (Car_Jet); -2 and -1 as it dies out
    int32_t fuel;           // +0x23C: 0x40000 at the start; the jet burns 100 a frame
    int8_t fins;            // +0x244: part 0x1000's setting, -1, 0 or 1 (L1 / L2); Car_SetParts sets 0
    int32_t squeal;         // (port) Car_Engine's squeal this frame, for Car_Sound
    uint8_t shake;          // (port) the pad's shake this frame asks for (VibrationPattern, 0 none): the game calls
    uint8_t shakeStrength;  // Pad_Vibrate straight from the physics for a player's car (flags 0xFB0 clear)
    bool crashed;           // (port) a wall in the Tunnel Race this frame that sounds (ACTION 0x11, for a player's car)
    uint8_t jetSound;       // (port) the jet's roar this frame: kJetStart or kJetStop (CQ_MAIN 0x10 + pad), 0 none
};
enum JetSound : uint8_t { kJetNone, kJetStart, kJetStop };

// The debug byte's readouts (the Car Readouts extra): while a printer is set, Car_Update and Car_Engine print what
// they work out as the debug build did (D_003356D0/D4, Debug_PrintCar 0x0026D700; docs/extras.md). The caller sets
// it around the one car that has them (flags exactly 2: player 1's, driven from the pad) and clears it after.
// `vectors` false leaves out all but Debug_PrintCar's lines, as in the field (race flag 0x400 turns D0/D4 off).
typedef void (*CarDebugPrint)(int x, int y, const char* format, ...);
void Car_SetDebugPrint(CarDebugPrint print, bool vectors = true);
// The debug byte's ride height (Car_Ground 0x0021B3D0; the Height Keys extra), for the next Car_Update: 1 (the
// d-pad's Up held) puts the car 0x1000 below where its suspension holds it, -1 (Down) 0x51EB above; 0 neither. Not
// while a probe finds a roof (Car_Suspension's 0x100). The caller sets it for a player's car and clears it after.
void Car_SetHeightKey(int key);

// Car_Init's physical part: the car at rest at `pos` (world units, on the ground) facing `yaw` (16-bit angle).
void Car_Place(CarBody& c, const float* pos, uint16_t yaw);
// func_0024FB80 then func_0021C928: a car put back at `pos` facing `yaw` mid-race (the Obstacle Course's
// checkpoints): func_0021C998 stops it (speeds, suspension, gear 1, rpm), its matrices go square; the fuel and the
// distance driven stay.
void Car_Reset(CarBody& c, const float* pos, uint16_t yaw);
// The PC's cheats (extras.h's kCheat*) for the next Car_Update: the caller sets them around a player's car and
// clears them after. Car_PlayerCheats is the set the Cheats screen has on.
enum CarCheat : uint32_t {
    kCarCheatUncapped = 0x1,   // top gear revs on past the limiter (to 32000), the jet's push not held to kJetTop
    kCarCheatFuel = 0x2,       // the tank full every frame
    kCarCheatPower = 0x4,      // twice the torque
    kCarCheatGrip = 0x8,       // twice the tires' grip
    kCarCheatGravity = 0x10,   // a third of the gravity (the fall and the suspension; the tires' load stays)
    kCarCheatDrag = 0x20,      // a quarter of the forward drag
};
void Car_SetCheats(uint32_t cheats);
uint32_t Car_PlayerCheats();

// Car_SetPlace (func_0021C928): the car's position and yaw, nothing else (Roulette's wheel carries the car with it).
void Car_SetPlace(CarBody& c, const float* pos, uint16_t yaw);
// Car_Stop (0x0021C998): the car stopped where it is: its speeds, the suspension's, the steering and the rpm to 0,
// first gear, the suspension at rest (the field's bump into a town car stops both).
void Car_Stop(CarBody& c);

// Car_Jet (func_00217F10), for a car with the jet turbines: while the race is on (`go`), R2 burns 100 of the fuel a
// frame and returns the push forward: 0xB2, under 30000 fuel 0x59 and under 12000 0x2C, when the count (+0x20E)
// also runs round 8 or 4 frames and the roar starts over each time. Let go, the count goes to -2, then up to 0.
int32_t Car_Jet(CarBody& c, uint32_t input, bool go);

// One frame of a player's car (Car_Update 0x0021BA70 with Car_Input 0x0021A990 on the pad path). `go`: the race has
// started (before that the engine runs but the car is held). `tunnel`: course 0x1C, the Tunnel Race, whose walls
// bounce the cars and which moves them twice as far (Car_Crash). `slowed`: in 2 Player (mode & 2) a car whose slot,
// and after the finish its place (+0x247), isn't 0 has more drag. The Flight Wing's swap changes `parts.flags` (the
// game writes it to the car's parts record, the save car's in the Adventure): the caller passes it on.
void Car_Update(CarBody& c, CarParts& parts, const Ground& ground, uint32_t input, bool go,
                bool tunnel = false, bool slowed = false);

// Matrix_FromUp (func_00208008): rotation rows from an up vector: x = normalize(up.y, -up.x, 0), y = up, z = x cross y.
void Matrix_FromUp(float m[3][4], const float* up);
// Matrix_TurnY (func_00208080): the rows turned by `angle` (radians) about their own y.
void Matrix_TurnY(float m[3][4], float angle);

// The car's position in world units from its fixed-point one (as Cars_Draw places it, func_0021C728).
void Car_WorldPosition(const CarBody& c, float* out);

// Cars_Collide (0x002179D8), after the cars' updates: every pair less than 1.8 apart (in 1/32768 of the fixed point,
// 2.8 units) with a bumper corner of one inside the other's box (0.9 either side, 1.5 front and back) is pushed apart
// to 0.9 (1.4 units) and both take the average of their speeds across the ground.
// Each pair that touched is written to `contacts` (up to `max`, if given) for func_00217830's bump sounds; returns
// how many touched.
struct CarContact {
    int a, b;
};
int Cars_Collide(CarBody* const* cars, int count, CarContact* contacts = nullptr, int max = 0);

}  // namespace game
