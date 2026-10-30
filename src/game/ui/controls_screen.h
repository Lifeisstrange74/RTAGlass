// The Controls screen: the PC's replacement for the field menu's Settings (FieldMenu_Settings, func_00216738),
// opened from there and from the title's Options. It keeps the PS2 screen's look and workings (the list with the
// horn cursor, the help window, the 3D pad leaning towards the button, Initial settings / Cancel change / Apply,
// the clash warnings and "Cancel?"), but each row is an action with a keyboard key and a controller button, set by
// pressing them. Four pages: player 1's and player 2's driving and menu actions. Apply sets the controls in use and
// writes controls.json (platform/controls.h). See docs/controls.md.
#pragma once

#include <stdint.h>

#include <string>

#include "../../gfx/draw2d.h"
#include "../../gfx/model.h"
#include "../../platform/controls.h"
#include "../../platform/input.h"
#include "window.h"

namespace game {

class ControlsScreen {
public:
    // Where data/ is, for the pad (models/SYS/OPTION/0.obj).
    void SetDataDir(const std::string& dir) { dataDir_ = dir; }
    // Opens it: the controls in use, the cursor on the first page's first row.
    void Start();
    // One frame: the pad, then the drawing, over the caller's background (the field menu's road, panel and title
    // car, or the title's). Returns the state, menu::kListLeft once it's over.
    int Update(gfx::Draw2D& d, const platform::Input& input);
    // The title car's move this frame: -40 while it drives in, 40 while it drives out (the field menu's).
    int CarStep() const { return carStep_; }

private:
    int Rows() const;          // the page's rows: the page, its actions, then Initial, Cancel and Apply
    int RowAction(int row) const;   // the action on `row`, or -1
    int RowY(int row) const;
    void CheckClashes();
    bool AnyWarning() const;
    bool Changed() const;
    int UpdateState(const platform::Input& input);
    void Capture(const platform::Input& input);
    void DrawPad(gfx::Draw2D& d, int input);
    void Draw(gfx::Draw2D& d);

    int state_ = 0, count_ = 0, carStep_ = 0;
    int page_ = 0;            // player * 2, + 1 for the menus' page
    int row_ = 0, col_ = 0;   // col 0 the key, 1 the controller button
    bool horn_ = false;       // the cursor shows
    int captureFrames_ = 0;   // frames left to wait for the key or button
    int frame_ = 0;
    platform::Controls edit_;
    // The warnings, per player, action and column: 1 a key or button another action has too, 2 player 1's menu
    // action without a key.
    uint8_t warn_[2][platform::kActionCount][2];
    int16_t rot_[3] = {0, 0, 0};   // the pad's lean, as the PS2 screen's (+0x64)
    Window cancel_;
    bool cancelOpen_ = false;
    std::string dataDir_;
    gfx::Model pad_;
    bool padLoaded_ = false;
};

}  // namespace game
