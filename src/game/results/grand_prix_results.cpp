#include "grand_prix_results.h"

#include "../../audio/sound.h"
#include "../save/save_data.h"
#include "../text.h"
#include "../ui/window.h"
#include "versus_results.h"
#include "../../game_text.inc"   // the game's text, GT_<address>_<length>, made from the ELF

namespace game {

namespace {

const int kTeams = 8, kStages = 7;
const int kMaxMoney = 999999;
const int kSeTick = 0x22, kSeCross = 0x1A;
// D_0029FCB8's class 3: the final's prize by the team's place.
const int kPrizes[6] = {80000, 60000, 40000, 30000, 20000, 10000};

int RowY(int row) { return (0x4D + 0xC * row) * 2; }   // half-line 0x4D + 0xC row, doubled

}  // namespace

int GrandPrixResults::Points(int stage, int car) const { return Save_Current().gpxPoints[stage][car]; }

void GrandPrixResults::Start(const RaceResult& result) {
    result_ = result;
    SaveRecord& r = Save_Current();
    stage_ = r.gpxStage < 0 ? 0 : r.gpxStage > kStages - 1 ? kStages - 1 : r.gpxStage;
    // Every car's points: 0x1E - place, an out car's (place 0xFF, 0x1F) stored as 0.
    for (int i = 0; i < 24; ++i) {
        int place = i < result_.cars ? result_.order[i] : -1;
        r.gpxPoints[stage_][i] = (int8_t)(place < 0 ? 0 : 0x1E - place);
    }
    state_ = 0;
    timer_ = 0;
    prize_ = 0;
}

void GrandPrixResults::Gpx_DrawStageOrder() {
    Window_DrawCellFrame(d_, 2, 3, 0x1C, 0x11, 0);
    Text_Print(d_, 3, 4, GT_00333840_5, 0);
    Text_Print(d_, 7, 4, GT_002EC430_10, 0);
    Text_Print(d_, 0x10, 4, GT_00333840_5, 0);
    Text_Print(d_, 0x14, 4, GT_002EC430_10, 0);
    // Two columns of twelve, a row every 4 frames, the right column from frame 0x30; the player's team blinks.
    for (int r = 0; r < 12; ++r) {
        int y = (0x41 + 0xC * r) * 2;
        for (int col = 0; col < 2; ++col) {
            int place = r + 12 * col;
            if (timer_ <= 4 * r + 0x30 * col) continue;
            int car = 0;
            while (car < result_.cars && result_.order[car] != place) ++car;
            if (car == result_.cars) continue;
            int pal = car < 3 && Blink() ? 6 : 0;
            Text_PrintAt(d_, col ? 0x154 : 0x50, y, Text_PlaceName(place), 0);
            Text_PrintAt(d_, col ? 0x19A : 0x96, y, result_.name[car] ? result_.name[car] : "", pal);
        }
    }
}

void GrandPrixResults::Gpx_DrawTeamStage() {
    Window_DrawCellFrame(d_, 2, 4, 0x1C, 0xE, 0);
    Text_Print(d_, 6, 5, GT_002EC4C0_9, 0);
    Text_Print(d_, 0x14, 5, state_ == 1 ? GT_003337F8_4 : GT_00333858_6, 0);
    static const int kPlaceX[3] = {0x15E, 0x19A, 0x1D6}, kPointsX[3] = {0x17C, 0x1B8, 0x1F4};
    for (int t = 0; t < kTeams; ++t) {
        int y = RowY(t), pal = t == 0 && Blink() ? 6 : 0;
        int x = Text_PrintAt(d_, 0x5A, y, GT_00333850_5, pal);
        const char* lead = result_.name[3 * t];
        Text_PrintAt(d_, x, y, lead ? lead : "", pal);
        int sum = 0;
        for (int m = 0; m < 3; ++m) {
            int car = 3 * t + m;
            if (state_ == 1) {
                if (car < result_.cars && result_.order[car] >= 0)
                    Text_PrintAt(d_, kPlaceX[m], y, Text_PlaceName(result_.order[car]), 0);
            } else {
                int p = Points(stage_, car);
                sum += p;
                Number_PrintSmallRight(d_, kPointsX[m], y, p, 0);
                if (m < 2) Text_PrintAt(d_, kPointsX[m] + 0xA, y, "+", 0);
            }
        }
        if (state_ == 2) Number_PrintSmallRight(d_, 0x230, y, sum, 0);
    }
}

int GrandPrixResults::Gpx_DrawStandings(const platform::Input& input) {
    Window_DrawCellFrame(d_, 2, 4, 0x1C, 0xE, 0);
    Text_Print(d_, 3, 5, GT_00333848_7, 0);
    Text_Print(d_, 0xA, 5, GT_002EC4C0_9, 0);
    Text_Print(d_, 0x15, 5, GT_002EC4D0_9, 0);
    // Each team's points over stages 0..+0x3310, sorted down (a[i] and a[j] swap while a[i] < a[j], so a tie keeps
    // the earlier team ahead: the player's wins it).
    struct Total {
        int total, team;
    } a[kTeams];
    for (int t = 0; t < kTeams; ++t) {
        a[t] = {0, t};
        for (int s = 0; s <= stage_; ++s)
            for (int m = 0; m < 3; ++m) a[t].total += Points(s, 3 * t + m);
    }
    for (int i = 0; i < kTeams; ++i)
        for (int j = i + 1; j < kTeams; ++j)
            if (a[i].total < a[j].total) {
                Total k = a[i];
                a[i] = a[j];
                a[j] = k;
            }
    int ours = 0;
    for (int i = 0; i < kTeams; ++i) {
        if (a[i].team == 0) ours = i;
        if (timer_ <= 8 * i) continue;
        int y = RowY(i), pal = a[i].team == 0 && Blink() ? 6 : 0;
        Text_PrintAt(d_, 0x50, y, Text_PlaceName(i), pal);
        int x = Text_PrintAt(d_, 0xAA, y, GT_00333850_5, pal);
        const char* lead = result_.name[3 * a[i].team];
        Text_PrintAt(d_, x, y, lead ? lead : "", pal);
        Number_PrintSmallRight(d_, 0x212, y, a[i].total, pal);
    }
    if (timer_ < 0x40) {
        if ((timer_ & 7) == 0) audio::Sound_PlaySe(kSeTick);
        return kRunning;
    }
    const bool final = stage_ == kStages - 1;
    if (final) {
        // The prize window (drawn whatever the place: a 7th or 8th team gets it empty) and the wallet.
        Window_DrawCellFrame(d_, 0xA, 0xE, 0x14, 0x11, 0);
        if (ours < 6) {
            prize_ = kPrizes[ours];
            Text_Print(d_, 0xB, 0xF, GT_002EC480_15, 0);
            Text_Print(d_, 0xB, 0x10, Text_PlaceName(ours), 0);
            Text_Print(d_, 0xD, 0x10, GT_00333838_6, 0);
            Number_PrintSmallRight(d_, 0x19A, 0x18A, prize_, 0);
            const SaveRecord& r = Save_Current();
            int after = r.money + prize_ > kMaxMoney ? kMaxMoney : r.money + prize_;
            Money_Draw(d_, 0x1B8, 0x152, r.money, r.currency);
            Money_Draw(d_, 0x1B8, 0x182, after, r.currency);
            Icon_Draw(d_, 0x204, ((timer_ >> 1) & 7) + 0x171, 7);
        }
    }
    if (!(input.Pressed(0) & platform::kCross)) return kRunning;
    // Cross: the stage done; after the final the prize paid, the stages back to 0 and won or not.
    SaveRecord& r = Save_Current();
    audio::Sound_PlaySe(kSeCross);
    ++r.gpxStage;
    if (r.gpxStage >= kStages) {
        Money_Pay(-prize_);
        r.gpxStage = 0;
        return a[0].team == 0 ? 7 : 6;
    }
    return result_.order[0] < 0 ? 0 : 3;
}

int GrandPrixResults::Update(const platform::Input& input) {
    Results_DrawBackdrop(d_);
    static const char* const kStageNames[kStages] = {"1", "2", "3", "4", "5", "6", "7"};
    Text_PrintAt(d_, 0xD2, 0x1B * 2, GT_002EC4E0_15, 0);
    Text_PrintAt(d_, 0x1C2, 0x1B * 2, kStageNames[stage_], 0);
    int out = kRunning;
    const bool cross = (input.Pressed(0) & platform::kCross) != 0;
    switch (state_) {
    case 0:
        Gpx_DrawStageOrder();
        if (timer_ < 0x60) {
            if ((timer_ & 3) == 0) audio::Sound_PlaySe(kSeTick);
        } else if (cross) {
            timer_ = -1;   // 0 after this frame's count
            state_ = 1;
            audio::Sound_PlaySe(kSeCross);
        }
        break;
    case 1:
    case 2:
        Gpx_DrawTeamStage();
        if (cross) {
            audio::Sound_PlaySe(kSeCross);
            if (state_ == 1) {
                state_ = 2;   // the timer runs on: the blink keeps its phase
            } else {
                state_ = 3;
                timer_ = -1;
            }
        }
        break;
    default:
        out = Gpx_DrawStandings(input);
        break;
    }
    ++timer_;
    return out;
}

}  // namespace game
