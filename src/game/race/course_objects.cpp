#include "course_objects.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include <map>

#include "../../audio/sound.h"
#include "../save/save_data.h"
#include "course.h"

namespace game {

namespace {

const float kPi = 3.14159265f;

// The copies' rows: Temple Raceway (D_002A2BF0), Night Glow Raceway (D_002A2C90), Lagoon Raceway (D_002A2E10),
// the Obstacle Course (D_002A2F30), and the Adventure's courses and events: C03 (D_002A2C50), C04 (D_002A2C70), C10
// (D_002A2CF0), C12 (D_002A2D50), C13 (D_002A2DF0), the Highway Race (A02, D_002A2F10), A13 (D_002A2F90) and A19
// (D_002A3010).
// {sbp, dbp, bw, psm, w, h, speed x, speed y, mask x, mask y}; psm 0x14 T4, 0x13 T8. The sources are the
// destinations' pictures twice as tall (or Temple's four frames of caustics), so the copy scrolls or flips them.
struct CopyRow {
    uint16_t sbp, dbp, bw, psm, w, h;
    int16_t speedX, speedY;
    uint16_t maskX, maskY;
};
const CopyRow kTempleCopies[] = {
    {0x31E5, 0x31D3, 4, kT4, 64, 64, 64, 64, 0xC0, 0xC0},   // the pool's caustics, a frame each 8 frames
    {0x31E5, 0x31DC, 4, kT4, 64, 64, 64, 64, 0xC0, 0xC0},
    {0x3221, 0x321E, 1, kT4, 32, 16, 0, 64, 0, 0x70},
};
const CopyRow kNightGlowCopies[] = {
    {0x3A6F, 0x3A66, 2, kT4, 64, 64, 0, -16, 0, 0x3F},
    {0x3A8D, 0x3A88, 1, kT4, 64, 32, 0, -64, 0, 0x30},
    {0x3A4D, 0x3A44, 2, kT4, 64, 64, 0, -2, 0, 0x3F},
};
const CopyRow kLagoonCopies[] = {
    {0x320C, 0x31F8, 2, kT8, 64, 64, 0, -16, 0, 0x3F},  {0x317E, 0x3179, 2, kT4, 64, 16, 0, -64, 0, 0x70},
    {0x30CD, 0x309E, 2, kT4, 128, 128, 0, -16, 0, 0x7F}, {0x30C4, 0x30BF, 1, kT4, 64, 16, 0, -64, 0, 0x30},
    {0x3197, 0x310E, 2, kT4, 64, 64, 0, -4, 0, 0x3F},   {0x3274, 0x3240, 2, kT8, 64, 128, 0, 1, 0, 0x7F},
    {0x31C4, 0x31B0, 2, kT8, 64, 64, 0, -16, 0, 0x3F},  {0x3138, 0x3117, 2, kT4, 128, 128, 0, -8, 0, 0x7F},
};
const CopyRow kObstacleCopies[] = {
    {0x30CD, 0x309E, 2, kT4, 128, 128, 0, 16, 0, 0x7F},
    {0x30C4, 0x30BF, 1, kT4, 64, 16, 0, 64, 0, 0x30},
    {0x3117, 0x310E, 1, kT4, 64, 64, 0, -4, 0, 0x3F},
};
const CopyRow kC03Copies[] = {{0x30F6, 0x30F3, 1, kT4, 32, 16, 0, 64, 0, 0x70}};
const CopyRow kC04Copies[] = {{0x336C, 0x3363, 2, kT4, 64, 64, 0, -16, 0, 0x3F}};
const CopyRow kC10Copies[] = {
    {0x3209, 0x31E8, 2, kT4, 128, 128, 0, -8, 0, 0x7F},
    {0x33B8, 0x33B3, 1, kT4, 64, 32, 0, -64, 0, 0x30},
    {0x31A7, 0x3186, 2, kT4, 128, 128, 0, 8, 0, 0x7F},
};
const CopyRow kC12Copies[] = {
    {0x310F, 0x30CD, 2, kT4, 128, 128, 0, -16, 0, 0x7F}, {0x310F, 0x30EE, 2, kT4, 128, 128, 0, -4, 0, 0x7F},
    {0x316B, 0x3150, 2, kT4, 64, 32, 0, 128, 0, 0x1E0},  {0x31E4, 0x3159, 2, kT4, 64, 32, 0, 128, 0, 0x1E0},
    {0x325D, 0x3162, 2, kT4, 64, 32, 0, 128, 0, 0x1E0},
};
const CopyRow kC13Copies[] = {{0x30D2, 0x309E, 2, kT8, 64, 128, 0, 1, 0, 0x7F}};   // follows the tide
const CopyRow kHighwayCopies[] = {{0x327C, 0x3208, 2, kT8, 64, 256, 0, 256, 0, 0xC0}};
const CopyRow kA13Copies[] = {
    {0x311B, 0x3100, 2, kT4, 64, 32, 0, 128, 0, 0x1E0}, {0x3194, 0x3109, 2, kT4, 64, 32, 0, 128, 0, 0x1E0},
    {0x320D, 0x3112, 2, kT4, 64, 32, 0, 128, 0, 0x1E0}, {0x30BF, 0x309E, 2, kT4, 128, 128, 0, -4, 0, 0x7F},
};
const CopyRow kA19Copies[] = {
    {0x30C8, 0x309E, 2, kT4, 128, 128, 0, -16, 0, 0x7F},
    {0x316A, 0x3136, 2, kT8, 64, 128, 0, 1, 0, 0x7F},   // follows the tide
};
// The Which-way? Race's plate and its fork's two signs (D_002DA030), which its task copies tiles of the atlas (003,
// 512 x 256 at TBP 0x30DA) into every frame: CopyTile.
const CopyRow kWhichWayCopies[] = {
    {0x30DA, 0x309E, 8, kT8, 64, 64, 16, 16, 511, 255},
    {0x30DA, 0x30B2, 8, kT8, 64, 64, 16, 16, 511, 255},
    {0x30DA, 0x30C6, 8, kT8, 64, 64, 16, 16, 511, 255},
};
const uint16_t kLagoonTide = 0x3240;   // the copy (row 5) that follows the tide (Lagoon_WaterTask +2), by its DBP

// Temple's caustics: frame (task +8 >> 3) & 3 takes these x and y steps (D_002A36D8, D_002A36E0) times the speed.
const int8_t kFlipX[4] = {0, 0, 32, 32};
const int8_t kFlipY[4] = {0, 16, 0, 16};

const int kTemple = 2, kNightGlow = 5, kLagoon = 14, kSlidingDoor = 0x13, kObstacle = 0x1D;
const int kC03 = 3, kC04 = 4, kC07 = 7, kC10 = 10, kC12 = 12, kC13 = 13, kHighway = 0x14, kA13 = 0x1F, kA16 = 0x22,
          kA19 = 0x25;
const int kSoccer = 0x1A;   // no course task: Soccer's own task places its ball (object 0)
const int kFirstEvent = 0x12;   // the event courses (ACTION): their games' tasks place objects of their own
const int kWhichWay = 0x1E;     // no course task: its own copies its tiles (CopyTile)
const int kGolf = 0x17;         // the ball's shadow's material (Load)
const uint16_t kC13Tide = 0x309E, kA19Tide = 0x3136;   // the copies that follow the tide, by their DBP

// Each course's copies.
struct CourseCopies {
    int id;
    const CopyRow* rows;
    int count;
};
#define COPIES(id, rows) {id, rows, (int)(sizeof rows / sizeof rows[0])}
const CourseCopies kCopies[] = {
    COPIES(kTemple, kTempleCopies), COPIES(kNightGlow, kNightGlowCopies), COPIES(kLagoon, kLagoonCopies),
    COPIES(kObstacle, kObstacleCopies), COPIES(kC03, kC03Copies), COPIES(kC04, kC04Copies),
    COPIES(kC10, kC10Copies), COPIES(kC12, kC12Copies), COPIES(kC13, kC13Copies),
    COPIES(kHighway, kHighwayCopies), COPIES(kA13, kA13Copies), COPIES(kA19, kA19Copies),
    COPIES(kWhichWay, kWhichWayCopies),
};
#undef COPIES

// C13_ObjectsTask: three objects (models 0-2) swing about x at one place, and the same three about z (turned a
// quarter about y first) at another (D_002A3780: 12 bytes each, the second places 36 bytes on). Object k's angle
// (degrees) starts at 180 and moves kSwingStep[k] a frame: up while the frame % kSwingPeriod[k] is under
// kSwingUp[k], down for the rest (D_002A3760, D_002A3770).
const float kSwingAt[3][3] = {{1035.5f, 118.18f, 767.21f}, {1035.5f, 118.18f, 767.21f}, {1035.5f, 118.18f, 767.21f}};
const float kSwingAt2[3][3] = {{590.59f, 118.18f, 820.44f}, {590.59f, 118.18f, 820.44f}, {590.59f, 118.18f, 820.44f}};
const int16_t kSwingPeriod[3] = {60, 40, 30}, kSwingUp[3] = {30, 20, 15};
const float kSwingStep[3] = {0.4f, 0.5f, 0.6f};

// A16_GateTask: the gate's halves (models 0 and 1) at D_002A37E0, turning about y as they open; closed, a car past
// x 1322 (D_00332E70) is turned back, and past 1322.2 (D_00332E74) put back further. The gate opens with event flag
// 0x5A (func_0023D778), over 360 frames.
const float kGateAt[2][3] = {{1323.8f, 300.0f, 855.3f}, {1323.8f, 300.0f, 839.7f}};
const float kGateLine = 1322.0f, kGateBack = 1322.2f;
const int kGateFlag = 0x5A, kGateOpen = 0x168, kGateCars = 12;

// The doors' places (D_002A3FB0, a door task's +0x1C picks one): 0-5 are course 3's (an Adventure course), 6-12 the
// Sliding Door Race's, across its track at x 295-661, 61 apart.
const float kDoors[13][3] = {
    {409.5f, 10, 95}, {379.5f, 10, 95}, {349.5f, 10, 95}, {319.5f, 10, 95}, {289.5f, 10, 95},
    {260, 10, 95},    {295, 5, 100},    {356, 5, 100},    {417, 5, 100},    {478, 5, 100},
    {539, 5, 100},    {600, 5, 100},    {661, 5, 100},
};
const int kFirstSlidingDoor = 6, kDoorCount = 13;
// A player's car this fast or more (km/h) sounds a door: ACTION 7 coming within 20 in x, 8 within 5.
const int kDoorSpeed = 61;
const uint32_t kSeDoorNear = 0x307, kSeDoorThrough = 0x308;

// Night Glow's rings bob on this (D_002A36E8), a step each 4 frames.
const float kBob[30] = {7.5f, 7.6f, 7.7f, 7.8f, 7.9f, 8.0f, 8.1f, 8.2f, 8.3f, 8.4f, 8.5f, 8.6f, 8.7f, 8.8f, 8.9f,
                        8.9f, 8.8f, 8.7f, 8.6f, 8.5f, 8.4f, 8.3f, 8.2f, 8.1f, 8.0f, 7.9f, 7.8f, 7.7f, 7.6f, 7.5f};

// The rotations sceVu0RotMatrixX/Y give, as rows of a 3x4 transform (column k = where axis k goes), at (x, y, z).
void TurnX(float a, float x, float y, float z, float* m) {
    float c = cosf(a), s = sinf(a);
    const float r[12] = {1, 0, 0, x, 0, c, -s, y, 0, s, c, z};
    memcpy(m, r, sizeof r);
}
void TurnY(float a, float x, float y, float z, float* m) {
    float c = cosf(a), s = sinf(a);
    const float r[12] = {c, 0, s, x, 0, 1, 0, y, -s, 0, c, z};
    memcpy(m, r, sizeof r);
}
// sceVu0RotMatrixY by `ay`, then RotMatrixZ by `az`, at (x, y, z).
void TurnYZ(float ay, float az, float x, float y, float z, float* m) {
    float cy = cosf(ay), sy = sinf(ay), cz = cosf(az), sz = sinf(az);
    // Rz Ry: rows of Rz times the columns of Ry.
    const float r[12] = {cz * cy, -sz, cz * sy, x, sz * cy, cz, sz * sy, y, -sy, 0, cy, z};
    memcpy(m, r, sizeof r);
}

// func_0023D778: event flag n of the save (D_0177FC98, the record's +0x538).
bool EventFlag(int n) { return ((Save_Current().eventFlags[(n >> 6) & 1] >> (n & 63)) & 1) != 0; }

// The race's object lighting state (D_0029D240) and distance cue: as the cars'.
gfx::DrawStyle ObjectStyle(const RaceLight& light) {
    gfx::DrawStyle s;
    s.cue = true;
    s.alphaMax = 128.0f;
    s.fogMax = 255.0f;
    s.scale = 0.5f;
    s.end = gfx::ViewEnd();   // 800, or Graphics Low's 600
    s.fog = true;
    s.fogColor = light.fogColor;
    s.clearDepth = false;
    return s;
}

const gfx::Rect kFrame = {0, 0, gfx::Draw2D::kWidth, gfx::Draw2D::kHeight};

}  // namespace

gfx::DrawStyle Course_ObjectStyle(const RaceLight& light) { return ObjectStyle(light); }

void CourseObjects::Load(gfx::Draw2D& draw, const std::string& dataDir, int id) {
    // The last course's pictures back as their files have them: another course may share them.
    if (draw_) {
        for (TexAnimation& a : animations_) Tex_Restore(*draw_, a);
    }
    draw_ = &draw;
    id_ = id;
    animations_.clear();
    for (gfx::Model& m : models_) m = gfx::Model();
    const CopyRow* rows = nullptr;
    int count = 0;
    for (const CourseCopies& c : kCopies)
        if (c.id == id) rows = c.rows, count = c.count;
    bool objects = id == kTemple || id == kNightGlow || id == kSlidingDoor || id == kObstacle || id == kC03 ||
                   id == kC13 || id == kA16 || (id >= kFirstEvent && id != Course::kTestCourse);   // (C16 has none)
    for (int k = 0; k < kModels; ++k) modelOf_[k] = k;
    if (!rows && !objects) return;
    std::string dir = dataDir + "\\models\\" + Course::ModelDir(id) + "\\";
    for (const gfx::Texture*& t : textureOf_) t = nullptr;
    textureFile_.clear();
    if (objects)
        for (int k = 0; k < kModels; ++k) models_[k].Load(dir + "obj" + std::to_string(k) + ".obj", draw);
    if (id == kGolf) {
        // A05's object 1 (the ball's shadow, a quad on 018's bottom left quarter) has no material in modelconv's file:
        // Course_ClearObjects gives it object 0's picture, 018 (TBP 0x2B6A, black with the shadow's alpha).
        gfx::Mesh ball, shadow;
        if (gfx::LoadObj(dir + "obj0.obj", ball) && gfx::LoadObj(dir + "obj1.obj", shadow) && !ball.parts.empty()) {
            textureFile_ = ball.parts[0].texture;
            for (gfx::MeshPart& p : shadow.parts) p.texture = ball.parts[0].texture;
            models_[1].Load(std::move(shadow), draw);
        }
    }
    std::string textures = dataDir + "\\textures\\";
    std::map<int, ManifestEntry> manifest = ReadManifest(textures + Course::ModelDir(id) + ".BIN.txt");
    for (int i = 0; i < count; ++i) {
        const CopyRow& r = rows[i];
        const TexCopy copy = {r.sbp, r.dbp, (uint8_t)r.bw, (uint8_t)r.psm, r.w, r.h,
                              r.speedX, r.speedY, r.maskX, r.maskY};
        TexAnimation a;
        if (Tex_LoadAnimation(draw, textures, Course::ModelDir(id), manifest, copy, a)) animations_.push_back(a);
    }
    Course_ClearObjects();
}

void CourseObjects::SetModel0Picture(int number) {
    // The course file's pictures are <dir>\NNN.dds: model 0's own file with its number changed.
    size_t slash = textureFile_.find_last_of("\\/");
    if (!draw_ || slash == std::string::npos) return;
    char name[16];
    snprintf(name, sizeof name, "%03d.dds", number);
    textureOf_[0] = &draw_->GetFile(textureFile_.substr(0, slash + 1) + name);
}

void CourseObjects::Course_ClearObjects() {
    frame_ = 0;
    tide_ = 0;
    ebb_ = false;
    gate_ = 0;
    holdPlayer_ = false;
    memset(placed_, 0, sizeof placed_);
    memset(doorState_, 0, sizeof doorState_);
    for (TexAnimation& a : animations_) Tex_Restore(*draw_, a);
}

void CourseObjects::Place(int i, const float rows[12]) {
    memcpy(place_[i], rows, sizeof place_[i]);
    placed_[i] = true;
}

void CourseObjects::Update(const CourseCars& cars) {
    if (!draw_) return;
    ++frame_;
    switch (id_) {
    case kC03:
        // Course_ObjectsTask gives course 3 a scroll (0x00241688) and door tasks for doors 0-5.
        Course_ScrollTask(*draw_);
        for (int door = 0; door < kFirstSlidingDoor; ++door) SlidingDoor_DoorTask(cars, door);
        break;
    case kC04:      // 0x00241760
    case kC10:      // 0x0023E7E0
    case kC12:      // 0x0023E8D0
    case kHighway:  // 0x00241840
    case kA13:      // 0x0023EFB0
        Course_ScrollTask(*draw_);
        break;
    case kC07: break;   // 0x00241838: an empty task
    case kC13: C13_ObjectsTask(*draw_); break;
    case kA16: A16_GateTask(cars); break;
    case kA19: A19_WaterTask(*draw_); break;
    case kTemple: Temple_ObjectsTask(*draw_); break;
    case kNightGlow: NightGlow_ObjectsTask(*draw_); break;
    case kLagoon: Lagoon_WaterTask(*draw_); break;
    case kSlidingDoor:
        // Course_ObjectsTask gives this course an empty task (0x00241BA0) and a door task for each of its doors.
        for (int door = kFirstSlidingDoor; door < kDoorCount; ++door) SlidingDoor_DoorTask(cars, door);
        break;
    case kObstacle: Obstacle_ObjectsTask(*draw_, cars); break;
    default: break;
    }
}

// The caustics flip through their four frames; the third copy scrolls. The water wheel (object 0) turns a degree a
// frame about x at (399, 70.8, 23.8).
void CourseObjects::Temple_ObjectsTask(gfx::Draw2D& d) {
    int flip = (frame_ >> 3) & 3;
    for (TexAnimation& a : animations_) {
        const TexCopy& c = a.copy;
        if (c.dbp == 0x321E) {
            Copy(d, a, 0, Tex_Step(frame_, c.speedY, c.maskY));
        } else {
            Copy(d, a, (kFlipX[flip] * c.speedX / 16) & c.maskX, (kFlipY[flip] * c.speedY / 16) & c.maskY);
        }
    }
    float m[12];
    TurnX((frame_ % 360 - 180) * kPi / 180.0f, 399.0f, 70.8f, 23.8f, m);
    Place(0, m);
}

// The three copies scroll. The ride (object 0) turns about y once in 256 frames at (560, 15.77, 910); its two rings
// of panels (objects 1 and 2) turn once in 352 and 384 frames at (588.39, 143.71) and bob on kBob, half a cycle apart.
void CourseObjects::NightGlow_ObjectsTask(gfx::Draw2D& d) {
    for (TexAnimation& a : animations_) Copy(d, a, 0, Tex_Step(frame_, a.copy.speedY, a.copy.maskY));
    float m[12];
    TurnY(((frame_ & 0xFF) - 0x80) * kPi / 128.0f, 560.0f, 15.77f, 910.0f, m);
    Place(0, m);
    TurnY((frame_ % 0x160 - 0xB0) * kPi / 176.0f, 588.3929f, kBob[(frame_ / 4 + 15) % 30], 143.7135f, m);
    Place(1, m);
    TurnY((frame_ % 0x180 - 0xC0) * kPi / 192.0f, 588.3929f, kBob[(frame_ / 4) % 30], 143.7135f, m);
    Place(2, m);
}

// The water scrolls; the beach's surf (copy 5) follows the tide in and out instead of the frame.
void CourseObjects::Lagoon_WaterTask(gfx::Draw2D& d) {
    if (frame_ == 1) tide_ = 0;
    for (TexAnimation& a : animations_) {
        if (a.copy.dbp == kLagoonTide) {
            Copy(d, a, 0, Tex_Step(tide_, a.copy.speedY, a.copy.maskY));
            Lagoon_TideStep();
        } else {
            Copy(d, a, 0, Tex_Step(frame_, a.copy.speedY, a.copy.maskY));
        }
    }
}

// A door's two halves (objects 0 and 1, 5 wide in z) meet at its place, and slide 5 apart each way as the nearest
// car (in x, within 20 in z) comes from 20 to 5 away. Only a player's car sounds it (car 0, and car 1 in 2 Player),
// going 61 km/h or more within 20 in z: coming within 20 in x, then within 5, then quiet again past 20.
void CourseObjects::SlidingDoor_DoorTask(const CourseCars& cars, int door) {
    const float* p = kDoors[door];
    float best = 1600.0f;
    for (int i = 0; i < cars.count; ++i) {
        if (cars.out[i]) continue;
        const float* c = cars.bodies[i].pos;
        float d = fabsf(p[0] - c[0]);
        if (d < best && p[2] - 20.0f < c[2] && c[2] < p[2] + 20.0f) best = d;
    }
    float t = best < 5.0f ? 0.0f : best > 20.0f ? 1.0f : (best - 5.0f) / 15.0f;

    uint8_t& state = doorState_[door];
    uint8_t old = state;
    for (int i = 0; i < (cars.twoPlayer ? 2 : 1) && i < cars.count; ++i) {
        const CarBody& b = cars.bodies[i];
        // Vec_SpeedKmh (func_0021D358): the velocity's length / 32768 x 216, truncated.
        float v = sqrtf((float)b.vel[0] * b.vel[0] + (float)b.vel[1] * b.vel[1] + (float)b.vel[2] * b.vel[2]);
        if ((int)(v / 32768.0f * 216.0f) < kDoorSpeed) continue;
        float x = b.pos[0], z = b.pos[2];
        if (!(p[2] - 20.0f < z && z < p[2] + 20.0f)) continue;
        if (state != 2 && p[0] - 20.0f <= x && x < p[0] + 20.0f) state = 1;
        if (p[0] - 5.0f <= x && x < p[0] + 5.0f) state = 2;
        if (state == 2 && (p[0] + 20.0f <= x || x < p[0] - 20.0f)) state = 0;
    }
    if (!cars.quiet && state != old) {
        if (state == 1) audio::Sound_PlaySe(kSeDoorNear);
        if (state == 2) audio::Sound_PlaySe(kSeDoorThrough);
    }

    // Matrices 4 n and 4 n + 1 (course 3's doors 0-5: 4 n, but 22 for door 5; the Sliding Door Race's: 4 n - 24).
    int k = door < 5 ? door * 4 : door < 6 ? door * 4 + 2 : door * 4 - 24;
    float open = (1.0f - t) * 5.0f;
    float m[12] = {1, 0, 0, p[0], 0, 1, 0, p[1], 0, 0, 1, p[2] - open - 5.0f};
    Place(k, m);
    m[11] = p[2] + open + 5.0f;
    Place(k + 1, m);
}

// The three copies scroll. In 2 Player object 0 goes up where the file has it: the fence across the track behind
// the start (x 240-241.5), which Race_ObstacleTask also keeps the cars from passing before the first gate.
void CourseObjects::Obstacle_ObjectsTask(gfx::Draw2D& d, const CourseCars& cars) {
    for (TexAnimation& a : animations_) Copy(d, a, 0, Tex_Step(frame_, a.copy.speedY, a.copy.maskY));
    if (cars.twoPlayer) {
        const float m[12] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0};
        Place(0, m);
    }
}

// Each copy scrolls by the frame (the copy's source row (frame x speed) / 16 & mask).
void CourseObjects::Course_ScrollTask(gfx::Draw2D& d) {
    for (TexAnimation& a : animations_) Copy(d, a, 0, Tex_Step(frame_, a.copy.speedY, a.copy.maskY));
}

// The swinging objects (kSwingAt), and the water: a copy that follows the tide as Lagoon's beach does.
void CourseObjects::C13_ObjectsTask(gfx::Draw2D& d) {
    if (frame_ == 1) {
        tide_ = 0;
        for (float& a : swing_) a = 180.0f;
    }
    for (int k = 0; k < 3; ++k) {
        // The angle in whole degrees (truncated to 16 bits), -180..180 about the resting position.
        float a = (float)((int16_t)(int)swing_[k] % 360 - 180) * kPi / 180.0f, m[12];
        TurnX(a, kSwingAt[k][0], kSwingAt[k][1], kSwingAt[k][2], m);
        Place(k, m);
        TurnYZ(-kPi / 2, a, kSwingAt2[k][0], kSwingAt2[k][1], kSwingAt2[k][2], m);   // D_00332E6C
        Place(k + 4, m);
        swing_[k] += frame_ % kSwingPeriod[k] < kSwingUp[k] ? kSwingStep[k] : -kSwingStep[k];
    }
    for (TexAnimation& a : animations_) {
        if (a.copy.dbp != kC13Tide) continue;
        Copy(d, a, 0, Tex_Step(tide_, a.copy.speedY, a.copy.maskY));
        Lagoon_TideStep();
    }
}

// The gate: with event flag 0x5A at the start it stands open; set later, it opens over 360 frames while the
// player's car is held (car flag 0x800). Closed, it turns the cars back.
void CourseObjects::A16_GateTask(const CourseCars& cars) {
    bool open = EventFlag(kGateFlag);
    if (frame_ == 1) gate_ = open ? kGateOpen : 0;
    if (!open) {
        for (int i = 0; i < kGateCars && i < cars.count; ++i) {
            CarBody& b = cars.bodies[i];
            float x = b.pos[0];
            if (kGateLine <= x) {
                // Back by twice this frame's move (16/25 of the velocity), and a quarter of the speed off.
                b.rpm = 0;
                int32_t move[3];
                for (int k = 0; k < 3; ++k) {
                    move[k] = (b.vel[k] << 5) / 25;
                    b.vel[k] = (int32_t)(b.vel[k] * 0.75f);
                }
                b.fixed[0] = (b.fixed[0] - move[0] * 2) & 0x0FFFFFFF;
                b.fixed[1] -= move[1] * 2;
                b.fixed[2] = (b.fixed[2] - move[2] * 2) & 0x0FFFFFFF;
            }
            if (kGateBack <= x) b.fixed[0] = (b.fixed[0] - 0x3E80) & 0x0FFFFFFF;
        }
    }
    holdPlayer_ = open && gate_ < kGateOpen;
    if (holdPlayer_) ++gate_;
    float m[12];
    TurnY((float)((0x2D0 - gate_) % 1440 - 0x2D0) * kPi / 720.0f, kGateAt[0][0], kGateAt[0][1], kGateAt[0][2], m);
    Place(0, m);
    TurnY((float)((gate_ + 0x2D0) % 1440 - 0x2D0) * kPi / 720.0f, kGateAt[1][0], kGateAt[1][1], kGateAt[1][2], m);
    Place(1, m);
}

// The water: the first copy scrolls by the frame, the second follows the tide.
void CourseObjects::A19_WaterTask(gfx::Draw2D& d) {
    for (TexAnimation& a : animations_) {
        if (a.copy.dbp == kA19Tide) {
            Copy(d, a, 0, Tex_Step(tide_, a.copy.speedY, a.copy.maskY));
            Lagoon_TideStep();
        } else {
            Copy(d, a, 0, Tex_Step(frame_, a.copy.speedY, a.copy.maskY));
        }
    }
}

void CourseObjects::Course_DrawObjects(gfx::Draw2D& d, const gfx::Camera3D& camera, const RaceLight& light) const {
    gfx::DrawSourceScope source(d.GetRenderer(), gfx::DrawSource::Objects);
    gfx::DrawStyle st = ObjectStyle(light);
    gfx::Lighting lit = Light_CarSet(light, 0);
    st.lighting = &lit;
    for (int i = 0; i < kInstances; ++i) {
        if (!placed_[i]) continue;
        st.transform = place_[i];
        st.texture = textureOf_[i & 3];
        models_[modelOf_[i & 3]].Draw(d, camera, kFrame, st);
    }
}

}  // namespace game
