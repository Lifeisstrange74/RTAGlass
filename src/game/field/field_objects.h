// The field's moving scenery: the models in the world blocks' objects sections (FieldWorld::Field_Object) as the
// field's view places them each frame. A reimplementation of func_0025D028, which Field_DrawView (func_00220938)
// calls after the cars: on the view's first frame it sets the palms' fronds swinging, then each frame it runs the
// handler of D_002C4AD0 for each of its blocks that is loaded. Of those handlers this has the scenery: block 39's
// windmill sails (func_0025D5F8) and the palm fronds of blocks 40 and 42 on the way to Fuji City (func_0025CC18,
// func_0025CE20); and the things to pick up on the roads (Jones's seven gemstones and four of the story's items) with
// block 6's night light; Peach Town's big peach and Tim's wallet (func_0025BE98); My City's three objects (block 11,
// func_0025C088); Fuji City's offering box (block 23, func_0025D2C8, with its block task); and town 12's UFO (block
// 22, func_0025D328, with its block task 0x00240228). Also Field_ObjectsTask's (0x002412E0) tide and the blocks'
// texture scrolls (D_002A3800's handlers: the water, the surf, the UFO's lights). See docs/field.md, "Objects".
#pragma once

#include <stdint.h>

#include <string>
#include <vector>

#include "../../gfx/draw2d.h"
#include "../race/car_physics.h"
#include "../race/race_light.h"
#include "../race/tex_anim.h"
#include "field_world.h"

namespace game {

class FieldObjects {
public:
    // func_0025D028 on the view's first frame (its task's +8 = 1): every frond's starting angle and way.
    void Reset();
    // func_0025D028 after that: the loaded blocks' handlers, in the frame seen from block `block` (the player's
    // car's). `frame` is the view task's frame count (+8), which turns the sails. Objects past `reach` from the eye
    // (the distance fade's) are left out.
    // `carPos` is the player's car's place in its block, which picks the items up, and `clock` the time of day.
    void Field_DrawObjects(gfx::Draw2D& d, const gfx::Camera3D& camera, const RaceLight& light,
                           const FieldWorld& world, int block, int frame, float reach, const float* carPos,
                           int32_t clock);
    // Peach_FallTask (0x002406F8), block 0x2B's task, each frame while that block is loaded (it starts over each
    // time the block loads): the player's car in block `block` bumping the big peach tree at 10 km/h or more
    // (`speedKmh`, Vec_SpeedKmh) drops the peach into the river, which carries it out to sea; the car at `carPos`
    // catching it on the way gives item 15/0x2F (James's peach).
    void Peach_Step(const FieldWorld& world, int block, const float* carPos, float speedKmh);
    // Fuji City's block task (func_00240CD8), each frame while block 23 is loaded: the offering box in front of the
    // shrine, over Otomi's door (Fuji City place 10). It keeps the cars out of its place; until stamp 17 (Otomi saved)
    // the player's car stopping in front of it by night (not during Q's Rally) is held while it slides back, opening
    // the way to her door. `cars[0]` is the player's (in block `block`; -1 while block 23 isn't loaded), then the town
    // cars; `clock` the time of day.
    // `guard` is Fuji City's Guard's car (the first town car), or null.
    void MoneyBox_Step(int block, int32_t clock, CarBody* const* cars, int count, CarBody* guard);
    // The player's car is held while the box slides (car +0x198 bit 0x800: Car_Input keeps the steering and holds
    // the brake, 0x0021AC54).
    bool CarHeld() const { return boxState_ == 1; }
    // Town 12's UFO's block task (0x00240228), each frame: `loaded` block 22, `clock` the time of day. It comes down
    // over its pad at 17:00, circles it until 17:57, hangs over its door all night and leaves at 6:30.
    void Ufo_Step(bool loaded, int32_t clock);

    // Field_ObjectsTask's texture copies: Load reads them once (the pictures from textures/FLD/<abc>.BIN.txt); each
    // frame Field_Scrolls steps the tide (Lagoon_TideStep) and runs D_002A3800's handlers of the loaded blocks by
    // `frame` (the task's count) or the tide; Restore puts the pictures back (they're shared with the races').
    void Load(gfx::Draw2D& d, const std::string& dataDir);
    void Field_Scrolls(gfx::Draw2D& d, const FieldWorld& world, int frame);
    void Restore(gfx::Draw2D& d);

private:
    static const int kPalms40 = 107, kPalms42 = 6, kFronds = 3;
    struct Frond {
        float angle;   // degrees about x, 0 down to -swing and back
        bool back;     // bit 0x80 of its byte: swinging back up
    };
    // func_0025CC18 and func_0025CE20: each palm's three fronds swing (if `swing`) and are drawn, objects 0-2 of
    // `objBlock`.
    void Palm_Draw(gfx::Draw2D& d, const gfx::Camera3D& camera, const FieldWorld& world, int block, int objBlock,
                   const float (*places)[3], Frond (*fronds)[kFronds], int count, float reach);
    // One object of block `objBlock` placed by `rows` (in its block's units), if it's in reach of the eye.
    void Object_Draw(gfx::Draw2D& d, const gfx::Camera3D& camera, const FieldWorld& world, int block, int objBlock,
                     int n, float* rows, float reach);

    Frond palms40_[kPalms40][kFronds];   // D_017608D0 (angles) and D_01760E20 (ways)
    Frond palms42_[kPalms42][kFronds];   // D_01760DD8 and D_01760F68
    gfx::DrawStyle style_;
    gfx::Lighting lighting_;

    // The peach (Peach_FallTask's state and D_002A2BC0, which keeps its place between the task's runs).
    bool peachTask_ = false;                       // the task is running (block 0x2B loaded)
    int peachState_ = 0;                           // D_0033573A: 0 on the tree, 1 falling, 2 floating, 4 sinking
    float peach_[3] = {562.4f, 44.04f, 1495.0f};   // D_002A2BC0, in block 0x2B's units
    float peachFall_ = 0;                          // D_0033570C: the fall a frame
    int peachBob_ = 0;                             // the task's +0x20: the bob, 0-60 and back
    bool peachBobDown_ = true;                     // +0x27: the bob counting down
    bool peachAcross_ = true;                      // +0x26: drifting to -z (else +z)

    // The offering box (D_00335739 its state: 0 shut, 1 sliding back, 2 open; D_002A2BD8 its z).
    bool boxTask_ = false;
    int boxState_ = 0;
    float boxZ_ = 1003.0f;

    // The UFO (D_00335738 its state; D_002A2BB0 its place in block 22's units; the task's +0x1C, +0x20 and +0x27).
    bool ufoTask_ = false;
    int ufoState_ = 7;
    float ufo_[3] = {900.0f, 25.0f, 300.0f};
    int ufoX_ = 0, ufoZ_ = 180;
    uint8_t ufoWay_ = 5;

    // The texture copies, one per destination picture (the blocks that share one copy it alike), and the task's
    // tide (+2) and D_00335708.
    struct FieldScroll {
        uint64_t blocks;   // the blocks that write the picture, a bit each: it's copied while any is loaded
        int kind;          // 0 by the frame, 1 by the tide, 2 block 22's lights (event flag 0x50)
        int row;           // block 22's row
        TexAnimation anim;
    };
    std::vector<FieldScroll> scrolls_;
    bool scrollsLoaded_ = false;
    int16_t tide_ = 0;
    bool ebb_ = false;
};

}  // namespace game
