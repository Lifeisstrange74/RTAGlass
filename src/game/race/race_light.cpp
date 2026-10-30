#include "race_light.h"

#include <math.h>
#include <string.h>

#include <algorithm>

namespace game {

namespace {

const int kCourses = 0x27;

// D_002A88E8 (each row's first word), by course id. Day runs 0xE484-0x255A7; the courses that start later are a
// sunset (0x278D0) or night.
const int32_t kClock[kCourses] = {0x1A5E0, 0x32898, 0x278D0, 0x1A5E0, 0x1A5E0, 0x29BF8, 0x1A5E0, 0x1A5E0, 0x32898,
                                  0x1A5E0, 0x278D0, 0x29BF8, 0x30570, 0x1A5E0, 0x1A5E0, 0x1A5E0, 0x29BF8, 0x23280,
                                  0x2BF20, 0x1A5E0, 0x2BF20, 0x1A5E0, 0x1A5E0, 0x1A5E0, 0x2BF20, 0x1A5E0, 0x2BF20,
                                  0x1A5E0, 0x2BF20, 0x1A5E0, 0x1A5E0, 0x2BF20, 0x1A5E0, 0x2BF20, 0x1A5E0, 0x1A5E0,
                                  0x2BF20, 0x1A5E0, 0x1A5E0};
// Each row's second word (D_002A88EC): the time of day the Adventure goes on at after a race on the course; 0: an
// hour after the clock before the race.
const int32_t kEndClock[kCourses] = {0x20F58, 0x182B8, 0x29BF8, 0x20F58, 0x1EC30, 0x2E248, 0x20F58, 0x23280, 0x182B8,
                                     0x20F58, 0x29BF8, 0x30570, 0, 0x1EC30, 0x20F58, 0x1EC30, 0x2E248, 0xCA26,
                                     0, 0x20F58, 0x4650, 0x20F58, 0x23280, 0x1C908, 0, 0x23280, 0x2E248,
                                     0x20F58, 0, 0x15F90, 0x1EC30, 0, 0x13C68, 0, 0, 0x1EC30,
                                     0x20F58, 0x1A5E0, 0x20F58};

// D_0029EA10 by course id: the background colour and FOGCOL (RGB, red low), the fog's scale and end.
struct CourseFog {
    uint32_t background, color;
    float scale, end;
};
const CourseFog kFog[kCourses] = {
    {0xF05800, 0xFFFFFF, 0.5f, 800.0f},  {0xDEE0DC, 0x404040, 0.75f, 500.0f}, {0x66688B, 0x285AD2, 0.5f, 800.0f},
    {0xF05800, 0xFFFFFF, 0.5f, 800.0f},  {0xF05800, 0xA5CBE9, 1.0f, 400.0f},  {0x0A0000, 0x000000, 0.5f, 800.0f},
    {0xF05800, 0xFFFFFF, 0.5f, 800.0f},  {0xF05800, 0xA5CBE9, 0.5f, 800.0f},  {0xDEE0DC, 0x808080, 0.75f, 400.0f},
    {0xF05800, 0xFFFFFF, 0.5f, 700.0f},  {0x66688B, 0x285AD2, 0.5f, 800.0f},  {0x0A0000, 0x000000, 0.75f, 500.0f},
    {0x000000, 0x000000, 0.5f, 800.0f},  {0xFFFFFF, 0xFFFFFF, 0.5f, 800.0f},  {0xFFFFFF, 0xFFFFFF, 0.5f, 800.0f},
    {0xFFFFFF, 0xFFFFFF, 0.5f, 800.0f},  {0x0A0000, 0x000000, 0.5f, 800.0f},  {0xF05800, 0xFFFFFF, 0.5f, 800.0f},
    {0x000000, 0x000000, 0.75f, 400.0f}, {0xDEE0DC, 0x404040, 0.75f, 500.0f}, {0x000000, 0x000000, 0.75f, 400.0f},
    {0xF05800, 0xA5CBE9, 0.5f, 800.0f},  {0xDEE0DC, 0x404040, 0.75f, 500.0f}, {0xDEE0DC, 0x507850, 0.75f, 500.0f},
    {0x000000, 0x404040, 0.75f, 500.0f}, {0xF05800, 0xA5CBE9, 0.5f, 600.0f},  {0xDEE0DC, 0x404040, 0.75f, 500.0f},
    {0xFFFFFF, 0xFFFFFF, 0.75f, 500.0f}, {0xDEE0DC, 0x404040, 0.75f, 500.0f}, {0xDEE0DC, 0x404040, 0.5f, 800.0f},
    {0xDEE0DC, 0x404040, 0.75f, 500.0f}, {0x000000, 0x000000, 0.5f, 800.0f},  {0xDEE0DC, 0x404040, 0.75f, 500.0f},
    {0xDEE0DC, 0x404040, 0.75f, 500.0f}, {0xDEE0DC, 0xFFFFFF, 0.75f, 500.0f}, {0xDEE0DC, 0xFFFFFF, 0.75f, 500.0f},
    {0x7F7F00, 0x7F7F00, 1.0f, 300.0f},  {0xED6A31, 0xFFFFFF, 0.5f, 800.0f},  {0xDEE0DC, 0xFFFFFF, 0.75f, 500.0f},
};

// D_0029E200 by course id (0x30 bytes each): where lights 1 and 2 shine from (Light_Setup turns them round and
// normalizes them).
const float kLightFrom[kCourses][2][3] = {
    {{1, -1, 0}, {-0.5f, -0.5f, 0.866f}},   {{1, -1, 0}, {-0.5f, -0.5f, 0.866f}},
    {{1, -1, 0}, {-0.5f, -0.5f, 0.866f}},   {{1, -1, 0}, {-0.5f, -0.5f, 0.866f}},
    {{1, -1, 0}, {-0.5f, -0.5f, 0.866f}},   {{1, -1, 0}, {-0.5f, -0.5f, 0.866f}},
    {{1, -1, 0}, {-0.5f, -0.5f, 0.866f}},   {{1, -1, 0}, {-0.5f, -0.5f, 0.866f}},
    {{1, -1, 0}, {-0.5f, -0.5f, 0.866f}},   {{1, -1, 0}, {-0.5f, -0.5f, 0.866f}},
    {{1, -1, 0}, {-0.5f, -0.5f, 0.866f}},   {{1, -1, 0}, {-0.5f, -0.5f, 0.866f}},
    {{1, 1, 0}, {-0.5f, -0.5f, 0.866f}},    {{1, -0.5f, 0}, {-0.5f, -0.5f, 0.866f}},
    {{1, -0.5f, 0}, {-0.5f, -0.5f, 0.866f}}, {{1, -0.5f, 0}, {-0.5f, -0.5f, 0.866f}},
    {{1, -1, 0}, {-0.5f, -0.5f, 0.866f}},   {{1, -1, 0}, {-0.5f, -0.5f, 0.866f}},
    {{1, -1, 0}, {-0.5f, -0.5f, 0.866f}},   {{1, -1, 0}, {-0.5f, -0.5f, 0.866f}},   // 0x12-0x26: the events
    {{1, -0.5f, 0}, {-0.5f, -0.5f, 0.866f}}, {{1, -1, 0}, {-0.5f, -0.5f, 0.866f}},
    {{1, -1, 0}, {-0.5f, -0.5f, 0.866f}},   {{1, -1, 0}, {-0.5f, -0.5f, 0.866f}},
    {{1, -1, 0}, {-0.5f, -0.5f, 0.866f}},   {{1, -0.5f, 0}, {-0.5f, -0.5f, 0.866f}},
    {{1, -0.5f, 1}, {-1, -0.5f, -1}},       {{1, -0.5f, 0}, {-0.5f, -0.5f, 0.866f}},
    {{1, -1, 0}, {-0.5f, -0.5f, 0.866f}},   {{1, 0, 0}, {-0.5f, -0.5f, 0.866f}},
    {{1, -1, 0}, {-0.5f, -0.5f, 0.866f}},   {{1, 1, 0}, {-0.5f, -0.5f, 0.866f}},
    {{1, -1, 0}, {-0.5f, -0.5f, 0.866f}},   {{1, -1, 0}, {-0.5f, -0.5f, 0.866f}},
    {{1, -1, 0}, {-0.5f, -0.5f, 0.866f}},   {{1, -1, 0}, {-0.5f, -0.5f, 0.866f}},
    {{1, -1, 0}, {-0.5f, -0.5f, 0.866f}},   {{1, -0.5f, 0}, {-0.5f, -0.5f, 0.866f}},
    {{1, -1, 0}, {-0.5f, -0.5f, 0.866f}},
};

// D_0029D500: the cars' colour sets (light 1, light 2, highlight, ambient). 0-2 are set 0's day, dusk and night,
// 3-5 set 1's; D_0029EC90 picks a course's sets 2 and 3 from the rest (kCourseSets).
const float kSets[52][4][3] = {
    {{0.6f, 0.6f, 0.6f}, {0.25f, 0.25f, 0.25f}, {0.6f, 0.6f, 0.6f}, {0.4f, 0.4f, 0.4f}},   // 0
    {{0.6f, 0.38f, 0.2f}, {0.3f, 0.19f, 0.1f}, {0.6f, 0.5f, 0.2f}, {0.5f, 0.4f, 0.4f}},
    {{0.3f, 0.3f, 0.3f}, {0, 0, 0}, {0.4f, 0.4f, 0.5f}, {0.4f, 0.4f, 0.5f}},
    {{0.4f, 0.4f, 0.4f}, {0.25f, 0.25f, 0.25f}, {0.2f, 0.2f, 0.2f}, {0.5f, 0.5f, 0.5f}},
    {{0.36f, 0.2f, 0.1f}, {0.3f, 0.19f, 0.1f}, {0.12f, 0.12f, 0.12f}, {0.5f, 0.4f, 0.4f}},
    {{0.3f, 0.3f, 0.3f}, {0, 0, 0}, {0.4f, 0.4f, 0.5f}, {0.4f, 0.4f, 0.5f}},   // 5
    {{0.3059f, 0.2824f, 0.2118f}, {0.3f, 0.3f, 0.3f}, {0.3f, 0.3f, 0.3f}, {0.5f, 0.5f, 0.5f}},
    {{0.1176f, 0.4235f, 0.6f}, {0.2f, 0.2f, 0.3f}, {0.8f, 0.8f, 0.9f}, {0.2f, 0.2f, 0.3f}},
    {{0.4471f, 0.4471f, 0.4235f}, {0.15f, 0.15f, 0.15f}, {0.8f, 0.8f, 0.8f}, {0.2f, 0.2f, 0.2f}},
    {{0.4706f, 0.1412f, 0.0706f}, {0.3f, 0.4f, 0.3f}, {0.12f, 0.3f, 0.2f}, {0.2f, 0.2f, 0.2f}},
    {{0.1647f, 0.4706f, 0.1176f}, {0.3f, 0.5f, 0.3f}, {0.8f, 0.8f, 0.3f}, {0.2f, 0.3f, 0.2f}},   // 10
    {{0.7f, 0.38f, 0.2f}, {0.3f, 0.19f, 0.1f}, {0.7f, 0.4f, 0.2f}, {0.3f, 0.25f, 0.2f}},
    {{0.1176f, 0.4235f, 0.6f}, {0.2f, 0.2f, 0.3f}, {0.8f, 0.8f, 0.9f}, {0.4f, 0.4f, 0.6f}},
    {{0.4471f, 0.4471f, 0.4235f}, {0.2f, 0.2f, 0.1f}, {0.6f, 0.6f, 0.5f}, {0.2f, 0.22f, 0.2f}},
    {{0.5647f, 0.0941f, 0.0235f}, {0.4f, 0.4f, 0.3f}, {0.9f, 0.6f, 0.2f}, {0.9f, 0.6f, 0.2f}},
    {{0.4706f, 0.1412f, 0.1412f}, {0.3f, 0.4f, 0.3f}, {0.8f, 0.6f, 0.6f}, {0.2f, 0.2f, 0.2f}},   // 15
    {{0.2353f, 0.1882f, 0.1882f}, {0.25f, 0.3f, 0.25f}, {0.5f, 0.45f, 0.45f}, {0.3f, 0.3f, 0.3f}},
    {{0.65f, 0.65f, 0.6f}, {0.3f, 0.3f, 0.3f}, {0.7f, 0.7f, 0.7f}, {0.5f, 0.55f, 0.5f}},
    {{0.8f, 0.8f, 0.7f}, {0.3f, 0.3f, 0.3f}, {0.7f, 0.7f, 0.6f}, {0.55f, 0.55f, 0.5f}},
    {{0.8f, 0.8f, 0.7f}, {0.3f, 0.3f, 0.3f}, {0.65f, 0.65f, 0.6f}, {0.5f, 0.5f, 0.5f}},
    {{0.65f, 0.65f, 0.6f}, {0.3f, 0.3f, 0.3f}, {0.7f, 0.7f, 0.7f}, {0.5f, 0.55f, 0.5f}},   // 20
    {{0.65f, 0.65f, 0.6f}, {0.3f, 0.3f, 0.3f}, {0.7f, 0.7f, 0.7f}, {0.5f, 0.55f, 0.5f}},
    {{0.65f, 0.65f, 0.6f}, {0.3f, 0.3f, 0.3f}, {0.7f, 0.7f, 0.7f}, {0.5f, 0.55f, 0.5f}},
    {{0.5f, 0.5f, 0.3f}, {0, 0, 0.2f}, {0.8f, 0.8f, 0.5f}, {0.3f, 0.3f, 0.5f}},
    {{0.5176f, 0.5176f, 0.4235f}, {0.2f, 0.2f, 0.1f}, {0.6f, 0.6f, 0.5f}, {0.22f, 0.22f, 0.2f}},
    {{0.6f, 0.6f, 0.6f}, {0.3f, 0.3f, 0.3f}, {0.6f, 0.6f, 0.6f}, {0.5f, 0.5f, 0.5f}},   // 25
    {{0.2118f, 0.2353f, 0.1882f}, {0.3f, 0.3f, 0.3f}, {0.3f, 0.32f, 0.3f}, {0.2f, 0.22f, 0.2f}},
    {{0.3059f, 0.2824f, 0.2118f}, {0.3f, 0.3f, 0.3f}, {0.3f, 0.3f, 0.3f}, {0.5f, 0.5f, 0.5f}},
    {{0.3f, 0.3f, 0.3f}, {0, 0, 0}, {0.4f, 0.4f, 0.5f}, {0.4f, 0.4f, 0.5f}},
    {{0.8f, 0.8f, 0.7f}, {0.3f, 0.3f, 0.3f}, {0.7f, 0.7f, 0.6f}, {0.55f, 0.55f, 0.5f}},
    {{0.8f, 0.8f, 0.7f}, {0.3f, 0.3f, 0.3f}, {0.7f, 0.7f, 0.6f}, {0.55f, 0.55f, 0.5f}},   // 30
    {{0.8f, 0.8f, 0.7f}, {0.3f, 0.3f, 0.3f}, {0.7f, 0.7f, 0.6f}, {0.5f, 0.55f, 0.5f}},
    {{0.4706f, 0.1412f, 0.0706f}, {0.3f, 0.4f, 0.3f}, {0.7f, 0.7f, 0.5f}, {0.25f, 0.25f, 0.2f}},
    {{0.65f, 0.65f, 0.6f}, {0.3f, 0.3f, 0.3f}, {0.7f, 0.7f, 0.7f}, {0.5f, 0.55f, 0.5f}},
    {{0.75f, 0.75f, 0.7f}, {0.75f, 0.75f, 0.7f}, {0.7f, 0.7f, 0.7f}, {0.4f, 0.45f, 0.4f}},
    {{0.65f, 0.65f, 0.6f}, {0.3f, 0.3f, 0.3f}, {0.7f, 0.7f, 0.7f}, {0.5f, 0.55f, 0.5f}},   // 35
    {{0.5f, 0.5f, 0.3f}, {0, 0, 0.2f}, {0.5f, 0.65f, 0.8f}, {0.3f, 0.3f, 0.5f}},
    {{0.65f, 0.65f, 0.6f}, {0.3f, 0.3f, 0.3f}, {0.7f, 0.7f, 0.7f}, {0.5f, 0.55f, 0.5f}},
    {{0.8f, 0.8f, 0.7f}, {0.3f, 0.3f, 0.3f}, {0.7f, 0.7f, 0.6f}, {0.5f, 0.55f, 0.5f}},
    {{0.5647f, 0.0941f, 0.0235f}, {0.4f, 0.4f, 0.3f}, {0.7f, 0.3f, 0.05f}, {0.8f, 0.5f, 0.2f}},
    {{0.5647f, 0.0941f, 0.0235f}, {0.4f, 0.4f, 0.3f}, {0.9f, 0.5f, 0.2f}, {0.9f, 0.6f, 0.2f}},   // 40
    {{0.65f, 0.65f, 0.6f}, {0.3f, 0.3f, 0.3f}, {0.7f, 0.7f, 0.7f}, {0.5f, 0.55f, 0.5f}},
    {{0.2118f, 0.2353f, 0.1882f}, {0.3f, 0.3f, 0.3f}, {0.3f, 0.32f, 0.3f}, {0.2f, 0.22f, 0.2f}},
    {{0.65f, 0.65f, 0.6f}, {0.3f, 0.3f, 0.3f}, {0.7f, 0.7f, 0.7f}, {0.5f, 0.55f, 0.5f}},
    {{0.8f, 0.8f, 0.7f}, {0.3f, 0.3f, 0.3f}, {0.65f, 0.65f, 0.6f}, {0.5f, 0.5f, 0.5f}},
    {{0.2f, 0.2f, 0.3f}, {0, 0, 0}, {0.2f, 0.22f, 0.22f}, {0.3f, 0.3f, 0.3f}},   // 45
    {{0.2f, 0.2f, 0.3f}, {0, 0, 0}, {0.2f, 0.22f, 0.22f}, {0.3f, 0.3f, 0.3f}},
    {{0.2f, 0.2f, 0.3f}, {0, 0, 0}, {0.2f, 0.22f, 0.22f}, {0.3f, 0.3f, 0.3f}},
    {{0.65f, 0.65f, 0.6f}, {0.3f, 0.3f, 0.3f}, {0.7f, 0.7f, 0.7f}, {0.5f, 0.55f, 0.5f}},
    {{0.65f, 0.65f, 0.6f}, {0.3f, 0.3f, 0.3f}, {0.7f, 0.7f, 0.7f}, {0.5f, 0.55f, 0.5f}},
    {{0.0706f, 0.3765f, 0.5529f}, {0.1f, 0.1f, 0.2f}, {0.7f, 0.7f, 0.8f}, {0.3f, 0.3f, 0.5f}},   // 50
    {{0.65f, 0.65f, 0.6f}, {0.3f, 0.3f, 0.3f}, {0.7f, 0.7f, 0.7f}, {0.5f, 0.55f, 0.5f}},
};

// D_0029EC90 ({course id, set 2, set 3} entries, looked up by course id): the entries of kSets that are its sets 2
// and 3.
const uint8_t kCourseSets[kCourses][2] = {{19, 19}, {6, 7},   {6, 7},   {6, 11},  {15, 16}, {7, 8},
                                          {12, 17}, {13, 18}, {45, 45}, {44, 44}, {9, 9},   {12, 12},
                                          {14, 10}, {20, 20}, {21, 21}, {22, 22}, {23, 23}, {24, 25},
                                          {26, 26}, {27, 27}, {28, 50}, {29, 29}, {30, 30}, {31, 31},
                                          {32, 32}, {33, 33}, {34, 34}, {35, 35}, {36, 36}, {37, 51},
                                          {38, 38}, {39, 40}, {41, 41}, {42, 42}, {43, 43}, {46, 46},
                                          {47, 47}, {48, 48}, {49, 49}};

// A course's row in the tables. (PC) The Test Course (0x27, C16), which the game has no light for, takes Peach
// Raceway's: midday, its light sets and white fog.
int Clamp(int course) { return course >= 0 && course < kCourses ? course : 0; }

void Normalize(float* v) {
    float n = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    if (n > 0)
        for (int k = 0; k < 3; ++k) v[k] /= n;
}

// The RGBA word of four bytes as Light_TimeOfDay stores them (cvt.w.s, then the low byte).
uint32_t Rgba(float r, float g, float b, uint32_t a) {
    return ((uint32_t)(int)r & 0xFF) | ((uint32_t)(int)g & 0xFF) << 8 | ((uint32_t)(int)b & 0xFF) << 16 | a << 24;
}

// A time-of-day set: day x w[0] + dusk x w[1] + night x w[2] (kSets entries first, first + 1, first + 2).
void BlendSet(int first, const float* w, float out[4][3]) {
    for (int r = 0; r < 4; ++r)
        for (int c = 0; c < 3; ++c)
            out[r][c] = kSets[first][r][c] * w[0] + kSets[first + 1][r][c] * w[1] + kSets[first + 2][r][c] * w[2];
}

}  // namespace

int32_t Light_CourseClock(int course) { return kClock[Clamp(course)]; }
int32_t Light_CourseEndClock(int course) { return kEndClock[Clamp(course)]; }

// The light matrix from the course's two light directions (func_00272E08: each turned round and normalized; the
// highlight comes per view), sets 0 and 1 as the day's (Light_TimeOfDay blends them each frame), and the course's
// own sets 2 and 3.
void Light_Setup(RaceLight& l, int course) {
    course = Clamp(course);
    memset(&l, 0, sizeof l);
    for (int i = 0; i < 2; ++i) {
        for (int k = 0; k < 3; ++k) l.dir[i][k] = -kLightFrom[course][i][k];
        Normalize(l.dir[i]);
    }
    memcpy(l.sets[0], kSets[0], sizeof l.sets[0]);
    memcpy(l.sets[1], kSets[3], sizeof l.sets[1]);
    memcpy(l.sets[2], kSets[kCourseSets[course][0]], sizeof l.sets[2]);
    memcpy(l.sets[3], kSets[kCourseSets[course][1]], sizeof l.sets[3]);
    l.weights[0] = 1.0f;
    l.fogScale = 0.5f;
    l.fogEnd = 800.0f;
    l.fogColor = 0x80FFFFFF;
    l.background = 0x80FFFFFF;
}

void Light_SetupMenu(RaceLight& l) {
    // D_0029E980: where the two lights come from.
    static const float kFrom[2][3] = {{3.0f, 0.5f, 1.0f}, {-1.0f, -1.0f, 0.0f}};
    memset(&l, 0, sizeof l);
    for (int i = 0; i < 2; ++i) {
        for (int k = 0; k < 3; ++k) l.dir[i][k] = -kFrom[i][k];
        Normalize(l.dir[i]);
    }
    for (int s = 0; s < 4; ++s) memcpy(l.sets[s], kSets[0], sizeof l.sets[s]);
    l.weights[0] = 1.0f;
    l.fogScale = 0.5f;
    l.fogEnd = 800.0f;
    l.fogColor = 0x80FFFFFF;
    l.background = 0x80FFFFFF;
}

// The day in five parts of the clock: night until 0xC15C, dawn over 9000 frames to 0xE483, day to 0x255A7, dusk over
// 9000 frames to 0x278CF, evening over 4500 frames to 0x28A63, then night. The weights (day, dusk, night) and the
// fog follow, both above water (height > -2.5) and below it.
void Light_TimeOfDay(RaceLight& l, int32_t t, float height) {
    float y = std::min(height, 300.0f);
    bool dry = y > -2.5f;
    float day = 0, dusk = 0, night = 0;
    if ((uint32_t)(t - 0xC15C) > 0x1C907u) {
        night = 1.1f;
        l.background = 0x80000000;
        l.fogEnd = 300.0f;
        l.fogScale = 1.0f;
        l.fogColor = 0x80080000;
    } else if (t <= 0xE483) {
        day = (float)(t - 0xC15C) / 9000.0f;
        night = 1.1f - day;
        l.background = Rgba(day * 255.0f, day * 255.0f, day < 0.5f ? day * 511.0f : 255.0f, l.background >> 24);
        if (dry) {
            l.fogEnd = std::max(300.0f, day * 500.0f + 300.0f - y);
            l.fogScale = night * 0.5f + 0.5f;
            l.fogColor = Rgba(day * 255.0f, day * 255.0f, day < 0.5f ? day * 511.0f : 255.0f, 0x7F);
        } else {
            l.fogEnd = 300.0f;
            l.fogScale = 1.0f;
            l.fogColor = Rgba(0, day * 127.0f, day < 0.5f ? day * 254.0f : 127.0f, 0x7F);
        }
    } else if (t <= 0x255A7) {
        day = 1.1f;
        l.background = 0x80FFFFFF;
        if (dry) {
            l.fogEnd = 800.0f - y;
            l.fogScale = 0.5f;
            l.fogColor = 0x80FFFFFF;
        } else {
            l.fogEnd = 300.0f;
            l.fogScale = 1.0f;
            l.fogColor = 0x807F7F00;
        }
    } else if (t <= 0x278CF) {
        dusk = (float)(t - 0x255A8) / 9000.0f;
        day = 1.1f - dusk;
        l.background = Rgba(255.0f, 255.0f - dusk * 94.0f, 255.0f - dusk * 128.0f, l.background >> 24);
        if (dry) {
            l.fogEnd = 800.0f - y;
            l.fogScale = 0.5f;
            l.fogColor = Rgba(255.0f, 255.0f - dusk * 94.0f, 255.0f - dusk * 128.0f, 0x80);
        } else {
            l.fogEnd = 300.0f;
            l.fogScale = 1.0f;
            l.fogColor = Rgba(0, 127.0f - dusk * 47.0f, 127.0f - dusk * 64.0f, 0x80);
        }
    } else {
        night = (float)(t - 0x278D0) / 4500.0f + 0.1f;
        dusk = 1.1f - night;
        l.background = Rgba(dusk * 255.0f, dusk * 161.0f, dusk * 127.0f, l.background >> 24);
        if (dry) {
            l.fogEnd = std::max(300.0f, dusk * 500.0f + 300.0f - y);
            l.fogScale = night * 0.5f + 0.5f;
            l.fogColor = Rgba(dusk * 255.0f, dusk * 161.0f, dusk * 127.0f, 0x7F);
        } else {
            l.fogEnd = 300.0f;
            l.fogScale = 1.0f;
            l.fogColor = Rgba(0, dusk * 80.0f, dusk * 63.0f, 0x7F);
        }
    }
    l.weights[0] = day;
    l.weights[1] = dusk;
    l.weights[2] = night;
    BlendSet(0, l.weights, l.sets[0]);
    BlendSet(3, l.weights, l.sets[1]);
}

void Light_CourseFog(RaceLight& l, int course) {
    const CourseFog& f = kFog[Clamp(course)];
    l.background = 0x80000000 | f.background;
    l.fogColor = 0x80000000 | f.color;
    l.fogScale = f.scale;
    l.fogEnd = f.end;
}

void Light_SetHighlight(RaceLight& l, const float* forward) {
    float h[3] = {l.dir[0][0] - forward[0], l.dir[0][1] - forward[1], l.dir[0][2] - forward[2]};
    Normalize(h);
    memcpy(l.dir[2], h, sizeof h);
}

void Light_SkyTint(const RaceLight& l, float* rgb) {
    for (int c = 0; c < 3; ++c) rgb[c] = ((l.background >> (8 * c)) & 0xFF) / 255.0f;
}

gfx::Lighting Light_CarSet(const RaceLight& l, int set) {
    gfx::Lighting g;
    memcpy(g.dir, l.dir, sizeof g.dir);
    memcpy(g.color, l.sets[set & 3], sizeof g.color);
    for (int p = 0; p < 2; ++p)
        for (int c = 0; c < 3; ++c) g.paint[p][c] = 1.0f;
    return g;
}

}  // namespace game
