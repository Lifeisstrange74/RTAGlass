#include "hud_parts.h"

#include <math.h>
#include <stdio.h>

namespace game {

namespace {

// SYS/SCORE.GSL: picture 0 holds the headings, the digits, the bars and the start lights (in 16 palettes), picture 1
// FINISH, WRONG WAY and the lap badge, picture 2 the lamps' glow (D_0029F5F8, the HUD's TEX0s). The game's sprites
// address one texel more than they draw; the rectangles here are the drawn size. Hud_SetCourse picks the file.
const char kScoreFile[] = "SYS/SCORE.GSL";
char g_score[24] = "SYS/SCORE.GSL";
const char* const kScore = g_score;
int g_pictureBase = 0;   // the file's number of the HUD's picture 0

struct HudSprite {
    int x, y, u, v, w, h;
};

// A heading's big letter and the rest of its word, palette 1.
const HudSprite kLabel[3][2] = {
    {{0, 0, 0, 112, 32, 30}, {32, 12, 32, 81, 130, 18}},    // D_0029F628: T + OTALTIME
    {{0, 0, 0, 80, 32, 32}, {32, 12, 32, 159, 98, 17}},     // D_0029F668: L + APTIME
    {{0, 0, 0, 112, 32, 30}, {32, 12, 119, 81, 43, 18}},    // D_0029F6A8: T + IME
};

const int kStylePalette[3] = {0, 1, 12};   // D_0029FCA8

void Draw(gfx::Draw2D& d, const gfx::Texture& t, int x, int y, const HudSprite& s) {
    d.Sprite(t, (float)(x + s.x), (float)(y + s.y), (float)s.w, (float)s.h, (float)s.u, (float)s.v, (float)s.w,
             (float)s.h);
}

}  // namespace

void Hud_SetCourse(int id) {
    if (id >= 0x12 && id < 0x27 && id != 0x22) snprintf(g_score, sizeof g_score, "SYS/SCOREA%02d.GSL", id - 0x12);
    else snprintf(g_score, sizeof g_score, "%s", kScoreFile);
    // The HUD's pictures are the file's uploads to TBP 0x2C36, 0x2CB8 and 0x2D3A, which come after the games' own
    // pictures in Golf's, Roulette's and Figure 8's files.
    g_pictureBase = id == 0x17 ? 11 : id == 0x18 ? 2 : id == 0x19 ? 7 : 0;
}

int Hud_Picture(int picture) { return g_pictureBase + picture; }

const char* Hud_ScoreFile() { return g_score; }

void Hud_DrawLabel(gfx::Draw2D& d, HudLabel label, int x, int y) {
    const gfx::Texture& t = d.Get(kScore, Hud_Picture(0), 1);
    for (const HudSprite& s : kLabel[label]) Draw(d, t, x, y, s);
}

void Hud_DrawTimeRow(gfx::Draw2D& d, int x, int y, int frames, HudTimeStyle style, int lap) {
    // The bar: its left end, 12 middle pieces and its right end; then the ' and " marks, all palette 1.
    const gfx::Texture& bar = d.Get(kScore, Hud_Picture(0), 1);
    Draw(d, bar, x, y, {0, 0, 0, 176, 18, 24});
    for (int i = 0; i < 12; ++i) Draw(d, bar, x, y, {18 + 8 * i, 0, 18, 176, 8, 24});
    Draw(d, bar, x, y, {114, 0, 26, 176, 18, 24});
    Draw(d, bar, x, y, {32, 4, 160, 48, 16, 16});
    Draw(d, bar, x, y, {74, 4, 176, 48, 16, 16});

    // Six digits as BCD, 10 being the dash.
    int digits[6];
    if (frames == 0 || frames == 0x7FFFFFFF) {
        for (int& n : digits) n = 10;
    } else if (frames > 0x57E3F) {
        const int kMax[6] = {9, 9, 5, 9, 9, 9};
        for (int i = 0; i < 6; ++i) digits[i] = kMax[i];
    } else {
        int sec = frames / 60 % 60, sub = frames % 60;
        const int v[6] = {frames / 36000, frames / 3600 % 10, sec / 10, sec % 10, sub / 6, sub * 100 / 60 % 10};
        for (int i = 0; i < 6; ++i) digits[i] = v[i];
    }
    const gfx::Texture& t = d.Get(kScore, Hud_Picture(0), kStylePalette[style]);
    const int kX[6] = {4, 18, 46, 60, 88, 102};
    for (int i = 5; i >= 0; --i) Draw(d, t, x, y, {kX[i], 4, 16 * digits[i], 64, 16, 16});

    if (lap == kNoLap) return;
    Draw(d, d.Get(kScore, Hud_Picture(1), 8), x, y, {-26, 0, 232, 168, 24, 24});
    const gfx::Texture& n = d.Get(kScore, Hud_Picture(1), 7);
    if (lap == kLapBest) {
        Draw(d, n, x, y, {-30, 5, 80, 200, 32, 16});
    } else if (lap < 10) {
        Draw(d, n, x, y, {-18, 5, lap * 8, 200, 8, 16});
    } else {
        Draw(d, n, x, y, {-22, 5, lap / 10 % 10 * 8, 200, 8, 16});
        Draw(d, n, x, y, {-14, 5, lap % 10 * 8, 200, 8, 16});
    }
}

void Hud_DrawPosition(gfx::Draw2D& d, int x, int y, int order, int cars) {
    const gfx::Texture& t = d.Get(kScore, Hud_Picture(0), 0);
    if (cars == 0) {
        // The small form (a teammate's place): the suffix at +30, the place in the 16 x 16 digits (the time rows'
        // bottom row, here pink) with its units at +14 and tens at +0. Order -1 draws nothing.
        if (order < 0) return;
        int suffix = order % 10 < 3 && order / 10 != 1 ? order % 10 : 3;
        Draw(d, t, x, y, {30, 0, 32 + 22 * suffix, 31, 22, 16});
        int place = order + 1;
        Draw(d, t, x, y, {14, 0, 16 * (place % 10), 64, 16, 16});
        if (place >= 10) Draw(d, t, x, y, {0, 0, 16 * (place / 10 % 10), 64, 16, 16});
        return;
    }
    if (cars > 0) Draw(d, t, x, y, {51, 0, 160, 105, 32, 32});
    // st, nd, rd for 1, 2, 3 (not 11-13), else th.
    int suffix = order % 10 < 3 && order / 10 != 1 ? order % 10 : 3;
    Draw(d, t, x, y, {51, 0, 32 + 22 * suffix, 31, 22, 16});
    // The large digits run 0-8 along the top row and 9 below.
    int place = order + 1;
    Draw(d, t, x, y, {23, 0, place % 10 % 9 * 28, place % 10 / 9 * 30, 28, 30});
    if (place >= 10) Draw(d, t, x, y, {0, 0, place / 10 % 9 * 28, place / 10 / 9 * 30, 28, 30});
    if (cars <= 0) return;
    const gfx::Texture& small = d.Get(kScore, Hud_Picture(0), 1);
    Draw(d, small, x, y, {71, 16, 32 + cars % 10 * 12, 48, 12, 16});
    if (cars >= 10) Draw(d, small, x, y, {61, 16, 32 + cars / 10 % 10 * 12, 48, 12, 16});
}

void Hud_DrawBanner(gfx::Draw2D& d, HudBanner banner, int palette, float x, float y, float scale) {
    // {u, v, w, h} in picture 1.
    const int kBanner[5][4] = {{0, 0, 210, 56}, {0, 56, 244, 40}, {163, 96, 86, 46}, {0, 96, 160, 32}, {0, 152, 136, 48}};
    const int* b = kBanner[banner];
    float w = b[2] / 2 * scale, h = b[3] / 2 * scale;
    if (w <= 0 || h <= 0) return;
    d.Sprite(d.Get(kScore, Hud_Picture(1), palette), x - w, y - h, 2 * w, 2 * h, (float)b[0], (float)b[1], (float)b[2], (float)b[3]);
}

void Hud_SetStartLights(StartLights& l, int step) {
    if (l.step == step) return;
    l = StartLights();
    l.step = step;
    // Red row: lit from step 2, going out left to right one a step from 3, the lamp going out fading its glow; at 6
    // the green row lights.
    for (int i = 0; i < 4; ++i) {
        bool red = step >= 2 && step <= 5 && i >= step - 2;
        l.lamp[i] = red ? 6 : 7;
        l.lamp[4 + i] = step == 6 ? 8 : 9;
    }
    if (step == 2) {
        for (int i = 0; i < 4; ++i) l.fade[i] = 1;
    } else if (step >= 3) {
        int out = step - 3;   // the lamp going out this step
        l.fade[out] = 2;
        for (int i = out; i < 4; ++i) l.glow[i] = 0x80;
        if (step == 6)
            for (int i = 4; i < 8; ++i) l.fade[i] = 1;
    }
}

void Hud_DrawStartLights(gfx::Draw2D& d, StartLights& l, int x, int y) {
    if (!l.step) return;
    x -= 88;
    y -= 44;
    // The panel, palette 6: its ends (D_002EBCE0, D_002EBD00) and 18 middle pieces of 8 (D_002EBD20).
    const gfx::Texture& panel = d.Get(kScore, Hud_Picture(0), 6);
    Draw(d, panel, x, y, {0, 0, 152, 168, 16, 88});
    Draw(d, panel, x, y, {160, 0, 176, 168, 16, 88});
    for (int i = 0; i < 18; ++i) Draw(d, panel, x, y, {16 + 8 * i, 0, 168, 168, 8, 88});

    // The lamps and glows are sampled half a texel inside their rectangles: filtered, the lamp's edge would pick up
    // the panel next to it in the atlas and the glow's the row below it.
    const gfx::Texture& glow = d.Get(kScore, Hud_Picture(2));
    for (int i = 0; i < 8; ++i) {
        int col = i % 4, row = i / 4;
        d.Sprite(d.Get(kScore, Hud_Picture(0), l.lamp[i]), (float)(x + 8 + 40 * col), (float)(y + 8 + 36 * row), 40, 36, 112.5f,
                 184.5f, 39, 35);
        if (l.fade[i] == 1) l.glow[i] = l.glow[i] < 0x80 ? l.glow[i] + 0x40 : 0x80;
        else if (l.fade[i] == 2) l.glow[i] = l.glow[i] > 0 ? l.glow[i] - 0x20 : 0;
        // Red 0x4334EC, green 0x50E800, at the glow's alpha.
        uint32_t rgb = row == 0 ? 0x4334EC : 0x50E800;
        d.Sprite(glow, (float)(x + 2 + 40 * col), (float)(y + 1 + 36 * row), 50, 50, 0.5f, 0.5f, 49, 49,
                 (uint32_t)l.glow[i] << 24 | rgb);
    }
}

void Hud_DrawScore(gfx::Draw2D& d, int mode, int x, int y, int s0, int s1) {
    if (mode != 1 && mode != 2) return;
    float k = mode == 2 ? 2.0f : 1.0f, apart = mode == 2 ? 160.0f : 40.0f;
    // Digit n is at ((n % 6) x 40, (n / 6) x 48); 10 is the dash.
    auto digit = [&](int n, int palette, float cx) {
        const gfx::Texture& t = d.Get(kScore, Hud_Picture(0), palette);
        d.Sprite(t, cx - 20 * k, y - 24 * k, 40 * k, 48 * k, (float)(n % 6 * 40), (float)(n / 6 * 48), 40, 48);
    };
    digit(10, 2, (float)x);
    digit(s0, 1, x - apart);
    digit(s1, 0, x + apart);
}

void Hud_DrawElement(gfx::Draw2D& d, HudElement& e) {
    if (e.type < 1 || e.type > 3) return;
    const gfx::Texture& t = d.Get(kScore, Hud_Picture(e.picture), e.palette);
    float hw = (float)(e.w / 2), hh = (float)(e.h / 2);
    if (e.type == 1) {
        d.Sprite(t, (float)(e.x - e.w / 2), (float)(e.y - e.h / 2), (float)e.w, (float)e.h, (float)e.u, (float)e.v,
                 (float)e.w, (float)e.h);
        return;
    }
    if (e.type == 3) {
        float k = e.scale;
        if (e.frames) {
            k = e.scale * e.t / e.frames;
            if (e.t < e.frames) ++e.t;
        } else if (k == 0) {
            k = 1;
        }
        d.Sprite(t, e.x - hw * k, e.y - hh * k, 2 * hw * k, 2 * hh * k, (float)e.u, (float)e.v, (float)e.w,
                 (float)e.h);
        return;
    }
    // Type 2: the angle follows the count up to 15, back down to -15 and up again over 60 frames. It draws at the
    // element's scale too (PCSX2: the events' LOSE and MISS are as big as their WIN!).
    float a = (float)e.t;
    e.t = (e.t + 1) % 60;
    if (a >= 45) a -= 60;
    else if (a >= 15) a = 30 - a;
    a *= 0.002f;   // D_00332E34
    float k = e.scale != 0 ? e.scale : 1.0f;
    float c = cosf(a) * k, s = sinf(a) * k;
    const float cx[4] = {-hw, hw, -hw, hw}, cy[4] = {-hh, -hh, hh, hh};
    float x[4], y[4];
    for (int i = 0; i < 4; ++i) {
        x[i] = e.x + cx[i] * c - cy[i] * s;
        y[i] = e.y + cx[i] * s + cy[i] * c;
    }
    d.Quad(t, x, y, (float)e.u, (float)e.v, (float)e.w, (float)e.h);
}

}  // namespace game
