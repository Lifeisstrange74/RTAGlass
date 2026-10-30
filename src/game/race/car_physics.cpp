#include "car_physics.h"

#include <math.h>
#include <string.h>

#include "../extras.h"
#include "../pad/vibration.h"
#include "../../game_text.inc"   // the game's text, GT_<address>_<length>, made from the ELF

namespace game {

namespace {

// --- The parts tables (from the executable) ---

// D_002ECA70 + 0xC: the tires' grip by surface type (the table's own entries 6 and 7 are 0).
const int16_t kTires[11][6] = {
    {196, 168, 128, 168, 96, 64},  {240, 196, 154, 160, 96, 64},  {333, 168, 154, 140, 80, 64},
    {512, 102, 102, 100, 51, 64},  {512, 160, 102, 128, 51, 64},  {240, 196, 196, 160, 128, 64},
    {240, 196, 240, 160, 128, 64}, {196, 196, 160, 160, 128, 64}, {240, 240, 160, 180, 168, 128},
    {196, 196, 160, 168, 168, 196}, {240, 196, 160, 168, 196, 196},
};
// D_002ECED0 + 0xC / + 0x10: the engines' force and fuel use.
const int32_t kEngineTorque[11] = {1500, 1800, 2200, 2600, 2900, 3300, 3600, 3900, 4200, 4500, 6000};
const int16_t kEngineFuel[11] = {3, 4, 5, 6, 7, 8, 6, 8, 12, 16, 20};
// D_002ED204: the bodies' mass.
const int32_t kWeights[5] = {25, 22, 20, 18, 15};
// D_002ED334: gearboxes, reverse first.
const int16_t kGearboxes[6][8] = {
    {-95, 116, 162, 227, 318, 446, 0, 0}, {-95, 116, 182, 291, 408, 490, 0, 0},
    {-95, 128, 220, 276, 387, 464, 557, 0}, {-95, 128, 260, 327, 458, 550, 660, 0},
    {-95, 144, 300, 414, 539, 647, 711, 0}, {-95, 156, 350, 446, 550, 625, 750, 0},
};
// D_002ED4F4: steering.
const int16_t kSteering[4] = {64, 96, 128, 160};
// D_002ED5DC: brakes, the force by frames held.
const uint8_t kBrakes[3][32] = {
    {1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 3, 4, 4, 4, 5, 5, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24, 26, 28, 30, 32},
    {4, 7, 10, 12, 14, 16, 17, 19, 20, 21, 22, 23, 24, 25, 25, 26, 26, 27, 28, 28, 28, 29, 29, 30, 30, 30, 31, 31,
     31, 32, 32, 32},
    {0, 1, 1, 1, 1, 2, 2, 2, 3, 3, 4, 4, 4, 5, 6, 6, 7, 8, 8, 9, 10, 11, 12, 13, 15, 16, 18, 20, 22, 25, 28, 32},
};

// D_002BE318: the player's parts in the Quick Race, by card (16-byte records laid out as SaveCar::parts: +0xD the
// horn, +0xE the part flags); and each card's course table row's (+4, +4) for its opponents: card 5's (Lagoon)
// have the propellers and the fins (0x1040), card 6's (Tin Raceway) the jet turbines and the fins (0x3000).
const uint8_t kQuickRacePlayer[8][16] = {
    {0x2B, 0, 1, 0, 0, 1, 0}, {0x00, 8, 4, 1, 4, 2, 0}, {0x75, 5, 4, 1, 4, 2, 0}, {0x8C, 9, 4, 1, 4, 2, 0},
    {0x72, 3, 8, 2, 5, 2, 0}, {0x5D, 7, 3, 1, 4, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0x40},
    {0x00, 3, 8, 1, 5, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x20}, {0x00, 4, 8, 2, 5, 2, 0},
};
// D_002BE278: 2 Player's parts in Race Right-Away and Random Race (modes 2 and 3), by course row, the same for all
// six cars (Car_Init); Custom Race (mode 6) uses each car's own SaveCar::parts. Row 8's part flags are 0x2000.
const uint8_t kVersusParts[10][16] = {
    {0, 2, 4, 1, 3, 2}, {0, 7, 4, 1, 3, 2}, {0, 1, 4, 1, 3, 2}, {0, 6, 4, 1, 3, 2}, {0, 6, 4, 1, 3, 2},
    {0, 2, 4, 1, 3, 2}, {0, 0, 4, 1, 3, 2}, {0, 3, 4, 1, 3, 2}, {0, 8, 5, 2, 3, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x20},
    {0, 2, 4, 1, 3, 2},
};
const uint8_t kQuickRaceOpponents[8][16] = {
    {0x36, 0, 2, 2, 0, 0, 0}, {0x41, 8, 9, 4, 5, 3, 3}, {0x50, 8, 9, 4, 5, 3, 3}, {0x46, 10, 9, 4, 5, 3, 3},
    {0x5A, 4, 5, 3, 3, 2, 0}, {0x41, 8, 9, 4, 5, 3, 3, 0, 0, 0, 0, 0, 0, 0, 0x40, 0x10},
    {0x50, 4, 10, 4, 5, 3, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0x30}, {0x50, 4, 10, 4, 5, 3, 3},
};

// --- Constants ---

const float kFixed = 20971.52f;    // D_00332C0C: fixed point per world unit
const float kPi = 3.14159265f;     // D_00332C04
const int32_t kGravity = 0x59;     // per frame, in velocity units
const int32_t kPropellerPush = 0x59;   // Car_Update: part 0x40's push in water, a frame
const int32_t kJetFuel = 100;      // Car_Jet: the fuel a frame of the jet burns
const int32_t kJetTop = 0xCF69;    // with the jet turbines, the forward speed is held to this
const int32_t kClearance = 0x51EB; // the suspension's reach (0.64 units)
const int32_t kClearanceBig = 0xA3D7;   // with the big tires (1.28 units)

uint16_t g_playerFlags = 0;        // D_0177EA50[0]'s part flags (Car_SetPlayerFlags)
int32_t g_clock = 0x1A5E0;         // the time of day (Car_SetClock); midday until one is set

// D_0029CA80: the ground probes in the car's frame: the front axle's middle, the rear wheels, the four corners.
const float kProbe[7][3] = {
    {0.0f, 0.0f, 0.68f}, {-0.74f, 0.0f, -0.66f}, {0.74f, 0.0f, -0.66f}, {-0.9f, 0.0f, 1.5f},
    {0.9f, 0.0f, 1.5f},  {-0.9f, 0.0f, -1.5f},   {0.9f, 0.0f, -1.5f},
};

// --- Arithmetic as the EE does it ---

int32_t Div(int32_t a, int32_t b) { return b ? (int32_t)((int64_t)a / b) : 0; }   // (the EE traps on 0)
int32_t Mul(int32_t a, int32_t b) { return (int32_t)((int64_t)a * b); }
int32_t SubSat(int32_t a, int32_t b) {   // psubsw
    int64_t d = (int64_t)a - b;
    return d > 0x7FFFFFFF ? 0x7FFFFFFF : d < -0x7FFFFFFFLL - 1 ? (int32_t)0x80000000 : (int32_t)d;
}

// Fixed_Rotate (func_0021D2D8): a fixed-point vector through a rotation (rows = axes): out = x row0 + y row1 + z row2, worked out
// on 1/32768 floats as the VU0 does (itof15 / ftoi15).
CarDebugPrint g_debugPrint = nullptr;
bool g_debugVectors = false;
int g_heightKey = 0;
uint32_t g_cheats = 0;   // Car_SetCheats

// kGravity, or a third of it for a car with the Low Gravity cheat.
int32_t Gravity() { return (g_cheats & kCarCheatGravity) ? kGravity / 3 : kGravity; }

// D_002EAD90, "%s %6d%6d%6d %d": a vector's z, x, y and w at (300, y).
void Debug_PrintVector(int y, const char* label, const int32_t* v) {
    if (g_debugPrint && g_debugVectors) g_debugPrint(300, y, "%s %6d%6d%6d %d", label, v[2], v[0], v[1], v[3]);
}

void Fixed_Rotate(const float m[3][4], const int32_t* in, int32_t* out) {
    float v[3] = {in[0] / 32768.0f, in[1] / 32768.0f, in[2] / 32768.0f};
    int32_t r[4];
    for (int k = 0; k < 4; ++k) r[k] = (int32_t)((m[0][k] * v[0] + m[1][k] * v[1] + m[2][k] * v[2]) * 32768.0f);
    memcpy(out, r, sizeof r);
}

void Normalize(float* v) {   // func_002727E8
    float l = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    if (l > 0) {
        v[0] /= l;
        v[1] /= l;
        v[2] /= l;
    }
}

void Cross(const float* a, const float* b, float* out) {   // func_002727A0
    float r[3] = {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]};
    memcpy(out, r, sizeof r);
}

// A vector turned by `angle` about y (sceVu0RotMatrixY's rows (c, 0, -s), (0, 1, 0), (s, 0, c) applied to it).
void TurnY(float* v, float angle) {
    float s = sinf(angle), c = cosf(angle);
    float x = v[0] * c + v[2] * s, z = -v[0] * s + v[2] * c;
    v[0] = x;
    v[2] = z;
}

}  // namespace

// Matrix_FromUp (func_00208008): rows from an up vector: x = normalize(up.y, -up.x, 0), y = up, z = x cross y.
void Matrix_FromUp(float m[3][4], const float* up) {
    float x[3] = {up[1], -up[0], 0};
    Normalize(x);
    for (int k = 0; k < 3; ++k) {
        m[0][k] = x[k];
        m[1][k] = up[k];
    }
    Cross(m[0], m[1], m[2]);
    m[0][3] = m[1][3] = m[2][3] = 0;
}

// Matrix_TurnY (func_00208080): the car turned by `angle` about its own y: rows (c x - s z, y, s x + c z).
void Matrix_TurnY(float m[3][4], float angle) {
    float s = sinf(angle), c = cosf(angle);
    for (int k = 0; k < 3; ++k) {
        float x = m[0][k], z = m[2][k];
        m[0][k] = c * x - s * z;
        m[2][k] = s * x + c * z;
    }
}

namespace {

void Transpose(const float m[3][4], float out[3][4]) {   // func_00272868 for a rotation
    for (int r = 0; r < 3; ++r)
        for (int k = 0; k < 3; ++k) out[r][k] = m[k][r];
    out[0][3] = out[1][3] = out[2][3] = 0;
}

float YawRadians(uint16_t yaw) { return (int16_t)yaw * kPi / 32768.0f; }

// The car's float position (+0x90) from its fixed-point one: x takes the 0x2000000 bit of z as half a wrap.
void UpdatePosition(CarBody& c) {
    c.pos[0] = (float)((c.fixed[0] + ((c.fixed[2] & 0x2000000) >> 1)) & 0x1FFFFFF) / kFixed;
    c.pos[1] = (float)c.fixed[1] / kFixed;
    c.pos[2] = (float)(c.fixed[2] & 0x1FFFFFF) / kFixed;
    c.pos[3] = 1.0f;
}

// --- The update's parts ---

// Car_AutoGear (0x0021CE48, the race's D_003356D8): Circle (reverse) selects reverse; from reverse, Cross alone
// selects first. Accelerating shifts up once the engine would turn faster than 8201 and down (from third up) once
// it turns slower than 5000 and the lower gear would stay under 8000; braking shifts down under 8000.
int8_t Car_AutoGear(int8_t gear, int32_t speed, uint32_t input, const int16_t* gears) {
    if (input & kCarReverse) return 0;
    int g = gear;
    if (g == 0) g = (input & (kCarAccel | kCarReverse)) == kCarAccel ? 1 : 0;
    int32_t s = speed << 7;
    if ((input & kCarAccel) && g != 0) {
        if (Div(s, gears[g]) >= 0x2009 && !(input & kCarBrake)) {
            if (g < 6 && gears[g + 1] > 0) return (int8_t)(g + 1);
            return (int8_t)g;
        }
        if (g < 3) return (int8_t)g;
        if (Div(s, gears[g]) < 0x1388 && Div(s, gears[g - 1]) < 0x1F40) return (int8_t)(g - 1);
        return (int8_t)g;
    }
    if (input & kCarBrake) {
        if (g < 2) return (int8_t)g;
        if (Div(s, gears[g - 1]) < 0x1F40) --g;
    }
    return (int8_t)g;
}

// Car_Engine (0x00218EE0): the engine and the driven tires. The engine pushes with torque x 128 / ratio below the
// rev limit (9000 accelerating, 3000 out of fuel, 0 off the throttle: then it brakes). The tires couple the engine's
// speed (rpm x ratio / 128) to the car's: their force is the slip times the mass, capped at the grip times the mass
// on both axes (a friction circle; the excess is the squeal, returned). The brake holds the engine back. `out`
// gets the new local velocity (x, z). Before the start (`go` clear) the car is held.
int32_t Car_Engine(int32_t* out, CarBody& c, const CarParts& p, int32_t vz, int32_t vx, int32_t grip, int32_t brake,
                   uint32_t input, bool go) {
    int32_t r12 = 0, r13 = 0, limit = 0;
    int16_t rpm = c.rpm;
    // The Uncapped Speed cheat: in top gear the limiter (and the fuel's cut-off past 0x251D) moves up to 32000.
    const bool uncapped = (g_cheats & kCarCheatUncapped) && c.gear >= 1 && c.gear < 7 && p.gears[c.gear + 1] == 0;
    const int16_t kUncappedRpm = 32000;
    if ((rpm < 0x251D || (uncapped && rpm < kUncappedRpm)) && (input & kCarAccel)) {
        c.fuel -= p.fuelUse;
        if (c.fuel < 0) {
            c.fuel = 0;
            limit = 0xBB8;
        } else {
            limit = uncapped ? kUncappedRpm : 0x2328;
        }
    }
    const int32_t torque = (g_cheats & kCarCheatPower) ? p.torque * 2 : p.torque;
    int32_t ratio = p.gears[c.gear];
    int32_t mass = p.mass;
    int32_t r20 = Div(0x280, ratio);
    if (r20 < 0) r20 = -r20;
    if (r20 == 0) r20 = 1;
    int32_t r17 = go ? Mul(grip, mass) : 0;
    int32_t m5 = (go && grip) ? mass : 0;
    int32_t r22 = rpm < limit ? Div(torque << 7, ratio) : limit < rpm ? Div((int32_t)0xFFFC1800, ratio) : 0;
    int32_t r7 = 0;
    if (r17 != 0) {
        r13 = vz - Mul(rpm, ratio) / 128;
        r7 = Mul(r13, m5);
    }
    int32_t sp48 = Div(r22 + r7, mass);
    int32_t r21 = sp48;
    int32_t r3 = Mul(Mul(rpm, ratio) / 128 + r21, m5);
    int32_t r9;
    if (r3 >= 0) {
        r9 = r3 < brake ? r3 : brake;
    } else {
        r9 = -brake;
        if (r9 < r3) r9 = r3;
    }
    r22 -= r9;
    int32_t r30 = Div(r22, mass);
    int32_t r23 = r22 - r7;
    int32_t r16 = Mul(-vx, m5);
    int32_t side = 0, squeal = 0;
    if (r17 < 0) {
        side = r16;
    } else if (r17 > 0) {
        float f20 = (float)r23, f0 = (float)r16, f21 = (float)r17;
        float f1 = sqrtf(f20 * f20 + f0 * f0);
        if (f21 < f1) {
            int32_t two = r17 * 2;
            if (r16 >= 0) side = two < r16 ? two : r16;
            else side = two < -r16 ? -two : r16;
            squeal = (int32_t)(f1 / f21);
        } else {
            side = r16;
        }
    }
    if (grip < 0) {
        if ((int32_t)c.rpm + Div(sp48 << 7, ratio) < 0) r30 = -vz;
    } else if (r17 < r23) {
        r12 = r22 - r17;
        r21 = Div(r17, mass);
        if (r13 < 0 && r21 + Div(r12, r20) < r13) {
            r21 = r13;
            r12 = 0;
        }
        r30 = Div(r17, mass);
    } else if (r23 < -r17) {
        r12 = r22 + r17;
        r21 = Div(-r17, mass);
        if (r13 > 0 && r13 < r21 + Div(r12, r20)) {
            r21 = r13;
            r12 = 0;
        }
        r30 = Div(-r17, mass);
    }
    out[2] = vz + r30;
    out[0] = vx + Div(side, mass);
    c.events |= 2;
    if (g_debugPrint && g_debugVectors) g_debugPrint(300, 120, "FX %d %d", r22, r7);
    int16_t next = (int16_t)(uint16_t)((uint16_t)c.rpm + Div((r21 + Div(r12, r20)) << 7, ratio));
    if (uncapped || c.rpm > 0x2710) {
        // (the cheat) Worked out without the 16-bit wrap, which the rpm past 0x2710 could reach.
        int32_t wide = c.rpm + Div((r21 + Div(r12, r20)) << 7, ratio);
        next = (int16_t)(wide > kUncappedRpm ? kUncappedRpm : wide < 0 ? 0 : wide);
    }
    if (next >= 0x2711 && !uncapped) next = 0x2710;
    if (next < 0) next = 0;
    c.rpm = next;
    if (go) {
        c.tire = Mul(c.rpm, ratio) / 128;
    } else {
        c.tire = 0;
        squeal = 0;
        out[0] = out[2] = 0;
    }
    if (g_debugPrint && g_debugVectors) {
        g_debugPrint(300, 136, GT_002EADC0_14, c.tire, c.rpm);
        g_debugPrint(300, 152, GT_002EADD0_9, squeal);
    }
    return squeal;
}

// Car_Slide (0x0021A088): sliding. When the turn asks more of the tires than their grip, the slide rate builds (by
// up to 8 a frame, to a third of the excess, at most 0x200) and the car turns further than its wheels point, which
// the slide angle remembers. Off the brake the rate settles to an eighth; the angle wears off faster the slower the
// wheels turn.
void Car_Slide(CarBody& c, int16_t yawStep, int32_t grip, int32_t brake, int32_t lateral) {
    uint16_t d6 = (uint16_t)c.slide;
    if ((uint16_t)(d6 + 0xFFF) < 0x1FFF) {
        if (grip < 0) {
            c.slideRate = 0;
        } else if (lateral < -grip || grip < lateral) {
            int32_t t;
            if (lateral < 0) {
                t = (lateral - grip) / 3;
                if (t < -0x200) t = -0x200;
                if (t - c.slideRate < 9) c.slideRate = (int16_t)t;
                else c.slideRate = (int16_t)(c.slideRate - 8);
            } else {
                t = (lateral + grip) / 3;
                if (t >= 0x201) t = 0x200;
                if (t - c.slideRate < 9) c.slideRate = (int16_t)t;
                else c.slideRate = (int16_t)(c.slideRate + 8);
            }
        } else {
            c.slideRate = 0;
        }
    }
    int32_t wheel = c.wheel;
    if (wheel == 0) c.slideRate = 0;
    int16_t s6 = (int16_t)d6;
    int16_t rate = c.slideRate;
    if ((s6 < 0 && yawStep > 0) || (s6 > 0 && yawStep < 0)) {
        c.events |= 0x20;
        c.slide = (int16_t)(d6 + rate / 2);
    } else {
        c.slide = (int16_t)(d6 + rate);
    }
    c.yaw = (uint16_t)(c.yaw + rate);
    if (brake < 0x1388) {
        rate = (int16_t)(rate / 8);
        c.slideRate = rate;
    } else {
        int16_t s = c.slide;
        bool add = s < 0 ? c.turn < 0 : c.turn > 0 ? true : s > 0 ? false : c.turn < 0;
        if (add) c.yaw = (uint16_t)(c.yaw + rate / 8);
    }
    if (rate != 0) return;
    uint16_t r10 = (uint16_t)c.slide;
    int32_t r5;
    if (c.tire == 0) {
        r5 = wheel != 0 ? 0 : c.slide;
    } else if (wheel == 0) {
        r5 = c.slide;
    } else {
        r5 = 0x1EC000 / wheel;
        if (r5 < 0) r5 = -r5;
        int16_t s4 = c.slide;
        if (s4 < 0) {
            r5 = -r5;
            if (r5 < s4) r5 = s4;
        } else if (s4 < r5) {
            r5 = s4;
        }
    }
    if (r5 < -0x400) r5 = -0x400;
    if (r5 >= 0x401) r5 = 0x400;
    c.slide = (int16_t)(r10 - r5);
}

// The first half of Car_Sound (func_00217C68), which Car_Drive calls for every car with the squeal: the effect's
// frames (+0x208) count up while the tires squeal (more than 0x40), while the driven tires turn with a rear wheel
// down on dirt, snow or a river bed (surface classes 1, 2 and 6), or while the car is under water on a river bed;
// otherwise a running count is negated, which car_effects.cpp reads as the effect dying away. The sounds are the
// other half (car_effects.cpp).
void Car_EffectTime(CarBody& c) {
    int ground = c.surface[0] & 0x700, kind = ground == 0x200 ? 3 : ground == 0x100 ? 1 : ground == 0x600 ? 2 : 0;
    bool more = c.squeal > 0x40;
    if (!more && c.tire != 0) {
        if (kind != 0 && (c.load[1] | c.load[2]) != 0) more = true;
        else if (c.events & 0x40) more = true;
    }
    if (!more && kind == 2 && c.water == 1) more = true;
    if (more) ++c.effect;
    else c.effect = -c.effect;
}

// Car_Drive (0x0021A310): gears, steering, brakes, the engine and tires, and turning. The steering angle grows by
// one a frame to +-0x20 while held and returns by 4 a frame. The turn's radius is the speed the steering started at
// times 0x28BE over the angle (at least 0x18000), and the car turns by the driven wheels' speed over the radius.
// The driven wheels follow the car's speed while the tires bear weight, and the brake slows them.
void Car_Drive(CarBody& c, const CarParts& p, const int32_t* local, uint32_t input, int32_t* accel, bool go) {
    int32_t out[4] = {0, 0, 0, 0};
    int32_t loaded = (c.load[1] == 0 && c.load[2] == 0) ? 0 : kGravity;
    int32_t ay = accel[1];
    c.gear = Car_AutoGear(c.gear, local[2], input, p.gears);
    int32_t speed = local[2] < 0 ? -local[2] : local[2];
    if (speed < c.turnSpeed) c.turnSpeed = speed;

    int16_t st = c.steerAngle;
    if (input & kCarLeft) {
        if (st > 0) {
            c.turnSpeed = speed;
            st = (int16_t)-st;
        } else if (--st < -0x20) {
            st = -0x20;
        }
    } else if (input & kCarRight) {
        if (st >= 0) {
            if (++st >= 0x21) st = 0x20;
        } else {
            c.turnSpeed = speed;
            st = (int16_t)-st;
        }
    } else if (speed != 0) {
        if (st >= 4) st -= 4;
        else if (st > 0) st = 0;
        else if (st < -3) st += 4;
        else if (st < 0) st = 0;
        c.turnSpeed = speed;
    }
    c.steerAngle = st;
    int32_t steer = Mul(p.steer, st) / 32;
    int16_t turn = 0;
    if (steer != 0) {
        int32_t radius = Div(Mul(c.turnSpeed, 0x28BE), steer);
        if (steer > 0 && radius <= 0x17FFF) radius = 0x18000;
        if (steer < 0 && radius > -0x18000) radius = -0x18000;
        turn = (int16_t)Div(0x1B4C14B6, radius);
    }
    c.turn = turn;
    if (turn == 0) accel[0] = 0;

    int32_t brake = 0;
    if (!(input & kCarBrake)) {
        c.brakeTime = 0;
    } else {
        if (c.brakeTime < 0x20) ++c.brakeTime;
        brake = (p.brakes[c.brakeTime - 1] * 10000) >> 5;
    }
    int32_t tireGrip = p.grip[c.surface[0] & 7];
    if (g_cheats & kCarCheatGrip) tireGrip *= 2;
    int32_t grip = Mul(loaded, tireGrip) / 256;
    c.squeal = Car_Engine(out, c, p, local[2], local[0], grip, brake, input, go);
    Car_EffectTime(c);

    int32_t mass = p.mass, wheel = c.wheel, vz = local[2];
    int32_t held = wheel != 0 ? brake : 0;
    int32_t cap = Mul(Mul(ay, tireGrip) / 256, mass);
    int32_t w = held == 0 ? wheel : 0;
    if (cap != 0) {
        w = vz;
        if (held != 0) {
            int32_t v = Mul(vz - wheel, mass);
            if (cap >= 0) {
                if (v < -cap) v = -cap;
                if (cap < v) v = cap;
            }
            int32_t q = Div(v, mass) + wheel, b = Div(held, mass);
            if (q >= 0) {
                if (q < b) b = q;
            } else {
                b = -b < q ? q : -b;
            }
            w = q - b;
        }
    }
    c.wheel = w;

    int32_t step = 0;
    if (c.load[0] != 0 || c.load[1] != 0) step = Mul(c.wheel, c.turn) / 43909;   // (also on water with part 0x100)
    int16_t yawStep = (int16_t)step;
    int32_t lateral;
    if (c.slide == 0 || (c.slide > 0 && yawStep > 0) || (c.slide < 0 && yawStep < 0) || c.slideRate == 0) {
        c.yaw = (uint16_t)(c.yaw + yawStep);
        int32_t extra = c.slide == 0 ? 0 : c.slideRate;
        lateral = Mul(yawStep + extra, c.wheel) / 0x28BE;
    } else {
        lateral = Mul(c.slideRate, c.wheel) / 0x28BE;
    }
    Fixed_Rotate(c.m, out, c.vel);
    Car_Slide(c, yawStep, grip, brake, lateral);
}

// Car_LevelInAir (0x002194B8): an airborne car (or one on its bumpers) levels out: its up vector loses a sixteenth
// of its lean each frame (very steep ones are first pulled up by 0.2).
void Car_LevelInAir(float* n, int flags, float yaw) {
    if (n[1] < 0.8f) n[1] += 0.2f;
    if (flags & 0x40) {
        TurnY(n, -yaw);
        n[2] /= 1.0625f;
        n[0] = (n[0] + 0.125f) / 1.0625f - 0.125f;
        TurnY(n, yaw);
    } else if (flags & 0x80) {
        TurnY(n, -yaw);
        n[2] /= 1.0625f;
        n[0] = (n[0] - 0.125f) / 1.0625f + 0.125f;
        TurnY(n, yaw);
    } else {
        n[0] /= 1.0625f;
        n[2] /= 1.0625f;
    }
    Normalize(n);
}

// Car_Suspension (0x0021AF28): the three wheels (front middle, rear left, rear right) on springs. Each probe found
// the ground under it; ground more than the clearance above a probe is a wall (a hit bit), not a step to climb, and
// a ceiling closer than a unit above is a roof (0x100). A wheel above the ground falls (its speed builds by
// `extra` a frame); one on it compresses its spring, which pushes back by 0x59 x load / 4096, damped, with the
// weight moved by braking, accelerating and turning (`accel`). Loads stay within 0..0x2000.
uint32_t Car_Suspension(CarBody& c, int32_t* probeY, const float* ceiling, const int32_t* accel, int32_t extra,
                        bool bigTires) {
    const int32_t clear = bigTires ? kClearanceBig : kClearance;
    int32_t az = accel[2], ax = accel[0];
    int32_t xfer[3] = {az > 0 ? (-az) / 8 : (-az) / 4, az / 4 + ax / 8, az / 4 + (-ax) / 8};
    uint32_t mask = 0;
    for (int i = 0; i < 7; ++i) {
        int32_t gy = (int32_t)(c.probe[i][1] * 32768.0f), oy = probeY[i];
        if (oy + clear < gy) {
            c.probe[i][1] = oy / 32768.0f;
            mask |= 1u << i;
            if (i < 5) c.spring[0] = 0;
            else c.spring[i - 4] = 0;
        }
        if ((int32_t)(ceiling[i] * 32768.0f) < oy + clear + 0x8000) mask |= 0x100;
    }
    for (int i = 0; i < 3; ++i) {
        int32_t gy = (int32_t)(c.probe[i][1] * 32768.0f), oy = probeY[i];
        int32_t d = gy - oy;
        int32_t& v = c.spring[i];
        int32_t& e = c.springStep[i];
        int32_t& load = c.load[i];
        bool spring = true;
        if (d < 0) {
            probeY[i] = oy - v;
            if (v == 0 && !(load < -d)) {
                e = d;
                spring = false;
            }
        } else {
            probeY[i] = oy + d;
            e = e + xfer[i] + Gravity();
        }
        if (spring) {
            e -= Mul(load, Gravity()) / 4096;
            int32_t t = Mul(e, 13) / 16;
            if (t != 0) e = e > 0 ? t : Mul(e, 15) / 16;
            int32_t n = load + e;
            if (n >= 0x2001) e = 0x2000 - load;
            else if (n < 0) e = -load;
        }
        load += e;
        if (d < 0) {
            probeY[i] += e;
            int32_t d2 = gy - probeY[i];
            if (d2 < 0) {
                v += extra;
                continue;
            }
            load += d2;
            if (!((mask >> i) & 1)) probeY[i] = gy;
        }
        // A landing at spring speed 0xB3 or more shakes the pad, harder the faster.
        if (v >= 0xB3) c.shake = kShakeLanding, c.shakeStrength = (uint8_t)(v >= 0x1000 ? 0xFF : v >> 4);
        e += v;
        v = 0;
    }
    return mask;
}

// Car_Ground (0x0021B3D0): the seven probes against the ground, the suspension, the car's height (the front wheel's
// third against the rear wheels') and its new orientation: up from the plane of the three wheels (levelling in the
// air), turned by the yaw. Returns the hit bits shifted down by 3 (1-8: the corners FL, FR, RL, RR; 0x20: a roof).
uint32_t Car_Ground(CarBody& c, const Ground& ground, int32_t* accel, const int32_t* local, int32_t climb,
                    bool bigTires, bool wingOut) {
    int32_t probeY[7];
    float ceiling[7];
    uint32_t missing = 0;
    // This frame's position (c.pos is still last frame's).
    float at[3] = {(float)((c.fixed[0] + ((c.fixed[2] & 0x2000000) >> 1)) & 0x1FFFFFF) / kFixed,
                   (float)c.fixed[1] / kFixed, (float)(c.fixed[2] & 0x1FFFFFF) / kFixed};
    for (int i = 0; i < 7; ++i) {
        float* p = c.probe[i];
        for (int k = 0; k < 3; ++k)
            p[k] = c.m[0][k] * kProbe[i][0] + c.m[1][k] * kProbe[i][1] + c.m[2][k] * kProbe[i][2] + at[k];
        p[3] = 1.0f;
        probeY[i] = (int32_t)(p[1] * 32768.0f);
        int32_t f = ground.Collision_Ground(p, &ceiling[i]);
        c.surface[i] = f;
        if (f == -1) missing |= 1u << i;
    }
    // The wheels' reach: gravity (local[3]) and the fins' lift; with the Flight Wing out the lift alone (0x0021B5A8).
    uint32_t mask = missing | Car_Suspension(c, probeY, ceiling, accel, wingOut ? climb : local[3] + climb, bigTires);
    c.fixed[1] = Div((Mul(probeY[0], 33) + Mul((probeY[1] + probeY[2]) / 2, 34)) / 0x43 << 4, 25);
    if (g_heightKey && !(mask & 0x100)) c.fixed[1] += g_heightKey > 0 ? -0x1000 : 0x51EB;

    // Water: the front probe's water level against last frame's height, 0.5 under it (1.35 when the player's car,
    // D_0177EA50[0], has the big tires: the game tests the player's flags for every car).
    float level = c.probe[0][3];
    if (c.pos[1] < level - ((g_playerFlags & kPartBigTires) ? 1.35f : 0.5f)) {
        if (c.water <= 0) {
            for (int k = 0; k < 3; ++k) c.spring[k] /= 2;
        }
        c.water = 1;
        int32_t s = (c.surface[0] & 0x3000) | 0x100651;
        c.surface[0] = c.surface[1] = c.surface[2] = s;
    } else if (c.pos[1] < level) {
        c.events |= 0x40;
        c.water = -1;   // (the splash sound 0x28 plays on entering)
    } else {
        if (c.water != 0) c.events |= 0x40;
        c.water = 0;
    }

    if (c.load[0] | c.load[1] | c.load[2]) {
        c.airTime = 0;
    } else if (c.airTime++ > 0x40) {
        int32_t avg = (c.spring[0] + c.spring[1] + c.spring[2]) / 3;
        c.spring[0] = c.spring[1] = c.spring[2] = avg;
        c.events |= 1;
    }
    int level16 = (c.events & 1) | ((mask & 0x78) ? 1 : 0);

    float yaw = YawRadians(c.yaw);
    const float* p0 = c.probe[0];
    const float* p1 = c.probe[1];
    const float* p2 = c.probe[2];
    float a[3] = {p2[0] - p0[0], (probeY[2] - probeY[0]) / 32768.0f, p2[2] - p0[2]};
    float b[3] = {p1[0] - p2[0], (probeY[1] - probeY[2]) / 32768.0f, p1[2] - p2[2]};
    float n[3];
    Cross(a, b, n);
    if (n[1] < 0) n[1] = -n[1];
    Normalize(n);
    if (level16) Car_LevelInAir(n, level16, yaw);
    Matrix_FromUp(c.m, n);
    Matrix_TurnY(c.m, yaw);
    Transpose(c.m, c.inv);
    return mask >> 3;
}

void Car_CrashShake(CarBody& c, uint32_t hits);

// Car_Crash's Tunnel Race branch (course 0x1C), every frame, hit or not: the car's velocity in its own frame, with a
// bounce set by the corners hit (a front corner: 0x8E3 sideways away from the wall and a turn of 0x200; a rear
// corner or a side: 0x11C7 sideways, the forward speed halved; the whole front or back: 0x11C7 back or forward; four
// corners: the race's flag 0x200, left out), back in the world, moves the car once more by 16/25 of it (so the
// tunnel's cars cover twice the ground), and becomes its speed across the ground.
void Car_CrashTunnel(CarBody& c, uint32_t hits) {
    int32_t v[4];
    Fixed_Rotate(c.inv, c.vel, v);
    const int32_t kSide = 0x8E3, kHard = 0x11C7;
    switch (hits & 0xF) {   // jtbl_002EAE20
    case 7: v[2] = -kHard; // fall through
    case 1: v[0] = kSide; c.yaw = (uint16_t)(c.yaw + 0x200); break;
    case 2: case 11: v[0] = -kSide; c.yaw = (uint16_t)(c.yaw - 0x200); break;
    case 3: v[2] = -kHard; break;
    case 12: v[2] = kHard; break;
    case 13: v[2] = kHard; // fall through
    case 4: case 5: case 6: v[0] = kHard; v[2] /= 2; break;
    case 8: case 9: case 10: case 14: v[0] = -kHard; v[2] /= 2; break;
    default: break;   // 0; 15
    }
    int32_t w[4];
    Fixed_Rotate(c.m, v, w);
    c.fixed[0] = (c.fixed[0] + Div(w[0] << 4, 25)) & 0x0FFFFFFF;
    c.fixed[1] += Div(w[1] << 4, 25);
    c.fixed[2] = (c.fixed[2] + Div(w[2] << 4, 25)) & 0x0FFFFFFF;
    c.vel[0] = w[0];
    c.vel[2] = w[2];
}

// Car_Crash (0x00219660): a car that hit something goes back to where it was a frame ago (two moves back) with its
// old heading, then is pushed off by its corners: a front corner turns it away by 0x200, cuts a quarter of its
// sideways and a sixteenth of its forward speed and pushes it back and aside; a rear corner halves its sideways
// speed; hitting with the whole front (or back) stops it. The pad shakes: a wall on the left starts with the large
// motor, on the right with the small one, anything else runs both. Walls are silent but in the Tunnel Race (course
// 0x1C), where it runs Car_CrashTunnel instead of the corners' pushes, every frame.
void Car_Crash(CarBody& c, uint32_t hits, uint16_t yaw0, bool tunnel) {
    if (hits) {
        int32_t back[3] = {Div(-c.vel[0] << 5, 25), Div(-c.vel[1] << 5, 25), Div(-c.vel[2] << 5, 25)};
        c.yaw = yaw0;
        c.fixed[0] = (c.fixed[0] + back[0]) & 0x0FFFFFFF;
        c.fixed[1] += back[1];
        c.fixed[2] = (c.fixed[2] + back[2]) & 0x0FFFFFFF;
        // A wall sounds (ACTION 0x11, restarted: Sound_PlaySe 0x8311 then 0x311) at most every 0x20 frames.
        if (tunnel && c.crashTime == 0) {
            c.crashed = true;
            c.crashTime = 0x20;
        }
    }
    if (tunnel) {
        Car_CrashTunnel(c, hits);
        Car_CrashShake(c, hits);
        return;
    }
    int32_t local[4];
    Fixed_Rotate(c.inv, c.vel, local);
    int32_t dv[4] = {0, 0, 0, 0}, push[4] = {0, 0, 0, 0};
    switch (hits & 0xF) {
    case 1: case 7:   // front left
        dv[0] = (-local[0]) / 4;
        dv[2] = (-local[2]) / 16;
        c.yaw = (uint16_t)(c.yaw + 0x200);
        push[2] = -0x800;
        push[0] = 0x800;
        break;
    case 2: case 11:   // front right
        dv[0] = (-local[0]) / 4;
        dv[2] = (-local[2]) / 16;
        c.yaw = (uint16_t)(c.yaw - 0x200);
        push[0] = -0x800;
        push[2] = -0x800;
        break;
    case 3:   // the front
        dv[2] = -local[2];
        push[2] = -0x800;
        break;
    case 4: case 6: case 13:   // rear left
        push[0] = 0x800;
        dv[0] = (-local[0]) / 2;
        dv[2] = (-local[2]) / 8;
        push[2] = 0x800;
        break;
    case 5:   // the left side
        push[0] = 0x800;
        break;
    case 8: case 9: case 14:   // rear right
        push[2] = 0x800;
        dv[0] = (-local[0]) / 2;
        dv[2] = (-local[2]) / 8;
        push[0] = -0x800;
        break;
    case 10:   // the right side
        push[0] = -0x800;
        break;
    case 12:   // the back
        dv[2] = -local[2];
        push[2] = 0x800;
        break;
    default:   // 0; 15 (stuck on all four corners: the game flags the race, 0x200)
        break;
    }
    int32_t w[4];
    Fixed_Rotate(c.m, push, w);
    c.fixed[0] = (c.fixed[0] + Div(w[0] << 4, 25)) & 0x0FFFFFFF;
    c.fixed[1] += Div(w[1] << 4, 25);
    c.fixed[2] = (c.fixed[2] + Div(w[2] << 4, 25)) & 0x0FFFFFFF;
    Fixed_Rotate(c.m, dv, w);
    c.vel[0] += w[0];
    c.vel[2] += w[2];
    Car_CrashShake(c, hits);
}

// Car_Crash's end: the pad's shake for the corners hit (jtbl_002EAE60).
void Car_CrashShake(CarBody& c, uint32_t hits) {
    switch (hits & 0xF) {   // jtbl_002EAE60
    case 0: break;
    case 1: case 4: case 5: c.shake = kShakeLeft, c.shakeStrength = 0xFF; break;
    case 2: case 8: case 10: c.shake = kShakeRight, c.shakeStrength = 0xFF; break;
    default: c.shake = kShakeHit, c.shakeStrength = 0xFF; break;
    }
}

// Car_SpinWheels (0x0021ACD0): the wheels' angles for drawing: the front ones roll with the wheel speed (radius
// 0.33), the rear ones with the engine's (0.38), at most 0.8 radians a frame; on the big tires both with radius 0.55,
// at most 0.2. Then the steering angle.
void Car_SpinWheels(CarBody& c, bool bigTires) {
    const float most = bigTires ? 0.2f : 0.8f;
    auto spin = [most](float angle, int32_t speed, float radius) {
        float d = speed / 32768.0f / radius;
        if (d > most) d = most;
        if (d < -most) d = -most;
        angle += d;
        if (angle < -kPi) angle += 2 * kPi;
        else if (kPi < angle) angle -= 2 * kPi;
        return angle;
    };
    c.wheelAngle[0] = spin(c.wheelAngle[0], c.wheel, bigTires ? 0.55f : 0.33f);
    c.wheelAngle[2] = spin(c.wheelAngle[2], c.tire, bigTires ? 0.55f : 0.38f);
    c.wheelAngle[1] = (float)(c.steerAngle << 7) * kPi / 32768.0f;
}

}  // namespace

namespace {

// func_002175D0: whether one of a's bumper corners (its corner probes) is inside b's box.
bool Car_Overlaps(const CarBody& a, const CarBody& b) {
    for (int i = 3; i < 7; ++i) {
        float d[3] = {a.probe[i][0] - b.pos[0], a.probe[i][1] - b.pos[1], a.probe[i][2] - b.pos[2]};
        float x = b.inv[0][0] * d[0] + b.inv[1][0] * d[1] + b.inv[2][0] * d[2];
        float z = b.inv[0][2] * d[0] + b.inv[1][2] * d[1] + b.inv[2][2] * d[2];
        if (-0.9f <= x && x <= 0.9f && -1.5f <= z && z <= 1.5f) return true;
    }
    return false;
}

}  // namespace

void Car_WorldPosition(const CarBody& c, float* out) {
    CarBody t;
    memcpy(t.fixed, c.fixed, sizeof t.fixed);
    UpdatePosition(t);
    memcpy(out, t.pos, 3 * sizeof(float));
}

int Cars_Collide(CarBody* const* cars, int count, CarContact* contacts, int max) {
    int touched = 0;
    for (int i = 0; i + 1 < count; ++i) {
        CarBody& a = *cars[i];
        for (int j = i + 1; j < count; ++j) {
            CarBody& b = *cars[j];
            float d[3];
            for (int k = 0; k < 3; ++k) d[k] = SubSat(b.fixed[k], a.fixed[k]) / 32768.0f;
            float len = sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
            if (len == 0 || !(len < 1.8f)) continue;
            if (!Car_Overlaps(b, a) && !Car_Overlaps(a, b)) continue;
            float k = (0.9f - len) / len;
            if (k < 0) k = 0.01f;
            // (A collision with a player's car also plays a sound and shakes the pad: func_00217830.)
            if (touched < max) contacts[touched] = {i, j};
            ++touched;
            for (int c = 0; c < 3; ++c) {
                float push = d[c] * k * 32768.0f;
                a.fixed[c] = (int32_t)((float)a.fixed[c] - push);
                b.fixed[c] = (int32_t)((float)b.fixed[c] + push);
            }
            a.vel[0] = b.vel[0] = (a.vel[0] + b.vel[0]) / 2;
            a.vel[2] = b.vel[2] = (a.vel[2] + b.vel[2]) / 2;
        }
    }
    return touched;
}

CarParts Car_SetParts(const uint8_t* r, bool teammate) {
    CarParts p = {};
    for (int k = 0; k < 6; ++k) {
        p.grip[k] = kTires[r[1] % 11][k];
        if (teammate) p.grip[k] = (int16_t)(p.grip[k] + p.grip[k] / 16);
    }
    p.torque = kEngineTorque[r[2] % 11];
    p.fuelUse = kEngineFuel[r[2] % 11];
    p.mass = kWeights[r[3] % 5];
    if (teammate) p.mass -= p.mass / 4;
    for (int k = 0; k < 8; ++k) p.gears[k] = kGearboxes[r[4] % 6][k];
    p.steer = kSteering[r[5] % 4];
    p.brakes = kBrakes[r[6] % 3];
    return p;
}

uint16_t Car_PartFlags(const uint8_t* r) { return (uint16_t)(r[0xE] | r[0xF] << 8); }

const uint8_t* QuickRace_PlayerParts(int card) { return kQuickRacePlayer[card & 7]; }
const uint8_t* QuickRace_OpponentParts(int card) { return kQuickRaceOpponents[card & 7]; }
const uint8_t* Versus_RowParts(int row) { return kVersusParts[row >= 0 && row < 10 ? row : 0]; }

int32_t Car_Jet(CarBody& c, uint32_t input, bool go) {
    if (!go) {   // race flags & 0xC not 4
        if (c.jet) c.jetSound = kJetStop;
        c.jet = 0;
        return 0;
    }
    if ((input & kCarR2) && c.fuel >= kJetFuel) {
        c.fuel -= kJetFuel;
        int32_t push = 0xB2;
        if (c.jet == 0) c.jetSound = kJetStart;
        int16_t n = (int16_t)(c.jet + 1);
        if (c.fuel < 0x2EE0) {
            n &= 3;
            push = 0x2C;
        } else if (c.fuel < 0x7530) {
            n &= 7;
            push = 0x59;
        }
        c.jet = n;
        if (n == 0) c.jetSound = kJetStop;
        return push;
    }
    if (c.jet == 0) return 0;
    c.jetSound = kJetStop;
    c.jet = c.jet > 0 ? -2 : (int16_t)(c.jet + 1);
    return 0;
}

void Car_Place(CarBody& c, const float* pos, uint16_t yaw) {
    memset(&c, 0, sizeof c);
    float up[3] = {0, 1, 0};
    Matrix_FromUp(c.m, up);
    Matrix_TurnY(c.m, YawRadians(yaw));
    Transpose(c.m, c.inv);
    Matrix_FromUp(c.body, up);
    c.yaw = yaw;
    for (int k = 0; k < 3; ++k) c.fixed[k] = (int32_t)(pos[k] * kFixed);
    UpdatePosition(c);
    for (int k = 0; k < 3; ++k) c.load[k] = 0x1000;
    for (int i = 0; i < 7; ++i) c.surface[i] = 0x2000;
    c.gear = 1;
    c.fuel = 0x40000;
}

void Car_Stop(CarBody& c) {
    c.gear = 1;
    for (int32_t& v : c.spring) v = 0;
    c.brakeTime = 0;
    c.rpm = c.steerAngle = c.slideRate = 0;
    for (int k = 0; k < 4; ++k) c.vel[k] = c.prevVel[k] = 0;
    c.wheel = c.tire = 0;
    c.effect = 0;
    for (int k = 0; k < 3; ++k) {
        c.springStep[k] = 0;
        c.load[k] = 0x1000;
    }
    c.jet = 0;
}

void Car_Reset(CarBody& c, const float* pos, uint16_t yaw) {
    int32_t fuel = c.fuel, odometer = c.odometer;
    Car_Place(c, pos, yaw);
    c.fuel = fuel;
    c.odometer = odometer;
}

void Car_SetPlace(CarBody& c, const float* pos, uint16_t yaw) {
    for (int k = 0; k < 3; ++k) c.fixed[k] = (int32_t)(pos[k] * kFixed);
    UpdatePosition(c);
    c.yaw = yaw;
}

// Car_Update (0x0021BA70) for a car driven from the pad. Its velocity is turned into the car's frame; quadratic
// drag takes off (v / 0x600)^2 / mass forward and (v / 1024)^2 / mass sideways; Car_Drive works out the new
// velocity, which moves the car by 16/25 of it; then the ground, collisions and the body's tilt. Gravity is 0x59 a
// frame, felt through the suspension.
void Car_SetPlayerFlags(uint16_t flags) { g_playerFlags = flags; }
void Car_SetDebugPrint(CarDebugPrint print, bool vectors) {
    g_debugPrint = print;
    g_debugVectors = vectors;
}
void Car_SetHeightKey(int key) { g_heightKey = key; }
void Car_SetCheats(uint32_t cheats) { g_cheats = cheats; }
uint32_t Car_PlayerCheats() {
    return (Extras_On(kCheatUncappedSpeed) ? kCarCheatUncapped : 0) | (Extras_On(kCheatInfiniteFuel) ? kCarCheatFuel : 0) |
           (Extras_On(kCheatPowerBoost) ? kCarCheatPower : 0) | (Extras_On(kCheatSuperGrip) ? kCarCheatGrip : 0) |
           (Extras_On(kCheatReduceDrag) ? kCarCheatDrag : 0) |
           (Extras_On(kCheatLowGravity) ? kCarCheatGravity : 0);
}
void Car_SetClock(int32_t clock) { g_clock = clock; }

void Car_Update(CarBody& c, CarParts& p, const Ground& ground, uint32_t input, bool go, bool tunnel,
                bool slowed) {
    c.events = 0;
    c.shake = 0;
    c.crashed = false;
    if (c.crashTime) --c.crashTime;
    if (c.bump) --c.bump;
    if (g_cheats & kCarCheatFuel) c.fuel = 0x40000;
    uint16_t yaw0 = c.yaw;
    int32_t accel[4];
    Debug_PrintVector(16, GT_00333598_4, c.prevVel);
    for (int k = 0; k < 4; ++k) accel[k] = SubSat(c.vel[k], c.prevVel[k]);
    Fixed_Rotate(c.inv, accel, accel);
    Debug_PrintVector(48, GT_003335A0_4, accel);
    c.vel[1] -= Gravity();
    Debug_PrintVector(24, GT_003335A8_4, c.vel);
    memcpy(c.prevVel, c.vel, sizeof c.vel);
    int32_t local[4];
    Fixed_Rotate(c.inv, c.vel, local);
    Debug_PrintVector(32, GT_003335B0_4, local);
    int32_t g[4] = {0, Gravity(), 0, 0};
    Fixed_Rotate(c.inv, g, g);
    local[3] = g[1];
    // The headlights (+0x1FC): on outside the clock's 0xE484-0x278D0 (6:30-18:00), and the lights button held turns
    // them the other way.
    bool night = (uint32_t)(g_clock - 0xE484) > 0x1944Cu;
    c.lights = night != ((input & kCarHeadlights) != 0) ? 1 : 0;
    c.jetSound = kJetNone;
    if (p.flags & kPartJet) {
        local[2] += Car_Jet(c, input, go);
        if (local[2] > kJetTop && !(g_cheats & kCarCheatUncapped)) local[2] = kJetTop;
    }

    // The propellers (part 0x40), in water (wading or under): Circle pushes back 0x59, else Cross forward 0x59.
    if ((p.flags & kPartPropellers) && c.water != 0) {
        if (input & kCarReverse) local[2] -= kPropellerPush;
        else if (input & kCarAccel) local[2] += kPropellerPush;
    }

    // Drag: (v / 0x600)^2 / mass, more for a `slowed` car (0x500), and more wading (the divisor halved) and under
    // water (quartered). (The Reduce Drag cheat) the divisor doubled: a quarter of the drag.
    int32_t div = slowed ? 0x500 : 0x600;
    if (g_cheats & kCarCheatDrag) div *= 2;
    if (c.water < 0) div >>= 1;
    if (c.water > 0) div /= 4;
    int32_t q = local[2] / div, d = Div(Mul(q, q), p.mass);
    if (g_debugPrint && g_debugVectors) g_debugPrint(220, 48, GT_00333588_7, d);
    local[2] += q >= 0 ? -d : d;
    auto damp = [&](int32_t& v) {
        int32_t s = c.water > 0 ? v / 64 : v / 1024;
        int32_t dd = Div(Mul(s, s), p.mass);
        v += s >= 0 ? -dd : dd;
    };
    damp(local[0]);
    for (int k = 0; k < 3; ++k) damp(c.spring[k]);
    if (c.spring[0] == 0 || c.spring[1] == 0 || c.spring[2] == 0) c.vel[1] = 0;

    // The Flight Wing (0x0021C114): out over 300 km/h (Vec_SpeedKmh, func_0021D358: the speed across the ground),
    // folded again under it.
    if (p.flags & (kPartWingFolded | kPartWingOut)) {
        const float vx = c.vel[0] / 32768.0f, vz = c.vel[2] / 32768.0f;
        const float kmh = sqrtf(vx * vx + vz * vz) * 216.0f;
        if ((p.flags & kPartWingFolded) && kmh > 300.0f)
            p.flags = (uint16_t)((p.flags & ~kPartWingFolded) | kPartWingOut);
        else if (!(p.flags & kPartWingFolded) && kmh < 300.0f)
            p.flags = (uint16_t)((p.flags & ~kPartWingOut) | kPartWingFolded);
    }

    // The fins (part 0x1000): L1 sets them down a step (from 0 or 1), L2 up (from 0 or -1); and left / right turn
    // the car by its speed forward / 512 on top of its steering, on the ground or off it.
    if (p.flags & kPartFins) {
        int8_t s = c.fins;
        if (s >= 0 && (input & kCarL1)) c.fins = --s;
        if (s <= 0 && (input & kCarL2)) c.fins = (int8_t)(s + 1);
        int dir = (input & kCarRight) ? 1 : (input & kCarLeft) ? -1 : 0;
        if (dir) c.yaw = (uint16_t)(c.yaw + dir * local[2] / 512);
    }

    Debug_PrintVector(40, GT_003335B0_4, local);
    Car_Drive(c, p, local, input, accel, go);
    Debug_PrintVector(64, GT_003335A0_4, accel);
    Debug_PrintVector(80, GT_003335A8_4, c.vel);
    c.fixed[0] = (c.fixed[0] + Div(c.vel[0] * 16, 25)) & 0x0FFFFFFF;
    c.fixed[1] += Div(c.vel[1] * 16, 25);
    c.fixed[2] = (c.fixed[2] + Div(c.vel[2] * 16, 25)) & 0x0FFFFFFF;
    // The lift Car_Ground adds to the suspension's load: (fins + 1) x speed / 2048, under water fins x speed / 256.
    int32_t climb = c.water <= 0 ? (c.fins + 1) * local[2] / 2048 : c.fins * local[2] / 256;
    const bool bigTires = (p.flags & kPartBigTires) != 0;
    uint32_t hits = Car_Ground(c, ground, accel, local, climb, bigTires, (p.flags & kPartWingOut) != 0);
    UpdatePosition(c);
    hits |= ground.Collision_Posts(c.pos, c.yaw);   // Car_HitPosts (0x00219ED8): the race's slot
    Car_Crash(c, hits, yaw0, tunnel);
    Debug_PrintVector(88, GT_003335A8_4, c.vel);

    int32_t rear = (c.load[1] + c.load[2]) / 2;
    float tilt[3] = {(c.load[2] - c.load[1]) / 48496.0f, 1.0f, (c.load[0] - rear) / 43909.0f};
    Matrix_FromUp(c.body, tilt);
    c.bodyHeight = ((c.load[0] + rear) / 2 - 0x1000) * (-1.0f / 32768.0f) + (bigTires ? kBigTiresLift : 0.0f);
    c.odometer += c.wheel < 0 ? -c.wheel : c.wheel;
    Car_SpinWheels(c, bigTires);
    // Debug_PrintCar (0x0026D700), but its BLOCK line, which the caller prints (it knows the field's blocks).
    if (g_debugPrint) {
        g_debugPrint(32, 32, "X:%4.2f", c.pos[0]);
        g_debugPrint(32, 40, "Y:%4.2f", c.pos[1]);
        g_debugPrint(32, 48, "Z:%4.2f", c.pos[2]);
        g_debugPrint(32, 56, GT_0032FB28_8, (unsigned)c.surface[0]);
        g_debugPrint(32, 64, GT_0032FB38_9, (unsigned)c.events);
    }
}

}  // namespace game
