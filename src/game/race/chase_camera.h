// The race's chase camera: a reimplementation of Camera_ChaseTask (0x0021E690) for one player, with the views it
// takes from D_0029CE00, the tilt follower (func_0021DC18, Camera_FollowTilt) and the swing away from walls
// (func_0021E070, Camera_KeepOutOfWalls). See docs/race.md, "The camera".
//
// The camera sits in a frame on the player's car: the car's heading, tilted by its up vector (the slope's pitch
// followed slowly, the body's lean on its springs taken as it is). In that frame the view's eye is turned from the
// preset's place by the camera's pitch, and by its yaw: the swing (+0x16) plus the turn (+0x1A, the car's slide
// angle the other way, so the camera looks where the car goes). The camera button (Triangle) switches between the
// chase view (0) and the driver's view (1).
#pragma once

#include <stdint.h>

#include "../../gfx/model.h"
#include "car_physics.h"
#include "collision.h"

namespace game {

class ChaseCamera {
public:
    // The task's first frames: the follower cleared and the chase view (D_0029CE00[0]). `split` is RaceView_Init's
    // mode: 0 the whole screen; 1 the top half and 2 the bottom (2 Player's players 1 and 2), with the split
    // screen's views (D_0029CE00[2], [3]) centred in their half. `soccer`: course 0x1A's chase views (8, 9).
    // `bigTires`: the car has part flag 0x400, whose views are 4 on (higher, and looking down more).
    void Reset(int split = 0, bool soccer = false, bool bigTires = false);
    // The car's part flag 0x400 changed (a part fitted in a talk); the next view taken goes by it.
    void SetBigTires(bool bigTires) { bigTires_ = bigTires; }

    // One frame of Camera_ChaseTask for the car `car` at `pos` (world units). `toggle`: the camera button was
    // pressed (taken unless the race is still); `still`: race flag 0x8000 (paused), when the camera only follows.
    void Camera_ChaseTask(const CarBody& car, const float* pos, const Ground& ground, bool toggle, bool still);

    // The debug byte's free camera (Camera_DebugMove 0x0021F198; the Free Camera extra): what moves it this frame.
    // While one is set, Camera_ChaseTask moves the view by it in place of the swing and pitch settling back, and
    // doesn't keep the camera out of the walls.
    struct DebugMove {
        uint32_t held;     // controller 2's held buttons (platform::Button)
        int stick[2][2];   // each controller's right stick, x and y, -128..127 (0 centred, or no controller)
    };
    void SetDebugMove(const DebugMove* move) { debugMove_ = move; }

    // func_0021EC58, the camera's first task on the Adventure's field, `age` frames old: for 0x40 frames it looks at
    // the car from the front (the swing a half turn), then swings round behind it 0x200 a frame. True once it's
    // done (the task then becomes Camera_ChaseTask and clears the field's flag 0x400, the door check's).
    bool Camera_FieldStartTask(const CarBody& car, const float* pos, int age);

    const gfx::Camera3D& View() const { return camera_; }
    // The driver's view: the player's own car isn't seen (its near plane, 1.5 ahead, is past the car's nose).
    bool Inside() const { return view_ == 1; }

private:
    struct Preset {   // D_0029CE00: 0x20 bytes each
        float eye[3];      // in the car's frame
        float focal;
        int16_t pitch;
        uint32_t flags;    // +0x1C: the view (1: the driver's)
    };

    void Camera_SetView(int view);   // func_0021FAE8
    void Camera_FollowTilt(const CarBody& car, const float* pos);   // func_0021DC18
    void Camera_BuildMatrices();   // 0x0021F698 (with Camera_RotationFromAngles 0x0021F340)
    bool Camera_CornerBlocked(const Ground& ground, int corner) const;
    void Camera_KeepOutOfWalls(const Ground& ground);   // func_0021E070
    void Camera_DebugMove(const DebugMove& m);          // 0x0021F198, and func_0026D880's readout

    int split_ = 0;
    bool soccer_ = false;
    bool bigTires_ = false;
    int view_ = 0;
    Preset preset_ = {};
    int16_t pitch_ = 0;    // +0x14: down
    int16_t swing_ = 0;    // +0x16
    int16_t turn_ = 0;     // +0x1A
    float follow_[4] = {}; // D_0172FF30: the up vector's side (x) and forward (z) tilt followed, with their steps
    float frame_[3][4] = {};   // the car's frame the camera sits in (rows: right, up, forward in the world)
    float pos_[3] = {};
    gfx::Camera3D camera_ = {};
    const DebugMove* debugMove_ = nullptr;
};

}  // namespace game
