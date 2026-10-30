#include "loading.h"

#include "../text.h"
#include "../../game_text.inc"   // the game's text, GT_<address>_<length>, made from the ELF

namespace game {

void Loading_Draw(gfx::Draw2D& d, int frame) {
    if (frame < 0x40) {
        d.BeginFrameFromLast(frame & 1 ? 0.5f : -0.5f);
        d.Fill(0, 0, gfx::Draw2D::kWidth, gfx::Draw2D::kHeight, 0x06000000);
    } else {
        d.BeginFrame(0x80000000);
    }
    Text_Print(d, 12, 10, GT_002EA398_14, 0);
    d.EndFrame(0x20);
}

}  // namespace game
