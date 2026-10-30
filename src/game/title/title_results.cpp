#include "title_results.h"

#include <stdio.h>

#include "../../audio/sound.h"
#include "../save/save_data.h"
#include "../story/race_list.h"
#include "../text.h"
#include "../../game_text.inc"   // the game's text, GT_<address>_<length>, made from the ELF

namespace game {

namespace {

const char* const kResultsMenu[2] = {GT_003353D8_6, GT_003353D0_6};   // D_002DC918
// D_002DC920: the 2 Player course rows' courses (Endurance Run, Desert, Night Glow, River, Sunny Beach, Highway,
// Tunnel, Sliding Door, Obstacle Course, Soccer), for the Oppose panel.
const int kVersusRowCourse[10] = {17, 4, 5, 10, 13, 20, 28, 19, 29, 26};
// D_002DC930: the Course list; item 5 (the Tunnel Race) is row 6.
const int kRecordCourse[6] = {17, 4, 5, 10, 13, 28};
const int kRivals = 20;
const int kSeChoose = 0x1A, kSeBack = 0x1B, kSeBuzz = 0x1C;

const char* g_courses[6];
const char* g_rivalNames[kRivals];

// A top-3 entry: the time (bits 0-23, 0 empty) and whose (bits 24-31: -1 the record's player, else a rival).
int Entry_Time(uint32_t e) { return (int32_t)(e << 8) >> 8; }
const char* Entry_Name(uint32_t e) {
    const SaveRecord& r = Save_Record(0);
    int owner = (int8_t)(e >> 24);
    if (owner < 0) return r.name;
    return owner < kRivals ? r.rivals[owner].name : "";
}

}  // namespace

void VersusRecords::Start() {
    state_ = 0;
    cursor_ = 0;
    for (int i = 0; i < 6; ++i) g_courses[i] = Race_CourseName(kRecordCourse[i]);
}

void VersusRecords::Records_DrawOppose(int rival) {
    // The y's are the PS2's half-lines, doubled for Text_PrintAt.
    const SaveVersusRival& v = Save_Record(0).rivals[rival];
    Window_DrawFrame(d_, 0xF0, 0x16, 0x17C, 0xB0, 0);
    Text_PrintAt(d_, 0x168, 0x16 * 2, GT_003353F0_5, 0);
    Text_PrintAt(d_, 0x1B8, 0x16 * 2, GT_003353F8_4, 0);
    int wins = 0, plays = 0;
    char buf[16];
    for (int i = 0; i < 10; ++i) {
        int y = (29 * i / 2 + 0x21) * 2;
        Text_PrintAt(d_, 0x104, y, Race_CourseName(kVersusRowCourse[i]), 0);
        snprintf(buf, sizeof buf, "%3d:%3d", v.wins[i], v.races[i]);
        Text_PrintAt(d_, 0x258 - Text_Width(buf), y, buf, 0);
        wins += v.wins[i];
        plays += v.races[i];
    }
    Text_PrintAt(d_, 0x104, 0xB2 * 2, GT_00335338_5, 0);
    snprintf(buf, sizeof buf, "%3d:%3d", wins, plays);
    Text_PrintAt(d_, 0x258 - Text_Width(buf), 0xB2 * 2, buf, 0);
}

void VersusRecords::Records_DrawCourse(int row) {
    const SaveVersusCourse& c = Save_Record(0).versus[row];
    static const char* const kRanks[3] = {"1ST", "2ND", "3RD"};
    Window_DrawFrame(d_, 0x28, 0x6E, 0x230, 0x58, 0);
    Text_PrintAt(d_, 0x8C, 0x6E * 2, GT_0032F410_8, 0);
    Text_PrintAt(d_, 0x1A4, 0x6E * 2, GT_0032F420_9, 0);
    for (int side = 0; side < 2; ++side) {
        const int label = side ? 0x154 : 0x3C, x = side ? 0x1A4 : 0x8C;
        const uint32_t* top = side ? c.topTotals : c.topLaps;
        for (int k = 0; k < 3; ++k) {
            const int timeY = 0x79 + 0x16 * k, nameY = 0x84 + 0x16 * k;
            Text_PrintAt(d_, label, timeY * 2, kRanks[k], 0);
            Text_PrintAt(d_, label, nameY * 2, GT_00335418_4, 0);
            const uint32_t e = top[k];
            if (!(e & 0xFFFFFF)) continue;
            // (The PS2's BEST TIME column tells the player from a rival by the total's owner but takes a rival's name
            // by the lap's (asm 0x0026A800); the PC names the total's owner.)
            Text_PrintAt(d_, x, nameY * 2, Entry_Name(e), 0);
            Time_Print(d_, x, timeY * 2, Entry_Time(e), 0);
        }
    }
}

bool VersusRecords::Update(const platform::Input& input) {
    const uint32_t pressed = input.Pressed(0);
    const bool cross = (pressed & platform::kCross) != 0, triangle = (pressed & platform::kTriangle) != 0;
    if (state_ == 0) {
        // The top menu, centred, its cursor where it was left; with no rivals Oppose shows in the disabled colour.
        const SaveRecord& r = Save_Record(0);
        rivals_ = 0;
        while (rivals_ < kRivals && r.rivals[rivals_].name[0]) {
            g_rivalNames[rivals_] = r.rivals[rivals_].name;
            ++rivals_;
        }
        window_ = Window();
        window_.SetList(kResultsMenu, 2, 2);
        window_.pad = 0;
        window_.style = 0;
        window_.sel = (signed char)cursor_;
        if (!rivals_) window_.SetLineFlags(window_.FirstItemLine(), kLineSelectable | kLineDisabled);
        state_ = 1;
    } else if (state_ == 2 || state_ == 4) {
        window_ = Window();
        if (state_ == 2) {
            window_.SetList(g_courses, 6, 6);
            window_.x = 0x28;
        } else {
            window_.SetList(g_rivalNames, rivals_, rivals_ < 10 ? rivals_ : 10);
            window_.x = 0x14;
            window_.w = 0xC8;
        }
        window_.y = 0x16;
        window_.pad = 0;
        window_.style = 0;
        ++state_;
    }
    window_.Update(d_, input);
    switch (state_) {
    case 1:
        cursor_ = window_.sel;
        if (cross && cursor_ == 0) {
            if (rivals_ > 0) {
                audio::Sound_PlaySe(kSeChoose);
                state_ = 4;
            } else {
                audio::Sound_PlaySe(kSeBuzz);
            }
        } else if (cross) {
            audio::Sound_PlaySe(kSeChoose);
            state_ = 2;
        } else if (triangle) {
            audio::Sound_PlaySe(kSeBack);
            return false;
        }
        break;
    case 3:
    case 5: {
        int sel = window_.sel < 0 ? 0 : window_.sel;
        if (state_ == 3) Records_DrawCourse(sel < 5 ? sel : 6);
        else Records_DrawOppose(sel);
        if (triangle) {
            audio::Sound_PlaySe(kSeBack);
            state_ = 0;
        }
        break;
    }
    }
    return true;
}

}  // namespace game
