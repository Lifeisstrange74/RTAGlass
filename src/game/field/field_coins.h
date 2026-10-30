// The field's 100 ChoroQ coins: each lies in a world block, spins where it lies and is picked up by driving through it.
// A high-level reimplementation of Field_CheckCoins (0x00241BA8, a task of func_00210430's: the pickups), Coins_Draw
// (0x00241D38, Field_DrawView's step after the cars) and Coins_MapDots (0x00241E90, the field map's yellow dots with
// the Coin Radar). Which coins are still out is the save's (+0x548, Pickup_Taken), which the bars' hints and My City's
// Coine read too. See docs/field.md, "Coins".
#pragma once

#include <vector>

#include "../../gfx/draw2d.h"
#include "../../gfx/model.h"
#include "../hud/minimap.h"
#include "../race/race_light.h"
#include "field_world.h"

namespace game {

struct FieldCoin {
    float pos[3];   // in its block's units
    int block;      // the world block (FieldWorld ids)
};
const FieldCoin& Field_Coin(int i);   // D_002A3970[i], 0-99

class FieldCoins {
public:
    // SYS/COIN.BIN's model (loaded at boot by func_0020B348; models/SYS/COIN/0.obj).
    void Load(gfx::Draw2D& d, const std::string& dataDir);

    // Field_CheckCoins: each coin still out in the car's block `block` within 1.4 (D_00332EEC) of `pos` (the car's
    // place in it) is taken: SE 0x29 and its bit cleared; the one that leaves 70 sends mail 0x7E.
    void Field_CheckCoins(int block, const float* pos);
    // Coins_Draw: the coins still out in the loaded blocks, seen from block `block`, each turned about the vertical by
    // the view's frame count (a turn every 256 frames), lit as the field's objects are.
    void Coins_Draw(gfx::Draw2D& d, const gfx::Camera3D& camera, const RaceLight& light, const FieldWorld& world,
                    int block, int frame, float reach);
    // Coins_MapDots: the field map's markers for the coins still out in the loaded blocks, in block `block`'s units.
    static void Coins_MapDots(const FieldWorld& world, int block, std::vector<MapMarker>& out);

private:
    gfx::Model model_;
    gfx::DrawStyle style_;
    gfx::Lighting lighting_;
};

}  // namespace game
