// Soccer (course 0x1A, ACTION/A08): 2 Player's row 9 and the Adventure's soccer. Two teams of three cars push a ball
// about a walled pitch; the first team to three goals wins. A high-level reimplementation of its task
// (func_00254430: the kick-offs, the goals, the end), the ball (func_00253B80 the cars, func_00253E90 the walls,
// func_00254290 its roll) and the AI routine the course row gives the cars the AI drives (func_00254CC8, with
// func_00253680, func_00253448 and func_00253230). See docs/events.md, "Soccer".
#include <math.h>
#include <string.h>

#include "../../audio/sound.h"
#include "../pad/vibration.h"
#include "race.h"

namespace game {

namespace {

const float kPi = 3.14159265f;

// D_002AC480, a 64-byte entry per place on the team (car +0x24A): the kick-off heading and spot, then for the AI the
// spot it goes back to, and where it plays: while the ball's x is inside (xMin, xMax), or the ball is within `reach`
// of the car, it goes for the ball. Team A (places 0-2) kicks off facing +x and scores past x 320; place 2 is its
// keeper. Team B (3-5) faces -x and scores past x 80.
struct SoccerPlace {
    int16_t yaw;
    float start[3];
    float home[3];
    float xMin, xMax, reach;
};
const SoccerPlace kPlaces[6] = {
    {0x4000, {180, 1, 205}, {200, 1, 200}, 80, 320, 25},
    {0x4000, {180, 1, 195}, {260, 1, 200}, 140, 320, 25},
    {0x4000, {100, 1, 200}, {100, 1, 200}, 80, 170, 10},
    {-0x4000, {220, 1, 195}, {200, 1, 200}, 80, 320, 25},
    {-0x4000, {220, 1, 205}, {140, 1, 200}, 80, 260, 25},
    {-0x4000, {300, 1, 200}, {300, 1, 200}, 230, 320, 10},
};

// The goals: the ball past x 320 is team A's, under x 80 team B's. Three wins.
const float kGoalA = 320.0f, kGoalB = 80.0f, kGoalZ = 200.0f;
const int kWinningScore = 3;

// The pitch (func_00253E90): the ends at x 82 and 318, open between z 182 and 218 back to the goals' nets at x 62
// and 338; the sides at z 122 and 278. The ball bounces off at 0.7 of its speed; it rests at height 2 (its centre),
// falls 0.0027 a frame faster above it and bounces off the ground at 0.6.
const float kEndA = 82.0f, kEndB = 318.0f, kMouth0 = 182.0f, kMouth1 = 218.0f, kNetA = 62.0f, kNetB = 338.0f;
const float kSide0 = 122.0f, kSide1 = 278.0f;
const float kWallBounce = -0.7f, kGroundBounce = -0.6f;   // D_00333064-D_0033306C, D_00333074
const float kBallHeight = 2.0f, kGravity = 0.0027222219f;  // D_00333070
// The roll's slowing each frame (D_00333098 in the air, D_0033309C on the ground).
const float kAirDrag = 0.993f, kGroundDrag = 0.97f;

// The cars (func_00253B80): a car within 2.5 of the ball's centre pushes it away from the car's centre at 0.7 of
// the ball's speed across the ground (D_00333058), plus the car's own velocity (fixed point, x D_0033305C); with the
// car's centre within 2 across the ground the push is at least 0.0926 (D_00333060) and the ball hops (0.1089 up,
// D_00333054), as it does from a car going over 50 km/h.
const float kTouch = 2.5f, kUnder = 2.0f, kKeep = 0.7f, kCarVel = 3.5095214e-05f;
const float kMinPush = 0.09259259f, kHop = 0.10888888f;
const int kHopSpeed = 50;

// The task's sounds: the ball hit (ACTION 0x0A, at most every 10 frames), the kick-off's whistle (0x0C) and a goal
// (0x0F).
const uint32_t kSeBall = 0x30A, kSeKickoff = 0x30C, kSeGoal = 0x30F;
const int kBallSoundWait = 10;

// The banners (picture 1 of SCOREA08.GSL), at (320, 224), zooming to twice their size over 30 frames: KICKOFF!
// (D_0029FAA8, palette 2) and GOAL!! (D_0029FAC8, palette 3).
const int kKickoffSprite[4] = {0, 96, 232, 54}, kGoalSprite[4] = {0, 152, 232, 48};

// Vec_SpeedKmh (func_0021D358): a fixed-point velocity's length / 32768 x 216.
float SpeedKmh(const CarBody& c) {
    float x = (float)c.vel[0], y = (float)c.vel[1], z = (float)c.vel[2];
    return sqrtf(x * x + y * y + z * z) / 32768.0f * 216.0f;
}

// The direction from a car to (x, z), less its heading, as a 16-bit angle (func_00274AF8 is atan2f).
int16_t AngleTo(const CarBody& c, float x, float z) {
    float a = atan2f(x - c.pos[0], z - c.pos[2]) / kPi * 32768.0f - (float)(int16_t)c.yaw;
    return (int16_t)(int)a;
}

float WrapPi(float a) {
    while (a < -kPi) a += 2 * kPi;
    while (a > kPi) a -= 2 * kPi;
    return a;
}

void Normalize3(float* v) {
    float l = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    if (l > 0) {
        for (int k = 0; k < 3; ++k) v[k] /= l;
    }
}

void Cross(const float* a, const float* b, float* out) {
    float r[3] = {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]};
    memcpy(out, r, sizeof r);
}

void SetBanner(HudElement& e, bool on, const int* sprite, int palette) {
    if (e.type == (on ? 3 : 0)) return;
    memset(&e, 0, sizeof e);
    if (!on) return;
    e.type = 3;
    e.x = 320;
    e.y = 0xE0;
    e.picture = 1;
    e.palette = palette;
    e.u = sprite[0];
    e.v = sprite[1];
    e.w = sprite[2];
    e.h = sprite[3];
    e.frames = 30;
    e.scale = 2.0f;
}

}  // namespace

// Hud_SetKickoff (func_002351A0) and Hud_SetGoal (func_00235238): on, or off (type 0).
void Race::Soccer_SetKickoff(bool on) { SetBanner(elements_[kElemKickoff], on, kKickoffSprite, 2); }
void Race::Soccer_SetGoal(bool on) { SetBanner(elements_[kElemGoal], on, kGoalSprite, 3); }

// Hud_SetScore (func_002350F0): the score at +0x8438. Mode 1 small at the top (x 320, or 560 in 2 Player), 2 large
// in the middle, 0 none.
void Race::Soccer_SetScore(int mode) {
    SoccerScore& s = soccer_.score;
    s.mode = mode;
    s.s0 = soccer_.goals[0];
    s.s1 = soccer_.goals[1];
    s.x = mode == 1 && players_ > 1 ? 0x230 : 0x140;
    s.y = mode == 1 ? 0x2C : 0xE0;
}

void Race::Soccer_BrakeAll(bool on) {
    for (int i = 0; i < carCount_; ++i) braked_[i] = on;
}

// func_00253B80: each car against the ball.
void Race::Ball_HitCars() {
    SoccerBall& b = soccer_.ball;
    for (int i = 0; i < carCount_; ++i) {
        const CarBody& c = bodies_[i];
        float d[3] = {b.pos[0] - c.pos[0], b.pos[1] - c.pos[1], b.pos[2] - c.pos[2]};
        if (!(sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]) < kTouch)) {
            b.touching &= ~(1u << i);
            continue;
        }
        if (!(b.touching & (1u << i))) {
            b.touching |= 1u << i;
            b.kicked = true;
            // The pad shakes for a car whose flags (+0x198) are 2 or 0, which is player 1's car (pad flags & 1 = 0)
            // and not while braked (0x800).
            if (i == 0 && !braked_[0]) Pad_Vibrate(0, kShakeBump, 0x40);
        }
        float n[3] = {d[0], d[1], d[2]};
        Normalize3(n);
        float speed = sqrtf(b.vel[0] * b.vel[0] + b.vel[2] * b.vel[2]) * kKeep;
        float v[3];
        for (int k = 0; k < 3; ++k) v[k] = n[k] * speed + (float)c.vel[k] * kCarVel;
        b.vel[0] = v[0];
        b.vel[2] = v[2];
        if (sqrtf(d[0] * d[0] + d[2] * d[2]) < kUnder) {
            if (sqrtf(v[0] * v[0] + v[2] * v[2]) < kMinPush) {
                b.vel[0] = n[0] * kMinPush;
                b.vel[2] = n[2] * kMinPush;
            }
            b.vel[1] = kHop;
        } else if (SpeedKmh(c) > kHopSpeed) {
            b.vel[1] = kHop;
        }
    }
}

// func_00253E90: the pitch's walls and the ground. In a corner (behind an end line, outside the goal's mouth) the
// ball goes back out the shorter way.
void Race::Ball_HitWalls() {
    SoccerBall& b = soccer_.ball;
    float& x = b.pos[0];
    float& z = b.pos[2];
    auto pushX = [&](float by) {
        x -= by;
        b.vel[0] *= kWallBounce;
        b.kicked = true;
    };
    auto pushZ = [&](float by) {
        z -= by;
        b.vel[2] *= kWallBounce;
        b.kicked = true;
    };
    float dx = x - kEndA;
    bool atEnd = dx < 0;
    float net = x - kNetA;
    bool inNet = net < 0;
    if (!atEnd) {
        dx = x - kEndB;
        atEnd = dx > 0;
        net = x - kNetB;
        inNet = net > 0;
    }
    if (atEnd) {
        float dz = z - kMouth0;
        bool corner = dz < 0;
        if (!corner) {
            dz = z - kMouth1;
            corner = dz > 0;
        }
        if (corner) {
            if (dx == 0) pushZ(dz);
            else if (dz == 0) pushX(dx);
            else if (fabsf(dz) < fabsf(dx)) pushZ(dz);
            else pushX(dx);
        } else if (inNet) {
            pushX(net);
        }
    }
    float dz = z - kSide0;
    if (dz < 0 || (dz = z - kSide1) > 0) pushZ(dz);
    if (b.pos[1] > kBallHeight) {
        b.vel[1] -= kGravity;
    } else {
        b.pos[1] = kBallHeight;
        b.vel[1] *= kGroundBounce;
    }
}

// func_00254290: the ball rolls: its two axes turn about z by minus its x speed and about x by its z speed (radians a
// frame, sceVu0RotMatrixZ then X).
void Race::Ball_Spin() {
    SoccerBall& b = soccer_.ball;
    float az = WrapPi(-b.vel[0]), ax = WrapPi(b.vel[2]);
    float cz = cosf(az), sz = sinf(az), cx = cosf(ax), sx = sinf(ax);
    for (float* v : {b.up, b.dir}) {
        float x = v[0] * cz - v[1] * sz, y = v[0] * sz + v[1] * cz, z = v[2];
        v[0] = x;
        v[1] = y * cx - z * sx;
        v[2] = y * sx + z * cx;
        Normalize3(v);
    }
}

// The task's ball part, every frame it isn't paused: the cars, the drag, the move, the walls, the roll, the hit's
// sound; then the ball's matrix (Matrix_Billboard on its axes, at its position) is the course's object 0
// (D_0177EB10), A08's ball.
void Race::Ball_Step() {
    SoccerBall& b = soccer_.ball;
    Ball_HitCars();
    float drag = b.pos[1] > kBallHeight ? kAirDrag : kGroundDrag;
    b.vel[0] *= drag;
    b.vel[2] *= drag;
    for (int k = 0; k < 3; ++k) b.pos[k] += b.vel[k];
    Ball_HitWalls();
    Ball_Spin();
    if (b.soundWait) --b.soundWait;
    if (b.kicked) {
        b.kicked = false;
        if (b.soundWait == 0) {
            b.soundWait = kBallSoundWait;
            audio::Sound_PlaySe(kSeBall);
        }
    }
    // Matrix_Billboard: x = up x dir (normalised), y = up, z = x x y; the columns here.
    float ax[3], az[3];
    Cross(b.up, b.dir, ax);
    Normalize3(ax);
    Cross(ax, b.up, az);
    const float m[12] = {ax[0], b.up[0], az[0], b.pos[0], ax[1], b.up[1], az[1], b.pos[1],
                         ax[2], b.up[2], az[2], b.pos[2]};
    objects_.SetObject(0, m);
}

// Race_SoccerTask's state 0 (func_00254430): every car back to its place (Car_ResetForRespawn, Car_SetPlace), braked;
// the ball in the middle, at rest. In 2 Player the cars (player 1, player 2, then their teammates in turn) take the
// places 0, 3, 1, 4, 2, 5.
void Race::Soccer_Kickoff() {
    for (int i = 0; i < carCount_ && i < 6; ++i) {
        int place = players_ > 1 ? i / 2 + (i & 1) * 3 : i;
        soccer_.place[i] = place;
        Car_Reset(bodies_[i], kPlaces[place].start, (uint16_t)kPlaces[place].yaw);
        Car_WorldPosition(bodies_[i], cars_[i].pos);
    }
    SoccerBall& b = soccer_.ball;
    memset(&b, 0, sizeof b);
    b.up[1] = 1.0f;
    b.dir[2] = 1.0f;
    b.pos[0] = 200.0f;
    b.pos[1] = kBallHeight;
    b.pos[2] = 200.0f;
    Soccer_BrakeAll(true);
}

// func_00254430, a frame. The states, each counting its frames (+4) and going on to the next on the same frame:
//   0  the kick-off: race flag 4 off and 0x10000 (no map, meter or canvas) on, Soccer_Kickoff;
//   1  8 frames black;  2  fading in over 0x40, then the HUD back and the score at the top;
//   3  0x20 frames, then KICKOFF! and the whistle;  4  0x80 frames, then green and the pause (race flags 0x104), the
//      brakes off, KICKOFF! gone, and the song the first time;
//   5  play until the ball is past a goal line: no pause, the goal's sound, the score, GOAL!!;
//   6  0x80 frames, then GOAL!! goes and the score shows large in the middle (7 holds it 0x80 frames; the next
//      state starts on the next frame);
//   7  then the score goes: at three, the winner's WIN! and the other's LOSE (Race_Outcome 2: team A, player 1's,
//      won; 1: team B) and 9; else the HUD off, every car braked, and 8;
//   8  fading to black over 0x80, then 0 again;
//   9  0x100 frames, then the banners go, the HUD off, every car braked, the song fading out; 10 fading to black
//      over 0x80, the song stopped, and the end.
// While the race is paused the canvas is hidden (HudCanvas_SetHidden) and nothing moves.
bool Race::Race_SoccerTask() {
    SoccerState& s = soccer_;
    int& t = s.count;
    switch (s.state) {
    case 0:
        go_ = false;
        s.hud = false;
        Soccer_Kickoff();
        s.state = 1;
        t = 0;
        // fall through
    case 1:
        event_.fadeAll = 0x80;
        if (t++ < 8) break;
        s.state = 2;
        t = 0;
        // fall through
    case 2:
        event_.fadeAll = (0x40 - t) * 2;
        if (t++ < 0x40) break;
        s.hud = true;
        Soccer_SetScore(1);
        s.state = 3;
        t = 0;
        // fall through
    case 3:
        if (t++ < 0x20) break;
        Soccer_SetKickoff(true);
        audio::Sound_PlaySe(kSeKickoff);
        s.state = 4;
        t = 0;
        // fall through
    case 4:
        if (t++ < 0x80) break;
        go_ = true;
        noPause_ = false;
        Soccer_BrakeAll(false);
        Soccer_SetKickoff(false);
        if (s.goals[0] + s.goals[1] == 0) audio::Sound_StartSong();
        t = 0;
        s.state = 5;
        // fall through
    case 5: {
        float x = s.ball.pos[0];
        int team = x > kGoalA ? 0 : x < kGoalB ? 1 : -1;
        if (team < 0) break;
        noPause_ = true;
        audio::Sound_PlaySe(kSeGoal);
        s.scorer = team;
        ++s.goals[team];
        Soccer_SetGoal(true);
        Soccer_SetScore(0);
        s.state = 6;
        t = 0;
    }
        // fall through
    case 6:
        if (t++ < 0x80) break;
        Soccer_SetGoal(false);
        Soccer_SetScore(2);
        s.state = 7;
        t = 0;
        break;
    case 7:
        if (t++ < 0x80) break;
        Soccer_SetScore(0);
        if (s.goals[0] == kWinningScore) {
            Event_SetWin(0);
            result_.outcome = 2;
            if (players_ > 1) Event_SetLose(1);
        } else if (s.goals[1] == kWinningScore) {
            Event_SetLose(0);
            result_.outcome = 1;
            if (players_ > 1) Event_SetWin(1);
        } else {
            s.hud = false;
            Soccer_BrakeAll(true);
            t = 0;
            s.state = 8;
            break;
        }
        s.state = 9;
        t = 0;
        break;
    case 8:
        event_.fadeAll = t;
        if (t++ < 0x80) break;
        s.state = 0;
        t = 0;
        break;
    case 9:
        if (t++ < 0x100) break;
        for (int p = 0; p < 2; ++p) elements_[kElemWin + p * 2].type = elements_[kElemLose + p * 2].type = 0;
        noPause_ = true;
        s.hud = false;
        Soccer_BrakeAll(true);
        audio::Sound_FadeOutSong();
        s.state = 10;
        t = 0;
        break;
    case 10:
        event_.fadeAll = t;
        if (t++ < 0x80) break;
        audio::Sound_StopSong();
        s.state = -1;
        t = 0;
        s.done = true;
        break;
    default:
        break;
    }
    Ball_Step();
    return s.done;
}

// func_00253230: steers car `car` by `angle` (where it wants to go, less its heading) through its AI slot's gear:
// 0 brakes to a stop and then picks forward (1) or reverse (2) by whether the angle is within a quarter turn; 1
// drives forward, steering while the angle is over 0x80, and brakes back to 0 when it is past 0x6000; 2 reverses,
// steering the other way while the angle is over 0x2000, and brakes back to 0 once it's within that. Over `cap`
// km/h (0: none) it lets go of the pedals and only steers. True when it stopped in gear 0.
bool Race::Soccer_Steer(int car, int angle, int cap) {
    SoccerAi& a = soccer_.ai[soccer_.place[car]];
    const CarBody& c = bodies_[car];
    bool stopped = false;
    switch (a.gear) {
    case 0:
        a.bits = kCarBrake;
        if (SpeedKmh(c) == 0.0f) {
            stopped = true;
            a.gear = angle <= 0x4000 && angle >= -0x4000 ? 1 : 2;
        }
        break;
    case 1:
        if (angle > 0x6000 || angle < -0x6000) {
            a.gear = 0;
            a.bits = kCarBrake;
        } else if (angle > 0x80) {
            a.bits = kCarAccel | kCarRight;
        } else if (angle < -0x80) {
            a.bits = kCarAccel | kCarLeft;
        } else {
            a.bits = kCarAccel;
        }
        break;
    case 2:
        if (angle > 0x2000) {
            a.bits = kCarAccel | kCarReverse | kCarLeft;
        } else if (angle < -0x2000) {
            a.bits = kCarAccel | kCarReverse | kCarRight;
        } else {
            a.bits = kCarBrake;
            a.gear = 0;
        }
        break;
    default:
        break;
    }
    if (cap > 0 && cap < (int)SpeedKmh(c)) a.bits &= kCarLeft | kCarRight;
    return stopped;
}

// func_00253448: for the ball. The car aims 2.3 behind the ball on the line from the goal it attacks (x 320 for team
// A, 80 for team B; z 200). Facing along the ball's line to the goal (within 0x800) it goes at up to 100 km/h; else
// flat out from further than 20, 45 km/h nearer.
void Race::Soccer_Attack(int car) {
    const SoccerBall& b = soccer_.ball;
    const CarBody& c = bodies_[car];
    float goalX = soccer_.place[car] < 3 ? kGoalA : kGoalB;
    float line = atan2f(goalX - b.pos[0], kGoalZ - b.pos[2]) / kPi * 32768.0f - (float)(int16_t)c.yaw;
    int16_t toGoal = (int16_t)(int)line;
    float back[3] = {b.pos[0] - goalX, b.pos[1], b.pos[2] - kGoalZ};
    Normalize3(back);
    float target[3];
    for (int k = 0; k < 3; ++k) target[k] = b.pos[k] + back[k] * 2.3f;   // D_0033304C
    int16_t angle = AngleTo(c, target[0], target[2]);
    if (toGoal > -0x800 && toGoal < 0x800) {
        Soccer_Steer(car, angle, 100);
        return;
    }
    float dx = target[0] - c.pos[0], dz = target[2] - c.pos[2];
    Soccer_Steer(car, angle, sqrtf(dx * dx + dz * dz) > 20.0f ? 0 : 45);
}

// func_00254CC8 with func_00253680: the input bits for a car the AI drives, none before green. Each car's slot
// (+0x70 + 12 x its place) has a mode: 0 goes for the ball while it's in the car's part of the pitch (or within its
// reach), else 1 drives back to its spot (10 km/h within 20 of it, stopping there), then 2 turns to its kick-off
// heading at 10 km/h and stops: and the ball coming into its part sends it back to 0.
uint32_t Race::Soccer_CarAi(int car) {
    if (!go_) return 0;
    const int place = soccer_.place[car];
    SoccerAi& a = soccer_.ai[place];
    const SoccerPlace& p = kPlaces[place];
    const CarBody& c = bodies_[car];
    const SoccerBall& b = soccer_.ball;
    float bx = b.pos[0] - c.pos[0], bz = b.pos[2] - c.pos[2];
    bool inPlay = (p.xMin < b.pos[0] && b.pos[0] < p.xMax) || sqrtf(bx * bx + bz * bz) < p.reach;
    switch (a.mode) {
    case 0:
        if (inPlay) {
            Soccer_Attack(car);
        } else {
            a.gear = 0;
            a.mode = 1;
        }
        break;
    case 1: {
        if (inPlay) {
            a.gear = 0;
            a.mode = 0;
            break;
        }
        int16_t angle = AngleTo(c, p.home[0], p.home[2]);
        float dx = p.home[0] - c.pos[0], dz = p.home[2] - c.pos[2];
        bool stopped = false;
        if (sqrtf(dx * dx + dz * dz) < 20.0f) {
            a.gear = 0;
            stopped = Soccer_Steer(car, angle, 10);
        } else {
            Soccer_Steer(car, angle, 0);
        }
        if (stopped) {
            a.gear = 0;
            a.mode = 2;
        }
        break;
    }
    case 2: {
        if (inPlay) {
            a.gear = 0;
            a.mode = 0;
            break;
        }
        int16_t angle = (int16_t)(p.yaw - (int16_t)c.yaw);
        bool stopped = false;
        if (angle > -0x400 && angle < 0x400) {
            a.gear = 0;
            stopped = Soccer_Steer(car, angle, 10);
        } else {
            Soccer_Steer(car, angle, 10);
        }
        if (stopped) {
            a.gear = 0;
            a.mode = 0;
        }
        break;
    }
    default:
        break;
    }
    return a.bits;
}

}  // namespace game
