#include "versus_results.h"

#include <math.h>

#include "../save/save_data.h"
#include "../story/race_list.h"
#include "../text.h"
#include "../ui/window.h"
#include "../../game_text.inc"   // the game's text, GT_<address>_<length>, made from the ELF

namespace game {

namespace {

const char kScore[] = "SYS/SCORE.GSL";

const int kRetired = 0xFF;   // a place byte for a car out of the race

}  // namespace

void Results_DrawBackdrop(gfx::Draw2D& d) {
    // The checker: picture 1's 64 x 64 corner at (192, 192) in palette 6, a sprite of 320 x 228 texels repeating it
    // that the PS2 shows over the whole frame, so each tile is 128 x 125.75 pixels here (measured on PCSX2). Sampled
    // half a texel in, so the filtered edges don't pick up the picture beside it.
    const gfx::Texture& tile = d.Get(kScore, 1, 6);
    const float kTileW = 128.0f, kTileH = 448.0f * 64 / 228;
    const float x0 = floorf(d.Left() / kTileW) * kTileW;   // on to a wide screen's edges
    for (float y = 0; y < gfx::Draw2D::kHeight; y += kTileH)
        for (float x = x0; x < d.Right(); x += kTileW)
            d.Sprite(tile, x, y, kTileW, kTileH, 192.5f, 192.5f, 63, 63);
    // The banner, picture 0 palette 2: its left end (64 x 48), the middle's 8-texel column stretched over 280, the
    // right end (16 x 48).
    const gfx::Texture& banner = d.Get(kScore, 0, 2);
    d.Sprite(banner, 140, 40, 64, 48, 0, 208, 64, 48);
    d.Sprite(banner, 204, 40, 280, 48, 64.5f, 208, 7, 48);
    d.Sprite(banner, 484, 40, 16, 48, 72, 208, 16, 48);
}

int VersusResults::Place(int side, int member) const {
    int car = result_.versusCar[side][member];
    return car < 0 || result_.order[car] < 0 ? kRetired : result_.order[car];
}

void VersusResults::Start(const RaceResult& result, int row, int flags) {
    result_ = result;
    row_ = row;
    frame_ = 0;
    // TEAM BATTLE with both second teammates (+0x676, +0x3ABE): 18 minus the sum of player 1's team's places (from
    // 0), as a byte. Otherwise 21 to player 1 for finishing ahead, else 0.
    teams_ = (flags & 8) && Save_Record(0).teammates[1][0] && Save_Record(1).teammates[1][0];
    if (teams_)
        points_ = (uint8_t)(18 - Place(0, 0) - Place(0, 1) - Place(0, 2));
    else
        points_ = (uint8_t)Place(0, 0) < (uint8_t)Place(1, 0) ? 21 : 0;
}

void VersusResults::Versus_DrawRow(int member, int side) {
    int car = result_.versusCar[side][member];
    int place = Place(side, member);
    // The original takes y in half-lines: (member + 3 x side) x 12 + 0x41.
    int y = ((member + side * 3) * 12 + 0x41) * 2;
    Text_PrintAt(d_, 0x46, y, car >= 0 && result_.name[car] ? result_.name[car] : "", 0);
    if (place == kRetired) {
        Text_PrintAt(d_, 0x104, y, "retire", 0);
        return;
    }
    Text_PrintAt(d_, 0x118, y, Text_PlaceName(place), 0);
    Number_PrintRight(d_, 0x172, y, 6 - place, 0);
    Time_Print(d_, 0x186, y, result_.total[car], 0);
    Time_Print(d_, 0x1E0, y, result_.best[car], 0);
}

bool VersusResults::Update(const platform::Input& input, const int rival[2]) {
    Results_DrawBackdrop(d_);
    Text_PrintAt(d_, 0xE6, 0x1B * 2, Race_CourseName(result_.course), 0);
    Window_DrawCellFrame(d_, 2, 3, 0x1C, 0xB, 0);
    Text_Print(d_, 4, 4, GT_002EC430_10, 0);
    Text_Print(d_, 0xD, 4, GT_003337F8_4, 0);
    Text_Print(d_, 0x10, 4, GT_00333800_4, 0);
    Text_Print(d_, 0x14, 4, GT_00333808_4, 0);
    Text_Print(d_, 0x17, 4, GT_002EC440_8, 0);
    Versus_DrawRow(0, 0);
    Versus_DrawRow(0, 1);
    if (teams_) {
        Versus_DrawRow(1, 0);
        Versus_DrawRow(2, 0);
        Versus_DrawRow(1, 1);
        Versus_DrawRow(2, 1);
    }
    Window_DrawCellFrame(d_, 9, 0xB, 0x15, 0x10, 0);
    if (teams_) {
        Text_Print(d_, 0xB, 0xC, GT_002EC450_13, 0);
        Number_PrintRight(d_, 0x12C, 0xA1 * 2, points_, 0);
        Text_Print(d_, 0xF, 0xD, "vs", 0);
        Number_PrintRight(d_, 0x172, 0xA1 * 2, 21 - points_, 0);
    }
    // The winner flashes: palette 6 for 16 frames, then white for 16.
    Text_Print(d_, 0xB, 0xE, Outcome() == 2 ? GT_002EC460_13 : GT_002EC470_13, (frame_ & 0x10) ? 0 : 6);
    ++frame_;
    if (!(input.Pressed(platform::kEitherPad) & platform::kCross)) return false;
    // Results_UpdateRecords, race flag 0x400000, and Race_Outcome to the parent.
    Versus_SaveRecords(rival, Outcome());
    return true;
}

void VersusResults::Versus_SaveRecords(const int rival[2], int outcome) const {
    VersusRace race;
    for (int p = 0; p < 2; ++p) {
        int car = result_.versusCar[p][0];
        race.place[p] = Place(p, 0);
        race.total[p] = car >= 0 ? result_.total[car] : kNoRecordTime;
        race.bestLap[p] = car >= 0 ? result_.best[car] : kNoRecordTime;
    }
    Results_UpdateVersus(row_, race, outcome, rival);
}

}  // namespace game
