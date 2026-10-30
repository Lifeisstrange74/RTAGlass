// The field's map in the bottom left: the world's map models seen from above round the player's car, inside a frame,
// with the car's arrow. A high-level reimplementation of func_00242FE8 (the field HUD's map task, made by
// func_00210430), which draws the frame (func_00249208), the loaded blocks' maps (func_00242438) and the markers, and
// switches between a near and a far view on the map key (Select). See docs/hud.md.
//
// The town cars' green squares (D_00290940, for each town car on a loaded block) and, with the Coin Radar (item 0x2D
// of kind 0xF, Save_HasItem), the coins' yellow ones (func_00241E90, every other 32 frames) come from the caller.
//
// The frame's lamps blink (palette 11 for 32 frames of each 64) while there's unread mail, once the player has My
// Garage (stamp 40).
#pragma once

#include <stdint.h>

#include <vector>

#include "../../gfx/draw2d.h"
#include "../hud/minimap.h"
#include "field_world.h"

namespace game {

class FieldMap {
public:
    // A new field's map task: the near view (Task_Create clears +0x25).
    void Reset() { zoom_ = 0; }
    // The map key was pressed: the other view (func_00242FE8's +0x25).
    void Toggle() { zoom_ ^= 1; }

    // The frame, the maps of `world` round `pos` (block `block`'s units; the player's car's position) and the car's
    // arrow turned by `yaw` (radians), coloured for the time of day `clock`, then `markers` (in the same units).
    // `frame` is the map task's age (+0x08), which blinks the lamps.
    void Draw(gfx::Draw2D& d, const FieldWorld& world, int block, const float* pos, float yaw, int32_t clock,
              const std::vector<MapMarker>& markers, int frame) const;
    // Q's Rally's title, DESTINATION and TIME (FieldMap_Task, while race flag 0x1000 is up).
    static void Map_DrawRally(gfx::Draw2D& d);

private:
    int zoom_ = 0;
};

}  // namespace game
