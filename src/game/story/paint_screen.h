// The Paint Shops' paint screen: a high-level reimplementation of PaintShop_Task (func_00256958) and the tasks it
// hands over to, which the building scene runs for a talk's car pick (end code 6, scene result 8 | car << 8). A lamp
// slides down at the top right; panels at the left set the car's two colours (Colors: one or two, then Body's
// red, green and blue levels, each 0-15, for paint 1 and paint 2) and its wheels' colour (Wheel: 12 of them), and
// the car on the turntable shows the change at once. OK pays 100 for new paint and 100 for new wheels; Cancel puts
// the car back as it was. Every piece is in the Paint Shop's room picture. See docs/paint.md.
#pragma once

#include <stdint.h>

#include "../../gfx/draw2d.h"
#include "../../platform/input.h"
#include "../ui/window.h"

namespace game {

class PaintScreen {
public:
    explicit PaintScreen(gfx::Draw2D& draw);

    // PaintShop_Task's first frame for car `car` of the team (D_0177EA50: 0 the player's, 1 and 2 the teammates'):
    // the save's car paint takes the wheel colour as its top byte, and the paint as it is now is kept for Cancel.
    // `picture` is the Paint Shop's room picture; `beam` and `glow` are the room file's textures 4 and 5.
    void Start(int car, const gfx::Texture* picture, const gfx::Texture* beam, const gfx::Texture* glow);
    // One frame, drawn over the room. `carState` is the scene's car state (+0x27): the screen waits for the car
    // on the turntable to face the camera (0xC or 0xD for the player's car, 4 for a teammate's). Returns -1 on the
    // frame the screen tells the scene it's done (+0x04 = -1, so the talk starts over), else 0. The lamp then slides
    // away over a few more frames: keep calling Update while Active().
    int Update(const platform::Input& input, int carState);
    bool Active() const { return stage_ != kDone; }
    void Stop() { stage_ = kDone; }
    // The save's car paint changed on this frame, for the scene to show it.
    bool Changed() const { return changed_; }

private:
    enum Stage { kWait, kSlideIn, kMenu, kAsk, kSlideOut, kDone };

    uint32_t& Paint();                     // the save car's +0x00
    int PaintShop_Cost();                  // 100 for the colours changed, 100 for the wheels
    void PaintShop_Input(const platform::Input& input);   // func_002560F0
    void PaintShop_Draw();                 // func_002558B8, and the sprites of func_00255070 it fills in
    void PaintShop_DrawLamp();             // the lamp (D_0175B570 + 0x70) at (0x178, lampY_)
    void PaintShop_DrawBeam();             // the lamp's light (+0x3150)
    void PaintShop_DrawGlows();            // func_002557A0's two glows (+0x3220, +0x3270)
    void PaintShop_MenuTask(const platform::Input& input);     // func_00256640
    void PaintShop_AnswerTask(const platform::Input& input);   // func_002564E8
    void PaintShop_Done();                 // the scene told; on to func_002568D8

    gfx::Draw2D& d_;
    const gfx::Texture* picture_ = nullptr;
    const gfx::Texture* beam_ = nullptr;
    const gfx::Texture* glow_ = nullptr;
    int car_ = 0;            // +0x1C
    uint32_t original_ = 0;  // +0x20: the paint (with the wheel colour on top) to go back to, or that was paid for
    Stage stage_ = kDone;
    int row_ = 0;            // +0x26: 0 Colors, 1-3 paint 1's red, green, blue, 4-6 paint 2's, 7 Wheel, 8 OK/Cancel
    bool twoTone_ = false;   // +0x27: two colours
    bool apply_ = false;     // +0x25: OK (true) or Cancel; a change picks OK
    int timer_ = 0;          // +0x02: the lamp's slide; in the menu, the light's flash after a change (0x20 down)
    int lampY_ = -0x38;      // the lamp sprite's y
    int frame_ = 0;          // +0x08: the task's frames
    int beamU_ = 0;          // the light's texture scroll, (frame x 4) & 0x7C
    int beamFlash_ = 0;      // the light's colour (+0x3148) as the menu last set it, a PS2 RGBA word
    int glowAt_ = 8;         // D_002AC600's place for the first glow (the second is 4 on); 8 out of sight
    Window ask_;             // Notice_AskTask's "Cancel?" / "OK?", or Notice_Task's "Need more money"
    bool notice_ = false;    // ask_ is the notice: any press answers 0
    bool changed_ = false;
};

}  // namespace game
