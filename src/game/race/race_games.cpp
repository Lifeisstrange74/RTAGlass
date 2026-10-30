// The Adventure's games against the clock or one car, each a course row's event task in place of Race_Task: the Drag
// Race (course 0x16), Rock Climbing (0x15), the Volcano Course (0x1F), Rainbow Jump (0x1B), Beach Flag (0x25) and
// the Treasure Hunting Maze (0x12). High-level reimplementations of func_00260638, func_0025FBF8, func_0025E630,
// func_00254D20, func_002638C0 and func_0025F6A0. See docs/minigames.md.
#include <math.h>
#include <string.h>

#include "../../audio/sound.h"
#include "../hud/hud_games.h"
#include "../save/save_data.h"
#include "race.h"

namespace game {

namespace {

const float kPi = 3.14159f;   // D_00333124, D_00333134
const uint32_t kSeBump = 0x14, kSeFlag = 0x302;

// Which side of the line (x0, z0)-(x1, z1) (x, z) is on: >= 0 past it (the games' line test).
float LineSide(const float* l, float x, float z) { return (l[3] - l[1]) * (x - l[2]) - (l[2] - l[0]) * (z - l[3]); }

// The Drag Race: both cars at z 388 facing +z, the player at x 417.5 and Johnny at 422.5; the line is z 790. Out of
// the lane (x 410-430) or back under z 370 is COURSE OUT. A win within 480 frames is Race_Outcome 3.
const float kDragStart[2][3] = {{417.5f, 24.5f, 388.0f}, {422.5f, 24.5f, 388.0f}};
const float kDragLine = 790.0f, kDragLaneX0 = 410.0f, kDragLaneX1 = 430.0f, kDragBack = 370.0f;
const int kDragRecord = 0x1E1;
// COURSE OUT (D_0029FA28), palette 11, rocking, in the MISS COURSE! slot.
const int kCourseOut[4] = {0, 0, 256, 46};

// Rock Climbing: the goal line by Mason's cabin and the line behind the start (D_002D9938, D_002D9928). The goal
// sends the player on to place 6 (Mason's); under 7200 frames with stamp 0x39 earned is Race_Outcome 2.
const float kRockGoal[4] = {167.0f, 215.3f, 162.6f, 211.4f};
const float kRockBack[4] = {75.9f, 98.9f, 82.6f, 84.9f};
const int kRockPlace = 6, kRockRecord = 0x1C21, kRockStamp = 0x39;
// GOAL!! (D_0029FAE8), palette 3, zooming in, in the WIN! slot.
const int kGoalA03[4] = {0, 0, 232, 48};

// The Volcano Course: on top of the tool box (height 169-171, inside D_002C4C40's square) is CLEAR; under height
// 100 is MISS; back at x 110 or less gives up. With stamp 0x37 earned, a time under 4200 is Race_Outcome 2 and a
// slower one 0; the first time 1. The box, its lid and the wrench are A13's objects 0-2 (D_002C4C10).
const float kBoxX0 = 306.5f, kBoxX1 = 311.0f, kBoxZ0 = 282.2f, kBoxZ1 = 286.5f, kBoxY0 = 169.0f, kBoxY1 = 171.0f;
const float kVolcanoFall = 100.0f, kVolcanoBack = 110.0f;
const int kVolcanoRecord = 0x1069, kVolcanoStamp = 0x37;
const float kToolBox[3][3] = {{308.75f, 170.0f, 286.5f}, {308.75f, 170.8f, 286.5f}, {308.75f, 170.0f, 285.0f}};
const float kWrenchRise = 0.03f;   // D_00333120

// Rainbow Jump: the start (652.8, 400, 1198.5) facing +x; at rest between heights 275 and 350 is CLEAR (Race_Outcome
// 2), under 275 MISS (1).
const float kRainbowStart[3] = {652.8f, 400.0f, 1198.5f};
const float kRainbowFall = 275.0f, kRainbowTop = 350.0f;

// Beach Flag: both cars facing -x with the flag (D_003293D0) behind them; the first within 1.5 of it wins.
const float kFlagStart[2][3] = {{354.0f, 103.5f, 769.2f}, {354.0f, 103.8f, 773.8f}};
const float kFlag[3] = {857.8f, 103.21f, 653.6f};
const float kFlagReach = 1.5f;

// The Treasure Hunting Maze: into the chest's square (D_002C4F50) is CLEAR; back out of the entrance (x 44-56, z 12
// or less) gives up. Under 10800 frames with stamp 14 earned is Race_Outcome 2, else 1. The chest is A00's objects
// 0-2 turned -pi/2 about y (D_00333138), the lid (1) also about z (D_002C4F20).
const float kChestX0 = 48.5f, kChestX1 = 54.8f, kChestZ0 = 346.4f, kChestZ1 = 353.6f;
const float kEntranceX0 = 44.0f, kEntranceX1 = 56.0f, kEntranceZ = 12.0f;
const int kMazeRecord = 0x2A31, kMazeStamp = 14;
const float kChest[3][3] = {{50.0f, 20.0f, 350.0f}, {50.0f, 21.35f, 350.0f}, {50.0f, 20.0f, 350.0f}};

// Curling: the car is the stone. It starts at (875, 31.94, 792.5) facing -z; over z 700 it brakes for good and slides
// on the ice. Stood still, its distance from the target's centre (D_002DA0B0) under 2, 8, 15 or 23 (D_002DA090)
// scores 100, 50, 20 or 10 (D_002DA0A0). Three tries; the total wins with a new record (stamp 70 earned) or 200 or
// more (not), and 300 is Race_Outcome 3.
const float kCurlingStart[3] = {875.0f, 31.94f, 792.5f};
const float kCurlingTarget[3] = {875.0f, 31.94f, 525.0f};
const float kRingRadius[4] = {2.0f, 8.0f, 15.0f, 23.0f};
const int kRingPoints[4] = {100, 50, 20, 10};
const float kCurlingLine = 700.0f;
const int kCurlingWait = 0x78, kCurlingShow = 0xB4, kCurlingStamp = 0x46, kCurlingWin = 200, kCurlingPerfect = 300;
// FIRST TRY, SECOND TRY and FINAL (D_0029FC48), picture 0, palette 0, zooming in over 10 frames in the countdown's
// slot; NEW RECORD (D_0029F928), picture 1, palette 12, in the WIN! slot.
const int kTryBanner[3][4] = {{0, 0, 256, 48}, {0, 48, 256, 48}, {0, 96, 256, 48}};
const int kNewRecord[4] = {0, 152, 252, 40};

// Ski Jumping: the start (790, 364, 1290) facing -z; the take-off z 1100, the jump 1100 - z; the quit off the start
// platform's left (x 770 or less with z 1280 or more). The run-out slows the car 0x98 a frame; 140 metres win.
const float kSkiStart[3] = {790.0f, 364.0f, 1290.0f};
const float kTakeOff = 1100.0f, kQuitZ = 1280.0f, kQuitX = 770.0f;
const int32_t kRunOutSlow = 0x98;
const int kSkiJumpWin = 140, kSkiJumpStamp = 0x42;

// Fishing: 30 fish in the fence (x and z 720-880), each its kind's speed (D_002C4BD0 by i & 3), caught within 2.5;
// 20 caught is FINISH!. D_002C4BC0 is Barrel Dodging's table again.
const int kFishes = 30, kFishToCatch = 20;
const float kFence0 = 720.0f, kFence1 = 880.0f, kCatchReach = 2.5f;
const float kFishSpeed[4] = {0.08f, 0.05f, 0.2f, 0.15f};
const uint8_t kLaneMix[16] = {8, 1, 13, 4, 5, 11, 15, 9, 16, 6, 3, 10, 7, 14, 2, 12};

// The Which-way? Race: three stretches (D_002D9E70 the starts, facing +z), each with its fork's line (D_002D9E40: z
// 534, 834 or 434) and the line between its two roads (D_002D9E10: x 100, 350 or 700), and its time (D_002D9F40).
const float kWayStart[3][3] = {{99.9f, 101.0f, 55.8f}, {349.9f, 101.0f, 57.0f}, {700.0f, 101.0f, 55.9f}};
const float kWayFork[3][4] = {{83, 534, 63, 534}, {333, 834, 313, 834}, {683, 434, 663, 434}};
const float kWaySide[3][4] = {{100, 450, 100, 480}, {350, 750, 350, 780}, {700, 350, 700, 380}};
const int kWayTime[3] = {1800, 3600, 1200};
// The rounds' tiles (D_002D9F18): 64 x 64 corners in the atlas, plain colours, halves, three colours twice, quarters.
struct WayRound {
    int count;
    int tiles[12][2];
};
const WayRound kWayRounds[5] = {
    {4, {{0, 0}, {64, 0}, {128, 0}, {192, 0}}},
    {6, {{256, 0}, {320, 0}, {256, 64}, {320, 64}, {384, 64}, {448, 64}}},
    {12, {{0, 64}, {64, 64}, {128, 64}, {192, 64}, {0, 128}, {64, 128}, {128, 128}, {192, 128}, {256, 128}, {320, 128},
          {384, 128}, {448, 128}}},
    {12, {{0, 64}, {64, 64}, {128, 64}, {192, 64}, {0, 128}, {64, 128}, {128, 128}, {192, 128}, {256, 128}, {320, 128},
          {384, 128}, {448, 128}}},
    {8, {{0, 192}, {64, 192}, {128, 192}, {192, 192}, {256, 192}, {320, 192}, {384, 192}, {448, 192}}},
};
// After the fifth, the run to the goal (D_00333194) to CLEAR! at z 432; after a miss, the car rolled (+z at 0x23A0,
// braked) from (875, 101, 35) until it drops under 80 or passes z 100.
const float kWayGoalRun[3] = {860.0f, 101.0f, 406.8f}, kWayMissRun[3] = {875.0f, 101.0f, 35.0f};
const float kWayGoalZ = 432.0f, kWayDropY = 80.0f, kWayDropZ = 100.0f;
const int32_t kWayRoll = 0x23A0;
// TIME UP (D_0029FA88), picture 0, palette 1, rocking, in the MISS COURSE! slot.
const int kTimeUp[4] = {0, 0, 210, 46};

// The lid's angle: ((counter % 360) - 180) degrees, 0 until it opens, then a degree a frame to 45.
const int kLidShut = 0xB4, kLidOpen = 0xE1;
float LidAngle(int lid) { return ((lid % 360) - 180) * kPi / 180.0f; }

float Distance(const float* a, const float* b) {
    float d[3] = {a[0] - b[0], a[1] - b[1], a[2] - b[2]};
    return sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
}

void SetSprite(HudElement& e, int type, const int* sprite, int palette, int y) {
    e = HudElement();
    e.type = type;
    e.x = 320;
    e.y = y;
    e.picture = 1;
    e.palette = palette;
    e.u = sprite[0];
    e.v = sprite[1];
    e.w = sprite[2];
    e.h = sprite[3];
    e.frames = 30;
    e.scale = 2.0f;
}

}  // namespace

void Race::Event_SetCourseOut(int side) {
    HudElement& e = elements_[kElemMissCourse + side];
    if (e.type != 2) SetSprite(e, 2, kCourseOut, 11, players_ == 1 ? 0xA8 : side ? 0x150 : 0x70);
}

void Race::Event_SetGoalA03(int side) {
    HudElement& e = elements_[kElemWin + side * 2];
    if (e.type != 3) SetSprite(e, 3, kGoalA03, 3, players_ == 1 ? 0xA8 : side ? 0x150 : 0x70);
}

// func_00260638. 1: the line and the lane (the player crossing first is WIN!, Race_Outcome 3 within 480 frames or
// 2; Johnny over it first is LOSE and 1; out of the lane COURSE OUT and 0), Johnny braked past the line, the time;
// 2: no pause, race over (flag 8); 3: the fade to black over 0x80 frames, the banners lit over it.
bool Race::Race_DragTask() {
    GameState& g = game_;
    switch (g.state) {
    case 1: {
        const CarBody& me = bodies_[0];
        if (me.pos[0] < kDragLaneX0 || me.pos[0] > kDragLaneX1 || me.pos[2] < kDragBack) {
            result_.outcome = 0;
            Event_SetCourseOut(0);
            g.state = 2;
            braked_[0] = true;
            noPause_ = true;
        }
        if (me.pos[2] > kDragLine) {
            g.state = 2;
            braked_[0] = true;
            noPause_ = true;
            if (carCount_ > 1 && bodies_[1].pos[2] > kDragLine) {
                result_.outcome = 1;
                Event_SetLose(0);
            } else {
                result_.outcome = g.time < kDragRecord ? 3 : 2;
                Event_SetWin(0);
            }
        }
        if (carCount_ > 1 && bodies_[1].pos[2] > kDragLine) braked_[1] = true;
        if (go_ && !paused_) ++g.time;
        Event_SetTime(g.time);
        break;
    }
    case 2:
        noPause_ = true;
        g.state = 3;
        break;
    case 3:
        event_.fadeAll = g.count;
        if (g.count < 0x80) ++g.count;
        else g.done = true;
        break;
    default:
        break;
    }
    return g.done;
}

// func_0025FBF8. 1: over the goal line GOAL!! and on to Mason's (place 6); back over the line behind the start
// Race_Outcome 0; the time. 2 and 3 as the Drag Race's.
bool Race::Race_RockClimbTask() {
    GameState& g = game_;
    switch (g.state) {
    case 1: {
        const CarBody& me = bodies_[0];
        if (LineSide(kRockGoal, me.pos[0], me.pos[2]) >= 0) {
            result_.outcome = g.time < kRockRecord && Stamp_Earned(kRockStamp) ? 2 : 1;
            result_.scenePlace = kRockPlace;
            g.state = 2;
            braked_[0] = true;
            Event_SetGoalA03(0);
        }
        if (LineSide(kRockBack, me.pos[0], me.pos[2]) >= 0) {
            result_.outcome = 0;
            g.state = 2;
            braked_[0] = true;
        }
        if (go_ && !paused_) ++g.time;
        Event_SetTime(g.time);
        break;
    }
    case 2:
        noPause_ = true;
        g.state = 3;
        break;
    case 3:
        event_.fadeAll = g.count;
        if (g.count < 0x80) ++g.count;
        else g.done = true;
        break;
    default:
        break;
    }
    return g.done;
}

// A13's box at its place, the lid turned about x as it opens, the wrench rising (sceVu0UnitMatrix, RotMatrixX,
// CopyVector).
void Race::Volcano_PlaceObjects() {
    const float box[12] = {1, 0, 0, kToolBox[0][0], 0, 1, 0, kToolBox[0][1], 0, 0, 1, kToolBox[0][2]};
    objects_.SetObject(0, box);
    float c = cosf(LidAngle(game_.lid)), s = sinf(LidAngle(game_.lid));
    const float lid[12] = {1, 0, 0, kToolBox[1][0], 0, c, -s, kToolBox[1][1], 0, s, c, kToolBox[1][2]};
    objects_.SetObject(1, lid);
    const float wrench[12] = {1, 0, 0, kToolBox[2][0], 0, 1, 0, game_.wrench, 0, 0, 1, kToolBox[2][2]};
    objects_.SetObject(2, wrench);
}

// func_0025E630. 1: stopped on the box is the bump, CLEAR and green off; falling is MISS; going back gives up; the
// time. 2: the lid opens and the wrench rises for 45 frames. 3: no green or pause; 4: 33 frames, then the fade over
// 64 frames, the banners lit over it.
bool Race::Race_VolcanoTask() {
    GameState& g = game_;
    switch (g.state) {
    case 1: {
        const CarBody& me = bodies_[0];
        if (me.pos[1] >= kBoxY0 && me.pos[1] < kBoxY1 && me.pos[0] > kBoxX0 && me.pos[0] < kBoxX1 &&
            me.pos[2] > kBoxZ0 && me.pos[2] < kBoxZ1) {
            audio::Sound_PlaySe(kSeBump);
            go_ = false;
            noPause_ = true;
            braked_[0] = true;
            g.state = 2;
            if (Stamp_Earned(kVolcanoStamp)) result_.outcome = g.time < kVolcanoRecord ? 2 : 0;
            else result_.outcome = 1;
            Event_SetClear(0);
        }
        if (me.pos[1] <= kVolcanoFall) {
            Event_SetMiss(0);
            result_.outcome = 0;
            g.state = 3;
            braked_[0] = true;
        }
        if (me.pos[0] <= kVolcanoBack) {
            result_.outcome = 0;
            g.state = 3;
            braked_[0] = true;
        }
        if (go_ && !paused_) ++g.time;
        Event_SetTime(g.time);
        break;
    }
    case 2:
        ++g.lid;
        g.wrench += kWrenchRise;
        if (g.lid == kLidOpen) g.state = 3;
        break;
    case 3:
        go_ = false;
        noPause_ = true;
        g.count = 0;
        g.state = 4;
        // fall through
    case 4:
        if (g.count > 0x40) event_.fadeAll = g.count - 0x40;
        if (g.count < 0xC0) g.count += 2;
        else g.done = true;
        break;
    default:
        break;
    }
    Volcano_PlaceObjects();
    return g.done;
}

// func_00254D20. 1: under height 275 is MISS (Race_Outcome 1); at rest (the place as last frame and no speed) under
// 350 is CLEAR (2). 2: no pause, the car braked and stopped; 3: 129 frames with the banner, then the song fades and
// the HUD goes (race flag 0x10000); 4: the fade to black over 64 frames. The canvas hides while paused.
bool Race::Race_RainbowTask() {
    GameState& g = game_;
    CarBody& me = bodies_[0];
    switch (g.state) {
    case 1:
        if (me.pos[1] < kRainbowFall) {
            Event_SetMiss(0);
            result_.outcome = 1;
            g.state = 2;
        } else if (me.pos[1] < kRainbowTop && !memcmp(g.last, me.pos, sizeof g.last) && !me.vel[0] && !me.vel[1] &&
                   !me.vel[2]) {
            Event_SetClear(0);
            result_.outcome = 2;
            g.state = 2;
        }
        memcpy(g.last, me.pos, sizeof g.last);
        if (g.state != 2) break;
        // fall through
    case 2:
        noPause_ = true;
        braked_[0] = true;
        me.vel[0] = me.vel[1] = me.vel[2] = 0;
        g.state = 3;
        g.count = 0;
        // fall through
    case 3:
        if (g.count++ < 0x80) break;
        audio::Sound_FadeOutSong();
        g.hidden = true;
        g.count = 0;
        g.state = 4;
        // fall through
    case 4:
        event_.fadeAll = g.count << 1;
        if (g.count++ < 0x40) break;
        g.done = true;
        break;
    default:
        break;
    }
    return g.done;
}

// func_002638C0. 1: the first car within 1.5 of the flag wins: the player WIN! (Race_Outcome 2), Micky LOSE (1), the
// flag's sound either way and the flag gone; both on one frame is both banners and 1. 2: no pause, race over, both
// braked; 3: 64 frames, then the fade over 128, the banners lit over it.
bool Race::Race_BeachFlagTask() {
    GameState& g = game_;
    switch (g.state) {
    case 1:
        if (Distance(kFlag, bodies_[0].pos) < kFlagReach) {
            result_.outcome = 2;
            Event_SetWin(0);
            audio::Sound_PlaySe(kSeFlag);
            g.state = 2;
            g.taken = true;
        }
        if (carCount_ > 1 && Distance(kFlag, bodies_[1].pos) < kFlagReach) {
            result_.outcome = 1;
            Event_SetLose(0);
            audio::Sound_PlaySe(kSeFlag);
            g.state = 2;
            g.taken = true;
        }
        break;
    case 2:
        noPause_ = true;
        braked_[0] = braked_[1] = true;
        g.state = 3;
        break;
    case 3:
        if (g.count > 0x40) event_.fadeAll = g.count - 0x40;
        if (g.count < 0xC0) ++g.count;
        else g.done = true;
        break;
    default:
        break;
    }
    // The flag: object 0 at its place until it's reached (its w at 0 leaves it undrawn).
    if (!g.taken) {
        const float m[12] = {1, 0, 0, kFlag[0], 0, 1, 0, kFlag[1], 0, 0, 1, kFlag[2]};
        objects_.SetObject(0, m);
    } else {
        objects_.HideObject(0);
    }
    return g.done;
}

// A00's chest: sceVu0RotMatrixY by -pi/2, the lid then RotMatrixZ by its angle.
void Race::Maze_PlaceObjects() {
    for (int k = 0; k < 3; ++k) {
        float a = k == 1 ? LidAngle(game_.lid) : 0.0f, c = cosf(a), s = sinf(a);
        // Rz(a) Ry(-pi/2).
        const float m[12] = {0, -s, -c, kChest[k][0], 0, c, -s, kChest[k][1], 1, 0, 0, kChest[k][2]};
        objects_.SetObject(k, m);
    }
}

// func_0025F6A0. 1: into the chest's square is the bump, CLEAR, no pause and the brake; out of the entrance gives
// up; the time. 2: the lid opens over 45 frames; 3: no pause; 4: 64 frames, then the fade over 128, CLEAR lit over
// it. Its end also sets race flag 0x400000 (the picture held for the loader).
bool Race::Race_MazeTask() {
    GameState& g = game_;
    switch (g.state) {
    case 1: {
        const CarBody& me = bodies_[0];
        if (me.pos[0] > kChestX0 && me.pos[0] < kChestX1 && me.pos[2] > kChestZ0 && me.pos[2] < kChestZ1) {
            audio::Sound_PlaySe(kSeBump);
            Event_SetClear(0);
            g.state = 2;
            noPause_ = true;
            braked_[0] = true;
            result_.outcome = g.time < kMazeRecord && Stamp_Earned(kMazeStamp) ? 2 : 1;
        }
        if (me.pos[0] >= kEntranceX0 && me.pos[0] <= kEntranceX1 && me.pos[2] <= kEntranceZ) {
            g.state = 3;
            braked_[0] = true;
            result_.outcome = 0;
        }
        if (go_ && !paused_) ++g.time;
        Event_SetTime(g.time);
        break;
    }
    case 2:
        if (++g.lid == kLidOpen) g.state = 3;
        break;
    case 3:
        g.count = 0;
        noPause_ = true;
        g.state = 4;
        // fall through
    case 4:
        if (g.count > 0x40) event_.fadeAll = g.count - 0x40;
        if (g.count < 0xC0) ++g.count;
        else g.done = true;
        break;
    default:
        break;
    }
    Maze_PlaceObjects();
    return g.done;
}

void Race::Event_SetNewRecord(int side) {
    HudElement& e = elements_[kElemWin + side * 2];
    if (e.type != 3) SetSprite(e, 3, kNewRecord, 12, players_ == 1 ? 0xA8 : side ? 0x150 : 0x70);
}

void Race::Curling_SetTryBanner(int n) {
    HudElement& e = elements_[kElemCountdown];
    if (n == 0) {
        e.type = 0;
        return;
    }
    SetSprite(e, 3, kTryBanner[n - 1], 0, players_ > 1 ? 0xE0 : 0x70);
    e.picture = 0;
    e.frames = 10;
}

// func_002631A0. 0: the car back at the start (Car_Stop, Car_SetPlace), the try's banner, the HUD back, Race_Outcome
// 0; 1: after 120 frames the board, green and the pause (and the song on the first try); the car over z 700 brakes
// for good and slides. 2: once it stands still, the points by its distance from the target's centre; the third try
// totals them, the record and the outcome. 3: 180 frames of the points, then the HUD hidden and on to 4 (the fade
// between tries; 6 puts the car back for the next) or 7-8 (the end's fade).
bool Race::Race_CurlingTask() {
    GameState& g = game_;
    CarBody& me = bodies_[0];
    switch (g.state) {
    case 0:
        Car_Reset(me, kCurlingStart, 0x8000);
        Car_WorldPosition(me, cars_[0].pos);
        Curling_SetTryBanner(g.attempt + 1);
        g.hidden = false;
        g.state = 1;
        result_.outcome = 0;
        // fall through
    case 1:
        if (++g.frames >= kCurlingWait) {
            g.board = true;
            Curling_SetTryBanner(0);
            go_ = true;
            noPause_ = false;
            // The task calls Sound_StartSong every frame until the line on the first try; the port starts it once.
            if (g.attempt == 0 && g.frames == kCurlingWait) audio::Sound_StartSong();
        }
        if (me.pos[2] <= kCurlingLine) {
            g.state = 2;
            g.frames = 0;
            braked_[0] = true;
        }
        break;
    case 2: {
        // Vec_SpeedKmh's float bits: the car stands still when its speed across the ground is exactly 0.
        if (me.vel[0] || me.vel[2]) break;
        float d = Distance(kCurlingTarget, me.pos);
        int points = 0;
        for (int i = 0; i < 4; ++i) {
            if (d < kRingRadius[i]) {
                points = kRingPoints[i];
                result_.outcome = 1;
                break;
            }
        }
        // Curling_SetPoints (func_002354F0): the try's points, and the total of those over 0.
        g.points[g.attempt] = points;
        g.points[3] = 0;
        for (int k = 0; k < 3; ++k)
            if (g.points[k] > 0) g.points[3] += g.points[k];
        int total = g.points[3];
        if (g.attempt != 2) {
            g.bigScore = true;
            g.state = 3;
        } else {
            // Curling_CheckRecord (func_00235570): a total over the record is saved as the record.
            SaveRecord& save = Save_Current();
            bool record = (uint32_t)save.curlingBest < (uint32_t)total;
            if (record) save.curlingBest = total;
            bool stamp = Stamp_Earned(kCurlingStamp);
            if ((record && stamp) || (total >= kCurlingWin && !stamp)) {
                // A win holds state 2 for 120 frames, scoring again each frame: with stamp 70 earned the record
                // check then fails and the next frame takes the loss (by the asm; not seen on the PS2).
                result_.outcome = 2;
                if (++g.frames < kCurlingWait) {
                    g.bigScore = true;
                } else {
                    g.frames = 0;
                    g.bigScore = false;
                    Event_SetNewRecord(0);
                    g.state = 3;
                }
            } else {
                result_.outcome = total == kCurlingPerfect ? 3 : 1;
                g.bigScore = true;
                g.state = 3;
            }
        }
        noPause_ = true;
        break;
    }
    case 3:
        if (++g.frames >= kCurlingShow) {
            g.frames = 0;
            g.hidden = true;
            g.bigScore = false;
            go_ = false;
            noPause_ = true;
            g.state = g.attempt < 2 ? 4 : 7;
        }
        break;
    case 4:
        event_.fadeAll = g.count;
        if (g.count >= 0x80) {
            g.count = 0;
            g.state = 6;
        } else {
            ++g.count;
        }
        break;
    case 6:
        event_.fadeAll = 0x80;
        Car_Reset(me, kCurlingStart, 0x8000);
        Car_WorldPosition(me, cars_[0].pos);
        ++g.attempt;
        g.state = 0;
        braked_[0] = false;
        g.board = false;
        break;
    case 7:
        noPause_ = true;
        g.state = 8;
        audio::Sound_FadeOutSong();
        break;
    case 8:
        event_.fadeAll = g.count;
        if (g.count < 0x80) ++g.count;
        else g.done = true;
        break;
    default:
        break;
    }
    return g.done;
}

// func_00263E68. The in-run (phase 0): past z 1100 the car is off; off the start platform's left edge is the quit
// (Race_Outcome 0). In the air (1): the jump's length 1100 - z, until the car is down on the ground under it. The
// run-out (2): the brake, the task slowing the car 0x98 a frame until it stands still; then the record, and NEW
// RECORD (2) for 140 metres with a record or stamp 0x42 yet to earn, else FINISH! (1). 2: race over; 3: the fade.
bool Race::Race_SkiJumpTask() {
    GameState& g = game_;
    CarBody& me = bodies_[0];
    switch (g.state) {
    case 1:
        if (g.phase == 0) {
            if (me.pos[2] <= kTakeOff) g.phase = 1;
            if (me.pos[2] >= kQuitZ && me.pos[0] <= kQuitX) {
                result_.outcome = 0;
                g.state = 2;
                braked_[0] = true;
            }
            g.distance = (int)(kTakeOff - me.pos[2]);
        } else if (g.phase == 1) {
            float p[4] = {me.pos[0], me.pos[1], me.pos[2], 1.0f}, ceiling;
            collision_.Collision_Ground(p, &ceiling);
            g.distance = (int)(kTakeOff - me.pos[2]);
            if (me.pos[1] <= p[1]) {
                g.phase = 2;
                noPause_ = true;
            }
        } else {
            float vx = me.vel[0] / 32768.0f, vz = me.vel[2] / 32768.0f;
            if ((int)(sqrtf(vx * vx + vz * vz) * 216.0f) < 2) me.vel[0] = me.vel[2] = 0;
            for (int k = 0; k < 3; k += 2) {
                if (me.vel[k] < 0) me.vel[k] += kRunOutSlow;
                if (me.vel[k] > 0) me.vel[k] -= kRunOutSlow;
            }
            me.brakeTime = 1;
            braked_[0] = true;
            if (!me.vel[0] && !me.vel[2]) {
                // SkiJump_CheckRecord (func_00235698): a jump over the record is saved as the record.
                SaveRecord& save = Save_Current();
                bool record = (uint32_t)save.skiJumpBest < (uint32_t)g.distance;
                if (record) save.skiJumpBest = g.distance;
                if (g.distance >= kSkiJumpWin && (record || !Stamp_Earned(kSkiJumpStamp))) {
                    result_.outcome = 2;
                    Event_SetNewRecord(0);
                } else {
                    result_.outcome = 1;
                    Event_SetFinish(0);
                }
                g.state = 2;
                me.brakeTime = 0;
            }
        }
        break;
    case 2:
        noPause_ = true;
        g.state = 3;
        break;
    case 3:
        event_.fadeAll = g.count;
        if (g.count < 0x80) ++g.count;
        else g.done = true;
        break;
    default:
        break;
    }
    return g.done;
}

// func_0025D7C8. A fish turns 0x400 a frame towards its heading (snapping within 0x3FF), and once it faces it picks
// a new one away from the fence it's near (by the frame count); it swims nose first at its kind's speed, a unit
// above the sea floor, and the car within 2.5 of it after green catches it (one a frame).
void Race::Fish_Task(int i) {
    Fish& f = fish_[i];
    uint16_t d = (uint16_t)(f.yaw - f.target), nd = (uint16_t)-d;
    if (f.caught) {
        objects_.HideObject(i);
        return;
    }
    float p[4] = {f.pos[0], f.pos[1], f.pos[2], 1.0f}, ceiling;
    collision_.Collision_Ground(p, &ceiling);   // D_00335710 (func_002080D8(1): the course's)
    f.pos[1] = p[1] + 1.0f;
    if ((uint32_t)((int32_t)f.yaw - (int32_t)f.target + 0x3FF) < 0x7FF) f.yaw = f.target;
    if (nd >= d) f.yaw -= 0x400;
    if (d >= nd) f.yaw += 0x400;
    uint32_t frames = (uint32_t)Info_Frames();
    if (f.yaw == f.target) {
        uint16_t k = (uint16_t)((frames << 8) & 0x7F00);
        float x = f.pos[0], z = f.pos[2];
        if (x < kFence0) f.target = (uint16_t)(k + 0x4000);
        if (z < kFence0) f.target = k;
        if (x >= kFence1) f.target = (uint16_t)(k - 0x4000);
        if (z >= kFence1) f.target = (uint16_t)(k + 0x8000);
    } else {
        if (d == nd) f.target = (uint16_t)((frames << 8) & 0xFF00);
        for (int c = 0; c < 3; c += 2) {
            if (f.pos[c] < kFence0) f.pos[c] = kFence0;
            if (f.pos[c] >= kFence1) f.pos[c] = kFence1;
        }
    }
    if (Distance(f.pos, bodies_[0].pos) < kCatchReach && !game_.catchSignal && go_) {
        audio::Sound_PlaySe(kSeFlag);
        f.caught = true;
        objects_.HideObject(i);
        game_.catchSignal = true;
        return;
    }
    // The step: +0x18 = sinf (func_00274A10), +0x10 = -cosf (func_00274938), so along the model's -x, nose first.
    float a = (int16_t)f.yaw * kPi / 32768.0f, c = cosf(a), s = sinf(a);
    f.pos[0] += -c * kFishSpeed[i & 3];
    f.pos[2] += s * kFishSpeed[i & 3];
    // sceVu0UnitMatrix, RotMatrixY by the heading, the fish's place.
    const float m[12] = {c, 0, s, f.pos[0], 0, 1, 0, f.pos[1], -s, 0, c, f.pos[2]};
    objects_.SetObject(i, m);
}

// func_0025DBB8. 0: the 30 fish on the diagonal x = z from the frame count; 1: each catch counted, the 20th FINISH!
// (Race_Outcome 1); 2: no pause, the brake; 3: from 0x40 the fade, 1 a frame to 0xC0, the count and FINISH! lit.
bool Race::Race_FishingTask() {
    GameState& g = game_;
    switch (g.state) {
    case 0: {
        uint64_t frames = Info_Frames();
        for (int i = 0; i < kFishes; ++i) {
            Fish& f = fish_[i];
            int64_t r = (int64_t)(frames * i * kLaneMix[i & 15]);
            f.pos[0] = f.pos[2] = (float)(r % 160 + 720);
            f.pos[1] = 0;
            f.yaw = f.target = (uint16_t)(r % 0xFFFF);
            f.caught = false;
        }
        g.state = 1;
        // fall through
    }
    case 1:
        if (g.catchSignal) {
            ++g.caught;
            g.catchSignal = false;
        }
        if (g.caught == kFishToCatch) {
            result_.outcome = 1;
            Event_SetFinish(0);
            g.state = 2;
        }
        break;
    case 2:
        noPause_ = true;
        g.state = 3;
        braked_[0] = true;
        g.count = 0;
        break;
    case 3:
        if (g.count > 0x40) event_.fadeAll = g.count - 0x40;
        if (g.count < 0xC0) ++g.count;
        else g.done = true;
        break;
    default:
        break;
    }
    // The fish's tasks run after their parent's; after the 20th the catch signal stays up and no more are caught.
    for (int i = 0; i < kFishes; ++i) Fish_Task(i);
    if (g.state >= 2) g.catchSignal = true;
    return g.done;
}

void Race::Event_SetTimeUp(int side) {
    HudElement& e = elements_[kElemMissCourse + side];
    if (e.type == 2) return;
    SetSprite(e, 2, kTimeUp, 1, players_ == 1 ? 0xA8 : side ? 0x150 : 0x70);
    e.picture = 0;
}

// Car_ResetForRespawn and Car_SetPlace facing +z, the car keeping its forward (z) speed.
void Race::WhichWay_PutCar(const float* pos, int32_t vz) {
    Car_Reset(bodies_[0], pos, 0);
    bodies_[0].vel[2] = vz;
    Car_WorldPosition(bodies_[0], cars_[0].pos);
}

// func_00262678. 1: a new round on one of the three stretches (from the frame count), keeping the car's speed, with the
// round's right tile and a decoy and the side the right road is on. 2: past the fork's line the side decides (the
// fifth right fork Race_Outcome 1, a wrong one 0); the round's time runs down to TIME UP; the plate and signs copy
// their tiles. After the fifth the run to CLEAR! at z 432; after a miss MISS and the car rolled off on its own. 3:
// race over; 4: the fade, 2 a frame. 5: the round's fade out, 16 a frame; 7: black, then the next round (no fade in),
// or the run to the goal or the miss's, which 6 fades in.
bool Race::Race_WhichWayTask() {
    GameState& g = game_;
    CarBody& me = bodies_[0];
    uint64_t frames = Info_Frames();
    switch (g.state) {
    case 0:
        // The countdown comes from here, a frame later than the other events' (not kept by the port).
        g.attempt = 0;
        g.state = 1;
        // fall through
    case 1: {
        g.stretch = (int)((frames >> 1) % 3);
        WhichWay_PutCar(kWayStart[g.stretch], me.vel[2]);
        const WayRound& r = kWayRounds[g.attempt];
        int old = g.right;
        int f = (int)(uint8_t)(frames % r.count);
        if (f == old) f = (uint8_t)(old + 2);
        int dcy = (int)(uint8_t)((frames >> 8) % r.count);
        if (dcy == f) dcy = (uint8_t)(dcy + 2);
        g.right = f % r.count;
        g.decoy = dcy % r.count;
        g.sign = (frames & 1) ? 1 : -1;
        g.state = 2;
        g.frames = kWayTime[g.stretch];
        // fall through
    }
    case 2:
        if (g.result == 0) {
            if (LineSide(kWayFork[g.stretch], me.pos[0], me.pos[2]) >= 0) {
                noPause_ = true;
                if (LineSide(kWaySide[g.stretch], me.pos[0], me.pos[2]) * g.sign >= 0) {
                    if (g.attempt >= 4) {
                        result_.outcome = 1;
                        g.result = 1;
                    }
                } else {
                    result_.outcome = 0;
                    g.result = 2;
                }
                g.state = 5;
            }
            if (go_ && !paused_) --g.frames;
            if (g.frames == 0) {
                noPause_ = true;
                Event_SetTimeUp(0);
                g.state = 3;
                result_.outcome = 0;
            }
            g.timeLimit = g.frames / 60;
            g.timeShown = true;
        } else if (g.result == 1) {
            if (me.pos[2] >= kWayGoalZ) {
                Event_SetClear(0);
                g.state = 3;
            }
        } else if (g.result == 2) {
            Event_SetMiss(0);
            me.vel[2] = kWayRoll;
            me.vel[0] = 0;
            if (me.pos[1] <= kWayDropY || me.pos[2] >= kWayDropZ) g.state = 3;
        }
        // The plate shows the right tile, and the sign on the right road's side the same.
        {
            const WayRound& r = kWayRounds[g.attempt];
            const int* right = r.tiles[g.right];
            const int* decoy = r.tiles[g.decoy];
            objects_.CopyTile(0, right[0], right[1]);
            const int* minus = g.sign > 0 ? decoy : right;
            const int* plus = g.sign > 0 ? right : decoy;
            objects_.CopyTile(1, minus[0], minus[1]);
            objects_.CopyTile(2, plus[0], plus[1]);
        }
        break;
    case 3:
        noPause_ = true;
        g.state = 4;
        break;
    case 4:
        event_.fadeAll = g.count;
        if (g.count < 0x80) g.count += 2;
        else g.done = true;
        break;
    case 5:
        event_.fadeAll = g.count;
        if (g.count < 0x80) g.count += 0x10;
        else g.state = 7;
        break;
    case 6:
        event_.fadeAll = g.count;
        if (g.count > 0) g.count -= 0x10;
        else g.state = 2;
        break;
    case 7:
        event_.fadeAll = 0x80;
        if (g.result == 0) {
            g.count = 0;
            ++g.attempt;
            g.state = 1;
            noPause_ = false;
        } else {
            // HudCanvas_InitWhichWay again: TIME LIMIT and the banners go.
            for (HudElement& e : elements_) e = HudElement();
            g.timeShown = false;
            if (g.result == 1) {
                WhichWay_PutCar(kWayGoalRun, me.vel[2]);
            } else {
                braked_[0] = true;
                WhichWay_PutCar(kWayMissRun, kWayRoll);
            }
            g.frames = 0;
            g.state = 6;
        }
        break;
    default:
        break;
    }
    return g.done;
}

void Race::Race_DrawGamePieces() {
    switch (task_) {
    case kTaskCurling: {
        CurlingBoard b = {game_.board, game_.bigScore, game_.attempt, {}, Save_Current().curlingBest};
        for (int k = 0; k < 4; ++k) b.points[k] = game_.points[k];
        Curling_DrawBoard(d_, b);
        break;
    }
    case kTaskSkiJump:
        SkiJump_DrawDistance(d_, game_.distance, Save_Current().skiJumpBest);
        break;
    case kTaskFishing:
        Fishing_DrawCount(d_, game_.caught);
        break;
    case kTaskWhichWay:
        if (game_.timeShown) WhichWay_DrawTimeLimit(d_, game_.timeLimit);
        break;
    case kTaskGolf: {
        const GolfState& g = golf_;
        if (g.panel) Golf_DrawPanel(d_, g.hole, g.panelYards, g.panelToGo, Golf_Par(g.hole), g.panelStrokes);
        break;
    }
    default:
        break;
    }
}

// The games' first frames (Race_EventStart): the song (BGM_05 for all of them) and their cars.
void Race::Race_GameStart() {
    memset(&game_, 0, sizeof game_);
    game_.state = 1;
    game_.lid = kLidShut;
    game_.wrench = kToolBox[2][1];
    audio::Sound_LoadSong(0x0A100201);   // BGM_05
    int cars = carCount_ < 2 ? carCount_ : 2;
    switch (task_) {
    case kTaskDrag:
        // State 0: both cars to the start; car 1's +0x19A at 1 lets Johnny steer from the start (Ai_Drive, path 0x16).
        for (int i = 0; i < cars; ++i) Car_Place(bodies_[i], kDragStart[i], 0);
        if (cars > 1) lap_[1].zone = 1;
        break;
    case kTaskRainbow:
        // State 0: Car_ResetForRespawn and Car_SetPlace at the start (the grid's place too), facing +x.
        Car_Reset(bodies_[0], kRainbowStart, 0x4000);
        break;
    case kTaskCurling:
        // The task's frame 0: the points none yet, no countdown; the song waits for the first try's start.
        for (int& p : game_.points) p = -1;
        game_.state = 0;
        noPause_ = true;
        break;
    case kTaskSkiJump:
        // State 0 (frame 1): the car to the in-run's top facing -z (the grid's place too).
        Car_Place(bodies_[0], kSkiStart, 0x8000);
        break;
    case kTaskFishing:
        game_.state = 0;
        break;
    case kTaskWhichWay:
        game_.state = 0;
        break;
    case kTaskFigure8:
        // The task's first frame: every object draws the cone (model 0), the gate 0, no countdown till a step.
        figure8_ = Figure8State();
        objects_.ShareModel0();
        countdownStart_ = -1;
        noPause_ = true;
        break;
    case kTaskGolf:
        // The task's frame 0: the view off with the cars muted, black; frame 1: the ball's own picture (017).
        golf_ = GolfState();
        Roulette_HideView(true);
        objects_.SetModel0Picture(17);
        noPause_ = true;
        break;
    case kTaskRoulette:
        // No countdown and no pause (race flag 0x100 never comes); the task loads its song itself.
        roulette_ = RouletteState();
        hideView_ = overhead_ = false;
        noPause_ = true;
        break;
    case kTaskBeachFlag:
        // State 0: both cars facing -x; Micky (Ai_Drive, path 0x25) steers from the start.
        for (int i = 0; i < cars; ++i) Car_Place(bodies_[i], kFlagStart[i], 0xC000);
        if (cars > 1) lap_[1].zone = 1;
        break;
    default:
        // Rock Climbing, the Volcano Course and the maze start on the course's grid.
        break;
    }
}

}  // namespace game
