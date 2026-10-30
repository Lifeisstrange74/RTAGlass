// My Garage (town 0): the player's home, a high-level reimplementation of func_0022D1E0 (the entry task) and
// func_0022CD38 (the scene). A 3D room (SHOP/GARAGE.BIN, one of ten by the save's interior, +0x652) seen by a fixed
// camera, the player's car driving in, sprites over it (a trophy for each town race won, the Grand Prix picture, the
// story items on show) and a menu: Check E-mail (the mail reader), Redecorate (the interiors owned), Go play (straight
// into a game's building found so far) and Go outside. Its first visit, from Wonder Realty, gives stamp 40, the
// first interior and three mails. See docs/buildings.md, "My Garage".
#pragma once

#include <string>
#include <vector>

#include "../../gfx/draw2d.h"
#include "../../gfx/model.h"
#include "../../platform/input.h"
#include "../race/car.h"
#include "../race/car_physics.h"
#include "../race/race_light.h"
#include "../ui/window.h"
#include "mail_screen.h"

namespace game {

class GarageScene {
public:
    GarageScene(gfx::Draw2D& draw, const std::string& dataDir);

    void Start();
    // One frame (BeginFrame to EndFrame). Returns 0 while it runs, then kDriveOut (Go outside: the field at the
    // garage's door) or kGoTo (Go play: the building Destination() names).
    enum { kDriveOut = 10, kGoTo = 11 };
    int Update(const platform::Input& input);
    void Destination(int* town, int* place) const {
        *town = goTown_;
        *place = goPlace_;
    }

private:
    // func_0022CD38's sub-states (+0x25).
    enum { kDriveIn = 0, kMenu = 1, kRedecorate = 2, kGoPlay = 3, kMail = 4 };

    void Garage_LoadRoom();
    void Garage_PlaceCar(float x, float z);
    void Garage_Draw();
    void Garage_DrawSprites();
    void Garage_OpenList(int kind);

    gfx::Draw2D& d_;
    std::string dataDir_;
    gfx::Camera3D camera_;
    RaceLight light_;
    gfx::Model room_, roomB_;
    int roomShown_;     // the interior loaded
    Cars cars_;
    RaceCar car_;
    CarBody body_;
    CarRecord record_;
    Window menu_, list_;
    std::vector<const char*> listNames_;
    std::vector<int> listIds_;   // the list's items: the interiors or D_0029F440's games
    MailScreen mail_;
    int state_;
    int age_;      // task +8
    int ending_;   // func_0022C418's frames, or -1
    int fade_;
    int result_;
    int goTown_, goPlace_;
};

}  // namespace game
