#include "extras.h"

#include <stdio.h>
#include <string.h>

#include <vector>

#include "../util/files.h"
#include "../util/json.h"

namespace game {

namespace {

const char kFormat[] = "RTAGlass extras";
const int kVersion = 1;

const char* const kNames[kExtraCount] = {
    "lifeMeter", "qWave", "mail5", "testCourse", "body149", "demoSong", "duckQuiz", "language",
    "pauseItems", "readouts", "freeCamera", "heightKeys", "clockScrub", "testMenu", "mapTuner",
    "cheats", "uncappedSpeed", "infiniteFuel", "powerBoost", "superGrip", "reduceDrag", "lowGravity", "slowRivals", "maxMoney",
    "allParts",
};

ExtrasSet g_extras;
std::string g_path;

}  // namespace

bool Extras_On(Extra e) {
    return e >= 0 && e < kExtraCount && g_extras.on[e] && (e < kExtraFirstCheat || g_extras.on[kExtraCheats]);
}
bool Cheats_Enabled() { return g_extras.on[kExtraCheats]; }
const ExtrasSet& Extras_Get() { return g_extras; }

bool Extras_Apply(const ExtrasSet& s) {
    g_extras = s;
    if (g_path.empty()) return true;
    return util::WriteReplacing(g_path, [&](FILE* f) {
        fprintf(f, "{\n    \"format\": \"%s\",\n    \"version\": %d,\n", kFormat, kVersion);
        fprintf(f, "    \"notes\": \"true turns on content the original game has but never reaches, its debug "
                   "features, or (with cheats true) the PC's cheats; false (or missing) is the original game\"");
        for (int e = 0; e < kExtraCount; ++e) fprintf(f, ",\n    \"%s\": %s", kNames[e], s.on[e] ? "true" : "false");
        fprintf(f, "\n}\n");
    });
}

void Extras_Load(const std::string& path) {
    g_path = path;
    memset(&g_extras, 0, sizeof g_extras);
    std::vector<uint8_t> data;
    if (path.empty() || !util::ReadAll(path, data) || data.empty()) return;
    util::JsonParser parser = {(const char*)&data[0], (const char*)&data[0] + data.size()};
    util::Json root = parser.Value(0);
    const util::Json* format = root.Get("format");
    if (!parser.ok || !format || format->text != kFormat) return;
    for (int e = 0; e < kExtraCount; ++e) {
        const util::Json* v = root.Get(kNames[e]);
        g_extras.on[e] = v && v->text == "true";
    }
}

void Extras_Force(Extra e) {
    if (e >= 0 && e < kExtraCount) g_extras.on[e] = true;
}

const char* Extra_Name(Extra e) { return e >= 0 && e < kExtraCount ? kNames[e] : ""; }

int Extra_FromName(const std::string& name) {
    for (int e = 0; e < kExtraCount; ++e)
        if (name == kNames[e]) return e;
    return -1;
}

}  // namespace game
