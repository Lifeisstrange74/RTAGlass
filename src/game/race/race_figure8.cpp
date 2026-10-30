// Figure 8 (course 0x19, ACTION/A07; the Adventure's race 45, Sandpolis 6): a time trial round traffic cones in
// five steps. A menu picks the step; the race's start lights; the step's gates in order (a wrong side is MISS
// COURSE!), FINISH! after the last; a cone touched costs 5 seconds and blinks PYLON TOUCH!; the step's results, then
// the menu again. A high-level reimplementation of the event task func_0025A120, the cones' tasks (func_00259A08),
// the step menu (func_0025AB28) and the results (func_0025B0C0). See docs/minigames.md.
#include <math.h>
#include <stdio.h>
#include <string.h>

#include <algorithm>

#include "../../audio/sound.h"
#include "../hud/minimap.h"
#include "../save/save_data.h"
#include "../text.h"
#include "race.h"
#include "../../game_text.inc"   // the game's text, GT_<address>_<length>, made from the ELF

namespace game {

namespace {

struct Figure8Step {
    int x, z;            // D_002C3A18: the start, facing +z at height 1
    int cones, gates;    // D_002C3A30, D_002C3A38
    int stamp;           // D_002C3A40: a clean finish's stamp
    int goal;            // D_002C3AF8: the goal time (only shown)
    MapView view;        // D_002A42E0: the map's view for the step
};
#include "figure8_tables.inc"

const float kPi = 3.14159265f;
const float kConeY = 1.5f, kTouch = 1.5f, kSideGap = 0.6f;   // D_003330B8
const float kHopUp = 0.003f, kSlideScale = 0.01f, kGravity = 0.008f, kSlideDrag = -0.008f;   // D_003330BC-C8
const int kTimeMax = 0x57E40, kTouchesMax = 99, kPenalty = 300;   // 5 seconds a cone
const uint32_t kSeCone = 0x314, kSeOk = 0x1A, kSeCancel = 0x1B;
// PYLON TOUCH! (D_0029FA68), picture 1, palette 2, standing at (320, 412) in its own element (+0x8774).
const int kPylonTouch[4] = {0, 96, 160, 32};
// SCOREA07.GSL: the steps' maps 000-004, the title 005 ("Figure 8", 256 x 40) and the cone tile 006 (64 x 64).
const char kFile[] = "SYS/SCOREA07.GSL";

float Side(int line, int sign, float x, float z) {
    const float* l = kLines[line];
    return ((l[3] - l[1]) * (x - l[2]) - (l[2] - l[0]) * (z - l[3])) * sign;
}

void DrawPicture(gfx::Draw2D& d, int n, int x, int y, int w, int h) {
    d.Sprite(d.Get(kFile, n, 0), (float)x, (float)y, (float)w, (float)h, 0, 0, (float)w, (float)h);
}

// The menus' background: the cone tile repeated over the screen (Sprite_DrawRepeat; on to a wide screen's edges),
// and the title.
void DrawBackground(gfx::Draw2D& d) {
    const int x0 = (int)floorf(d.Left() / 64) * 64;
    for (int y = 0; y < gfx::Draw2D::kHeight; y += 64)
        for (int x = x0; x < d.Right(); x += 64) DrawPicture(d, 6, x, y, 64, 64);
    DrawPicture(d, 5, 50, 34, 256, 40);
}

}  // namespace

// Figure8_ConeTask (func_00259A08): a cone. The car within 1.5 of it tells the task (a cone at rest is a touch: 1;
// else only the sound: 2) and, by the car's speed, sets it wobbling and sliding (1-14 km/h) or tipping, hopping and
// sliding (15 on). Settled and the car away, it's back at its home, but still turned as it lay.
void Race::Figure8_ConeTask(int j) {
    Figure8State& f = figure8_;
    Figure8Cone& c = f.cones[j];
    const CarBody& me = bodies_[0];
    const float* home = kConeHomes[f.step][j];
    float dx = c.pos[0] - me.pos[0], dy = c.pos[1] - me.pos[1], dz = c.pos[2] - me.pos[2];
    if (sqrtf(dx * dx + dy * dy + dz * dz) < kTouch) {
        if (!(c.flags & 1)) c.count = 0;
        if (me.pos[0] < c.pos[0] - kSideGap) c.side = 0;
        else if (me.pos[0] > c.pos[0] + kSideGap) c.side = 1;
        else if (me.pos[2] < c.pos[2] - kSideGap) c.side = 2;
        else if (me.pos[2] > c.pos[2] + kSideGap) c.side = 3;
        else c.side = 0;
        float vx = me.vel[0] / 32768.0f, vz = me.vel[2] / 32768.0f;
        int k = (int)(sqrtf(vx * vx + vz * vz) * 216.0f);   // Vec_SpeedKmh
        c.vel[1] = k * kHopUp;
        c.vel[0] = (me.vel[0] / 152) * kSlideScale;
        c.vel[2] = (me.vel[2] / 152) * kSlideScale;
        memcpy(c.vel0, c.vel, sizeof c.vel0);
        f.coneSignal = c.flags == 0 ? 1 : 2;
        if (k >= 1 && k <= 14) c.flags |= 9;
        else if (k >= 15) c.flags |= 0xE;
        if (c.down) c.flags &= ~2;
    }
    if (c.flags & 1) {
        int dir = (c.count & 7) < 4 ? 1 : -1;
        switch (c.side) {
        case 0: c.rz = (int16_t)(c.rz - 0x80 * dir); break;
        case 1: c.rz = (int16_t)(c.rz + 0x80 * dir); break;
        case 2: c.rx = (int16_t)(c.rx + 0x80 * dir); break;
        default: c.rx = (int16_t)(c.rx - 0x80 * dir); break;
        }
        if (++c.count == 16) c.flags &= ~1;
    }
    if (c.flags & 2) {
        switch (c.side) {
        case 0: c.rz = (int16_t)(c.rz - 0x400); break;
        case 1: c.rz = (int16_t)(c.rz + 0x400); break;
        case 2: c.rx = (int16_t)(c.rx + 0x400); break;
        default: c.rx = (int16_t)(c.rx - 0x400); break;
        }
        if (c.rz <= -0x4800 || c.rz >= 0x4800 || c.rx >= 0x4800 || c.rx <= -0x4800) {
            c.down = true;
            c.flags = (uint8_t)((c.flags & ~2) | 0x10);
        }
    }
    if (c.flags & 4) {
        c.pos[1] += c.vel[1];
        c.vel[1] -= kGravity;
        if (c.pos[1] <= kConeY) {
            c.pos[1] = kConeY;
            if (++c.count < 8) c.vel[1] *= -0.5f;
            else c.flags = (uint8_t)((c.flags & ~4) | 0x10);
        }
    }
    if (c.flags & 8) {
        c.pos[0] += c.vel[0];
        c.pos[2] += c.vel[2];
        for (int k = 0; k < 3; k += 2) {
            c.vel[k] += c.vel0[k] * kSlideDrag;
            if ((c.vel0[k] > 0 && c.vel[k] < 0) || (c.vel0[k] < 0 && c.vel[k] > 0)) c.vel[k] = 0;
        }
        if (c.vel[0] == 0 && c.vel[2] == 0) c.flags = (uint8_t)((c.flags & ~8) | 0x10);
    }
    float hx = home[0] - me.pos[0], hy = kConeY - me.pos[1], hz = home[1] - me.pos[2];
    if (c.flags == 0x10 && sqrtf(hx * hx + hy * hy + hz * hz) >= kTouch) {
        c.pos[0] = home[0];
        c.pos[1] = kConeY;
        c.pos[2] = home[1];
        c.flags = 0;
    }
    // Unit, then RotMatrixX, (Y: 0), RotMatrixZ: Rz Rx, at its place.
    float ax = c.rx * kPi / 32768.0f, az = c.rz * kPi / 32768.0f;
    float cx = cosf(ax), sx = sinf(ax), cz = cosf(az), sz = sinf(az);
    const float m[12] = {cz, -sz * cx, sz * sx, c.pos[0], sz, cz * cx, -cz * sx, c.pos[1], 0, sx, cx, c.pos[2]};
    objects_.SetObject(j, m);
}

// Figure8_RecordTime (func_0025AA58): the step's time (0 a miss), touches and total; a total under the best is the
// save's new best.
void Race::Figure8_RecordTime(int time, int touches) {
    Figure8State& f = figure8_;
    const int s = f.cursor;
    time = std::min(time, kTimeMax);
    f.stepTime[s] = time;
    if (time == 0) return;
    touches = std::min(touches, kTouchesMax);
    f.stepHits[s] = touches;
    int total = std::min(time + touches * kPenalty, kTimeMax);
    f.stepTotal[s] = total;
    if (total < f.stepBest[s]) {
        f.stepBest[s] = total;
        Save_Current().figure8Best[s] = total;
        f.stepNew[s] = true;
    }
}

// func_0025A120's frame, after the cars. 0: the step menu (the view off) and the car stopped; 5: on its answer, quit,
// or the step: the car at its start, its cones, the canvas and song, the start lights, the view; 1: the gates, the
// time, the cones' signals; 2: race over, the brake, the song fading; 3: the fade over 128 frames, then the results;
// 4: on their answer, the menu again.
bool Race::Race_Figure8Task() {
    Figure8State& f = figure8_;
    CarBody& me = bodies_[0];
    switch (f.state) {
    case 0: {
        const int last = f.step;
        Figure8State fresh;
        fresh.step = last;
        f = fresh;
        f.cursor = last;   // Figure8_InitMenu (func_0025B500)
        for (int i = 0; i < 5; ++i) {
            f.stepTime[i] = f.stepTotal[i] = f.stepHits[i] = 0x7FFFFFFF;
            f.stepBest[i] = Save_Current().figure8Best[i];
        }
        for (HudElement& e : elements_) e = HudElement();
        eventTime_ = -1;
        f.child = kFigure8Select;
        f.childState = f.childCount = f.childFrame = 0;
        f.window = Window();
        for (int i = 0; i < 5; ++i) {
            snprintf(f.lines[i], sizeof f.lines[i], GT_00328FD0_10, i + 1);
            f.window.AddLine(f.lines[i], kLineSelectable);
        }
        f.window.style = 2;
        f.window.x = 0x154;
        f.window.y = 0x2C;
        f.window.w = 0x104;
        f.window.sel = (signed char)f.cursor;
        Roulette_HideView(true);
        // The map and the canvas are off over the menu, the first one too (race flag 0x10000: by PCSX2, though
        // nothing in the task sets it before the first step's results).
        game_.hidden = true;
        noPause_ = true;
        f.state = 5;
        Car_Reset(me, me.pos, me.yaw);   // Car_ResetForRespawn
        Car_WorldPosition(me, cars_[0].pos);
        for (int i = 0; i < 17; ++i) objects_.HideObject(i);
        break;
    }
    case 5: {
        if (!f.pulse) break;
        f.pulse = false;
        me.fuel = 0x40000;
        if (f.pulseValue == -1) {
            f.end = true;
            break;
        }
        f.step = f.pulseValue;
        const Figure8Step& s = kSteps[f.step];
        const float start[3] = {(float)s.x, 1.0f, (float)s.z};
        Car_SetPlace(me, start, 0);
        Car_WorldPosition(me, cars_[0].pos);
        for (int i = 0; i < 17; ++i) objects_.HideObject(i);
        for (int j = 0; j < s.cones; ++j) {
            Figure8Cone& c = f.cones[j];
            c = Figure8Cone();
            c.pos[0] = kConeHomes[f.step][j][0];
            c.pos[1] = kConeY;
            c.pos[2] = kConeHomes[f.step][j][1];
        }
        f.coneCount = s.cones;
        for (HudElement& e : elements_) e = HudElement();   // HudCanvas_InitFigure8 (func_00235050)
        eventTime_ = -1;
        audio::Sound_LoadSong(0x0A100201);   // BGM_05
        countdownStart_ = frame_ + 1;        // the countdown's task runs from the next frame
        lights_ = StartLights();
        f.coneSignal = 0;
        Roulette_HideView(false);
        game_.hidden = false;
        noPause_ = false;   // the countdown's race flag 0x100 at its frame 300
        f.state = 1;
        break;
    }
    case 1: {
        const Figure8Step& s = kSteps[f.step];
        const int n = f.gate;
        if (n > 0 && !paused_) ++f.time;
        bool missed = false;
        if (n < s.gates) {
            const int8_t* g = kGates[f.step][n];
            if (Side(g[0], g[1], me.pos[0], me.pos[2]) >= 0) ++f.gate;
            if (Side(g[2], g[3], me.pos[0], me.pos[2]) >= 0 || Side(g[4], g[5], me.pos[0], me.pos[2]) >= 0) {
                Event_SetMissCourse(0);
                f.state = 2;
                result_.outcome = 0;
                f.time = 0;
                missed = true;
            }
        }
        if (!missed) {
            if (n > 0) {
                const int8_t* g = kGates[f.step][n - 1];
                if (Side(g[0], g[1], me.pos[0], me.pos[2]) < 0) --f.gate;
            }
            if (s.gates == n) {
                result_.outcome = 1;
                if (f.hits == 0) Stamp_Queue(s.stamp);
                Event_SetFinish(0);
                f.state = 2;
            }
            if (f.coneSignal == 1) {
                audio::Sound_PlaySe(kSeCone);
                ++f.hits;
                f.coneSignal = 0;
                f.pylonBlink = 0x80;
            }
            if (f.coneSignal == 2) {
                audio::Sound_PlaySe(kSeCone);
                f.coneSignal = 0;
            }
            Event_SetTime(f.time);
        }
        break;
    }
    case 2:
        noPause_ = true;
        braked_[0] = true;
        audio::Sound_FadeOutSong();
        f.state = 3;
        break;
    case 3:
        event_.fadeAll = f.fade;
        if (f.fade < 0x80) {
            ++f.fade;
        } else {
            Figure8_RecordTime(f.time, f.hits);
            f.child = kFigure8Result;
            f.childState = f.childCount = f.childFrame = 0;
            audio::Sound_LoadSong(0x0A100201);
            elements_[kElemMissCourse].type = 0;
            elements_[kElemFinish].type = 0;
            Roulette_HideView(true);
            game_.hidden = true;
            f.state = 4;
        }
        break;
    case 4:
        if (!f.pulse) break;
        f.pulse = false;
        go_ = false;
        braked_[0] = false;
        countdownStart_ = -1;
        f.state = 0;
        break;
    default:
        break;
    }
    // The cones' tasks run after their parent's; they stay till the next step starts.
    if (f.state != 0 && f.state != 5)
        for (int j = 0; j < f.coneCount; ++j) Figure8_ConeTask(j);
    // The common tail: PYLON TOUCH! blinks 16 frames in 32 while its count runs down.
    if (f.pylonBlink > 0 && !paused_) --f.pylonBlink;
    HudElement& e = elements_[kElemPylon];
    int on = (f.pylonBlink & 0x10) ? 1 : 0;
    if (e.type != on) {
        e = HudElement();
        if (on) {
            e.type = 1;
            e.x = 320;
            e.y = 0x19C;
            e.picture = 1;
            e.palette = 2;
            e.u = kPylonTouch[0];
            e.v = kPylonTouch[1];
            e.w = kPylonTouch[2];
            e.h = kPylonTouch[3];
        }
    }
    return f.end;
}

// RaceMap_DrawStep (func_00242A88) with the step's view, RaceMap_DrawCars and RaceMap_DrawObjects (the cones as
// yellow markers).
void Race::Figure8_DrawMap() {
    const Figure8State& f = figure8_;
    MapMarker markers[1 + 17];
    int n = 0;
    markers[n++] = {MapMarker::kPlayer, cars_[0].pos[0], cars_[0].pos[2], cars_[0].yaw, 0};
    for (int j = 0; j < f.coneCount && f.state != 0 && f.state != 5; ++j)
        markers[n++] = {MapMarker::kBall, f.cones[j].pos[0], f.cones[j].pos[2], 0, 0};
    hud_.DrawMap(d_, kSteps[f.step].view, markers, n);
}

// The menus, drawn over the race's frame: the step menu (Figure8_SelectTask, func_0025AB28) and the results
// (Figure8_ResultTask, func_0025B0C0). Each tells its parent through `pulse`.
void Race::Figure8_DrawScreens(const platform::Input& in) {
    Figure8State& f = figure8_;
    if (f.child == kFigure8None) return;
    const int frame = f.childFrame++;
    DrawBackground(d_);
    if (f.child == kFigure8Select) {
        // The window's own cursor (its answer isn't used; it closes as the task counts out).
        f.window.Update(d_, in);
        for (int i = 0; i < 5; ++i) Time_Print(d_, 0x1F4, (0x2C + 0xC * i) * 2, f.stepBest[i], 0);
        Window_DrawFrame(d_, 0x154, 0x74, 0x104, 0x60, 0);
        Text_PrintAt(d_, 0x168, 0xF4, GT_00334B88_4, 0);   // D_00334B88
        Time_Print(d_, 0x1F4, 0x7A * 2, kSteps[f.cursor].goal, 0);
        for (int k = 0; k < 5 && kStepText[f.window.sel][k]; ++k)
            Text_PrintAt(d_, 0x168, 0x10C + 0x18 * k, kStepText[f.window.sel][k], 0);
        Text_PrintAt(d_, 0x168, 0x16C, GT_00329008_13, 6);   // D_00329008
        Text_PrintAt(d_, 0x168, 0x184, GT_00329018_13, 6);   // D_00329018
        DrawPicture(d_, f.cursor, 50, 100, 256, 256);
        uint32_t pressed = in.Pressed(0);
        switch (f.childState) {
        case 0:
            f.childState = 1;
            // fall through
        case 1:
            if (f.window.sel != 5) {
                f.cursor = f.window.sel;
                f.childResult = f.window.sel;
            } else {
                f.childResult = -1;
            }
            if (pressed & platform::kCross) {
                audio::Sound_PlaySe(kSeOk);
                f.childState = 2;
            } else if (pressed & platform::kTriangle) {
                audio::Sound_PlaySe(kSeCancel);
                f.childResult = -1;
                f.childState = 2;
            }
            break;
        case 2:
            if (++f.childCount > 0x10) {
                f.pulse = true;
                f.pulseValue = f.childResult;
                f.child = kFigure8None;
            }
            break;
        default:
            break;
        }
        return;
    }
    // The results: the step, its time, the cones hit, the total, the best (blinking when new) and the goal.
    const int s = f.cursor;
    char line[32];
    Window_DrawFrame(d_, 0x64, 0x38, 0x1B8, 0x54, 0);
    snprintf(line, sizeof line, GT_00334B90_6, s + 1);
    Text_PrintAt(d_, 0x78, 0x70, line, 0);
    if (f.stepTime[s] == 0) {
        Text_PrintAt(d_, 0x78, 0xA0, GT_00329068_14, 6);   // D_00329068
    } else {
        int c = f.stepNew[s] && (frame & 0x20) ? 6 : 0;
        Text_PrintAt(d_, 0x78, 0xA0, GT_00334B98_4, 0);
        Time_Print(d_, 0x12C, 0x50 * 2, f.stepTime[s], 0);
        Text_PrintAt(d_, 0x78, 0xB8, GT_00329078_9, 0);
        snprintf(line, sizeof line, GT_00329088_16, f.stepHits[s], f.stepHits[s] * 5);
        Text_PrintAt(d_, 0x12C, 0xB8, line, 0);
        Text_PrintAt(d_, 0x78, 0xD0, GT_003290A0_10, 0);
        Time_Print(d_, 0x12C, 0x68 * 2, f.stepTotal[s], c);
        Text_PrintAt(d_, 0x78, 0xE8, GT_003290B0_9, 0);
        Time_Print(d_, 0x12C, 0x74 * 2, f.stepBest[s], c);
        Text_PrintAt(d_, 0x78, 0x100, GT_003290C0_9, 0);
        Time_Print(d_, 0x12C, 0x80 * 2, kSteps[s].goal, 0);
    }
    switch (f.childState) {
    case 0:
        f.childState = 1;
        // fall through
    case 1:
        if (in.Pressed(0) & (platform::kCross | platform::kTriangle)) {
            audio::Sound_PlaySe(kSeOk);
            f.childState = 2;
        }
        break;
    case 2:
        if (++f.childCount > 0x10) {
            f.pulse = true;
            f.pulseValue = -1;
            f.child = kFigure8None;
        }
        break;
    default:
        break;
    }
}

}  // namespace game
