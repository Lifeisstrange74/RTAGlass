// The Extras screen: the PC's switches for the original game's unused content and its debug features (game/extras.h),
// opened from the title's Options and the field menu's Settings. It has the Controls screen's look and workings
// (the list window with the horn cursor, the help window, Initial settings / Cancel change / Apply, "Cancel?"), on
// two pages, Unused Content and Debug. Left, Right or Cross switches a row; Apply sets the extras in use and writes
// extras.json. The Cheats screen is the same with one page, the cheats (offered only with extras.json's "cheats"
// true). See docs/extras.md.
#pragma once

#include "../../gfx/draw2d.h"
#include "../../platform/input.h"
#include "../extras.h"
#include "window.h"

namespace game {

class ExtrasScreen {
public:
    // Opens it: the extras in use, the cursor on the first page's first row. `cheats`: the Cheats screen.
    void Start(bool cheats = false);
    // One frame: the pad, then the drawing, over the caller's background. Returns the state, menu::kListLeft once
    // it's over.
    int Update(gfx::Draw2D& d, const platform::Input& input);
    // The title car's move this frame: -40 while it drives in, 40 while it drives out (the field menu's).
    int CarStep() const { return carStep_; }

private:
    int Rows() const;            // the page row, the page's extras, then Initial, Cancel and Apply
    int RowExtra(int row) const;   // the extra on `row`, or -1
    int RowY(int row) const;
    bool Changed() const;
    int UpdateState(const platform::Input& input);
    void Draw(gfx::Draw2D& d);

    int state_ = 0, count_ = 0, carStep_ = 0;
    int page_ = 0;   // 0 Unused Content, 1 Debug, 2 Cheats (the Cheats screen's only page)
    int row_ = 0;
    bool horn_ = false;
    ExtrasSet edit_;
    Window cancel_;
    bool cancelOpen_ = false;
};

}  // namespace game
