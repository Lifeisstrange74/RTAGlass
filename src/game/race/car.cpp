#include "car.h"

#include <windows.h>
#undef far   // (windows.h's; Car_DrawParts has a parameter of the name)
#undef near

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "../save/save_data.h"
#include "../story/race_list.h"
#include "../../game_text.inc"   // the game's text, GT_<address>_<length>, made from the ELF

namespace game {

namespace {

struct OpponentRecord {
    int group, index;   // where the game keeps it: D_002C22C8[group] + 16 index
    CarRecord record;
};

// A Quick Race course's row of D_002BE158: its id, and its opponents (+4) in the order they are entered.
struct QuickRaceCourse {
    int id;
    struct {
        int group, index;
    } opponents[23];
};

// The opponents of the Quick Race (D_002C22C8[group] + 16 index): paint, body, driver.
const OpponentRecord kOpponents[] = {
    {1, 7, {0xF90F90, 122, GT_00333F50_7}},
    {1, 9, {0x0BF0BF, 120, GT_00333F58_4}},
    {1, 10, {0x0D50D5, 16, GT_00333F60_4}},
    {1, 11, {0x48F48F, 136, GT_00333F68_7}},
    {1, 14, {0x58B58B, 92, GT_00333F78_5}},
    {1, 15, {0x0FF00F, 94, GT_00333F80_5}},
    {1, 16, {0x555555, 64, GT_00333F88_6}},
    {1, 28, {0x888888, 91, GT_00333F90_5}},
    {1, 34, {0xBEEF64, 30, GT_00333FC8_5}},
    {1, 35, {0x523523, 21, GT_00333FD0_7}},
    {1, 36, {0x21F21F, 68, GT_00333FD8_6}},
    {1, 38, {0xEFFEFF, 14, GT_00333FE8_6}},
    {2, 7, {0xFFF08F, 27, GT_002FE660_15}},
    {2, 14, {0xEFFEFF, 42, GT_00334010_7}},
    {2, 15, {0x362216, 72, GT_00300150_8}},
    {2, 31, {0x000042, 11, GT_00301660_9}},
    {2, 33, {0x299666, 4, GT_00301B68_8}},
    {2, 34, {0x078462, 109, GT_00334048_6}},
    {3, 9, {0x145145, 107, GT_00334070_4}},
    {3, 11, {0x20A20A, 19, GT_00334080_5}},
    {3, 12, {0xBB1BB1, 25, GT_003054C0_9}},
    {3, 16, {0xD4F682, 37, GT_00334098_6}},
    {3, 18, {0x00F00F, 49, GT_003340A0_5}},
    {3, 33, {0xF9FF9F, 135, GT_003340B8_7}},
    {3, 40, {0x15F15F, 69, GT_00308450_8}},
    {3, 41, {0x035035, 106, GT_00308700_8}},
    {4, 2, {0xE30E30, 114, GT_003340F0_7}},
    {6, 6, {0x0DF000, 36, GT_00334198_6}},
    {6, 17, {0x999FFF, 5, GT_003120E8_11}},
    {6, 27, {0xEFFEFF, 117, GT_003341E0_4}},
    {6, 28, {0x042042, 24, GT_003341E8_7}},
    {6, 29, {0x07F07F, 146, GT_003341F0_6}},
    {6, 30, {0xFFF026, 129, GT_00334200_5}},
    {6, 31, {0x00E00E, 128, GT_00334208_6}},
    {7, 6, {0x031031, 54, GT_003348D0_5}},
    {7, 8, {0x1237AD, 110, GT_003166C8_16}},
    {7, 9, {0x13F13F, 96, GT_00334240_4}},
    {7, 11, {0x0F90F9, 104, GT_00334250_4}},
    {7, 16, {0x0DD040, 66, GT_00334278_5}},
    {7, 28, {0x555555, 41, GT_00334280_4}},
    {7, 29, {0x00FF30, 18, GT_00334288_4}},
    {7, 31, {0x434434, 105, GT_00334298_5}},
    {8, 17, {0xEB5EB5, 103, GT_003342F8_4}},
    {8, 18, {0x00F00F, 98, "Peo"}},
    {8, 19, {0x61105F, 121, "Giz"}},
    {8, 20, {0x12D000, 83, GT_00334310_6}},
    {8, 21, {0x060060, 79, GT_00334318_5}},
    {8, 22, {0xFFAFFA, 85, GT_00334320_5}},
    {8, 23, {0x00B040, 88, "Mug"}},
    {9, 12, {0x05F090, 142, GT_003340C8_5}},
    {9, 15, {0xFFFFFF, 67, GT_003342C0_4}},
    {9, 19, {0x00F00F, 63, GT_00334230_5}},
    {9, 23, {0xFFFFFF, 56, GT_00333FB0_6}},
    {9, 24, {0x11100F, 58, GT_00334170_5}},
    {9, 25, {0x3D03D0, 9, GT_00324E58_9}},
    {9, 26, {0x0CF0CF, 23, GT_00334330_6}},
    {9, 27, {0xA08A08, 93, GT_00334338_6}},
    {9, 28, {0x7FF7FF, 20, GT_00334340_4}},
};

// D_002BE158, the Quick Race course table: course id, then its opponents (+4) as (group, index) pairs. All eight
// share the first 17 and the last; the five between are the course's own.
const QuickRaceCourse kQuickRace[] = {
    {0,
     {{8, 17}, {8, 18}, {1, 28}, {9, 26}, {7, 6}, {4, 2}, {6, 29}, {6, 27}, {9, 27}, {7, 11}, {7, 28}, {3, 12},
      {3, 41}, {9, 28}, {3, 11}, {9, 25}, {2, 7}, {1, 14}, {1, 38}, {1, 15}, {1, 11}, {1, 7}, {1, 34}}},
    {2,
     {{8, 17}, {8, 18}, {1, 28}, {9, 26}, {7, 6}, {4, 2}, {6, 29}, {6, 27}, {9, 27}, {7, 11}, {7, 28}, {3, 12},
      {3, 41}, {9, 28}, {3, 11}, {9, 25}, {2, 7}, {2, 33}, {2, 31}, {2, 34}, {2, 14}, {2, 15}, {1, 34}}},
    {5,
     {{8, 17}, {8, 18}, {1, 28}, {9, 26}, {7, 6}, {4, 2}, {6, 29}, {6, 27}, {9, 27}, {7, 11}, {7, 28}, {3, 12},
      {3, 41}, {9, 28}, {3, 11}, {9, 25}, {2, 7}, {3, 18}, {3, 40}, {3, 16}, {3, 33}, {3, 9}, {1, 34}}},
    {6,
     {{8, 17}, {8, 18}, {1, 28}, {9, 26}, {7, 6}, {4, 2}, {6, 29}, {6, 27}, {9, 27}, {7, 11}, {7, 28}, {3, 12},
      {3, 41}, {9, 28}, {3, 11}, {9, 25}, {2, 7}, {6, 17}, {6, 31}, {6, 28}, {6, 30}, {6, 6}, {1, 34}}},
    {9,
     {{8, 17}, {8, 18}, {1, 28}, {9, 26}, {7, 6}, {4, 2}, {6, 29}, {6, 27}, {9, 27}, {7, 11}, {7, 28}, {3, 12},
      {3, 41}, {9, 28}, {3, 11}, {9, 25}, {2, 7}, {1, 35}, {1, 36}, {1, 9}, {1, 10}, {1, 16}, {1, 34}}},
    {14,
     {{8, 17}, {8, 18}, {1, 28}, {9, 26}, {7, 6}, {4, 2}, {6, 29}, {6, 27}, {9, 27}, {7, 11}, {7, 28}, {3, 12},
      {3, 41}, {9, 28}, {3, 11}, {9, 25}, {2, 7}, {7, 29}, {7, 8}, {7, 16}, {7, 31}, {7, 9}, {1, 34}}},
    {16,
     {{8, 17}, {8, 18}, {1, 28}, {9, 26}, {7, 6}, {4, 2}, {6, 29}, {6, 27}, {9, 27}, {7, 11}, {7, 28}, {3, 12},
      {3, 41}, {9, 28}, {3, 11}, {9, 25}, {2, 7}, {8, 19}, {8, 20}, {8, 21}, {8, 22}, {8, 23}, {1, 34}}},
    {17,
     {{8, 17}, {8, 18}, {1, 28}, {9, 26}, {7, 6}, {4, 2}, {6, 29}, {6, 27}, {9, 27}, {7, 11}, {7, 28}, {3, 12},
      {3, 41}, {9, 28}, {3, 11}, {9, 25}, {2, 7}, {9, 12}, {9, 15}, {9, 23}, {9, 19}, {9, 24}, {1, 34}}},
};

// The player's car by card (D_0029C028), painted as its body comes (D_002ED720[body] + 4). Quick Race keeps no
// other choice: QuickRace_Task copies these into the player's record (D_0177F760).
const CarRecord kQuickRaceCar[] = {
    {0x888888, 26, "Q027"},
    {0x7DF7DF, 66, "Q067"},
    {0xD32D32, 51, "Q052"},
    {0x1EF1EF, 133, "Q134"},
    {0x00C00D, 73, "Q074"},
    {0xEFEEFE, 89, "Q090"},
    {0x666666, 46, "Q047"},
    {0xC10EFF, 87, "Q088"},
};

const CarRecord* FindOpponent(int group, int index) {
    for (const OpponentRecord& o : kOpponents)
        if (o.group == group && o.index == index) return &o.record;
    return nullptr;
}

// Car_LightsUnderWater (func_00226058): the light set as uploaded for a car `depth` units above the water level at
// its front wheels (the level is 0 where there is no water). More than 2 under, every colour (both lights, the
// highlight and the ambient) loses half its red and a hundredth more of each colour a unit deeper: the car turns
// green under water. Red goes no lower than 0, green and blue no lower than a quarter.
void Car_LightsUnderWater(gfx::Lighting& l, float depth) {
    if (!(depth < -2.0f)) return;
    float r = depth * 0.01f + 0.5f, gb = depth * 0.01f + 1.0f;
    if (r < 0) r = 0;
    if (gb < 0.25f) gb = 0.25f;
    for (int i = 0; i < 4; ++i) {
        l.color[i][0] *= r;
        l.color[i][1] *= gb;
        l.color[i][2] *= gb;
    }
}

// Where the tires go on a car (D_0029CAF0, Car_DrawTires 0x002213E8): the front ones and the middle of the rear pair.
const float kTirePos[3][3] = {{-0.74f, 0.33f, 0.68f}, {0.74f, 0.33f, 0.68f}, {0.0f, 0.38f, -0.66f}};
// The big tires' places (D_0029CB20): front left, front right, rear left, rear right; TIRE.BIN section 4 on the left,
// 5 on the right. PARTS.BIN model 10 is the lift between them and the body.
const float kBigTirePos[4][3] = {{-0.4f, 0.55f, 0.68f}, {0.4f, 0.55f, 0.68f}, {-0.4f, 0.55f, -0.66f},
                                 {0.4f, 0.55f, -0.66f}};
// The close-up wheels' places (D_0029CB60, func_00221570): x, y, z, w. The right-hand ones (odd) are turned Rz(pi);
// the rear w scales them up by 1/w (0.33 / 0.38: the ChoroQ's bigger rear wheels, centred where TIRE.BIN's are).
const float kCloseWheelPos[4][4] = {{-0.74f, 0.33f, 0.68f, 1.0f}, {0.74f, 0.33f, 0.68f, 1.0f},
                                    {-0.625f, 0.33f, -0.573f, 0.868421f}, {0.625f, 0.33f, -0.573f, 0.868421f}};
// D_0029D1A0: each wheel colour's paint levels (r, g, b: 0.1 + 0.05 n, D_0029D150); colours 12 on are all 0.
const uint8_t kWheelColorLevels[12][3] = {
    {12, 12, 12}, {2, 2, 2},  {14, 0, 0}, {14, 6, 8},  {0, 7, 0},  {4, 12, 0},
    {1, 3, 14},   {4, 11, 14}, {13, 8, 0}, {12, 12, 1}, {7, 1, 8}, {10, 6, 12},
};
const int kTunnelCourse = 0x1C;   // the Tunnel Race (ACTION/A10)
const int kModelLift = 10;
const int kModelBillboard = 9;
const uint16_t kPartBillboard = 0x200;

#include "car_parts.inc"

// Part 6's propellers (Car_Draw): four of PARTS.BIN model 7, from x -0.63 (D_00332CC8) 0.42 apart (D_00332CC4).
const float kPropellerX = -0.63f, kPropellerStep = 0.42f;
const int kPropellers = 4;
// The parts Car_Draw draws by flag bits 0-8: the place (kPartPlace's column) of each, -1 for part 7, which has none
// of its own (the game's switch skips it, so it sits where the last part drawn did).
const int kPartPlaceOf[9] = {0, 0, 1, 1, 2, 3, 4, -1, 5};
const int kPropellerPart = 6, kModelPropeller = 7, kModelPropellersFar = 11;

// The shadow's height above the ground, so it doesn't fight the road for depth.
const float kShadowLift = 0.02f;

// The cars are drawn with the world's distance cue (programs 0x20 and 0x50 take vf08 = (128, 255, 0.5, 800) for the
// fade and the fog alike) towards the course's fog colour, alpha blended, depth tested (D_0029D240).
gfx::DrawStyle CarStyle(const RaceLight& light) {
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

// A car's place (3x4, as gfx::DrawStyle::transform: column k is the car's axis k, the last column its position):
// from its physics, or turned by `yaw` about the vertical. `y` is its height.
void CarMatrix(const RaceCar& car, float y, float* m) {
    if (car.physics) {
        const CarBody& b = *car.physics;
        for (int r = 0; r < 3; ++r) {
            for (int k = 0; k < 3; ++k) m[r * 4 + k] = b.m[k][r];
            m[r * 4 + 3] = car.pos[r];
        }
        m[7] = y;
        return;
    }
    float s = sinf(car.yaw), c = cosf(car.yaw);
    const float rows[12] = {c, 0, s, car.pos[0], 0, 1, 0, y, -s, 0, c, car.pos[2]};
    for (int i = 0; i < 12; ++i) m[i] = rows[i];
}

// a after b (3x4 transforms: world = a (b p)).
void Compose(const float* a, const float* b, float* out) {
    float r[12];
    for (int i = 0; i < 3; ++i) {
        for (int k = 0; k < 4; ++k)
            r[i * 4 + k] = a[i * 4] * b[k] + a[i * 4 + 1] * b[4 + k] + a[i * 4 + 2] * b[8 + k] + (k == 3 ? a[i * 4 + 3] : 0);
    }
    memcpy(out, r, sizeof r);
}

// m x (translation by t).
void Translate(const float* m, const float* t, float* out) {
    const float tm[12] = {1, 0, 0, t[0], 0, 1, 0, t[1], 0, 0, 1, t[2]};
    Compose(m, tm, out);
}

// The body on its suspension in the car's frame: rows = the body's axes in the car's (CarBody::body), raised by
// the body's height (the game's car +0x40 matrix and +0x74).
void BodyMatrix(const CarBody& b, float* out) {
    for (int r = 0; r < 3; ++r) {
        for (int k = 0; k < 3; ++k) out[r * 4 + k] = b.body[k][r];
        out[r * 4 + 3] = r == 1 ? b.bodyHeight : 0;
    }
}

// A tire at `t` in the car's frame, rolled by `spin` about its axle (x) and turned by `steer` about the vertical
// (Car_DrawTires: identity, sceVu0RotMatrixX, then RotMatrixY, then the translation).
void TireMatrix(const float* t, float spin, float steer, float* out) {
    float cs = cosf(spin), ss = sinf(spin), cy = cosf(steer), sy = sinf(steer);
    // p -> (x, y cs - z ss, y ss + z cs) -> (x cy + z sy, y, -x sy + z cy)
    const float m[12] = {cy, ss * sy, cs * sy, t[0], 0, cs, -ss, t[1], -sy, ss * cy, cs * cy, t[2]};
    memcpy(out, m, sizeof m);
}

// The paint levels of a record (CarRecord::paint) as colours.
void Paint(uint32_t paint, float out[2][3]) {
    for (int i = 0; i < 6; ++i) out[i / 3][i % 3] = 0.1f + 0.05f * ((paint >> (i * 4)) & 0xF);   // D_0029D150
}

const gfx::Rect kFrame = {0, 0, gfx::Draw2D::kWidth, gfx::Draw2D::kHeight};

// func_002230C8: both paints the wheel colour's levels.
void WheelPaint(int wheelColor, float out[2][3]) {
    for (int k = 0; k < 3; ++k) {
        float level = 0.1f + 0.05f * (wheelColor >= 0 && wheelColor < 12 ? kWheelColorLevels[wheelColor][k] : 0);
        out[0][k] = out[1][k] = level;
    }
}

// PSMT4's block order in a page (128 x 128 texels: 4 x 8 blocks of 32 x 16), by block row and column.
const int kBlocksT4[8][4] = {{0, 2, 8, 10},    {1, 3, 9, 11},    {4, 6, 12, 14},   {5, 7, 13, 15},
                             {16, 18, 24, 26}, {17, 19, 25, 27}, {20, 22, 28, 30}, {21, 23, 29, 31}};

// The tires' picture as Car_DrawTires addresses it: TIRE.BIN's T4 picture (TBP 0x1C44) in CLUT +5, the wheel colour,
// starting at TBP 0x1C44 + 2 x +6, the Wheels part. Starting 2w blocks on, every block of the picture reads the
// block 2w further in PSMT4's order: the 32 x 32 corner the tires use (blocks 0 and 1; sections 0-3) shows the hub
// at blocks 2w and 2w + 1, one of the 15 (Spoke 666's skull is the last). What moves past the page reads the CLUT
// and beyond, black here. The big tires (sections 4 and 5) take TBP 0x1C44 itself: `wheel` 0. Null for palette 0
// and wheel 0, the models' own picture. Made once each and kept.
const gfx::Texture* TirePicture(gfx::Draw2D& d, int wheelColor, int wheel) {
    if (wheelColor < 0 || wheelColor > 15) wheelColor = 0;
    if (wheel < 0 || wheel > 15) wheel = 0;
    if (wheel == 0) {
        if (wheelColor == 0) return nullptr;
        const gfx::Texture& t = d.Get("CARS/TIRE.BIN", 0, wheelColor);
        return t.id ? &t : nullptr;
    }
    static const gfx::Texture* made[16][16];
    static gfx::Draw2D* madeFor = nullptr;
    if (madeFor != &d) {
        memset(made, 0, sizeof made);
        madeFor = &d;
    }
    if (made[wheelColor][wheel]) return made[wheelColor][wheel];
    std::vector<uint32_t> src;
    int w = 0, h = 0;
    if (!d.ReadPixels("CARS/TIRE.BIN", 0, wheelColor, src, w, h) || w < 128 || h < 128) return nullptr;
    const int bw = w / 4, bh = h / 8;   // a block, at the picture's own scale
    int place[32][2];
    for (int r = 0; r < 8; ++r)
        for (int c = 0; c < 4; ++c) place[kBlocksT4[r][c]][0] = c, place[kBlocksT4[r][c]][1] = r;
    std::vector<uint32_t> out(src.size(), 0);
    for (int r = 0; r < 8; ++r)
        for (int c = 0; c < 4; ++c) {
            const int b = kBlocksT4[r][c] + 2 * wheel;
            if (b >= 32) continue;
            for (int y = 0; y < bh; ++y)
                memcpy(&out[(size_t)(r * bh + y) * w + c * bw],
                       &src[(size_t)(place[b][1] * bh + y) * w + place[b][0] * bw], bw * sizeof(uint32_t));
        }
    char key[64];
    snprintf(key, sizeof key, "CARS/TIRE.BIN#0#%d#wheel%d", wheelColor, wheel);
    const gfx::Texture& t = d.GetFile(key, out, w, h);
    return made[wheelColor][wheel] = t.id ? &t : nullptr;
}

#include "driver_records.inc"

}  // namespace

const char* Car_DriverRecordName(int group, int index) {
    if (group <= 0 || group >= 32 || index < 0 || index >= kDriverGroupSize[group]) return nullptr;
    return kDriverGroups[group][index].name;
}

const CarRecord* Car_DriverRecord(int group, int index) {
    if (group <= 0 || group >= 32 || index < 0 || index >= kDriverGroupSize[group]) return nullptr;
    return &kDriverGroups[group][index];
}

int QuickRace_Card(int courseId) {
    for (int i = 0; i < 8; ++i)
        if (kQuickRace[i].id == courseId) return i;
    return -1;
}

const CarRecord& QuickRace_PlayerCar(int card) { return kQuickRaceCar[card]; }

int Race_CreateCars(int courseId, RaceCar* out) {
    int card = QuickRace_Card(courseId);
    if (card < 0) return 0;
    const QuickRaceCourse* course = &kQuickRace[card];
    const int count = Cars::kMaxCars;
    out[0] = RaceCar();
    out[0].record = &kQuickRaceCar[card];
    out[0].slot = count - 1;
    out[0].town = -1;
    for (int i = 0; i < count - 1; ++i) {
        RaceCar& car = out[i + 1];
        car = RaceCar();
        car.record = FindOpponent(course->opponents[i].group, course->opponents[i].index);
        car.town = course->opponents[i].group;
        // Race_CreateCars' Quick Race loop (flags 3): the slot is the entry's number but for these swaps.
        car.slot = i == 3 ? 6 : i == 6 ? 3 : i == 9 ? 12 : i == 12 ? 9 : i;
    }
    return count;
}

int Race_CreateVersusCars(bool teams, int rowCars, RaceCar* out, CarRecord* records) {
    // D_0177EA50[0..5]: record 0's cars 0-2, then record 1's (Versus_Task's loader). A teammate's name is its
    // driver record's (+0x674, Save_GetCarName).
    for (int e = 0; e < 6; ++e) {
        const SaveRecord& r = Save_Record(e / 3);
        const SaveCar& c = r.cars[e % 3];
        CarRecord& rec = records[e];
        rec.paint = c.paint & 0xFFFFFF;
        rec.body = c.body;
        rec.wheelColor = c.wheelColor;
        rec.wheel = c.wheel;
        rec.billboard = c.billboard;
        rec.name = r.name;
        if (e % 3) {
            const char* name = Car_DriverRecordName(r.teammates[e % 3 - 1][0], r.teammates[e % 3 - 1][1]);
            rec.name = name ? name : "";
        }
    }
    int count = 0;
    auto add = [&](int entry, int slot) {
        RaceCar& car = out[count++];
        car = RaceCar();
        car.record = &records[entry];
        car.slot = slot;
        car.town = -1;
        car.side = entry / 3;
        car.entry = entry;
    };
    if (!teams || rowCars < 6) {
        add(0, 1);
        add(3, 0);
        return count;
    }
    add(0, 5);
    add(3, 4);
    // The teammates' slots count up from 0: player 1's teammate 1, player 2's, then the teammates 2.
    int slot = 0;
    for (int member = 1; member <= 2; ++member) {
        for (int side = 0; side < 2; ++side) {
            if (!Save_Record(side).teammates[member - 1][0]) continue;
            add(side * 3 + member, slot++);
        }
    }
    return count;
}

int Team_Find(int group, int index) {
    const SaveRecord& r = Save_Current();
    for (int m = 0; m < 2; ++m)
        if (r.teammates[m][0] && r.teammates[m][0] == group && r.teammates[m][1] == index) return m + 1;
    return 0;
}

int Race_CreateAdventureCars(int race, RaceCar* out, CarRecord* records, int* lanes) {
    const AdventureRace& row = AdventureRace_Get(race);
    const SaveRecord& r = Save_Current();
    const int cars = row.cars;
    int count = 0;
    auto add = [&](const CarRecord* record, int slot, int lane, int entry, int town, int index) {
        RaceCar& car = out[count];
        car = RaceCar();
        car.record = record;
        car.slot = slot;
        car.town = town;
        car.index = index;
        car.entry = entry;
        lanes[count++] = lane;
    };
    // D_0177EA50[0-2]: the save's cars; a teammate's name is its driver record's.
    for (int m = 0; m < 3; ++m) {
        records[m].paint = r.cars[m].paint & 0xFFFFFF;
        records[m].body = r.cars[m].body;
        records[m].wheelColor = r.cars[m].wheelColor;
        records[m].wheel = r.cars[m].wheel;
        records[m].billboard = r.cars[m].billboard;
        records[m].name = r.name;
        if (m) {
            const char* name = Car_DriverRecordName(r.teammates[m - 1][0], r.teammates[m - 1][1]);
            records[m].name = name ? name : "";
        }
    }
    // The Tunnel Race (course 0x1C, Akiban's, My City place 18): Car_Init gives the player the building's person's
    // car (D_0177EA5C) in lane 3, so both machines are alike (StartAdventure gives it the row's parts too).
    int lane = 0;
    if (row.course == kTunnelCourse) {
        if (const CarRecord* akiban = Car_DriverRecord(9, 18)) {
            records[0].paint = akiban->paint;
            records[0].body = akiban->body;
            records[0].wheelColor = akiban->wheelColor;
            records[0].wheel = akiban->wheel;
            records[0].billboard = akiban->billboard;
        }
        lane = 3;
    }
    add(&records[0], cars - 1, lane, 0, -1, 0);
    int back = cars - 2, front = 0;
    if (cars >= 3) {
        for (int m = 1; m <= 2; ++m)
            if (r.teammates[m - 1][0]) add(&records[m], front++, m, m, r.teammates[m - 1][0], r.teammates[m - 1][1]);
    }
    if (row.rank == 3) {
        // The World Grand Prix (Race_CreateCars' rank-3 branch, 0x0020F01C): the list in order as cars 3 on, so team
        // t is cars 3t..3t+2 (the player's the first), each in lane 3 + k and slot 2 + k but for the Quick Race's
        // swaps, with no skip for the player's teammates.
        for (int k = 0; count < cars; ++k) {
            int group = row.opponents[k][0], index = row.opponents[k][1];
            const CarRecord* record = Car_DriverRecord(group, index);
            if (!record) break;
            int s = 2 + k;
            s = s == 3 ? 6 : s == 6 ? 3 : s == 9 ? 12 : s == 12 ? 9 : s;
            add(record, s, k + 3, -1, group, index);
        }
        return count;
    }
    // An opponent races unless they're one of the player's teammates.
    auto opponent = [&](int k, int slot) {
        int group = row.opponents[k][0], index = row.opponents[k][1];
        const CarRecord* record = Car_DriverRecord(group, index);
        if (Team_Find(group, index) || !record) return false;
        add(record, slot, k + 3, -1, group, index);
        return true;
    };
    // The list's last six, from the back of the grid forwards.
    for (int k = cars - 2; k >= 0 && k >= cars - 7 && count < cars; --k)
        if (opponent(k, back)) --back;
    // Then from the list's start, behind the teammates.
    for (int i = 0; i < cars - 1 && count < cars; ++i)
        if (opponent(i, front)) ++front;
    return count;
}

int Race_CreateDemoCars(int race, RaceCar* out, int* lanes) {
    const AdventureRace& row = AdventureRace_Get(race);
    int count = 0;
    for (int e = row.cars - 2; e > 0; --e) {
        const int group = row.opponents[e][0], index = row.opponents[e][1];
        const CarRecord* record = Car_DriverRecord(group, index);
        if (!record) continue;
        RaceCar& car = out[count];
        car = RaceCar();
        car.record = record;
        car.slot = e;
        car.town = group;
        car.index = index;
        car.entry = -1;
        lanes[count++] = e + 3;
    }
    return count;
}

const char* Car_HomeTown(const RaceCar& car) {
    // D_002BE438: the towns by group, 8 bytes each (the name, then the town's map and entry point).
    static const char* const kTowns[] = {GT_00328770_9,      GT_00328760_10,    GT_00328750_9,      GT_00328740_9,
                                         GT_00328730_15, GT_00328720_13, GT_00328710_14, GT_00328700_13,
                                         GT_00328488_10,      GT_003345E8_7};
    const int town = car.town >= 0 ? car.town : Stamp_Earned(0x28) ? 9 : 1;
    return town < (int)(sizeof kTowns / sizeof kTowns[0]) ? kTowns[town] : "???";
}

namespace {

// A body's model files, in the order BodyModel takes them: sections 0-5, the shadow, the headlights of sections 0
// and 1, the brake lights of sections 0 and 2.
const int kBodyFiles = 11;
std::string BodyFile(const std::string& dataDir, int body, int n) {
    char dir[32];
    if (body < 150) snprintf(dir, sizeof dir, "CAR%d\\Q%02d\\", body / 30, body);
    else snprintf(dir, sizeof dir, "CARS\\Q%d\\", body);
    std::string path = dataDir + "\\models\\" + dir;
    if (n < 6) return path + std::to_string(n) + ".obj";
    if (n == 6) return path + "shadow.obj";
    if (n < 9) return path + std::to_string(n - 7) + "_head.obj";
    return path + std::to_string((n - 9) * 2) + "_brake.obj";
}

}  // namespace

gfx::Model& Cars::BodyModel(Body& b, int n) {
    if (n < 6) return b.model[n];
    if (n == 6) return b.shadow;
    if (n < 9) return b.head[n - 7];
    return b.brake[n - 9];
}

// The thread Cars_Request starts: it reads each body's meshes (no graphics), then sets `done`.
struct Cars::Reading {
    std::string dataDir;
    std::vector<int> bodies;
    std::vector<gfx::Mesh> meshes;   // kBodyFiles a body
    std::vector<bool> read;          // each one's file was there
    HANDLE thread = nullptr;
    volatile LONG done = 0;
    size_t next = 0;                 // the next mesh Cars_Stream puts on the card
    Body building;                   // the body it's putting there

    static DWORD WINAPI Run(void* self) {
        Reading& r = *static_cast<Reading*>(self);
        for (size_t i = 0; i < r.bodies.size(); ++i)
            for (int n = 0; n < kBodyFiles; ++n)
                r.read[i * kBodyFiles + n] = gfx::LoadObj(BodyFile(r.dataDir, r.bodies[i], n),
                                                          r.meshes[i * kBodyFiles + n]);
        InterlockedExchange(&r.done, 1);
        return 0;
    }
    void Wait() {
        if (!thread) return;
        WaitForSingleObject(thread, INFINITE);
        CloseHandle(thread);
        thread = nullptr;
    }
    ~Reading() { Wait(); }
};

Cars::Cars() {}
Cars::~Cars() {}

void Cars::Load(gfx::Draw2D& draw, const std::string& dataDir, const RaceCar* cars, int count) {
    if (reading_) {
        // A request still out: its bodies first, all of them now.
        reading_->Wait();
        Cars_Stream(draw, 1e9);
    }
    for (int i = 0; i < count; ++i) {
        int body = cars[i].record ? cars[i].record->body : -1;
        if (body < 0 || bodies_.count(body)) continue;
        Body& b = bodies_[body];
        for (int n = 0; n < kBodyFiles; ++n) BodyModel(b, n).Load(BodyFile(dataDir, body, n), draw);
    }
    if (parts_[0].Empty())
        for (int n = 0; n < kParts; ++n)
            parts_[n].Load(dataDir + "\\models\\CARS\\PARTS\\" +std::to_string(n) + ".obj", draw);
    if (tires_[0].Empty())
        for (int t = 0; t < 6; ++t)
            tires_[t].Load(dataDir + "\\models\\CARS\\TIRE\\" + std::to_string(t) + ".obj", draw);
    if (wheels_[0].Empty())
        for (int w = 0; w < 16; ++w)
            wheels_[w].Load(dataDir + "\\models\\CARS\\WHEEL\\" + std::to_string(w) + ".obj", draw);
}

void Cars::Cars_Request(gfx::Draw2D& draw, const std::string& dataDir, const RaceCar* cars, int count) {
    if (reading_) {
        // The last request isn't on the card yet: it all goes now.
        reading_->Wait();
        Cars_Stream(draw, 1e9);
    }
    std::unique_ptr<Reading> r(new Reading);
    r->dataDir = dataDir;
    for (int i = 0; i < count; ++i) {
        int body = cars[i].record ? cars[i].record->body : -1;
        if (body < 0 || bodies_.count(body)) continue;
        bool seen = false;
        for (int b : r->bodies) seen |= b == body;
        if (!seen) r->bodies.push_back(body);
    }
    if (r->bodies.empty()) return;
    r->meshes.resize(r->bodies.size() * kBodyFiles);
    r->read.resize(r->meshes.size());
    r->thread = CreateThread(nullptr, 0, &Reading::Run, r.get(), 0, nullptr);
    if (!r->thread) Reading::Run(r.get());   // no thread: read them here
    else SetThreadPriority(r->thread, THREAD_PRIORITY_BELOW_NORMAL);
    reading_ = std::move(r);
}

bool Cars::Cars_Stream(gfx::Draw2D& draw, double budgetMs) {
    if (!reading_) return true;
    Reading& r = *reading_;
    if (!r.done) return false;
    r.Wait();
    LARGE_INTEGER freq, start, now;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&start);
    // A body goes into bodies_ only once all its models are on the card, so a car is drawn whole or not at all.
    while (r.next < r.meshes.size()) {
        size_t i = r.next / kBodyFiles;
        int n = (int)(r.next % kBodyFiles);
        if (n == 0) r.building = Body();
        gfx::Model& m = BodyModel(r.building, n);
        if (r.read[r.next]) m.Load(std::move(r.meshes[r.next]), draw);
        ++r.next;
        if (n == kBodyFiles - 1) bodies_[r.bodies[i]] = std::move(r.building);
        QueryPerformanceCounter(&now);
        if ((now.QuadPart - start.QuadPart) * 1000.0 / freq.QuadPart > budgetMs) break;
    }
    if (r.next < r.meshes.size()) return false;
    reading_.reset();
    return true;
}

// Car_Draw (0x00221788). By the car's distance, its view depth over the focal length (Cars_Draw's): the near body
// (section 0) under 0.03 (15 units at focal 500) for the first six cars that close (D_003356E8, in car order), else
// the far one (section 1). Then section 2 (3 with part flag 0x1000), sections 4 and 5 by part flags 0x2000 and 0x4000
// (the jet turbines, in CARS/PARTS.BIN's picture), the parts of flags 0x1FF (the far propellers with the far body),
// and under 0.05 (D_00332CCC, 25 units) the tires as Car_DrawTires (0x002213E8) places them; beyond that section 3
// of TIRE.BIN, all four at once, in the car's frame. At distance exactly 0 (Car_DrawCard's `close`) the hi-detail
// WHEEL.BIN wheels instead (func_00221570, Car_DrawCloseWheels).
// The tires' picture goes by the record's +5 and +6 (TirePicture). A driven car's body sits on its suspension (tilted
// and raised by it) and its front tires roll and steer, the rear ones roll. With part flag 0x400 the big tires take
// the place of the tires at every distance (Car_DrawBigTires).
// The parts: PARTS.BIN model n for flag 1 << n (n 0-8), in the body's frame at the body's place for it (D_002ED720 +
// 0x44 body: kPartPlace). Part 6 (0x40, the propellers) is model 6, a bar, and four of model 7 spun about the car's
// length by the rear wheels' angle (car +0x88, sceVu0RotMatrixZ by minus it); far away (here: the reflection) model
// 11 instead. Part 0x200, the billboard, is model 9 at part 5's place in the sponsor's picture (Car_DrawParts).
// Cars_DrawMirrored's reflection: the car's matrix with its axes' heights negated and its own height mirrored about
// the ground under its first corner probe (car +0x144), lit by the lights with their heights negated (the light
// matrix's second row), and drawn as from 15 units away (0.03 of the focal length: the far body, separate tires).
// Where Car_Draw puts the car: `m` the car's matrix (a reflection's mirrored about the ground under its first corner
// probe, car +0x144), `bm` the body's, on its suspension. A car standing level has m, but on the big tires 0.85 up:
// the rooms, the 2 Player cards, the garage and the photos set the body's +0x74 to it (D_00332E14-E28).
void CarPlace(const RaceCar& car, bool mirrored, float* m, float* bm) {
    CarMatrix(car, car.pos[1], m);
    if (mirrored) {
        float ground = car.physics ? car.physics->probe[3][1] : car.pos[1];
        for (int k = 0; k < 3; ++k) m[4 + k] = -m[4 + k];
        m[7] = 2 * ground - m[7];
    }
    if (car.physics) {
        BodyMatrix(*car.physics, bm);
        Compose(m, bm, bm);
    } else if (car.partFlags & kPartBigTires) {
        const float up[3] = {0, kBigTiresLift, 0};
        Translate(m, up, bm);
    } else {
        memcpy(bm, m, 12 * sizeof(float));
    }
}

int Cars::Car_Draw(gfx::Draw2D& d, const gfx::Camera3D& camera, const RaceLight& raceLight, const RaceCar& car,
                   float distance, int& nearBodies, bool mirrored) const {
    // Graphics Low takes the far body and the four tires as one sooner, and the near body for two cars at most.
    const bool low = gfx::LowDetail();
    const float kNearBody = low ? 0.02f : 0.03f, kSeparateTires = low ? 0.03f : 0.05f;   // D_00332CC0, D_00332CCC
    const int kMaxNearBodies = low ? 2 : 6;
    auto it = bodies_.find(car.record->body);
    if (it == bodies_.end()) return -1;
    const Body& b = it->second;
    // The colour set the ground under the car picks (car +0x19C bits 12-13); one standing level takes set 2.
    gfx::Lighting light = Light_CarSet(raceLight, car.physics ? car.physics->surface[0] >> 12 : 2);
    Paint(car.record->paint, light.paint);
    if (car.physics) Car_LightsUnderWater(light, car.pos[1] - car.physics->probe[0][3]);
    if (mirrored)
        for (int l = 0; l < 3; ++l) light.dir[l][1] = -light.dir[l][1];
    float m[12], bm[12];
    CarPlace(car, mirrored, m, bm);
    gfx::DrawStyle st = CarStyle(raceLight);
    st.lighting = &light;
    st.transform = bm;
    bool nearBody = distance < kNearBody && nearBodies < kMaxNearBodies;
    if (nearBody) ++nearBodies;
    int section = nearBody || b.model[1].Empty() ? 0 : 1;
    b.model[section].Draw(d, camera, kFrame, st);
    b.model[(car.partFlags & kPartFins) ? 3 : 2].Draw(d, camera, kFrame, st);
    // Sections 4 and 5 by part flags 0x2000 and 0x4000, with PARTS.BIN's picture in palette 10 or 11.
    for (int s = 0; s < 2; ++s)
        if (car.partFlags & (kPartJet << s)) b.model[4 + s].Draw(d, camera, kFrame, st);
    Car_DrawParts(d, camera, car, st, !nearBody);
    if (car.partFlags & kPartBigTires) {
        Car_DrawBigTires(d, camera, car, st, m, mirrored);
        return section;
    }
    st.texture = TirePicture(d, car.record->wheelColor, car.record->wheel);
    if (distance >= kSeparateTires) {
        st.transform = m;
        tires_[3].Draw(d, camera, kFrame, st);
        return section;
    }
    for (int t = 0; t < 3; ++t) {
        float tm[12];
        if (car.physics) {
            const float* a = car.physics->wheelAngle;
            TireMatrix(kTirePos[t], t < 2 ? a[0] : a[2], t < 2 ? a[1] : 0.0f, tm);
            Compose(m, tm, tm);
        } else {
            Translate(m, kTirePos[t], tm);
        }
        st.transform = tm;
        tires_[t].Draw(d, camera, kFrame, st);
    }
    return section;
}

void Cars::Car_DrawCard(gfx::Draw2D& d, const gfx::Camera3D& camera, const RaceLight& raceLight, const RaceCar& car,
                        bool silhouette, bool close) const {
    auto it = bodies_.find(car.record->body);
    if (it == bodies_.end()) return;
    const Body& b = it->second;
    gfx::Lighting light = Light_CarSet(raceLight, close ? 0 : 2);
    Paint(car.record->paint, light.paint);
    float m[12], bm[12];
    CarPlace(car, false, m, bm);
    gfx::DrawStyle st = CarStyle(raceLight);
    st.lighting = &light;
    st.transform = bm;
    gfx::DrawStyle parts = st;
    if (silhouette)
        for (int k = 0; k < 3; ++k) st.tint[k] = 0.0f;
    b.model[0].Draw(d, camera, kFrame, st);
    b.model[(car.partFlags & kPartFins) ? 3 : 2].Draw(d, camera, kFrame, st);
    for (int s = 0; s < 2; ++s)
        if (car.partFlags & (kPartJet << s)) b.model[4 + s].Draw(d, camera, kFrame, st);
    Car_DrawParts(d, camera, car, parts, false);
    if (car.partFlags & kPartBigTires) {
        Car_DrawBigTires(d, camera, car, st, m, false);
        return;
    }
    if (close) {
        Car_DrawCloseWheels(d, camera, car, st, m);
        return;
    }
    st.texture = TirePicture(d, car.record->wheelColor, car.record->wheel);
    for (int t = 0; t < 3; ++t) {
        float tm[12];
        Translate(m, kTirePos[t], tm);
        st.transform = tm;
        tires_[t].Draw(d, camera, kFrame, st);
    }
}

// func_00221570: for each wheel the base (Rz(pi) on the right), the roll Rx (front +0x80, rear +0x88), the front's
// steer Ry (+0x84), the place D_0029CB60 with its w, in the car's frame; the hub (1 + the Wheels part), then the tire.
void Cars::Car_DrawCloseWheels(gfx::Draw2D& d, const gfx::Camera3D& camera, const RaceCar& car, gfx::DrawStyle st,
                               const float* m) const {
    gfx::Lighting light = *st.lighting;
    WheelPaint(car.record->wheelColor, light.paint);
    st.lighting = &light;
    st.texture = nullptr;
    const int hub = 1 + (car.record->wheel < 15 ? car.record->wheel : 0);
    for (int i = 0; i < 4; ++i) {
        const float* p = kCloseWheelPos[i];
        const float t[3] = {p[0], p[1], p[2]};
        float roll = 0, steer = 0;
        if (car.physics) {
            roll = car.physics->wheelAngle[i < 2 ? 0 : 2];
            steer = i < 2 ? car.physics->wheelAngle[1] : 0.0f;
        }
        float tm[12];
        TireMatrix(t, roll, steer, tm);
        if (i & 1) {
            const float flip[12] = {-1, 0, 0, 0, 0, -1, 0, 0, 0, 0, 1, 0};
            Compose(tm, flip, tm);
        }
        // The homogeneous w: everything over it.
        for (float& v : tm) v /= p[3];
        Compose(m, tm, tm);
        st.transform = tm;
        wheels_[hub].Draw(d, camera, kFrame, st);
        wheels_[0].Draw(d, camera, kFrame, st);
    }
}

void Cars::Car_DrawWheelCard(gfx::Draw2D& d, const gfx::Camera3D& camera, const RaceLight& raceLight, int item,
                             int wheelColor) const {
    if (item < 0 || item > 14) return;
    gfx::Lighting light = Light_CarSet(raceLight, 2);
    WheelPaint(wheelColor, light.paint);
    static const float kIdentity[12] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0};
    gfx::DrawStyle st = CarStyle(raceLight);
    st.lighting = &light;
    st.transform = kIdentity;
    wheels_[1 + item].Draw(d, camera, kFrame, st);
}

// Car_Draw's part flag 0x400: PARTS.BIN model 10 (untextured, in paint 1) in the car's frame, which the reflection
// (func_00221F70) leaves out; then TIRE.BIN's sections 4 and 5 at kBigTirePos, in the tire picture at TBP 0x1C44
// itself (CLUT +5; the Wheels part doesn't move it). The front pair turns by the front wheels' roll (+0x80) and the
// steering (+0x84), the rear pair by the rear roll (+0x88).
void Cars::Car_DrawBigTires(gfx::Draw2D& d, const gfx::Camera3D& camera, const RaceCar& car, gfx::DrawStyle st,
                            const float* m, bool mirrored) const {
    st.transform = m;
    if (!mirrored) parts_[kModelLift].Draw(d, camera, kFrame, st);
    st.texture = TirePicture(d, car.record->wheelColor, 0);
    for (int t = 0; t < 4; ++t) {
        float tm[12];
        if (car.physics) {
            const float* a = car.physics->wheelAngle;
            TireMatrix(kBigTirePos[t], t < 2 ? a[0] : a[2], t < 2 ? a[1] : 0.0f, tm);
            Compose(m, tm, tm);
        } else {
            Translate(m, kBigTirePos[t], tm);
        }
        st.transform = tm;
        tires_[4 + (t & 1)].Draw(d, camera, kFrame, st);
    }
}

// func_00222360, after every car (Cars_Draw sets its GS state first, func_00223490): with the headlights on, chain A
// of the body section drawn (0 near, 1 far); with the brake lights, chains B of sections 0 and 2 (whichever of 2 and
// 3 the car shows). Program 0x50 in the car's texture: unlit, the colour times vf17 = (1, 1, 0.5, 1) (D_0029D2D0),
// added to the frame by its alpha (ALPHA_2 0x48, D_0029D2F0).
void Cars::Car_DrawLights(gfx::Draw2D& d, const gfx::Camera3D& camera, const RaceLight& raceLight, const RaceCar& car,
                          int section, bool mirrored) const {
    // A driven car's headlights are its physics' +0x1FC (R1 held).
    uint8_t lights = car.lights | (car.physics && car.physics->lights ? kLightsHead : 0);
    if (!lights || section < 0 || section > 1) return;
    auto it = bodies_.find(car.record->body);
    if (it == bodies_.end()) return;
    const Body& b = it->second;
    float m[12], bm[12];
    CarPlace(car, mirrored, m, bm);
    gfx::DrawStyle st = CarStyle(raceLight);
    st.transform = bm;
    st.tint[2] = 0.5f;
    st.state.additive = true;
    if (lights & kLightsHead) b.head[section].Draw(d, camera, kFrame, st);
    if (lights & kLightsBrake)
        for (int s = 0; s < 2; ++s) b.brake[s].Draw(d, camera, kFrame, st);
}

void Cars::Car_DrawParts(gfx::Draw2D& d, const gfx::Camera3D& camera, const RaceCar& car, gfx::DrawStyle st,
                         bool far) const {
    if (!(car.partFlags & 0x3FF)) return;
    const float(*place)[2] = kPartPlace[car.record->body < 151 ? car.record->body : 0];
    const float* body = st.transform;
    float y = 0, z = 0, tm[12];
    for (int n = 0; n < 9; ++n) {
        if (!(car.partFlags & (1 << n))) continue;
        if (kPartPlaceOf[n] >= 0) {
            y = place[kPartPlaceOf[n]][0];
            z = place[kPartPlaceOf[n]][1];
        }
        const float t[3] = {0, y, z};
        Translate(body, t, tm);
        st.transform = tm;
        if (n != kPropellerPart) {
            parts_[n].Draw(d, camera, kFrame, st);
        } else if (far) {
            parts_[kModelPropellersFar].Draw(d, camera, kFrame, st);
        } else {
            parts_[n].Draw(d, camera, kFrame, st);
            float a = car.physics ? -car.physics->wheelAngle[2] : 0.0f, c = cosf(a), s = sinf(a);
            for (int k = 0; k < kPropellers; ++k) {
                const float spin[12] = {c, -s, 0, kPropellerX + k * kPropellerStep, s, c, 0, y, 0, 0, 1, z};
                Compose(body, spin, tm);
                parts_[kModelPropeller].Draw(d, camera, kFrame, st);
            }
        }
    }
    // Part flag 0x200, after the others: model 9 at part 5's place (+0x20), its TEX0 the T8 64x32 picture at TBP
    // 0x1BE4 + 10 x the record's +7, CLUT 0x1BEC + 10 n: PARTS.BIN's picture 1 + n, a sponsor's.
    if (car.partFlags & kPartBillboard) {
        const float t[3] = {0, place[kPartPlaceOf[5]][0], place[kPartPlaceOf[5]][1]};
        Translate(body, t, tm);
        st.transform = tm;
        int n = car.record->billboard >= 0 && car.record->billboard < 5 ? car.record->billboard : 0;
        const gfx::Texture& pic = d.Get("CARS/PARTS.BIN", 1 + n);
        st.texture = pic.id ? &pic : nullptr;
        parts_[kModelBillboard].Draw(d, camera, kFrame, st);
    }
}

// Cars_Draw (0x002228A8): first every car's shadow, a 2 x 3 quad of the bottom right quarter of the car's texture
// laid on the ground under it, then the cars. The shadow's matrix is the car's with its y column cleared, and program
// 0x50 gives each corner its height from vf18: the ground found under the car's corner probes (car +0x140..+0x170),
// so the shadow stays on the ground when the car jumps. The shadow blends at half alpha (vf17.w) and writes no depth:
// its TEST_2 (D_0029D290, 0x5380B) fails every pixel under 0x80 and keeps only its colour (AFAIL RGB_ONLY), so the
// frame's alpha stays the ground's (which the field's night darkening goes by).
void Cars::Cars_Draw(gfx::Draw2D& d, const gfx::Camera3D& camera, const RaceLight& light, const RaceCar* cars,
                     int count, int hidden, bool mirrored) const {
    gfx::DrawSourceScope source(d.GetRenderer(), gfx::DrawSource::Cars);
    const float kDrawn = 0.003f;   // D_00332CF4: nearer than this (or behind the camera), a car isn't drawn
    const float kReflection = 0.03f;   // the reflections are drawn as from 15 units (far body, separate tires)
    gfx::DrawStyle shadow = CarStyle(light);
    shadow.alpha = 0.5f;
    shadow.state.depthWrite = false;
    shadow.state.gsAlphaTest = true;
    for (int i = 0; i < count; ++i) {
        if (!cars[i].record || cars[i].out || i == hidden) continue;
        auto it = bodies_.find(cars[i].record->body);
        if (it == bodies_.end()) continue;
        float m[12], ground[4];
        CarMatrix(cars[i], cars[i].pos[1] + kShadowLift, m);
        shadow.cornerY = nullptr;
        if (cars[i].physics) {
            const CarBody& b = *cars[i].physics;
            for (int k = 0; k < 4; ++k) ground[k] = b.probe[3 + k][1] + kShadowLift;
            m[4] = m[5] = m[6] = 0;
            m[7] = (ground[0] + ground[1] + ground[2] + ground[3]) / 4;   // for the bounds test
            shadow.cornerY = ground;
        }
        shadow.transform = m;
        it->second.shadow.Draw(d, camera, kFrame, shadow);
    }
    if (gfx::LowDetail()) mirrored = false;   // Graphics Low leaves out the reflections
    int nearBodies = 0;
    int section[kMaxCars], reflected[kMaxCars];   // Cars_Draw's sp40: the body each car was drawn with, or -1
    for (int i = 0; i < count && i < kMaxCars; ++i) {
        section[i] = reflected[i] = -1;
        if (!cars[i].record || cars[i].out || i == hidden) continue;
        const float* p = cars[i].pos;
        float depth = (p[0] - camera.eye[0]) * camera.forward[0] + (p[1] - camera.eye[1]) * camera.forward[1] +
                      (p[2] - camera.eye[2]) * camera.forward[2];
        float distance = depth / camera.focalX;
        if (distance < kDrawn) continue;
        if (mirrored) reflected[i] = Car_Draw(d, camera, light, cars[i], kReflection, nearBodies, true);
        section[i] = Car_Draw(d, camera, light, cars[i], distance, nearBodies);
    }
    for (int i = 0; i < count && i < kMaxCars; ++i) {
        if (reflected[i] >= 0) Car_DrawLights(d, camera, light, cars[i], reflected[i], true);
        if (section[i] >= 0) Car_DrawLights(d, camera, light, cars[i], section[i], false);
    }
}

}  // namespace game
