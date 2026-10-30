// What New Game does before the story starts: asks for the player's name, then the currency unit, then plays the
// opening scene (the president's office, town 8 place 9), then loads Q's Factory of town 1. A high-level
// reimplementation of NewGame_Task (0x0026D550, with 0x0026D5E0 and 0x0026D628), the task the root runs after
// NewGame_Setup (0x0026D438), and of NewGame_AfterOpening (0x0026D678). Continue (NewGame_Continue, 0x0026D370) goes
// straight to the loading screen and the Q's Factory of the town the save was made in. A race picked from the
// factory's race list is run as func_002105B8 runs it (state 4's result 4: Race_Setup with the race, then state 9:
// the factory again), with the Adventure's results. Driving out of the factory (result 10, then 5: func_00210158 and
// func_00210430) goes on to the field around the town (field.h), and driving into a door there (state 0's result 1)
// to that building's scene (story/room_scene.h; town 0's door, My Garage). Result 15 runs the ending.
#pragma once

#include "../../gfx/draw2d.h"
#include "../../platform/input.h"
#include "../field/field.h"
#include "../race/race.h"
#include "../results/adventure_results.h"
#include "../results/grand_prix_results.h"
#include "../story/ending.h"
#include "../story/garage_scene.h"
#include "../story/room_scene.h"
#include "../ui/window.h"
#include "name_entry.h"
#include "test_menu.h"

namespace game {

class NewGame {
public:
    NewGame(gfx::Draw2D& draw, const std::string& dataDir);

    void Start();
    // A debug start (--factory): a new save, then the loading screen before Q's Factory, as after the opening.
    // `race` >= 0 (--advrace): straight into that Adventure race, then that race's town's Q's Factory. `fieldTown`
    // >= 0 (--field): straight out onto the field at that town's Q's Factory instead. `roomTown` >= 0 (--room):
    // into building `roomPlace` of that town instead (if the port opens it).
    void StartFactory(int race = -1, int fieldTown = -1, int roomTown = -1, int roomPlace = 0);
    // Development (--clock): the Adventure starts and goes on at this time of day instead of 9:00.
    static void SetClock(int32_t clock);
    // Development (--ending N): --factory goes straight to the ending, N 0 the whole one from the president's office
    // (town 8 place 9), 1 My City Cinemas' replay (town 9 place 8).
    static void SetEnding(int ending);
    // NewGame_Continue (0x0026D370): after a save is loaded into player 1's record.
    void Continue();
    // The root's result 2 (the Test Menu extra's line): Debug_TestMenuTask (0x0026B848, test_menu.h) over the
    // complete record. Its items run here: Go Town's Adventure (for good), To Shop's scenes, the races and the
    // endings, each back to the test menu when over; Go Back Title ends the flow.
    void StartTestMenu();
    // One frame, drawn on black. Returns true when the flow is over (Quit game: back to the title).
    bool Update(const platform::Input& input);

private:
    void NewGame_Setup();   // 0x0026D438
    enum Step {
        kName, kCurrency, kOpening, kLoading, kFactory, kRaceLoading, kRace, kResults, kFieldLoading, kField,
        kStamps, kGarage, kEnding, kTestMenu, kTestScene
    };
    // The loading screen, then the building `place` of `town` (My Garage for town 0), if the PC has its scene.
    bool OpenBuilding(int town, int place);
    // The loading screen, then the field from the door of `place` of `town`.
    void EnterField(int town, int place);
    // The test menu's frame and what it asks for; true for Go Back Title.
    bool TestMenu_Update(const platform::Input& input);
    // A test menu action is over: the field left, its list again (`up`: To Shop's quit, the towns).
    void TestMenu_Back(bool up = false);

    gfx::Draw2D& d_;
    NameEntry entry_;
    RoomScene scene_;
    GarageScene garage_;
    Step step_;
    int frame_;          // the loading screen's frames
    int town_;           // the town whose building the loading screen opens
    int place_;          // and the building: 0 its Q's Factory (RoomScene::Room_Open says which the PC has)
    CarRecord you_;      // the player's car (the save's car 0) for the scenes
    Race race_;
    AdventureResults results_;
    GrandPrixResults gpResults_;   // Results_GrandPrixTask for the World Grand Prix's stages (races 28-34)
    bool gp_ = false;              // kResults is gpResults_'s
    int raceId_;        // the Adventure race picked in the race list (info+0x20)
    Field field_;
    int fieldTown_, fieldPlace_;   // the door the field starts from (info+0x23, info+0x21)
    int32_t clock_;     // the time of day (info+0x14), which the field and the rooms run
    int32_t raceClock_; // Adventure_Task +0x20: the clock as a race began
    Ending ending_;     // the scene's result 15: the ending (Shop_SceneTask's state 3)
    StampBook stamps_;  // Adventure_Task's result 5: the stamps a building earned, before the field (state 7)
    TestMenu test_;     // the developers' test menu
    bool testing_ = false;    // what runs is the test menu's (a race, a To Shop scene, an ending): back to it after
    bool shopping_ = false;   // ... To Shop's scene (Room_Start under Debug_PlaceListTask)
    bool solo_ = false;       // the race is Race Solo's
    int sceneShown_ = -1;     // kTestScene: frames since the ending scene was released (Fade_InTask), -1 loading
};

}  // namespace game
