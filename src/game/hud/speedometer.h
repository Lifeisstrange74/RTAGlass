// The speedometer: the tachometer dial with its needle, the fuel gauge, the gear and the speed in mph. A high-level
// reimplementation of Meter_Draw (0x002490A0), which runs one of eleven meter routines (D_002A8FB0) by the player's
// meter part (the save's car 0's parts[14]), each drawing its own ITEM/METERn.GSL (loaded at TBP 0x237A): meter 0
// here, the one every car starts with, and Meter_Draw10, the split screen's; meters 1-9 in meters.cpp. See
// docs/hud.md, "The meters".
//
// Meters 0 and 3 also draw the Adventure's clock on the field (the routines' last argument). They read only the
// readout below, so the races and the field fill it the same way.
#pragma once

#include "../../gfx/draw2d.h"

namespace game {

struct CarBody;

// What a meter shows, read from the car (Meter_Rpm 0x00249520, Meter_Fuel 0x00249568, Meter_Speed 0x002494E8,
// Meter_Gear 0x002495B8).
struct MeterReadout {
    int rpm;     // +0x1D0: 0..10000
    int fuel;    // +0x23C: 0..0x40000 (full)
    int speed;   // km/h: 216 x the speed across the ground (func_0021D358 of +0xF0)
    int gear;    // +0x1FF: 0 reverse, 1..
    int fins;    // +0x244: the fins' setting, -1, 0 or 1 (Meter_Fins 0x002495C0)
    bool finsPart;   // the car has the fins (part flag 0x1000): Meter_Draw passes it to the routine
    // On the field (Meter_Draw's field flag: course -1 or 0x22), the time of day (info+0x14, Meter_ClockDigits and
    // Meter_ClockIcon read it); -1 elsewhere, no clock.
    int32_t clock;
    int32_t timeOfDay;   // info+0x14 always (Meter_TimeOfDay, func_002495E8): meter 1's hour hand, meter 3's night
    bool paused;         // race flag 0x8000: the meters that count (1, 3, 5, 6, 7) hold still
};
// The per-player count some meters keep (buf+0x1000 of D_01757BC0[i]): a float in meters 1 and 6, an int in meters
// 3, 5 and 7; cleared on the meter task's first frame.
struct MeterState {
    float accF = 0;
    int32_t accI = 0;
};
// The car's readout; `finsPart` is the race's to set (the part flags are the parts record's).
MeterReadout Meter_Read(const CarBody& car);

// Where the meter goes (D_002A8FF0): one player, then players 1 and 2 of a split screen.
const int kMeterPosition[3][2] = {{484, 294}, {484, 70}, {484, 294}};

// Meter_Draw (0x002490A0) for one player: routine `meter` (the meter part, 0-10) at its own place (kMeterTopLeft).
// `frame` is the meter task's count (blinks and cycles), `s` the player's count.
void Meter_Draw(gfx::Draw2D& d, int meter, const MeterReadout& r, MeterState& s, int frame);
// Each meter's top left for one player (D_002A8FF0, D_002A9038 ... D_002A9AF0, entry 0).
extern const int kMeterTopLeft[11][2];
// Meter_Draw0 (0x002496C8): meter 0 with its top left at (x, y). `frame` counts frames, for the low-fuel blink.
void Meter_Draw0(gfx::Draw2D& d, int x, int y, const MeterReadout& r, int frame);
// Meters 1-9 (meters.cpp: Meter_Draw1 0x00249FA0 ... Meter_Draw9 0x0024DA50).
void Meter_DrawPart(gfx::Draw2D& d, int meter, int x, int y, const MeterReadout& r, MeterState& s, int frame);
// func_0024EB10 (Sprite_DrawTurned): `w` x `h` texels at (u, v) over the corners (x0..x1, y0..y1) around the pivot
// (px, py), turned by `angle` half turns (clockwise on the screen).
void Meter_DrawTurned(gfx::Draw2D& d, const gfx::Texture& t, float px, float py, float x0, float y0, float x1,
                      float y1, float angle, float u, float v, float w, float h);
// Meter_Fins' index (0x002495C0): 1 for fins -1, 2 for 1, else 0.
inline int Meter_FinsIndex(int fins) { return fins == -1 ? 1 : fins == 1 ? 2 : 0; }
int Meter_ClockIcon(int32_t clock);     // 0x00249670: 0 the moon, 1 the sun, 2 the evening
int Meter_ClockDigits(int32_t clock);   // 0x002495F0: HH:MM as BCD nibbles, 0xA the colon
// Meter_Draw10 (func_0024E1A0): the split screen's small meter, whatever the car's meter part: the rpm as an arc of
// bars, the speed in mph, the gear and the fuel as a bar, with its top left at (x, y) (D_002A9AF0: (492, 112) for
// player 1, (492, 336) for player 2), and for a car with the fins an arrow by their setting. `frame` cycles EMPTY's
// colours.
void Meter_Draw10(gfx::Draw2D& d, int x, int y, const MeterReadout& r, int frame);

}  // namespace game
