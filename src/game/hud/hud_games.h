// The Adventure's games' own pieces of the HUD canvas, drawn from the course's SCOREAnn.GSL as HudCanvas_Draw's
// element types draw them: Curling's score board (func_00231208), Ski Jumping's distance (func_002318B0) and
// Fishing's count (func_00231C90). Coordinates are the 640x448 frame's, top left. See docs/minigames.md.
#pragma once

#include "../../gfx/draw2d.h"

namespace game {

// Sprite_Draw of picture `picture`'s (u, v, w, h) in `palette` at (x, y), drawn w x h, or stretched to W x H (the
// quads of func_0024EB10).
void Hud_DrawPiece(gfx::Draw2D& d, int picture, int palette, float x, float y, int u, int v, int w, int h,
                   float W = 0, float H = 0);
// The games' digits in picture 1: large 40 x 48 at ((n % 6) x 40, (n / 6) x 48 + 56), 10 the dash; small 16 x 20 at
// (16 n, 194).
void Hud_DrawLargeDigit(gfx::Draw2D& d, int palette, int n, float x, float y, float W = 40, float H = 48,
                        int picture = 1);
void Hud_DrawSmallDigit(gfx::Draw2D& d, int palette, int n, float x, float y);
// func_0024F4A0 and func_0024F4E0: v's decimal digits, ones first (at least one); returns how many.
int Hud_Digits(int v, int* digits);

// Curling's board (canvas +0x844C): the record, the three tries' points and their total (-1 shows dashes), and,
// with `bigScore`, the try's points large in the middle.
struct CurlingBoard {
    bool shown;          // +0x844C
    bool bigScore;       // +0x845C
    int attempt;         // +0x8458: the try whose points the big score shows
    int points[4];       // +0x8460: the tries' and the total, -1 none
    int best;            // save +0x12CC
};
void Curling_DrawBoard(gfx::Draw2D& d, const CurlingBoard& b);
// Ski Jumping's distance (canvas +0x8470): the record (save +0x12C8) and the jump, in metres.
void SkiJump_DrawDistance(gfx::Draw2D& d, int distance, int best);
// Fishing's count (canvas +0x8480): the fish and how many are caught.
void Fishing_DrawCount(gfx::Draw2D& d, int count);
// The Which-way? Race's TIME LIMIT (canvas +0x8490) and the seconds left, -1 dashes.
void WhichWay_DrawTimeLimit(gfx::Draw2D& d, int seconds);
// Golf's hole panel (canvas +0x8414): the hole, its yards and par on the left; the yards to go and the shot on the
// right.
void Golf_DrawPanel(gfx::Draw2D& d, int hole, int yards, int yardsToGo, int par, int strokes);

}  // namespace game
