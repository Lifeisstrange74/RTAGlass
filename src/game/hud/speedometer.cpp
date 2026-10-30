#include "speedometer.h"

#include <math.h>

#include "../race/car_physics.h"

namespace game {

namespace {

const float kPi = 3.14159265f;
const char kMeter0[] = "ITEM/METER0.GSL";   // picture 0: the dial and digits (T8); 1: needle and gauges (T4)

const int kFuelFull = 0x40000;
const int kFuelLow = 0x17FFF;   // at or below: the gauge blinks

}  // namespace

void Meter_DrawTurned(gfx::Draw2D& d, const gfx::Texture& t, float px, float py, float x0, float y0, float x1,
                      float y1, float angle, float u, float v, float w, float h) {
    float c = cosf(angle * kPi), s = sinf(angle * kPi);
    const float cx[4] = {x0, x1, x0, x1}, cy[4] = {y0, y0, y1, y1};
    float x[4], y[4];
    for (int i = 0; i < 4; ++i) {
        x[i] = px + cx[i] * c - cy[i] * s;
        y[i] = py + cx[i] * s + cy[i] * c;
    }
    d.Quad(t, x, y, u, v, w, h);
}

const int kMeterTopLeft[11][2] = {
    {484, 294}, {484, 291}, {474, 304}, {492, 304}, {424, 244}, {472, 292},
    {492, 304}, {456, 304}, {441, 304}, {470, 300}, {492, 336},
};

void Meter_Draw(gfx::Draw2D& d, int meter, const MeterReadout& r, MeterState& s, int frame) {
    if (meter < 0 || meter > 10) meter = 0;
    const int x = kMeterTopLeft[meter][0], y = kMeterTopLeft[meter][1];
    if (meter == 0) Meter_Draw0(d, x, y, r, frame);
    else if (meter == 10) Meter_Draw10(d, x, y, r, frame);
    else Meter_DrawPart(d, meter, x, y, r, s, frame);
}

MeterReadout Meter_Read(const CarBody& car) {
    MeterReadout r;
    r.rpm = car.rpm;
    r.fuel = car.fuel;
    float vx = car.vel[0] / 32768.0f, vz = car.vel[2] / 32768.0f;
    r.speed = (int)(sqrtf(vx * vx + vz * vz) * 216.0f);
    r.gear = car.gear;
    r.fins = car.fins;
    r.finsPart = false;
    r.clock = -1;
    r.timeOfDay = 0;
    r.paused = false;
    return r;
}

void Meter_Draw0(gfx::Draw2D& d, int x, int y, const MeterReadout& r, int frame) {
    int fuel = r.fuel < kFuelFull ? r.fuel : kFuelFull;
    bool blink = fuel <= kFuelLow && (frame & 0x20);
    float px = x + 70.0f, py = y + 70.0f;
    // The fuel gauge: a strip, and over it the full one turned up to 50 degrees by the fuel left; the dial covers
    // the turned part's lower end (D_00327820).
    Meter_DrawTurned(d, d.Get(kMeter0, 1, blink ? 3 : 0), px, py, 46, -30, 66, 30, 0, 40, 48, 20, 60);
    int palette = fuel == 0 ? 5 : blink ? 4 : 2;
    Meter_DrawTurned(d, d.Get(kMeter0, 1, palette), px, py, 46, -30, 66, 30, -fuel / (float)kFuelFull * 0.2777778f, 64,
                     48,
               20, 60);

    const gfx::Texture& dial = d.Get(kMeter0, 0);
    d.Sprite(dial, (float)x, (float)y, 140, 128, 0, 0, 140, 128);
    // The gear: R, 1-5 on one row, 6 and up on the next.
    int g = r.gear < 0 ? 0 : r.gear;
    d.Sprite(dial, x + 97.0f, y + 60.0f, 20, 14, 140.0f + 20 * (g % 5), 80.0f + 16 * (g / 5), 20, 14);
    // The speed in mph (D_00332F48 = 1.609), up to three digits from the right: 0-6 on one row, 7-9 below.
    int mph = (int)(r.speed / 1.609f);
    for (int i = 0, n = mph; i < 3; ++i) {
        int digit = n % 10;
        d.Sprite(dial, x + 89.0f - 17 * i, y + 94.0f, 16, 23, 140.0f + 16 * (digit % 7), 23.0f * (digit / 7), 16, 23);
        n /= 10;
        if (!n) break;
    }

    // The fins (part flag 0x1000; D_003278B0): one picture of both arrows, the palette lighting one by their setting
    // (12 neither, 14 for -1, 13 for 1).
    if (r.finsPart) {
        static const int kFinsPalette[3] = {12, 14, 13};
        const gfx::Texture& fins = d.Get(kMeter0, 1, kFinsPalette[Meter_FinsIndex(r.fins)]);
        d.Sprite(fins, x + 29.0f, y + 59.0f, 16, 27, 16, 48, 16, 27);
    }

    // The needle: 0 rpm points down to the left, 10000 up to the right (-0.7 to 0.3 half turns), then its hub.
    int rpm = r.rpm < 10000 ? r.rpm : 10000;
    const gfx::Texture& parts = d.Get(kMeter0, 1, 12);
    Meter_DrawTurned(d, parts, x + 70.0f, y + 68.0f, -4, -59, 4, 0, rpm / 10000.0f - 0.7f, 3, 53, 8, 59);
    d.Sprite(parts, x + 64.0f, y + 64.0f, 12, 12, 16, 80, 12, 12);

    // The field's clock, between the speed and the fins: its frame (D_00327890), then Meter_ClockIcon's moon, sun or
    // evening, then Meter_ClockDigits' HH:MM (glyph 10 the colon) from the right, 9 apart.
    if (r.clock >= 0) {
        d.Sprite(parts, x + 20.0f, y + 100.0f, 94, 38, 0, 0, 94, 38);
        d.Sprite(dial, x + 22.0f, y + 113.0f, 24, 24, 140.0f + 24 * Meter_ClockIcon(r.clock), 49, 24, 24);
        int digits = Meter_ClockDigits(r.clock);
        for (int i = 0; i < 5; ++i, digits >>= 4)
            d.Sprite(parts, x + 91.0f - 9 * i, y + 122.0f, 8, 8, 8.0f * (digits & 0xF), 40, 8, 8);
    }
}

// Meter_ClockIcon (0x00249670): METER0 picture 0's icon by the clock: 0 the moon until 6:30 (0xE483) and after 18:00
// (0x278D0), 1 the sun until 15:59 (0x2327F, the 0x13C67 bound between them giving 1 too), then 2 the evening.
int Meter_ClockIcon(int32_t clock) {
    if (clock <= 0xE483) return 0;
    if (clock <= 0x2327F) return 1;
    if (clock <= 0x278D0) return 2;
    return 0;
}

// Meter_ClockDigits (0x002495F0): the clock in minutes (150 frames, so 9000 an hour) as BCD HH:MM from the low
// nibble: minutes, tens of minutes, 0xA (the colon), hours, tens of hours.
int Meter_ClockDigits(int32_t clock) {
    int t = clock / 150, r = t % 10;
    t /= 10;
    r |= (t % 6) << 4;
    t /= 6;
    r |= 0xA00 | (t % 10) << 12;
    t /= 10;
    return r | (t % 10) << 16;
}

// Meter_Draw10 (func_0024E1A0): picture 0 is the plate (132 x 96, with the rpm arc's slices and the digits beside
// it), 1 the fuel bar cut to 16 lengths by palette, 2 the bar's track, EMPTY and the gear glyphs.
void Meter_Draw10(gfx::Draw2D& d, int x, int y, const MeterReadout& r, int frame) {
    const char kMeter10[] = "ITEM/METER10.GSL";
    // The fuel in 15 steps, rounding up (Meter_Fuel(car, 15, 1)): its track, the bar (palette 15 - f is f/15 of it),
    // and EMPTY at none, cycling red, orange and pale yellow every 8 frames. The plate's frame goes over them.
    int fuel = r.fuel < kFuelFull ? (r.fuel > 0 ? r.fuel : 0) : kFuelFull;
    int f = (int)(((int64_t)fuel * 15 + kFuelFull - 1) / kFuelFull);
    d.Sprite(d.Get(kMeter10, 2, 0), x + 7.0f, y + 76.0f, 100, 18, 0, 0, 100, 18);
    d.Sprite(d.Get(kMeter10, 1, 15 - f), x + 7.0f, y + 76.0f, 100, 18, 0, 0, 100, 18);
    if (f == 0) d.Sprite(d.Get(kMeter10, 2, (frame >> 3) % 3 + 1), x + 30.0f, y + 82.0f, 58, 8, 0, 24, 58, 8);
    const gfx::Texture& plate = d.Get(kMeter10, 0);
    d.Sprite(plate, (float)x, (float)y, 132, 96, 0, 0, 132, 96);
    // The rpm in 16 slices of the arc (Meter_Rpm(car, 16, 1)), each a 4 x 60 column that holds its own part of it.
    int rpm = r.rpm < 10000 ? (r.rpm > 0 ? r.rpm : 0) : 10000;
    int n = (rpm * 16 + 9999) / 10000;
    for (int i = 0; i < n; ++i) d.Sprite(plate, x + 34.0f + 6 * i, (float)y, 4, 60, 136.0f + 6 * i, 0, 4, 60);
    // The speed in mph, up to three digits from the right, 19 apart.
    int mph = (int)(r.speed / 1.609f);
    for (int i = 0, v = mph; i < 3; ++i) {
        d.Sprite(plate, x + 78.0f - 19 * i, y + 49.0f, 20, 26, 20.0f * (v % 10), 102, 20, 26);
        v /= 10;
        if (!v) break;
    }
    // The fins (part flag 0x1000): their setting by Meter_Fins, an arrow left of the speed, picture 0's (240, 0) at
    // (9, 12) for -1 (D_00327FB0) and (240, 24) at (9, 36) for 1 (D_00327FD0); nothing at 0.
    if (r.finsPart && r.fins == -1) d.Sprite(plate, x + 9.0f, y + 12.0f, 16, 16, 240, 0, 16, 16);
    if (r.finsPart && r.fins == 1) d.Sprite(plate, x + 9.0f, y + 36.0f, 16, 16, 240, 24, 16, 16);
    // The gear in the white circle: R, then 1st on (glyph 1, "0", is never used), black on white (palette 4).
    int g = r.gear < 0 ? 0 : r.gear;
    int cell = g + (g != 0);
    d.Sprite(d.Get(kMeter10, 2, 4), x + 10.0f, y + 62.0f, 16, 10, 16.0f * cell, 32, 16, 10);
}

}  // namespace game
