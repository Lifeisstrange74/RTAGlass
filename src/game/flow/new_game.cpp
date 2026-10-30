#include "new_game.h"

#include "../../audio/sound.h"
#include "../save/save_data.h"
#include "../story/race_list.h"
#include "loading.h"

namespace game {

namespace {

// The time of day the Adventure starts and goes on at (NewGame_Setup's and NewGame_Continue's 0x13C68, 9:00), or
// --clock's.
const int32_t kStartClock = 0x13C68;
int32_t g_startClock = -1;
int32_t StartClock() { return g_startClock >= 0 ? g_startClock : kStartClock; }
int g_ending = -1;   // --ending
const int kCloudHill = 8;   // the town in the clouds, a field of its own
const int kFirstGrandPrix = 0x1C, kLastGrandPrix = 0x22;   // the World Grand Prix's seven stages
// How long a loading screen's frame builds the field's blocks (a frame being 16.7 ms).
const double kLoadingStreamMs = 8.0;

}  // namespace

NewGame::NewGame(gfx::Draw2D& draw, const std::string& dataDir)
    : d_(draw), entry_(draw), scene_(draw, dataDir), garage_(draw, dataDir), step_(kName), frame_(0), town_(1), place_(0), you_(),
      race_(draw, dataDir), results_(draw), gpResults_(draw), raceId_(0), field_(draw, dataDir), ending_(draw), fieldTown_(1), fieldPlace_(0), clock_(kStartClock), raceClock_(kStartClock),
      test_(draw, dataDir) {}

// NewGame_Setup (0x0026D438): a new save for both players (func_00229FD8) and player 1's first car, one of six
// bodies (func_00229F70), picked by info+0 (the frames since boot) modulo 6 (0x0026D460).
void NewGame::NewGame_Setup() {
    static const int kStartingBodies[6] = {0x2B, 0x00, 0x75, 0x8C, 0x72, 0x5D};
    Save_InitBoth();
    Save_SetStartingCar(kStartingBodies[Info_Frames() % 6]);
    Save_SetCurrent(0);
    clock_ = StartClock();
}

void NewGame::Start() {
    NewGame_Setup();
    step_ = kName;
    entry_.Start(NameEntry::kName);
}

void NewGame::StartFactory(int race, int fieldTown, int roomTown, int roomPlace) {
    NewGame_Setup();
    step_ = kLoading;
    frame_ = 0;
    town_ = 1;
    place_ = 0;
    if (g_ending >= 0) {
        // Into the room the ending starts from; kLoading goes on to it.
        town_ = g_ending ? 9 : 8;
        place_ = g_ending ? 8 : 9;
        return;
    }
    if (roomTown >= 0 && ((roomTown == 0 && roomPlace == 0) || RoomScene::Room_Open(roomTown, roomPlace))) {
        town_ = roomTown;
        place_ = roomPlace;
        return;
    }
    if (fieldTown >= 1 && fieldTown <= 9) {
        town_ = fieldTown;
        Save_Current().townsUnvisited &= (uint16_t)~(1u << town_);   // as its factory would (Place_MarkFound)
        EnterField(town_, 0);
        return;
    }
    if (race < 0 || race >= kAdventureRaces) return;
    for (int t = 1; t < 10; ++t) {
        int first, count;
        AdventureRace_Town(t, &first, &count);
        if (race >= first && race < first + count) town_ = t;
    }
    raceId_ = race;
    step_ = kRaceLoading;
    // As #15 does when a race is picked: event flag 3, so Q's talk after it goes by how it went (#16).
    Save_Current().eventFlags[0] |= 1ull << 3;
}

// NewGame_Continue (0x0026D370): the loader's jobs (1, the meter's 0xA0000 + parts[14], 0x70003), D_0177EA50 the
// save's three cars, the clock at 0x13C68 and Room_Start (func_00211808) on the
// save's town (+0x12DC), place 0: its Q's Factory, the only place the game saves. A town the PC can't open (a save
// edited by hand) starts at Peach Town's.
void NewGame::Continue() {
    Save_SetCurrent(0);
    town_ = Save_Current().saveTown;
    if (town_ < 1 || town_ > 9) town_ = 1;
    place_ = 0;
    step_ = kLoading;
    frame_ = 0;
    clock_ = StartClock();
}

// Debug_TestMenuTask's first frame (test_menu.h): the clock (info+0x14) at 0x13C68, then the menu.
void NewGame::StartTestMenu() {
    test_.Start();
    clock_ = StartClock();
    testing_ = shopping_ = solo_ = false;
    step_ = kTestMenu;
}

bool NewGame::TestMenu_Update(const platform::Input& input) {
    const TestMenu::Action action = test_.Update(input);
    switch (action) {
    case TestMenu::kRunning: break;
    case TestMenu::kTitle: return true;
    case TestMenu::kAdventure:
        // Go Town: func_00211808, Room_Start on (info+0x23, info+0x21), and the task becomes Adventure_Task (state
        // 4): the Adventure from that building (or, without its talk, from its door), not back to the menu.
        testing_ = false;
        if (!OpenBuilding(test_.Town(), test_.Place())) EnterField(test_.Town(), test_.Place());
        break;
    case TestMenu::kShop:
        // To Shop: Room_Start (func_00211708) as Debug_PlaceListTask's child, so the scene's end comes back to it.
        testing_ = shopping_ = true;
        if (!OpenBuilding(test_.Town(), test_.Place())) TestMenu_Back();
        break;
    case TestMenu::kRace:
        testing_ = true;
        raceId_ = test_.Race();
        solo_ = test_.Solo();
        step_ = kRaceLoading;
        frame_ = 0;
        break;
    case TestMenu::kEndingScene:
        testing_ = true;
        field_.Ending_Start(test_.Scene());
        sceneShown_ = -1;
        step_ = kTestScene;
        break;
    case TestMenu::kEndingFull:
    case TestMenu::kTheater:
        testing_ = true;
        ending_.Start(field_, action == TestMenu::kEndingFull);
        step_ = kEnding;
        break;
    }
    return false;
}

void NewGame::TestMenu_Back(bool up) {
    testing_ = shopping_ = solo_ = false;
    test_.Resume(up);
    step_ = kTestMenu;
}

void NewGame::SetClock(int32_t clock) { g_startClock = clock; }

void NewGame::SetEnding(int ending) { g_ending = ending; }

bool NewGame::OpenBuilding(int town, int place) {
    if (!(town == 0 && place == 0) && !RoomScene::Room_Open(town, place)) return false;
    town_ = town;
    place_ = place;
    step_ = kLoading;
    frame_ = 0;
    return true;
}

void NewGame::EnterField(int town, int place) {
    fieldTown_ = town;
    fieldPlace_ = place;
    frame_ = 0;
    step_ = kFieldLoading;
}

bool NewGame::Update(const platform::Input& input) {
    if (step_ == kTestMenu) return TestMenu_Update(input);
    if (step_ == kTestScene || (step_ == kEnding && testing_ && (input.Pressed(0) & platform::kStart))) {
        // Debug_EndingMenuTask's Start, whatever runs: the children gone, Sound_MuteCars and Sound_MuteSong (the song
        // plays on silent), the window again.
        if (input.Pressed(0) & platform::kStart) {
            audio::Sound_EngineMute(0);
            audio::Sound_EngineMute(1);
            audio::Sound_StopEffects();
            audio::Sound_MuteSong();
            field_.Leave();
            TestMenu_Back();
            // (A black frame: the window takes the next one, so that this press doesn't choose in it too.)
            d_.BeginFrame(0x80000000);
            d_.EndFrame(0);
            return false;
        }
        // Ending_FieldScene (0x0026CA48) with the scene: the ending's field, once loaded released with Fade_InTask
        // (0x40 frames) and the clock D_002DCD28[scene]; it runs until the menu ends it.
        d_.BeginFrame(0x80000000);
        if (field_.Ending_Update() && sceneShown_ < 0) sceneShown_ = 0;
        if (sceneShown_ >= 0) {
            int a = 0x80 - 2 * (sceneShown_ < 0x40 ? sceneShown_ : 0x40);
            if (a > 0) d_.Fill(0, 0, gfx::Draw2D::kWidth, gfx::Draw2D::kHeight, (uint32_t)a << 24);
            ++sceneShown_;
        }
        d_.EndFrame(Race::kFrameBlend);
        return false;
    }
    if (step_ == kOpening) {
        // The scene draws its own frames. NewGame_AfterOpening (0x0026D678): D_0177EA50 becomes the save's three cars,
        // the loader runs (job 0x70001) and Room_Start (func_00211808) opens town 1, place 0: Q's Factory.
        if (scene_.Update(input)) {
            step_ = kLoading;
            frame_ = 0;
            town_ = 1;
            place_ = 0;
        }
        return false;
    }
    if (step_ == kLoading) {
        // The loading screen holds the office's last picture for 64 frames (as the other loads do), then the building
        // (Room_Start: town_, place_).
        Loading_Draw(d_, frame_);
        if (++frame_ >= 0x40 + 16) {
            // Place_MarkFound (0x0023C7B0) for place 0: the town visited (save +0x12DA), so Warp lists it. Shop_SceneTask
            // calls it as the scene ends; the PC marks the town as its factory opens too, so a game quit from the
            // factory's menu has it.
            if (place_ == 0) Save_Current().townsUnvisited &= (uint16_t)~(1u << town_);
            if (town_ == 0) {
                // Adventure_Task's town 0: the loader's GARAGE.BIN chunk and func_0022D1E0.
                garage_.Start();
                step_ = kGarage;
                return false;
            }
            const SaveCar& c = Save_Current().cars[0];
            you_.paint = c.paint & 0xFFFFFF;
            you_.body = c.body;
            you_.wheelColor = c.wheelColor;
            you_.wheel = c.wheel;
            you_.billboard = c.billboard;
            you_.name = Save_Current().name;
            scene_.Start(town_, place_, &you_, c.partFlags);
            scene_.SetClock(clock_);
            step_ = kFactory;
            if (g_ending >= 0) {
                ending_.Start(field_, town_ == 8);
                g_ending = -1;
                step_ = kEnding;
            }
        }
        return false;
    }
    if (step_ == kEnding) {
        // Shop_SceneTask's state 3: Adventure_Task stays in state 4, its clock running under the ending. Then
        // Room_AfterEnding: the cinema's talk again, or (the first time) result 5 as from any building, the stamp
        // book with stamp 100 and the field at the Secretary's door at 6:00 (no drive-out).
        if (clock_++ > kClockHour) clock_ -= kClockHour;
        if (ending_.Update()) {
            field_.Leave();
            if (testing_) {
                // Debug_EndingMenuTask's child (or To Shop's scene's) is over: the window again.
                TestMenu_Back();
                return false;
            }
            if (scene_.Room_AfterEnding()) {
                step_ = kFactory;
            } else {
                scene_.ExitPlace(&town_, &place_);
                int clock = scene_.TakeClock();
                if (clock >= 0) clock_ = clock;
                Town_FoundStamp(town_);
                Rally_SetRunning(Save_EventFlag(0x50));
                if (stamps_.Start(false)) step_ = kStamps;
                else EnterField(town_, place_);
            }
        }
        return false;
    }
    if (step_ == kFactory) {
        // Adventure_Task's state 4 runs the clock under the scene, a frame at a time round the hour.
        if (clock_++ > kClockHour) clock_ -= kClockHour;
        scene_.SetClock(clock_);
        int r = scene_.Update(input);
        if (r && shopping_) {
            // To Shop: Debug_PlaceListTask takes the scene's result. Quit (6) ends the list, back to the towns; a race
            // (4) opens the scene again (the race isn't run); another place (11: info+0x23 and +0x21 the talk's code
            // 0x14 set) opens that one, its town's list made again; the drive out (5) shows the stamps it queued
            // (Stamp_ShowTask with 0). The PC then comes back to the list (what the PS2 does after the stamps isn't
            // known: a GUESS); the ending (15), which Debug_PlaceListTask doesn't take, runs and comes back too.
            if (r == RoomScene::kQuit) {
                TestMenu_Back(true);
            } else if (r == RoomScene::kRace) {
                OpenBuilding(town_, place_);
            } else if (r == RoomScene::kGoTo) {
                int town, place;
                scene_.Destination(&town, &place);
                test_.SetPlace(town, place);
                if (!OpenBuilding(town, place)) TestMenu_Back();
            } else if (r == RoomScene::kEnding) {
                ending_.Start(field_, town_ == 8);
                step_ = kEnding;
            } else if (stamps_.Start(false)) {
                step_ = kStamps;
            } else {
                TestMenu_Back();
            }
            return false;
        }
        if (r) {
            if (r == RoomScene::kQuit) return true;
            if (r == RoomScene::kEnding) {
                // Code 0x18: Ending_Task in Cloud Hill, elsewhere (My City Cinemas) the replay.
                ending_.Start(field_, town_ == 8);
                step_ = kEnding;
                return false;
            }
            if (r == RoomScene::kRace) {
                // func_002105B8 state 4, result 4: Race_Setup with the race (info+0x20), behind the loading screen.
                raceId_ = scene_.Race();
                step_ = kRaceLoading;
                frame_ = 0;
                return false;
            }
            // Driving out (result 10, then the scene's end): func_00210158 loads the field around the town,
            // func_00210430 starts its tasks, and the car comes out of the factory's door.
            if (r == RoomScene::kDriveOut) {
                // Result 5: func_0023D878 (every place of the town found: its stamp), then func_0026D008's book
                // for what the scene queued (state 7 waits for it), then the field.
                // With Q's Rally on (event flag 0x50) the field starts with race flag 0x1000: the rally's timer runs.
                // The car comes out at the building's door, or where a pre-code 0x14 sent it (info's town and place).
                scene_.ExitPlace(&town_, &place_);
                int clock = scene_.TakeClock();
                if (clock >= 0) clock_ = clock;
                Town_FoundStamp(town_);
                Rally_SetRunning(Save_EventFlag(0x50));
                if (stamps_.Start(false)) step_ = kStamps;
                else EnterField(town_, place_);
            } else if (r == RoomScene::kGoTo) {
                // A talk's code 0x14 sends the car to another place: that building (or, without its talk, its
                // door: only with the message data missing).
                int town, place;
                scene_.Destination(&town, &place);
                if (!OpenBuilding(town, place)) EnterField(town, place);
            }
        }
        return false;
    }
    if (step_ == kFieldLoading) {
        // Adventure_Task state 6: the loads behind the loading screen, then the field fades in. The PC reads the
        // blocks on its loading threads meanwhile and builds a few milliseconds of them a frame.
        if (frame_ == 0) field_.Preload(fieldTown_, fieldPlace_);
        Loading_Draw(d_, frame_);
        field_.Stream(kLoadingStreamMs);
        if (++frame_ >= 0x40 + 16) {
            field_.Start(fieldTown_, fieldPlace_, clock_);
            step_ = kField;
        }
        return false;
    }
    if (step_ == kField) {
        d_.BeginFrame(0x80000000);
        Field::Result r = field_.Update(input);
        d_.EndFrame(Race::kFrameBlend);
        clock_ = field_.Clock();
        if (r != Field::kRunning) field_.Leave();
        if (r == Field::kEnter) {
            Rally_SetRunning(false);   // the door sets info+0x28 to 0x400000: the rally's timer stops inside
            // State 0's result 1: the building of (info+0x23, info+0x21). (Every door has its talk; without the
            // message data the car is put back out of the door.)
            if (!OpenBuilding(field_.Town(), field_.Place())) EnterField(field_.Town(), field_.Place());
        } else if (r == Field::kWarp) {
            // Adventure_Task's state 2 with a town: place 0 (its Q's Factory, or My Garage), the clock 1-4 hours on
            // by the menu's frames, and state 3: the loader and the building.
            // Warping ends Q's Rally (event flag 0x50 and race flag 0x1000 cleared), with no message.
            Rally_SetRunning(false);
            Save_SetEventFlag(0x50, false);
            clock_ += 9000 * ((field_.WarpFrames() & 3) + 1);
            if (clock_ > kClockHour) clock_ -= kClockHour;
            OpenBuilding(field_.WarpTown(), 0);
        }
        return false;
    }
    if (step_ == kGarage) {
        int r = garage_.Update(input);
        if (r && shopping_) {
            // To Shop's My Garage: its end (result 5 with town 0) ends Debug_PlaceListTask, back to the towns.
            TestMenu_Back(true);
            return false;
        }
        if (r == GarageScene::kDriveOut) {
            // Go outside: result 5 as from any building, the stamp book (stamp 40 the first time), the field at the
            // garage's door.
            town_ = place_ = 0;
            Town_FoundStamp(0);
            Rally_SetRunning(Save_EventFlag(0x50));
            if (stamps_.Start(false)) step_ = kStamps;
            else EnterField(0, 0);
        } else if (r == GarageScene::kGoTo) {
            // Go play: that game's building, as a door would.
            int town, place;
            garage_.Destination(&town, &place);
            if (!OpenBuilding(town, place)) OpenBuilding(0, 0);
        }
        return false;
    }
    if (step_ == kStamps) {
        // The book; its last 129 frames darken the held picture.
        bool done;
        if (stamps_.Holding()) {
            d_.BeginFrameFromLast(0.0f);
            d_.Fill(0, 0, gfx::Draw2D::kWidth, gfx::Draw2D::kHeight, 0x06000000);
            done = stamps_.Update(d_, input);
            d_.EndFrame(0);
        } else {
            d_.BeginFrame(0x80000000);
            done = stamps_.Update(d_, input);
            d_.EndFrame(0);
        }
        if (done) {
            if (shopping_) TestMenu_Back();
            else EnterField(town_, place_);
        }
        return false;
    }
    if (step_ == kRaceLoading) {
        Loading_Draw(d_, frame_);
        if (++frame_ >= 0x40 + 16) {
            race_.StartAdventure(raceId_, testing_ && solo_);
            raceClock_ = clock_;
            step_ = kRace;
        }
        return false;
    }
    if (step_ == kRace || step_ == kResults) {
        // State 9: whatever the race's end (Retire, or the results' outcome), the loader and the factory again.
        d_.BeginFrame(step_ == kRace ? race_.ClearColor() : 0x80000000);
        bool back = false;
        if (step_ == kRace) {
            const Race::Result result = race_.Update(input);
            if (result != Race::kRunning) race_.Race_SaveDistance();
            switch (result) {
            case Race::kRunning: break;
            case Race::kRetired:
                Race_Outcome(0);
                if (raceId_ >= kFirstGrandPrix && raceId_ <= kLastGrandPrix) {
                    // A World Grand Prix stage retired or fallen out of: Gpx_RetireTeam, then its results all the
                    // same (the team scores 0).
                    gpResults_.Start(race_.RetireResult());
                    gp_ = true;
                    step_ = kResults;
                    break;
                }
                back = true;
                break;
            case Race::kOver:
                if (raceId_ >= kFirstEvent) {
                    // Race_SetupTask: an event has no results screen. The fade goes black and func_00238950 sets
                    // the Tunnel Race's outcome by the player's place, and the Single Lap Race's by both cars'
                    // heights too (3 when both stay under 280); the others keep the event task's.
                    int outcome = race_.Outcome().outcome;
                    const bool won = race_.Outcome().place == 0;
                    if (raceId_ == 0x26) outcome = won ? 2 : 1;
                    if (raceId_ == 0x28) {
                        const bool low0 = race_.CarHeight(0) < 280.0f, low1 = race_.CarHeight(1) < 280.0f;
                        if (low0 && low1) outcome = 3;
                        if (!low0 && !low1) outcome = won ? 2 : 1;
                    }
                    Race_Outcome(outcome);
                    // A game may send the player on to another place of the town (info+0x21): Barrel Dodging's
                    // goal is Grandpa Tal's house, Peach Town's place 13.
                    if (race_.Outcome().scenePlace >= 0) place_ = race_.Outcome().scenePlace;
                    back = true;
                    break;
                }
                if (raceId_ >= kFirstGrandPrix && raceId_ <= kLastGrandPrix) {
                    // Race_SetupTask on the World GPX rows: Results_GrandPrixTask, never Results_Task.
                    gpResults_.Start(race_.Outcome());
                    gp_ = true;
                    step_ = kResults;
                    break;
                }
                results_.Start(race_.Outcome(), raceId_);
                gp_ = false;
                step_ = kResults;
                if (!results_.HasScreen()) {
                    Race_Outcome(results_.Outcome());
                    back = true;
                }
                break;
            }
        } else if (gp_) {
            int r = gpResults_.Update(input);
            if (r != GrandPrixResults::kRunning) {
                Race_Outcome(r);
                gp_ = false;
                back = true;
            }
        } else if (int r = results_.Update(input)) {
            // Race_SetupTask keeps what the results tell it for the talk.
            Race_Outcome(r);
            back = true;
        }
        d_.EndFrame(step_ == kRace ? race_.FrameBlend() : Race::kFrameBlend);
        if (back && testing_) {
            // A race of the test menu's lists: Race_SetupTask's end, the list again.
            TestMenu_Back();
            return false;
        }
        if (back) {
            // State 9 also sets the clock to the course's end time, or an hour after the race began.
            int32_t end = Light_CourseEndClock(AdventureRace_Get(raceId_).course);
            clock_ = end ? end : raceClock_ + 9000;
            step_ = kLoading;
            frame_ = 0;
        }
        return false;
    }
    d_.BeginFrame(0x80000000);
    bool done = false;
    switch (step_) {
    case kName:
        if (entry_.Update(input)) {
            step_ = kCurrency;
            entry_.Start(NameEntry::kCurrency);
        }
        break;
    case kCurrency:
        if (entry_.Update(input)) {
            // NewGame_AfterCurrency: the opening scene in the president's office (town 8, place 9), where the
            // player's car (D_0177EA50[0]) is the Secretary's (group 8 record 8, seen in a PCSX2 save state).
            step_ = kOpening;
            scene_.Start(8, 9, Car_DriverRecord(8, 8));
        }
        break;
    }
    d_.EndFrame(0x20);
    return done;
}

}  // namespace game
