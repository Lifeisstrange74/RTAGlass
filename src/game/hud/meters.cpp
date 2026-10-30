// The meter parts 1-9: Meter_Draw's routines Meter_Draw1 (0x00249FA0) to Meter_Draw9 (0x0024DA50), each over its
// own ITEM/METERn.GSL. Sprite records {x, y, u, v, w, h, flip} are the ELF's (flip bit 0 mirrors left to right, bit 1
// top to bottom); the turned sprites go through Meter_DrawTurned (func_0024EB10). The readouts are Meter_Rpm and
// Meter_Fuel scaled (rounding up), Meter_Speed's mph (no leading zeros, at most 3 digits, unless said), Meter_Gear
// (0 R) and Meter_Fins. See docs/hud.md, "The meters".
#include <math.h>
#include <stdint.h>
#include <stdio.h>

#include "speedometer.h"

namespace game {

namespace {

const int kRpmMax = 10000, kFuelMax = 0x40000;
const float kKmhPerMph = 1.609f;   // D_00332F44

struct MeterPiece {
    int16_t x, y, u, v, w, h;
    uint8_t flip;
};

// ---- meter 2 ----
const MeterPiece kM2Dial[2] = {{0, 0, 0, 0, 110, 128, 0}, {110, 78, 110, 78, 40, 50, 0}};   // D_002A9098
const MeterPiece kM2Rpm[10] = {   // D_002A90D8
    {30, 100, 109, 53, 24, 24, 0}, {16, 91, 133, 52, 26, 24, 0}, {7, 79, 160, 52, 24, 22, 0},
    {3, 66, 185, 52, 22, 18, 0},   {3, 46, 208, 52, 22, 20, 0},  {6, 28, 230, 52, 26, 24, 0},
    {15, 13, 160, 80, 26, 26, 0},  {29, 6, 186, 80, 24, 26, 0},  {48, 2, 210, 80, 18, 24, 0},
    {67, 2, 230, 80, 16, 26, 0},
};
const MeterPiece kM2Fuel = {80, 102, 0, 0, 58, 12, 0};   // D_003278F0
// ---- meter 3 ----
const MeterPiece kM3Empty = {46, 9, 96, 56, 24, 18, 0};          // D_00327980
const MeterPiece kM3Planet = {24, 30, 168, 0, 68, 68, 0};        // D_003279A0
const MeterPiece kM3Dial = {3, 0, 132, 0, 124, 128, 0};          // D_003279C0
const MeterPiece kM3Fins[3] = {{105, 49, 128, 64, 32, 24, 0},    // D_003279E0, D_00327A00, D_00327A20
                               {93, 35, 128, 40, 32, 24, 0},
                               {93, 67, 128, 88, 32, 24, 0}};
// ---- meter 4 ----
const MeterPiece kM4Base = {5, 149, 0, 0, 140, 36, 0};   // D_00327A40
const MeterPiece kM4Fuel = {160, 7, 0, 2, 36, 138, 0};   // D_00327A60
const MeterPiece kM4Rpm[10] = {   // D_002A92B8
    {6, 174, 2, 9, 58, 11, 0},    {28, 167, 61, 2, 50, 18, 0}, {44, 161, 0, 25, 36, 23, 0},
    {59, 156, 40, 30, 24, 28, 0}, {74, 153, 71, 30, 12, 30, 0}, {84, 150, 91, 26, 18, 34, 0},
    {85, 149, 0, 64, 31, 36, 0},  {87, 151, 33, 66, 40, 34, 0}, {89, 157, 24, 100, 48, 28, 0},
    {95, 168, 73, 104, 51, 16, 0},
};
const MeterPiece kM4Frame[4] = {   // D_002A93F8
    {0, 130, 0, 0, 160, 58, 0}, {72, 161, 0, 88, 56, 22, 0}, {156, 2, 213, 0, 43, 128, 0},
    {160, 130, 160, 0, 39, 19, 0},
};
const MeterPiece kM4FinsHub = {96, 88, 152, 72, 54, 54, 0};         // D_00327A80
const MeterPiece kM4FinsLevelL = {96, 107, 168, 56, 15, 16, 0};     // D_00327AA0
const MeterPiece kM4FinsLevelR = {135, 107, 184, 56, 15, 16, 0};    // D_00327AC0
const MeterPiece kM4FinsUp = {115, 88, 168, 40, 16, 15, 0};         // D_00327AE0
const MeterPiece kM4FinsDown = {115, 127, 184, 40, 16, 15, 0};      // D_00327B00
const MeterPiece kM4GearBack = {156, 151, 120, 64, 29, 29, 0};      // D_00327B20
// ---- meter 5 ----
const MeterPiece kM5Dial[5] = {   // D_002A94C0
    {0, 0, 72, 0, 76, 72, 0}, {76, 0, 72, 0, 76, 72, 1}, {0, 72, 152, 0, 76, 68, 0}, {76, 72, 152, 0, 76, 68, 1},
    {62, 80, 112, 96, 30, 11, 0},
};
const MeterPiece kM5Wings[2] = {{11, 4, 0, 0, 65, 120, 0}, {76, 4, 0, 0, 65, 120, 1}};   // D_002A9560
const MeterPiece kM5Face[4] = {   // D_002A95A0
    {51, 15, 0, 0, 50, 40, 0}, {34, 103, 0, 62, 87, 37, 0}, {9, 95, 0, 42, 32, 18, 0}, {113, 95, 0, 42, 32, 18, 1},
};
const MeterPiece kM5Swing[5] = {   // D_00327B40 ... D_00327BC0
    {63, 115, 87, 60, 27, 31, 0}, {84, 109, 87, 60, 27, 31, 0}, {99, 96, 87, 60, 27, 31, 0},
    {43, 109, 87, 60, 27, 31, 0}, {28, 96, 87, 60, 27, 31, 0},
};
const uint8_t kM5SwingOrder[8] = {0, 1, 2, 1, 0, 3, 4, 3};   // jtbl_00327CA0
const MeterPiece kM5Fins[6] = {   // D_00327BE0: pairs for Meter_Fins 0, 2 and 1
    {29, 27, 176, 97, 24, 22, 0}, {99, 27, 176, 97, 24, 22, 1}, {29, 27, 152, 97, 24, 22, 0},
    {99, 27, 152, 97, 24, 22, 1}, {29, 27, 200, 97, 24, 22, 0}, {99, 27, 200, 97, 24, 22, 1},
};
const MeterPiece kM5Fuel[2] = {{5, 45, 0, 0, 28, 55, 0}, {120, 45, 0, 0, 28, 55, 1}};   // D_002A9620
// ---- meter 6 ----
const MeterPiece kM6GearPips[6] = {   // D_002A9698: gear g draws [6 - g .. 5]
    {117, 76, 145, 2, 6, 20, 0}, {111, 73, 154, 2, 6, 20, 0}, {111, 79, 145, 2, 6, 20, 0},
    {105, 76, 154, 2, 6, 20, 0}, {105, 82, 145, 2, 6, 20, 0}, {99, 79, 154, 2, 6, 20, 0},
};
const MeterPiece kM6Reverse = {86, 102, 176, 0, 12, 12, 0};   // D_002A9758
const MeterPiece kM6Empty = {9, 8, 0, 0, 64, 56, 0};          // D_00327D30
const MeterPiece kM6Full = {10, 8, 72, 0, 56, 50, 0};         // D_00327D50
const MeterPiece kM6Fins[3] = {{100, 39, 233, 25, 22, 22, 0},   // D_00327D70, D_00327D90, D_00327DB0
                               {91, 23, 231, 1, 22, 22, 0},
                               {103, 60, 236, 50, 20, 20, 0}};
// ---- meter 7 ----
const MeterPiece kM7Spin[5] = {   // D_002A97B0
    {92, 26, 176, 76, 30, 44, 0}, {117, 47, 213, 0, 42, 36, 0}, {107, 82, 212, 36, 40, 40, 0},
    {67, 82, 172, 36, 40, 40, 0}, {55, 47, 169, 0, 42, 36, 0},
};
const MeterPiece kM7Empty = {52, 10, 160, 0, 24, 24, 0};       // D_00327DD0
const MeterPiece kM7FinsUp = {141, 14, 128, 0, 24, 32, 0};     // D_00327DF0
const MeterPiece kM7FinsDown = {141, 80, 128, 0, 24, 32, 2};   // D_00327E10
// ---- meter 8 ----
const MeterPiece kM8Fuel = {71, 48, 0, 0, 46, 56, 0};          // D_00327E30
const MeterPiece kM8Empty = {23, 64, 128, 64, 46, 36, 0};      // D_00327E50
const MeterPiece kM8Reverse = {120, 88, 128, 0, 30, 28, 0};    // D_002A9898
const MeterPiece kM8Gear[6] = {   // D_002A98B8
    {120, 88, 160, 0, 30, 28, 0}, {149, 70, 192, 0, 30, 28, 0}, {118, 61, 224, 0, 30, 28, 0},
    {153, 40, 128, 32, 30, 28, 0}, {119, 31, 160, 32, 30, 28, 0}, {156, 10, 192, 32, 30, 28, 0},
};
const MeterPiece kM8Rpm[10] = {   // D_002A9978
    {16, 104, 128, 112, 15, 15, 0}, {6, 87, 128, 112, 15, 15, 0},  {4, 66, 128, 112, 15, 15, 0},
    {7, 46, 128, 112, 15, 15, 0},   {16, 28, 128, 112, 15, 15, 0}, {30, 14, 128, 112, 15, 15, 0},
    {48, 5, 128, 112, 15, 15, 0},   {68, 2, 128, 112, 15, 15, 0},  {89, 5, 128, 112, 15, 15, 0},
    {107, 14, 128, 112, 15, 15, 0},
};
const MeterPiece kM8FinsUp = {33, 38, 168, 0, 16, 24, 0};      // D_00327E70
const MeterPiece kM8FinsDown = {33, 38, 168, 0, 16, 24, 2};    // D_00327E90
// ---- meter 9 ----
const MeterPiece kM9Dial = {4, 0, 104, 0, 152, 128, 0};        // D_00327EB0
const MeterPiece kM9Plate = {44, 112, 0, 105, 110, 20, 0};     // D_00327ED0
const MeterPiece kM9Mark = {101, 114, 48, 32, 16, 16, 0};      // D_00327EF0
const MeterPiece kM9FinsUp = {10, 8, 79, 0, 16, 16, 0};        // D_00327F10
const MeterPiece kM9FinsDown = {10, 27, 79, 19, 16, 16, 0};    // D_00327F30
const MeterPiece kM9Devil = {0, 31, 0, 0, 70, 110, 0};         // D_00327F50

// Sprite_Draw (0x0024F7C8): a piece at (x, y) + its place, mirrored by its flags.
void Piece(gfx::Draw2D& d, const gfx::Texture& t, const MeterPiece& p, int x, int y) {
    float u = p.u, v = p.v, uw = p.w, vh = p.h;
    if (p.flip & 1) {
        u += p.w;
        uw = -uw;
    }
    if (p.flip & 2) {
        v += p.h;
        vh = -vh;
    }
    d.Sprite(t, (float)(x + p.x), (float)(y + p.y), p.w, p.h, u, v, uw, vh);
}

// Meter_Rpm / Meter_Fuel with a scale and the clamp: (clamped x scale + max - 1) / max.
int Scaled(int v, int max, int scale) {
    if (v < 0) v = 0;
    if (v > max) v = max;
    return (int)(((int64_t)v * scale + max - 1) / max);
}
int Clamped(int v, int max) { return v < 0 ? 0 : v > max ? max : v; }
int Mph(const MeterReadout& r) { return (int)(r.speed / kKmhPerMph); }
int Gear(const MeterReadout& r) { return r.gear < 0 ? 0 : r.gear; }

// The speed's digits from the right: `place(i, digit)` draws digit i (0 the ones). At least one, at most three; with
// `all3` always three (leading zeros).
template <typename F>
void Digits(int value, bool all3, F place) {
    for (int i = 0; i < 3; ++i) {
        place(i, value % 10);
        value /= 10;
        if (!value && !all3) break;
    }
}

void Meter_Draw1(gfx::Draw2D& d, int x, int y, const MeterReadout& r, MeterState& s) {
    // The Chronometer: one picture. The dial, the fins, the gear, the speed (always three digits), then the hands:
    // the rpm's sub-dial counting up (1/8 turn a frame at 10000), the fuel, the hour hand (a turn in 12 hours) and the
    // speed's (270 degrees to 186 mph, and on).
    const gfx::Texture& t = d.Get("ITEM/METER1.GSL", 0);
    d.Sprite(t, (float)x, (float)y, 140, 141, 0, 0, 140, 141);
    if (r.finsPart) {
        static const int kFinsU[3] = {0x90, 0xB0, 0xA0};
        d.Sprite(t, x + 86.0f, y + 47.0f, 16, 16, (float)kFinsU[Meter_FinsIndex(r.fins)], 32, 16, 16);
    }
    d.Sprite(t, x + 93.0f, y + 94.0f, 13, 15, 144.0f + 13 * Gear(r), 0, 13, 15);
    Digits(Mph(r), true, [&](int i, int n) {
        d.Sprite(t, x + 111.0f - 9 * i, y + 74.0f, 9, 12, 144.0f + 9 * n, 16, 9, 12);
    });
    if (!r.paused) s.accF = fmodf(s.accF + r.rpm / (float)kRpmMax * 0.125f, 2.0f);
    Meter_DrawTurned(d, t, x + 70.0f, y + 40.0f, -3, -3, 15, 3, s.accF - 0.5f, 144, 61, 18, 6);
    Meter_DrawTurned(d, t, x + 39.0f, y + 70.0f, -3, -3, 15, 3, 0.75f - Clamped(r.fuel, kFuelMax) / (float)kFuelMax,
                     144, 61, 18, 6);
    Meter_DrawTurned(d, t, x + 70.0f, y + 101.0f, -3, -3, 15, 3, 2.0f * r.timeOfDay / 108000.0f - 0.5f, 144, 61, 18, 6);
    Meter_DrawTurned(d, t, x + 70.0f, y + 70.0f, -5, -5, 52, 5, Mph(r) / 186.4512f * 1.5f + 0.6667f, 144, 48, 57, 10);
}

void Meter_Draw2(gfx::Draw2D& d, int x, int y, const MeterReadout& r) {
    // The Rainbow: the dial, the gear, the speed (three digits), the rpm's arc of 10, the fins' needle, the fuel bar
    // cut by palette.
    const gfx::Texture& t = d.Get("ITEM/METER2.GSL", 0);
    for (const MeterPiece& p : kM2Dial) Piece(d, t, p, x, y);
    d.Sprite(t, x + 51.0f, y + 65.0f, 10, 20, 170.0f + 10 * Gear(r), 30, 10, 20);
    Digits(Mph(r), true, [&](int i, int n) {
        d.Sprite(t, x + 123.0f - 20 * i, y + 50.0f, 20, 26, 110.0f + 20 * (n % 7), 26.0f * (n / 7), 20, 26);
    });
    const int rpm = Scaled(r.rpm, kRpmMax, 10);
    for (int i = 0; i < rpm; ++i) Piece(d, t, kM2Rpm[i], x, y);
    if (r.finsPart) {
        static const float kFinsAngle[3] = {0.5f, 0.8333f, 0.1667f};   // D_00332F68, D_00332F6C
        const float angle = kFinsAngle[Meter_FinsIndex(r.fins)];
        Meter_DrawTurned(d, t, x + 89.0f, y + 18.0f, -4, -15, 4, 3, angle, 241, 30, 8, 18);
    }
    Piece(d, d.Get("ITEM/METER2.GSL", 1, 14 - Scaled(r.fuel, kFuelMax, 14)), kM2Fuel, x, y);
}

void Meter_Draw3(gfx::Draw2D& d, int x, int y, const MeterReadout& r, MeterState& s) {
    // The Space meter: by night (Meter_ClockIcon's moon, in races too) its backdrop modulated twice as bright (TEX0
    // entry 2, RGBAQ 0x80FFFFFF); the fuel's arc turned by what's left; a planet cycling its palettes as fast as the
    // rpm; the dial; the gear, the speed, the field's clock and the fins.
    const char kFile[] = "ITEM/METER3.GSL";
    if (Meter_ClockIcon(r.timeOfDay) == 0)
        d.Sprite(d.Get(kFile, 0, 0), (float)x, (float)y, 132, 128, 0, 0, 132, 128, 0x80FFFFFF);
    const float px = x + 58.0f, py = y + 63.0f;
    const int fuel = Clamped(r.fuel, kFuelMax);
    Meter_DrawTurned(d, d.Get(kFile, 1, 15), px, py, -44, -57, 44, -17, 0, 160, 72, 88, 40);
    Meter_DrawTurned(d, d.Get(kFile, 1, 4), px, py, -44, -57, 44, -17, 0.5667f - fuel / (float)kFuelMax * 0.5667f, 160,
                     72, 88, 40);
    if (fuel == 0) Piece(d, d.Get(kFile, 1, 15), kM3Empty, x, y);
    const int rpm = Clamped(r.rpm, kRpmMax);
    int palette = 5;
    if (rpm != 0) {
        if (!r.paused) s.accI += rpm / 1000;
        palette = 6 + (s.accI >> 4) % 9;
    }
    Piece(d, d.Get(kFile, 1, palette), kM3Planet, x, y);
    Piece(d, d.Get(kFile, 0, 1), kM3Dial, x, y);
    d.Sprite(d.Get(kFile, 1, 1), x + 86.0f, y + 92.0f, 24, 16, 24.0f * Gear(r), 24, 24, 16);
    const gfx::Texture& digits = d.Get(kFile, 1, 0);
    Digits(Mph(r), false, [&](int i, int n) {
        d.Sprite(digits, x + 66.0f - 16 * i, y + 44.0f, 16, 22, 16.0f * n, 0, 16, 22);
    });
    if (r.clock >= 0) {
        // The field's clock: Meter_ClockDigits' HH:MM, digits only.
        const gfx::Texture& t = d.Get(kFile, 1, 2);
        int bcd = Meter_ClockDigits(r.clock);
        for (int i = 0; i < 5; ++i, bcd >>= 4)
            d.Sprite(t, x + 51.0f - 8 * i, y + 84.0f, 8, 10, 8.0f * (bcd & 0xF), 40, 8, 10);
    }
    if (r.finsPart) Piece(d, d.Get(kFile, 1, 3), kM3Fins[Meter_FinsIndex(r.fins)], x, y);
}

void Meter_Draw4(gfx::Draw2D& d, int x, int y, const MeterReadout& r) {
    // The Triangle: the base, the fuel's column cut by palette, the rpm's 10 bars, the frame, the speed, the fins' hub
    // (always) and arrows, the gear in its circle.
    const char kFile[] = "ITEM/METER4.GSL";
    Piece(d, d.Get(kFile, 3, 0), kM4Base, x, y);
    Piece(d, d.Get(kFile, 2, 15 - Scaled(r.fuel, kFuelMax, 15)), kM4Fuel, x, y);
    const gfx::Texture& bars = d.Get(kFile, 1, 0);
    const int rpm = Scaled(r.rpm, kRpmMax, 10);
    for (int i = 0; i < rpm; ++i) Piece(d, bars, kM4Rpm[i], x, y);
    const gfx::Texture& t = d.Get(kFile, 0, 0);
    for (const MeterPiece& p : kM4Frame) Piece(d, t, p, x, y);
    Digits(Mph(r), false, [&](int i, int n) {
        d.Sprite(t, x + 106.0f - 12 * i, y + 164.0f, 12, 14, 12.0f + 12 * n, 113, 12, 14);
    });
    const gfx::Texture& fins = d.Get(kFile, 0, 6);
    Piece(d, fins, kM4FinsHub, x, y);
    if (r.finsPart) {
        switch (Meter_FinsIndex(r.fins)) {
        case 0:
            Piece(d, fins, kM4FinsLevelL, x, y);
            Piece(d, fins, kM4FinsLevelR, x, y);
            break;
        case 1: Piece(d, fins, kM4FinsUp, x, y); break;
        default: Piece(d, fins, kM4FinsDown, x, y); break;
        }
    }
    Piece(d, d.Get(kFile, 0, 10), kM4GearBack, x, y);
    d.Sprite(d.Get(kFile, 0, 8), x + 163.0f, y + 160.0f, 16, 10, 16.0f * Gear(r), 64, 16, 10);
}

void Meter_Draw5(gfx::Draw2D& d, int x, int y, const MeterReadout& r, MeterState& s) {
    // Love Sick: the dial, the wings in the rpm's level (palette 0-7), the face, at level 7 a heart swinging side to
    // side (2 frames a pose), the speed, the gear, the fins and the fuel (cut by palette).
    const char kFile[] = "ITEM/METER5.GSL";
    const gfx::Texture& dial = d.Get(kFile, 1, 8);
    for (const MeterPiece& p : kM5Dial) Piece(d, dial, p, x, y);
    const int level = Scaled(r.rpm, kRpmMax, 9), wings = level < 7 ? level : 7;
    for (const MeterPiece& p : kM5Wings) Piece(d, d.Get(kFile, 1, wings), p, x, y);
    const gfx::Texture& face = d.Get(kFile, 0, 0);
    for (const MeterPiece& p : kM5Face) Piece(d, face, p, x, y);
    if (wings == 7) {
        int k = s.accI >> 1;
        if (!r.paused) {
            ++s.accI;
            if (k >= 8) {
                s.accI = 0;
                k = 0;
            }
        }
        Piece(d, face, kM5Swing[kM5SwingOrder[k & 7]], x, y);
    }
    const gfx::Texture& digits = d.Get(kFile, 1, 10);
    Digits(Mph(r), false, [&](int i, int n) {
        d.Sprite(digits, x + 87.0f - 20 * i, y + 55.0f, 20, 24, 72.0f + 20 * (n % 8), 72.0f + 24 * (n / 8), 20, 24);
    });
    const int g = Gear(r);
    d.Sprite(face, x + 65.0f, y + 26.0f, 22, 18, 50.0f + 22 * (g % 3), 18.0f * (g / 3), 22, 18);
    if (r.finsPart) {
        static const int kPair[3] = {0, 4, 2};
        const gfx::Texture& fins = d.Get(kFile, 1, 9);
        const int k = kPair[Meter_FinsIndex(r.fins)];
        Piece(d, fins, kM5Fins[k], x, y);
        Piece(d, fins, kM5Fins[k + 1], x, y);
    }
    const gfx::Texture& fuel = d.Get(kFile, 2, 11 - Scaled(r.fuel, kFuelMax, 11));
    for (const MeterPiece& p : kM5Fuel) Piece(d, fuel, p, x, y);
}

void Meter_Draw6(gfx::Draw2D& d, int x, int y, const MeterReadout& r, MeterState& s) {
    // Life: the fuel's arc turned by what's left (to 145 degrees), the tank full or empty, the dial, a wheel turning
    // with the rpm (backwards in reverse), the gear's pips, the speed's digits round a ring, the fins.
    const char kFile[] = "ITEM/METER6.GSL";
    const float px = x + 64.0f, py = y + 64.0f;
    const int fuel = Clamped(r.fuel, kFuelMax);
    Meter_DrawTurned(d, d.Get(kFile, 1, 3), px, py, -58, -58, 36, 18, 0, 140, 0, 94, 76);
    Meter_DrawTurned(d, d.Get(kFile, 1, 2), px, py, -58, -58, 36, 18, fuel / (float)kFuelMax * 0.8056f, 140, 0, 94, 76);
    Piece(d, d.Get(kFile, 1, 0), fuel == 0 ? kM6Empty : kM6Full, x, y);
    const gfx::Texture& t = d.Get(kFile, 0, 0);
    d.Sprite(t, (float)x, (float)y, 132, 128, 0, 0, 132, 128);
    const int g = Gear(r);
    if (!r.paused) s.accF = fmodf(s.accF + (g == 0 ? -r.rpm : r.rpm) / (float)kRpmMax * 0.125f, 2.0f);
    Meter_DrawTurned(d, t, x + 65.0f, y + 65.0f, -42, -42, 42, 42, s.accF, 143, 23, 84, 84);
    if (g == 0) {
        Piece(d, t, kM6Reverse, x, y);
    } else {
        for (int i = 6 - (g < 6 ? g : 6); i < 6; ++i) Piece(d, t, kM6GearPips[i], x, y);
    }
    // The digits are stored turned: 12 x 16 texels on a 16 x 12 quad, round the ring from straight down.
    const gfx::Texture& digits = d.Get(kFile, 1, 1);
    Digits(Mph(r), false, [&](int i, int n) {
        Meter_DrawTurned(d, digits, x + 65.0f, y + 65.0f, -8, 42, 8, 54, 0.05f + 0.105f * i, 12.0f * n, 64, 12, 16);
    });
    if (r.finsPart) Piece(d, t, kM6Fins[Meter_FinsIndex(r.fins)], x, y);
}

void Meter_Draw7(gfx::Draw2D& d, int x, int y, const MeterReadout& r, MeterState& s, int frame) {
    // Cherry: the dial, a cherry turning with the rpm, the fuel in 16 strips (EMPTY blinking at none), the speed, the
    // gear, and both fins arrows always (the set one blinking).
    const char kFile[] = "ITEM/METER7.GSL";
    const gfx::Texture& t = d.Get(kFile, 0, 0);
    d.Sprite(t, (float)x, (float)y, 168, 128, 0, 0, 168, 128);
    const int spin = Clamped(r.rpm, kRpmMax) / 1000;
    if (spin) {
        if (!r.paused) s.accI += spin;
        Piece(d, t, kM7Spin[(s.accI / 20) % 5], x, y);
    }
    const int f = Scaled(r.fuel, kFuelMax, 16);
    for (int j = f; j < 16; ++j)
        d.Sprite(d.Get(kFile, 1, 1), x + 18.0f, y + 63.0f - 4 * j, 120, 4, 0, 60.0f - 4 * j, 120, 4);
    for (int j = 0; j < f; ++j)
        d.Sprite(d.Get(kFile, 1, 0), x + 18.0f, y + 63.0f - 4 * j, 120, 4, 0, 60.0f - 4 * j, 120, 4);
    if (f == 0) Piece(d, d.Get(kFile, 1, 11 + (frame >> 3) % 3), kM7Empty, x, y);
    const gfx::Texture& digits = d.Get(kFile, 1, 9);
    Digits(Mph(r), false, [&](int i, int n) {
        d.Sprite(digits, x + 116.0f - 21 * i, y + 63.0f, 24, 26, 24.0f * n, 70, 24, 26);
    });
    d.Sprite(d.Get(kFile, 1, 10), x + 20.0f, y + 81.0f, 32, 32, 32.0f * Gear(r), 96, 32, 32);
    const int fins = r.finsPart ? Meter_FinsIndex(r.fins) : 0, lit = 6 + ((frame >> 3) & 1);
    Piece(d, d.Get(kFile, 1, fins == 1 ? lit : 5), kM7FinsUp, x, y);
    Piece(d, d.Get(kFile, 1, fins == 2 ? lit : 5), kM7FinsDown, x, y);
}

void Meter_Draw8(gfx::Draw2D& d, int x, int y, const MeterReadout& r, int frame) {
    // The Duck: the fuel (cut by palette) under the dial, EMPTY at none, the speed, the gear's stack, the fins' arrow
    // cycling its colours, the rpm's dots with a blinking head.
    const char kFile[] = "ITEM/METER8.GSL";
    const int f = Scaled(r.fuel, kFuelMax, 13);
    Piece(d, d.Get(kFile, 2, 13 - f), kM8Fuel, x, y);
    const gfx::Texture& t = d.Get(kFile, 0, 0);
    d.Sprite(t, (float)x, (float)y, 128, 128, 0, 0, 128, 128);
    if (f == 0) Piece(d, t, kM8Empty, x, y);
    const gfx::Texture& digits = d.Get(kFile, 1, 0);
    Digits(Mph(r), false, [&](int i, int n) {
        d.Sprite(digits, x + 81.0f - 16 * i, y + 20.0f, 16, 24, 16.0f * n, 0, 16, 24);
    });
    const int g = Gear(r);
    if (g == 0) {
        Piece(d, t, kM8Reverse, x, y);
    } else {
        for (int i = 0; i < g && i < 6; ++i) Piece(d, t, kM8Gear[i], x, y);
    }
    if (r.finsPart) {
        const int fins = Meter_FinsIndex(r.fins);
        const gfx::Texture& arrow = d.Get(kFile, 1, 1 + (frame >> 3) % 3);
        if (fins == 1) Piece(d, arrow, kM8FinsUp, x, y);
        if (fins == 2) Piece(d, arrow, kM8FinsDown, x, y);
    }
    const int n = Scaled(r.rpm, kRpmMax, 10);
    for (int i = 0; i + 1 < n; ++i) Piece(d, t, kM8Rpm[i], x, y);
    if (n >= 1 && (frame & 1))
        d.Sprite(t, x + kM8Rpm[n - 1].x - 4.0f, y + kM8Rpm[n - 1].y - 7.0f, 26, 30, 192, 96, 26, 30);
}

void Meter_Draw9(gfx::Draw2D& d, int x, int y, const MeterReadout& r, int frame) {
    // The Devil: the dial, the plate and its mark, the speed and the gear in the glyph sheet, the fins, the rpm's and
    // the fuel's columns of bars (the fuel's all flickering at none), then the devil over it all.
    const char kFile[] = "ITEM/METER9.GSL";
    const gfx::Texture& dial = d.Get(kFile, 0, 0);
    Piece(d, dial, kM9Dial, x, y);
    Piece(d, d.Get(kFile, 1, 6), kM9Plate, x, y);
    const gfx::Texture& glyphs = d.Get(kFile, 1, 5);
    Piece(d, glyphs, kM9Mark, x, y);
    auto glyph = [&](float gx, float gy, int cell) {
        d.Sprite(glyphs, gx, gy, 16, 16, 16.0f * (cell % 4), 16.0f * (cell / 4), 16, 16);
    };
    Digits(Mph(r), false, [&](int i, int n) { glyph(x + 81.0f - 14 * i, y + 114.0f, n + 1); });
    const int g = Gear(r);
    glyph(x + 120.0f, y + 114.0f, g + (g > 0));
    if (r.finsPart) {
        const int fins = Meter_FinsIndex(r.fins);
        if (fins == 1) Piece(d, dial, kM9FinsUp, x, y);
        if (fins == 2) Piece(d, dial, kM9FinsDown, x, y);
    }
    const gfx::Texture& rpmBar = d.Get(kFile, 1, 4);
    const int n = Scaled(r.rpm, kRpmMax, 14);
    for (int j = 0; j < n; ++j) d.Sprite(rpmBar, x + 56.0f, y + 108.0f - 8 * j, 96, 8, 160, 104.0f - 8 * j, 96, 8);
    const int f = Scaled(r.fuel, kFuelMax, 13);
    const gfx::Texture& fuelBar = d.Get(kFile, 1, f > 0 ? 0 : 1 + (frame >> 1) % 3);
    for (int j = 0, count = f > 0 ? f : 13; j < count; ++j)
        d.Sprite(fuelBar, x + 32.0f, y + 100.0f - 8 * j, 80, 8, 72, 96.0f - 8 * j, 80, 8);
    Piece(d, dial, kM9Devil, x, y);
}

}  // namespace

void Meter_DrawPart(gfx::Draw2D& d, int meter, int x, int y, const MeterReadout& r, MeterState& s, int frame) {
    switch (meter) {
    case 1: Meter_Draw1(d, x, y, r, s); break;
    case 2: Meter_Draw2(d, x, y, r); break;
    case 3: Meter_Draw3(d, x, y, r, s); break;
    case 4: Meter_Draw4(d, x, y, r); break;
    case 5: Meter_Draw5(d, x, y, r, s); break;
    case 6: Meter_Draw6(d, x, y, r, s); break;
    case 7: Meter_Draw7(d, x, y, r, s, frame); break;
    case 8: Meter_Draw8(d, x, y, r, frame); break;
    case 9: Meter_Draw9(d, x, y, r, frame); break;
    default: break;
    }
}

}  // namespace game
