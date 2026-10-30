// The race's cars: who races a Quick Race and from which grid slot (Race_Setup 0x0020F950, Race_CreateCars
// 0x0020ED30, Car_Init 0x00218458), and how the cars are drawn (Cars_Draw 0x002228A8, Car_Draw 0x00221788): the
// body of CARn/Qnn.BIN lit and painted by program 0x20, the tires of CARS/TIRE.BIN and the shadow. The models are
// data/models/CARn/Qnn and CARS/TIRE (tools/modelconv). See docs/cars.md.
#pragma once

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "../../gfx/draw2d.h"
#include "../../gfx/model.h"
#include "car_physics.h"
#include "race_light.h"

namespace game {

// A car record, what D_0177EA50 points at for each car (16 bytes in the game).
struct CarRecord {
    uint32_t paint;     // +0: six 4-bit levels (0.1 + 0.05 n): paint 1 (r, g, b) in bits 0-11, paint 2 in 12-23
    int body;           // +4: the body, CARn/Qnn.BIN (n = body / 30; 150 is in CARS)
    const char* name;   // the driver (+0xC of the opponent table; the player's car: the body's own name)
    int wheelColor = 0; // +5: the tires' palette, 0-11 (a save car's; every driver record's is 0)
    int wheel = 0;      // +6: the Wheels part, 0-14: which of TIRE.BIN's hub tiles the tires show (driver records' 0)
    int billboard = 0;  // +7: the billboard's sponsor, 0-4 (part flag 0x200): which of PARTS.BIN's pictures it shows
};

// A car in the race: its record, grid slot and where it is.
struct RaceCar {
    const CarRecord* record;
    int slot;          // +0x247: the grid slot (0 = the front)
    float pos[3];      // +0x90, on the ground
    float yaw;         // radians: 0 faces +z, pi/2 +x
    const CarBody* physics;   // a driven car's physics (orientation, suspension, wheels); null: it stands level
    int town;          // D_01787008 +0: an opponent's home town (its record's group in D_002C22C8), -1 the player
    int index = 0;     // D_01787008 +1: its record's index in the group (the Adventure's opponents and teammates)
    bool out;          // +0x198 = 0: out of the race (it fell off the course): not driven, drawn, collided or placed
    // 2 Player: the side it races for (car flags 0x40: player 2's), and its D_0177EA50 entry (Race_CreateCars' word
    // bits 5-9, +0x194: 0-2 player 1's car and teammates, 3-5 player 2's), which is also its AI lane.
    int side;
    int entry;
    uint16_t partFlags = 0;   // its part flags (CarParts::flags): Car_Draw draws sections 4 and 5 by 0x2000/0x4000
    // Its lights on (func_00222360's flags): kLightsHead (car +0x1FC), kLightsBrake (+0x1FE).
    uint8_t lights = 0;
};

const uint8_t kLightsHead = 1, kLightsBrake = 2;

// The Ranking's town column (D_002BE438): an opponent's town, or the player's: Peach Town, or My City once stamp 40
// (My Garage) is earned (Race_RankingTask 0x0023794C: Stamp_Earned(0x28)).
const char* Car_HomeTown(const RaceCar& car);

// Race_CreateCars (0x0020ED30) for a Quick Race on course `courseId`: the player's car first (the card's car,
// D_0029C028, in the last slot), then the course's 23 opponents (D_002BE158 + 4) in slots 0-22, except that the
// 4th and 7th, and the 10th and 13th, trade places. Returns the number of cars (24), or 0 for a course that isn't in
// the Quick Race.
int Race_CreateCars(int courseId, RaceCar* out);
// A driver record's name: D_002C22C8[group] + 16 x index, +0xC (the towns' people, the opponents and the teammates
// the saves name by (group, index)); null for none.
const char* Car_DriverRecordName(int group, int index);
// The driver record itself (paint, body, name): the cars of the Adventure's scenes; null for none.
const CarRecord* Car_DriverRecord(int group, int index);
// Where Car_Draw puts `car`: its matrix `m` and its body's `bm` on the suspension (3 x 4, row-major: world = M p).
void CarPlace(const RaceCar& car, bool mirrored, float* m, float* bm);
// Race_CreateCars in 2 Player (Race_Setup with mode & 2): no course opponents. Player 1's car in slot 1 and player
// 2's in slot 0; with `teams`, on a 6-car row, the teammates in slots 0-3 (player 1's first, then player 2's,
// teammate 1 then 2, a missing teammate skipped) and the players behind them in slots 5 and 4. The cars come out
// players first (player 1, player 2), then the teammates; records[entry] (D_0177EA50) is each car's, from the save's
// cars: its body, its paint and the driver's name. Returns the number of cars.
int Race_CreateVersusCars(bool teams, int rowCars, RaceCar* out, CarRecord* records);
// Race_CreateCars in the Adventure (mode 0) for Adventure race row `race` (race_list.h): the player's car in the
// last slot, then with three or more cars the save's teammates (+0x674, +0x676) in slots 0 and 1 (lanes 1 and 2),
// then the row's opponents: the list's last six from the back (slots counting down from cars - 2), then the rest
// from the list's start in the slots after the teammates', until the grid is full. An opponent who is one of the
// player's teammates (Team_Find) is skipped. Every opponent's lane is 3 + its place in the list. The player's and
// the teammates' records[member] (D_0177EA50) are the save's cars, with entry = member (0-2); the opponents have
// entry -1 and town = their group. lanes[i] is each car's +0x194. Returns the number of cars.
int Race_CreateAdventureCars(int race, RaceCar* out, CarRecord* records, int* lanes);
// The attract demo's cars (Race_Setup in mode 8, asm 0x0020FE18): Adventure row `race`'s list entries cars - 2 down
// to 1 as cars 0, 1, ..., each in the grid slot of its entry and lane entry + 3; entry 0 and the player don't race.
// Returns the number of cars (22 for Endurance Run).
int Race_CreateDemoCars(int race, RaceCar* out, int* lanes);
// Team_Find (0x0023DB80): 1 or 2 if (group, index) is the save's teammate 1 or 2, else 0.
int Team_Find(int group, int index);
// The Quick Race card of course `courseId` (the row of D_002BE158), or -1.
int QuickRace_Card(int courseId);
// The player's car on card `card` (D_0029C028: the body, in its own paint).
const CarRecord& QuickRace_PlayerCar(int card);

// The cars' models and drawing.
class Cars {
public:
    static const int kMaxCars = 24;

    Cars();
    ~Cars();
    Cars(const Cars&) = delete;
    Cars& operator=(const Cars&) = delete;

    // Loads the bodies of `cars` (each body once) and the tires.
    void Load(gfx::Draw2D& draw, const std::string& dataDir, const RaceCar* cars, int count);
    // The same while the game runs (the field's town cars, as the loader's job 0x37 brings them in): the bodies not
    // loaded yet are read on a thread of their own, and Cars_Stream puts them on the card a few at a time. One
    // request at a time: a new one waits for the last.
    void Cars_Request(gfx::Draw2D& draw, const std::string& dataDir, const RaceCar* cars, int count);
    // Puts what the thread has read on the card for about `budgetMs`; true once everything asked for is loaded.
    bool Cars_Stream(gfx::Draw2D& draw, double budgetMs);

    // Every car's shadow, then every car, with the race's lighting (Cars_Draw, 0x002228A8), then the cars' lights
    // (RaceCar::lights). Car `hidden` isn't drawn (the driver's view: the game's near plane cuts the player's car
    // away). With `mirrored`, each car is drawn upside down under the ground first, as its reflection
    // (Cars_DrawMirrored, func_00222B70).
    void Cars_Draw(gfx::Draw2D& d, const gfx::Camera3D& camera, const RaceLight& light, const RaceCar* cars,
                   int count, int hidden = -1, bool mirrored = false) const;

    // The Change parts cards' cars (func_00244F60's body, Lights, Wing Set, Options and Sticker cards): `car`
    // standing level, its near body, its parts and tires. With `silhouette` the body and tires are black (the
    // light set D_002A8BB0 those cards use; on the PS2 they show black) and only the parts of flags 0x1FF keep their
    // colours.
    // `close`: as Car_Draw at distance 0 (func_002236D8: the photos, the 2 Player cards), light colour set 0 and the
    // close-up WHEEL.BIN wheels instead of the tires.
    void Car_DrawCard(gfx::Draw2D& d, const gfx::Camera3D& camera, const RaceLight& light, const RaceCar& car,
                      bool silhouette, bool close = false) const;
    // The parts screen's Wheel card (func_00244F60, category 7): WHEEL.BIN's hub of Wheels item `item` alone at the
    // origin, both paints the wheel colour `wheelColor` (the edited car's, SaveCar +5).
    void Car_DrawWheelCard(gfx::Draw2D& d, const gfx::Camera3D& camera, const RaceLight& light, int item,
                           int wheelColor) const;

private:
    struct Body {
        gfx::Model model[6];   // the file's sections: 0 near, 1 far, 2/3 by part flag 0x1000, 4/5 by flags 0x2000
                               // (the jet turbines) and 0x4000, in CARS/PARTS.BIN's picture
        gfx::Model shadow;
        gfx::Model head[2];    // the lights: chain A (the headlights) of sections 0 and 1
        gfx::Model brake[2];   // chain B (the brake lights) of sections 0 and 2
    };

    // 0x00221788. `distance` is the car's view depth over the focal length; `nearBodies` counts the near bodies drawn.
    // Returns the body section drawn (0 near, 1 far), or -1.
    int Car_Draw(gfx::Draw2D& d, const gfx::Camera3D& camera, const RaceLight& light, const RaceCar& car,
                 float distance, int& nearBodies, bool mirrored = false) const;
    // func_00222360: the car's lights by RaceCar::lights, for the body `section` Car_Draw drew.
    void Car_DrawLights(gfx::Draw2D& d, const gfx::Camera3D& camera, const RaceLight& light, const RaceCar& car,
                        int section, bool mirrored) const;
    // Car_Draw's parts (CARS/PARTS.BIN, flags 0x1FF) with the body's style `st`; `far`: the propellers' far model.
    void Car_DrawParts(gfx::Draw2D& d, const gfx::Camera3D& camera, const RaceCar& car, gfx::DrawStyle st,
                       bool far) const;
    // Part flag 0x400's lift (model 10, but for a reflection) and big tires, in the car's frame `m`, with the
    // tires' style `st`.
    void Car_DrawBigTires(gfx::Draw2D& d, const gfx::Camera3D& camera, const RaceCar& car, gfx::DrawStyle st,
                          const float* m, bool mirrored) const;
    // func_00221570, Car_Draw at distance exactly 0 without the big tires: each wheel WHEEL.BIN's hub of the car's
    // Wheels part, then its tire, in the wheel colour, in the car's frame `m`.
    void Car_DrawCloseWheels(gfx::Draw2D& d, const gfx::Camera3D& camera, const RaceCar& car, gfx::DrawStyle st,
                             const float* m) const;

    static gfx::Model& BodyModel(Body& b, int n);   // a body's model n of the files' order (car.cpp)
    std::map<int, Body> bodies_;   // by body number
    struct Reading;                 // Cars_Request's thread and what it has read (car.cpp)
    std::unique_ptr<Reading> reading_;
    static const int kParts = 12;
    gfx::Model parts_[kParts];     // CARS/PARTS.BIN's models (D_0177EAC8)
    gfx::Model tires_[6];          // CARS/TIRE.BIN sections 0-2: front left, front right, the rear pair; 3: all four;
                                   // 4 and 5: the big tires, left and right
    gfx::Model wheels_[16];        // CARS/WHEEL.BIN (D_0177AB30): 0 the tire, 1 + n the hub of Wheels item n
};

}  // namespace game
