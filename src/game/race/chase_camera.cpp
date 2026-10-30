#include "chase_camera.h"

#include <math.h>

#include "../../gfx/draw2d.h"
#include "../../platform/input.h"
#include "../debug_text.h"
#include "../../game_text.inc"   // the game's text, GT_<address>_<length>, made from the ELF

namespace game {

namespace {

const float kPi = 3.14159265358979f;
const float kAngle = kPi / 32768.0f;   // a 16-bit angle in radians

// D_0029CE00: the views: 0-1 one player's chase and driver's views, 2-3 the split screen's, 4-7 the same for part
// flag 0x400 (the big tires), 8 and 9 Soccer's (course 0x1A) chase views, one player's and the split screen's: further
// back and looking down more, to see the ball.
const float kPresetEye[10][3] = {{0, 2.0f, -7.0f},  {0, 1.5f, -0.25f}, {0, 1.2f, -7.0f}, {0, 1.5f, -0.25f},
                                 {0, 2.5f, -7.0f},  {0, 2.2f, 0.0f},   {0, 1.6f, -7.0f}, {0, 2.0f, -0.25f},
                                 {0, 2.0f, -9.3f},  {0, 1.2f, -10.8f}};
const float kPresetFocal[10] = {500.0f, 500.0f, 461.0f, 500.0f, 500.0f, 500.0f, 461.0f, 500.0f, 500.0f, 461.0f};
const int16_t kPresetPitch[10] = {0x380, 0, 0x600, 0, 0x580, 0, 0x800, 0, 0xD80, 0x880};
const float kFocal = 500.0f;

const int16_t kPitchStep = 0x80;     // the pitch moves to its view's by this much a frame
const int16_t kSwingStep = 0x80;     // the swing returns to 0, and turns away from a wall, by this much
const float kFollowStep = 0.001f;    // the tilt follower's speed grows by this much a frame

// D_0029CF40's first two points: the near plane's bottom corners (x 320, y 112 of the field at depth 1.5).
const float kCornerX = 320.0f * 1.5f / kFocal, kCornerY = -112.0f * 1.5f / (kFocal * 0.47f), kCornerZ = 1.5f;

void Normalize(float* v) {
    float l = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    if (l > 0)
        for (int k = 0; k < 3; ++k) v[k] /= l;
}

// The follower's step (func_0021DC18): towards the target, speeding up by kFollowStep a frame but never past it.
void Follow(float& value, float& step, float target) {
    float d = target - value;
    if (d > 0) {
        step = step < kFollowStep ? kFollowStep : step + kFollowStep;
        if (step < d) d = step;
    } else if (d < 0) {
        step = -kFollowStep < step ? -kFollowStep : step - kFollowStep;
        if (d < step) d = step;
    }
    value += d;
    step = d;
}

}  // namespace

void ChaseCamera::Reset(int split, bool soccer, bool bigTires) {
    split_ = split;
    soccer_ = soccer;
    bigTires_ = bigTires;
    for (float& f : follow_) f = 0;
    Camera_SetView(0);
}

// func_0021FAE8 (Camera_SetView): the camera takes the view's preset (its pitch, no swing or turn). Camera_ChaseTask
// picks it: the view plus 2 in the split screen, plus 4 on the big tires (the car's part flag 0x400); on course 0x1A
// the chase view is 8 (9 split) and the driver's view is 1 either way.
void ChaseCamera::Camera_SetView(int view) {
    view_ = view;
    int preset = soccer_ ? (view ? 1 : 8 + (split_ ? 1 : 0)) : view + (split_ ? 2 : 0) + (bigTires_ ? 4 : 0);
    for (int k = 0; k < 3; ++k) preset_.eye[k] = kPresetEye[preset][k];
    preset_.focal = kPresetFocal[preset];
    preset_.pitch = kPresetPitch[preset];
    preset_.flags = (uint32_t)view;
    pitch_ = preset_.pitch;
    swing_ = turn_ = 0;
}

void ChaseCamera::Camera_ChaseTask(const CarBody& car, const float* pos, const Ground& ground, bool toggle,
                                   bool still) {
    const float yaw = (int16_t)car.yaw * kAngle, s = sinf(yaw), c = cosf(yaw);
    if (toggle && !still) {
        // The camera button: the other view, and the follower starts from the car's tilt as it is.
        Camera_SetView(view_ ^ 1);
        follow_[0] = car.m[1][0] * c - car.m[1][2] * s;
        follow_[2] = car.m[1][0] * s + car.m[1][2] * c;
        follow_[1] = follow_[3] = 0;
    }
    if (debugMove_) {
        if (!still) Camera_DebugMove(*debugMove_);
    } else if (!still) {
        // The swing wears off; in the chase view the pitch goes back to the view's.
        if (swing_ < -0x7F) swing_ = (int16_t)(swing_ + kSwingStep);
        if (swing_ >= 0x80) swing_ = (int16_t)(swing_ - kSwingStep);
        if (view_ == 0) {
            // func_0021FB20's view: the chase view's (2 split, 4 more on the big tires), but in the Adventure's
            // soccer (mode 0 on course 0x1A) 8. So 2 Player's soccer camera starts at view 9's pitch and settles to
            // view 2's (PCSX2: 0x600).
            int16_t target = kPresetPitch[soccer_ && !split_ ? 8 : (split_ ? 2 : 0) + (bigTires_ ? 4 : 0)];
            if (pitch_ + kPitchStep < target) pitch_ = (int16_t)(pitch_ + kPitchStep);
            else if (target < pitch_ - kPitchStep) pitch_ = (int16_t)(pitch_ - kPitchStep);
            else pitch_ = target;
        }
    }
    // The chase view turns against the slide, so it looks along the car's path.
    turn_ = view_ == 0 ? (int16_t)-car.slide : 0;
    Camera_FollowTilt(car, pos);
    Camera_BuildMatrices();
    if (!still && view_ == 0 && !debugMove_) Camera_KeepOutOfWalls(ground);
}

// Camera_DebugMove (0x0021F198): controller 2's held d-pad moves the eye 0.1 a frame in the car's frame (Up/Down
// up and down, Right/Left across), L2/L1 (the PC's Wing Down/Up) along; Triangle zooms in x 1.01162 a frame, Square
// (the PC's Brake) out while the focal length is over 500; each controller's right stick swings and pitches it.
// Then func_0026D880 prints the camera.
void ChaseCamera::Camera_DebugMove(const DebugMove& m) {
    using namespace platform;
    const float kStep = 0.1f, kZoom = 1.01162f;
    if (m.held & kUp) preset_.eye[1] += kStep;
    if (m.held & kDown) preset_.eye[1] -= kStep;
    if (m.held & kRight) preset_.eye[0] += kStep;
    if (m.held & kLeft) preset_.eye[0] -= kStep;
    if (m.held & kWingDown) preset_.eye[2] += kStep;
    if (m.held & kWingUp) preset_.eye[2] -= kStep;
    if (m.held & kTriangle) preset_.focal *= kZoom;
    if ((m.held & kBrake) && preset_.focal > 500.0f) preset_.focal /= kZoom;
    for (int p = 0; p < 2; ++p) {
        swing_ = (int16_t)(swing_ - (m.stick[p][0] << 7));
        pitch_ = (int16_t)(pitch_ - (m.stick[p][1] << 7));
    }
    DebugText_Printf(16, 80, "CX:%.2f", preset_.eye[0]);
    DebugText_Printf(16, 88, "CY:%.2f", preset_.eye[1]);
    DebugText_Printf(16, 96, "CZ:%.2f", preset_.eye[2]);
    DebugText_Printf(16, 104, "ZM:%.0f(%.0f)", preset_.focal, preset_.focal * 18.0f / 320.0f);
    // +0x18 isn't in the port (0).
    DebugText_Printf(16, 112, GT_0032FB58_12, pitch_ * 180 / 32768, swing_ * 180 / 32768, 0);
    DebugText_Printf(32, 120, "%X %X %X", (unsigned)(int)pitch_, (unsigned)(int)swing_, 0u);
}

bool ChaseCamera::Camera_FieldStartTask(const CarBody& car, const float* pos, int age) {
    turn_ = 0;
    bool done = age >= 0x80;
    if (age < 0x40) {
        if (view_ == 0) swing_ = -0x8000;
    } else if (!done) {
        if (view_ == 0) swing_ = (int16_t)(swing_ + 0x200);
    }
    Camera_FollowTilt(car, pos);
    Camera_BuildMatrices();
    return done;
}

// func_0021DC18: the car's up vector in its heading's frame; the forward tilt (the slope) followed slowly. The chase
// view takes the body's lean against the car's side tilt; the driver's view follows the side tilt too and adds the
// lean. The frame is that up vector with the car's heading.
void ChaseCamera::Camera_FollowTilt(const CarBody& car, const float* pos) {
    const float yaw = (int16_t)car.yaw * kAngle, s = sinf(yaw), c = cosf(yaw);
    const float* up = car.m[1];
    float x = up[0] * c - up[2] * s, y = up[1], z = up[0] * s + up[2] * c;
    if (view_ == 0) {
        Follow(follow_[2], follow_[3], z);
        x = -car.body[1][0];
        y = 1.0f;
        z = follow_[2];
    } else {
        Follow(follow_[0], follow_[1], x);
        x = follow_[0] - car.body[1][0];
        Follow(follow_[2], follow_[3], z);
        z = follow_[2] + car.body[1][2];
    }
    float u[3] = {x * c + z * s, y, -x * s + z * c};
    Normalize(u);
    Matrix_FromUp(frame_, u);
    Matrix_TurnY(frame_, yaw);
    for (int k = 0; k < 3; ++k) pos_[k] = pos[k];
}

// Camera_RotationFromAngles turned by the frame: Ry(swing + turn) Rx(pitch) in the car's frame; the eye is the
// preset's place turned the same way, from the car.
void ChaseCamera::Camera_BuildMatrices() {
    const float b = (int16_t)(swing_ + turn_) * kAngle, p = pitch_ * kAngle;
    const float sb = sinf(b), cb = cosf(b), sp = sinf(p), cp = cosf(p);
    const float right[3] = {cb, 0, -sb}, up[3] = {sb * sp, cp, cb * sp}, fwd[3] = {sb * cp, -sp, cb * cp};
    float eye[3];
    for (int k = 0; k < 3; ++k)
        eye[k] = preset_.eye[0] * right[k] + preset_.eye[1] * up[k] + preset_.eye[2] * fwd[k];
    gfx::Camera3D& v = camera_;
    for (int k = 0; k < 3; ++k) {
        v.right[k] = right[0] * frame_[0][k] + right[1] * frame_[1][k] + right[2] * frame_[2][k];
        v.up[k] = up[0] * frame_[0][k] + up[1] * frame_[1][k] + up[2] * frame_[2][k];
        v.forward[k] = fwd[0] * frame_[0][k] + fwd[1] * frame_[1][k] + fwd[2] * frame_[2][k];
        v.eye[k] = pos_[k] + eye[0] * frame_[0][k] + eye[1] * frame_[1][k] + eye[2] * frame_[2][k];
    }
    v.focalX = preset_.focal;
    v.focalY = preset_.focal * 0.94f;
    // Camera_SetScreenCenter: the frame's middle, or with a split screen each half's (y 1992 and 2104 of the GS's
    // 2048 on the field, D_00332C4C): 112 and 336 on the frame.
    v.centerX = gfx::Draw2D::kWidth / 2.0f;
    v.centerY = split_ == 0 ? gfx::Draw2D::kHeight / 2.0f : split_ == 1 ? 112.0f : 336.0f;
}

// A near corner (0 left, 1 right) is in a wall when the course has no ground there or its ground is above it.
bool ChaseCamera::Camera_CornerBlocked(const Ground& ground, int corner) const {
    const float x = corner ? kCornerX : -kCornerX;
    float p[4], ceiling;
    for (int k = 0; k < 3; ++k)
        p[k] = camera_.eye[k] + x * camera_.right[k] + kCornerY * camera_.up[k] + kCornerZ * camera_.forward[k];
    p[3] = 0;
    const float y = p[1];
    return ground.Collision_Ground(p, &ceiling) == -1 || y < p[1];
}

// func_0021E070: while the near plane's left corner is in a wall, the camera swings the other way 0x80 at a time
// (and the same for the right), at most half a turn.
void ChaseCamera::Camera_KeepOutOfWalls(const Ground& ground) {
    if (ground.Empty()) return;
    if (!Camera_CornerBlocked(ground, 0)) {
        if (!Camera_CornerBlocked(ground, 1)) return;
        do {
            swing_ = (int16_t)(swing_ + kSwingStep);
            Camera_BuildMatrices();
        } while (swing_ >= 0 && Camera_CornerBlocked(ground, 1));
        while (Camera_CornerBlocked(ground, 0)) {
            swing_ = (int16_t)(swing_ + kSwingStep);
            Camera_BuildMatrices();
            if (swing_ < 0) break;
        }
    } else {
        do {
            swing_ = (int16_t)(swing_ - kSwingStep);
            Camera_BuildMatrices();
        } while (swing_ <= 0 && Camera_CornerBlocked(ground, 0));
        while (Camera_CornerBlocked(ground, 1)) {
            swing_ = (int16_t)(swing_ - kSwingStep);
            Camera_BuildMatrices();
            if (swing_ > 0) break;
        }
    }
}

}  // namespace game
