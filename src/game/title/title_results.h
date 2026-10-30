// The title's Results: the 2 Player records of the save Title_ResultsTask (0x0020A580) loads into record 0. A
// high-level reimplementation of VersusRecords_Task (func_00269F48): a menu of Oppose (each rival's wins and races
// by course) and Course (each of six courses' three best laps and totals, and whose). See docs/title.md.
#pragma once

#include "../../gfx/draw2d.h"
#include "../../platform/input.h"
#include "../ui/window.h"

namespace game {

class VersusRecords {
public:
    explicit VersusRecords(gfx::Draw2D& draw) : d_(draw) {}

    void Start();
    // One frame. False once Triangle has left the top menu.
    bool Update(const platform::Input& input);

private:
    void Records_DrawOppose(int rival);   // state 5's panel
    void Records_DrawCourse(int row);     // state 3's panel

    gfx::Draw2D& d_;
    Window window_;
    int state_ = 0;    // +0x25: 1 the top menu, 3 the courses, 5 the rivals (0, 2 and 4 make their windows)
    int cursor_ = 0;   // +0x27: the top menu's cursor
    int rivals_ = 0;   // the rivals listed: the leading entries with a name
};

}  // namespace game
