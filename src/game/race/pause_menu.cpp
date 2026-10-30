#include "pause_menu.h"

#include "../text.h"
#include "../../game_text.inc"   // the game's text, GT_<address>_<length>, made from the ELF

namespace game {

namespace {

// The menu's items, in order (D_00335608 "Retire", D_00335610 "Ranking"), then the debug build's (jtbl_0032FA00):
// 3 toggles race flag 0x10000 (SE 0x1A), 4 fills the player's tank (car +0x23C = 0x40000, SE 0x1A; its line then
// greys out: window +0x54 |= 1), 5 toggles the player's car flag 0x80 (the AI drives it), 6 makes the task the AI
// zone table editor (task +0xC = Ai_ZoneTableTask, 0x0026BE28; no sound). The retail ELF has no English text for
// them; the unused Japanese pause menu func_0026B018 has items 3 and 4's: "meetaa shoukyo" / "meetaa hyouji" (hide /
// show the meter) and "mantan" (a full tank). The PC's names for 5 and 6 are its own.
enum Item { kItemContinue, kItemRetire, kItemRanking, kItemHideMeter, kItemFullTank, kItemAiDrive, kItemZoneTable };

}  // namespace

void PauseMenu::Open(bool ranking, int pad, bool debug, bool meterHidden) {
    window_ = Window();
    window_.pad = (unsigned char)pad;
    int n = 0;
    window_.AddLine(GT_002EA1C8_8, kLineSelectable);
    items_[n++] = kItemContinue;
    window_.AddLine(GT_00335608_6, kLineSelectable);
    items_[n++] = kItemRetire;
    if (ranking) {
        window_.AddLine(GT_00335610_7, kLineSelectable);
        items_[n++] = kItemRanking;
    }
    meterLine_ = -1;
    if (debug) {
        meterLine_ = n;
        window_.AddLine(meterHidden ? "Show Meter" : "Hide Meter", kLineSelectable);
        items_[n++] = kItemHideMeter;
        window_.AddLine("Full Tank", kLineSelectable);
        items_[n++] = kItemFullTank;
        window_.AddLine("AI Drive", kLineSelectable);
        items_[n++] = kItemAiDrive;
        window_.AddLine("Zone Table", kLineSelectable);
        items_[n++] = kItemZoneTable;
    }
    window_.startConfirms = true;
    ranking_ = false;
    scroll_ = 0;
    frames_ = 0;
}

PauseMenu::Choice PauseMenu::Update(gfx::Draw2D& d, const platform::Input& input, const RankingRow* rows,
                                    int count) {
    if (ranking_) {
        // The window waits (undrawn) while the Ranking is up, and comes back the frame after it closes.
        if (!Race_RankingTask(d, input, rows, count)) ranking_ = false;
        return kNone;
    }
    int r = window_.Update(d, input);
    if (r == 0) return kNone;
    if (r == -1) window_.sel = 0;
    int item = items_[window_.sel];
    switch (item) {
    case kItemContinue:
        return kContinue;
    case kItemRetire:
        return kRetire;
    case kItemRanking:
        ranking_ = true;
        scroll_ = 0;
        frames_ = 0;
        // The Ranking task starts at once, in the same frame (its first frame can't close it).
        Race_RankingTask(d, input, rows, count);
        break;
    case kItemHideMeter:
        return kHideMeter;
    case kItemFullTank:
        // Once a pause: the line greys out.
        if (window_.LineFlags(window_.sel) & kLineDisabled) break;
        window_.SetLineFlags(window_.sel, kLineSelectable | kLineDisabled);
        return kFullTank;
    case kItemAiDrive:
        return kAiDrive;
    case kItemZoneTable:
        return kZoneTable;
    }
    return kNone;
}

void PauseMenu::SetMeterHidden(bool hidden) {
    if (meterLine_ >= 0) window_.SetLineText(meterLine_, hidden ? "Show Meter" : "Hide Meter");
}

bool PauseMenu::Race_RankingTask(gfx::Draw2D& d, const platform::Input& input, const RankingRow* rows,
                                 int count) {
    for (int i = 0; i < count; ++i) {
        const RankingRow& row = rows[i];
        int place = row.place >= 0 && row.place < 24 ? row.place : 23;
        // The original takes y in half-lines: (place x 24 - scroll + 128) / 2.
        int y = (place * 24 - scroll_ + 0x80) / 2 * 2;
        Text_PrintAt(d, 0x78, y, Text_PlaceName(place), 0);
        Text_PrintAt(d, 0xB4, y, row.name, 0);
        Text_PrintAt(d, 0x17C, y, row.town, 0);
    }
    uint32_t held = input.Held();
    if (scroll_ < count * 24 - 0x120 && (held & platform::kDown)) scroll_ += 2;
    if (scroll_ != 0 && (held & platform::kUp)) scroll_ -= 2;
    bool open = !(frames_ != 0 && (input.Pressed() & (platform::kCross | platform::kTriangle)));
    ++frames_;
    return open;
}

}  // namespace game
