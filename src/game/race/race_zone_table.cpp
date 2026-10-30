// The AI zone table editor, the debug build's pause item 6 (the Pause Items extra): the speed table the player's car
// drives by, a zone per row, with the marks the last pass left, the player's car as the cursor. With item 5 (the AI
// drives the player's car) it's the tool the courses' tables were tuned with: let the AI drive, see where it had to
// steer (X), change the speed, save. See docs/debug.md and docs/ai.md ("Speeds", "Marks").
#include <stdio.h>
#include <string.h>

#include "../debug_text.h"
#include "course.h"
#include "race.h"

namespace game {

namespace {

const uint32_t kZoneTableOverlay = 0x80006666;   // Fade_SetOverlay's colour: (0x66, 0x66, 0) opaque
const int kStatusFrames = 120;                    // the PC's: how long a save's result stays up

}  // namespace

// Every car on the course shares one table in the game (Ai_Init 0x0024FCD8 points +0x258 at D_002A9B80's, the
// player's car too) but the teammates (their own, in the save). The port's cars point at the path's speeds (null)
// or at a constant table (an Adventure race's, adventure_speeds.inc), so the editor first makes the player's the
// race's own and points every car that shared it at that copy. The game edits its data for the rest of the session;
// the PC's copy lasts for the race, and a saved table comes back with the next race (AiPath::Load,
// Ai_LoadRaceZoneTable).
uint8_t* Race::Ai_EditZoneTable() {
    uint8_t* const shared = ai_[0].speeds;
    if (shared == zoneSpeeds_) return zoneSpeeds_;
    if (shared) {
        memcpy(zoneSpeeds_, shared, sizeof zoneSpeeds_);
    } else {
        memset(zoneSpeeds_, 0, sizeof zoneSpeeds_);
        for (int z = 0; z < path_.Zones() && z < 256; ++z) zoneSpeeds_[z] = path_.Zone(z).speed;
    }
    for (int i = 0; i < carCount_; ++i)
        if (ai_[i].speeds == shared) ai_[i].speeds = zoneSpeeds_;
    // Where Select saves it (the PC's): the game wrote host:ai_g%02d.c by the race number (info +0x20) as C source to
    // build in. The PC keeps the table beside the path it belongs to: an Adventure race's own table as Gnn (nn the
    // race, as the game's name), else the path's (Cnn, or Ann for an event) as Cnn.speeds.txt.
    char name[16];
    if (adventure_ && shared) snprintf(name, sizeof name, "G%02d", adventureRace_);
    else snprintf(name, sizeof name, "%s", Course::File(id_));
    zoneFile_ = dataDir_ + "\\ai\\" + name + ".speeds.txt";
    return zoneSpeeds_;
}

void Race::Ai_LoadRaceZoneTable() {
    if (!ai_[0].speeds) return;   // the path's speeds: AiPath::Load reads their file
    char name[16];
    snprintf(name, sizeof name, "G%02d", adventureRace_);
    uint8_t table[256];
    if (Ai_LoadZoneTable(dataDir_ + "\\ai\\" + name + ".speeds.txt", table))
        memcpy(Ai_EditZoneTable(), table, sizeof table);
}

// Ai_ZoneTableTask (func_0026D020), a frame: Fade_SetOverlay(0x80006666) in place of the pause's half black, then
// Ai_DrawZoneTable for the player's car (D_0177AC50); pad 1's pressed (+0x30) Up adds 1 to its zone's (+0x24A) entry
// in its table (+0x258), Down takes 1 off (a byte: 0 goes to 255, which Ai_Drive reads & 0x7F); then Cross or
// Triangle (0x50) give the task back to Race_PauseTask (the window shows again the next frame), else Select (0x100)
// saves the table: Ai_SaveZoneTable(info +0x20, table). On the PC, Select is the Navigator action. The game printed
// "AI Save %s" and the result on the development PC's console; the PC shows them over the table for a while.
bool Race::Ai_ZoneTableTask(const platform::Input& input) {
    uint8_t* table = Ai_EditZoneTable();
    const size_t slash = zoneFile_.find_last_of('\\');
    const std::string file = zoneFile_.substr(slash == std::string::npos ? 0 : slash + 1);
    d_.Fill(0, 0, gfx::Draw2D::kWidth, gfx::Draw2D::kHeight, kZoneTableOverlay);
    Ai_DrawZoneTable(DebugText_Printf, ai_[0], table);
    if (zoneStatusFrames_ > 0) {
        --zoneStatusFrames_;
        DebugText_Printf(32, 40, "AI SAVE %s", file.c_str());
        DebugText_Printf(32, 48, "%s", zoneStatus_);
    }
    DebugText_Draw(d_);
    const uint32_t pressed = input.Pressed(0);
    uint8_t& speed = table[ai_[0].zone & 0xFF];
    if (pressed & platform::kUp) ++speed;
    if (pressed & platform::kDown) --speed;
    if (pressed & (platform::kCross | platform::kTriangle)) {
        zoneStatusFrames_ = 0;
        return false;
    }
    if (pressed & platform::kNavigator) {
        std::string header = "/* " + file + ": the AI's speed table on " + Course::File(id_) +
                             ", saved by the zone table editor: a line a zone, 5 km/h a step, 0 none. "
                             "Delete it for the original table. See docs/ai.md. */";
        Ai_SaveZoneTable(zoneFile_, header, table, &zoneStatus_);
        zoneStatusFrames_ = kStatusFrames;
    }
    return true;
}

}  // namespace game
