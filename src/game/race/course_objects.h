// A race course's moving parts: the objects a course task turns or moves (Temple Raceway's water wheel, Night Glow
// Raceway's ride and its two rings of panels, the Sliding Door Race's doors, the Obstacle Course's fence) and the
// textures it animates (flowing water, the tide on Lagoon Raceway's beach). A reimplementation of the race's course
// task (Course_ObjectsTask 0x00241490 and the tasks it hands over to), Course_ClearObjects (0x00223410) and
// Course_DrawObjects (0x002232E8). The objects are data/models/<COURSE or ACTION>/<file>/objN.obj (tools/modelconv);
// the textures' copies work on the course's texture set. See docs/race.md, "Course objects".
#pragma once

#include <stdint.h>

#include <string>
#include <vector>

#include "../../gfx/draw2d.h"
#include "../../gfx/model.h"
#include "car_physics.h"
#include "race_light.h"
#include "tex_anim.h"

namespace game {

// What a course task sees of the race each frame: the cars (D_0177AC50, in their order there; `out` for those out
// of the race, +0x198 = 0), whether it's 2 Player (pad +0x0A & 2), and race flags 0x48 (the replay, the race over),
// which keep the doors quiet.
struct CourseCars {
    CarBody* bodies;   // A16's gate turns the cars back
    const bool* out;
    int count;
    bool twoPlayer;
    bool quiet;
};

// Course_DrawObjects' style (D_0029D240's state, the cars' distance cue), which the field's objects draw with too;
// the lighting is the caller's (Light_CarSet(light, 0)).
gfx::DrawStyle Course_ObjectStyle(const RaceLight& light);

class CourseObjects {
public:
    // Course `id`'s object models and its texture animations' pictures, as the course file has them. Once per
    // course; Course_ClearObjects then readies each race.
    void Load(gfx::Draw2D& draw, const std::string& dataDir, int id);

    // A race's start: the PS2 creates a new Course_ObjectsTask every race, whose first frame runs Course_ClearObjects
    // (0x00223410). No objects placed, the task's frame count and Lagoon's tide back to 0, and the animated
    // textures back to the course file's pictures. (Whether the PS2 uploads them again for a rerun isn't checked;
    // the task's first frame copies over them either way.)
    void Course_ClearObjects();

    // One frame of the course's task: Temple_ObjectsTask (0x0023E140), NightGlow_ObjectsTask (0x0023E508),
    // Lagoon_WaterTask (0x0023ECA0), SlidingDoor_DoorTask (0x00241F68) for each of course 0x13's seven doors, or
    // Obstacle_ObjectsTask (0x0023EE70). Courses without one have nothing to do.
    void Update(const CourseCars& cars);
    // A16's gate holds the player's car while it opens (car flag 0x800, which the task sets and clears).
    bool HoldPlayer() const { return holdPlayer_; }
    // Another task's object: matrix `i` of D_0177EB10 (rows of a 3 x 4, the columns the object's axes and place).
    // Soccer's ball is A08's object 0.
    void SetObject(int i, const float rows[12]) { Place(i, rows); }
    // Another task's copy (the Which-way? Race's): row `row` of the course's copies, from the source's (sx, sy).
    void CopyTile(int row, int sx, int sy) {
        if (draw_ && row < (int)animations_.size()) Copy(*draw_, animations_[row], sx, sy);
    }
    // A task leaving matrix i undrawn (its [3][3] at 0): the port keeps placed matrices from frame to frame.
    void HideObject(int i) { placed_[i] = false; }
    // Barrel Dodging's first frame copies model 0 and its TEX0 (D_0177F310 + 0x270, D_0177EA30) over models 1-3, so
    // all 32 of its matrices draw the barrel.
    void ShareModel0() { modelOf_[1] = modelOf_[2] = modelOf_[3] = 0; }
    // A task's own TEX0 for model 0 (D_0177EA30[0]): Golf's frame 1 gives the ball A05's picture 17 (the dimples) in
    // place of 18. `number` is the picture's number in the course file (only Golf's course uses it).
    void SetModel0Picture(int number);

    // Course_DrawObjects (0x002232E8): each placed object, model (its number & 3), lit as the cars are by the
    // course's lights and colour set 0, with the cars' distance cue.
    void Course_DrawObjects(gfx::Draw2D& d, const gfx::Camera3D& camera, const RaceLight& light) const;

private:
    static const int kModels = 4;      // D_0177F310 + 0x270: a course file's object sections
    static const int kInstances = 32;  // D_0177EB10: placed while [3][3] is set

    void Place(int i, const float rows[12]);
    // The copy's source corner (sx, sy) is written into the destination's top left w x h.
    void Copy(gfx::Draw2D& d, TexAnimation& a, int sx, int sy) { Tex_Copy(d, a, sx, sy); }
    void Temple_ObjectsTask(gfx::Draw2D& d);
    void NightGlow_ObjectsTask(gfx::Draw2D& d);
    void Lagoon_WaterTask(gfx::Draw2D& d);
    void Lagoon_TideStep() { game::Lagoon_TideStep(tide_, ebb_); }   // 0x0023E018
    void SlidingDoor_DoorTask(const CourseCars& cars, int door);
    void Obstacle_ObjectsTask(gfx::Draw2D& d, const CourseCars& cars);
    void Course_ScrollTask(gfx::Draw2D& d);   // C03 0x00241688, C04 0x00241760, C10 0x0023E7E0, C12 0x0023E8D0,
                                              // the Highway Race 0x00241840, A13 0x0023EFB0
    void C13_ObjectsTask(gfx::Draw2D& d);     // 0x0023E9C0
    void A16_GateTask(const CourseCars& cars);   // 0x0023F0A0
    void A19_WaterTask(gfx::Draw2D& d);       // 0x0023F398

    gfx::Model models_[kModels];
    int modelOf_[kModels] = {0, 1, 2, 3};   // the model matrix i & 3 draws (ShareModel0)
    const gfx::Texture* textureOf_[kModels] = {};   // a picture in place of the model's own (SetModel0Picture)
    std::string textureFile_;          // model 0's own picture's file (Golf's course)
    float place_[kInstances][12];
    bool placed_[kInstances];
    std::vector<TexAnimation> animations_;
    gfx::Draw2D* draw_ = nullptr;
    int id_ = -1;
    int frame_ = 0;                    // the task's +8: frames since the race was set up
    int16_t tide_ = 0;                 // Lagoon_WaterTask +2
    bool ebb_ = false;                 // D_00335708: the tide going out
    uint8_t doorState_[13] = {};       // each door task's +0x26: 0, 1 a player's car coming, 2 going through
    float swing_[3] = {};              // C13's angles (D_017526F0), degrees
    int16_t gate_ = 0;                 // A16_GateTask +2: 0 closed, 360 open
    bool holdPlayer_ = false;
};

}  // namespace game
