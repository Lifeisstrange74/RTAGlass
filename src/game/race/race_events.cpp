// The events' tasks: what runs in place of Race_Task on the Sliding Door Race (course 0x13) and the Obstacle Course
// (0x1D), in 2 Player (rows 7 and 8) and the Adventure (its rows 0x23 and 0x30). A high-level reimplementation of
// func_002595F0, func_0025EB70 and the Highway Race's func_0025FF00 (course 0x14, row 5; the Adventure's too) with
// their countdown (func_00233A80) and banners. See docs/events.md.
#include <string.h>

#include "../../audio/sound.h"
#include "race.h"
#include "../save/save_data.h"

namespace game {

namespace {

// func_00233A80, by the task's frame: the beep (CQ_MAIN 0x2D) at 60, green at 240, then the song and the pause at
// 300. A number comes up every 60 frames, each zooming in over 10 frames to twice its size.
const int kCountdownBeeps = 60, kCountdownGo = 240, kCountdownSong = 300;
const uint32_t kSeCountdown = 0x2D;
// Its sprite lists, {u, v, w, h}: "3", "2", "1", "GO!". The Sliding Door Race's (kind 0, D_0029F7E8) are in picture 1
// in palette 1, the Obstacle Course's (kind 2, D_0029F868) in picture 0 in palette 13.
const int kCountdownSprites[4][4] = {{0, 56, 32, 40}, {32, 56, 32, 40}, {64, 56, 32, 40}, {106, 48, 126, 42}};
const int kCountdownObstacleDown = 160;

// The banners (picture 1 of the course's SCOREAnn.GSL): WIN! (D_0029F908, palette 12; Soccer's D_0029F8E8,
// palette 0) zooms in over 30 frames to twice its size; LOSE (0x13: D_0029F948, palette 2; 0x1A: D_0029F968,
// palette 1; elsewhere D_0029F988, palette 13) and MISS (D_0029F9A8, palette 13) rock. Centred on x 320, at y 168
// with one player, 112 and 336 in each half with two.
const int kWin[4] = {0, 144, 160, 48}, kWinSoccer[4] = {0, 0, 160, 48};
const int kLoseSliding[4] = {0, 0, 176, 48}, kLose[4] = {0, 192, 168, 48}, kLoseSoccer[4] = {0, 48, 176, 48};
const int kMiss[4] = {0, 192, 176, 50};
// FINISH! (Hud_SetFinish, func_00234128: D_0029F6E8) in player p's element at +0x8590 + 0x408 p: picture 1, palette 0,
// zooming in to its own size, at y 112 (336 for player 2) even with one player.
const int kFinish[4] = {0, 0, 210, 56};
// CLEAR (Hud_SetClear, func_00234708: D_0029FA48), palette 0, zooming in, in the WIN! slot (the games played alone:
// Barrel Dodging, the Volcano Course, ...). Barrel Dodging's debug print calls it GOAL; SCOREA15's picture 1 has CLEAR
// there.
const int kClear[4] = {0, 0, 230, 48};
// MISS COURSE! (func_00233158, at +0x8748), rocking: on 0x14 D_0029F9C8 in palette 3, on 0x19 D_0029F9E8 in
// palette 1, elsewhere D_0029FA08 in palette 1.
const int kMissCourseHighway[4] = {0, 0, 256, 36}, kMissCourse19[4] = {0, 56, 256, 36};
const int kMissCourse[4] = {0, 48, 256, 50};

// The Sliding Door Race (func_002595F0, func_00259498): both cars start at x 720 facing -x; stopping (the speed
// exactly 0) inside the goal, x 222.5-235.75 and z 83-117, wins; stopping anywhere past x 710 short of it, or
// going below x 175 or under y 0 (the pond), fails.
const float kSlidingStart[2][3] = {{720.0f, 6.0f, 97.5f}, {720.0f, 6.0f, 102.5f}};
const uint16_t kSlidingYaw = 0xC000;
const float kGoalX0 = 222.5f, kGoalX1 = 235.75f, kGoalZ0 = 83.0f, kGoalZ1 = 117.0f;
const float kStartX = 710.0f, kPondX = 175.0f;
// One player: the opponent (car 1) is stopped once past x 231, in the goal.
const float kOpponentStop = 231.0f;

// The Obstacle Course (func_0025EB70): gates {x0, z0, x1, z1} (D_002C4C50); a car on gate n has passed lines 1..n,
// and passing line n + 1 takes it on (back over line n back again). Line 14 is the goal.
const float kGates[15][4] = {
    {306.5f, 282.2f, 306.5f, 286.5f}, {200, 149.5f, 0, 149.5f}, {200, 400, 0, 400},     {200, 600, 0, 600},
    {200, 760, 0, 760},               {240, 800, 240, 1000},    {700, 800, 700, 1000},  {800, 1300, 600, 1300},
    {800, 1400, 800, 1500},           {1400, 1400, 1400, 1500}, {1400, 1060, 1500, 1060}, {1400, 860, 1500, 860},
    {1400, 660, 1500, 660},           {1400, 480, 1500, 480},   {1400, 281, 1500, 281},
};
const int kGoalGate = 14;
// Where a car that fell is put back, by its gate: {x, y, z, yaw} for player 1 (D_002C4D40) and player 2
// (D_002C4E20), 4 higher. -1 where no fall is possible (the height below is 0).
const float kRespawn[2][14][4] = {
    {{-1, -1, -1, 0},       {164, 134, 102.5f, 0}, {146, 134, 430, 0},     {130, 108, 650, 0},
     {-1, -1, -1, 0},       {327, 69.6f, 841, 16384}, {-1, -1, -1, 0},     {-1, -1, -1, 0},
     {-1, -1, -1, 0},       {1460, 61, 1180, 32768}, {1445, 61, 940, 32768}, {1460, 61, 750, 32768},
     {1455, 61, 560, 32768}, {1461, 61, 370, 32768}},
    {{-1, -1, -1, 0},       {168.5f, 134, 102.5f, 0}, {150, 134, 430, 0},  {134, 108, 650, 0},
     {-1, -1, -1, 0},       {327, 69.6f, 837, 16384}, {-1, -1, -1, 0},     {-1, -1, -1, 0},
     {-1, -1, -1, 0},       {1456, 61, 1180, 32768}, {1441, 61, 940, 32768}, {1456, 61, 750, 32768},
     {1451, 61, 560, 32768}, {1457, 61, 370, 32768}},
};
// A car at or under this height on its gate fell (D_002C4F00).
const uint8_t kFallHeight[14] = {0, 100, 60, 60, 0, 10, 0, 0, 0, 50, 50, 50, 50, 50};
// The starts (x 165 with one player, 162.5 and 167.5 with two, y 134, z 102.5, facing +z); one player going back over
// this line (D_002C4F10) on gate 0 is out.
const float kObstacleStart[3] = {165.0f, 162.5f, 167.5f};
const float kObstacleStartY = 134.0f, kObstacleStartZ = 102.5f;
const float kStartLine[4] = {250, 95, 250, 110};
// Two players: a car still before gate 1 can't go past x 238.5.
const float kObstacleFence = 238.5f;
// One player: under 7200 frames (2 minutes) wins (Race_Outcome 2), slower only finishes (1).
const int kObstacleTime = 0x1C20;

// The Highway Race (func_0025FF00): both cars start at x 412 facing -x, player 1 (or the one player) at z 871 and
// the other car at z 875.2. A car is off the course (MISS COURSE!) past the highway's edge in five of the AI path's
// zones: z under 806 in zones 0x13 and 0x54, 994 or more in 0x14, 0x40 and 0x41. Reaching zone 0x55 after zone 0x14
// (and not back before it since) is the goal.
const float kHighwayStart[2][3] = {{412.0f, 53.0f, 871.0f}, {412.0f, 53.0f, 875.2f}};
const uint16_t kHighwayYaw = 0xC000;
const int kHighwayTurn = 0x14, kHighwayGoal = 0x55;
const float kHighwayNear = 806.0f, kHighwayFar = 994.0f;
// One player: a win in 3000 frames or under is Race_Outcome 3, slower 2.
const int kHighwayTime = 3000;

bool Highway_OffCourse(int zone, float z) {
    switch (zone) {
    case 0x13:
    case 0x54:
        return z < kHighwayNear;
    case 0x14:
    case 0x40:
    case 0x41:
        return z >= kHighwayFar;
    default:
        return false;
    }
}

// Which side of a gate's line (x0, z0)-(x1, z1) the point is on: >= 0 past it.
float GateSide(const float* g, const float* pos) {
    return (g[3] - g[1]) * (pos[0] - g[2]) - (g[2] - g[0]) * (pos[2] - g[3]);
}

void SetElement(HudElement& e, int type, const int* sprite, int picture, int palette, int x, int y) {
    memset(&e, 0, sizeof e);
    e.type = type;
    e.x = x;
    e.y = y;
    e.picture = picture;
    e.palette = palette;
    e.u = sprite[0];
    e.v = sprite[1];
    e.w = sprite[2];
    e.h = sprite[3];
    e.frames = 30;
    e.scale = 2.0f;
}

}  // namespace

Race::RaceTask Race::Race_TaskFor(int course) {
    switch (course) {
    case 0x13: return kTaskSlidingDoor;
    case 0x14: return kTaskHighway;
    case 0x1A: return kTaskSoccer;
    case 0x1D: return kTaskObstacle;
    case 0x21: return kTaskBarrel;
    case 0x16: return kTaskDrag;
    case 0x15: return kTaskRockClimb;
    case 0x1F: return kTaskVolcano;
    case 0x1B: return kTaskRainbow;
    case 0x25: return kTaskBeachFlag;
    case 0x12: return kTaskMaze;
    case 0x20: return kTaskCurling;
    case 0x23: return kTaskSkiJump;
    case 0x24: return kTaskFishing;
    case 0x1E: return kTaskWhichWay;
    case 0x18: return kTaskRoulette;
    case 0x19: return kTaskFigure8;
    case 0x17: return kTaskGolf;
    default: return kTaskRace;
    }
}

void Race::Race_EventStart() {
    memset(&event_, 0, sizeof event_);
    eventTime_ = -1;
    for (HudElement& e : elements_) e = HudElement();
    int cars = carCount_ < 2 ? carCount_ : 2;
    if (task_ == kTaskSlidingDoor) {
        // func_0021C928 for cars 0 and 1; car 1's +0x19A at 1 lets the opponent steer from the start.
        for (int i = 0; i < cars; ++i) Car_Place(bodies_[i], kSlidingStart[i], kSlidingYaw);
        if (cars > 1) lap_[1].zone = 1;
        event_.phase = 1;
        audio::Sound_LoadSong(0x0A0D0201);   // ROOM_1
    } else if (task_ == kTaskHighway) {
        // State 0 puts both cars at the start; car 1's +0x19A (D_0177B05A) at 1 lets the opponent steer.
        for (int i = 0; i < cars; ++i) Car_Place(bodies_[i], kHighwayStart[i], kHighwayYaw);
        if (cars > 1) lap_[1].zone = 1;
        event_.state[0] = 1;
        event_.state[1] = players_ > 1 ? 1 : -1;
        audio::Sound_LoadSong(0x0A100201);   // BGM_05
    } else if (task_ == kTaskSoccer) {
        // The task's first frame: its block cleared and ROOM_1; there's no countdown task, and no pause (race flag
        // 0x100) until the first kick-off.
        memset(&soccer_, 0, sizeof soccer_);
        noPause_ = true;
        audio::Sound_LoadSong(0x0A0D0201);   // ROOM_1
    } else if (task_ >= kTaskDrag) {
        Race_GameStart();
    } else if (task_ == kTaskBarrel) {
        // The task's block cleared (HudCanvas_InitBarrel, func_002355A8, +0x8008 = 0x21) and BGM_05; the car stays
        // on the course's grid.
        memset(&barrel_, 0, sizeof barrel_);
        audio::Sound_LoadSong(0x0A100201);   // BGM_05
    } else {
        for (int p = 0; p < 2; ++p) {
            event_.state[p] = p < players_ ? 2 : -1;
            if (p >= cars || p >= players_) continue;
            float pos[3] = {kObstacleStart[players_ > 1 ? p + 1 : 0], kObstacleStartY, kObstacleStartZ};
            Car_Place(bodies_[p], pos, 0);
        }
        audio::Sound_LoadSong(0x0A100201);   // BGM_05
    }
    for (int i = 0; i < cars; ++i) Car_WorldPosition(bodies_[i], cars_[i].pos);
}

void Race::Race_EventCountdownTask() {
    if (frame_ > kCountdownSong) return;
    if (frame_ == kCountdownBeeps) audio::Sound_PlaySe(kSeCountdown);
    if (frame_ == kCountdownGo) go_ = true;
    HudElement& e = elements_[kElemCountdown];
    if (frame_ % 60 == 0) {
        int n = frame_ / 60;
        int sprite[4];
        memcpy(sprite, kCountdownSprites[n ? (n - 1) & 3 : 0], sizeof sprite);
        // Kind 2 (D_0029F868): courses 0x14, 0x15 and 0x1D; kind 1 (0x20, 0x24) draws kind 0's sprites from picture
        // 0; kind 3 (0x1E) puts them on the seam (y 224) even with one player.
        bool kind2 = id_ == 0x14 || id_ == 0x15 || id_ == 0x1D, kind1 = id_ == 0x20 || id_ == 0x24;
        bool seam = players_ > 1 || id_ == 0x1E;
        if (kind2) sprite[1] += kCountdownObstacleDown;
        SetElement(e, n ? 3 : 0, sprite, kind2 || kind1 ? 0 : 1, kind2 ? 13 : 1, 320, seam ? 0xE0 : 0x70);
        e.frames = 10;
    }
    if (frame_ == kCountdownSong) {
        audio::Sound_StartSong();
        e.type = 0;
    }
}

void Race::Event_SetWin(int side) {
    HudElement& e = elements_[kElemWin + side * 2];
    bool soccer = id_ == 0x1A;
    if (e.type != 3)
        SetElement(e, 3, soccer ? kWinSoccer : kWin, 1, soccer ? 0 : 12, 320,
                   players_ == 1 ? 0xA8 : side ? 0x150 : 0x70);
}

void Race::Event_SetLose(int side) {
    HudElement& e = elements_[kElemLose + side * 2];
    const int* sprite = id_ == 0x13 ? kLoseSliding : id_ == 0x1A ? kLoseSoccer : kLose;
    int palette = id_ == 0x13 ? 2 : id_ == 0x1A ? 1 : 13;
    if (e.type != 2) SetElement(e, 2, sprite, 1, palette, 320, players_ == 1 ? 0xA8 : side ? 0x150 : 0x70);
}

void Race::Event_SetFinish(int side) {
    HudElement& e = elements_[kElemFinish + side];
    if (e.type == 3) return;
    SetElement(e, 3, kFinish, 1, 0, 320, side ? 336 : 112);
    e.scale = 1.0f;
}

void Race::Event_SetClear(int side) {
    HudElement& e = elements_[kElemWin + side * 2];
    if (e.type != 3) SetElement(e, 3, kClear, 1, 0, 320, players_ == 1 ? 0xA8 : side ? 0x150 : 0x70);
}

void Race::Event_SetMiss(int side) {
    HudElement& e = elements_[kElemLose + side * 2];
    if (e.type != 2) SetElement(e, 2, kMiss, 1, 13, 320, players_ == 1 ? 0xA8 : side ? 0x150 : 0x70);
}

void Race::Event_SetMissCourse(int side) {
    HudElement& e = elements_[kElemMissCourse + side];
    if (e.type == 2) return;
    const int* sprite = id_ == 0x14 ? kMissCourseHighway : id_ == 0x19 ? kMissCourse19 : kMissCourse;
    SetElement(e, 2, sprite, 1, id_ == 0x14 ? 3 : 1, 320, players_ == 1 ? 0xA8 : side ? 0x150 : 0x70);
}

bool Race::Race_EventTask() {
    event_.fadeAll = event_.fadeHalf[0] = event_.fadeHalf[1] = 0;
    switch (task_) {
    case kTaskSlidingDoor: return Race_SlidingDoorTask();
    case kTaskHighway: return Race_HighwayTask();
    case kTaskSoccer: return Race_SoccerTask();
    case kTaskBarrel: return Race_BarrelTask();
    case kTaskDrag: return Race_DragTask();
    case kTaskRockClimb: return Race_RockClimbTask();
    case kTaskVolcano: return Race_VolcanoTask();
    case kTaskRainbow: return Race_RainbowTask();
    case kTaskBeachFlag: return Race_BeachFlagTask();
    case kTaskMaze: return Race_MazeTask();
    case kTaskCurling: return Race_CurlingTask();
    case kTaskSkiJump: return Race_SkiJumpTask();
    case kTaskFishing: return Race_FishingTask();
    case kTaskWhichWay: return Race_WhichWayTask();
    case kTaskRoulette: return Race_RouletteTask();
    case kTaskFigure8: return Race_Figure8Task();
    case kTaskGolf: return Race_GolfTask();
    default: return Race_ObstacleTask();
    }
}

// func_00259498: car `car` standing still in the goal wins (the first to); standing still short of it, or in the
// pond, fails. Either way its brake goes on (flag 0x800).
void Race::SlidingDoor_CheckCar(int car) {
    const CarBody& b = bodies_[car];
    int r;
    if (b.pos[0] < kPondX || b.pos[1] < 0) {
        r = -1;
    } else if (b.vel[0] || b.vel[1] || b.vel[2]) {
        r = 0;
    } else if (b.pos[0] < kGoalX1 && kGoalX0 < b.pos[0] && b.pos[2] < kGoalZ1 && b.pos[2] > kGoalZ0) {
        r = 1;
    } else {
        r = b.pos[0] < kStartX ? -1 : 0;
    }
    if (r == 0) return;
    braked_[car] = true;
    if (r == 1) {
        if (event_.winner == 0) event_.winner = car + 1;
    } else {
        event_.failed |= 1 << car;
    }
}

// func_002595F0. Phase 1 watches the cars until one stops on the goal (or, with one player, the player fails, or with
// two both fail); each player who failed shows MISS. Phase 2 sets Race_Outcome (the failures, then the winner: 2
// player 1, 1 player 2) and the banners: the winner's WIN!, and LOSE with the brake on for the loser if they hadn't
// failed. Then no pause, 64 frames, a 128-frame fade to black, and the end.
bool Race::Race_SlidingDoorTask() {
    EventState& ev = event_;
    if (ev.phase == 1) {
        bool toEnd = false, missOne = false;
        if (ev.winner == 0) {
            if (!(ev.failed & 1)) SlidingDoor_CheckCar(0);
            if (!(ev.failed & 2) && carCount_ > 1) SlidingDoor_CheckCar(1);
            if (ev.winner != 0) toEnd = true;
            else if (players_ > 1) missOne = true;
            else if (ev.failed & 1) toEnd = true;
        } else {
            toEnd = true;
        }
        if (toEnd) {
            ev.phase = 2;
            missOne = true;
        }
        if (missOne && (ev.failed & 1)) Event_SetMiss(0);
        if (players_ > 1 && (ev.failed & 2)) Event_SetMiss(1);
        if (ev.failed == 3) ev.phase = 2;
        if (players_ == 1 && carCount_ > 1 && bodies_[1].pos[0] < kOpponentStop) {
            bodies_[1].vel[0] = bodies_[1].vel[2] = 0;
            braked_[1] = true;
        }
        return false;
    }
    if (ev.phase == 2) {
        result_.outcome = ev.failed;
        if (ev.winner) {
            result_.outcome = ev.winner ^ 3;
            if (players_ > 1) {
                Event_SetWin(ev.winner >> 1);
                if (!(ev.failed & (ev.winner ^ 3))) {
                    Event_SetLose(ev.winner & 1);
                    braked_[ev.winner & 1] = true;
                }
            } else {
                if (ev.winner & 1) Event_SetWin(0);
                if (ev.winner == 2 && !(ev.failed & 1)) Event_SetLose(0);
            }
        }
        noPause_ = true;
        ev.phase = 3;
    }
    if (ev.phase == 3) {
        if (ev.fade > 0x40) ev.fadeAll = ev.fade - 0x40;
        if (ev.fade < 0xC0) ++ev.fade;
        else ev.phase = -1;
        return false;
    }
    return ev.phase == -1;
}

// func_0025EB70's part for player p's car (switch 1 for player 1, switch 2 for player 2). 2: racing: the gates, a
// fall (to 5), the goal (both to 3); 3: no pause (player 1's also fades the song out) and to 4; 4: hold 32 frames
// and fade that player's half out over 64, then the end; 5: the same, then 1: put back at the gate and to 6: fade
// back in, then racing again with the pause back.
void Race::Obstacle_PlayerTask(int p) {
    EventState& ev = event_;
    int& cp = ev.checkpoint[p];
    CarBody& b = bodies_[p];
    int& t = ev.count[p];
    switch (ev.state[p]) {
    case 1: {
        ev.fadeHalf[p] = 0x80;
        const float* r = kRespawn[p][cp < 14 ? cp : 13];
        float pos[3] = {r[0], r[1] + 4.0f, r[2]};
        // Car_Stop also puts out the jet (+0x20E) and stops its roar.
        if (b.jet) sounds_[p].Car_Jet(kJetStop, false);
        Car_Reset(b, pos, (uint16_t)(int16_t)(int)r[3]);
        Car_WorldPosition(b, cars_[p].pos);
        ev.state[p] = 6;
        braked_[p] = false;
        break;
    }
    case 2: {
        if (cp < kGoalGate && GateSide(kGates[cp + 1], b.pos) >= 0) ++cp;
        if (cp != 0 && GateSide(kGates[cp], b.pos) < 0) --cp;
        if (players_ == 1 && cp == 0 && GateSide(kStartLine, b.pos) >= 0) {
            result_.outcome = 0;
            ev.state[p] = 3;
            go_ = false;
        }
        if (cp < kGoalGate && b.pos[1] <= kFallHeight[cp]) {
            if (p == 0) result_.outcome = 0;
            braked_[p] = true;
            ev.state[p] = players_ > 1 ? 5 : 3;
            noPause_ = true;
        }
        if (cp == kGoalGate) {
            if (players_ > 1) {
                Event_SetWin(p);
                Event_SetLose(p ^ 1);
                braked_[0] = braked_[1] = true;
                ev.state[0] = ev.state[1] = 3;
                result_.outcome = p == 0 ? 2 : 1;
            } else {
                braked_[0] = true;
                ev.state[0] = 3;
                result_.outcome = 1;
                Event_SetFinish(0);
                if (ev.time <= kObstacleTime) result_.outcome = 2;
            }
        }
        // One player's time (+0xC, the TIME element: Hud_SetTime): from the first gate, while green.
        if (players_ == 1) {
            if ((cp != 0 || ev.time > 0) && go_ && !paused_) ++ev.time;
            Event_SetTime(ev.time);
        }
        break;
    }
    case 3:
        if (p == 0) audio::Sound_FadeOutSong();
        noPause_ = true;
        ev.state[p] = 4;
        break;
    case 4:
        if (t > 0x40) ev.fadeHalf[p] = t - 0x40;
        if (t >= 0xC0) ev.done = true;
        else t += 2;
        break;
    case 5:
        if (t > 0x40) ev.fadeHalf[p] = t - 0x40;
        if (t < 0xC0) t += 2;
        else ev.state[p] = 1;
        break;
    case 6:
        ev.fadeHalf[p] = t;
        if (t > 0) {
            t -= 2;
        } else {
            ev.state[p] = 2;
            noPause_ = false;
        }
        break;
    default:
        break;
    }
}

// func_0025EB70: each player's part, then (two players) the fence before gate 1: a car past x 238.5 goes back two
// moves with its engine at 0. The end: the cars' sounds off, black.
bool Race::Race_ObstacleTask() {
    for (int p = 0; p < players_ && p < carCount_; ++p) Obstacle_PlayerTask(p);
    if (players_ > 1) {
        for (int i = 0; i < 2 && i < carCount_; ++i) {
            CarBody& b = bodies_[i];
            if (event_.checkpoint[i] != 0 || b.pos[0] < kObstacleFence) continue;
            b.rpm = 0;
            b.fixed[2] = (b.fixed[2] - (b.vel[2] << 5) / 25) & 0x0FFFFFFF;
            b.fixed[0] = (b.fixed[0] - (b.vel[0] << 5) / 25) & 0x0FFFFFFF;
            b.fixed[1] -= (b.vel[1] << 5) / 25;
        }
    }
    return event_.done;
}

// func_0025FF00's state machine for player p (+4 player 1's, +6 player 2's; -1 none). 1: racing: off the course
// is MISS COURSE! and the other player's WIN! (one player: Race_Outcome 0); the goal is WIN! and the other's LOSE
// (one player: LOSE if the opponent got there first). Either way both go to 2: no pause, both braked, then 3: a
// count up 2 a frame, from 0x40 fading to black, and at 0xC0 the end. With one player the opponent at the goal
// first is state 5 in the second slot.
void Race::Highway_PlayerTask(int p) {
    EventState& ev = event_;
    int& t = ev.count[p];
    switch (ev.state[p]) {
    case 1: {
        int zone = ai_[p].zone;
        if (Highway_OffCourse(zone, bodies_[p].pos[2])) {
            Event_SetMissCourse(p);
            if (players_ > 1) {
                result_.outcome = p ? 2 : 1;
                Event_SetWin(p ^ 1);
            } else {
                result_.outcome = 0;
            }
            ev.state[0] = ev.state[1] = 2;
        }
        if ((ev.flags >> p & 1) && zone == kHighwayGoal) {
            if (p == 0 && ev.state[1] == 5) {
                Event_SetLose(0);
                result_.outcome = 1;
            } else {
                Event_SetWin(p);
                if (players_ > 1) {
                    Event_SetLose(p ^ 1);
                    result_.outcome = p ? 1 : 2;
                } else {
                    result_.outcome = ev.time <= kHighwayTime ? 3 : 2;
                }
            }
            ev.state[0] = ev.state[1] = 2;
        }
        // Player 1's part also watches the one player's opponent, and keeps both cars' "past zone 0x14" bits.
        if (p == 0 && players_ == 1 && (ev.flags & 2) && carCount_ > 1 && ai_[1].zone == kHighwayGoal)
            ev.state[1] = 5;
        for (int i = p; i < 2 && i < carCount_; ++i) {
            if (ai_[i].zone == kHighwayTurn) ev.flags |= 1 << i;
            if (ai_[i].zone < kHighwayTurn) ev.flags &= ~(1 << i);
        }
        // One player's time (+0xC, Hud_SetTime), while green and not paused.
        if (p == 0 && players_ == 1) {
            if (go_ && !paused_) ++ev.time;
            Event_SetTime(ev.time);
        }
        break;
    }
    case 2:
        // Race flag 8 on and 0x100 off: no pause from here; both cars braked.
        noPause_ = true;
        braked_[0] = braked_[1] = true;
        ev.state[p] = 3;
        break;
    case 3:
        if (t > 0x40) ev.fadeAll = t - 0x40;
        if (t < 0xC0) t += 2;
        else ev.done = true;
        break;
    default:
        break;
    }
}

// func_0025FF00: player 1's part, then player 2's. The end: the cars' sounds off, black.
bool Race::Race_HighwayTask() {
    Highway_PlayerTask(0);
    Highway_PlayerTask(1);
    return event_.done;
}

// The overlays the tasks asked for this frame: Fade_SetOverlay over everything with one player, func_0022EA78 and
// func_0022EAA0 over the top and bottom halves with two (alpha on the PS2's scale, 0x80 black).
void Race::Event_DrawFades(bool underCanvas) {
    auto fill = [&](float y, float h, int a) {
        if (a > 0) d_.Fill(0, y, gfx::Draw2D::kWidth, h, (uint32_t)(a < 0x80 ? a : 0x80) << 24);
    };
    const float half = gfx::Draw2D::kHeight / 2.0f;
    // The Sliding Door task draws the canvas before its Fade_SetOverlay, so the fade covers the banners; the Obstacle
    // Course and the Highway Race set their overlays first and draw the canvas after (PCSX2: WIN! and MISS COURSE!
    // stay lit over the black).
    if (underCanvas != (task_ != kTaskSlidingDoor)) return;
    if (players_ > 1)
        for (int p = 0; p < 2; ++p) fill(p * half, half, event_.fadeHalf[p]);
    // With one player the Obstacle Course's fades are Fade_SetOverlay's too.
    int a = event_.fadeAll;
    if (players_ == 1 && event_.fadeHalf[0] > a) a = event_.fadeHalf[0];
    fill(0, 2 * half, a);
}

void Race::Race_EventResult() {
    RaceResult& r = result_;
    r.course = id_;
    r.place = lap_[0].order;
    r.totalTime = r.bestLap = r.raceBestLap = kNoRecordTime;
    for (int& l : r.laps) l = kNoRecordTime;
    r.cars = carCount_;
    for (int i = 0; i < carCount_; ++i) {
        r.order[i] = lap_[i].order;
        r.total[i] = r.best[i] = kNoRecordTime;
        r.name[i] = cars_[i].record ? cars_[i].record->name : "";
    }
    for (auto& side : r.versusCar)
        for (int& c : side) c = -1;
    for (int i = 0; i < carCount_ && players_ > 1; ++i) r.versusCar[side_[i]][cars_[i].entry % 3] = i;
}

}  // namespace game
