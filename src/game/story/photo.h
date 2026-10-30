// The car over a photo: Photo_ViewTask (func_0022BFD0) draws, after the photo (SYS/PUTI.BIN's picture, twice its
// size), the car it was taken with standing level at the origin, from camera D_0029F3B0, lit by course 1's light, as
// Car_Draw at distance 0 (the close-up WHEEL.BIN wheels; func_002236D8). The album's viewer takes the car from the
// photo's record (save +0x54), the take mode from the save's car 0 (just copied into it).
#pragma once

#include "../../gfx/draw2d.h"
#include "../race/car.h"
#include "../save/save_data.h"

namespace game {

struct PhotoCar {
    CarRecord record;
    RaceCar car;
};

// The photo's car from its 12-byte record (or a SaveCar's first 12 bytes).
void Photo_SetCar(PhotoCar& p, uint32_t paint, int body, int wheelColor, int wheel, int billboard, uint16_t partFlags);
// The car over the photo; `cars` must have its body.
void Photo_DrawCar(gfx::Draw2D& d, const Cars& cars, const PhotoCar& p);

}  // namespace game
