// The Adventure's field: driving around the world between the buildings. A high-level reimplementation of
// Adventure_Task's (0x002105B8) field states: func_00210158 and func_00210430 set it up
// (the world around the town's block, the field's tasks), Car_Init's field start (func_002182B0) puts the player's
// car at the door of the place it left by, and each frame state 0 runs the time of day, the car, the chase camera
// and the view (func_00220938), and the door check (func_00259138), which ends the field when the car drives into a
// building. See docs/field.md.
//
// Start opens the field's menu (FieldMenu_Task, field_menu.h): Adventure_Task's state 2 waits on it with the field
// stopped.
//
// The ChoroQ coins (field_coins.h) lie about the world to be picked up, and the towns' own cars (field_town_cars.h)
// drive round their towns: bumping into one starts Adventure_Task's state 8, their talk (Message_Task) over the
// stopped field.
//
// Cloud Hill (town 8) is a course field: course 0x22 (ACTION/A16) in place of the world's blocks (FieldWorld's course
// mode), its gate (A16_GateTask, CourseObjects), and driving off its roads (below y 200) lands the car in
// Pollepolle's building on Papaya Island.
#pragma once

#include <stdint.h>

#include <string>
#include <vector>

#include "../../gfx/draw2d.h"
#include "../../platform/input.h"
#include "../race/car.h"
#include "../race/car_effects.h"
#include "../race/car_physics.h"
#include "../race/chase_camera.h"
#include "../race/course_objects.h"
#include "../hud/speedometer.h"
#include "../race/race_light.h"
#include "../story/message_task.h"
#include "field_coins.h"
#include "field_map.h"
#include "field_menu.h"
#include "field_objects.h"
#include "field_town_cars.h"
#include "field_world.h"

namespace game {

// A town's place in the world (D_002BE438: its block and how many places and cars it has) and its doors.
struct FieldDoor {
    // D_002C0698[town] + 32 x place: the door's four corners (x, z) in the town's block, round its inside; a car
    // leaving drives out by the side from corner 2 to corner 3.
    float corner[8];
};
struct FieldTown {
    int block;               // +4: the world block it's in
    int places;              // +6: its places (the doors), whose people are its first records (D_002C22C8)
    int cars;                // +7: its cars on the roads, the records after them
    const FieldDoor* doors;
};
const FieldTown& Field_Town(int town);   // towns 0-21
// MyCity_PlaceOpen (func_0023DEB8): My City's place `place` is open (its resident has moved in, or it's always open).
bool MyCity_PlaceOpen(int place);

class Field {
public:
    Field(gfx::Draw2D& draw, const std::string& dataDir);

    // func_00210158 and func_00210430 for `town`, with the player's car at the door of `place` (the building it
    // drove out of), facing away from it. `clock` is the time of day (Adventure_Task's info+0x14).
    void Start(int town, int place, int32_t clock);
    // Starts reading the blocks around the start that Start(town, place) will need, so a loading screen can show
    // meanwhile (calling Stream each frame builds what's read; Start waits for the rest).
    void Preload(int town, int place);
    void Stream(double budgetMs);

    // One frame, drawn into a frame already begun. kEnter: the car drove into a building (Town(), Place());
    // kWarp: the menu's Warp chose WarpTown() (0 My Garage), after WarpFrames() of the menu.
    // A talk that ends with code 0x14 (result 11) goes to the place it names as kEnter does.
    enum Result { kRunning, kEnter, kWarp };
    Result Update(const platform::Input& input);

    int Town() const { return town_; }
    int Place() const { return place_; }
    int32_t Clock() const { return clock_; }
    int WarpTown() const { return menu_.WarpTown(); }
    int WarpFrames() const { return menu_.Frames(); }
    // The field left (a door, a warp, the ending over): its animated pictures back as their files have them, for the
    // races that share them.
    void Leave() { objects_.Restore(d_); }

    // The ending's scenes (field_ending.cpp): func_0026CA48(scene) for scenes 0-9, the towns 22-31. Field_Setup of
    // the scene's town (Cloud Hill's course for scene 0, else a world block) and Field_CreateTasks' ending mode
    // (arg 1): the player's car driven by the AI on the scene's path, the town's cars standing, Camera_FixedTask,
    // and no HUD, radio, coins, doors, talks or menu. Ending_Start begins the loads.
    static const int kEndingScenes = 10;
    void Ending_Start(int scene);
    // One frame of the scene, drawn into a frame already begun. False (nothing drawn) while its loads are still on;
    // the first true frame is func_0026CA48's release (the clock set from the scene's table, its fade-in begins).
    bool Ending_Update();

private:
    // func_00259138: the car inside a door of a town on its block, no higher than the ground at the door's way
    // out: that town and place, and true.
    bool Field_CheckDoors();
    // func_002207D0: by night the headlight beams and the frame darkened but for them.
    void Field_DrawNight(const gfx::Camera3D& camera);
    void Perf_Place() const;
    int Block() const { return FieldWorld::Field_BlockAt(body_.fixed[0], body_.fixed[2]); }
    // info+0x54, the block the doors and coins go by: the car's, or on Cloud Hill block 64 (the course's).
    int TownBlock() const { return world_.OnCourse() ? FieldWorld::kCourseBlock : Block(); }
    // func_0022E718, whenever the field hands over (a door, a talk, the menu): the car's odometer into the save.
    void Field_SaveDistance();
    // func_002182B0's start at `place` of `town`: its block, the position in it (y before the drop to the ground), the
    // yaw, and the world fixed point (x, z).
    static int Field_StartPoint(int town, int place, float* pos, uint16_t* yaw, int32_t* fixed);

    gfx::Draw2D& d_;
    std::string dataDir_;
    FieldWorld world_;
    Cars carModels_;
    CarRecord record_;   // the player's car (D_0177EA50[0]): the save's car 0
    RaceCar car_;
    CarBody body_;
    CarParts parts_;
    ChaseCamera chase_;
    CarSounds sounds_;
    CarEffects effects_;
    RaceLight light_;
    FieldMap map_;
    FieldObjects objects_;
    CourseObjects courseObjects_;   // Cloud Hill's: the A16 gate
    FieldCoins coins_;
    FieldTownCars townCars_;
    MessageTask talk_;   // Adventure_Task's state 8: a town car's talk (info+0x28 flags 0x50000 meanwhile)
    bool talking_;
    StampBook stamps_;   // Adventure_Task's state 10: func_0026D008 after a talk
    bool stampsOpen_;
    int talkTown_, talkPlace_;   // a bump's talk (info+0x23, +0x21), -1: none waiting
    std::vector<MapMarker> markers_;   // the map's town cars and coins, this frame
    FieldMenu menu_;
    bool menuOpen_;      // Adventure_Task's state 2: the menu's task runs, the field's are stopped
    int town_, place_;
    int32_t clock_;
    int frame_;
    int mileMail_;   // Adventure_Task +2: the 6000 m stretches the mileage mail has counted
    bool doorsShut_;     // info+0x28 flag 0x400: no door check (func_00259138) until the first camera is done
    bool gateLoaded_ = false;   // courseObjects_ has A16's gate
    MeterState meterState_;     // the meter's count (Meter_Draw's buf+0x1000)

    // The ending's scene: the town's other cars (slots 1 on), the player's car's path (slot 0's) and AI.
    struct EndingCar {
        CarRecord record;
        RaceCar car;
        CarBody body;
        CarParts parts;
    };
    void Ending_LoadCars(int town);
    void Ending_Camera(gfx::Camera3D* camera);   // Camera_FixedTask (0x0021F098)
    std::vector<EndingCar> endCars_;
    AiPath endPath_;
    AiCar endAi_;
    int endScene_ = -1;
    int endAge_ = -1;      // frames since the scene's release; -1 while it loads
    float endFocal_ = 0;   // scene 0's zoom
};

}  // namespace game
