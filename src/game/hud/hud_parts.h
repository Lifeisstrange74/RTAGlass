// The pieces the game's HUD is built from, drawn from SYS/SCORE.GSL as its element drawers draw them (HudCanvas_Draw
// 0x00233860 and the element types 0x0022EE70-0x00231F28): headings, time rows with their lap badge, and the race
// position. A layout places them: game::RaceHud for races; the Adventure HUD will reuse them. Coordinates are the
// 640x448 frame's. See docs/hud.md.
#pragma once

#include "../../gfx/draw2d.h"

namespace game {

// The HUD's file for course `id`: SYS/SCORE.GSL, or on an event course (ACTION/Ann.BIN, ids 0x12 on) its own copy,
// SYS/SCOREAnn.GSL, the same pictures in other colours (Race_Setup's loader request, D_0029AF58 entries 0x07-0x29;
// A16, course 0x22, has none and keeps SCORE.GSL). The pieces below draw from the last one set.
void Hud_SetCourse(int id);
const char* Hud_ScoreFile();
// The file's number of the HUD's picture `picture` (0 the headings and digits, 1 the banners, 2 the lamps' glow).
int Hud_Picture(int picture);

// The headings, with a large first letter (sprite lists D_0029F628, D_0029F668, D_0029F6A8).
enum HudLabel { kLabelTotalTime, kLabelLapTime, kLabelTime };
void Hud_DrawLabel(gfx::Draw2D& d, HudLabel label, int x, int y);

// The digits' colour in a time row (D_0029FCA8 by style): the best lap, a running time, any other lap.
enum HudTimeStyle { kTimeBest, kTimeRunning, kTimeOther };
// A lap badge value: none, or the BEST plate instead of a number.
const int kNoLap = -1;
const int kLapBest = 0;

// Hud_DrawTimeRow (element type 0x0022F750): a time in frames as MM'SS"CC on a dark bar 132 wide with its top left at
// (x, y). 0 or 0x7FFFFFFF shows dashes, more than 99'59"99 shows that. `lap` adds a badge left of the bar.
void Hud_DrawTimeRow(gfx::Draw2D& d, int x, int y, int frames, HudTimeStyle style, int lap = kNoLap);

// Hud_DrawPosition (element type 0x0022FE50): the place, `order` counting from 0, in large digits with its suffix
// (st, nd, rd, th) and, with `cars` > 0, "/cars" below it. (x, y) is the top left of the tens digit's cell. With
// `cars` 0 it's the small form the teammates' places use: 16-pixel digits and the suffix, nothing for order -1.
void Hud_DrawPosition(gfx::Draw2D& d, int x, int y, int order, int cars);

// Picture 1's banners, as the element drawer (Hud_DrawElements 0x0022EE70) draws its sprite lists: FINISH!
// (D_0029F6E8), WRONG WAY (D_0029F768) and its U-turn arrow (D_0029F788), FINAL LAP (D_0029F708) and REPLAY
// (D_0029F748). Centred on (x, y), at `scale` (the zooming element type 3) in `palette`.
enum HudBanner { kBannerFinish, kBannerWrongWay, kBannerWrongWayArrow, kBannerFinalLap, kBannerReplay };
void Hud_DrawBanner(gfx::Draw2D& d, HudBanner banner, int palette, float x, float y, float scale = 1.0f);

// The start lights: a panel of four red lamps over four green ones (the canvas element at +0x800C). `step` is what
// Hud_SetStartLights last set; each lamp has a palette (lit or off) and a glow that fades in or out over it.
struct StartLights {
    int step;       // 0 hidden, 1 all off, 2 all red, 3-5 one more red out, 6 green
    int lamp[8];    // palette of each lamp, red row first: 6 red lit, 7 red off, 8 green lit, 9 green off
    int fade[8];    // its glow: 0 steady, 1 fading in, 2 fading out
    int glow[8];    // the glow's alpha, 0-0x80
};
// Hud_SetStartLights (0x002329A0): sets the lamps for `step` when it changes.
void Hud_SetStartLights(StartLights& l, int step);
// Hud_DrawStartLights (element type 0x0022F338): the 176x88 panel centred on (x, y). It also steps the glows' fades,
// so it runs once a frame, as the game's drawer does.
void Hud_DrawStartLights(gfx::Draw2D& d, StartLights& l, int x, int y);

// One of the HUD canvas's 0x2C-byte sprite elements, as Hud_DrawElements (0x0022EE70) draws it: a one-sprite list
// (u, v, w, h) of the HUD file's `picture` in `palette`, centred on (x, y). Type 1 stands still; type 2 rocks to and
// fro at `scale` (a 60-frame triangle of +-15 x 0.002 radians); type 3 zooms in from nothing to `scale` over
// `frames` frames.
// The events' banners and countdown are these (events.md).
struct HudElement {
    int type;            // +0x00: 0 none, 1 still, 2 rocking, 3 zooming
    int x, y;            // +0x04, +0x08
    int picture;         // +0x0C
    int palette;         // +0x10
    int u, v, w, h;      // the sprite list (+0x14): one sprite
    int t;               // +0x1C: the frame count
    int frames;          // +0x24
    float scale;         // +0x28
};
// Draws `e` and steps its count, once a frame.
void Hud_DrawElement(gfx::Draw2D& d, HudElement& e);
// The PC's: where a HUD piece at x goes on a wide screen (gfx::Draw2D's anchors): the left third's to the left edge,
// the right third's to the right, the middle's in the middle.
inline gfx::Anchor Hud_AnchorAt(float x) {
    return x < 640.0f / 3 ? gfx::Anchor::Left : x >= 1280.0f / 3 ? gfx::Anchor::Right : gfx::Anchor::Center;
}

// Soccer's score (Hud_DrawScore, the element type 0x00230C20, at canvas +0x8438): team A's goals in palette 1, a
// dash in palette 2 and team B's in palette 0, from picture 0's 40 x 48 digits (SCOREA08.GSL), centred on (x, y).
// Mode 1 is the small one, the digits 40 either side; mode 2 twice the size, 160 either side; 0 draws nothing.
void Hud_DrawScore(gfx::Draw2D& d, int mode, int x, int y, int s0, int s1);

}  // namespace game
