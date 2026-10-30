// The opponents' driving: a reimplementation of the game's car AI (car/ai, D_003356DC = Ai_Drive). The AI drives
// through the same input bits as the pad (car_physics.h): it holds each stretch of the course at that stretch's
// speed, under a cap set by its grid slot, and steers for a gate two stretches ahead. See docs/ai.md.
//
// A path (data/ai/Cnn.txt, written by tools/modelconv from the executable's D_002BD578) is a chain of gates, short
// lines across the racing line, and zones, the stretches between two gates. Where the course forks, a zone has two
// next (or previous) zones, and a split line through the gates' split points picks between them.
#pragma once

#include <stdint.h>

#include <string>
#include <vector>

namespace game {

struct CarBody;
typedef void (*CarDebugPrint)(int x, int y, const char* format, ...);   // car_physics.h's

struct AiLine {
    float x0, z0, x1, z1;    // the gate's ends (world x, z)
    float splitX, splitZ;    // a point on the split line, where the path forks
};

struct AiZone {
    uint8_t in, out;           // the gates it runs between
    uint8_t prev, prevAlt;     // the zones before it (equal unless the path forks)
    uint8_t next, nextAlt;     // and after it
    uint8_t rank;              // its place along the course, for the race order (+0x24B)
    uint8_t speed;             // the speed to hold in it (5 km/h; 0: none), D_002A9B80's byte for this zone
};

class AiPath {
public:
    // Reads `path` (data/ai/Cnn.txt); then, if there is one, the zone table the zone table editor saved for it
    // (Cnn.speeds.txt, Ai_LoadZoneTable) replaces the zones' speeds.
    bool Load(const std::string& path);
    bool Empty() const { return zones_.empty(); }

    const AiLine& Line(int i) const { return lines_[i]; }
    const AiZone& Zone(int i) const { return zones_[i]; }
    int Zones() const { return (int)zones_.size(); }

private:
    std::vector<AiLine> lines_;
    std::vector<AiZone> zones_;
};

// A car's place on the path and its AI settings (fields of its 0x270-byte record).
struct AiCar {
    int zone;           // +0x24A: the zone it's in
    int rank;           // +0x24B: that zone's rank
    float dist;         // +0x24C: how far it is to the gate it aims for, along the course
    int wrongWay;       // +0x248: frames it has faced more than 112 degrees off the course's direction
    int lane;           // +0x194 & 3: which part of the gate it aims through (0, 3 whole; 1 second half; 2 first)
    int cap;            // +0x246: its speed cap (2.5 km/h; 0: none)
    // +0x258: its speed table, a byte per zone; null for the path's (the course's). An Adventure teammate's is its
    // own in the save (Ai_SpeedTable), which its driving changes (Ai_Drive's learning).
    uint8_t* speeds = nullptr;
    // +0x25C: its marks, a byte per zone (D_01759D70 + 0x100 x car, cleared at the start): 0x80 if it was steering
    // the last frame it was in the zone, 0x40 if not, OR its speed then (5 km/h, 2-62).
    uint8_t marks[256];
};

// Ai_Init's speed table for an Adventure teammate on course `course`: the save's (record +0x1300, a 0x1000 block per
// teammate, a 0x100 table per D_002AC080[course]). Null for courses without one (0x12 on).
uint8_t* Ai_SpeedTable(uint8_t (*tables)[16][256], int course);

// Ai_Init (0x0024FCD8) and the AI part of Car_Init (0x00218458): a car's settings. `lane` is the record's +0x194
// (3 + the opponent's number in a Quick Race, 0 for the player); `level` is the parts record's AI level and `slot`
// the car's grid slot. Every car but the one in slot 0 gets a cap of level + 1 - slot (with nobody else's
// cars on the grid); a player's car (`player`) gets none.
void Ai_Init(AiCar& a, int lane, int level, int slot, bool player);

// Ai_TrackZone's debug readouts (the Car Readouts extra): while a printer is set it prints the zone, and with
// `full` (player 1's car) the target line, the yaw, the distance and REVERSE, as the debug byte did. The caller sets
// it around a player's car.
void Ai_SetDebugPrint(CarDebugPrint print, bool full);

// Ai_TrackZone (0x00250068): moves the car's zone on (or back) to the one it's in, then its rank and distance.
// Returns the zone after it (on the side of a fork the car is on), whose far gate the AI aims for. Runs for every
// car, the player's too: the race order uses the rank and the distance.
int Ai_TrackZone(const AiPath& p, AiCar& a, const CarBody& c);

// Ai_TargetAngles (0x0024FED8): the directions from `pos` to the two edges of the part of gate `l` that `lane`
// aims through (16-bit angles, 0 = +z, 0x4000 = +x): `first` towards its first end (or middle), `second` towards
// its second end (or middle).
void Ai_TargetAngles(const AiLine& l, const float* pos, int lane, int16_t& first, int16_t& second);

// Ai_Drive (0x002508E0): this frame's input bits. Accelerates, coasts (one step over) or brakes (two over) to hold
// the zone's speed and the cap; steers while the target lies outside the car's heading. A car facing more than a
// quarter turn away from it is turned 0x100 a frame on the spot once the race has started (`go`). `steer`: the
// car has reached the line (its lap zone, +0x19A, is set); until then it drives straight. It marks the zone (the
// car's marks). `learn` (Car_Input's last argument: an Adventure teammate, flags 0x30, the AI driving it): first
// the zone's speed in its own table moves by what the marks say of the last time round (ai.md, "Marks").
uint32_t Ai_Drive(const AiPath& p, AiCar& a, int next, CarBody& c, bool steer, bool go, bool learn = false);
// The Slow Rivals cheat (extras.h): while set, Ai_Drive aims for three quarters of the zone's speed. The race sets it
// around an opponent's car and clears it after.
void Ai_SetSlow(bool slow);

// The AI zone table editor (the debug pause item 6, Ai_ZoneTableTask; the race runs it, race_zone_table.cpp).
//
// Ai_DrawZoneTable (0x00250738): car `a`'s speed table `speeds` (256 zones) and its marks, through `print`, in
// columns of 20 zones: only the column of the car's zone and those either side.
void Ai_DrawZoneTable(CarDebugPrint print, const AiCar& a, const uint8_t* speeds);
// Ai_SaveZoneTable (0x0020E5B8): writes `speeds` to `path`, after a `header` line, as the game wrote its tables:
// one line a zone up to the last non-zero one, "\t%d,\t/* zone %2d */". `status` gets what the game printed after
// "AI Save %s": "done", "File open error" or "write error!".
bool Ai_SaveZoneTable(const std::string& path, const std::string& header, const uint8_t* speeds,
                      const char** status);
// Reads such a file (the PC's): each line that starts with a number is the next zone's speed; zones after the last
// are 0. False (and `speeds` untouched) if there's no file.
bool Ai_LoadZoneTable(const std::string& path, uint8_t* speeds);

}  // namespace game
