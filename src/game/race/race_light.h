// The race's lighting: each course's time of day, fog and car lights. A reimplementation of the game's light state
// D_0177F5E0 and what fills it: Light_Setup (0x00225790) when the race is set up, then every frame Light_TimeOfDay
// (0x00225988) from the course's clock and, on every course but Endurance Run, Light_CourseFog (0x00226248).
// See docs/race.md, "Lighting, fog and the distance fade".
#pragma once

#include <stdint.h>

#include "../../gfx/model.h"

namespace game {

struct RaceLight {
    // +0x140, vf09 of programs 0x30/0x40: the weights of the field's day, dusk and night colours.
    float weights[3];
    // +0x158/+0x15C: the field's fog, F = clamp((end - w) x scale, 0, 255). The alpha fade keeps (0.5, 800).
    float fogScale, fogEnd;
    uint32_t fogColor;   // +0x170: FOGCOL, a PS2 RGBA word (red in the low byte); the cars fog to it too
    // +0x174 (pad +0x50): the background colour, also RGBA. On Endurance Run the sky is tinted by it (Sky_Draw).
    uint32_t background;
    // +0x00: the cars' light matrix (light 1, light 2, highlight), world directions.
    float dir[3][3];
    // +0x40 (D_0177F620): the cars' four colour sets (light 1, light 2, highlight, ambient). A car takes the one its
    // ground's surface flags pick (bits 12-13). Sets 0 and 1 follow the time of day; 2 and 3 are the course's.
    float sets[4][4][3];
};

// Endurance Run's sky (func_002262D8, Sky_Draw): the sky model's colour times the background colour.
void Light_SkyTint(const RaceLight& l, float* rgb);

// The clock a race on `course` starts at (D_002A88E8): frames into the game's hour-long day, 0x34BC0 frames.
int32_t Light_CourseClock(int course);
// The clock the Adventure goes on at after a race on `course` (D_002A88EC); 0: the clock before it + 9000.
int32_t Light_CourseEndClock(int course);
// Only Endurance Run's clock runs (Race_SetupTask), a frame at a time, wrapping at the hour.
const int kClockCourse = 0x11;
const int32_t kClockHour = 0x34BC0;

void Light_Setup(RaceLight& l, int course);   // 0x00225790
// Light_Setup's kind 2, the menus' (the Settings screen's pad): fixed directions (D_0029E980) and colour set 0, no
// time of day.
void Light_SetupMenu(RaceLight& l);
// The weights, fog and sets 0-1 for `clock`. `height` is the player's car's (Endurance Run; 0 elsewhere): its fog
// thins as the car climbs, and below -2.5 (in the water) it closes in, green-blue.
void Light_TimeOfDay(RaceLight& l, int32_t clock, float height);   // 0x00225988
void Light_CourseFog(RaceLight& l, int course);                    // 0x00226248
// The highlight: halfway between light 1 and the view, from the camera's forward axis (each view).
void Light_SetHighlight(RaceLight& l, const float* forward);   // 0x002261A0
// A car's lighting for program 0x20 with colour set `set` (paint white).
gfx::Lighting Light_CarSet(const RaceLight& l, int set);

}  // namespace game
