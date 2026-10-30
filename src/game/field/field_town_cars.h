// The towns' own cars on the field: the people who aren't in a building drive round their town on loops of their
// own, and bumping into one starts a talk. A high-level reimplementation of Field_TownCarsTask (0x0020E708) with the
// town cars' Car_Init (0x00218458: the start in D_002C2E78, the speed cap) and Ai_Init (Ai_TownPath 0x0025FBC0: the
// loop in D_002D98A8), driven by Ai_Drive as the race's opponents are; Cars_BumpSound's field branch (0x00217830)
// tells who was bumped. Only one town's cars are out at a time: the first town (by number) with cars whose block is loaded.
// They come back to their starts each time that block comes in. The paths are data/ai/FLD/Tnn_cc.txt
// (tools/modelconv). See docs/field.md, "Town cars".
#pragma once

#include <stdint.h>

#include <string>
#include <vector>

#include "../../gfx/draw2d.h"
#include "../hud/minimap.h"
#include "../race/ai.h"
#include "../race/car.h"
#include "../race/car_physics.h"
#include "field_world.h"

namespace game {

// D_0029CBA0: the parts record Car_Init gives every town car (+0x180): tires 7, steering 2, the rest the first.
extern const uint8_t kTownCarParts[16];

class FieldTownCars {
public:
    explicit FieldTownCars(const std::string& dataDir) : dataDir_(dataDir) {}

    // Field_TownCarsTask's first frame for the field of `town` (My Garage's is My City's): that town's cars, once
    // `models` has their bodies.
    void Start(gfx::Draw2D& d, Cars& models, int town);
    // The models the first town's cars need, read meanwhile (the field's loading screen).
    void Preload(gfx::Draw2D& d, Cars& models, int town);

    // One frame of the task and the cars' (Car_Input with Ai_Drive, Car_Update): `models` gets the bodies of a town
    // that comes in. `clock` is the time of day, which some people's presence goes by (func_0023CD58) and the
    // headlights. Nothing moves while `frozen` (a talk: info+0x28 flag 0x40000).
    void Update(gfx::Draw2D& d, Cars& models, const FieldWorld& world, int32_t clock, bool frozen);

    // The cars out, for Cars_Collide (after the player's car) and Cars_Draw (in block `block`'s units).
    int Count() const { return (int)cars_.size(); }
    CarBody& Body(int i) { return cars_[i].body; }
    int Index(int i) const { return cars_[i].index; }
    int Town() const { return town_; }
    void Cars_Place(int block, RaceCar* out) const;

    // Cars_BumpSound's field branch, for a bump between the player's car and town car `i`: true (and the person's
    // (town, place) for the talk) if the car wasn't already standing after a bump; either way it stands 180 frames.
    bool Cars_Bumped(int i, int* town, int* place);

    // func_00239508's Yes and a join: each car out as the team now has it (a teammate drives the save's car, anyone
    // else their own record's).
    void Cars_Refresh();

    // FieldMap_Task's squares: every car out, in block `block`'s units.
    void Cars_MapDots(int block, std::vector<MapMarker>& out) const;

    // Town_Absent (0x0023CD58): person `index` of `town` isn't out at `clock` (also the test menu's place lists).
    static bool Town_Absent(int town, int index, int32_t clock);

private:
    // The task's +0x25: 0 looking for a town whose block is in, 1 its cars' bodies loading, 2 the cars out (until the
    // block goes). (4, the first frame's wait for the block, is 1 here: the bodies come first.)
    enum State { kScan = 0, kLoading = 1, kOut = 2 };
    struct TownCar {
        CarRecord record;
        RaceCar car;
        CarBody body;
        CarParts parts;
        AiPath path;
        AiCar ai;
        int index;          // the driver record (D_002C22C8[town] + 16 x index)
    };

    bool Field_TownCarsLoad(int town);            // the town's records and paths
    void Cars_RequestModels(gfx::Draw2D& d, Cars& models, int town);
    void Field_TownCarsCreate(int32_t clock);     // the Car_Init loop

    std::string dataDir_;
    State state_ = kScan;
    int town_ = -1;          // +0x27: the town whose cars are (or are being made) out
    std::vector<TownCar> cars_;   // the town's cars out
};

}  // namespace game
