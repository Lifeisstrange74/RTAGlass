#include "hud_games.h"

#include "hud_parts.h"

namespace game {

namespace {

// The record's pill ("BEST RECORD", D_002EBF20) and its small digits, as Curling and Ski Jumping draw them.
void DrawBest(gfx::Draw2D& d, int best) {
    Hud_DrawPiece(d, 1, 2, 32, 24, 0, 216, 112, 32);
    int n[10], count = Hud_Digits(best, n);
    for (int k = 0; k < count; ++k) Hud_DrawSmallDigit(d, 1, n[k], (float)(82 - 15 * k), 65);
}

}  // namespace

void Hud_DrawPiece(gfx::Draw2D& d, int picture, int palette, float x, float y, int u, int v, int w, int h, float W,
                   float H) {
    const gfx::Texture& t = d.Get(Hud_ScoreFile(), Hud_Picture(picture), palette);
    W = W > 0 ? W : (float)w;
    H = H > 0 ? H : (float)h;
    // On a wide screen each piece goes with its third of the screen (the games' pieces keep to their corners and
    // the middle, so a group never straddles two).
    gfx::AnchorScope anchor(d, Hud_AnchorAt(x + W / 2));
    d.Sprite(t, x, y, W, H, (float)u, (float)v, (float)w, (float)h);
}

void Hud_DrawLargeDigit(gfx::Draw2D& d, int palette, int n, float x, float y, float W, float H, int picture) {
    Hud_DrawPiece(d, picture, palette, x, y, n % 6 * 40, n / 6 * 48 + 56, 40, 48, W, H);
}

void Hud_DrawSmallDigit(gfx::Draw2D& d, int palette, int n, float x, float y) {
    Hud_DrawPiece(d, 1, palette, x, y, 16 * n, 194, 16, 20);
}

int Hud_Digits(int v, int* digits) {
    int count = 0;
    unsigned u = v < 0 ? 0u : (unsigned)v;
    do {
        digits[count++] = (int)(u % 10);
        u /= 10;
    } while (u && count < 10);
    return count;
}

// func_00231208, picture 1 throughout: the pill, the small "Pts" and the record; the four rows (palette 3, 30 x 36
// digits, three dashes for -1), the large "Pts" and the "1", "2", "3" circles (palette 11); then the big score,
// 80 x 96 digits centred on x 320.
void Curling_DrawBoard(gfx::Draw2D& d, const CurlingBoard& b) {
    int n[10];
    if (b.shown) {
        DrawBest(d, b.best);
        Hud_DrawPiece(d, 1, 1, 98, 65, 160, 194, 32, 20);
        for (int k = 0; k < 4; ++k) {
            int count = 3;
            if (b.points[k] == -1) n[0] = n[1] = n[2] = 10;
            else count = Hud_Digits(b.points[k], n);
            for (int j = 0; j < count; ++j)
                Hud_DrawLargeDigit(d, 3, n[j], (float)(545 - 30 * j), (float)(32 + 36 * k), 30, 36);
        }
        Hud_DrawPiece(d, 1, 3, 575, 140, 200, 104, 56, 48, 45, 36);
        for (int k = 0; k < 3; ++k) Hud_DrawPiece(d, 1, 11, 458, (float)(38 + 36 * k), 112 + 24 * k, 216, 24, 24);
    }
    if (b.bigScore && b.attempt >= 0 && b.attempt < 3) {
        int count = Hud_Digits(b.points[b.attempt], n);
        for (int j = 0; j < count; ++j)
            Hud_DrawLargeDigit(d, 3, n[j], (float)(240 + 40 * count - 80 * j), 160, 80, 96);
    }
}

// func_002318B0: the pill, the small and the large "M", the record and the jump (40 x 48 digits, palette 4).
void SkiJump_DrawDistance(gfx::Draw2D& d, int distance, int best) {
    DrawBest(d, best);
    Hud_DrawPiece(d, 1, 1, 98, 65, 160, 194, 24, 20);
    Hud_DrawPiece(d, 1, 4, 562, 24, 200, 104, 48, 48);
    int n[10], count = Hud_Digits(distance, n);
    for (int k = 0; k < count; ++k) Hud_DrawLargeDigit(d, 4, n[k], (float)(522 - 37 * k), 24);
}

// func_00231C90: the fish and the "x" (palette 9), then the count (40 x 48 digits, palette 4).
void Fishing_DrawCount(gfx::Draw2D& d, int count) {
    Hud_DrawPiece(d, 1, 9, 453, 40, 160, 104, 56, 38);
    Hud_DrawPiece(d, 1, 9, 512, 48, 216, 104, 22, 20);
    int n[10], k = Hud_Digits(count, n);
    for (int i = 0; i < k; ++i) Hud_DrawLargeDigit(d, 4, n[i], (float)(576 - 37 * i), 34);
}

// func_00231F28, picture 0: TIME LIMIT (D_002EC050: 0, 193, 124 x 20) in palette 7 at (258, 20), then the seconds
// in palette 3, the large digits drawn 30 x 36, centred on x 318 at y 40.
void WhichWay_DrawTimeLimit(gfx::Draw2D& d, int seconds) {
    Hud_DrawPiece(d, 0, 7, 258, 20, 0, 193, 124, 20);
    int n[10], count = 3;
    if (seconds == -1) n[0] = n[1] = n[2] = 10;
    else count = Hud_Digits(seconds, n);
    for (int j = 0; j < count; ++j)
        Hud_DrawLargeDigit(d, 3, n[j], (float)(15 * count + 288 - 30 * j), 40, 30, 36, 0);
}

// Golf's 20 x 30 digits (picture 1, palette 1: (20 n, 145)), right to left from the first x, `step` apart, at most
// four (func_0024F4A0's BCD: the first always, more while what's left isn't 0).
static void GolfDigits(gfx::Draw2D& d, int value, float x, float y, float step) {
    int n[10], count = Hud_Digits(value, n);
    for (int k = 0; k < count && k < 4; ++k) Hud_DrawPiece(d, 1, 1, x - step * k, y, 20 * n[k], 145, 20, 30);
}

// Hud_DrawGolfPanel (func_00230520): at (40, 20) the ball (D_002EBE60), the hole, its yards and "Y", "PAR" and the
// par; at (522, 20) "LAST", the yards to go and "Y", "SHOT" and the stroke being played.
void Golf_DrawPanel(gfx::Draw2D& d, int hole, int yards, int yardsToGo, int par, int strokes) {
    const float lx = 40, ly = 20, rx = 522, ry = 20;
    Hud_DrawPiece(d, 0, 3, lx + 30, ly + 0, 173, 1, 40, 40);
    Hud_DrawPiece(d, 1, 1, lx + 40, ly + 5, 20 * (hole + 1), 145, 20, 30);
    Hud_DrawPiece(d, 1, 1, lx + 78, ly + 46, 200, 145, 20, 30);
    GolfDigits(d, yards, lx + 52, ly + 46, 24);
    Hud_DrawPiece(d, 0, 4, lx + 4, ly + 80, 185, 89, 70, 30);
    Hud_DrawPiece(d, 1, 1, lx + 78, ly + 80, 20 * par, 145, 20, 30);
    Hud_DrawPiece(d, 0, 4, rx + 22, ry + 10, 201, 49, 54, 30);
    Hud_DrawPiece(d, 1, 1, rx + 78, ry + 46, 200, 145, 20, 30);
    GolfDigits(d, yardsToGo, rx + 52, ry + 46, 24);
    Hud_DrawPiece(d, 0, 4, rx + 0, ry + 80, 176, 121, 60, 32);
    GolfDigits(d, strokes + 1, rx + 70, ry + 80, 20);
}

}  // namespace game
