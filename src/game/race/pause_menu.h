// Start's pause menu during a race (Race_PauseTask, func_0026BC08): a window of Continue and Retire, and in the
// Adventure Ranking, which lists every car by its place with its driver and home town (Race_RankingTask,
// func_00237878). The race behind it stands still and is drawn at half brightness. See docs/pause.md.
#pragma once

#include "../../gfx/draw2d.h"
#include "../../platform/input.h"
#include "../ui/window.h"

namespace game {

// One line of the Ranking: a car's place (0 = first), its driver and its home town.
struct RankingRow {
    int place;
    const char* name;
    const char* town;
};

class PauseMenu {
public:
    enum Choice { kNone, kContinue, kRetire, kHideMeter, kFullTank, kAiDrive, kZoneTable };

    // The task's first frame: the window, centred, with Continue, Retire and, if `ranking` (the Adventure, on its
    // courses below 0x12), Ranking. Start chooses too (window mode 8). The window shows from the next frame, at
    // once, without fading in. Only pad `pad` works it (window mode 8 + the pad: in 2 Player the pad whose Start
    // paused).
    //
    // `debug` (the Pause Items extra) adds the debug build's items 3-6 (Race_PauseTask's jump table; their lines
    // aren't in the retail window): Hide Meter / Show Meter (`meterHidden` says which), Full Tank, AI Drive and Zone
    // Table. The first three leave the menu up; Zone Table hands over to the zone table editor, which the race runs
    // (Ai_ZoneTableTask) while the window waits, undrawn, as for the Ranking.
    void Open(bool ranking, int pad = 0, bool debug = false, bool meterHidden = false);
    // Item 3's line after a toggle: Show Meter while the meter is hidden.
    void SetMeterHidden(bool hidden);
    // One frame of the menu, over the race. kContinue for Continue (Triangle too), kRetire for Retire; Ranking
    // shows `rows` until Cross or Triangle closes it.
    Choice Update(gfx::Draw2D& d, const platform::Input& input, const RankingRow* rows, int count);

private:
    // Race_RankingTask: each car's row at y = place x 24 - scroll + 128, the place at x 120, the driver at 180, the
    // town at 380. Down and Up (held) scroll by 2 pixels a frame, down to the last car. Cross or Triangle closes it,
    // from its second frame on.
    bool Race_RankingTask(gfx::Draw2D& d, const platform::Input& input, const RankingRow* rows, int count);

    Window window_;
    int items_[8];   // each line's item (Race_PauseTask's sel: the retail window has no gaps before Ranking)
    int meterLine_;  // the line of item 3, or -1
    bool ranking_;   // +0x25: the Ranking is up
    int scroll_;     // the Ranking's +2
    int frames_;     // the Ranking's frame count (+8)
};

}  // namespace game
