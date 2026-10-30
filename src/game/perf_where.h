// Where the player is, for --perf's log (main.cpp): the field and the race fill it in each frame they run, and the
// log clears it before each frame, so other screens leave it empty.
#pragma once

#include <stdint.h>

namespace game {

struct PerfWhere {
    // "field": pos and yaw are --place's arguments after --field `id` (from that town's Q's Factory block's corner);
    // "race": course `id`'s own coordinates. Empty: neither.
    const char* mode;
    int id;
    float pos[3];
    int yaw;
    int32_t clock;   // the time of day, as --clock takes it
};

inline PerfWhere& Perf_Where() {
    static PerfWhere where = {};
    return where;
}

}  // namespace game
