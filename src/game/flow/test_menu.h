// The developers' test menu, left in the game but out of reach: the root task (func_00205088) starts it on the
// Adventure menu's result 2, a line the retail window doesn't have. The Test Menu extra adds the line (title_menu.h).
// A high-level reimplementation of Debug_TestMenuTask (0x0026B848) and its children: the town and place lists
// (Debug_TownListTask 0x0026C460, Debug_PlaceListTask 0x0026BE50), the race lists (Debug_RaceListTask 0x0026B1B8,
// Debug_GrandPrixListTask 0x0026B470, Debug_ActionListTask 0x0026B5C8) and the ending menu (Debug_EndingMenuTask
// 0x0026CCD0), with the complete record it starts from (Save_InitComplete, 0x00227C68). The books and the name entry
// run here; the scenes, races and endings run in the New Game flow (new_game.h), which asks Update for an action and
// hands back with Resume. See docs/extras.md, "Test Menu".
#pragma once

#include <string>
#include <vector>

#include "../../gfx/draw2d.h"
#include "../../platform/input.h"
#include "../field/field_menu.h"
#include "../ui/window.h"
#include "name_entry.h"

namespace game {

// Save_InitComplete (func_00227C68): record 0 made "complete" over Save_InitBoth's new one: Alec in euro, the Super
// A license, 10000 money and 100000 in the bank, every town visited, every photo taken and stamp text shown, every
// opponent beaten, every body but 149 and the parts owned, both teammates with their cars, all 128 mails, event
// flags 1, 4 and 90.
void Save_InitComplete();

class TestMenu {
public:
    TestMenu(gfx::Draw2D& draw, const std::string& dataDir);

    // Debug_TestMenuTask's first frame: Save_InitBoth, the complete record (record 0 current, its three cars
    // D_0177EA50's), the PC's guard on it (Save_SetTestRecord), and the menu window.
    void Start();

    // What Update asks the New Game flow to run. kAdventure: the Adventure from building Place() of Town() (Go Town:
    // Room_Start then Adventure_Task, func_00211808), never back. kShop: that building's scene alone (To Shop:
    // Room_Start, func_00211708), then Resume. kRace: Adventure race row Race() (Race_Setup 0x0020F950), Solo()
    // for Race Solo, then Resume. kEndingScene: the ending's field scene Scene() (Ending_FieldScene 0x0026CA48)
    // until Start; kEndingFull (Ending_Task 0x0026CB60) and kTheater (Ending_ReplayTask 0x0026D2C0) until they end
    // or Start: then Resume. kTitle: Go Back Title.
    enum Action { kRunning, kTitle, kAdventure, kShop, kRace, kEndingScene, kEndingFull, kTheater };
    // One frame, BeginFrame to EndFrame (while kRunning).
    Action Update(const platform::Input& input);
    int Town() const { return town_; }
    int Place() const { return place_; }
    int Race() const { return race_; }
    bool Solo() const { return solo_; }
    int Scene() const { return scene_; }
    // The action is over: back to the list it came from. `up`: To Shop's scene quit (result 6), back to the towns.
    void Resume(bool up = false);
    // To Shop's scene sent the car to another building (a talk's code 0x14, result 11): its place list, for the
    // scene the flow opens there.
    void SetPlace(int town, int place);

private:
    enum State {
        kMain, kTowns, kPlaces, kRaceList, kGrandPrix, kActionList, kEndings, kStamps, kPictures, kResults,
        kNameEntry, kAway
    };

    void Debug_TestMenuTask(const platform::Input& input, Action* action);      // 0x0026B848
    void Debug_TownListTask(const platform::Input& input);                       // 0x0026C460
    void Debug_PlaceListTask(const platform::Input& input, Action* action);      // 0x0026BE50
    void OpenPlaces();                                                           // its first frame
    void Debug_RaceListTask(const platform::Input& input, Action* action);       // 0x0026B1B8
    void Debug_GrandPrixListTask(const platform::Input& input, Action* action);  // 0x0026B470
    void Debug_ActionListTask(const platform::Input& input, Action* action);     // 0x0026B5C8
    void Debug_EndingMenuTask(const platform::Input& input, Action* action);     // 0x0026CCD0

    gfx::Draw2D& d_;
    std::string dataDir_;
    FieldMenu books_;   // Stamp_BookTask, Pictures_Task, Results_BookTask
    NameEntry entry_;
    Window main_, towns_, places_, grandPrix_, endings_;
    State state_ = kMain;
    State away_ = kMain;   // the list an action came from
    int frame_ = 0;        // the child task's frames (+0x08)
    bool shop_ = false;    // the town list's argument (+0x1C): 1 To Shop, 0 Go Town
    bool solo_ = false;    // the race list's: Race Solo
    int cursor_ = 0;       // the race lists' cursor (+0x27)
    int town_ = 0, place_ = 0, race_ = 0, scene_ = 0;
    std::vector<std::string> townNames_, placeNames_;
    std::vector<const char*> townList_, placeList_;
};

}  // namespace game
