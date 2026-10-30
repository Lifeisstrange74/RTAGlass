// The race: the chosen course and its field on the grid, drawn as the race
// draws them, through the game's camera (focal length 500, the chase camera's place behind the player's car). Every
// car runs the game's physics (car_physics.h) on the course's collision data, and they collide with each other; the
// player's car drives from the pad, the others by the game's AI along the course's path (ai.h). The race opens with
// the game's countdown (the start lights, the clock and the cars held until green, then the course's song). The
// player's car sounds its engine, skids, horn, splashes and bumps, and the cars throw up smoke, dust and spray
// (car_effects.h). Laps are counted at the line as the race task does, and the HUD (game/hud) shows the times, the
// place, the map and the meter. Start pauses it (pause_menu.h): Continue, Retire, and the Adventure's Ranking. When
// the player finishes, FINISH! comes up, the AI takes over the car, a trackside camera follows it and the race fades
// out; a finish in the first six then plays the replay of the final lap. On Tin Raceway a car that falls off the
// course is out of the race, and when it's the player's the race fades out and ends as Retire does. Then the race is
// over and its result goes
// to the mode that ran it (Quick Race, 2 Player's results, the Adventure's or the World Grand Prix's results). See docs/race.md, ai.md, cars.md,
// car_physics.md, hud.md, finish.md and pause.md.
//
// 2 Player (StartVersus) races the same way split screen: the two players' cars (and their teammates, AI-driven)
// with no course opponents, each player's view in their half of the screen with their own camera, pad, sounds and
// HUD; the race ends when both have finished. See docs/two_player.md.
//
// On the events' courses the course row's own task runs in place of Race_Task (race_events.cpp, docs/events.md):
// the Sliding Door Race (course 0x13), the Highway Race (0x14) and the Obstacle Course (0x1D), with their own
// countdown, banners and ending; and Soccer (0x1A, race_soccer.cpp): the ball, the kick-offs and goals, and the AI
// that plays the teammates. The Adventure's own games have theirs too (docs/minigames.md): Barrel Dodging
// (race_barrels.cpp); the Drag Race, Rock Climbing, the Volcano Course, Rainbow Jump, Beach Flag, the Treasure
// Hunting Maze, Curling, Ski Jumping, Fishing and the Which-way? Race (race_games.cpp); Roulette (race_roulette.cpp),
// Figure 8 (race_figure8.cpp) and Golf (race_golf.cpp), whose menus and cards their tasks' children draw over the
// frame.
#pragma once

#include <string>
#include <vector>

#include "../../gfx/draw2d.h"
#include "../../platform/input.h"
#include "../hud/race_hud.h"
#include "../ui/window.h"
#include "ai.h"
#include "car.h"
#include "car_effects.h"
#include "car_physics.h"
#include "collision.h"
#include "course.h"
#include "course_objects.h"
#include "pause_menu.h"
#include "chase_camera.h"
#include "race_camera.h"

namespace game {

// How a race ended, for the mode's results (D_01787008, D_01786FF0 and the save's records): the Quick Race goes back
// to the course select; the Adventure's results pay by place and can raise the license (docs/finish.md).
struct RaceResult {
    int course;                     // course id
    int place;                      // the player's, 0 = first
    int totalTime;                  // frames, from green to the line
    int laps[3];                    // the player's last three lap times
    int bestLap;                    // the best of them (or the course's best before)
    int raceBestLap;                // the best lap of this race (D_01787074)
    int cars;
    int order[Cars::kMaxCars];      // each car's place (car 0 the player's), -1 out of the race
    // Each car's total time (D_01786FF0) and best lap of the race (D_01787074), kept for the players and their
    // teammates; 0x7FFFFFFF for none.
    int total[Cars::kMaxCars];
    int best[Cars::kMaxCars];
    const char* name[Cars::kMaxCars];
    // Each car's driver record (D_01787008 +0, +1: group and index), group -1 for the player.
    int group[Cars::kMaxCars];
    int index[Cars::kMaxCars];
    // 2 Player: the car of each side's player (member 0) and teammates (1, 2), -1 for none; the Adventure's in side 0.
    int versusCar[2][3];
    // Race_Outcome (D_00335704) as an event task left it: in 2 Player 2 player 1 won, 1 player 2; 0 for a race.
    int outcome;
    // The place in the town (info+0x21) an event sent the player on to, -1 back to the building it came from.
    int scenePlace;
};

class Race {
public:
    enum Result { kRunning, kRetired, kOver };

    Race(gfx::Draw2D& draw, const std::string& dataDir);
    // Main_Loop's Frame_BlendPrevious for the frame (Draw2D::EndFrame): FIX 0x20, but none while race flag 0x8000
    // (the pause) stands.
    uint8_t FrameBlend() const { return paused_ ? 0 : kFrameBlend; }
    // Main_Loop's frame clear: the background colour (pad +0x50, the light state's +0x174), which shows wherever the
    // course and its sky leave the frame bare (Rainbow Jump's white below the clouds). Black once the race is over.
    uint32_t ClearColor() const { return phase_ == kDone ? 0x80000000u : light_.background | 0x80000000u; }
    // Car i's height (+0x94), which func_00238950 reads after the Single Lap Race.
    float CarHeight(int i) const { return i < carCount_ ? bodies_[i].pos[1] : 0.0f; }
    static const uint8_t kFrameBlend = 0x20;

    // Loads course `id`, its collision data and its Quick Race field, and puts every car on its grid slot (the
    // player on the last). (PC) On the Test Course (Course::kTestCourse) the player's car is alone, in the first slot.
    void Start(int id);
    // (PC) The lap to beat for the brighter chime, after Start, for a course the save has no card for (the Test
    // Course): frames, or 0x7FFFFFFF for none.
    void SetBestLap(int frames) { bestLap_[0] = frames; }
    // 2 Player (Race_Setup with mode & 2): course row `row` (course `id`, `cars` and `laps` from 2 Player's table)
    // in race mode `mode`, split screen: the two players' cars from their records and, when both have a second
    // teammate, each one's teammates; no course opponents.
    void StartVersus(int id, int row, int cars, int laps, int mode);
    // The Adventure (Race_Setup with mode 0): Adventure race row `race` (race_list.h), its course and laps, the
    // player's car and the save's teammates, each with their own parts, and the row's opponents with its parts record
    // (Race_CreateAdventureCars). The pause menu has the Ranking.
    // `solo`: the developers' test menu's Race Solo (race flag 0x800, which Race_Setup keeps): Race_CreateCars with no
    // setup, so the player's car alone in grid slot 0, no teammates or opponents; race flags 0x104 from the start
    // (green and the pause, the countdown playing on all the same); and Race_CompleteLap stops after a lap's time,
    // so the laps go on and the race ends only by Retire.
    void StartAdventure(int race, bool solo = false);
    // The title's attract demo (Title_AttractDemoTask's Race_Setup, race mode 8): Adventure row 24, Endurance Run,
    // with no player: 22 computer cars driving from the first frame, no Race_Task (no countdown, HUD, laps or
    // finish), no pause, the trackside camera on car 4. It runs until the title ends it.
    void StartDemo();
    // func_0022E5F8, at each of an Adventure race's ends (Race_SetupTask: Retire, the finish, an event's end): the
    // player's odometer to the save's distance (and its billboard sponsor's), each teammate's to its own sponsor's.
    void Race_SaveDistance();
    // Runs and draws one frame into a frame already begun. kRetired once Retire has been chosen in the pause menu
    // or the player's car has fallen off the course (the frame drawn stays up for the loading screen), kOver once the race and its replay are done and the screen
    // is black; Outcome() then says how it went.
    Result Update(const platform::Input& input);
    const RaceResult& Outcome() const { return result_; }
    // Gpx_RetireTeam (func_0020F330), as a World Grand Prix stage ends in a retire or a fall: the places as they
    // stand, the player's team out (-1, no points) and every other car's moved up past the team's cars that were
    // ahead of it (one out stays out).
    RaceResult RetireResult() const;

    // Development (--cartrace): a CSV row per frame of one car (0 the player's, 1-23 the opponents' as their record
    // numbers), the fields scripts/pcsx2/cartrace.py logs from the PS2, for comparing the physics and the AI.
    static void SetTrace(const std::string& path, int car);
    // Development (--autodrive): the AI drives the player's car too, as in the title's attract demo, held to `cap`
    // (2.5 km/h, 0 for none) until it finishes. For checking the finish and the replay without driving three laps.
    static void SetAutoDrive(int cap);
    // Development (--ranking): the pause menu has the Adventure's Ranking in a Quick Race too.
    static void SetRankingItem(bool on);
    // Development (--clock): every race starts at this time of day (frames, 0-0x34BC0; Light_TimeOfDay) instead of
    // its course's, as the debug byte's L1/L2 scrub on Endurance Run did. -1: the course's.
    static void SetClock(int32_t clock);
    // Development (--place): the player's car starts at (x, z) on the ground under height y, facing `yaw` (16-bit
    // angle, as --cartrace writes it), instead of on the grid. With scripts/pcsx2/teleport.py the same spot can be
    // seen on both.
    static void SetPlace(float x, float y, float z, int yaw);
    // --place's spot, if given (the field takes it in its start block's units, which may be past its edges).
    static bool Placed(float* pos, uint16_t* yaw);
    // Development (--gate): on the Obstacle Course player 1 starts on gate `gate` (car +0x24A), on the Highway Race
    // in AI zone `gate` (the same byte), as with PINE on the PS2.
    static void SetGate(int gate);
    // Development: the race seen from a fixed eye at (x, y, z) looking along yaw (16-bit angle, 0 = +z) tilted
    // `pitch` down, in place of the chase camera, to look at the course's scenery.
    static void SetEye(float x, float y, float z, int yaw, int pitch);
    // --eye's view in place of `view` (in the units of whatever is drawn: the field's are the car's block's); false
    // without --eye.
    static bool EyeView(gfx::Camera3D* view);

private:
    // A car's progress round the course (its record's +0x19A, +0x19B, +0x260 and +0x247).
    struct Lap {
        int zone;       // where it last was at the line: 0 not crossed yet, 1 backed out over it, 2-4 the three strips
        int laps;       // laps done
        int time;       // frames into this lap, counting from the first crossing
        int order;      // its place, 0 = first (the grid slot at the start)
        bool finished;  // record flag 0x200
        int best;       // its best lap of the race (D_01787068 +0xC)
        int total;      // its time at the finish (D_01786FF0)
    };

    static const int kMaxPlayers = 2;

    // Race_Setup's common part, after the cars are made: the course, the grid, the parts, the AI and the sounds.
    void Race_Setup(int id);
    // The player driving car `i` (0 or 1), or -1 for a car the AI drives. Players' cars come first.
    int PlayerOf(int i) const { return i < players_ ? i : -1; }
    void Car_PlaceOnGrid(RaceCar& car);   // the grid part of Car_Init (0x00218458)
    // Puts a car on the ground under it, as Car_Init's drop (D_00335710, the collision's ground query) does.
    void Land(RaceCar& car);
    gfx::Camera3D RaceView(int player) const;   // the player's chase camera's view (or --eye's)
    // Race_DrawView for one view: the course, the cars, the objects and the effects through `camera`.
    void Race_DrawView(const gfx::Camera3D& camera, int hidden);

    // The race from its start to the player's finish, then the replay: the frame's cars, line, order and HUD.
    // pad[p] is player p's input bits.
    void Race_Step(const uint32_t* pad);
    // Race_SetupTask (0x0020F3B8) once the finish's fade is done: a finish in the first six (except on courses 0x0F
    // and 0x10) plays the replay, else the race is over.
    void Race_AfterFinish();
    // Replay_Save (0x0021CBC8), as the final lap starts: the race as it stands; the player's inputs are recorded
    // from then on. Replay_Restore (0x0021CCB8) puts it back for the replay.
    void Replay_Save();
    void Replay_Restore();

    // The line (Race_Task's per-car part, 0x0022D620): which of the course's three strips the car is in moves its
    // zone on (D_002A8198); entering the first strip after the third completes a lap.
    void Race_CheckLapZones(int car);
    // Race_CountdownTask (0x0022DAE8): the start lights, the countdown sound, green (go_) and the song, by frame_.
    void Race_CountdownTask();
    void Race_CompleteLap(int car);   // 0x0022D390: the lap's time, the lap chime, FINAL LAP, the finish
    void Race_RankCars();             // 0x0022D7B8: the cars still racing put in order behind those that finished
    bool Car_Ahead(int a, int b) const;   // its Obj_Before
    // Race_SetLapRows (0x0022D8C8) and the rest of the HUD's state for this frame, for player `p`.
    void Race_FillHud(int p, RaceHudState& s, MapMarker* markers);
    // func_00217830, for each pair Cars_Collide pushed apart: a bump with the player's car sounds (CQ_MAIN 0x14)
    // unless the other car bumped it in the last 8 frames (+0x210), and not in the replay or after the finish.
    void Cars_BumpSound(const CarContact* contacts, int count);
    // Race_SetupTask on Start, from the song's start (race flag 0x100) until the race is over, not in the replay:
    // race flag 0x8000 (everything stands still), Sound_MuteCars, the pause sound (0x1F), the song muted, and the
    // pause menu (Race_PauseTask). Race_Resume is Continue: the flag cleared, the engine and the song back.
    void Race_Pause(int pad);
    void Race_Resume();
    void Race_UpdateLight();   // Race_SetupTask's part: the clock, Light_TimeOfDay, Light_CourseFog
    // Race_CheckFall (0x0022DC68), Race_Task's child on courses 0x0F and 0x10 (Tin Raceway) from green: a car still
    // racing that is below height 110 has fallen off. An opponent is out of the race (+0x198 = 0, place -1); the
    // player's car ends it: the song fades (Sound_FadeOutSong), the car sounds mute, race flag 0x10000 (no map or
    // meter) and no pause (0x100 cleared), and Race_Task becomes Race_FallOutTask (kFallOut).
    void Race_CheckFall();
    // (PC) The Test Course's road is a closed tube with nothing outside it but the start box under the gap where its
    // ends meet. A car that gets out, 3 lower than the tube's floor at both of its AI zone's gates, is put back on the
    // floor at the zone's far gate, stopped and facing along the road (Car_Reset, 4 above the ground, as the Obstacle
    // Course puts a car back at its checkpoint). The clock runs on.
    void TestCourse_PutBack();
    bool TestCourse_Floor(float x, float z, float* height) const;
    // SingleLap_FallTask (func_00263C38), Race_Task's child on course 0x26 (the Single Lap Race, A20) from green: it
    // puts both cars side by side on the start line (819, 300, 695 and 700, facing -x); then the first car below 280
    // decides the race (Race_Outcome 1 the player's, 2 Travis's, the player's car braked: flag 0x800), race flag 8
    // (over, no pause), and after a 0xC0-frame fade (the overlay from frame 0x41) the race ends (kEventDone).
    void Race_SingleLapTask();

    // The course row's task (D_002BE1D8 +8 and the Adventure's rows; by course): Race_Task, or an event's.
    enum RaceTask {
        kTaskRace, kTaskSlidingDoor, kTaskObstacle, kTaskHighway, kTaskSoccer, kTaskBarrel,
        // The Adventure's games played alone or against one car (race_games.cpp).
        kTaskDrag, kTaskRockClimb, kTaskVolcano, kTaskRainbow, kTaskBeachFlag, kTaskMaze, kTaskCurling, kTaskSkiJump,
        kTaskFishing, kTaskWhichWay, kTaskRoulette, kTaskFigure8, kTaskGolf
    };
    static RaceTask Race_TaskFor(int course);
    // The event task's first frame (func_0021C928's places, the HUD canvas, the song) and its countdown
    // (func_00233A80: 3, 2, 1, GO! from the course's HUD file, in place of the start lights).
    void Race_EventStart();
    void Race_EventCountdownTask();
    // The event task each frame, after the cars: true once it has ended (the screen black, race flag 8).
    bool Race_EventTask();
    bool Race_SlidingDoorTask();             // func_002595F0
    void SlidingDoor_CheckCar(int car);      // func_00259498
    bool Race_ObstacleTask();                // func_0025EB70
    void Obstacle_PlayerTask(int p);         // its state machine for player p's car
    bool Race_HighwayTask();                 // func_0025FF00
    void Highway_PlayerTask(int p);          // its state machine for player p's car
    // Soccer (race_soccer.cpp).
    bool Race_SoccerTask();                  // func_00254430
    void Soccer_Kickoff();                   // its state 0: the cars to their places, the ball to the middle
    void Soccer_SetKickoff(bool on);         // KICKOFF! (+0x82B4, func_002351A0)
    void Soccer_SetGoal(bool on);            // GOAL!! (+0x82E0, func_00235238)
    void Soccer_SetScore(int mode);          // the score (+0x8438, func_002350F0)
    void Soccer_BrakeAll(bool on);           // car flag 0x800 on all six
    void Ball_Step();                        // the task's ball part
    void Ball_HitCars();                     // func_00253B80
    void Ball_HitWalls();                    // func_00253E90
    void Ball_Spin();                        // func_00254290
    uint32_t Soccer_CarAi(int car);          // func_00254CC8 and func_00253680
    void Soccer_Attack(int car);             // func_00253448
    bool Soccer_Steer(int car, int angle, int cap);   // func_00253230
    // Race_CountdownForCourse's pick (func_002357C0, jtbl_002EC3E0): the race's start lights (Race_CountdownTask)
    // for the race courses and the Drag Race (0x16) and Ski Jumping (0x23); none for Soccer; else the event's.
    // Curling's task makes no countdown either: its own banner and wait are the start.
    bool Race_StartLightsCountdown() const {
        return task_ == kTaskRace || id_ == 0x16 || id_ == 0x19 || id_ == 0x23;
    }
    bool Race_EventCountdown() const {
        return !Race_StartLightsCountdown() && task_ != kTaskSoccer && task_ != kTaskCurling &&
               task_ != kTaskRoulette && task_ != kTaskGolf;
    }
    bool Race_HasCountdown() const { return Race_StartLightsCountdown() || Race_EventCountdown(); }
    // The games (race_games.cpp).
    bool Race_DragTask();                    // func_00260638
    bool Race_RockClimbTask();               // func_0025FBF8
    bool Race_VolcanoTask();                 // func_0025E630
    bool Race_RainbowTask();                 // func_00254D20
    bool Race_BeachFlagTask();               // func_002638C0
    bool Race_MazeTask();                    // func_0025F6A0
    void Race_GameStart();                   // their first frames: the song and the cars' places
    bool Race_CurlingTask();                 // func_002631A0
    bool Race_SkiJumpTask();                 // func_00263E68
    bool Race_FishingTask();                 // func_0025DBB8
    void Fish_Task(int i);                   // func_0025D7C8, for fish i
    bool Race_WhichWayTask();                // func_00262678
    // Roulette (race_roulette.cpp).
    bool Race_RouletteTask();                // func_00260DC0
    void Roulette_Wheel();                   // func_002609D0
    void Roulette_CarRide();                 // func_00260B48
    void Roulette_SetResult(int pocket);     // func_00262358
    void Roulette_HideView(bool on);         // Race_HideView (func_0024FB38): race flag 0x2000
    void Roulette_DrawScreens(const platform::Input& in);   // the board's and the result's tasks
    void Roulette_OpenWindow(int which);
    int Roulette_UpdateWindow(const platform::Input& in);
    int Roulette_WagerTask(const platform::Input& in);       // func_00261638
    // Figure 8 (race_figure8.cpp).
    bool Race_Figure8Task();                 // func_0025A120
    void Figure8_ConeTask(int j);            // func_00259A08
    void Figure8_RecordTime(int time, int hits);   // func_0025AA58
    void Figure8_DrawMap();                  // RaceMap_DrawStep (func_00242A88) and the cones
    void Figure8_DrawScreens(const platform::Input& in);   // the step menu's and the results' tasks
    // Golf (race_golf.cpp).
    bool Race_GolfTask();                    // func_00251B60
    bool Golf_CheckHit();                    // func_00250FE0
    void Golf_CarTouchBall();                // func_00251190
    bool GolfBall_Step();                    // Ball_Step, func_00251440
    void Golf_PlaceHole();                   // func_00250F00
    void Golf_RecordHole(int strokes);       // func_00252738
    void Golf_SetResultWord();               // func_00233338
    void Golf_DrawMap();                     // RaceMap_DrawHole (func_00242A88) with the ball
    static int Golf_Par(int hole);           // D_002AC0E2: the hole's par
    void Golf_DrawScreens(const platform::Input& in);   // the hole's card and the scorecard
    // The games' own screens drawn over the frame by their tasks' children (Roulette's, Figure 8's, Golf's).
    void Race_DrawGameScreens(const platform::Input& in);
    void WhichWay_PutCar(const float* pos, int32_t vz);
    void Event_SetTimeUp(int side);          // TIME UP (Hud_SetTimeUp, func_00234B98) in the MISS COURSE! slot
    void Race_DrawGamePieces();              // the games' own canvas pieces (hud_games.h)
    void Event_SetNewRecord(int side);       // NEW RECORD (Hud_SetNewRecord, func_002347F0) in the WIN! slot
    void Curling_SetTryBanner(int n);        // FIRST TRY / SECOND TRY / FINAL (func_00234618), 0 none
    void Volcano_PlaceObjects();             // its three matrices, every frame
    void Maze_PlaceObjects();
    void Event_SetCourseOut(int side);       // COURSE OUT (Hud_SetCourseOut, func_00234AB0)
    void Event_SetGoalA03(int side);         // GOAL!! (func_002348D8)
    // Barrel Dodging (race_barrels.cpp).
    bool Race_BarrelTask();                  // func_0025E278
    void Barrel_Task(int i);                 // func_0025DF80, for barrel i
    // The banners' elements: a player's WIN! (+0x86F0, func_00232D68) and LOSE or MISS (+0x871C, func_00232F08,
    // func_002349C8), and the countdown (+0x80A4).
    void Event_SetWin(int side);
    void Event_SetLose(int side);
    void Event_SetMiss(int side);
    void Event_SetClear(int side);           // CLEAR (Hud_SetClear, func_00234708) in the WIN! slot
    void Event_SetFinish(int side);          // FINISH! (Hud_SetFinish, func_00234128)
    void Event_SetMissCourse(int side);      // MISS COURSE! (+0x8748 + 0x408 side, func_00233158)
    // Hud_SetTime (func_00233FC8): TIME (D_002EC1C0's heading at (40, 20)) over a running time row at (40, 52),
    // showing `frames` (+0x808C); it stays up once set.
    void Event_SetTime(int frames) { eventTime_ = frames; }
    // The overlays the event asked for (the 2 Player halves, func_0022EA78 and func_0022EAA0, and Fade_SetOverlay's
    // over the whole screen): called under the canvas's elements (`underCanvas`) and again over them, it draws them
    // where the event's task does.
    void Event_DrawFades(bool underCanvas);
    // The result an event leaves: the course, the cars and Race_Outcome; no times or places.
    void Race_EventResult();

    RaceTask task_;
    bool braked_[Cars::kMaxCars];     // car flag 0x800: Car_Input holds the brake, the steering still works
    bool noPause_;                    // the event cleared race flag 0x100
    // The canvas's elements in the order HudCanvas_Draw draws them: the shared list (the countdown +0x80A4, Soccer's
    // KICKOFF! +0x82B4 and GOAL!! +0x82E0, the next +0x830C: Golf's CUP IN and result words use these three), then
    // each player's WIN! and LOSE/MISS and each one's MISS COURSE!. Soccer's score is drawn between the two.
    // FINISH! (+0x8590 + 0x408 p) comes before WIN! in each player's list; it's never up with them, so the port
    // keeps it last.
    // Figure 8's PYLON TOUCH! (+0x8774) is the last.
    enum { kElemCountdown, kElemKickoff, kElemGoal, kElemShared3, kElemWin, kElemLose, kElemMissCourse = 8,
           kElemFinish = 10, kElemPylon = 12, kElems = 13 };
    HudElement elements_[kElems];
    int eventTime_;                   // the TIME element's time, -1 not set
    // The event tasks' state (their +0x8CB0 block and Race_Task-like fields).
    struct EventState {
        int phase;          // Sliding Door: 0-3, -1 done
        int winner;         // its +0x25: car + 1 that stopped on the goal first, 0 none
        int failed;         // its +0x27: a bit a car that stopped short or fell in the pond
        int fade;           // its +2: the end's count
        int state[2];       // Obstacle Course, Highway Race: each player's state (+4, +6), -1 none
        int count[2];       // each player's fade count (+0, +2)
        int checkpoint[2];  // each car's gate (car +0x24A)
        int time;           // the one player's time (+0xC)
        int flags;          // Highway Race (+0x10): a bit per car that has passed zone 0x14
        bool done;          // +8
        int fadeAll;        // the overlay over the whole screen this frame (alpha, PS2 scale: 0x80 black)
        int fadeHalf[2];    // the overlay over each player's half (the whole screen with one player)
    } event_;
    // Soccer's task state (its +0x8CB0 block).
    struct SoccerBall {     // +0x20, 0x50 bytes
        float up[4], dir[4];   // two axes that roll with it (Matrix_Billboard's up and direction)
        float pos[4];          // its centre; it rests at height 2
        float vel[4];
        uint32_t touching;     // +0x44: a bit per car touching it
        int soundWait;         // +0x48: frames before its sound again
        bool kicked;           // +0x4C: hit by a car or a wall this frame
    };
    struct SoccerAi {       // +0x70 + 12 x place: an AI car's slot
        int mode;           // 0 for the ball, 1 back to its spot, 2 facing the kick-off way
        uint32_t bits;      // the input bits it drives with
        int gear;           // Soccer_Steer's: 0 stopping, 1 forward, 2 reverse
    };
    struct SoccerScore {    // the score element: mode 0 none, 1 small at the top, 2 large in the middle
        int mode, x, y, s0, s1;
    };
    struct SoccerState {
        int state, count;   // +0 and +4
        bool done;          // +8
        bool hud;           // race flag 0x10000 off: the map, the meters and the canvas show
        int scorer;         // +0x10: the team that scored last
        int goals[2];       // +0x14, +0x18: team A's (player 1's), team B's
        SoccerBall ball;
        SoccerAi ai[6];
        int place[Cars::kMaxCars];   // each car's place on its team (car +0x24A, D_002AC480)
        SoccerScore score;
    } soccer_;
    // The games' state (their +0x8CB0 blocks and the task's +2 count).
    struct GameState {
        int state;
        int count;          // the task's +2: the end's fade
        int time;           // the TIME element's frames
        int lid;            // the Volcano Course's and the maze's lid: degrees + 180
        float wrench;       // the Volcano Course's wrench's height
        bool taken;         // Beach Flag: the flag reached
        bool hidden;        // race flag 0x10000: the meter and the canvas hidden (Rainbow Jump's end)
        bool done;
        float last[3];      // Rainbow Jump: the car's place last frame
        int frames;         // Curling's +8: a count of frames in a state
        int attempt;        // Curling's try (the task's +0x26)
        bool board, bigScore;      // Curling's board (+0x844C) and its big score (+0x845C)
        int points[4];      // Curling's tries' points and their total (+0x8460), -1 none
        int phase;          // Ski Jumping's +8: 0 the in-run, 1 in the air, 2 the run-out
        int distance;       // Ski Jumping's jump (+0xC), shown from the first frame (+0x8470)
        int caught;         // Fishing's count (+0x9250)
        bool catchSignal;   // Fishing: the task's +4 and +0x24, a fish caught this frame
        int sign;           // the Which-way? Race's +0xC: +1 the right road on the +x side, -1 on the -x side
        int result;         // +0xD: 0 playing, 1 all five right, 2 a wrong fork
        int stretch;        // +0xE: 0-2
        int right, decoy;   // +0xF, +0x10: tiles of the round's list
        int timeLimit;      // TIME LIMIT's seconds (+0x849C)
        bool timeShown;     // +0x8490
    } game_;
    // Roulette's state: the task's block (+0x8CB0), the betting block (+0x8CF0) and its children's.
    enum { kRouletteNone, kRouletteBoard, kRouletteResult };
    enum { kRouletteNoMoney, kRouletteNeedBet, kRouletteStart };
    enum { kWindowOpen = -100 };
    struct RouletteWager {   // func_00261638's: +0x8010 amount, +0x8014 most, +0x8018 digits - 1, +0x801C the digit
        int state, frame, amount, max, cols, pos;
    };
    struct RouletteState {
        int state, count;    // +0, +4
        int wait;            // the task's +2: the hold before green
        uint16_t wheel, speed;   // +0xC, +0xE: the wheel's turn and its speed a frame
        bool drawCanvas;     // the task drew its canvas this frame
        int bet;             // +0x8008: the bet (and the cursor), 0-22
        int startMoney;      // +0x800C
        int number, colour;  // +0x8020, +0x8024: the pocket's number and colour
        bool won;            // +0x8028
        int child;           // the child task running: the board, the result
        int childState, childCount, childFrame, childResult;
        bool pulse;          // the child's answer (the task's +0x24) and its value (+4)
        int pulseValue;
        RouletteWager wager;
        Window window;
        int windowKind;
    } roulette_;
    // Figure 8's state: the task's block (+0x90F0), the menus' (+0x9100) and the cones' (+0x8CB0 + 0x40 j).
    enum { kFigure8None, kFigure8Select, kFigure8Result };
    struct Figure8Cone {
        uint8_t flags = 0;   // +2: 1 wobbling, 2 tipping, 4 hopping, 8 sliding, 0x10 settled
        int side = 0;        // +3: the side the car came from
        bool down = false;   // +4: knocked over
        int count = 0;       // +6
        float vel[3] = {}, vel0[3] = {};   // +8, +0x14: its speed and the speed it was given
        float pos[3] = {};   // +0x20
        int16_t rx = 0, rz = 0;   // +0x30, +0x34
    };
    struct Figure8State {
        int state = 0;       // +0
        bool end = false;    // +2
        int pylonBlink = 0;  // +4: PYLON TOUCH!'s count down
        int time = 0;        // +8
        int hits = 0;        // +0xC: cones touched
        int fade = 0;        // the task's +2
        int step = 0;        // the task's +0x1C: the step played
        int gate = 0;        // car +0x24A
        int coneSignal = 0;  // the task's +4 from a cone: 1 a touch, 2 only the sound
        bool pulse = false;  // a menu's answer (the task's +0x24) and its value (+4)
        int pulseValue = 0;
        int cursor = 0;      // +0x80A8: the menu's step
        // Each step's last time (0 a miss), cones hit, total and best (+0x80AC, +0x80C0, +0x80D4, +0x80FC), and
        // whether the best is new (+0x80E8).
        int stepTime[5] = {}, stepHits[5] = {}, stepTotal[5] = {}, stepBest[5] = {};
        bool stepNew[5] = {};
        int child = kFigure8None, childState = 0, childCount = 0, childFrame = 0, childResult = 0;
        Window window;
        char lines[5][16] = {};
        Figure8Cone cones[17];
        int coneCount = 0;
    } figure8_;
    // Golf's state: the task's block (+0x8CB0), the card's (+0x8D50 + 0x8000) and the panel (+0x8414).
    enum { kGolfNone, kGolfIntro, kGolfCard };
    struct GolfState {
        int state = 0, count = 0;   // +0, +4
        bool drawHud = false;       // the task drew this frame (not states 3 and 41)
        int hole = 0;               // +0x10
        int strokes[9] = {};        // +0x14: taken on each hole before the one being played
        float yardsToGo = 0;        // +0x38
        float axisA[3] = {}, axisB[3] = {};   // +0x40, +0x50: the ball's up and direction, rolling with it
        float ball[4] = {};         // +0x60: its centre (w the water's level)
        float vel[3] = {};          // +0x70
        int seCool = 0;             // +0x80
        uint32_t sePending = 0;     // +0x84
        bool armed = false;         // +0x88: the car has been 2.2 from the ball since the shot
        bool nudged = false;        // +0x8C
        bool underwater = false;    // +0x90
        int stopWait = 0;           // +0x94
        int airborne = 0;           // +0x98
        int yards[9] = {};          // the card's: G+0x8034
        int score[9] = {};          // G+0x8084: strokes - par, 0x7F none
        int total = 0;              // G+0x80A8
        int best[10] = {};          // G+0x805C, the round's at 9
        bool newBest[10] = {};      // G+0x80AC
        bool panel = false;         // +0x8414
        int panelYards = 0, panelToGo = 0, panelStrokes = 0;   // +0x8424, +0x8428, +0x8430
        int child = kGolfNone, childState = 0, childCount = 0, childFrame = 0;
        bool pulse = false;         // the card's answer (the task's +0x24)
    } golf_;
    bool hideView_ = false;   // race flag 0x2000: no 3D view, the camera still
    bool overhead_ = false;   // race flag 0x80000000: Camera_OverheadTask's view (D_0029CCA0 on course 0x18)
    struct Fish {           // Fishing's fish (canvas +0x8CB0 + 0x30 i)
        float pos[3];
        uint16_t yaw, target;   // +0x24, +0x2A
        bool caught;            // +0x20
    } fish_[30];
    // Barrel Dodging's state (its +0x8CB0 block): 32 barrels of 0x20 bytes, then the task's +0x90B0.
    struct Barrel {
        float pos[3];       // +0: its centre
        float shown;        // +0xC: the matrix's [3][3], 1 once it's drawn
        float fall;         // +0x10: its speed down off the ledge
        int state;          // +0x19: 0 rolling to the ledge, 1 falling, 2 rolling on the ground, 3 stopped (it hit)
        int frame;          // its task's +8
    };
    struct BarrelState {
        int state;          // +0x90B4
        int count;          // the task's +2: the end's fade
        bool hit;           // the task's +4: a barrel hit the car
        bool done;          // +0x90B8
        Barrel barrels[32];
    } barrel_;

    gfx::Draw2D& d_;
    std::string dataDir_;
    Course course_;
    CourseObjects objects_;           // the course task's objects and animated textures
    Collision collision_;
    Cars carModels_;
    RaceHud hud_;
    int id_;                          // course id (pad +0x22)
    int players_;                     // 1, or 2 in 2 Player (pad +0x0A & 2): the split screen
    int laps_;                        // the race's laps (the course row's)
    RaceCar cars_[Cars::kMaxCars];    // D_0177AC50, 0x270 bytes each; the players' cars first
    int carCount_;
    int side_[Cars::kMaxCars];        // whose side a car is on (car flags 0x40): 0 player 1's, 1 player 2's
    bool teammate_[Cars::kMaxCars];   // an Adventure teammate (car flags 0x30)
    CarRecord versusCars_[6];         // 2 Player's cars (D_0177EA50[0..5]), from the two records; the Adventure's 0-2
    bool adventure_;                  // the Adventure (pad +0x0A = 0): the Ranking in the pause menu
    bool demo_ = false;               // the attract demo (pad +0x0A = 8)
    bool solo_ = false;               // race flag 0x800: the test menu's Race Solo
    int replayGroup_ = 0;             // Camera_ReplayTask's +0x25: 0 the player, 1 and 2 the teammates
    int adventureRace_ = -1;          // the Adventure's race row (info+0x20), else -1
    bool pitting_[Cars::kMaxCars] = {};   // car flag 0x400: in the Endurance Run's pit lane
    // The debug pause items (the Pause Items extra): race flag 0x10000 as item 3 sets it (the map and the meters
    // hidden), and car flag 0x80 as item 5 sets it on the player's car (the AI drives it, as --autodrive does).
    bool meterHidden_ = false;
    bool aiDriven_[Cars::kMaxCars] = {};
    bool AiDriven(int i) const;
    // The zone table editor, item 6 (race_zone_table.cpp): while it's open it runs in the pause menu's place.
    // Ai_ZoneTableTask (0x0026D020) is one frame of it; false when Cross or Triangle goes back to the menu.
    bool zoneTable_ = false;
    bool Ai_ZoneTableTask(const platform::Input& input);
    // The player's speed table (+0x258), made the race's own (zoneSpeeds_) so the editor can change it: every car
    // that drove by the same table drives by the copy, as the game's cars share D_002A9B80's. zoneFile_ is where
    // the editor saves it (data/ai: the path's Cnn.speeds.txt, or Gnn.speeds.txt for an Adventure race's table).
    uint8_t zoneSpeeds_[256] = {};
    std::string zoneFile_;
    const char* zoneStatus_ = nullptr;   // the PC's: the last save's result, on the screen for a while
    int zoneStatusFrames_ = 0;
    uint8_t* Ai_EditZoneTable();
    // The PC's: an Adventure race's table saved by the editor (Gnn.speeds.txt), in place of the race's own.
    void Ai_LoadRaceZoneTable();
    HudElement pitBanners_[2] = {};       // Race_Task's PITTING (+0x8614) and GO! (+0x8640) on race 24
    // The Endurance Run's pit stop (Car_Input 0x0021A9D8): in zone 102 before the last lap the AI drives the car
    // (no jet) and the fuel fills 0x200 a frame. Returns whether car i is in it.
    bool Race_PitStop(int i, int next, uint32_t* bits, bool replay);
    // Race_Task's two banners on race 24 (0x0022E224): PITTING while the player's car pits, GO! in the pit lane's
    // exit (zone 103) facing the right way.
    void Race_DrawPitBanners();
    int replayCar_ = 0;               // and its car (+0x27)
    // The car of Camera_ReplayTask's group (car flags & 0x27E: 2 the player, 0x10 and 0x20 the teammates), or -1.
    int Replay_GroupCar(int group) const;
    CarBody bodies_[Cars::kMaxCars];  // each car's physics
    CarParts parts_[Cars::kMaxCars];
    AiPath path_;                     // the course's AI path (Ai_Init's +0x250/+0x254/+0x258)
    AiCar ai_[Cars::kMaxCars];        // each car's place on it, and the opponents' AI settings
    CarSounds sounds_[kMaxPlayers];   // each player's engine, skid loop and horn
    CarEffects effects_;              // the particles at the cars' wheels
    PauseMenu pause_;
    bool paused_;                     // race flag 0x8000
    MeterState meterState_[kMaxPlayers];   // each player's meter count (Meter_Draw clears it on its first frame)
    bool horn_[kMaxPlayers];          // each player's horn key held this frame
    // The Height Keys extra: each player's d-pad Up (1) or Down (-1) held, for Car_SetHeightKey.
    int heightKey_[kMaxPlayers] = {};
    // The Clock Scrub extra: player 1's L2 (Wing Down: +1) or L1 (Wing Up: -1) held, the clock's scrub this frame.
    int clockScrub_ = 0;
    Lap lap_[Cars::kMaxCars];
    int playerLaps_[kMaxPlayers][3];  // each player's last three lap times (D_01787068)
    int bestLap_[kMaxPlayers];        // the course's best lap (D_00335700: from the save, Race_InitTimes)
    StartLights lights_;              // the HUD canvas's start lights
    bool go_;                         // race flag 4: green; the clock runs and the cars may move
    int countdownStart_;              // the frame the countdown's task began (Figure 8's each step), -1 none yet
    int clock_;                       // the race clock (Race_Task +0x20): runs from green until the player finishes
    int finished_;                    // cars that finished (Race_Task +0x26)
    int frame_;                       // frames since the start
    RaceLight light_;                 // D_0177F5E0: the time of day, fog and car lights
    int32_t dayClock_;                // pad +0x14: the time of day (Light_CourseClock; only Endurance Run's runs)

    // After the player's finish (Race_Task +0x25 complete) and in the replay.
    // kFallOut: Race_FallOutTask (0x0022EAC8) after the player's car fell off: no clock, line, places or HUD; the
    // screen fades to black over 64 frames while the cars drive on, then the player's car is out and the race ends.
    // kEventDone: an event's task has ended (race_events.cpp).
    enum Phase { kRacing, kFinishFade, kReplay, kReplayFade, kFallOut, kEventDone, kDone };
    Phase phase_;
    int fade_;                        // Race_Task +2: the fade out's frames, to 0x80 (64 after a fall)
    int singleLap_ = -1;              // SingleLap_FallTask's state (+0x8CB0): -1 not yet, 0 place, 1 watch, 2 over,
                                      // 3 fade
    int singleLapFade_ = 0;           // its +2: the fade's frames
    int finish_[kMaxPlayers];         // frames since each player's FINISH! went up, -1 none
    int finalLap_;                    // Race_Task +0x27: FINAL LAP's blink frames left
    ChaseCamera chase_[kMaxPlayers];  // each player's camera while racing (Camera_ChaseTask)
    TrackCameras cameras_;            // the finish's and the replay's cameras (D_002A2A50)
    RaceResult result_;
    // The replay (race flags 0x20 recording, 0x40 playing): the race at the final lap's start, and the player's input
    // bits each frame since (the run-length list at race +0x44).
    struct Snapshot {
        RaceCar cars[Cars::kMaxCars];
        CarBody bodies[Cars::kMaxCars];
        AiCar ai[Cars::kMaxCars];
        Lap lap[Cars::kMaxCars];
        int finished, frame;
    };
    Snapshot replayStart_;
    bool replayPending_;              // the final lap began this frame: save at its end
    bool recording_;
    std::vector<uint32_t> replayBits_;
    size_t replayPos_;
};

// Car_ReadPad (0x0021A820): pad `pad` as the car code reads it, through that player's key setup (their record's
// +0x12E8, D_01780A48 + 0x3448 x player; the default: Cross accelerates, Square brakes, Circle reverses, R1 holds the
// lights on), the d-pad (or stick) steering. Car_HornHeld: the horn key (6) is held (the game starts the horn on its
// press and stops it on its release). Car_CameraButton: the camera key (7), the view switch (Camera_ChaseTask).
// Car_MapButton: the map key (9, D_01780A5A; Select by default), the field map's zoom (func_00242FE8).
uint32_t Car_ReadPad(const platform::Input& input, int pad = 0);
// info+0, the frames counted since the game started, which the events' games pick from (a barrel's lane). The port
// counts the races' frames.
uint64_t Info_Frames();
// Main_Loop (0x002006C0): info+0 counts every frame since boot (the root, TitleSequence::Update, calls this).
void Info_Tick();
bool Car_HornHeld(const platform::Input& input, int pad);
uint32_t Car_CameraButton(int pad);
uint32_t Car_MapButton(int pad);

}  // namespace game
