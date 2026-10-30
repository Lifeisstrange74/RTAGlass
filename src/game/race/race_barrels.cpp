// Barrel Dodging (course 0x21, ACTION/A15; the Adventure's race 52, the cave in Peach Town): the player drives up a
// slope while barrels roll down it, and getting to the top without one hitting the car wins. A high-level
// reimplementation of the event task func_0025E278 and the barrels' task func_0025DF80. See docs/events.md.
#include <math.h>
#include <string.h>

#include "../../audio/sound.h"
#include "race.h"

namespace game {

namespace {

// The barrels (func_0025DF80): 32, each its own task, matrix i of D_0177EB10 (model 0 for all: ShareModel0). They
// start at z 880 + 19 i, height 88.475, in one of five lanes (D_002C4BF0) picked by D_002C4BE0[i & 15] x (info's
// frame + i) % 5. On the ledge (state 0) a barrel rolls 0.15 a frame towards -z; under z 1420 it falls (1), its speed
// down 0.1 growing 0.01 a frame while it goes on 0.1 a frame, until its bottom (1.575 under the centre) is 78 or
// lower; then (2) it rolls on the ground (the course's ground query, its centre 1.575 above it) 0.15 a frame. Under
// z 810 it's back on the ledge at z 1448.5, in lane (info's frame % 5).
const int kBarrels = 32;
const float kLanes[5] = {744.4f, 750.0f, 755.6f, 747.1f, 752.9f};
const float kPi = 3.14159f;   // D_00333110
const uint8_t kLaneMix[16] = {8, 1, 13, 4, 5, 11, 15, 9, 16, 6, 3, 10, 7, 14, 2, 12};
const int kFirstZ = 0x370, kSpacing = 0x13;
const float kTopY = 88.475f, kFall = 0.1f;            // D_00333114 (D_00333100), D_00333118 (D_00333108)
const float kLedgeSpeed = 0.15f, kLedgeEnd = 1420.0f;  // D_003330E0, D_003330E4
const float kRadius = 1.575f, kGravity = 0.01f, kFallSpeed = 0.1f, kGroundY = 78.0f;   // D_003330E8-F0
const float kRollSpeed = 0.15f, kBottom = 810.0f, kRestartZ = 1448.5f;   // D_003330F4, D_003330FC, D_00333104
// A barrel is drawn from z 820 on, or on its task's odd frames (so from its second frame).
const float kShowZ = 820.0f;
// Its centre within 3.3 of the car's place (D_0177ACE0) is a hit (D_0033310C). It turns about x a full turn every
// 40 frames: (20 - frame % 40) pi / 20.
const float kHitDistance = 3.3f;
// The goal: the car's z at 1350 or more (D_0033311C).
const float kGoalZ = 1350.0f;
const uint32_t kSeHit = 0x14;   // CQ_MAIN 0x14, the bump
// Reaching the goal sends the player on to Peach Town's place 13 (info+0x21 = 0xD), Grandpa Tal's house at the top.
const int kGoalPlace = 0xD;

}  // namespace

// The PS2's barrels skip their moves under race flag 0x8000 (and stand unturned); the port's tasks don't run while
// paused at all, and the barrels stay as they were drawn.
void Race::Barrel_Task(int i) {
    Barrel& b = barrel_.barrels[i];
    switch (b.state) {
    case 0:
        b.pos[2] -= kLedgeSpeed;
        if (b.pos[2] < kLedgeEnd) b.state = 1;
        break;
    case 1:
        b.pos[1] -= b.fall;
        b.fall += kGravity;
        b.pos[2] -= kFallSpeed;
        if (b.pos[1] - kRadius <= kGroundY) b.state = 2;
        break;
    case 2: {
        float p[4] = {b.pos[0], b.pos[1], b.pos[2], 1.0f}, ceiling;
        collision_.Collision_Ground(p, &ceiling);   // D_00335710 (func_002080D8(1): the course's)
        b.pos[1] = p[1];
        b.pos[2] -= kRollSpeed;
        b.pos[1] += kRadius;
        if (b.pos[2] < kBottom) {
            b.pos[0] = kLanes[Info_Frames() % 5];
            b.pos[1] = kTopY;
            b.pos[2] = kRestartZ;
            b.fall = kFall;
            b.state = 0;
        }
        break;
    }
    default:
        break;
    }
    float d[3] = {b.pos[0] - bodies_[0].pos[0], b.pos[1] - bodies_[0].pos[1], b.pos[2] - bodies_[0].pos[2]};
    if (sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]) < kHitDistance) {
        barrel_.hit = true;   // the parent's +4 (and +0x24)
        b.state = 3;
    }
    if (b.pos[2] >= kShowZ || (b.frame & 1)) b.shown = 1.0f;
    // sceVu0UnitMatrix, turned about x while it rolls (not once it's hit), then its place.
    float a = 0;
    if (b.state != 3) a = (20 - b.frame % 40) * kPi / 20.0f;
    float c = cosf(a), s = sinf(a);
    const float m[12] = {1, 0, 0, b.pos[0], 0, c, -s, b.pos[1], 0, s, c, b.pos[2]};
    if (b.shown != 0) objects_.SetObject(i, m);
    ++b.frame;
}

// func_0025E278. State 0 (its first frame after the setup): the barrels; 1: the car at the goal is CLEAR (Race_Outcome
// 1, and on to place 13), a barrel's hit the bump, MISS and Race_Outcome 0 with green taken away; 2: no pause and the
// car braked; 3: a count up a frame, from 0x40 fading to black, and at 0xC0 the end.
bool Race::Race_BarrelTask() {
    BarrelState& st = barrel_;
    switch (st.state) {
    case 0:
        // The task's frame 1 also gives models 1-3 model 0's place and texture.
        objects_.ShareModel0();
        for (int i = 0; i < kBarrels; ++i) {
            Barrel& b = st.barrels[i];
            memset(&b, 0, sizeof b);
            b.pos[0] = kLanes[(kLaneMix[i & 15] * (Info_Frames() + i)) % 5];
            b.pos[1] = kTopY;
            b.pos[2] = (float)(kFirstZ + kSpacing * i);
            b.fall = kFall;
        }
        st.state = 1;
        // fall through
    case 1:
        if (bodies_[0].pos[2] >= kGoalZ) {
            Event_SetClear(0);
            result_.outcome = 1;
            result_.scenePlace = kGoalPlace;
            st.state = 2;
        } else if (st.hit) {
            audio::Sound_PlaySe(kSeHit);
            Event_SetMiss(0);
            result_.outcome = 0;
            st.state = 2;
            go_ = false;   // race flag 4 off: the car can't drive on
            st.hit = false;
        }
        break;
    case 2:
        noPause_ = true;
        st.state = 3;
        braked_[0] = true;
        st.count = 0;
        // fall through
    case 3:
        if (st.count > 0x40) event_.fadeAll = st.count - 0x40;
        if (st.count < 0xC0) ++st.count;
        else st.done = true;
        break;
    default:
        break;
    }
    // The barrels' tasks run after their parent's.
    if (st.state != 0)
        for (int i = 0; i < kBarrels; ++i) Barrel_Task(i);
    return st.done;
}

}  // namespace game
