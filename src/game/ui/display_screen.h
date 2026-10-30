// The Display screen: the PC's display settings (platform/display.h), opened from the title's Options and the field
// menu's Settings. It has the Extras screen's look and workings (the list window with the horn cursor, the help
// window, Initial settings / Cancel change / Apply, "Cancel?"). Left and Right (or Cross) change a row's value;
// Apply keeps them in display.json and the window, the screen and the picture change on the next frame. See
// docs/display.md.
#pragma once

#include <vector>

#include "../../gfx/draw2d.h"
#include "../../platform/display.h"
#include "../../platform/input.h"
#include "window.h"

namespace game {

class DisplayScreen {
public:
    // Opens it: the settings in use, the cursor on the first row.
    void Start();
    // One frame: the pad, then the drawing, over the caller's background. Returns the state, menu::kListLeft once
    // it's over.
    int Update(gfx::Draw2D& d, const platform::Input& input);
    // The title car's move this frame: -40 while it drives in, 40 while it drives out (the field menu's).
    int CarStep() const { return carStep_; }

private:
    enum Row {
        kRowMode, kRowResizable, kRowSize, kRowAspect, kRowHud, kRowScale, kRowVsync, kRowLimit, kRowGraphics,
        kSettingRows
    };
    int Rows() const { return kSettingRows + 3; }   // the settings, then Initial, Cancel and Apply
    int RowY(int row) const;
    bool Changed() const;
    // The Resolution row's choices for the mode being edited (0 x 0 first: Auto, but for Borderless), and which one
    // the edit has.
    std::vector<platform::DisplaySize> Sizes() const;
    int SizeIndex(const std::vector<platform::DisplaySize>& sizes) const;
    void ChangeValue(int row, int step);
    int UpdateState(const platform::Input& input);
    void Draw(gfx::Draw2D& d);

    int state_ = 0, count_ = 0, carStep_ = 0;
    int row_ = 0;
    bool horn_ = false;
    platform::DisplaySettings edit_ = {};
    Window cancel_;
    bool cancelOpen_ = false;
};

}  // namespace game
