// The Adventure's building scenes, where the cars talk: a high-level reimplementation of the scene task
// Shop_SceneTask (0x0022AEB0) as Room_Start (func_00211708 and func_0022C518) sets it up. A room is a pre-rendered
// picture from SHOP/Tnn.BIN over a tiled 3D floor, with the player's car (D_0177EA50[0]) and the place's person
// (D_0177EA5C) driving on it, seen by a fixed camera, and Message_Task (message_task.h) for what they say. So far
// this is what the opening (town 8, place 9: the president's office), Q's Factory (place 0 of towns 1-9: the car
// drives onto the turntable, which turns it to face the camera; Change parts, for which a teammate's car can take its
// place, the race list and the save point), the bars (the car drives in and stands while the bartender talks), the
// towns' shops and the Paint Shops (the turntable again, with the paint screen) use. See docs/new_game.md.
#pragma once

#include <string>

#include "../../gfx/draw2d.h"
#include "../../gfx/model.h"
#include "../../platform/input.h"
#include "../race/car.h"
#include "../race/car_physics.h"
#include "../race/race_light.h"
#include "message_task.h"
#include "paint_screen.h"
#include "parts_screen.h"

namespace game {

class RoomScene {
public:
    RoomScene(gfx::Draw2D& draw, const std::string& dataDir);

    // Whether (town, place) is a building: each town's Q's Factory (place 0 of towns 1-9) and every other place
    // of towns 1-21 with a talk (all of them; false only without the message data).
    static bool Room_Open(int town, int place);

    // The scene of `place` in `town`, with `you` as the player's car (slot 0) and the place's driver record as the
    // person (slot 3). `partFlags` are the player's car's (SaveCar::partFlags).
    void Start(int town, int place, const CarRecord* you, uint16_t partFlags = 0);
    // One frame (BeginFrame to EndFrame). Returns 0 while it runs, else its result for the parent once it's over:
    // kDriveOut (the car drove out: on to the town), kQuit (quit the game for today) or kRace (enter Race()).
    int Update(const platform::Input& input);
    // kGoTo: a talk's code 0x14 named another place (Destination()). kEnding: code 0x18, the ending (story/ending.h,
    // the full one in Cloud Hill, else the cinema's replay); after it, Room_AfterEnding.
    enum { kRace = 4, kQuit = 6, kDriveOut = 10, kGoTo = 11, kEnding = 15 };
    // Shop_SceneTask's state 3 after the ending: true, the scene goes on (the cinema's talk again, once president);
    // false, it's over as kDriveOut (ExitPlace, TakeClock) without the drive-out, stamp 100 queued.
    bool Room_AfterEnding();
    // The time of day the scene set (the ending's 54000), once; -1 none.
    int TakeClock() {
        int c = newClock_;
        newClock_ = -1;
        return c;
    }
    // kDriveOut: where the car comes out on the field.
    void ExitPlace(int* town, int* place) const {
        *town = leaveTown_;
        *place = leavePlace_;
    }
    void Destination(int* town, int* place) const {
        *town = goTown_;
        *place = goPlace_;
    }
    // Room_SceneKind (func_0022AD70): 0 an ordinary place, 1 Parts Shop, 2 Body Shop, 3 Paint Shop, 4 My City's
    // Recycle Shop, 5 Snow Bank.
    static int Room_SceneKind(int town, int place);
    // The picture of `place`'s room in `town` (SHOP/Tnn.BIN), whose corner also holds the parts cards' art.
    static const gfx::Texture* Room_Picture(gfx::Draw2D& d, int town, int place);
    // The race the talk picked (info+0x20, an AdventureRace), for kRace.
    int Race() const { return message_.Race(); }
    // The time of day the talk's codes test (info+0x14).
    void SetClock(int32_t clock) { message_.SetClock(clock); }

private:
    // A car on the room's floor (D_017524B0 + 0x90 n): CarBody's layout from +0x00 to +0x90, and +0x8C its speed,
    // +0x8E its heading.
    struct RoomCar {
        RaceCar car;
        CarBody body;
        int16_t speed;     // +0x8C: 12800 a unit a frame
        int16_t heading;   // +0x8E
    };

    void Room_PlaceCar(RoomCar& c, const float* place, float yaw, int16_t speed, int16_t heading);
    bool Room_CarPath(RoomCar& c, int path, int frame);    // func_0022A408
    bool Room_TurnCar(RoomCar& c, int facing);              // func_0022A6B8's turntable steps
    void Room_StickTurn(const platform::Input& input);      // state 2: the stick turns the turntable
    void Room_CarRequest();                                 // func_0022ABA0
    void Room_SetTeamCar(int car);                          // team_[car] from the save's car
    void Room_LoadCars();
    bool Room_HasTeam() const;                              // Q's Factory or a Paint Shop: the team's cars
    void Room_DriveCars();                                  // func_0022A6B8 (the opening's and the factory's states)
    void Room_SetCarMatrix(RoomCar& c, float yaw);
    void Room_Draw();
    void Room_DrawFloor(const gfx::Camera3D& cam);

    gfx::Draw2D& d_;
    std::string dataDir_;
    int town_, place_;
    const gfx::Texture* picture_;
    gfx::Camera3D camera_;
    RaceLight light_;
    Cars cars_;
    RoomCar you_, person_;
    RoomCar mate_;   // slot 2 (D_017524B0 + 0x120): a teammate's car, on the turntable for its parts
    int mateCar_;    // +0x20: which car of the team it is (1 or 2)
    CarRecord team_[3];   // the Adventure's cars (D_0177EA50) as the save has them (Room_HasTeam)
    MessageTask message_;
    PartsScreen parts_;
    bool inParts_;   // state +0x25 = 2: the parts window is open
    bool inShop_ = false;   // a shop's screen (Message_Task's func_00269E88, which parts_ runs) is open
    PaintScreen paint_;     // state +0x25 = 2 for scene result 8: the paint screen (PaintShop_Task)
    bool inPaint_ = false;  // state 2 with paint_: from the car pick until the screen tells the scene it's done
    int age_;        // the scene task's +0x08: frames since it started (or since the last state change)
    int carState_;   // +0x27
    int request_;    // +0x02: what the scene asks of the cars: 1 the player's car onto the turntable, 2-3 that
                     // teammate's, 4 drive out
    int turn_;       // +0x26: the turntable's angle, 1/256 of a half turn
    int fade_;       // frames since the scene started, for the fade in
    int ending_;     // func_0022C418's frames, or -1
    bool talked_;     // +0x25 != 0: the talk has started (state 0 starts it at age 0x41)
    int parentResult_;   // what the scene tells its parent (the parent's +0x04)
    int goTown_ = 0, goPlace_ = 0;   // kGoTo's place
    int leaveTown_ = 0, leavePlace_ = 0;
    int newClock_ = -1;   // kDriveOut's: this building's door, or pre-code 0x14's place
    int endingOut_ = -1;  // state 3's frames before the ending, -1 none
};

}  // namespace game
