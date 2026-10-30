#include "race_list.h"
#include "../../game_text.inc"   // the game's text, GT_<address>_<length>, made from the ELF

namespace game {

namespace {

#include "adventure_races.inc"

// D_002BE000: (first, count) by town.
const unsigned char kTownRaces[10][2] = {{0, 0}, {0, 3}, {3, 3}, {6, 4}, {10, 4},
                                         {14, 4}, {18, 2}, {20, 4}, {25, 1}, {24, 1}};

const char* const kCourseNames[0x27] = {
    GT_003286A0_13, GT_00328688_16, GT_00328678_14, GT_00328660_20, GT_00328650_14,
    GT_00328638_18, GT_00328620_21, GT_00328608_18, GT_003285F8_11, GT_003285E8_12,
    GT_003285D8_13, GT_00328620_21, GT_003285C0_16, GT_003285A8_19, GT_00328598_14,
    GT_00328588_11, GT_00328588_11, GT_00328578_13, GT_00328560_21, GT_00328548_17, GT_00328538_12,
    GT_00328528_13, GT_00328518_9, GT_00334558_4, GT_00328508_8, GT_003284F8_8, GT_00334550_6, GT_003284E8_12, GT_003284D8_11,
    GT_003284C8_15, GT_003284B8_15, GT_003284A8_14, GT_00334548_7, GT_00328498_14, GT_00328488_10,
    GT_00328478_11, GT_00334540_7, GT_00328468_10, GT_00328458_15,
};

}  // namespace

const AdventureRace& AdventureRace_Get(int race) {
    return kRaces[race >= 0 && race < kAdventureRaces ? race : 0];
}

void AdventureRace_Town(int town, int* first, int* count) {
    bool ok = town >= 0 && town < 10;
    *first = ok ? kTownRaces[town][0] : 0;
    *count = ok ? kTownRaces[town][1] : 0;
}

const char* AdventureRace_RankName(int rank) {
    static const char* const kRanks[5] = {GT_003338C8_7, GT_003338C0_7, GT_003338B8_7, GT_002EC520_9, ""};
    return rank >= 0 && rank < 5 ? kRanks[rank] : "";
}

int Race_Outcome(int set) {
    static int outcome = 0;
    if (set != -1) outcome = set;
    return outcome;
}

const char* Race_CourseName(int id) { return id >= 0 && id < 0x27 ? kCourseNames[id] : ""; }

}  // namespace game
