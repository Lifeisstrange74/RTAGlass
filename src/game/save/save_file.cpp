#include "save_file.h"

#include <windows.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <vector>

#include "../../util/files.h"
#include "../../util/json.h"

namespace game {

namespace {

const char kFormat[] = "RTAGlass save: a Road Trip player record (the PS2 memory card's BASLUS-20398)";
const int kVersion = 1;

enum Type { kU8, kS8, kU16, kS16, kS32, kHex16, kHex32, kHex64, kText, kBytes };

const int kTypeSize[] = {1, 1, 2, 2, 4, 2, 4, 8, 1, 1};

// A field: `count` values of `type` at `off` (from its group's entry), as a number (a "0x..." string for the hex
// types) or a list of them; a list of rows of `row` when there are more. kText is a string of up to `count` bytes;
// kBytes is `count` bytes as hex strings of `row` bytes. `skipZero`: left out while all zero (the game's unused
// bytes). The notes go into the file's "notes".
struct Field {
    const char* name;
    int off;
    Type type;
    int count;
    int row;
    bool skipZero;
    const char* note;
};

// A section: `repeat` entries `stride` apart from `base` (a list of objects), or one (an object).
struct Group {
    const char* name;
    int base, repeat, stride;
    const Field* fields;
    int count;
    const char* note;
};

#define FIELDS(a) a, (int)(sizeof(a) / sizeof(a[0]))

const Field kCar[] = {
    {"paint", 0x00, kHex32, 1, 1, false, "RGB as six 4-bit colour indices; the top byte copies wheelColor"},
    {"body", 0x04, kU8, 1, 1, false, nullptr},
    {"wheelColor", 0x05, kU8, 1, 1, false, nullptr},
    {"wheel", 0x06, kU8, 1, 1, false, nullptr},
    {"billboard", 0x07, kU8, 1, 1, false, "the billboard's sponsor, 0-4"},
    {"partFlags", 0x08, kHex16, 1, 1, false, nullptr},
    {"unused", 0x0A, kBytes, 2, 2, true, nullptr},
    {"parts", 0x0C, kU8, 16, 16, false,
     "by category: start body, tires, engine, chassis, transmission, steering, brakes, wheels, lights, wing set, "
     "special, option, sticker, horn, meter, -"},
};
const Field kPhoto[] = {
    {"paint", 0x00, kHex32, 1, 1, false, nullptr},
    {"body", 0x04, kU8, 1, 1, false, nullptr},
    {"wheelColor", 0x05, kU8, 1, 1, false, nullptr},
    {"wheel", 0x06, kU8, 1, 1, false, nullptr},
    {"billboard", 0x07, kU8, 1, 1, false, nullptr},
    {"partFlags", 0x08, kHex16, 1, 1, false, nullptr},
    {"unused", 0x0A, kBytes, 2, 2, true, nullptr},
};
const Field kPlayer[] = {
    {"name", 0x62C, kText, 19, 19, false, nullptr},
    {"currency", 0x63F, kText, 5, 5, false, "the money's unit"},
    {"greeting", 0x646, kText, 6, 6, false, nullptr},
    {"license", 0x651, kS8, 1, 1, false, "0 none, 1 B, 2 A, 3 Super A"},
    {"money", 0x654, kS32, 1, 1, false, nullptr},
    {"bank", 0x330C, kS32, 1, 1, false, "savings"},
    {"distance", 0x658, kS32, 1, 1, false, "metres driven"},
    {"sponsorDistance", 0x65C, kS32, 5, 5, false, "metres driven with each sponsor's billboard, not yet paid"},
    {"homeInterior", 0x652, kU8, 1, 1, false, nullptr},
    {"saveTown", 0x12DC, kS8, 1, 1, false, "the town it was saved in"},
    {"townsUnvisited", 0x12DA, kHex16, 1, 1, false, "a bit per town"},
    {"myCity", 0x670, kHex32, 1, 1, false, "a bit per resident moved in"},
    {"id", 0x3438, kHex64, 1, 1, false, "for 2 Player: bytes 3-7 the name's first five characters"},
};
const Field kFlags[] = {
    {"photoTaken", 0x508, kHex64, 2, 2, false, "bit n-1: photo n (100)"},
    {"photoCursor", 0x12E2, kS16, 1, 1, false, "the album's cursor: the photo taken last, or chosen"},
    {"stampEarned", 0x518, kHex64, 2, 2, false, "bit n-1: stamp n (100)"},
    {"stampShown", 0x528, kHex64, 2, 2, false, "bit n-1: stamp n's text shown"},
    {"events", 0x538, kHex64, 2, 2, false, "the event scripts' flags (128)"},
    {"pickupsLeft", 0x548, kHex64, 2, 2, false, "the field's pickups not yet collected (100)"},
    {"beaten", 0x558, kHex64, 10, 10, false, "opponents placed behind: a word per group, a bit per index"},
};
const Field kMail[] = {
    {"count", 0x628, kS32, 1, 1, false, nullptr},
    {"box", 0x5A8, kU8, 128, 16, false, "oldest first: bits 0-6 the mail, bit 7 read"},
};
const Field kStamps[] = {
    {"queued", 0x3300, kS32, 1, 1, false, nullptr},
    {"queue", 0x3304, kU8, 8, 8, false, "stamps waiting to be shown"},
    {"cursor", 0x12E0, kS16, 1, 1, false, nullptr},
};
const Field kTeam[] = {
    {"teammate1", 0x674, kS8, 2, 2, false, "(group, index); 0 none"},
    {"teammate2", 0x676, kS8, 2, 2, false, nullptr},
    {"speeds1", 0x1300, kBytes, 0x1000, 256, false, "teammate 1's speed tables, 16 tables, a byte a zone"},
    {"speeds2", 0x2300, kBytes, 0x1000, 256, false, nullptr},
};
const Field kItems[] = {
    {"kind", 0xC30, kHex32, 80, 5, false,
     "kind 0 bodies owned, 15 flags (160 bits each); 1-14 parts owned, word n has the items with more than n"},
};
const Field kPaints[] = {
    {"body", 0xD70, kHex32, 151, 8, false, "each body's paint, by body, 8 to a row"},
};
const Field kAreas[] = {
    {"hidden", 0xBD8, kHex32, 22, 22, false, "a word per area, a bit per place not yet found"},
};
const Field kAdventure[] = {
    {"place", 0xFF0, kU8, 78, 26, false, "rows: the player, teammate 1, teammate 2, each by course; 0 = 1st, 255 none"},
    {"lap", 0x1040, kS32, 78, 26, false, "best lap (frames)"},
    {"total", 0x1178, kS32, 78, 26, false, "best total (frames)"},
    {"teammates", 0x33C0, kU8, 104, 4, false, "by course: the teammates' (group, index) when each was set"},
};
const Field kMinigames[] = {
    {"rally", 0x12B0, kS32, 1, 1, false, nullptr},
    {"figure8", 0x12B4, kS32, 5, 5, false, "steps 1-5"},
    {"skiJump", 0x12C8, kS32, 1, 1, false, "metres"},
    {"curling", 0x12CC, kS32, 1, 1, false, nullptr},
    {"golf", 0x12D0, kS8, 9, 9, false, "strokes over par by hole; 127 none"},
    {"golfTotal", 0x12D9, kS8, 1, 1, false, nullptr},
};
const Field kGrandPrix[] = {
    {"stage", 0x3310, kS32, 1, 1, false, "stages done"},
    {"points", 0x3314, kS8, 168, 24, false, "a row a stage, by car"},
};
const Field kOptions[] = {
    {"buttons", 0x12E8, kHex16, 10, 10, false,
     "pad bits: accelerate, brake, reverse, R2 action, L1, L2, horn, camera, lights, map"},
    {"radio", 0x12DD, kS8, 1, 1, false, "0 off"},
    {"radioVolume", 0x12DE, kS16, 1, 1, false, nullptr},
};
const Field kQuickRace[] = {
    {"bestLap", 0x0, kS32, 1, 1, false, nullptr},
    {"bestTotal", 0x4, kS32, 1, 1, false, nullptr},
    {"attempts", 0x8, kS16, 1, 1, false, nullptr},
    {"bestPlace", 0xA, kS16, 1, 1, false, "0 = 1st, 255 none"},
};
const Field kVersusCourse[] = {
    {"topLaps", 0x0, kHex32, 3, 3, false, "time (frames) | whose << 24: 0xFF this player, else a rival"},
    {"topTotals", 0xC, kHex32, 3, 3, false, nullptr},
    {"bestLap", 0x18, kS32, 1, 1, false, nullptr},
    {"bestTotal", 0x1C, kS32, 1, 1, false, nullptr},
};
const Field kVersusRival[] = {
    {"id", 0x0, kHex64, 1, 1, false, nullptr},
    {"name", 0x8, kText, 20, 20, false, nullptr},
    {"wins", 0x1C, kU8, 10, 10, false, "by 2 Player course"},
    {"races", 0x26, kU8, 10, 10, false, nullptr},
};
const Field kUnused[] = {
    {"0504", 0x504, kBytes, 4, 4, true, nullptr},    {"0644", 0x644, kBytes, 2, 2, true, nullptr},
    {"064C", 0x64C, kBytes, 5, 5, true, nullptr},    {"0653", 0x653, kBytes, 1, 1, true, nullptr},
    {"0FCC", 0xFCC, kBytes, 0x24, 0x24, true, nullptr}, {"103E", 0x103E, kBytes, 2, 2, true, nullptr},
    {"12E4", 0x12E4, kBytes, 4, 4, true, nullptr},   {"12FC", 0x12FC, kBytes, 4, 4, true, nullptr},
    {"33BC", 0x33BC, kBytes, 4, 4, true, nullptr},   {"3428", 0x3428, kBytes, 0x10, 0x10, true, nullptr},
    {"3440", 0x3440, kBytes, 8, 8, true, nullptr},
};

const Group kGroups[] = {
    {"player", 0, 1, 0, FIELDS(kPlayer), nullptr},
    {"cars", 0, 3, 0x1C, FIELDS(kCar), "0 the player's, 1 and 2 the teammates'"},
    {"options", 0, 1, 0, FIELDS(kOptions), nullptr},
    {"quickRace", 0xB78, 8, 0xC, FIELDS(kQuickRace), "by course card"},
    {"adventure", 0, 1, 0, FIELDS(kAdventure), nullptr},
    {"minigames", 0, 1, 0, FIELDS(kMinigames), "best results"},
    {"worldGpx", 0, 1, 0, FIELDS(kGrandPrix), nullptr},
    {"flags", 0, 1, 0, FIELDS(kFlags), nullptr},
    {"areas", 0, 1, 0, FIELDS(kAreas), nullptr},
    {"items", 0, 1, 0, FIELDS(kItems), nullptr},
    {"paints", 0, 1, 0, FIELDS(kPaints), nullptr},
    {"mail", 0, 1, 0, FIELDS(kMail), nullptr},
    {"stamps", 0, 1, 0, FIELDS(kStamps), nullptr},
    {"team", 0, 1, 0, FIELDS(kTeam), nullptr},
    {"versusCourses", 0xA38, 10, 0x20, FIELDS(kVersusCourse), "2 Player records by course row"},
    {"versusRivals", 0x678, 20, 0x30, FIELDS(kVersusRival), "2 Player: other saves met"},
    {"photos", 0x54, 100, 12, FIELDS(kPhoto), "the album: the car as each picture was taken"},
    {"unused", 0, 1, 0, FIELDS(kUnused), "bytes the game never uses, when not zero"},
};

bool AllZero(const uint8_t* p, int n) {
    for (int i = 0; i < n; ++i)
        if (p[i]) return false;
    return true;
}

int64_t GetValue(const uint8_t* p, Type t) {
    switch (t) {
    case kS8: return (int8_t)*p;
    case kS16: { int16_t v; memcpy(&v, p, 2); return v; }
    case kS32: { int32_t v; memcpy(&v, p, 4); return v; }
    case kU16: case kHex16: { uint16_t v; memcpy(&v, p, 2); return v; }
    case kHex32: { uint32_t v; memcpy(&v, p, 4); return v; }
    case kHex64: { int64_t v; memcpy(&v, p, 8); return v; }
    default: return *p;
    }
}

void PutValue(uint8_t* p, Type t, int64_t v) { memcpy(p, &v, kTypeSize[t]); }

// The number of bytes a field takes.
int FieldBytes(const Field& fd) {
    return fd.type == kText || fd.type == kBytes ? fd.count : fd.count * kTypeSize[fd.type];
}

int Rows(const Field& fd) { return (fd.count + fd.row - 1) / fd.row; }   // the last may be short

// A string is written as text when it's printable (and needs no JSON escapes) and ends in zeros; otherwise as
// "#" and its bytes in hex.
bool PlainText(const uint8_t* p, int n) {
    int len = 0;
    while (len < n && p[len]) ++len;
    if (len == n) return false;
    for (int i = 0; i < len; ++i)
        if (p[i] < 0x20 || p[i] > 0x7E || p[i] == '"' || p[i] == '\\' || p[i] == '#') return false;
    return AllZero(p + len, n - len);
}

std::string Hex(const uint8_t* p, int n) {
    std::string s;
    char b[3];
    for (int i = 0; i < n; ++i) {
        snprintf(b, sizeof(b), "%02x", p[i]);
        s += b;
    }
    return s;
}

// A field's value: a number (or "0x..." for the bit sets and colours), a list of them, or a list of rows.
void WriteValue(FILE* f, const uint8_t* p, const Field& fd, const char* indent) {
    if (fd.type == kText) {
        if (PlainText(p, fd.count)) fprintf(f, "\"%s\"", (const char*)p);
        else fprintf(f, "\"#%s\"", Hex(p, fd.count).c_str());
        return;
    }
    int rows = Rows(fd);
    if (rows > 1) fprintf(f, "[\n");
    for (int r = 0; r < rows; ++r) {
        if (rows > 1) fprintf(f, "%s    ", indent);
        if (fd.type == kBytes) {
            fprintf(f, "\"%s\"", Hex(p + r * fd.row, fd.row).c_str());
        } else {
            if (fd.row > 1) fprintf(f, "[");
            for (int i = 0; i < fd.row && r * fd.row + i < fd.count; ++i) {
                int64_t v = GetValue(p + (r * fd.row + i) * kTypeSize[fd.type], fd.type);
                if (i) fprintf(f, ", ");
                switch (fd.type) {
                case kHex16: fprintf(f, "\"0x%04X\"", (unsigned)v); break;
                case kHex32: fprintf(f, "\"0x%08X\"", (unsigned)v); break;
                case kHex64: fprintf(f, "\"0x%016llX\"", (unsigned long long)v); break;
                default: fprintf(f, "%lld", (long long)v); break;
                }
            }
            if (fd.row > 1) fprintf(f, "]");
        }
        if (rows > 1) fprintf(f, r + 1 < rows ? ",\n" : "\n");
    }
    if (rows > 1) fprintf(f, "%s]", indent);
}

// One entry of a group: an object, a field a line (`inline`: all on one line, for the list sections).
void WriteEntry(FILE* f, const uint8_t* entry, const Group& g, bool inline_, const char* indent) {
    fprintf(f, "{");
    bool first = true;
    for (int k = 0; k < g.count; ++k) {
        const Field& fd = g.fields[k];
        if (fd.skipZero && AllZero(entry + fd.off, FieldBytes(fd))) continue;
        fprintf(f, first ? "" : ",");
        if (inline_) fprintf(f, first ? "\"%s\": " : " \"%s\": ", fd.name);
        else fprintf(f, "\n%s    \"%s\": ", indent, fd.name);
        std::string deeper = std::string(indent) + "    ";
        WriteValue(f, entry + fd.off, fd, deeper.c_str());
        first = false;
    }
    if (!inline_ && !first) fprintf(f, "\n%s", indent);
    fprintf(f, "}");
}

using util::Json;
using util::JsonParser;

bool ParseHex(const char* s, uint8_t* out, int n) {
    for (int i = 0; i < n; ++i) {
        unsigned v;
        if (!s[0] || !s[1] || sscanf(s, "%2x", &v) != 1) return false;
        out[i] = (uint8_t)v;
        s += 2;
    }
    return true;
}

// A number, or a string holding one ("0x..." for the hex fields).
bool ReadNumber(const Json& v, Type t, int64_t& out) {
    if (v.kind != Json::kNumber && v.kind != Json::kString) return false;
    char* e;
    const char* s = v.text.c_str();
    out = t == kHex64 ? (int64_t)_strtoui64(s, &e, 0) : _strtoi64(s, &e, 0);
    return e != s;
}

// A row of `fd` (`count` values from element `first`) from a number or a list.
void ReadRow(uint8_t* p, const Field& fd, int first, const Json& v) {
    int n = fd.count - first < fd.row ? fd.count - first : fd.row;
    for (int i = 0; i < n; ++i) {
        const Json* item = v.kind == Json::kArray ? (i < (int)v.items.size() ? &v.items[i] : nullptr)
                                                  : (i == 0 ? &v : nullptr);
        int64_t x;
        if (item && ReadNumber(*item, fd.type, x)) PutValue(p + (first + i) * kTypeSize[fd.type], fd.type, x);
    }
}

void ReadField(uint8_t* p, const Field& fd, const Json& v) {
    if (fd.type == kText) {
        if (v.kind != Json::kString) return;
        uint8_t buf[32] = {};
        if (!v.text.empty() && v.text[0] == '#') {
            if (!ParseHex(v.text.c_str() + 1, buf, fd.count)) return;
        } else {
            memcpy(buf, v.text.c_str(), v.text.size() < (size_t)fd.count - 1 ? v.text.size() : fd.count - 1);
        }
        memcpy(p, buf, fd.count);
        return;
    }
    int rows = Rows(fd);
    for (int r = 0; r < rows; ++r) {
        const Json* row = rows == 1 ? &v : (v.kind == Json::kArray && r < (int)v.items.size() ? &v.items[r] : nullptr);
        if (!row) break;
        if (fd.type == kBytes) {
            if (row->kind == Json::kString) ParseHex(row->text.c_str(), p + r * fd.row, fd.row);
        } else {
            ReadRow(p, fd, r * fd.row, *row);
        }
    }
}

void ReadEntry(uint8_t* entry, const Group& g, const Json& obj) {
    if (obj.kind != Json::kObject) return;
    for (int k = 0; k < g.count; ++k)
        if (const Json* v = obj.Get(g.fields[k].name)) ReadField(entry + g.fields[k].off, g.fields[k], *v);
}

}  // namespace

bool Save_WriteFile(const std::string& path, const SaveRecord& r) {
    const uint8_t* rec = (const uint8_t*)&r;
    return util::WriteReplacing(path, [&](FILE* f) {
        fprintf(f, "{\n    \"format\": \"%s\",\n    \"version\": %d,\n", kFormat, kVersion);
        // What the less obvious fields hold (the reader skips this).
        fprintf(f, "    \"notes\": {\n        \"times\": \"frames, 60 a second; 2147483647 is none\"");
        for (const Group& g : kGroups) {
            if (g.note) fprintf(f, ",\n        \"%s\": \"%s\"", g.name, g.note);
            for (int k = 0; k < g.count; ++k)
                if (g.fields[k].note)
                    fprintf(f, ",\n        \"%s.%s\": \"%s\"", g.name, g.fields[k].name, g.fields[k].note);
        }
        fprintf(f, "\n    }");
        for (const Group& g : kGroups) {
            fprintf(f, ",\n    \"%s\": ", g.name);
            if (g.repeat == 1) {
                WriteEntry(f, rec + g.base, g, false, "    ");
                continue;
            }
            fprintf(f, "[\n");
            for (int i = 0; i < g.repeat; ++i) {
                fprintf(f, "        ");
                WriteEntry(f, rec + g.base + i * g.stride, g, true, "        ");
                fprintf(f, i + 1 < g.repeat ? ",\n" : "\n");
            }
            fprintf(f, "    ]");
        }
        fprintf(f, "\n}\n");
    });
}

bool Save_ReadFile(const std::string& path, SaveRecord& r) {
    std::vector<uint8_t> data;
    if (!util::ReadAll(path, data)) return false;
    size_t i = 0;
    while (i < data.size() && (data[i] == ' ' || data[i] == '\t' || data[i] == '\r' || data[i] == '\n')) ++i;
    if (i == data.size() || data[i] != '{') {
        // A PS2 card's file: the record as it was in memory.
        if (data.size() != sizeof(SaveRecord)) return false;
        memcpy(&r, &data[0], sizeof(r));
        return true;
    }
    JsonParser parser = {(const char*)&data[0], (const char*)&data[0] + data.size()};
    Json root = parser.Value(0);
    const Json* format = root.Get("format");
    if (!parser.ok || !format || format->text != kFormat) return false;
    Save_Init(r);
    uint8_t* rec = (uint8_t*)&r;
    for (const Group& g : kGroups) {
        const Json* v = root.Get(g.name);
        if (!v) continue;
        if (g.repeat == 1) {
            ReadEntry(rec + g.base, g, *v);
        } else if (v->kind == Json::kArray) {
            for (int k = 0; k < g.repeat && k < (int)v->items.size(); ++k)
                ReadEntry(rec + g.base + k * g.stride, g, v->items[k]);
        }
    }
    return true;
}

bool Save_WriteRaw(const std::string& path, const SaveRecord& r) {
    return util::WriteReplacing(path, [&](FILE* f) { fwrite(&r, sizeof(r), 1, f); });
}

std::string Save_SlotPath(int slot) {
    char name[32];
    snprintf(name, sizeof(name), "\\save%d.json", slot);
    return util::ExeDir() + name;
}

bool Save_SlotExists(int slot) {
    return GetFileAttributesA(Save_SlotPath(slot).c_str()) != INVALID_FILE_ATTRIBUTES;
}

int Save_CheckFields() {
    std::vector<int> cover(sizeof(SaveRecord), 0);
    for (const Group& g : kGroups)
        for (int i = 0; i < g.repeat; ++i)
            for (int k = 0; k < g.count; ++k) {
                int at = g.base + i * g.stride + g.fields[k].off;
                for (int b = 0; b < FieldBytes(g.fields[k]); ++b) ++cover[at + b];
            }
    for (size_t i = 0; i < cover.size(); ++i)
        if (cover[i] != 1) return (int)i;
    return -1;
}

}  // namespace game
