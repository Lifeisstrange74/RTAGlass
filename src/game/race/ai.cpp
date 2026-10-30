#include "ai.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "../../util/files.h"
#include "car_physics.h"
#include "../../game_text.inc"   // the game's text, GT_<address>_<length>, made from the ELF

namespace game {

namespace {

const float kAngle = 10430.378f;   // D_00332F9C: 16-bit angle units per radian (0x8000 / pi)

// The game's 16-bit angle of the direction (dx, dz): atan2 scaled and truncated.
int16_t Angle(float dx, float dz) { return (int16_t)(int32_t)(atan2f(dx, dz) * kAngle); }

// The side test the AI uses for every line: true if p is on the line (x0, z0)-(x1, z1) or past it, the way the
// path runs.
bool Past(float x0, float z0, float x1, float z1, const float* p) {
    return (z1 - z0) * (p[0] - x1) - (x1 - x0) * (p[2] - z1) >= 0.0f;
}
bool PastGate(const AiLine& l, const float* p) { return Past(l.x0, l.z0, l.x1, l.z1, p); }

// Where the path forks: true if p is past the split line from the zone's first gate's split point to its second's
// (the alternative branch).
bool OnAlt(const AiPath& path, const AiZone& z, const float* p) {
    const AiLine &a = path.Line(z.in), &b = path.Line(z.out);
    return Past(a.splitX, a.splitZ, b.splitX, b.splitZ, p);
}

}  // namespace

bool AiPath::Load(const std::string& path) {
    lines_.clear();
    zones_.clear();
    FILE* f = fopen(path.c_str(), "r");
    if (!f) return false;
    char row[256];
    while (fgets(row, sizeof row, f)) {
        AiLine l;
        int v[8];
        if (sscanf(row, "line %f %f %f %f %f %f", &l.x0, &l.z0, &l.x1, &l.z1, &l.splitX, &l.splitZ) == 6) {
            lines_.push_back(l);
        } else if (sscanf(row, "zone %d %d %d %d %d %d %d %d", &v[0], &v[1], &v[2], &v[3], &v[4], &v[5], &v[6],
                          &v[7]) == 8) {
            AiZone z = {(uint8_t)v[0], (uint8_t)v[1], (uint8_t)v[2], (uint8_t)v[3],
                        (uint8_t)v[4], (uint8_t)v[5], (uint8_t)v[6], (uint8_t)v[7]};
            zones_.push_back(z);
        }
    }
    fclose(f);
    // Every index has to land inside the path.
    for (const AiZone& z : zones_) {
        size_t nl = lines_.size(), nz = zones_.size();
        if (z.in >= nl || z.out >= nl || z.prev >= nz || z.prevAlt >= nz || z.next >= nz || z.nextAlt >= nz) {
            lines_.clear();
            zones_.clear();
            return false;
        }
    }
    if (zones_.empty()) return false;
    // The PC's: a table the zone table editor saved for this path replaces its speeds (the shipped file stays as
    // modelconv wrote it).
    std::string base = path;
    if (base.size() > 4 && base.compare(base.size() - 4, 4, ".txt") == 0) base.resize(base.size() - 4);
    uint8_t table[256];
    if (Ai_LoadZoneTable(base + ".speeds.txt", table))
        for (size_t i = 0; i < zones_.size() && i < 256; ++i) zones_[i].speed = table[i];
    return true;
}

void Ai_Init(AiCar& a, int lane, int level, int slot, bool player) {
    memset(&a, 0, sizeof a);
    a.lane = lane & 3;
    // Car_Init: the cap is level + (cars of other players on the grid: none here) - slot + 1, except in the slot
    // that count names. The record keeps it in a byte.
    if (!player && level != 0 && slot != 0) a.cap = (uint8_t)(level + 1 - slot);
}

namespace {
CarDebugPrint g_debugPrint = nullptr;
bool g_debugFull = false;
bool g_slow = false;   // Ai_SetSlow
}  // namespace

void Ai_SetSlow(bool slow) { g_slow = slow; }

void Ai_SetDebugPrint(CarDebugPrint print, bool full) {
    g_debugPrint = print;
    g_debugFull = full;
}

int Ai_TrackZone(const AiPath& p, AiCar& a, const CarBody& c) {
    if (p.Empty()) return 0;
    const float* pos = c.pos;
    int guard = p.Zones();   // (the game has no limit; a loop of gates always ends)
    const AiZone* z = &p.Zone(a.zone);
    if (PastGate(p.Line(z->in), pos)) {
        // Past the zone's far gate: on to the next zone (the branch the car is on, at a fork), until it's inside.
        while (PastGate(p.Line(z->out), pos) && guard-- > 0) {
            a.zone = z->next == z->nextAlt ? z->next : OnAlt(p, *z, pos) ? z->nextAlt : z->next;
            z = &p.Zone(a.zone);
        }
    } else {
        // Behind its near gate: back a zone until it's past the near gate again.
        do {
            a.zone = z->prev == z->prevAlt ? z->prev : OnAlt(p, *z, pos) ? z->prevAlt : z->prev;
            z = &p.Zone(a.zone);
        } while (!PastGate(p.Line(z->in), pos) && guard-- > 0);
    }
    a.rank = z->rank;
    int next = z->next == z->nextAlt || OnAlt(p, *z, pos) ? z->nextAlt : z->next;

    // The course's direction here: square to the zone's far gate.
    const AiLine& out = p.Line(z->out);
    int16_t heading = (int16_t)(Angle(out.x0 - out.x1, out.z0 - out.z1) - 0x4000);
    int16_t off = (int16_t)(heading - c.yaw);
    a.wrongWay = off < -0x5000 || off > 0x5000 ? a.wrongWay + 1 : 0;

    // The distance to the middle of the next zone's far gate, along that direction.
    const AiLine& t = p.Line(p.Zone(next).out);
    float th = heading / kAngle;
    a.dist = ((t.z0 + t.z1) * 0.5f - pos[2]) * cosf(th) + ((t.x0 + t.x1) * 0.5f - pos[0]) * sinf(th);
    if (g_debugPrint) {
        // "ZONE %d %s %s": the zone, the side of its fork ahead the car is on, and (where the way ahead doesn't
        // fork) the side of the fork behind.
        const char* ahead = z->next == z->nextAlt ? GT_003344B0_6 : OnAlt(p, *z, pos) ? GT_003344A0_5 : GT_003344A8_4;
        const char* behind = z->next != z->nextAlt || z->prev == z->prevAlt ? GT_003344B0_6
                             : OnAlt(p, *z, pos)                            ? GT_003344A0_5
                                                                            : GT_003344A8_4;
        g_debugPrint(16, 152, GT_00328008_13, a.zone, ahead, behind);
        if (g_debugFull) {
            g_debugPrint(16, 160, GT_00328018_17, p.Zone(next).out, (unsigned)(uint16_t)heading);
            g_debugPrint(16, 168, GT_00328030_9, (unsigned)c.yaw);
            g_debugPrint(16, 176, GT_00328040_8, (int)(a.dist * 100.0f));
            if (a.wrongWay) g_debugPrint(300, 96, GT_003344B8_7);
        }
    }
    return next;
}

void Ai_TargetAngles(const AiLine& l, const float* pos, int lane, int16_t& first, int16_t& second) {
    float mx = (l.x0 + l.x1) * 0.5f, mz = (l.z0 + l.z1) * 0.5f;
    if (lane == 1) {
        first = Angle(mx - pos[0], mz - pos[2]);
        second = Angle(l.x1 - pos[0], l.z1 - pos[2]);
    } else if (lane == 2) {
        first = Angle(l.x0 - pos[0], l.z0 - pos[2]);
        second = Angle(mx - pos[0], mz - pos[2]);
    } else {
        first = Angle(l.x0 - pos[0], l.z0 - pos[2]);
        second = Angle(l.x1 - pos[0], l.z1 - pos[2]);
    }
}

uint8_t* Ai_SpeedTable(uint8_t (*tables)[16][256], int course) {
    // D_002AC080: the course's table (courses 0x0B and 0x10 share 0x06's and 0x0F's).
    static const uint8_t kTable[0x12] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 6, 11, 12, 13, 14, 14, 15};
    if (course < 0 || course >= 0x12) return nullptr;
    return (*tables)[kTable[course]];
}

// Ai_Drive's learning, for a teammate: the next zone's mark is from the last time round. Speeds and marks count in 5
// km/h, and a value outside 1-62 doesn't count.
// - The next zone's speed under its mark's speed less 2: that mark's "clean" bit (0x40) goes, and this zone's speed
//   comes down one if it's over this zone's mark's speed less 1 (and is 2 or more).
// - Then, the next zone marked steered (0x80): this zone's speed becomes its mark's speed less 1, if it was more
//   (or none). Else marked clean (0x40): its mark's speed plus 1, if it was less. Either way that mark's bit goes.
namespace {

void Ai_Learn(AiCar& a, int next) {
    uint8_t* s = a.speeds;
    uint8_t* m = a.marks;
    const int z = a.zone;
    auto counts = [](int v) { return v >= 1 && v <= 62; };
    if (s[next] != 0 && (m[next] & 0xC0)) {
        int mark = m[next] & 0x3F, d = 0;
        if (mark != 0 && counts(mark - 2)) d = s[next] - (mark - 2);
        if (d < 0) {
            m[next] &= 0xBF;
            if (s[z] >= 2) {
                int mz = m[z] & 0x3F, dz = 0;
                if (mz != 0 && counts(mz - 1)) dz = s[z] - (mz - 1);
                if (dz > 0) --s[z];
            }
        }
    }
    const int mz = m[z] & 0x3F;
    if (m[next] & 0x80) {
        m[next] &= 0x3F;
        bool over = true;
        if (s[z] != 0) over = mz != 0 && counts(mz - 1) && s[z] - (mz - 1) > 0;
        if (over && mz != 0 && counts(mz - 1)) s[z] = (uint8_t)(mz - 1);
    } else if (m[next] & 0x40) {
        m[next] &= 0x3F;
        if (mz != 0 && counts(mz + 1) && s[z] - (mz + 1) < 0) s[z] = (uint8_t)(mz + 1);
    }
}

}  // namespace

uint32_t Ai_Drive(const AiPath& p, AiCar& a, int next, CarBody& c, bool steer, bool go, bool learn) {
    if (p.Empty()) return 0;
    if (learn && a.speeds) Ai_Learn(a, next);
    // The speed: the driven wheels' in 5 km/h (the speedometer's scale, 216 km/h per 0x8000) against the zone's.
    uint32_t in = kCarAccel;
    int32_t speed = c.wheel * 216 / 163840;
    int target = (a.speeds ? a.speeds[a.zone] : p.Zone(a.zone).speed) & 0x7F;
    if (g_slow && target > 1) target = target * 3 / 4;
    if (target != 0) {
        int32_t over = speed - target;
        in = over < 1 ? kCarAccel : over < 2 ? 0 : kCarBrake;
    }
    // The cap, in 2.5 km/h.
    if (a.cap != 0) {
        int32_t over = c.wheel * 216 / 81920 - a.cap;
        if (over >= 3) in = kCarBrake;
        else if (over > 0) in = 0;
    }
    // The steering: towards the target while it lies to one side of the heading; on the spot if it's behind.
    int16_t first, second;
    Ai_TargetAngles(p.Line(p.Zone(next).out), c.pos, a.lane, first, second);
    int16_t right = (int16_t)(second - c.yaw), left = (int16_t)(first - c.yaw);
    uint8_t& mark = a.marks[a.zone];
    if (right > 0) {
        mark = 0x80;
        if (right > 0x4000) {
            if (go) c.yaw = (uint16_t)(c.yaw + 0x100);
        } else if (steer) {
            in |= kCarRight;
        }
    } else if (left < 0) {
        mark = 0x80;
        if (left < -0x4000) {
            if (go) c.yaw = (uint16_t)(c.yaw - 0x100);
        } else if (steer) {
            in |= kCarLeft;
        }
    } else {
        mark = 0x40;
        if (in == kCarAccel) in |= kCarR2;   // on target at full throttle
    }
    if (speed >= 2 && speed <= 62) mark |= (uint8_t)speed;
    return in;
}

// Ai_DrawZoneTable: 13 columns of 20 zones; a column c is drawn when it's within one of the car's zone's (+0x24A /
// 20). Each row r is zone 20c + r, at y = 8r + 64 (half-lines): a '>' (D_003344C0) at x = 80c + 24 if it's the
// car's zone; then, if its speed isn't 0, "%c %3d %3d" (D_00328050) at x = 80 (c & 7) + 32: its mark (+0x25C) as
// 'O' (0x40, clean) or 'X' (0x80, steered), else a space; the speed x 5 and the mark's speed (& 0x3F) x 5, both
// in km/h. Columns 8-12 (zones 160 on) wrap onto the left of the screen, but the game's '>' doesn't: from zone 160
// it is off the screen (x 664 on). The PC wraps it with its column (its choice), and reads no zone past 255 (the
// game's last column runs 4 bytes into the next table).
void Ai_DrawZoneTable(CarDebugPrint print, const AiCar& a, const uint8_t* speeds) {
    const int zone = a.zone & 0xFF, column = zone / 20;
    for (int c = 0; c < 13; ++c) {
        if (c < column - 1 || c > column + 1) continue;
        for (int r = 0; r < 20; ++r) {
            const int z = 20 * c + r, y = 8 * r + 0x40;
            if (z > 0xFF) break;
            if (z == zone) print(80 * (c & 7) + 0x18, y, ">");
            if (speeds[z] == 0) continue;
            const int mark = a.marks[z] & 0xC0;
            const char ch = mark == 0x40 ? 'O' : mark == 0x80 ? 'X' : ' ';
            print(80 * (c & 7) + 0x20, y, "%c %3d %3d", ch, speeds[z] * 5, (a.marks[z] & 0x3F) * 5);
        }
    }
}

// Ai_SaveZoneTable: the zones counted back from 256 to the last non-zero one (the game's loop runs off the table's
// start when all are 0; the PC writes no zones). func_00288EC0 opens "host:ai_g%02d.c" (D_002EA720, the race
// number info +0x20) with 0x602 (write, create, truncate), then a line a zone, "\t%d,\t/* zone %2d */\n"
// (D_002EA758); a failed write prints "write error!" and goes on, and "done" follows the close all the same. The PC
// adds a header comment and writes through a temporary file (util::WriteReplacing), so a failed save leaves the
// old file; its status is "write error!" if any line failed.
bool Ai_SaveZoneTable(const std::string& path, const std::string& header, const uint8_t* speeds,
                      const char** status) {
    int count = 256;
    while (count > 0 && speeds[count - 1] == 0) --count;
    bool writeError = false;
    bool ok = util::WriteReplacing(path, [&](FILE* f) {
        if (fprintf(f, "%s\n", header.c_str()) < 0) writeError = true;
        for (int i = 0; i < count; ++i)
            if (fprintf(f, GT_002EA758_20, speeds[i], i) < 0) writeError = true;
    });
    *status = ok ? "done" : writeError ? GT_002EA710_12 : GT_002EA6F8_15;
    return ok;
}

bool Ai_LoadZoneTable(const std::string& path, uint8_t* speeds) {
    FILE* f = fopen(path.c_str(), "r");
    if (!f) return false;
    uint8_t table[256] = {};
    int n = 0;
    char row[256];
    while (n < 256 && fgets(row, sizeof row, f)) {
        int v;
        if (sscanf(row, " %d", &v) == 1) table[n++] = (uint8_t)v;
    }
    fclose(f);
    memcpy(speeds, table, sizeof table);
    return true;
}

}  // namespace game
