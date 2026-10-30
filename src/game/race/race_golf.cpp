// Golf (course 0x17, ACTION/A05; the Adventure's race 43, Jumbo on Mushroom Road): nine holes where the car is the
// club. Driving into the ball hits it with the car's speed; the camera follows it until it stops, and the player
// drives on to it, until it stops in the cup. Each hole has a card first and a scorecard after; the round's score is
// the strokes against par. A high-level reimplementation of the event task func_00251B60, the ball (Golf_CheckHit
// func_00250FE0, Golf_CarTouchBall func_00251190, Ball_Step func_00251440), the cards (func_00252880, func_00252CB0)
// and the hole's panel and banners. See docs/minigames.md.
#include <math.h>
#include <stdio.h>
#include <string.h>

#include <algorithm>

#include "../../audio/sound.h"
#include "../hud/hud_games.h"
#include "../save/save_data.h"
#include "../text.h"
#include "race.h"
#include "../../game_text.inc"   // the game's text, GT_<address>_<length>, made from the ELF

namespace game {

namespace {

struct GolfHole {
    float car[3], tee[3], cup[3];   // D_002AC0B0 +0, +0x10, +0x20
    int yaw, par;                   // +0x30, +0x32
};
#include "golf_tables.inc"

const float kPi = 3.14159265f;
const int kHoleCount = 9;
const float kRadius = 0.45f;            // D_00332FB0, D_00332FE4: the ball's
const float kCupRadius = 8.5856f;       // D_0033302C...: within it (across the ground) is in the cup
const float kYard = 0.9144f;            // D_00333030
const float kHitReach = 2.0f, kTouchReach = 2.2f;   // D_00332FD0
const float kSoftLimit = 0.231481f;     // D_00332FB4: 50 km/h a frame
const float kSoftScale = 1.2f, kSoftAdd = 0.0925926f;   // D_00332FB8, D_00332FBC
const float kHardScale = 1.8f, kHardMax = 0.925926f, kLift = 0.00272222f;   // D_00332FC0-CC
const float kNudge = 0.0694444f, kHop = 0.108889f;   // D_00332FD4, D_00332FE0
const float kGravity = 0.00272222f, kWaterGravity = 0.00266805f, kWaterDrag = 0.99f;   // D_00332FE8-F4
const float kNearCup = 1.8f;            // D_00332FF8
const float kStopSpeed = 0.0185185f;    // D_00333028: 4 km/h
const int kStopWait = 0x78;
const float kShadowLift = 0.05f;        // D_00333044
const uint32_t kSeHard = 0x317, kSeSoft = 0x318, kSeHoleInOne = 0x319, kSeNearCup = 0x31A, kSeAtCup = 0x31B;
const uint32_t kSeWater = 0x31C, kSeOk = 0x1A;
// SCOREA05.GSL: the holes' maps 000-008 (192 x 320), the "Golf" bar 009 (256 x 40) and the tile 010 (64 x 64).
const char kFile[] = "SYS/SCOREA05.GSL";
// CUP IN (D_0029FB08: picture 1, palette 3) in the shared list's +0x82B4, at (320, 112).
const int kCupIn[6] = {1, 3, 0, 104, 180, 40};
// The result words (func_00233338) at (320, 224), by strokes - par: {picture, palette, u, v, w, h}.
const int kBurst[6] = {1, 2, 0, 180, 190, 75}, kHoleInOne[6] = {1, 0, 0, 0, 256, 62};
const int kWords[7][6] = {
    {1, 0, 0, 64, 256, 40},    // -3 ALBATROSS!
    {0, 0, 0, 0, 160, 42},     // -2 EAGLE
    {0, 0, 0, 43, 164, 42},    // -1 BIRDIE
    {0, 1, 0, 86, 104, 42},    // 0 PAR
    {0, 2, 0, 128, 176, 48},   // +1 BOGEY
    {0, 2, 0, 177, 256, 40},   // +2 DOUBLE BOGEY
    {0, 2, 0, 218, 256, 38},   // +3 TRIPLE BOGEY
};
const int kNone = 0x7F;

float Length(const float* v) { return sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]); }
float HDist(const float* a, const float* b) {
    float dx = a[0] - b[0], dz = a[2] - b[2];
    return sqrtf(dx * dx + dz * dz);
}
void Normalize(float* v) {
    float l = Length(v);
    if (l > 0) v[0] /= l, v[1] /= l, v[2] /= l;
}
float Wrap(float a) {
    while (a < -kPi) a += 2 * kPi;
    while (a > kPi) a -= 2 * kPi;
    return a;
}
// The yards to go: across the ground to the cup, less the cup's radius, in yards; 0 in the cup.
float YardsToGo(const float* ball, const float* cup) {
    float d = HDist(ball, cup) - kCupRadius;
    return d > 0 ? d / kYard : 0.0f;
}

void SetWord(HudElement& e, const int* w, int y) {
    e = HudElement();
    e.type = 3;
    e.x = 320;
    e.y = y;
    e.picture = w[0];
    e.palette = w[1];
    e.u = w[2];
    e.v = w[3];
    e.w = w[4];
    e.h = w[5];
    e.frames = 30;
    e.scale = 2.0f;
}

void DrawPicture(gfx::Draw2D& d, int n, int x, int y, int w, int h) {
    d.Sprite(d.Get(kFile, n, 0), (float)x, (float)y, (float)w, (float)h, 0, 0, (float)w, (float)h);
}

// The cards' background: the tile repeated over the screen (on to a wide screen's edges), and the "Golf" bar.
void DrawBackground(gfx::Draw2D& d) {
    const int x0 = (int)floorf(d.Left() / 64) * 64;
    for (int y = 0; y < gfx::Draw2D::kHeight; y += 64)
        for (int x = x0; x < d.Right(); x += 64) DrawPicture(d, 10, x, y, 64, 64);
}

// A score as the card prints it: none "---", 0 "0", else signed.
void ScoreText(char* out, size_t n, int v) {
    if (v == kNone) snprintf(out, n, "---");
    else if (v == 0) snprintf(out, n, "0");
    else snprintf(out, n, "%+d", v);
}

}  // namespace

// Golf_CheckHit (func_00250FE0): the car within 2 of the ball hits it with its own speed: under 50 km/h a soft shot
// (its direction, 1.2 times the speed plus 20 km/h), else a hard one (1.8 times, lifted by how far over 50 it was).
bool Race::Golf_CheckHit() {
    GolfState& g = golf_;
    const CarBody& me = bodies_[0];
    float d[3] = {g.ball[0] - me.pos[0], g.ball[1] - me.pos[1], g.ball[2] - me.pos[2]};
    if (!(Length(d) < kHitReach)) return false;
    g.armed = g.nudged = false;
    float v[3] = {me.vel[0] / 32768.0f, me.vel[1] / 32768.0f, me.vel[2] / 32768.0f};
    float s = Length(v);
    if (s < kSoftLimit) {
        Normalize(v);
        for (float& c : v) c *= s * kSoftScale + kSoftAdd;
        g.sePending = kSeSoft;
    } else {
        float over = s - kSoftLimit;
        for (float& c : v) c *= kHardScale;
        float a = std::min(over, kHardMax);
        v[1] = a * 180.0f / kHardMax * kLift;
        g.sePending = kSeHard;
    }
    memcpy(g.vel, v, sizeof g.vel);
    return true;
}

// Golf_CarTouchBall (func_00251190): after a shot, the car coming back within 2.2 of the falling ball nudges it: at
// least 15 km/h along its way (or away from the car), with a hop.
void Race::Golf_CarTouchBall() {
    GolfState& g = golf_;
    const CarBody& me = bodies_[0];
    float d[3] = {g.ball[0] - me.pos[0], g.ball[1] - me.pos[1], g.ball[2] - me.pos[2]};
    if (!(Length(d) < kTouchReach)) {
        g.armed = true;
        return;
    }
    if (!(g.vel[1] <= 0) || !g.armed) return;
    float v[3] = {g.vel[0], 0, g.vel[2]};
    float s = Length(v);
    if (s != 0) {
        if (s < kNudge) {
            Normalize(v);
            for (float& c : v) c *= kNudge;
        }
    } else {
        v[0] = d[0];
        v[2] = d[2];
        if (Length(v) == 0) {
            float a = (int16_t)(uint16_t)(me.yaw + 0x8000) * kPi / 32768.0f;
            v[0] = sinf(a);
            v[2] = cosf(a);
        }
        Normalize(v);
        for (float& c : v) c *= kNudge;
    }
    memcpy(g.vel, v, sizeof g.vel);
    g.nudged = true;
    g.vel[1] = kHop;
}

// Ball_Step (func_00251440): the ball moved; in the air drag and gravity (less in water); on the ground pushed out of
// it, bounced off the triangle's slope by its surface's pair; the water's splash going in and out; the landing's
// sound near the cup; its spin. Stopped (under 4 km/h for 121 frames): true.
bool Race::GolfBall_Step() {
    GolfState& g = golf_;
    float* P = g.ball;
    float* V = g.vel;
    for (int k = 0; k < 3; ++k) P[k] += V[k];
    float q[4] = {P[0], P[1], P[2], P[3]}, ceiling;
    const float* tri = nullptr;
    int32_t r = collision_.Collision_Ground(q, &ceiling, &tri);
    if (P[1] < q[3] && !g.underwater) {
        g.underwater = true;
        audio::Sound_PlaySe(kSeWater);
    } else if (P[1] >= q[3] && g.underwater) {
        g.underwater = false;
        audio::Sound_PlaySe(kSeWater);
    }
    if (r < 0) {
        V[0] = -V[0];
        V[2] = -V[2];
    } else if (q[1] + kRadius < P[1]) {
        ++g.airborne;
        float drag = g.underwater ? kAirDrag * kWaterDrag : kAirDrag;
        V[0] *= drag;
        V[2] *= drag;
        V[1] -= g.underwater ? kWaterGravity : kGravity;
    } else if (Length(V) != 0) {
        float s = Length(V);
        float n[3] = {0, 1, 0};
        if (tri) {
            memcpy(n, tri, sizeof n);
            Normalize(n);
        }
        float push = n[1] * (q[1] - P[1]) + kRadius;
        float dp[3] = {n[0] * push, n[1] * push, n[2] * push};
        if (Length(dp) > s)
            for (int k = 0; k < 3; ++k) dp[k] = n[k] * s;
        for (int k = 0; k < 3; ++k) P[k] += dp[k];
        float dot = n[0] * V[0] + n[1] * V[1] + n[2] * V[2];
        float R[3] = {V[0] - 2 * dot * n[0], V[1] - 2 * dot * n[1], V[2] - 2 * dot * n[2]};
        int k = r & 0xF;
        const float* b = kBounce[k < 7 ? k : 0];   // types 7 on would read past the table (A05 has none)
        V[0] = R[0] * b[0];
        V[2] = R[2] * b[0];
        V[1] = R[1] * b[1];
        if (g.underwater)
            for (int c = 0; c < 3; ++c) V[c] *= kWaterDrag;
        if (g.nudged && k != 5) V[0] = V[2] = 0;
        if (g.airborne >= 0x10) {
            float dc = HDist(P, kHoles[g.hole].cup);
            if (dc < kNearCup) audio::Sound_PlaySe(kSeAtCup);
            else if (dc < kCupRadius) audio::Sound_PlaySe(kSeNearCup);
        }
        g.airborne = 0;
    }
    // The spin: its two axes turned about z by -V.x / 0.45 and about x by V.z / 0.45 (rolling with its motion).
    float az = Wrap(-V[0] / kRadius), ax = Wrap(V[2] / kRadius);
    float cz = cosf(az), sz = sinf(az), cx = cosf(ax), sx = sinf(ax);
    for (float* a : {g.axisA, g.axisB}) {
        float x = cz * a[0] - sz * a[1], y = sz * a[0] + cz * a[1], z = a[2];
        a[0] = x;
        a[1] = cx * y - sx * z;
        a[2] = sx * y + cx * z;
        Normalize(a);
    }
    if (Length(V) >= kStopSpeed) {
        g.stopWait = kStopWait;
        return false;
    }
    if (g.stopWait) {
        --g.stopWait;
        return false;
    }
    V[0] = V[1] = V[2] = 0;
    return true;
}

// Golf_PlaceHole (func_00250F00): the car stopped at the hole's start (full jet fuel), the ball on the tee.
void Race::Golf_PlaceHole() {
    GolfState& g = golf_;
    const GolfHole& h = kHoles[g.hole];
    Car_Reset(bodies_[0], h.car, (uint16_t)h.yaw);
    Car_WorldPosition(bodies_[0], cars_[0].pos);
    bodies_[0].fuel = 0x40000;
    memset(g.axisA, 0, sizeof g.axisA);
    memset(g.axisB, 0, sizeof g.axisB);
    g.axisA[1] = g.axisB[2] = 1.0f;
    g.ball[0] = h.tee[0];
    g.ball[1] = h.tee[1] + kRadius;
    g.ball[2] = h.tee[2];
    g.ball[3] = 0;
    memset(g.vel, 0, sizeof g.vel);
    g.seCool = g.sePending = 0;
    g.armed = g.nudged = g.underwater = false;
    g.stopWait = g.airborne = 0;
}

// Golf_RecordHole (func_00252738): the hole's score (strokes with the holing one, against par), the round's total, a
// new best for the hole or the round into the save; after the ninth, Race_Outcome (1 over par, 2 at or under).
void Race::Golf_RecordHole(int strokes) {
    GolfState& g = golf_;
    SaveRecord& save = Save_Current();
    const int h = g.hole;
    int s = std::min(strokes + 1, 99);
    int rel = s - kHoles[h].par;
    g.score[h] = rel;
    g.total = std::min(g.total + rel, 99);
    if (rel < g.best[h]) {
        g.best[h] = rel;
        save.golfHole[h] = (int8_t)rel;
        g.newBest[h] = true;
    }
    if (h == kHoleCount - 1) {
        if (g.total < g.best[kHoleCount]) {
            g.best[kHoleCount] = g.total;
            save.golfTotal = (int8_t)g.total;
            g.newBest[kHoleCount] = true;
        }
        result_.outcome = g.total > 0 ? 1 : 2;
    }
}

// func_00233338: the hole's result word over the course, by its strokes against par.
void Race::Golf_SetResultWord() {
    GolfState& g = golf_;
    const int s = g.strokes[g.hole] + 1;
    const int rel = s - kHoles[g.hole].par;
    if (s == 1) {
        SetWord(elements_[kElemGoal], kBurst, 224);
        SetWord(elements_[kElemShared3], kHoleInOne, 224);
    } else if (rel >= -3 && rel <= 3) {
        SetWord(elements_[kElemGoal], kWords[rel + 3], 224);
    }
}

// func_00251B60's frame, after the cars. 0: the card's par and yards; 2: the hole's card; 10: the car and ball on
// the hole, the panel's values, ROOM_1, black for 8 frames (11), the fade in over 64 (30), then the HUD, green and the
// song; 31-33: 32 frames, then green and no brake; 32: the shot; 34: the ball's flight under the ball camera (32
// frames on), the car's nudges, and when it stops, the cup (CUP IN, the word: 36) or the next shot (35: 65 frames,
// the stroke counted, the chase camera); 36-37: 192 frames, the fade; 40: the room song, the record, the scorecard;
// 41: on to the next hole or the end.
bool Race::Race_GolfTask() {
    GolfState& g = golf_;
    bool draw = true;
    g.drawHud = true;
    switch (g.state) {
    case 0:
        for (int i = 0; i < kHoleCount; ++i)
            g.yards[i] = (int)(std::max(0.0f, HDist(kHoles[i].tee, kHoles[i].cup) - kCupRadius) / kYard);
        // Golf_InitCard (func_00252608): the round none yet, the bests from the save (0x7F none).
        for (int i = 0; i < kHoleCount; ++i) {
            g.score[i] = kNone;
            g.best[i] = Save_Current().golfHole[i];
        }
        g.best[kHoleCount] = Save_Current().golfTotal;
        g.total = 0;
        g.state = 2;
        g.count = 0;
        // fall through
    case 2:
        Roulette_HideView(true);
        g.child = kGolfIntro;
        g.childState = g.childCount = g.childFrame = 0;
        g.state = 3;
        g.count = 0;
        // fall through
    case 3:
        if (g.pulse) {
            g.pulse = false;
            event_.fadeAll = 0x80;
            g.state = 10;
            g.count = 0;
        }
        draw = false;
        break;
    case 10: {
        Roulette_HideView(false);
        Golf_PlaceHole();
        game_.hidden = true;
        go_ = false;
        noPause_ = true;
        braked_[0] = false;
        audio::Sound_LoadSong(0x0A0D0201);   // ROOM_1
        g.yardsToGo = YardsToGo(g.ball, kHoles[g.hole].cup);
        // HudCanvas_InitGolf (func_00234E98): the canvas cleared, the panel's values, the panel off.
        for (HudElement& e : elements_) e = HudElement();
        g.panel = false;
        g.panelYards = g.panelToGo = (int)g.yardsToGo;
        g.panelStrokes = 0;
        g.state = 11;
        g.count = 0;
    }
        // fall through
    case 11:
        event_.fadeAll = 0x80;
        if (g.count++ < 8) break;
        g.state = 30;
        g.count = 0;
        // fall through
    case 30:
        event_.fadeAll = (0x40 - g.count) * 2;
        if (g.count++ < 0x40) break;
        game_.hidden = false;
        go_ = true;
        noPause_ = false;
        g.panel = true;
        audio::Sound_StartSong();
        g.state = 31;
        g.count = 0;
        // fall through
    case 31:
        if (g.count++ < 0x20) break;
        g.state = 33;
        g.count = 0;
        // fall through
    case 33:
        go_ = true;
        braked_[0] = false;
        g.state = 32;
        g.count = 0;
        // fall through
    case 32:
        if (Golf_CheckHit()) {
            go_ = false;   // the car only coasts
            g.state = 34;
            g.count = 0;
        }
        break;
    case 34:
        if (g.count < 0x20 && ++g.count == 0x20) {
            overhead_ = true;   // race flag 0x80000000: the ball camera
            braked_[0] = true;
        }
        Golf_CarTouchBall();
        if (GolfBall_Step()) {
            if (g.yardsToGo == 0.0f) {
                noPause_ = true;
                overhead_ = false;
                if (g.strokes[g.hole] == 0) audio::Sound_PlaySe(kSeHoleInOne);
                g.panel = false;
                SetWord(elements_[kElemKickoff], kCupIn, 112);
                Golf_SetResultWord();
                g.state = 36;
            } else {
                g.state = 35;
            }
            g.count = 0;
        }
        g.yardsToGo = YardsToGo(g.ball, kHoles[g.hole].cup);
        g.panelToGo = (int)g.yardsToGo;
        break;
    case 35:
        if (g.count++ < 0x41) break;
        g.count = 0;
        if (g.strokes[g.hole] < 0x62) ++g.strokes[g.hole];
        g.panelStrokes = g.strokes[g.hole];
        g.state = 33;
        overhead_ = false;
        break;
    case 36:
        if (g.count++ < 0xC0) break;
        game_.hidden = true;
        audio::Sound_FadeOutSong();
        g.state = 37;
        g.count = 0;
        // fall through
    case 37:
        event_.fadeAll = g.count;
        if (g.count++ < 0x80) break;
        audio::Sound_StopSong();
        g.state = 40;
        g.count = 0;
        break;
    case 40:
        audio::Sound_LoadSong(0x0A0B0102);   // the rooms' song
        audio::Sound_StartSong();
        Roulette_HideView(true);
        Golf_RecordHole(g.strokes[g.hole]);
        g.child = kGolfCard;
        g.childState = g.childCount = g.childFrame = 0;
        g.state = 41;
        g.count = 0;
        // fall through
    case 41:
        if (g.pulse) {
            g.pulse = false;
            event_.fadeAll = 0x80;
            if (g.hole + 1 < kHoleCount) {
                ++g.hole;
                g.state = 2;
            } else {
                g.state = -1;
            }
            g.count = 0;
        }
        draw = false;
        break;
    default:
        return true;
    }
    if (!draw) {
        // States 3 and 41 return before the drawing: no ball, shadow, map or canvas.
        g.drawHud = false;
        objects_.HideObject(0);
        objects_.HideObject(1);
        return false;
    }
    // Object 0, the ball: Matrix_Billboard of its two axes (up and direction), at its place.
    float x[3] = {g.axisA[1] * g.axisB[2] - g.axisA[2] * g.axisB[1], g.axisA[2] * g.axisB[0] - g.axisA[0] * g.axisB[2],
                  g.axisA[0] * g.axisB[1] - g.axisA[1] * g.axisB[0]};
    Normalize(x);
    const float ball[12] = {x[0], g.axisA[0], g.axisB[0], g.ball[0], x[1], g.axisA[1], g.axisB[1], g.ball[1],
                            x[2], g.axisA[2], g.axisB[2], g.ball[2]};
    objects_.SetObject(0, ball);
    // Object 1, the shadow: on the ground under the ball, turned to its slope (Matrix_FromUp), 0.05 up.
    float q[4] = {g.ball[0], g.ball[1], g.ball[2], 1.0f}, ceiling;
    const float* tri = nullptr;
    if (collision_.Collision_Ground(q, &ceiling, &tri) >= 0) {
        float n[3] = {0, 1, 0};
        if (tri) {
            memcpy(n, tri, sizeof n);
            Normalize(n);
        }
        // Two axes square to the slope's normal: x across it, z along it.
        float zx[3] = {0, -n[2], n[1]};
        if (Length(zx) < 1e-6f) zx[0] = 1;
        Normalize(zx);
        float xx[3] = {n[1] * zx[2] - n[2] * zx[1], n[2] * zx[0] - n[0] * zx[2], n[0] * zx[1] - n[1] * zx[0]};
        const float shadow[12] = {xx[0], n[0], zx[0], q[0], xx[1], n[1], zx[1], q[1] + kShadowLift,
                                  xx[2], n[2], zx[2], q[2]};
        objects_.SetObject(1, shadow);
    } else {
        objects_.HideObject(1);
    }
    // The shots' sounds wait 10 frames apart.
    if (g.seCool) --g.seCool;
    if (g.sePending) {
        if (g.seCool == 0) {
            g.seCool = 10;
            audio::Sound_PlaySe(g.sePending);
        }
        g.sePending = 0;
    }
    return false;
}

int Race::Golf_Par(int hole) { return kHoles[hole].par; }

// RaceMap_DrawHole (func_00242A88): the race's map with course 0x17's view, clipped; the car and the ball.
void Race::Golf_DrawMap() {
    MapMarker markers[2] = {{MapMarker::kPlayer, cars_[0].pos[0], cars_[0].pos[2], cars_[0].yaw, 0},
                            {MapMarker::kBall, golf_.ball[0], golf_.ball[2], 0, 0}};
    hud_.DrawMap(d_, RaceMap_EditView(id_), markers, 2);
}

// The cards, drawn over the race's frame: the hole's (Golf_HoleIntroTask, func_00252880) and the scorecard
// (Golf_ScoreCardTask, func_00252CB0). Cross or Triangle, then 17 frames, and the answer.
void Race::Golf_DrawScreens(const platform::Input& in) {
    GolfState& g = golf_;
    if (g.child == kGolfNone) return;
    const int frame = g.childFrame++;
    char line[48];
    DrawBackground(d_);
    if (g.child == kGolfIntro) {
        const int h = g.hole;
        Window_DrawFrame(d_, 0x154, 0x2C, 0x104, 0x90, 0);
        snprintf(line, sizeof line, GT_003344F0_7, h + 1);
        Text_PrintAt(d_, 360, 100, line, 0);
        snprintf(line, sizeof line, "%3d Y", g.yards[h]);
        Text_PrintAt(d_, 360, 124, line, 0);
        snprintf(line, sizeof line, GT_00334500_6, kHoles[h].par);
        Text_PrintAt(d_, 360, 148, line, 0);
        for (int i = 0; i < 8 && kHoleText[h][i]; ++i) Text_PrintAt(d_, 360, 196 + 24 * i, kHoleText[h][i], 0);
        DrawPicture(d_, 9, 50, 34, 256, 40);
        DrawPicture(d_, h, 78, 80, 192, 320);
    } else {
        DrawPicture(d_, 9, 50, 34, 256, 40);
        Window_DrawFrame(d_, 100, 56, 440, 144, 0);
        Text_PrintAt(d_, 120, 112, GT_00328378_37, 0);   // D_00328378
        Text_PrintAt(d_, 120, 376, GT_00334528_7, 0);                                // D_00334528
        int parTotal = 0;
        for (int i = 0; i < kHoleCount; ++i) parTotal += kHoles[i].par;
        for (int i = 0; i <= kHoleCount; ++i) {
            const int y = 136 + 24 * (i + (i == kHoleCount));
            if (i < kHoleCount) {
                snprintf(line, sizeof line, "%d", i + 1);
                Text_PrintAt(d_, 180 - 16 * (int)strlen(line), y, line, 0);
            }
            snprintf(line, sizeof line, "%d", i < kHoleCount ? kHoles[i].par : parTotal);
            Text_PrintAt(d_, 270 - Text_Width(line), y, line, 0);
            const int c = g.newBest[i] && (frame & 0x20) ? 6 : 0;
            ScoreText(line, sizeof line, i < kHoleCount ? g.score[i] : g.total);
            Text_PrintAt(d_, 390 - Text_Width(line), y, line, c);
            ScoreText(line, sizeof line, g.best[i]);
            Text_PrintAt(d_, 500 - Text_Width(line), y, line, c);
        }
    }
    switch (g.childState) {
    case 0:
        g.childState = 1;
        // fall through
    case 1:
        if (in.Pressed(0) & (platform::kCross | platform::kTriangle)) {
            audio::Sound_PlaySe(kSeOk);
            g.childState = 2;
        }
        break;
    case 2:
        if (++g.childCount > 0x10) {
            g.pulse = true;
            g.child = kGolfNone;
        }
        break;
    default:
        break;
    }
}

}  // namespace game
