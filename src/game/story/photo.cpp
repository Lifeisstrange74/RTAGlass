#include "photo.h"

#include <math.h>

#include "../race/race_light.h"

namespace game {

namespace {

const float kPi = 3.14159265f;
// D_0029F3B0: the eye (in the camera's turned frame), the focal length, pitch and yaw.
const float kPhotoEye[3] = {1.4f, 2.0f, -20.7f};
const float kPhotoFocal = 1588.0f;
const int16_t kPhotoPitch = 0x0380;
const int16_t kPhotoYaw = (int16_t)0x9880;

}  // namespace

void Photo_SetCar(PhotoCar& p, uint32_t paint, int body, int wheelColor, int wheel, int billboard, uint16_t partFlags) {
    p.record = CarRecord();
    p.record.paint = paint & 0xFFFFFF;
    p.record.body = body;
    p.record.wheelColor = wheelColor;
    p.record.wheel = wheel;
    p.record.billboard = billboard;
    p.record.name = "";
    p.car = RaceCar();
    p.car.record = &p.record;
    p.car.town = -1;
    p.car.partFlags = partFlags;
}

void Photo_DrawCar(gfx::Draw2D& d, const Cars& cars, const PhotoCar& p) {
    // Light_Setup(1, 0), the screen's centre, Camera_BuildMatrices with D_0029F3B0, Light_SetHighlight; the car at
    // the origin, yaw 0 (D_0029F320's fifth place), on its big tires 0.85 up.
    static RaceLight light;
    Light_Setup(light, 1);
    const float a = kPhotoPitch * kPi / 32768.0f, b = kPhotoYaw * kPi / 32768.0f;
    const float ca = cosf(a), sa = sinf(a), cb = cosf(b), sb = sinf(b);
    const float right[3] = {cb, 0, -sb}, up[3] = {sb * sa, ca, cb * sa}, fwd[3] = {sb * ca, -sa, cb * ca};
    gfx::Camera3D cam;
    for (int k = 0; k < 3; ++k) {
        cam.right[k] = right[k];
        cam.up[k] = up[k];
        cam.forward[k] = fwd[k];
        cam.eye[k] = kPhotoEye[0] * right[k] + kPhotoEye[1] * up[k] + kPhotoEye[2] * fwd[k];
    }
    cam.focalX = kPhotoFocal;
    cam.focalY = kPhotoFocal * 0.94f;
    cam.centerX = gfx::Draw2D::kWidth / 2.0f;
    cam.centerY = gfx::Draw2D::kHeight / 2.0f;
    Light_SetHighlight(light, cam.forward);
    d.ClearDepth();
    PhotoCar shown = p;
    shown.car.record = &shown.record;
    cars.Car_DrawCard(d, cam, light, shown.car, false, true);
}

}  // namespace game
