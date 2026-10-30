// The PC's Extras: switches for what the original game has but never lets the player reach (unused content), and
// for its leftover debug features. All are off by default, which is the original game. The Extras screen
// (ui/extras_screen.h) sets them, from the title's Options and the field menu's Settings; they're kept in
// extras.json next to the executable, apart from the saves, as controls.json is. See docs/extras.md.
#pragma once

#include <string>

namespace game {

enum Extra {
    // Unused content.
    kExtraLifeMeter,    // My City's Parts Shop stocks the Life Meter after the Which-way? Race (stamp 47)
    kExtraQWave,        // the radio's third station, Q-Wave (2CH)
    kExtraMail5,        // Wonder Realty's "Now Open!" mail when the Endurance Run opens
    kExtraTestCourse,   // COURSE/C16.BIN, a course in no table, in the Quick Race
    kExtraBody149,      // body 149 (CAR4/Q149.BIN), which nothing gives, in the Body Shops
    kExtraDemoSong,     // DEMO.TSQ, a song nothing plays, on the title's attract demo
    kExtraDuckQuiz,     // Cloud Hill's duck quiz line #20, behind flag 75, which nothing sets
    kExtraLanguage,     // the title Options' Language item (the US build leaves it out): its French and German title menus
    // Debug (docs/debug.md).
    kExtraPauseItems,   // the race pause menu's debug items: hide the map and meter, refuel, AI drives, zone table
    kExtraReadouts,     // the debug byte's readouts on the race screen
    kExtraFreeCamera,   // the debug byte's free camera on controller 2
    kExtraHeightKeys,   // the debug byte's ride height on the d-pad
    kExtraClockScrub,   // the debug byte's L1/L2 time of day on the Endurance Run (and in the field)
    kExtraTestMenu,     // the developers' test menu (func_0026B848) as a third line of the Adventure menu
    kExtraMapTuner,     // the debug byte's course map tuner (RaceMap_Draw) on controller 2
    // Cheats (the PC's own, docs/extras.md): the master switch has no row, it's "cheats" in extras.json, put in by
    // hand; while it's off the Cheats screen isn't offered and every cheat is off, whatever its own setting.
    kExtraCheats,
    kCheatUncappedSpeed,   // the players' cars rev on past the limiter in top gear, and the jet's push isn't held
    kCheatInfiniteFuel,    // the players' tanks stay full (the jet too)
    kCheatPowerBoost,      // the players' engines twice as strong
    kCheatSuperGrip,       // the players' tires grip twice as well
    kCheatReduceDrag,      // a quarter of the air's drag on the players' cars
    kCheatLowGravity,      // a third of the gravity for the players' cars
    kCheatSlowRivals,      // the AI's opponents aim for three quarters of their zone speeds
    kCheatMaxMoney,        // the money held at 999999 in the field
    kCheatAllParts,        // every shop stocks every part (and body) of its kind
    kExtraCount
};
const int kExtraFirstDebug = kExtraPauseItems;     // the Debug page's first
const int kExtraFirstCheat = kCheatUncappedSpeed;  // the Cheats screen's first

struct ExtrasSet {
    bool on[kExtraCount];
};

// The setting in use (a cheat only with the master switch on).
bool Extras_On(Extra e);
// extras.json's "cheats": the Cheats screen is offered.
bool Cheats_Enabled();
const ExtrasSet& Extras_Get();
// Sets it, and writes extras.json (if Extras_Load gave it a path). False if the write failed.
bool Extras_Apply(const ExtrasSet& s);
// Reads `path` (a missing file or entry is off) and keeps it for Extras_Apply; an empty path keeps everything off
// and never writes (the scripted checks).
void Extras_Load(const std::string& path);
// Turns one on for this run only (the --extra option): extras.json isn't written.
void Extras_Force(Extra e);
// extras.json's name for each, or -1 for one that isn't.
const char* Extra_Name(Extra e);
int Extra_FromName(const std::string& name);

}  // namespace game
