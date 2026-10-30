// The game's bitmap font: SYS/FONTE.GSL, a 512x384 atlas of 16x24 cells with 16 palettes (colours).
// Reimplements Text_Print (0x00204310), Text_PrintAt (0x00203FC8) and Text_DrawGlyph (0x002017B0).
#pragma once

#include "../gfx/draw2d.h"

namespace game {

// Draws `str` at text cell (col, row): x = col * 20 + 10, y = (row * 12 + 5) * 2 in the 640x448 frame.
// `color` is the palette (0 white, 4 orange ...). Returns the x after the last glyph.
int Text_Print(gfx::Draw2D& d, int col, int row, const char* str, int color, uint32_t rgba = 0x80808080);

// Draws `str` with its top-left at pixel (x, y). (The original takes y in half-lines and doubles it.) Two-byte
// characters are the pad button symbols, e.g. "\x81\x7E" the cross. Returns the x after the last glyph.
int Text_PrintAt(gfx::Draw2D& d, int x, int y, const char* str, int color, uint32_t rgba = 0x80808080);

// Text_PrintClipped (func_00204230): Text_PrintAt with everything above pixel row `top` cut off (the message box's
// scrolling top line).
int Text_PrintClipped(gfx::Draw2D& d, int x, int y, const char* str, int color, int top, uint32_t rgba = 0x80808080);

// Text_PrintScaled (func_002045D0 with func_00201420): `str` at `scale` (256 = full size) with its top-left at
// pixel (x, y): each glyph its width x scale / 256 wide and 24 x scale / 256 tall, a space 10 x scale / 256. Only
// single-byte characters (the item cards' names). Returns the x after the last glyph.
int Text_PrintScaled(gfx::Draw2D& d, int x, int y, const char* str, int color, int scale, uint32_t rgba = 0x80808080);

// Draws one glyph with its top-left at (x, y); returns the x of the next one.
int Text_DrawGlyph(gfx::Draw2D& d, int x, int y, unsigned char c, int color, uint32_t rgba = 0x80808080);

// The race places, " 1st" to "24th" (D_0029FD30), as the Ranking and the results print them.
const char* Text_PlaceName(int place);

// Width in pixels of `str` when printed (Text_Width, 0x00203F40).
int Text_Width(const char* str);

// Number_PrintRight (0x00201E48): `value` in the atlas's large 16x20 digits, ending at pixel x, top at y. (The
// original takes y in half-lines.) Returns the x of the leftmost digit.
int Number_PrintRight(gfx::Draw2D& d, int x, int y, int value, int color, uint32_t rgba = 0x80808080);
// Number_PrintDigits (0x002046C0): the same with exactly `digits` digits, leading zeros included.
int Number_PrintDigits(gfx::Draw2D& d, int x, int y, int value, int digits, int color, uint32_t rgba = 0x80808080);

// Text_PrintSmall (0x002008C0): `str` in the atlas's narrow 10x20 characters, 10 pixels apart; ' " and - come
// from the main font. Times and other figures use it. (The original takes y in half-lines.)
void Text_PrintSmall(gfx::Draw2D& d, int x, int y, const char* str, int color, uint32_t rgba = 0x80808080);
// Number_PrintSmallRight (0x00202150): `value` in the narrow 10x20 digits, with a comma every three digits, ending
// at pixel x, top at pixel y. Returns the x of the leftmost character. (The atlas's comma cell is empty, so the PS2
// shows a gap: "4 321".)
int Number_PrintSmallRight(gfx::Draw2D& d, int x, int y, int value, int color, uint32_t rgba = 0x80808080);
// Time_Print (0x00203B78): a time in frames as M'SS"CC with Text_PrintSmall; "--'--"--" for no time
// (0x7FFFFFFF), and 99'59"99 at most.
void Time_Print(gfx::Draw2D& d, int x, int y, int frames, int color, uint32_t rgba = 0x80808080);

}  // namespace game
