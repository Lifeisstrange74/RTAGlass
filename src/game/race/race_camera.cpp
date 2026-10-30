#include "race_camera.h"

#include <math.h>
#include <stdio.h>

#include "../../gfx/draw2d.h"

namespace game {

namespace {

const float kPi = 3.14159265f;
const float kMinFocal = 500.0f, kMaxFocal = 5333.0f;   // D_00332E38 (and E3C, E40)

float Clamp(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }

// The game's projection: the frame's centre, and 0.94 of the focal length vertically (0.47 on its 224-line field).
void SetProjection(gfx::Camera3D& c, float focal) {
    c.focalX = focal;
    c.focalY = focal * 0.94f;
    c.centerX = gfx::Draw2D::kWidth / 2.0f;
    c.centerY = gfx::Draw2D::kHeight / 2.0f;
}

// A view from `eye` along `fwd` (a unit vector), level: right = up x forward, up = forward x right.
void SetAxes(gfx::Camera3D& c, const float* eye, const float* fwd) {
    float rx = fwd[2], rz = -fwd[0], r = sqrtf(rx * rx + rz * rz);
    if (r < 1e-6f) {
        rx = 1.0f;
        rz = 0.0f;
        r = 1.0f;
    }
    const float right[3] = {rx / r, 0.0f, rz / r};
    const float up[3] = {fwd[1] * right[2] - fwd[2] * right[1], fwd[2] * right[0] - fwd[0] * right[2],
                         fwd[0] * right[1] - fwd[1] * right[0]};
    for (int i = 0; i < 3; ++i) {
        c.eye[i] = eye[i];
        c.forward[i] = fwd[i];
        c.right[i] = right[i];
        c.up[i] = up[i];
    }
}

// The view the 16-bit angles give (Camera_RotationFromAngles: Ry(yaw) Rx(pitch), pitch down), from `eye`.
void SetAngles(gfx::Camera3D& c, const float* eye, int pitch, int yaw, float extraYaw) {
    float a = (int16_t)pitch * (2 * kPi / 65536.0f), b = (int16_t)yaw * (2 * kPi / 65536.0f) + extraYaw;
    const float fwd[3] = {sinf(b) * cosf(a), -sinf(a), cosf(b) * cosf(a)};
    SetAxes(c, eye, fwd);
}

}  // namespace

bool TrackCameras::Load(const std::string& path) {
    cameras_.clear();
    FILE* f = fopen(path.c_str(), "r");
    if (!f) return false;
    char row[256];
    while (fgets(row, sizeof row, f)) {
        TrackCamera c;
        if (sscanf(row, "camera %f %f %f %f %f %d %d %d %d %x", &c.x, &c.y, &c.z, &c.radius, &c.zoom, &c.eye[0],
                   &c.eye[1], &c.eye[2], &c.reach, &c.flags) == 10)
            cameras_.push_back(c);
    }
    fclose(f);
    // The list has to end with the camera used everywhere else.
    if (!cameras_.empty()) cameras_.back().flags |= kLast;
    return !cameras_.empty();
}

// The car is looked at from 1 above its ground point. The first camera whose sphere holds that point (or the last)
// takes the view. With kEye it looks at the car from its eye, or from its centre when the car is out of reach; with
// kZoom the focal length follows the distance (75 a unit on most courses), so the car keeps its size until the
// lens can zoom no further. Without kEye the last camera follows reach behind the car at its pitch, turned with the
// car's heading.
gfx::Camera3D TrackCameras::Camera_TrackSide(const float* pos, float yaw) const {
    gfx::Camera3D c;
    const float target[3] = {pos[0], pos[1] + 1.0f, pos[2]};
    if (cameras_.empty()) {
        const float eye[3] = {pos[0], pos[1] + 3.0f, pos[2] - 7.0f};
        SetAngles(c, eye, 0x1000, 0, 0.0f);
        SetProjection(c, 400.0f);
        return c;
    }
    const TrackCamera* t = &cameras_.back();
    float d = 0.0f;
    for (const TrackCamera& e : cameras_) {
        float dx = target[0] - e.x, dy = target[1] - e.y, dz = target[2] - e.z;
        d = sqrtf(dx * dx + dy * dy + dz * dz);
        if (e.flags & kLast || d < e.radius) {
            t = &e;
            break;
        }
    }
    float focal = t->zoom;
    if (t->flags & kZoom) focal = Clamp(d * t->zoom, kMinFocal, kMaxFocal);

    if (t->flags & kEye) {
        float eye[3] = {t->x, t->y, t->z};
        float hx = target[0] - t->x, hz = target[2] - t->z;
        if (sqrtf(hx * hx + hz * hz) < (float)t->reach) {
            for (int i = 0; i < 3; ++i) eye[i] = (float)t->eye[i] + (t->flags & kEyeAbsolute ? 0.0f : eye[i]);
            float ex = target[0] - eye[0], ey = target[1] - eye[1], ez = target[2] - eye[2];
            d = sqrtf(ex * ex + ey * ey + ez * ez);
            if (t->flags & kZoom) focal = Clamp(d * t->zoom, kMinFocal, kMaxFocal);
        }
        float fwd[3] = {target[0] - eye[0], target[1] - eye[1], target[2] - eye[2]};
        float n = sqrtf(fwd[0] * fwd[0] + fwd[1] * fwd[1] + fwd[2] * fwd[2]);
        if (n < 1e-6f) n = 1.0f;
        for (float& v : fwd) v /= n;
        SetAxes(c, eye, fwd);
    } else if (t->flags & (kFollowX | kFollowZ)) {
        const float center[3] = {t->flags & kFollowX ? pos[0] : t->x, pos[1], t->flags & kFollowZ ? pos[2] : t->z};
        if (t->flags & kZoom) focal = Clamp(t->reach * t->zoom, kMinFocal, kMaxFocal);
        SetAngles(c, center, t->eye[0], t->eye[1], t->flags & kFollowYaw ? yaw : 0.0f);
        for (int i = 0; i < 3; ++i) c.eye[i] = center[i] - c.forward[i] * (float)t->reach;
    } else {
        const float eye[3] = {t->x, t->y, t->z};
        SetAngles(c, eye, t->eye[0], t->eye[1], 0.0f);
    }
    SetProjection(c, focal);
    return c;
}

}  // namespace game
