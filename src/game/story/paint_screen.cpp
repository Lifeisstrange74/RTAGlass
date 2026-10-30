#include "paint_screen.h"

#include "../../audio/sound.h"
#include "../save/save_data.h"
#include "../text.h"
#include "parts_screen.h"
#include "../../game_text.inc"   // the game's text, GT_<address>_<length>, made from the ELF

namespace game {

namespace {

// D_0029D190: a colour level's byte (0x80 = 1.0) for the screen's colour bars; the car itself takes D_0029D150's
// 0.1 + 0.05 n (car.cpp's Paint).
const uint8_t kLevelByte[16] = {0x19, 0x26, 0x33, 0x3F, 0x4C, 0x59, 0x66, 0x72,
                                0x7F, 0x8C, 0x99, 0xA5, 0xB2, 0xBF, 0xCC, 0xD8};
// D_002AC600: where the two glows go round the lamp while the light flashes; 8 is out of sight.
const int16_t kGlowPlace[9][2] = {{0x16C, 0x00}, {0x17D, 0x08}, {0x19D, 0x0F}, {0x1C7, 0x14}, {0x1F3, 0x14},
                                  {0x219, 0x11}, {0x230, 0x08}, {0x000, 0x00}, {0x000, -0x40}};
// The rows' places: the colour bars (+0xBF0 + 0x600 n), their letters (+0x9B0 + 0x60 n), the levels' digits.
const int kBarY[6] = {0x85, 0x98, 0xAB, 0xD3, 0xE6, 0xF9};
const int kLetterY[6] = {0x83, 0x96, 0xA9, 0xD1, 0xE4, 0xF7};
const int kDigitsY[6] = {0x81, 0x94, 0xA7, 0xCF, 0xE2, 0xF5};
// func_002558B8's switch: the horn cursor's y by row (as the game has them: rows 1-3 and 4-6 only 8 apart).
const int kCursorY[9] = {0x3E, 0x8A, 0x92, 0x9A, 0xD8, 0xE0, 0xE8, 0x148, 0x18A};
// The level bars' colours (+0x2FF0 + 0x30 n): red, green, blue.
const uint32_t kBarColor[3] = {0x800000FFu, 0x8000FF00u, 0x80FF0000u};
const gfx::Rect kFrame = {0, 0, gfx::Draw2D::kWidth, gfx::Draw2D::kHeight};

int Nibble(uint32_t paint, int n) { return (int)(paint >> (4 * n)) & 0xF; }

// A panel of the room picture (func_00255070): its left end, 0xDC of an 8-texel strip (repeated by the GS's
// REGION_REPEAT; the strip is the same all along, so here it's stretched), its right end.
// The short ones (Colors, the bottom row) are 0x28 high from (0x50, 0x132), the tall ones 0x4E from (0, 0x132).
void Panel(gfx::Draw2D& d, const gfx::Texture& t, int y, bool tall, uint32_t rgba) {
    const int u = tall ? 0 : 0x50, h = tall ? 0x4E : 0x28;
    d.Sprite(t, 0x24, (float)y, 0x30, (float)h, (float)u, 0x132, 0x30, (float)h, rgba);
    d.Sprite(t, 0x54, (float)y, 0xDC, (float)h, (float)(u + 0x30), 0x132, 8, (float)h, rgba);
    d.Sprite(t, 0x130, (float)y, 0x12, (float)h, (float)(u + 0x38), 0x132, 0x12, (float)h, rgba);
}

// A panel's orange tab (0x64 x 0x1A from (0, 0x116)) and its label, 3 right and 3 down.
void Tab(gfx::Draw2D& d, const gfx::Texture& t, int y, int labelV) {
    d.Sprite(t, 0x24, (float)y, 0x64, 0x1A, 0, 0x116, 0x64, 0x1A);
    d.Sprite(t, 0x2A, (float)(y + 3), 0x5C, 0x14, 0x80, (float)labelV, 0x5C, 0x14);
}

// The light and the glows are added to the frame by their alpha (ALPHA 0x48; checked in the GS dump
// captures/adventure/paint_flash.gs).
gfx::Draw3DState Additive() {
    gfx::Draw3DState st;
    st.depthTest = st.depthWrite = false;
    st.additive = true;
    return st;
}

}  // namespace

PaintScreen::PaintScreen(gfx::Draw2D& draw) : d_(draw) {}

uint32_t& PaintScreen::Paint() { return Save_Current().cars[car_].paint; }

int PaintScreen::PaintShop_Cost() {
    const uint32_t d = Paint() ^ original_;
    return ((d & 0xFFFFFF) ? 100 : 0) + ((d >> 24) ? 100 : 0);
}

// PaintShop_Task (func_00256958), its first frame: +0x00's top byte = +0x05 (the wheel colour); the paint kept
// (+0x20); two colours (+0x27) unless paint 2 is paint 1 (func_00255070 then builds the sprites).
void PaintScreen::Start(int car, const gfx::Texture* picture, const gfx::Texture* beam, const gfx::Texture* glow) {
    car_ = car;
    picture_ = picture;
    beam_ = beam;
    glow_ = glow;
    SaveCar& c = Save_Current().cars[car];
    c.paint = (c.paint & 0xFFFFFF) | (uint32_t)c.wheelColor << 24;
    original_ = c.paint;
    twoTone_ = (c.paint & 0xFFF) != ((c.paint >> 12) & 0xFFF);
    row_ = 0;
    apply_ = false;
    timer_ = 0;
    lampY_ = -0x38;
    frame_ = 0;
    beamU_ = 0;
    beamFlash_ = 0;
    glowAt_ = 8;
    notice_ = false;
    changed_ = false;
    stage_ = kWait;
}

// func_002560F0, the menu's input on pad 1. Up and Down (repeating) move between the rows, SE 0x1D; a one-colour
// car skips paint 2's rows (down from 3 to Wheel, up from Wheel to 3). Left and Right (repeating) step a colour
// level 0-15 (no sound), which flashes the light (+0x02 = 0x20) and picks OK; on Wheel they step the wheel colour
// 0-11 (both +0x05 and the paint's top byte; SE 0x1D, OK). A press of Left or Right on Colors switches between one
// and two colours, on the bottom row between OK and Cancel (SE 0x1D). One colour: paint 2 copies paint 1.
void PaintScreen::PaintShop_Input(const platform::Input& input) {
    const uint32_t rep = input.Repeated(0), press = input.Pressed(0);
    if ((rep & platform::kUp) && row_ > 0) {
        --row_;
        audio::Sound_PlaySe(0x1D);
    }
    if ((rep & platform::kDown) && row_ < 8) {
        ++row_;
        audio::Sound_PlaySe(0x1D);
    }
    const int step = (rep & platform::kRight) ? 1 : (rep & platform::kLeft) ? -1 : 0;
    uint32_t& paint = Paint();
    if ((row_ == 4 || row_ == 6) && !twoTone_) {
        row_ = row_ == 4 ? 7 : 3;   // (the row skipped to waits for the next frame's input)
    } else if (row_ == 0 || row_ == 8) {
        if (press & (platform::kLeft | platform::kRight)) {
            if (row_ == 0)
                twoTone_ = !twoTone_;
            else
                apply_ = !apply_;
            audio::Sound_PlaySe(0x1D);
        }
    } else if (row_ == 7) {
        SaveCar& c = Save_Current().cars[car_];
        const int n = c.wheelColor + step;
        if (n >= 0 && n < 12) {
            c.wheelColor = (uint8_t)n;
            paint = (paint & 0xFFFFFF) | (uint32_t)n << 24;
            if (step) {
                audio::Sound_PlaySe(0x1D);
                apply_ = true;
            }
        }
    } else {
        const int shift = 4 * (row_ - 1);
        int v = Nibble(paint, row_ - 1);
        if ((step > 0 && v < 15) || (step < 0 && v > 0)) {
            v += step;
            timer_ = 0x20;
            apply_ = true;
        }
        paint = (paint & ~(0xFu << shift)) | (uint32_t)v << shift;
    }
    if (!twoTone_) paint = (paint & 0xFF000FFF) | (paint & 0xFFF) << 12;
}

void PaintScreen::PaintShop_DrawLamp() {
    if (picture_) d_.Sprite(*picture_, 0x178, (float)lampY_, 0xD6, 0x38, 0x120, 0x140, 0xD6, 0x38);
}

// The lamp's light (+0x3150): a quad from under the lamp down over the turntable, the 128 x 128 wave texture
// scrolling sideways (and repeating), in the colour the menu set (+0x3148: the row's red, green or blue at 4 x the
// flash's alpha).
void PaintScreen::PaintShop_DrawBeam() {
    if (!beam_ || !(beamFlash_ >> 24)) return;
    const uint32_t c = gfx::Draw2D::Color((uint32_t)beamFlash_);
    const float s0 = beamU_ / 128.0f, s1 = s0 + 1.0f;
    const gfx::Vertex v[4] = {{0x190, 0x10, 0, c, s0, 0, 1, 1}, {0x236, 0x10, 0, c, s1, 0, 1, 1},
                              {0x170, 0x100, 0, c, s0, 1, 1, 1}, {0x256, 0x100, 0, c, s1, 1, 1, 1}};
    const gfx::Vertex tri[6] = {v[0], v[1], v[2], v[1], v[3], v[2]};
    d_.Triangles(tri, 6, beam_, false, true, kFrame, Additive());
}

// func_002557A0: the two cyan glows (32 x 32), at D_002AC600's places 4 apart going round the lamp while the light
// flashes, else out of sight.
void PaintScreen::PaintShop_DrawGlows() {
    if (!glow_ || glowAt_ == 8) return;
    const uint32_t c = gfx::Draw2D::Color(0x80808000u);   // (0, 0x80, 0x80): cyan
    for (int g = 0; g < 2; ++g) {
        const int16_t* p = kGlowPlace[(glowAt_ + 4 * g) & 7];
        const float x = p[0], y = p[1];
        const gfx::Vertex v[4] = {{x, y, 0, c, 0, 0, 1, 1}, {x + 32, y, 0, c, 1, 0, 1, 1},
                                  {x, y + 32, 0, c, 0, 1, 1, 1}, {x + 32, y + 32, 0, c, 1, 1, 1, 1}};
        const gfx::Vertex tri[6] = {v[0], v[1], v[2], v[1], v[3], v[2]};
        d_.Triangles(tri, 6, glow_, true, true, kFrame, Additive());
    }
}

// func_002558B8 with func_00255070's sprites, in the canvas's order: the lamp; the panels Colors (y 0x34), Body
// (0x76, paint 1; paint 2's at 0xC4 is grey and faint for one colour), Wheel (0x12C) and the bottom row (0x180);
// on Colors "1" and "2" (orange the one chosen), at the bottom OK and Cancel (orange the one chosen); the wheel
// colours with a ring on the car's (and on the Wheel row the pointing hand over it); each colour row's ball (bright
// on the cursor's row) and 16 squares of its colour with that level stepping 0-15, a bar in the level's colour
// under them as long as the level; the light and the glows. Then the levels in digits and the cursor.
void PaintScreen::PaintShop_Draw() {
    PaintShop_DrawLamp();
    if (!picture_) return;
    const gfx::Texture& t = *picture_;
    const uint32_t paint = Paint();
    const int wheel = (int)(paint >> 24);
    Panel(d_, t, 0x34, false, 0x80808080u);
    Tab(d_, t, 0x1E, 0xF0);
    Panel(d_, t, 0x76, true, 0x80808080u);
    Tab(d_, t, 0x60, 0x104);
    Panel(d_, t, 0x12C, true, 0x80808080u);
    Tab(d_, t, 0x116, 0x118);
    d_.Sprite(t, 0x66, 0x13E, 0xE4, 0x2A, 0x118, 0xE0, 0xE4, 0x2A);
    Panel(d_, t, 0x180, false, 0x80808080u);
    const float one = (float)(twoTone_ ? 0xCC : 0x6C), two = (float)(twoTone_ ? 0x6C : 0xCC);
    d_.Sprite(t, one, 0x3E, 0x40, 0x18, twoTone_ ? 0x40 : 0.0f, 0xF8, 0x40, 0x18);
    d_.Sprite(t, two, 0x3E, 0x40, 0x18, twoTone_ ? 0.0f : 0x40, 0xE0, 0x40, 0x18);
    d_.Sprite(t, 0x51, 0x18A, 0x60, 0x18, 0xB0, (float)(apply_ ? 0x160 : 0x148), 0x60, 0x18);
    d_.Sprite(t, 0xBA, 0x18A, 0x60, 0x18, (float)(apply_ ? 0xB0 : 0x50), (float)(apply_ ? 0x130 : 0x160), 0x60, 0x18);
    const float wx = (float)(wheel * 27 / 2 + 0x65);
    d_.Sprite(t, wx, (float)(0x13E + 18 * (wheel & 1)), 0x20, 0x1C, 0x1B0, 0x120, 0x20, 0x1C);
    if (row_ == 7) d_.Sprite(t, wx, (float)(0x126 + 18 * (wheel & 1)), 0x20, 0x20, 0x1D5, 0x11C, 0x20, 0x20);
    Panel(d_, t, 0xC4, true, twoTone_ ? 0x80808080u : 0x60404040u);
    for (int k = 0; k < 6; ++k) {
        uint32_t rgba = k == row_ - 1 ? 0x80808080u : (k < 3 || twoTone_) ? 0x80404040u : 0x40404040u;
        d_.Sprite(t, 0x5B, (float)kLetterY[k], 0x10, 0x10, 0x100, (float)(0xF0 + 0x10 * (k % 3)), 0x10, 0x10, rgba);
    }
    for (int j = 0; j < 6; ++j) {
        const int base = j / 3 * 3;
        const uint32_t alpha = (j < 3 || twoTone_) ? 0x80u : 0x40u;
        for (int i = 0; i < 16; ++i) {
            uint32_t rgba = alpha << 24;
            for (int ch = 0; ch < 3; ++ch)
                rgba |= (uint32_t)kLevelByte[ch == j % 3 ? i : Nibble(paint, base + ch)] << (8 * ch);
            d_.Sprite(t, (float)(0x70 + 10 * i), (float)kBarY[j], 10, 10, 0x80, 0xE0, 10, 10, rgba);
        }
    }
    for (int j = 0; j < 6; ++j) {
        const uint32_t rgba = (kBarColor[j % 3] & 0xFFFFFF) | ((j < 3 || twoTone_) ? 0x80u : 0x40u) << 24;
        d_.Fill(0x70, (float)(kBarY[j] + 10), (float)(10 * Nibble(paint, j) + 10), 2, rgba);
    }
    PaintShop_DrawBeam();
    PaintShop_DrawGlows();
    for (int j = 0; j < 6; ++j) Number_PrintSmallRight(d_, 0x128, kDigitsY[j], Nibble(paint, j), 0);
    Window_DrawCursor(d_, 0x20, kCursorY[row_]);
}

// func_00256640, the menu. The input; the light's colour by the row (red, green or blue at 4 x +0x02's alpha,
// +0x02 counting down), its scroll and the glows (going round while it flashes). Cross on the bottom row, or
// Triangle anywhere (which picks Cancel), ends it: nothing changed, the scene is told at once; else SE 0x1A and
// with Cancel Notice_AskTask's "Cancel?", with OK "OK?" if the money covers the cost (Notice_Task's "Need more
// money" if not, which picks Cancel), then func_002564E8. The money now and after the cost (Wallet_Draw).
void PaintScreen::PaintShop_MenuTask(const platform::Input& input) {
    const uint32_t before = Paint();
    PaintShop_Input(input);
    changed_ = Paint() != before;
    beamFlash_ = timer_ << 26;
    if (row_ >= 1 && row_ <= 6) beamFlash_ |= 0xFF << (8 * ((row_ - 1) % 3));
    if (timer_ != 0) --timer_;
    beamU_ = (frame_ * 4) & 0x7C;
    glowAt_ = timer_ ? (frame_ / 2) & 7 : 8;
    PaintShop_Draw();
    const uint32_t press = input.Pressed(0);
    if ((row_ == 8 && (press & platform::kCross)) || (press & platform::kTriangle)) {
        if (press & platform::kTriangle) apply_ = false;
        if (Paint() == original_) {
            PaintShop_Done();
        } else {
            audio::Sound_PlaySe(0x1A);
            ask_ = Window();
            notice_ = false;
            if (!apply_) {
                ask_.AddLine(GT_00333558_7, 0);
            } else if (Save_Current().money < PaintShop_Cost()) {
                apply_ = false;
                ask_.AddLine(GT_00328448_15, 0);
                notice_ = true;
                audio::Sound_PlaySe(0x1E);
            } else {
                ask_.AddLine("OK?", 0);
            }
            if (!notice_) {
                ask_.AddLine("Yes", kLineSelectable);
                ask_.AddLine("No", kLineSelectable);
            }
            stage_ = kAsk;
            timer_ = 0;
        }
    }
    Money_DrawAfter(d_, PaintShop_Cost(), frame_, true);
}

// func_002564E8, waiting for the answer, the screen as it was (the light and the glows held). Yes: with OK the
// cost is paid (Money_Pay, SE 0x305) and the paint is the car's; with Cancel (or after Need more money) the car
// gets back its paint and wheel colour; either way the scene is told. No: back to the menu, SE 0x1B.
void PaintScreen::PaintShop_AnswerTask(const platform::Input& input) {
    PaintShop_Draw();
    const int r = ask_.Update(d_, input);
    if (r != 0) {
        const int answer = notice_ ? 0 : (r == 1 ? ask_.sel : 1);
        if (answer == 0) {
            if (apply_) {
                Money_Pay(PaintShop_Cost());
                original_ = Paint();
            } else {
                Paint() = original_;
                Save_Current().cars[car_].wheelColor = (uint8_t)(original_ >> 24);
                changed_ = true;
            }
            PaintShop_Done();
        } else {
            stage_ = kMenu;
            audio::Sound_PlaySe(0x1B);
        }
    }
    Money_DrawAfter(d_, PaintShop_Cost(), frame_, true);
}

void PaintScreen::PaintShop_Done() {
    stage_ = kSlideOut;
    timer_ = 0;
}

int PaintScreen::Update(const platform::Input& input, int carState) {
    changed_ = false;
    int result = 0;
    switch (stage_) {
    case kWait:
        // PaintShop_Task waits for the car to face the camera on the turntable, then the lamp slides in.
        if (car_ == 0 ? (carState == 0xC || carState == 0xD) : carState == 4) {
            timer_ = -0x38;
            stage_ = kSlideIn;
        } else {
            break;
        }
        // fall through
    case kSlideIn:
        // func_00256A78: the lamp 4 lower a frame, from -0x38 to 0, alone.
        timer_ += 4;
        lampY_ = timer_;
        PaintShop_DrawLamp();
        if (timer_ == 0) stage_ = kMenu;
        break;
    case kMenu:
        PaintShop_MenuTask(input);
        if (stage_ == kSlideOut) result = -1;
        break;
    case kAsk:
        PaintShop_AnswerTask(input);
        if (stage_ == kSlideOut) result = -1;
        break;
    case kSlideOut:
        // func_002568D8: the lamp 4 higher a frame, alone, until it's out of sight at -0x38.
        timer_ -= 4;
        lampY_ = timer_;
        PaintShop_DrawLamp();
        if (timer_ == -0x38) stage_ = kDone;
        break;
    case kDone:
        break;
    }
    ++frame_;
    return result;
}

}  // namespace game
