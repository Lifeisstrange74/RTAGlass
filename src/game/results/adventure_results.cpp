#include "adventure_results.h"

#include "../../audio/sound.h"
#include "../save/save_data.h"
#include "../story/race_list.h"
#include "../text.h"
#include "../ui/window.h"
#include "versus_results.h"
#include "../../game_text.inc"   // the game's text, GT_<address>_<length>, made from the ELF

namespace game {

namespace {

const int kOut = 0xFF;          // a place byte for a car out of the race
const int kPrizePlaces = 6;
const int kMaxMoney = 999999;
const int kSePrize = 0x305;

// D_0029FCB8: the prize by the race's prize class (its rank byte) and place.
const int kPrizes[5][kPrizePlaces] = {
    {800, 500, 400, 300, 200, 100},       {1500, 1200, 1000, 800, 600, 500},
    {2500, 2000, 1600, 1200, 1000, 800},  {80000, 60000, 40000, 30000, 20000, 10000},
    {10000, 7500, 5000, 3000, 2000, 1000},
};

int Prize(int race, int place) {
    int c = AdventureRace_Get(race).rank;
    return place < kPrizePlaces && c < 5 ? kPrizes[c][place] : 0;
}

// The colour of a time against its record: palette 6 when it beats it, on the blink's bright half; else white.
int RecordColor(int time, int record, bool blink) { return record != kNoRecordTime && time < record && blink ? 6 : 0; }

}  // namespace

int AdventureResults::Place(int member) const {
    int car = Car(member);
    return car < 0 || result_.order[car] < 0 ? kOut : result_.order[car];
}

void AdventureResults::Start(const RaceResult& result, int race) {
    result_ = result;
    race_ = race;
    frame_ = 0;
    // Results_PrizeTotal: the player's prize and each teammate's.
    prize_ = 0;
    for (int m = 0; m < 3; ++m)
        if (Car(m) >= 0 && Place(m) < kPrizePlaces) prize_ += Prize(race, Place(m));
    // The license: on a race of the player's class, the next one once every race of the class has been placed in
    // the first six (this one by this race, or before).
    license_ = 0;
    const SaveRecord& r = Save_Current();
    if (race < 24 && r.license != 3) {
        license_ = r.license + 1;
        for (int i = 0; i < 24; ++i) {
            if (AdventureRace_Get(i).rank != r.license) continue;
            if (i == race && Place(0) < kPrizePlaces) continue;
            if (r.adventurePlace[0][i] >= kPrizePlaces) {
                license_ = 0;
                break;
            }
        }
    }
}

void AdventureResults::Results_DrawRow(int member) {
    const SaveRecord& r = Save_Current();
    int place = Place(member), car = Car(member), rec = Record();
    bool blink = ((frame_ + 4 + 4 * member) & 0x10) != 0;
    // The original's y in half-lines: member x 0x18 + 0x41, its records' line 12 below.
    int y = (member * 0x18 + 0x41) * 2, y2 = (member * 0x18 + 0x4D) * 2;
    Text_PrintAt(d_, 0x46, y, result_.name[car] ? result_.name[car] : "", 0);
    Text_Print(d_, 8, member * 2 + 6, GT_00333818_7, 0);
    if (place != kOut)
        Text_PrintAt(d_, 0x118, y, Text_PlaceName(place), place >= 6 ? 0 : 6);
    else
        Text_PrintAt(d_, 0x104, y, "retire", 0);
    int best = r.adventurePlace[member][rec];
    Text_PrintAt(d_, 0x10E, y2, "(", 0);
    Text_PrintAt(d_, 0x118, y2, best != kNoRecordPlace ? Text_PlaceName(best) : " --", 5);
    Text_PrintAt(d_, 0x140, y2, ")", 0);
    int total = r.adventureTotal[member][rec], lap = r.adventureLap[member][rec];
    if (place != kOut) Time_Print(d_, 0x168, y, result_.total[car], RecordColor(result_.total[car], total, blink));
    Text_PrintAt(d_, 0x15E, y2, "(", 0);
    Time_Print(d_, 0x168, y2, total, 5);
    Text_PrintAt(d_, 0x1B8, y2, ")", 0);
    if (place != kOut) Time_Print(d_, 0x1E0, y, result_.best[car], RecordColor(result_.best[car], lap, blink));
    Text_PrintAt(d_, 0x1D6, y2, "(", 0);
    Time_Print(d_, 0x1E0, y2, lap, 5);
    Text_PrintAt(d_, 0x230, y2, ")", 0);
}

void AdventureResults::Results_DrawPrizes() {
    // Each place of the first six that one of the team took, a line each from row 0xD.
    int team = 1 + (Car(1) >= 0) + (Car(2) >= 0), row = 0xD;
    for (int place = 0; place < kPrizePlaces; ++place) {
        int m = 0;
        while (m < team && Place(m) != place) ++m;
        if (m == team) continue;
        Text_Print(d_, 0xB, row, Text_PlaceName(place), 0);
        Text_Print(d_, 0xD, row, GT_00333838_6, 0);
        Number_PrintSmallRight(d_, 0x19A, 0x12A + 0x18 * (row - 0xC), Prize(race_, place), 0);
        ++row;
    }
}

int AdventureResults::Update(const platform::Input& input) {
    Results_DrawBackdrop(d_);
    Text_PrintAt(d_, 0xE6, 0x1B * 2, Race_CourseName(result_.course), 0);
    Window_DrawCellFrame(d_, 2, 3, 0x1C, 0xB, 0);
    Text_Print(d_, 4, 4, GT_002EC430_10, 0);
    Text_Print(d_, 0xD, 4, GT_00333840_5, 0);
    Text_Print(d_, 0x13, 4, GT_00333808_4, 0);
    Text_Print(d_, 0x17, 4, GT_002EC440_8, 0);
    for (int m = 0; m < 3; ++m)
        if (Car(m) >= 0) Results_DrawRow(m);
    if (prize_) {
        Window_DrawCellFrame(d_, 0xA, 0xB, 0x14, 0x11, 0);
        Text_Print(d_, 0xB, 0xC, GT_002EC480_15, 0);
        Results_DrawPrizes();
        static const char* const kLicenses[4] = {"", GT_002EC490_15, GT_002EC4A0_15, GT_002EC4B0_15};
        if (license_) Text_Print(d_, 0xB, 0x10, kLicenses[license_], (frame_ & 0x10) ? 0 : 6);
    }
    // func_0022E7F0: the money now, and what it will be with the prize, the arrow bobbing between.
    SaveRecord& r = Save_Current();
    int after = r.money + prize_ > kMaxMoney ? kMaxMoney : r.money + prize_;
    Money_Draw(d_, 0x1B8, 0x152, r.money, r.currency);
    Money_Draw(d_, 0x1B8, 0x182, after, r.currency);
    Icon_Draw(d_, 0x204, ((frame_ >> 1) & 7) + 0x171, 7);
    if (frame_++ == 0 || !(input.Pressed(0) & platform::kCross)) return 0;
    // Cross: the prize into the wallet (func_0023DC58), the license, the records.
    r.money = after;
    if (prize_) audio::Sound_PlaySe(kSePrize);
    if (license_) r.license = (int8_t)license_;
    Results_UpdateRecords();
    int place = Place(0);
    // The Endurance Run (race 0x18), whatever the place: stamp 49 and My City's resident 5 (Quick-Pic Shop No.29).
    if (race_ == 0x18) {
        Stamp_Queue(49);
        Resident_MoveIn(5);
    }
    // In the first six with a garage (stamp 40): the fan club's stamp 95 and its mail the first time, then a fan's
    // mail. (The seed is info+0's low word, the frames since boot: 0x00236778.)
    if (place < kPrizePlaces && Stamp_Earned(40)) {
        if (Stamp_Queue(95)) Mail_Send(0x1E);
        else Mail_SendRandom(0x1F, 0x31, (int32_t)Info_Frames());
    }
    if (license_) return 5;
    if (place == 0) return 4;
    if (place < kPrizePlaces) return 3;
    return prize_ ? 2 : 1;
}

void AdventureResults::Results_UpdateRecords() {
    SaveRecord& r = Save_Current();
    int rec = Record();
    for (int m = 0; m < 3; ++m) {
        if (m > 0 && !r.teammates[m - 1][0]) break;
        int car = Car(m);
        if (car < 0) break;
        int place = Place(m), total = result_.total[car], lap = result_.best[car];
        uint8_t& bestPlace = r.adventurePlace[m][rec];
        int32_t& bestTotal = r.adventureTotal[m][rec];
        int32_t& bestLap = r.adventureLap[m][rec];
        // A teammate's records were set by whoever was the teammate then (+0x33C0): another one's race replaces them
        // all when it's better at any of them.
        if (m > 0) {
            uint8_t* who = r.adventureTeammates[rec][m - 1];
            if ((who[0] != (uint8_t)r.teammates[m - 1][0] || who[1] != (uint8_t)r.teammates[m - 1][1]) &&
                (place < bestPlace || (uint32_t)total < (uint32_t)bestTotal || (uint32_t)lap < (uint32_t)bestLap)) {
                who[0] = (uint8_t)r.teammates[m - 1][0];
                who[1] = (uint8_t)r.teammates[m - 1][1];
                bestLap = lap;
                bestTotal = total;
                bestPlace = (uint8_t)place;
            }
        }
        if (place < bestPlace) bestPlace = (uint8_t)place;
        if ((uint32_t)total < (uint32_t)bestTotal) bestTotal = total;
        if ((uint32_t)lap < (uint32_t)bestLap) bestLap = lap;
    }
    // func_0023DCF8: every car placed behind the player, beaten (group 13's word is the tenth).
    int mine = Place(0);
    for (int i = 0; i < result_.cars; ++i) {
        if (i == Car(0)) continue;
        int theirs = result_.order[i] < 0 ? kOut : result_.order[i];
        if (mine >= theirs || result_.group[i] <= 0) continue;
        int g = result_.group[i] == 13 ? 9 : result_.group[i] - 1;
        if (g < 10) r.beaten[g] |= 1ull << result_.index[i];
    }
}

}  // namespace game
