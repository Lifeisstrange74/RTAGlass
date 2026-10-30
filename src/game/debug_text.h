// The developers' debug text (func_00203A50): an 8 x 8 font in the FONTE atlas (u 448, v 224, palette 15), which
// the debug byte's readouts print with (docs/debug.md, docs/extras.md). Code anywhere queues a line with
// DebugText_Printf while it works something out, as the original prints in the middle of Car_Update; the screen
// draws the frame's lines over everything and clears them.
#pragma once

#include "../gfx/draw2d.h"

namespace game {

// Queues a line at (x, y) in the original's units: pixels across, half-lines (224 a frame) down.
void DebugText_Printf(int x, int y, const char* format, ...);
// func_00203A50's drawing: each character 8 pixels on, 8 x 16 on the 448-line frame, point sampled (on the PS2 a
// texel is a pixel of the 224-line field, each line shown twice, so the font is sharp). The font has no lower case
// (its cells 0x60-0x7F are empty, so on the PS2 "%x"'s a-f don't show); the PC draws them as capitals.
void DebugText_Draw(gfx::Draw2D& d);
void DebugText_Clear();

}  // namespace game
