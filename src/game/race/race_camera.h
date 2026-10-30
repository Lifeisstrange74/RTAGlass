// The race's trackside cameras: what the screen shows once the player has finished and during the replay, in place
// of the chase camera. A reimplementation of Camera_TrackSide (0x00238B18), which Camera_FinishTask (0x0021E4D8) and
// Camera_ReplayTask (0x0021E328) hand the view to. See docs/finish.md.
//
// A course's cameras (data/camera/Cnn.txt, written by tools/modelconv from the executable's D_002A2A50) are a list
// of spheres around the track, each with an eye. While the followed car is inside a sphere, the view looks at the car
// from that sphere's eye and zooms with the distance, so the car keeps its size on the screen; the last entry, used
// everywhere else, follows behind the car.
#pragma once

#include <stdint.h>

#include <string>
#include <vector>

#include "../../gfx/model.h"

namespace game {

struct TrackCamera {
    float x, y, z, radius;   // where it's used: within radius of (x, y, z)
    float zoom;              // the focal length, or with kZoom the focal length per unit of distance to the car
    int eye[3];              // kEye: the eye (kEyeAbsolute: world, else from x y z); following: the view's pitch
                             // (down) and yaw (from the car's heading with kFollowYaw), 16-bit angles
    int reach;               // kEye: the eye is used within this horizontal distance (else x y z); following: the
                             // eye's distance behind the car
    uint32_t flags;
};

class TrackCameras {
public:
    enum Flags {
        kZoom = 2,             // focal length = zoom x the distance to the car, 500 to 5333
        kFollowX = 4,          // following (no kEye): centred on the car's x (else the entry's)
        kFollowZ = 8,          // and its z
        kFollowYaw = 0x10,     // and turned with its heading
        kEyeAbsolute = 0x20,
        kEye = 0x1000,         // look at the car from the eye
        kLast = 0x8000,
    };

    bool Load(const std::string& path);
    bool Empty() const { return cameras_.empty(); }

    // Camera_TrackSide: the view of a car at `pos` (on the ground) heading `yaw` (radians).
    gfx::Camera3D Camera_TrackSide(const float* pos, float yaw) const;

private:
    std::vector<TrackCamera> cameras_;
};

}  // namespace game
