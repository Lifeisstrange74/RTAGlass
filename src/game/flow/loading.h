// The loading screen: Loading_DrawTask (0x0020E140), which the loader (func_0020D120) runs while it reads files.
#pragma once

#include "../../gfx/draw2d.h"

namespace game {

// Draws one frame of the loading screen, `frame` frames after loading began. For the first 64 the last picture
// stays up and darkens by a 6/128 black overlay each frame, going soft as it does on the PS2; after that the screen
// is black. "NOW LOADING..." is at text cell (12, 10) throughout.
void Loading_Draw(gfx::Draw2D& d, int frame);

}  // namespace game
