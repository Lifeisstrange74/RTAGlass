#include "field_objects.h"

#include <math.h>
#include <stdlib.h>

#include <algorithm>

#include "../../audio/sound.h"
#include "../race/course_objects.h"
#include "../save/save_data.h"

namespace game {

namespace {

#include "field_objects.inc"

const float kPi = 3.14159265f;   // D_003330D4, D_003330D8, D_003330DC
const gfx::Rect kFrame = {0, 0, gfx::Draw2D::kWidth, gfx::Draw2D::kHeight};

// The blocks of func_0025D028's table (D_002C4AD0) drawn here.
const int kSailBlock = 39, kPalmBlock40 = 40, kPalmBlock42 = 42;

// Town 12's UFO (block 22's task 0x00240228 and its draw func_0025D328): its home D_002A2BB0, the heights it hangs at
// and leaves to, the circle's start (D_00332E88), the steps a frame (D_00332E84, 8C, 94-A0, A4) and the clock's
// times: 17:00 (and the 17:00-17:53 span), 17:56:59, 6:30, and the day's and night's spans from 6:30.
const int kUfoBlock = 22;
const float kUfoHome[3] = {900.0f, 25.0f, 300.0f};
const float kUfoAway = 100.0f, kUfoCircleZ = 229.32f;
const float kUfoSink = 0.1f, kUfoSlide = 0.2f, kUfoReturn = 0.2f, kUfoRise = 0.1f;
const int32_t kUfoArrive = 0x255A8, kUfoArriveSpan = 0x1FA4, kUfoLeaveCircle = 0x2770D;
const int32_t kUfoMorning = 0xE484, kUfoDaySpan = 0x17123, kUfoNightSpan = 0x190C7;

// Field_ObjectsTask's texture copies (D_002A3800's handlers, rows D_002A3050-D_002A3470), in table order: the block,
// the kind (0 the task's frame count, 1 the tide, 2 block 22's lights by event flag 0x50), then as the race's.
struct FieldCopyRow {
    uint8_t block, kind;
    uint16_t sbp, dbp, bw, psm, w, h;
    int16_t speedX, speedY;
    uint16_t maskX, maskY;
};
const FieldCopyRow kFieldCopies[] = {
    {43, 0, 0x3772, 0x3759, 2, kT4, 64, 128, 0, -16, 0, 0x7F},     // 223: 213/035 -> 213/034
    {43, 0, 0x37B0, 0x37AB, 1, kT4, 64, 16, 0, -64, 0, 0x30},      // 223: 213/037 -> 213/036
    {23, 0, 0x34DF, 0x34BE, 2, kT4, 128, 128, 0, -4, 0, 0x7F},     // 113: C14/006 -> C14/005
    {23, 0, 0x3525, 0x3520, 1, kT4, 64, 16, 0, -32, 0, 0x30},      // 113: 113/003 -> 113/002
    {19, 0, 0x3939, 0x390A, 2, kT4, 128, 128, 0, -16, 0, 0x7F},    // 103: A19/002 -> A17/000
    {19, 0, 0x3930, 0x392B, 1, kT4, 64, 16, 0, -64, 0, 0x30},      // 103: 103/057 -> A17/001
    {19, 0, 0x397F, 0x397A, 2, kT4, 64, 16, 0, -64, 0, 0x70},      // 103: C05/086 -> A17/002
    {35, 0, 0x34ED, 0x34BE, 2, kT4, 128, 128, 0, -16, 0, 0x7F},    // 203: A19/002 -> A17/000
    {35, 0, 0x34E4, 0x34DF, 1, kT4, 64, 16, 0, -64, 0, 0x30},      // 203: 103/057 -> A17/001
    {35, 0, 0x3533, 0x352E, 2, kT4, 64, 16, 0, -64, 0, 0x70},      // 203: C05/086 -> A17/002
    {35, 0, 0x3555, 0x354C, 1, kT4, 64, 64, 0, -16, 0, 0x3F},      // 203: 203/007 -> A17/003
    {47, 0, 0x34ED, 0x34BE, 2, kT4, 128, 128, 0, -16, 0, 0x7F},    // 233: A19/002 -> A17/000
    {47, 0, 0x34E4, 0x34DF, 1, kT4, 64, 16, 0, -64, 0, 0x30},      // 233: 233/002 -> 233/001
    {47, 0, 0x353C, 0x3537, 2, kT4, 64, 16, 0, -64, 0, 0x70},      // 233: C05/086 -> A17/002
    {47, 0, 0x3555, 0x352E, 1, kT4, 64, 64, 0, -4, 0, 0x3F},       // 233: A11/005 -> A11/004
    {47, 1, 0x35A5, 0x3571, 2, kT8, 64, 128, 0, 1, 0, 0x7F},       // 233: A19/006 -> A19/005 (the surf)
    {39, 0, 0x353A, 0x350B, 2, kT4, 128, 128, 0, -16, 0, 0x7F},    // 213: A19/002 -> A17/000
    {39, 0, 0x3531, 0x352C, 1, kT4, 64, 16, 0, -64, 0, 0x30},      // 213: 103/057 -> A17/001
    {39, 0, 0x3723, 0x370A, 2, kT4, 64, 128, 0, -16, 0, 0x7F},     // 213: 213/035 -> 213/034
    {39, 0, 0x3761, 0x375C, 1, kT4, 64, 16, 0, -64, 0, 0x30},      // 213: 213/037 -> 213/036
    {39, 0, 0x3580, 0x357B, 2, kT4, 64, 16, 0, -64, 0, 0x70},      // 213: C05/086 -> A17/002
    {36, 0, 0x288D, 0x285E, 2, kT4, 128, 128, 0, -16, 0, 0x7F},    // 210: A19/002 -> A17/000
    {36, 0, 0x2884, 0x287F, 1, kT4, 64, 16, 0, -64, 0, 0x30},      // 210: 103/057 -> A17/001
    {40, 1, 0x2BCA, 0x285E, 2, kT8, 64, 128, 0, 1, 0, 0x7F},       // 220: A19/006 -> A19/005 (the surf)
    {22, 2, 0x2C87, 0x2C7E, 1, kT4, 64, 64, 16, -16, 0x7F, 0x3F},  // 111: 111/001 -> 111/000 (running lights)
    {22, 2, 0x2CEC, 0x2CA8, 2, kT8, 128, 128, 0, 16, 0x1FF, 0xFF}, // 111: 111/003 -> 111/002 (the glow)
    {38, 1, 0x2CB2, 0x2C7E, 2, kT8, 64, 128, 0, 1, 0, 0x7F},       // 211: A19/006 -> A19/005 (the surf)
    {42, 1, 0x2CB2, 0x2C7E, 2, kT8, 64, 128, 0, 1, 0, 0x7F},       // 221: A19/006 -> A19/005 (the surf)
    {37, 0, 0x30CD, 0x309E, 2, kT4, 128, 128, 0, -16, 0, 0x7F},    // 212: A19/002 -> A17/000
    {37, 0, 0x30C4, 0x30BF, 1, kT4, 64, 16, 0, -64, 0, 0x30},      // 212: 103/057 -> A17/001
    {41, 0, 0x31A9, 0x3190, 2, kT4, 64, 128, 0, -16, 0, 0x7F},     // 222: 213/035 -> 213/034
    {41, 0, 0x31E7, 0x31E2, 1, kT4, 64, 16, 0, -64, 0, 0x30},      // 222: 213/037 -> 213/036
    {45, 0, 0x3263, 0x324A, 2, kT4, 64, 128, 0, -16, 0, 0x7F},     // 232: 213/035 -> 213/034
    {45, 0, 0x32A1, 0x329C, 1, kT4, 64, 16, 0, -64, 0, 0x30},      // 232: 213/037 -> 213/036
};
const int kSails = 22;
// D_002C43B0: how far each of a palm's three fronds swings, in degrees; a thirtieth of it a frame.
const float kSwing[3] = {8.0f, 8.0f, 3.0f};
// D_002C4B40: the 32 numbers func_0025D028 starts the fronds from.
const int kScatter[32] = {2, -4, -10, 5, 3, 14, 1, -5, -1, 10, -6, 12, 4, -16, 8, 16,
                          -14, 15, -2, 9, -3, -11, -12, 6, -9, -15, 7, -7, 13, -8, 11, -13};

// The things to pick up (func_0025C528, func_0025C300, func_0025C1D8, func_0025C818, func_0025CA08, func_0025D3D8,
// func_0025D4E8, and Peach_Draw 0x0025BE98's wallet): each is drawn, object n of its block at its place there, while the player has neither the item
// (kind 15) nor the stamp that ends its story, and the car coming within `radius` of it gives it (SE 0x302,
// Item_Give). The car's place is the one in its own block, the item's in the item's block, as the game compares
// them. The gems end with stamp 9 (Jones's "Collected 7 gemstones!"); the last two of blocks 33 and 38 go by course
// objects 1 and 2 (D_0177EB50, D_0177EB90), which Course_DrawObjects draws as object 0 of the block in slot 1 or 2:
// their own.
struct Pickup {
    int block, item, stamp;
    float place[3];
    float radius;
    int n;
};
const Pickup kPickups[] = {
    {36, 0x2B, 80, {1292.0f, 122.0f, 890.0f}, 1.5f, 0},     // the Fluffy Mushroom (D_002C4160)
    {35, 0x26, 9, {400.0f, 354.0f, 1240.0f}, 1.5f, 0},      // a gem (D_002C4170, D_002C4190)
    {35, 0x25, 9, {1040.0f, 162.28f, 110.0f}, 2.0f, 1},     // a gem (D_002C4180)
    {47, 0x2C, 84, {451.14f, 0.0f, 561.0f}, 2.5f, 0},       // the love letter (D_002A2BE0)
    {47, 0x24, 9, {765.77f, 40.63f, 1207.7f}, 2.5f, 1},     // a gem (D_002C41B0)
    {47, 0x2A, 64, {1107.8f, 40.38f, 875.43f}, 1.5f, 2},    // the Papu Flower (D_002C41C0)
    {6, 0x22, 9, {130.0f, -37.11f, 1530.0f}, 1.5f, 0},      // a gem (D_002C41D0)
    {5, 0x28, 39, {485.0f, 45.0f, 1210.0f}, 1.5f, 0},       // Benji's fountain pen (D_002C41F0)
    {5, 0x20, 9, {700.0f, 90.48f, 1000.0f}, 1.5f, 1},       // a gem (D_002C4200)
    {38, 0x23, 9, {1360.0f, 3.5f, 1260.0f}, 1.5f, 0},       // a gem (D_002C4230)
    {33, 0x21, 9, {540.0f, -9.5f, 980.0f}, 1.5f, 0},        // a gem (D_002C4240)
    {43, 0x17, 5, {437.5f, 30.2f, 535.0f}, 1.5f, 2},        // Tim's wallet (D_002C4120), till the police have it
};
// func_0025C818's other object: block 6's object 1 at D_002C41E0, turning about y a degree a frame ((frame % 360 -
// 180) degrees), outside 4:00-20:00 (the clock 36000-179999).
const int kLightBlock = 6;
// Block 47's scroll handler (0x0023F900) moves the love letter with the tide: x = 451.14 + tide x 0.004.
const int kLetterItem = 0x2C;
const float kLetterTide = 0.004f;
const float kLightPlace[3] = {537.5f, 46.12f, 1499.0f};
// Fuji City's offering box (block 23): func_0025D2C8 draws course object 3, the block's object 0, at D_002A2BD0
// (542.5, 48, z); its block task func_00240CD8 slides it from z 1003 (D_00332EE0) to 994 (D_00332EE4), 0.02 a frame
// (D_00332EE8). Its zones (D_002A3880-D_002A3900, func_00240BE0: inside all four edges), in block 23's units: in
// front of it, where the car stops (3880); the shut box (38A0) and the open one (38C0), which throw a car back; the
// strip in front, which pushes a car out by day (38E0); and the box's own place, which puts a car back in front
// (3900, to D_0177AC50 +0xA0 = (0x7AF65A1, 0xF5BF5, 0x74429C9), the world's fixed point).
const int kBoxBlock = 23;
const float kBoxPlace[2] = {542.5f, 48.0f};
const float kBoxShut = 1003.0f, kBoxOpen = 994.0f, kBoxSlide = 0.02f;
const float kBoxStop[8] = {543.5f, 1013.0f, 543.5f, 1014.5f, 545.75f, 1014.5f, 545.75f, 1013.0f};
const float kBoxShutZone[8] = {541.5f, 1002.0f, 541.5f, 1012.5f, 547.75f, 1012.5f, 547.75f, 1002.0f};
const float kBoxOpenZone[8] = {541.5f, 993.0f, 541.5f, 1004.0f, 547.75f, 1004.0f, 547.75f, 993.0f};
const float kBoxFront[8] = {542.0f, 1012.5f, 542.0f, 1014.5f, 547.25f, 1014.5f, 547.25f, 1012.5f};
const float kBoxInside[8] = {542.0f, 1002.5f, 542.0f, 1012.0f, 547.25f, 1012.0f, 547.25f, 1002.5f};
const int32_t kBoxOut[3] = {0x7AF65A1, 0xF5BF5, 0x74429C9};
// The Guard's post (car slot 1's +0xA0, D_0177AC50 + 0x310): Fuji City's first town car, T02_00, in front of the box.
const int32_t kGuardPost[3] = {0x7AE3D70, 0xF5BF5, 0x74428F4};
const float kBoxLow = 50.0f;   // only a car below this height counts
const int kBoxCars = 12;       // the player's and the town cars' slots

// func_00240BE0: (x, z) inside the quad of `q` (four corners x, z), every edge's cross product > 0.
bool InQuad(const float* q, const float* p) {
    for (int k = 0; k < 4; ++k) {
        const float* a = q + 2 * k;
        const float* b = q + 2 * ((k + 1) & 3);
        if (!((b[1] - a[1]) * (p[0] - b[0]) - (b[0] - a[0]) * (p[2] - b[1]) > 0.0f)) return false;
    }
    return true;
}

// The box throws a car back: its speed three quarters (truncated), its place back by twice its speed's step, the
// rpm 0.
void Box_ThrowBack(CarBody& c) {
    const int32_t kWorld = 0x0FFFFFFF;
    int32_t step[3];
    for (int k = 0; k < 3; ++k) {
        step[k] = (c.vel[k] << 5) / 25;
        c.vel[k] = (int32_t)(c.vel[k] * 0.75f);
    }
    c.rpm = 0;
    c.fixed[0] = (c.fixed[0] - 2 * step[0]) & kWorld;
    c.fixed[1] -= 2 * step[1];
    c.fixed[2] = (c.fixed[2] - 2 * step[2]) & kWorld;
}

// func_0025C088, block 11 (My City): its three objects, unturned, at D_002C4130/40/50: object 0 once Ms. Flower's
// flower has bloomed (stamp 41), object 1 during Q's Rally (event flag 0x50), object 2, the statue, with stamp 50.
const int kCityBlock = 11;
const float kCityPlaces[3][3] = {{1180.5f, 26.0f, 699.5f}, {879.1f, 30.0f, 966.4f}, {1238.9f, 27.65f, 1035.9f}};

// The big peach (Peach_FallTask, Peach_Draw): block 0x2B's object 0. The tree (D_002A3870) is bumped within 4.3
// (D_00332EA8) at 10 km/h; the peach starts at D_00332EB0-EB8, falls 0.2 a frame (D_00332EAC) and 0.005 faster each
// (D_00332EBC), drifting 0.15 to -z (D_00332EC0), until it's 0.5 under the water's top. Floating, it goes 0.2 a frame
// along +x (D_00332EC8), bobs 0.005 a step of its counter (D_00332EC4) and zigzags across z, 0.01 to +z (D_00332ECC)
// or 0.1 to -z (D_00332ED0), turning where the ground there isn't the river's (surfaces 0x100651 and 0x101651). At
// x 1920, out at sea, it sinks: 0.2 along x (D_00332ED4), 0.01 down (D_00332ED8), and the same zigzag without the
// turns (D_00332ED8, D_00332EDC), till it's down at the ground.
const int kPeachBlock = 0x2B;
const float kPeachTree[3] = {559.9f, 28.0f, 1501.7f};
const float kPeachStart[3] = {562.4f, 44.04f, 1496.4f};
const float kPeachSea = 1920.0f;
const int32_t kRiver = 0x100651, kRiverDeep = 0x101651;

float Distance(const float* a, const float* b) {
    float dx = a[0] - b[0], dy = a[1] - b[1], dz = a[2] - b[2];
    return sqrtf(dx * dx + dy * dy + dz * dz);
}

bool StampEarned(int s) { return Stamp_Earned(s); }

// sceVu0RotMatrixZ / X by `a`, at (x, y, z): rows of a 3x4 (column k = where axis k goes).
void TurnZ(float a, const float* at, float* m) {
    float c = cosf(a), s = sinf(a);
    const float r[12] = {c, -s, 0, at[0], s, c, 0, at[1], 0, 0, 1, at[2]};
    std::copy(r, r + 12, m);
}
void TurnX(float a, const float* at, float* m) {
    float c = cosf(a), s = sinf(a);
    const float r[12] = {1, 0, 0, at[0], 0, c, -s, at[1], 0, s, c, at[2]};
    std::copy(r, r + 12, m);
}
void TurnY(float a, const float* at, float* m) {
    float c = cosf(a), s = sinf(a);
    const float r[12] = {c, 0, s, at[0], 0, 1, 0, at[1], -s, 0, c, at[2]};
    std::copy(r, r + 12, m);
}

}  // namespace

void FieldObjects::Reset() {
    tide_ = 0;
    ebb_ = false;
    // func_0025D028 state 1: the odd palms start swinging back; each starts at -(|(D_002C4B40[k & 31] << 12) + k| mod
    // the count) / its swing, which can be past the swing's end (it turns back on the first frame).
    auto start = [](Frond (*fronds)[kFronds], int count) {
        for (int k = 0; k < count; ++k) {
            float f = (float)abs(((kScatter[k & 31] << 12) + k) % count);
            for (int j = 0; j < kFronds; ++j) {
                fronds[k][j].back = (k & 1) != 0;
                fronds[k][j].angle = -(f / kSwing[j]);
            }
        }
    };
    start(palms40_, kPalms40);
    start(palms42_, kPalms42);
}

void FieldObjects::Object_Draw(gfx::Draw2D& d, const gfx::Camera3D& camera, const FieldWorld& world, int block,
                               int objBlock, int n, float* rows, float reach) {
    const gfx::Model* model = world.Field_Object(objBlock, n);
    if (!model) return;
    // func_00221288: the place in the block, moved by where that block lies from the car's (D_0029D080).
    float dx, dz;
    FieldWorld::Field_BlockOffset(block, objBlock, &dx, &dz);
    rows[3] += dx;
    rows[11] += dz;
    float lo[3], hi[3];
    if (model->Bounds(-1, lo, hi)) {
        float r2 = 0, e2 = 0;
        for (int k = 0; k < 3; ++k) {
            float e = std::max(fabsf(lo[k]), fabsf(hi[k]));
            r2 += e * e;
            float p = rows[k * 4 + 3] - camera.eye[k];
            e2 += p * p;
        }
        float out = sqrtf(e2) - sqrtf(r2);
        if (out > reach) return;
    }
    style_.transform = rows;
    model->Draw(d, camera, kFrame, style_);
}

void FieldObjects::Palm_Draw(gfx::Draw2D& d, const gfx::Camera3D& camera, const FieldWorld& world, int block,
                             int objBlock, const float (*places)[3], Frond (*fronds)[kFronds], int count,
                             float reach) {
    for (int k = 0; k < count; ++k) {
        for (int j = 0; j < kFronds; ++j) {
            // Down a thirtieth of its swing a frame to -swing, then back up to 0 (the turn takes the same frame).
            Frond& f = fronds[k][j];
            if (!f.back) {
                f.angle -= kSwing[j] / 30.0f;
                if (f.angle < -kSwing[j]) f.back = true;
            }
            if (f.back) {
                f.angle += kSwing[j] / 30.0f;
                if (f.angle >= 0.0f) f.back = false;
            }
            float m[12];
            TurnX(f.angle * kPi / 180.0f, places[k], m);
            Object_Draw(d, camera, world, block, objBlock, j, m, reach);
        }
    }
}

void FieldObjects::Peach_Step(const FieldWorld& world, int block, const float* carPos, float speedKmh) {
    if (!world.Loaded(kPeachBlock)) {
        peachTask_ = false;   // the block's task ends with it (state 5, "PEACH_DETACH")
        return;
    }
    if (!peachTask_) {
        peachTask_ = true;
        peachState_ = 0;
        peachBob_ = 0;
        peachBobDown_ = true;
        peachAcross_ = true;
    }
    // The ground under the peach where it is now: its height and the water's top.
    float q[4] = {peach_[0], peach_[1], peach_[2], 1.0f}, ceiling;
    world.Field_Ground(kPeachBlock, q, &ceiling);
    const float ground = q[1], water = q[3];
    auto river = [&]() {
        float r[4] = {peach_[0], peach_[1], peach_[2], 1.0f};
        int32_t flags = world.Field_Ground(kPeachBlock, r, &ceiling);
        return flags == kRiver || flags == kRiverDeep;
    };
    if (!peachBobDown_) {
        if (++peachBob_ >= 60) peachBobDown_ = true;
    } else if (--peachBob_ <= 0) {
        peachBobDown_ = false;
    }
    switch (peachState_) {
    case 0:
        if (block == kPeachBlock && Distance(kPeachTree, carPos) < 4.3f && speedKmh >= 10.0f) {
            audio::Sound_PlaySe(0x14);
            peachState_ = 1;
            std::copy(kPeachStart, kPeachStart + 3, peach_);
            peachFall_ = 0.2f;
        }
        break;
    case 1:
        peach_[1] -= peachFall_;
        peach_[2] -= 0.15f;
        peachFall_ += 0.005f;
        if (peach_[1] <= water - 0.5f) {
            audio::Sound_PlaySe(0x31C);
            peachState_ = 2;
            peachAcross_ = true;
            peachBobDown_ = true;
        }
        break;
    case 2:
        peach_[0] += 0.2f;
        peach_[1] = water - 0.5f + peachBob_ * 0.005f;
        if (Distance(peach_, carPos) < 1.5f) {
            audio::Sound_PlaySe(0x302);
            Item_Give(15, 0x2F);
            peachState_ = 0;
        }
        if (!peachAcross_) {
            peach_[2] += 0.01f;
            if (!river()) {
                peach_[2] -= 0.01f;
                peachAcross_ = !peachAcross_;
            }
        }
        if (peachAcross_) {
            peach_[2] -= 0.1f;
            if (!river()) {
                peach_[2] += 0.1f;
                peachAcross_ = !peachAcross_;
            }
        }
        if (peach_[0] >= kPeachSea) peachState_ = 4;
        break;
    case 4:
        peach_[0] += 0.2f;
        peach_[1] -= 0.01f;
        peach_[2] += peachAcross_ ? -0.1f : 0.01f;
        if (peach_[1] <= ground) peachState_ = 0;   // gone: the tree can be bumped again
        break;
    }
}

void FieldObjects::MoneyBox_Step(int block, int32_t clock, CarBody* const* cars, int count, CarBody* guard) {
    if (block < 0) {   // block 23 isn't loaded: no task (it starts over when the block comes in)
        boxTask_ = false;
        return;
    }
    if (!boxTask_ || Stamp_Earned(17)) {   // the task's first frame, or Otomi saved: shut for good
        boxZ_ = kBoxShut;
        boxState_ = 0;
        boxTask_ = true;
        if (guard)
            for (int k = 0; k < 3; ++k) guard->fixed[k] = kGuardPost[k];
    }
    const bool night = (uint32_t)(clock - 0xD2F0) > 0x2327Fu;   // outside 0xD2F0-0x3056F
    CarBody& you = *cars[0];
    if (count > kBoxCars) count = kBoxCars;
    switch (boxState_) {
    case 0:
        if (block != kBoxBlock) break;
        if (!Save_EventFlag(0x50) && !Stamp_Earned(17) && night && InQuad(kBoxStop, you.pos) && you.pos[1] < kBoxLow) {
            boxState_ = 1;
            Car_Stop(you);
        }
        for (int i = 0; i < count; ++i)
            if (InQuad(kBoxShutZone, cars[i]->pos) && cars[i]->pos[1] < kBoxLow) Box_ThrowBack(*cars[i]);
        if ((Stamp_Earned(17) || !night) && InQuad(kBoxFront, you.pos)) {
            you.rpm = 0;
            int32_t step = (you.vel[2] << 5) / 25;
            you.fixed[2] = (you.fixed[2] + 2 * (step < 0 ? -step : step)) & 0x0FFFFFFF;
        }
        if (InQuad(kBoxInside, you.pos) && you.pos[1] < kBoxLow)
            for (int k = 0; k < 3; ++k) you.fixed[k] = kBoxOut[k];
        break;
    case 1:
        if (boxZ_ > kBoxOpen) boxZ_ -= kBoxSlide;
        else boxState_ = 2;
        break;
    case 2:
        if (block != kBoxBlock) break;
        for (int i = 0; i < count; ++i)
            if (InQuad(kBoxOpenZone, cars[i]->pos) && cars[i]->pos[1] < kBoxLow) Box_ThrowBack(*cars[i]);
        break;
    }
    // By day, once Otomi is saved, or in Q's Rally, with the player in block 23, the Guard is put back at his post
    // every frame (0x00241230): he can't be pushed away.
    if (guard && (Stamp_Earned(17) || !night || Save_EventFlag(0x50)) && block == kBoxBlock)
        for (int k = 0; k < 3; ++k) guard->fixed[k] = kGuardPost[k];
}

void FieldObjects::Field_DrawObjects(gfx::Draw2D& d, const gfx::Camera3D& camera, const RaceLight& light,
                                     const FieldWorld& world, int block, int frame, float reach, const float* carPos,
                                     int32_t clock) {
    gfx::DrawSourceScope source(d.GetRenderer(), gfx::DrawSource::Objects);
    // Course_DrawObjects' state before them (D_0029D240, the cars' light matrix and colour set 0).
    style_ = Course_ObjectStyle(light);
    lighting_ = Light_CarSet(light, 0);
    style_.lighting = &lighting_;
    style_.cpu = true;   // a few triangles each: all the fronds (or sails) go as one draw call
    if (world.Loaded(kSailBlock)) {
        // func_0025D5F8: each sail turns about z, a degree a frame, a quarter or a half (by its number & 3).
        for (int i = 0; i < kSails; ++i) {
            float a;
            switch (i & 3) {
            case 0: a = (frame % 360 - 180) * kPi / 180.0f; break;
            case 1: a = (frame % 1440 - 720) * kPi / 720.0f; break;
            default: a = (frame % 720 - 360) * kPi / 360.0f; break;
            }
            float m[12];
            TurnZ(a, kSailPlaces[i], m);
            Object_Draw(d, camera, world, block, kSailBlock, 0, m, reach);
        }
    }
    if (world.Loaded(kPalmBlock40))
        Palm_Draw(d, camera, world, block, kPalmBlock40, kPalmPlaces40, palms40_, kPalms40, reach);
    if (world.Loaded(kPalmBlock42))
        Palm_Draw(d, camera, world, block, kPalmBlock42, kPalmPlaces42, palms42_, kPalms42, reach);
    style_.cpu = false;
    for (const Pickup& p : kPickups) {
        if (!world.Loaded(p.block) || Save_HasItem(15, p.item) || StampEarned(p.stamp)) continue;
        float place[3] = {p.place[0], p.place[1], p.place[2]};
        if (p.item == kLetterItem) place[0] += tide_ * kLetterTide;
        float dx = place[0] - carPos[0], dy = place[1] - carPos[1], dz = place[2] - carPos[2];
        if (sqrtf(dx * dx + dy * dy + dz * dz) < p.radius) {
            audio::Sound_PlaySe(0x302);
            Item_Give(15, p.item);
        }
        float m[12] = {1, 0, 0, place[0], 0, 1, 0, place[1], 0, 0, 1, place[2]};
        Object_Draw(d, camera, world, block, p.block, p.n, m, reach);
    }
    // Peach_Draw: the peach while it falls and floats, every other frame as it sinks.
    if (peachTask_ && world.Loaded(kPeachBlock) && peachState_ != 0 && (peachState_ != 4 || (frame & 1))) {
        float m[12] = {1, 0, 0, peach_[0], 0, 1, 0, peach_[1], 0, 0, 1, peach_[2]};
        Object_Draw(d, camera, world, block, kPeachBlock, 0, m, reach);
    }
    if (world.Loaded(kBoxBlock)) {
        // func_0025D2C8: the box (course object 3, block 23's object 0) where its task has it.
        float m[12] = {1, 0, 0, kBoxPlace[0], 0, 1, 0, kBoxPlace[1], 0, 0, 1, boxZ_};
        Object_Draw(d, camera, world, block, kBoxBlock, 0, m, reach);
    }
    if (world.Loaded(kCityBlock)) {
        const bool shown[3] = {StampEarned(41), Save_EventFlag(0x50), StampEarned(50)};
        for (int n = 0; n < 3; ++n) {
            if (!shown[n]) continue;
            const float* p = kCityPlaces[n];
            float m[12] = {1, 0, 0, p[0], 0, 1, 0, p[1], 0, 0, 1, p[2]};
            Object_Draw(d, camera, world, block, kCityBlock, n, m, reach);
        }
    }
    // func_0025D328: the UFO at its place, never turned, unlit (program 0x50: the vertex colours as they are); not
    // while it's away (states 6 and 7) or during Q's Rally (event flag 0x50).
    if (world.Loaded(kUfoBlock) && ufoTask_ && ufoState_ < 6 && !Save_EventFlag(0x50)) {
        float m[12] = {1, 0, 0, ufo_[0], 0, 1, 0, ufo_[1], 0, 0, 1, ufo_[2]};
        style_.lighting = nullptr;
        Object_Draw(d, camera, world, block, kUfoBlock, 0, m, reach);
        style_.lighting = &lighting_;
    }
    if (world.Loaded(kLightBlock) && (uint32_t)(clock - 36000) > 143999u) {
        float m[12];
        TurnY((frame % 360 - 180) * kPi / 180.0f, kLightPlace, m);
        Object_Draw(d, camera, world, block, kLightBlock, 1, m, reach);
    }
}

void FieldObjects::Ufo_Step(bool loaded, int32_t clock) {
    if (!loaded) {
        // Field_ObjectsTask writes D_002A3950[0] (7) when block 22 goes: the task ends, to start over next time.
        ufoTask_ = false;
        ufoState_ = 7;
        return;
    }
    if (!ufoTask_) {
        // The task's first run: home, and the state the clock is in.
        ufoTask_ = true;
        ufoWay_ = 5;
        ufoZ_ = 180;
        ufoX_ = 0;
        for (int k = 0; k < 3; ++k) ufo_[k] = kUfoHome[k];
        const uint32_t day = (uint32_t)(clock - kUfoMorning);
        if ((uint32_t)(clock - kUfoArrive) < (uint32_t)kUfoArriveSpan) ufoState_ = 1;
        if (day > (uint32_t)kUfoNightSpan) ufoState_ = 4;
        if (day <= (uint32_t)kUfoDaySpan) {
            ufoState_ = 6;
            ufo_[1] = kUfoAway;
        }
    }
    switch (ufoState_) {
    case 0:   // coming down over its pad
        if ((ufo_[1] -= kUfoSink) <= kUfoHome[1]) ufoState_ = 1;
        break;
    case 1:   // sliding out to the circle's start
        if (ufo_[2] > kUfoCircleZ && (ufo_[2] -= kUfoSlide) <= kUfoCircleZ) {
            ufo_[2] = kUfoCircleZ;
            ufoState_ = 2;
        }
        break;
    case 2:
        // Round its pad: two counters ping-pong 0-360 a quarter turn apart, the velocity a sampled sine (a circle of
        // about 71 in 720 frames).
        if (ufoWay_ & 1) {
            if (++ufoX_ >= 360) ufoWay_ = (uint8_t)((ufoWay_ | 2) & ~1);
        } else if (ufoWay_ & 2) {
            if (--ufoX_ <= 0) ufoWay_ = (uint8_t)((ufoWay_ | 1) & ~2);
        }
        if (ufoWay_ & 4) {
            if (++ufoZ_ >= 360) ufoWay_ = (uint8_t)((ufoWay_ | 8) & ~4);
        } else if (ufoWay_ & 8) {
            if (--ufoZ_ <= 0) ufoWay_ = (uint8_t)((ufoWay_ | 4) & ~8);
        }
        ufo_[2] += (ufoZ_ - 180) * kPi / 720.0f;
        ufo_[0] += (ufoX_ - 180) * kPi / 720.0f;
        if (clock > kUfoLeaveCircle) ufoState_ = 3;
        break;
    case 3:   // back over the pad
        for (int k = 0; k <= 2; k += 2) {
            const float h = kUfoHome[k];
            if (ufo_[k] < h && (ufo_[k] += kUfoReturn) >= h) ufo_[k] = h;
            if (ufo_[k] > h && (ufo_[k] -= kUfoReturn) <= h) ufo_[k] = h;
        }
        if (ufo_[0] == kUfoHome[0] && ufo_[2] == kUfoHome[2]) {
            ufoState_ = 4;
            ufoZ_ = 180;
            ufoWay_ = 5;
            ufoX_ = 0;
        }
        break;
    case 4:   // the night over its door
        if ((uint32_t)(clock - kUfoMorning) <= (uint32_t)kUfoDaySpan) ufoState_ = 5;
        break;
    case 5:   // leaving
        if ((ufo_[1] += kUfoRise) >= kUfoAway) ufoState_ = 6;
        break;
    case 6:   // away until 17:00
        if (clock > kUfoArrive - 1) ufoState_ = 0;
        break;
    }
}

void FieldObjects::Load(gfx::Draw2D& d, const std::string& dataDir) {
    if (scrollsLoaded_) return;
    scrollsLoaded_ = true;
    const std::string textures = dataDir + "\\textures\\";
    int row22 = 0;
    for (const FieldCopyRow& r : kFieldCopies) {
        const std::string dir = "FLD\\" + FieldWorld::Field_BlockFile(r.block);
        const TexCopy copy = {r.sbp, r.dbp, (uint8_t)r.bw, (uint8_t)r.psm, r.w, r.h,
                              r.speedX, r.speedY, r.maskX, r.maskY};
        FieldScroll s;
        s.blocks = 1ull << r.block;
        s.kind = r.kind;
        s.row = r.kind == 2 ? row22++ : 0;
        if (!Tex_LoadAnimation(d, textures, dir, ReadManifest(textures + dir + ".BIN.txt"), copy, s.anim)) continue;
        // Every block that writes a picture writes it alike (the one task's count or tide): one copy each, run
        // while any of those blocks is loaded (the water, A17/000, is written by six).
        bool seen = false;
        for (FieldScroll& o : scrolls_)
            if (o.anim.target == s.anim.target) {
                o.blocks |= s.blocks;
                seen = true;
            }
        if (!seen) scrolls_.push_back(s);
    }
}

void FieldObjects::Field_Scrolls(gfx::Draw2D& d, const FieldWorld& world, int frame) {
    // Field_ObjectsTask: the tide first, then each loaded block's copies.
    Lagoon_TideStep(tide_, ebb_);
    const bool rally = Save_EventFlag(0x50);
    for (FieldScroll& s : scrolls_) {
        bool loaded = false;
        for (int b = 0; b < 64 && !loaded; ++b) loaded = (s.blocks >> b & 1) && world.Loaded(b);
        if (!loaded) continue;
        const TexCopy& c = s.anim.copy;
        if (s.kind == 1) {
            Tex_Copy(d, s.anim, 0, Tex_Step(tide_, c.speedY, c.maskY));
        } else if (s.kind == 2) {
            // Block 22 (0x0023FCB0): the running lights and the glow; during Q's Rally the pictures' blank halves.
            if (s.row == 0)
                Tex_Copy(d, s.anim, rally ? ((c.speedX << 6) / 16 & c.maskX) : 0,
                         Tex_Step(frame, c.speedY, c.maskY));
            else
                Tex_Copy(d, s.anim, 0, rally ? ((c.speedY << 7) / 16 & c.maskY) : 0);
        } else {
            Tex_Copy(d, s.anim, 0, Tex_Step(frame, c.speedY, c.maskY));
        }
    }
}

void FieldObjects::Restore(gfx::Draw2D& d) {
    for (FieldScroll& s : scrolls_) Tex_Restore(d, s.anim);
}

}  // namespace game
