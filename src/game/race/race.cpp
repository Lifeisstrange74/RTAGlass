#include "race.h"

#include <math.h>
#include <stdio.h>

#include <vector>

#include "../../audio/sound.h"
#include "../debug_text.h"
#include "../extras.h"
#include "../pad/vibration.h"
#include "../perf_where.h"
#include "../save/save_data.h"
#include "../story/race_list.h"

namespace game {

namespace {

const float kPi = 3.14159265f;

// The line, by course id (D_002A8198): three strips side by side across the track, in the order a car drives
// through them, each {x0, z0, x1, z1}.
const float kLapZones[Course::kCourses][3][4] = {
    {{588, 535, 590, 576.8f}, {590, 535, 592, 576.8f}, {592, 535, 594, 576.8f}},
    {{588, 535, 590, 576.8f}, {590, 535, 592, 576.8f}, {592, 535, 594, 576.8f}},
    {{716, 149, 734, 151}, {716, 147, 734, 149}, {716, 145, 734, 147}},
    {{111, 318, 139.5f, 320}, {111, 320, 139.5f, 322}, {111, 322, 139.5f, 324}},
    {{420, 230, 422, 260}, {418, 230, 420, 260}, {416, 230, 418, 260}},
    {{566.5f, 395.8f, 603.6f, 397.8f}, {566.5f, 397.8f, 603.6f, 399.8f}, {566.5f, 399.8f, 603.6f, 401.8f}},
    {{725, 540.9f, 727, 680.5f}, {723, 540.9f, 725, 680.5f}, {721, 540.9f, 723, 680.5f}},
    {{599.9f, 259.7f, 601.9f, 296.3f}, {597.9f, 259.7f, 599.9f, 296.3f}, {595.9f, 259.7f, 597.9f, 296.3f}},
    {{799.3f, 905, 801.3f, 935}, {801.3f, 905, 803.3f, 935}, {803.3f, 905, 805.3f, 935}},
    {{799.3f, 905, 801.3f, 935}, {801.3f, 905, 803.3f, 935}, {803.3f, 905, 805.3f, 935}},
    {{435, 444, 475.5f, 446}, {435, 442, 475.5f, 444}, {435, 440, 475.5f, 442}},
    {{725, 540.9f, 727, 680.5f}, {723, 540.9f, 725, 680.5f}, {721, 540.9f, 723, 680.5f}},
    {{548, 330, 550, 350}, {550, 330, 552, 350}, {552, 330, 554, 350}},
    {{862.8f, 640, 864.8f, 740}, {860.8f, 640, 862.8f, 740}, {858.8f, 640, 860.8f, 740}},
    {{843, 498, 845, 549.5f}, {841, 498, 843, 549.5f}, {839, 498, 841, 549.5f}},
    {{726.8f, 1170, 728.8f, 1210}, {728.8f, 1170, 730.8f, 1210}, {730.8f, 1170, 732.8f, 1210}},
    {{726.8f, 1170, 728.8f, 1210}, {728.8f, 1170, 730.8f, 1210}, {730.8f, 1170, 732.8f, 1210}},
    {{791, 1065, 793, 1115}, {793, 1065, 795, 1115}, {795, 1065, 797, 1115}},
    {{791, 1065, 793, 1115}, {793, 1065, 795, 1115}, {795, 1065, 797, 1115}},   // 0x12-0x26: the events
    {{791, 1065, 793, 1115}, {793, 1065, 795, 1115}, {795, 1065, 797, 1115}},
    {{588, 868, 590, 878}, {408, 868, 410, 878}, {406, 868, 408, 878}},
    {{791, 1065, 793, 1115}, {793, 1065, 795, 1115}, {795, 1065, 797, 1115}},
    {{410, 388, 430, 390}, {410, 390, 430, 392}, {410, 392, 430, 394}},
    {{791, 1065, 793, 1115}, {793, 1065, 795, 1115}, {795, 1065, 797, 1115}},
    {{791, 1065, 793, 1115}, {793, 1065, 795, 1115}, {795, 1065, 797, 1115}},
    {{791, 1065, 793, 1115}, {793, 1065, 795, 1115}, {795, 1065, 797, 1115}},
    {{791, 1065, 793, 1115}, {793, 1065, 795, 1115}, {795, 1065, 797, 1115}},
    {{791, 1065, 793, 1115}, {793, 1065, 795, 1115}, {795, 1065, 797, 1115}},
    {{523.8f, 694.5f, 556, 697.5f}, {523.8f, 697.5f, 556, 700.5f}, {523.8f, 700.5f, 556, 703.5f}},
    {{1416, 279, 1477, 281}, {98, 149, 229, 151}, {98, 151, 229, 153}},
    {{523.8f, 692.5f, 556, 694.5f}, {523.8f, 694.5f, 556, 696.5f}, {523.8f, 696.5f, 556, 698.5f}},
    {{523.8f, 692.5f, 556, 694.5f}, {523.8f, 694.5f, 556, 696.5f}, {523.8f, 696.5f, 556, 698.5f}},
    {{523.8f, 692.5f, 556, 694.5f}, {523.8f, 694.5f, 556, 696.5f}, {523.8f, 696.5f, 556, 698.5f}},
    {{523.8f, 692.5f, 556, 694.5f}, {523.8f, 694.5f, 556, 696.5f}, {523.8f, 696.5f, 556, 698.5f}},
    {{523.8f, 692.5f, 556, 694.5f}, {523.8f, 694.5f, 556, 696.5f}, {523.8f, 696.5f, 556, 698.5f}},
    {{523.8f, 692.5f, 556, 694.5f}, {523.8f, 694.5f, 556, 696.5f}, {523.8f, 696.5f, 556, 698.5f}},
    {{523.8f, 692.5f, 556, 694.5f}, {523.8f, 694.5f, 556, 696.5f}, {523.8f, 696.5f, 556, 698.5f}},
    {{351.5f, 760, 353, 783}, {349.5f, 760, 351.5f, 783}, {347.5f, 760, 349.5f, 783}},
    {{816, 690, 818, 706}, {814, 690, 816, 706}, {812, 690, 814, 706}},
    // (PC) 0x27, the Test Course: across the road (z 821-851) at the start box's far edge, x 410, driving -x.
    {{408, 815, 410, 857}, {406, 815, 408, 857}, {404, 815, 406, 857}},
};

// Every Quick Race is three laps (D_002BE158 +2).
const int kQuickRaceLaps = 3;

// Lap chimes: a lap, and a lap that beat the course's best.
const uint32_t kSeLap = 0x2E, kSeBestLap = 0x2F;
// A player's car running into water (Car_Ground), and bumping another car (func_00217830).
const uint32_t kSeSplash = 0x28, kSeBump = 0x14;
// A wall in the Tunnel Race (course 0x1C, Car_Crash): ACTION.TSQ's entry 0x11, restarted each time.
const uint32_t kSeCrash = 0x311, kSeCrashStop = 0x8311;
const int kTunnelCourse = 0x1C;
const int kGateCourse = 0x22;   // A16, whose gate (A16_GateTask) holds the player's car as it opens

const int kNoTime = 0x7FFFFFFF;

// Race_CountdownTask (0x0022DAE8), in frames from the race's first: the screen fades in from black over the first
// 0x40; the lights show all red, then one goes out a second, and the green ones light at kCountdownGo, which starts
// the clock and lets the cars go. The countdown's beeps (CQ_MAIN entry 0x2D) start with the first lamp going out, the
// course's song a second after green, and the lights go a second after that.
const int kCountdownFade = 0x40;
const int kCountdownBeeps = 60, kCountdownGo = 240, kCountdownSong = 300, kCountdownEnd = 360;
const uint32_t kSeCountdown = 0x2D;

// The finish (Race_CompleteLap, Race_Task): a car that finishes is driven by the AI held to 100 km/h (cap 0x28);
// FINAL LAP blinks (8 frames on, 8 off) for 0x80 frames; WRONG WAY blinks once the player has faced the wrong way
// for 0x3D frames. From the player's finish the race fades to black over 0x80 frames.
const int kFinishCap = 0x28;
const int kFinalLapBlink = 0x80;
const int kWrongWayFrames = 0x3D;
const int kFadeOut = 0x80;
// Race_CheckFall: on courses 0x0F and 0x10 (Tin Raceway, high over a floor at -50) a car below this height has
// fallen off. Race_FallOutTask fades out over 64 frames (Fade_SetOverlay h << 25).
const float kFallHeight = 110.0f;
const int kFallOutFrames = 64;
const int kSingleLapCourse = 0x26;   // the Single Lap Race (ACTION/A20), Cloud Hill's Travis
// Race_SetupTask: the replay follows a finish in the first six, with REPLAY.TSQ's song (file 0x18).
const int kReplayPlaces = 6;
const uint32_t kReplaySong = 0x0A180201;

#include "adventure_speeds.inc"
const int kAdventureSpeedRaces = 35;

// Each race course's song (D_0029F5B0, for Sound_LoadSong): BGM_01-BGM_10 (files 0x0C-0x15) into slot 2, entry 1.
const uint32_t kCourseSong[0x12] = {
    0x0A0C0201, 0x0A0C0201, 0x0A130201, 0x0A130201, 0x0A100201, 0x0A100201, 0x0A120201, 0x0A140201, 0x0A110201,
    0x0A110201, 0x0A110201, 0x0A120201, 0x0A140201, 0x0A0F0201, 0x0A0F0201, 0x0A150201, 0x0A150201, 0x0A0E0201,
};

// Race_Task's first frame: the race courses' songs by the table; Tunnel Race (0x1C) has BGM_11 (file 0x16), the other
// event courses ROOM_1 (file 0x0D). (The Adventure's row 0x1A, Endurance Run against one car, has BGM_12, 0x17.)
// (PC) The Test Course, which has no song, borrows Peach Raceway's, BGM_01.
uint32_t Race_CourseSong(int id) {
    if (id == Course::kTestCourse) return kCourseSong[0];
    if (id >= 0 && id < 0x12) return kCourseSong[id];
    return id == 0x1C ? 0x0A160201 : 0x0A0D0201;
}

bool InZone(const float* z, const float* pos) { return z[0] < pos[0] && pos[0] < z[2] && z[1] < pos[2] && pos[2] < z[3]; }

// The game's projection: x = 320 + 500 x/z, y = 224 - 470 y/z on the 448-line frame (0.47 on its 224-line field).
const float kFocal = 500.0f;

FILE* g_trace = nullptr;
int g_traceCar = 0;
int g_autoDrive = -1;   // --autodrive: the player's cap, -1 off
bool g_rankingItem = false;   // --ranking
int32_t g_clock = -1;          // --clock
bool g_place = false;           // --place
int g_gate = -1;                // --gate
float g_placePos[3];
int g_placeYaw;
bool g_eye = false;             // --eye
float g_eyePos[3];
int g_eyeYaw, g_eyePitch;

// The pause (Race_SetupTask, Race_PauseTask): its sound, and the black over the race while it's up (Fade_SetOverlay
// 0x40000000: half brightness, the HUD included; only the menu is drawn over it).
const uint32_t kSePause = 0x1F;
const uint32_t kPauseOverlay = 0x40000000;
// Ranking is in the menu only in the Adventure (pad +0x0A = 0), on courses below 0x12.
const int kRankingCourses = 0x12;

}  // namespace

void Race::SetAutoDrive(int cap) { g_autoDrive = cap; }

// The AI drives car i rather than its pad: --autodrive, or the debug pause item 5 (car flag 0x80).
bool Race::AiDriven(int i) const { return g_autoDrive >= 0 || aiDriven_[i]; }
void Race::SetRankingItem(bool on) { g_rankingItem = on; }
void Race::SetClock(int32_t clock) { g_clock = clock; }
void Race::SetPlace(float x, float y, float z, int yaw) {
    g_place = true;
    g_placePos[0] = x;
    g_placePos[1] = y;
    g_placePos[2] = z;
    g_placeYaw = yaw;
}

bool Race::Placed(float* pos, uint16_t* yaw) {
    if (!g_place) return false;
    for (int k = 0; k < 3; ++k) pos[k] = g_placePos[k];
    *yaw = (uint16_t)g_placeYaw;
    return true;
}

void Race::SetGate(int gate) { g_gate = gate; }

void Race::SetEye(float x, float y, float z, int yaw, int pitch) {
    g_eye = true;
    g_eyePos[0] = x;
    g_eyePos[1] = y;
    g_eyePos[2] = z;
    g_eyeYaw = yaw;
    g_eyePitch = pitch;
}

void Race::SetTrace(const std::string& path, int car) {
    if (g_trace) fclose(g_trace);
    g_trace = fopen(path.c_str(), "w");
    g_traceCar = car;
    if (g_trace)
        fprintf(g_trace, "frame,input,px,py,pz,vx,vy,vz,yaw,steer,turn,wheel,rpm,gear,dc,e0,e4,c0,c4,c8,slide,surface,"
                         "zone,rank,dist,water,level,effect,squeal\n");
}

namespace {

// The key setup's entries (D_01780A48 + 0x3448 x player): accelerate, brake, reverse, R2's action, L1's, L2's, the
// horn, the camera, the lights, the map.
enum CarKey { kKeyAccel, kKeyBrake, kKeyReverse, kKeyR2, kKeyL1, kKeyL2, kKeyHorn, kKeyCamera, kKeyLights,
              kKeyMap };

// The PS2 reads each entry's pad button from the player's record (save +0x12E8). The PC binds the actions to keys
// and controller buttons itself (platform/controls.h), so an entry is its action's button; the record's key setup
// is kept only for the memory card.
uint32_t Car_Key(int, CarKey key) {
    static const uint32_t kActions[10] = {
        platform::kGas,      platform::kBrake, platform::kReverse, platform::kJet,  platform::kWingUp,
        platform::kWingDown, platform::kHorn,  platform::kView,    platform::kHorn, platform::kNavigator,
    };
    return kActions[key];
}

}  // namespace

uint32_t Car_ReadPad(const platform::Input& input, int pad) {
    uint32_t held = input.Held(pad), pressed = input.Pressed(pad), bits = 0;
    if (held & Car_Key(pad, kKeyAccel)) bits |= kCarAccel;
    if (held & Car_Key(pad, kKeyBrake)) bits |= kCarBrake;
    if (held & Car_Key(pad, kKeyReverse)) bits |= kCarAccel | kCarReverse;
    if (held & Car_Key(pad, kKeyR2)) bits |= kCarR2;
    if (held & Car_Key(pad, kKeyLights)) bits |= kCarHeadlights;
    if (pressed & Car_Key(pad, kKeyL1)) bits |= kCarL1;
    if (pressed & Car_Key(pad, kKeyL2)) bits |= kCarL2;
    // The steering: on the PS2 the d-pad's left and right always (the pad word's 0xA000), as the stick does; on the
    // PC its own keys and buttons (the d-pad's by default), and the left stick.
    if (held & platform::kSteerLeft) bits |= kCarLeft;
    if (held & platform::kSteerRight) bits |= kCarRight;
    return bits;
}

bool Car_HornHeld(const platform::Input& input, int pad) { return (input.Held(pad) & Car_Key(pad, kKeyHorn)) != 0; }

uint32_t Car_CameraButton(int pad) { return Car_Key(pad, kKeyCamera); }
uint32_t Car_MapButton(int pad) { return Car_Key(pad, kKeyMap); }

Race::Race(gfx::Draw2D& draw, const std::string& dataDir)
    : d_(draw), dataDir_(dataDir), id_(-1), players_(1), laps_(kQuickRaceLaps), carCount_(0), side_(),
      teammate_(), versusCars_(), adventure_(false), bodies_(), parts_(), horn_(), lap_(), playerLaps_(), bestLap_(), lights_(),
      go_(false), clock_(0), finished_(0), frame_(0), light_(), dayClock_(0), phase_(kRacing), fade_(0), finish_(),
      finalLap_(0), result_(), replayStart_(), replayPending_(false), recording_(false), replayPos_(0) {}

void Race::Start(int id) {
    demo_ = solo_ = false;
    adventureRace_ = -1;
    // The dev path (--race) can pick a course outside the Quick Race; it borrows Peach Raceway's field.
    carCount_ = Race_CreateCars(id, cars_);
    if (!carCount_) carCount_ = Race_CreateCars(0, cars_);
    // (PC) The Test Course is a time trial: Peach Raceway's player car alone, in the first slot.
    if (id == Course::kTestCourse) {
        carCount_ = 1;
        cars_[0].slot = 0;
    }
    players_ = 1;
    laps_ = kQuickRaceLaps;
    adventure_ = false;
    for (int i = 0; i < carCount_; ++i) {
        side_[i] = 0;
        teammate_[i] = false;
    }
    int card = QuickRace_Card(id);
    // Race_InitTimes (0x0022D288): the lap to beat for the brighter chime is the card's record in the save.
    bestLap_[0] = card >= 0 ? Save_Current().quickRace[card].bestLap : kNoTime;
    if (card < 0) card = 0;
    for (int i = 0; i < carCount_; ++i) {
        const uint8_t* record = i == 0 ? QuickRace_PlayerParts(card) : QuickRace_OpponentParts(card);
        parts_[i] = Car_SetParts(record);
        // Car_Init: the part flags are the parts record's (+0xE; for the opponents their course table's +0x12).
        parts_[i].flags = Car_PartFlags(record);
        cars_[i].partFlags = parts_[i].flags;
        // Race_CreateCars gives the Quick Race's opponents +0x194 = 3, 4, 5, ... in turn: their lanes cycle.
        Ai_Init(ai_[i], i == 0 ? 0 : i + 2, record[0], cars_[i].slot, i == 0);
        // Car_Init starts a player's engine; its horn is the parts' sound (+0xD) in D_002F00AC (+0x245).
        if (i == 0) sounds_[0].Start(0x3C + 2 * record[0xD]);
    }
    Race_Setup(id);
}

void Race::StartVersus(int id, int row, int cars, int laps, int mode) {
    demo_ = solo_ = false;
    adventureRace_ = -1;
    // Race_Setup: teams when both players have a second teammate.
    bool teams = Save_Record(0).teammates[1][0] && Save_Record(1).teammates[1][0];
    carCount_ = Race_CreateVersusCars(teams, cars, cars_, versusCars_);
    players_ = 2;
    laps_ = laps;
    adventure_ = false;
    for (int i = 0; i < carCount_; ++i) {
        int entry = cars_[i].entry, member = entry % 3;
        SaveRecord& r = Save_Record(entry / 3);
        side_[i] = cars_[i].side;
        teammate_[i] = member != 0;
        // Car_Init: Race Right-Away and Random Race give every car the row's parts (D_002BE278); Custom Race each
        // car its own.
        const uint8_t* record = mode == 6 ? r.cars[member].parts : Versus_RowParts(row);
        parts_[i] = Car_SetParts(record, teammate_[i]);
        parts_[i].flags = mode == 6 ? r.cars[member].partFlags : Car_PartFlags(record);
        cars_[i].partFlags = parts_[i].flags;
        // Players' and teammates' cars have no cap (car flags 0x33); the lane is the entry (+0x194). A teammate
        // drives by its own speed table in its player's save.
        Ai_Init(ai_[i], entry, record[0], cars_[i].slot, true);
        if (teammate_[i]) ai_[i].speeds = Ai_SpeedTable(&r.teammateSpeeds[member - 1], id);
        // Car_Init starts each player's engine (engine flags & 1); the horn is the parts' sound + the player.
        if (i < players_) sounds_[i].Start(0x3C + 2 * record[0xD] + i, i);
    }
    // Race_InitTimes in 2 Player: D_00335700 = 0, so no lap is ever the brighter chime's or a highlighted row.
    for (int p = 0; p < players_; ++p) bestLap_[p] = 0;
    Race_Setup(id);
}

void Race::StartAdventure(int race, bool solo) {
    demo_ = false;
    solo_ = solo;
    adventureRace_ = race;
    const AdventureRace& row = AdventureRace_Get(race);
    int lanes[Cars::kMaxCars];
    carCount_ = Race_CreateAdventureCars(race, cars_, versusCars_, lanes);
    if (solo) {
        // Race_CreateCars (0x0020ED30) without a setup or teammates: the player's car alone, in slot 0.
        carCount_ = 1;
        cars_[0].slot = 0;
    }
    players_ = 1;
    laps_ = row.laps;
    adventure_ = true;
    const SaveRecord& r = Save_Current();
    for (int i = 0; i < carCount_; ++i) {
        int member = cars_[i].entry;
        side_[i] = 0;
        teammate_[i] = member > 0;
        // Car_Init: the player's and the teammates' cars race with their own parts (the save car's +0xC) and part
        // flags (+8); every opponent with the row's parts record (setup + 4, its flags setup + 0x12).
        // (The Tunnel Race's player has Akiban's car: the row's parts.)
        bool own = member >= 0 && row.course != kTunnelCourse;
        const uint8_t* record = own ? r.cars[member].parts : row.parts;
        parts_[i] = Car_SetParts(record, teammate_[i]);
        parts_[i].flags = own ? r.cars[member].partFlags : Car_PartFlags(record);
        cars_[i].partFlags = parts_[i].flags;
        // The player's and teammates' cars have no cap (car flags 0x33); a teammate drives by its own speed table.
        Ai_Init(ai_[i], lanes[i], record[0], cars_[i].slot, member >= 0);
        // Ai_Init in the Adventure (0x0024FE78): a teammate drives by its own table in the save, every other car by
        // its race's (D_002A9B80 + race x 0x100; never learned into, as Ai_Drive learns only for a teammate).
        if (teammate_[i]) ai_[i].speeds = Ai_SpeedTable(&Save_Record(0).teammateSpeeds[member - 1], row.course);
        else if (race < kAdventureSpeedRaces) ai_[i].speeds = const_cast<uint8_t*>(kAdventureSpeeds[race]);
        if (i == 0) sounds_[0].Start(0x3C + 2 * record[0xD]);
    }
    Ai_LoadRaceZoneTable();   // the PC's: the race's table as the zone table editor saved it, if it did
    // Race_InitTimes: the lap to beat is the race's Adventure record (races 26 and 27 share 24's and 25's).
    bestLap_[0] = race < kTownRaceRows ? r.adventureLap[0][race < 0x1A ? race : race - 2] : kNoTime;
    Race_Setup(row.course);
}

void Race::StartDemo() {
    const int kDemoRace = 24;
    const AdventureRace& row = AdventureRace_Get(kDemoRace);
    int lanes[Cars::kMaxCars];
    demo_ = true;
    solo_ = false;
    adventureRace_ = -1;   // (mode 8: Car_Input's pit tests info+0x20, 24, but no car reaches the line's last lap)
    carCount_ = Race_CreateDemoCars(kDemoRace, cars_, lanes);
    players_ = 0;
    laps_ = row.laps;
    adventure_ = false;
    for (int i = 0; i < carCount_; ++i) {
        side_[i] = 0;
        teammate_[i] = false;
        // Car_Init: a computer car on a course: the row's parts record (setup + 4) and its flags.
        parts_[i] = Car_SetParts(row.parts);
        parts_[i].flags = Car_PartFlags(row.parts);
        cars_[i].partFlags = parts_[i].flags;
        Ai_Init(ai_[i], lanes[i], row.parts[0], cars_[i].slot, false);
    }
    bestLap_[0] = kNoTime;
    Race_Setup(row.course);
}

bool Race::Race_PitStop(int i, int next, uint32_t* bits, bool replay) {
    const int kEnduranceRun = 24, kPitLane = 102, kSeRefuel = 0x309;
    const bool pit = adventureRace_ == kEnduranceRun && ai_[i].zone == kPitLane && lap_[i].zone != 0 &&
                     lap_[i].laps < laps_ - 1;
    // Car flags exactly 2: player 1's car as the pad drives it (not finished, not AI-driven).
    const bool pad = PlayerOf(i) == 0 && !lap_[i].finished && !AiDriven(i) && !replay;
    if (!pit) {
        if (pitting_[i] && pad) audio::Sound_PlaySe(kSeRefuel | 0x8000);
        pitting_[i] = false;
        return false;
    }
    if (!pitting_[i] && pad) {
        sounds_[0].StopHorn();
        audio::Sound_PlaySe(kSeRefuel);
    }
    pitting_[i] = true;
    *bits = Ai_Drive(path_, ai_[i], next, bodies_[i], true, go_, false) & ~(uint32_t)kCarR2;
    bodies_[i].fuel = bodies_[i].fuel + 0x200 > 0x40000 ? 0x40000 : bodies_[i].fuel + 0x200;
    return true;
}

void Race::Race_DrawPitBanners() {
    // func_00234378 / func_00234450: an element of type 3 (scale 2, picture 1) at (320, 112), set up again each
    // time it comes on. (GO!'s test also needs Race_Task's +0x25 bit 0 and +0x27 clear: not traced, left out.)
    struct Banner {
        int u, v, w, h, palette;
    };
    static const Banner kBanners[2] = {{0, 128, 160, 24, 3}, {0, 216, 128, 40, 10}};   // D_0029F7A8, D_0029F7C8
    const bool on[2] = {pitting_[0], ai_[0].zone == 103 && ai_[0].wrongWay == 0};
    for (int k = 0; k < 2; ++k) {
        HudElement& e = pitBanners_[k];
        const int type = on[k] ? 3 : 0;
        if (e.type != type) {
            e = HudElement();
            e.type = type;
            e.x = 320;
            e.y = 112;
            e.picture = 1;
            e.palette = kBanners[k].palette;
            e.u = kBanners[k].u;
            e.v = kBanners[k].v;
            e.w = kBanners[k].w;
            e.h = kBanners[k].h;
            e.scale = 2.0f;
        }
        Hud_DrawElement(d_, e);
    }
}

void Race::Race_SaveDistance() {
    SaveRecord& r = Save_Current();
    for (int i = 0; i < carCount_; ++i) {
        const int m = cars_[i].entry;
        if (m < 0 || m > 2) continue;
        // The car's parts record (+0x184) is the save car's: its flags (the billboard, 0x200) and sponsor (+7). (The
        // Tunnel Race's lent car races on the row's parts, with no billboard.)
        const int32_t v = bodies_[i].odometer / 0x8000;
        if (m == 0) r.distance += v;
        if ((parts_[i].flags & 0x200) && r.cars[m].billboard < 5) r.sponsorDistance[r.cars[m].billboard] += v;
        bodies_[i].odometer = 0;
    }
}

void Race::Race_Setup(int id) {
    for (MeterState& m : meterState_) m = MeterState();
    if (id != id_) {
        course_.Load(d_, dataDir_, id);
        collision_.Load(dataDir_ + "\\collision\\" + Course::File(id) + ".pmp");
        objects_.Load(d_, dataDir_, id);
        hud_.Load(dataDir_, id, Course::ModelDir(id));
        if (!path_.Load(dataDir_ + "\\ai\\" + Course::File(id) + ".txt"))
            fprintf(stderr, "no AI path for course %d: the opponents stay on the grid\n", id);
        cameras_.Load(dataDir_ + "\\camera\\" + Course::File(id) + ".txt");
    }
    // Each race's cars can differ (2 Player's come from the records): Load skips bodies already loaded.
    carModels_.Load(d_, dataDir_, cars_, carCount_);
    id_ = id;
    Hud_SetCourse(id);   // the loader's request for the course's HUD file (SCORE.GSL or an event's own)
    // Race_Setup's new Course_ObjectsTask: the objects and animated textures start over, on the same course too.
    objects_.Course_ClearObjects();
    // Race_Setup: the course's lights (Light_Setup) and the time of day its race starts at.
    Light_Setup(light_, id);
    dayClock_ = g_clock >= 0 ? g_clock : Light_CourseClock(id);
    for (int i = 0; i < carCount_; ++i) {
        RaceCar& car = cars_[i];
        Car_PlaceOnGrid(car);
        uint16_t yaw = (uint16_t)(Course::Grid(id_).dir << 14);
        if (i == 0 && g_place) {
            for (int k = 0; k < 3; ++k) car.pos[k] = g_placePos[k];
            yaw = (uint16_t)g_placeYaw;
            car.yaw = (int16_t)yaw * kPi / 32768.0f;
            Land(car);
        }
        Car_Place(bodies_[i], car.pos, yaw);
        car.physics = &bodies_[i];
        // Car_Init: on the big tires the body starts 0.85 up (+0x74; Car_Update keeps it there).
        if (parts_[i].flags & kPartBigTires) bodies_[i].bodyHeight = kBigTiresLift;
        if (PlayerOf(i) >= 0 && g_autoDrive > 0) ai_[i].cap = g_autoDrive;
        Lap& lap = lap_[i];
        lap.zone = lap.laps = lap.time = 0;
        lap.order = car.slot;
        lap.finished = false;
        lap.best = lap.total = kNoTime;   // Race_InitTimes
        car.out = false;
    }
    // D_0177EA50[0], player 1's car: Car_Ground's water test reads its flags for every car. (The demo points it at
    // the save's car 0.)
    Car_SetPlayerFlags(demo_ ? Save_Record(0).cars[0].partFlags : parts_[0].flags);
    for (int p = 0; p < kMaxPlayers; ++p) {
        for (int& t : playerLaps_[p]) t = kNoTime;
        // RaceView_Init's split mode: the whole screen, or the top and bottom halves. A player's car on the big
        // tires takes the views 4 on.
        chase_[p].Reset(players_ > 1 ? p + 1 : 0, id_ == 0x1A, p < carCount_ && (parts_[p].flags & kPartBigTires));
        finish_[p] = -1;
    }
    clock_ = finished_ = 0;
    frame_ = 0;
    countdownStart_ = 0;
    go_ = false;
    phase_ = kRacing;
    paused_ = false;
    fade_ = finalLap_ = 0;
    result_ = RaceResult();
    result_.scenePlace = -1;
    replayPending_ = recording_ = false;
    replayBits_.clear();
    replayPos_ = 0;
    lights_ = StartLights();
    effects_.Load(d_);
    // Race_Task's first frame loads the course's song; the countdown starts it. (The demo has no Race_Task: the
    // title plays REPLAY.TSQ.)
    if (!demo_) audio::Sound_LoadSong(Race_CourseSong(id));
    // The course row's task: on an event's course its own (race_events.cpp) instead of Race_Task.
    task_ = Race_TaskFor(id);
    for (bool& b : braked_) b = false;
    for (bool& b : pitting_) b = false;
    for (HudElement& e : pitBanners_) e = HudElement();
    singleLap_ = -1;
    singleLapFade_ = 0;
    noPause_ = false;
    if (task_ != kTaskRace) {
        float placed[3] = {cars_[0].pos[0], cars_[0].pos[1], cars_[0].pos[2]};
        Race_EventStart();
        if (g_place) {   // --place wins over the event's start
            Car_Place(bodies_[0], placed, (uint16_t)g_placeYaw);
            Car_WorldPosition(bodies_[0], cars_[0].pos);
        }
        if (task_ == kTaskObstacle && g_gate >= 0 && g_gate < 14) event_.checkpoint[0] = g_gate;
        if (task_ == kTaskHighway && g_gate >= 0 && g_gate < path_.Zones()) ai_[0].zone = g_gate;
    }
    // Race Solo (race flag 0x800): Race_Setup's branch for it sets race flags 0x104 at once, green (the clock runs and
    // the car may go) and the pause; Race_Task's countdown still plays its lights, beeps and song.
    if (solo_) go_ = true;
    if (demo_) {
        // Mode 8: race flags 4 (green) at once, and Car_Init's lap zone 1 (0x00218E84), so the AI steers from the
        // start. TitleSeq_Task takes every press before the race sees it: no pause.
        go_ = true;
        noPause_ = true;
        for (int i = 0; i < carCount_; ++i) lap_[i].zone = 1;
    }
}

void Race::Race_CountdownTask() {
    // Its task's frame: from the race's start, or Figure 8's step's (none before the first).
    const int t = frame_ - countdownStart_;
    if (countdownStart_ < 0 || t > kCountdownEnd) return;
    if (t == kCountdownBeeps) audio::Sound_PlaySe(kSeCountdown);
    if (t == kCountdownGo) go_ = true;
    if (t == kCountdownSong) audio::Sound_StartSong();
    // Step 2 (all red) for the first second, one step a second up to 6 (green) at kCountdownGo; gone at the end.
    if (t < kCountdownSong)
        Hud_SetStartLights(lights_, t / 60 + 2);
    else
        Hud_SetStartLights(lights_, t < kCountdownEnd ? 6 : 0);
}

// The grid (D_002A7F28): slot 0 at the table's position, each slot 5 units further back, and odd slots 7.5 to the
// side the table's side flag picks. The car faces dir quarter turns from +z. The game then drops it onto the
// ground (D_00335710): the highest surface under it, so the table's height only matters where surfaces overlap
// (Oval Raceway's grid is 30 below its track, Lagoon Raceway's 100).
void Race::Car_PlaceOnGrid(RaceCar& car) {
    const GridStart& g = Course::Grid(id_);
    // The rows 5 apart are the race courses' (ids below 0x12): on the events' courses every car stands on the first
    // row, the odd slots beside the even ones.
    float row = id_ < 0x12 ? car.slot * 5.0f : 0.0f, side = (car.slot & 1) * 7.5f;
    float* pos = car.pos;
    pos[0] = g.x;
    pos[1] = g.y;
    pos[2] = g.z;
    switch (g.dir & 3) {
    case 0:   // +z
        pos[0] += g.side ? -side : side;
        pos[2] -= row;
        break;
    case 1:   // +x
        pos[0] -= row;
        pos[2] += g.side ? -side : side;
        break;
    case 2:   // -z
        pos[0] += g.side ? side : -side;
        pos[2] += row;
        break;
    case 3:   // -x
        pos[0] += row;
        pos[2] += g.side ? side : -side;
        break;
    }
    car.yaw = (g.dir & 3) * (kPi / 2);
    car.physics = nullptr;
    Land(car);
}

void Race::Land(RaceCar& car) {
    float p[4] = {car.pos[0], car.pos[1], car.pos[2], 1.0f}, ceiling;
    if (collision_.Collision_Ground(p, &ceiling) >= 0) {
        car.pos[1] = p[1];
        return;
    }
    // No collision data: the field's surface nearest the grid's height.
    std::vector<float> h;
    course_.Surfaces(car.pos[0], car.pos[2], h);
    if (h.empty()) return;
    float best = h[0];
    for (float y : h)
        if (fabsf(y - car.pos[1]) < fabsf(best - car.pos[1])) best = y;
    car.pos[1] = best;
}

// A camera at `eye` looking along `yaw` (16-bit, 0 = +z) tilted `pitch` down, focal length 500.
static gfx::Camera3D FixedView(const float* eye, int yaw16, int pitch16) {
    float yaw = (int16_t)yaw16 * kPi / 32768.0f, pitch = (int16_t)pitch16 * kPi / 32768.0f;
    float sy = sinf(yaw), cy = cosf(yaw), sp = sinf(pitch), cp = cosf(pitch);
    gfx::Camera3D c;
    const float fwd[3] = {sy * cp, -sp, cy * cp}, right[3] = {cy, 0, -sy}, up[3] = {sy * sp, cp, cy * sp};
    for (int i = 0; i < 3; ++i) {
        c.forward[i] = fwd[i];
        c.right[i] = right[i];
        c.up[i] = up[i];
        c.eye[i] = eye[i];
    }
    c.focalX = kFocal;
    c.focalY = kFocal * 0.94f;
    c.centerX = gfx::Draw2D::kWidth / 2.0f;
    c.centerY = gfx::Draw2D::kHeight / 2.0f;
    return c;
}

// Camera_OverheadTask (func_0021ED90) under race flag 0x80000000: on course 0x18 the preset D_0029CCA0, 50 above the
// wheel's centre looking straight down (pitch 0x4000).
const float kRouletteEye[3] = {400.0f, 50.0f, 400.0f};

// On course 0x17 (Golf) the preset D_0029CC80: 20 behind the ball in -z and 2 up, pitched 0x1000 down, the ball's place
// (D_0177EB40) as last frame left it.
gfx::Camera3D Race::RaceView(int player) const {
    if (overhead_ && id_ == 0x17) {
        const float eye[3] = {golf_.ball[0], golf_.ball[1] + 2.0f, golf_.ball[2] - 20.0f};
        return FixedView(eye, 0, 0x1000);
    }
    if (overhead_) return FixedView(kRouletteEye, 0, 0x4000);
    gfx::Camera3D c = chase_[player].View();
    if (players_ == 1) EyeView(&c);
    return c;
}

bool Race::EyeView(gfx::Camera3D* view) {
    if (!g_eye) return false;
    *view = FixedView(g_eyePos, g_eyeYaw, g_eyePitch);
    return true;
}

void Race::Race_CheckLapZones(int i) {
    const float(*z)[4] = kLapZones[id_ < Course::kCourses ? id_ : 0];
    const float* pos = cars_[i].pos;
    Lap& lap = lap_[i];
    if (InZone(z[0], pos)) {
        if (lap.zone == 4) Race_CompleteLap(i);
        lap.zone = 2;
    } else if (InZone(z[1], pos)) {
        lap.zone = 3;
    } else if (InZone(z[2], pos)) {
        if (lap.zone == 3) lap.zone = 4;
    } else if (lap.zone == 2) {
        lap.zone = 1;   // backed out over the line
    }
}

void Race::Race_CompleteLap(int i) {
    Lap& lap = lap_[i];
    bool replay = phase_ == kReplay || phase_ == kReplayFade;   // race flag 0x40: no times, chimes or places
    int p = PlayerOf(i);
    if (p >= 0 && !replay) {
        // A player's lap: a chime, a brighter one for a lap that beats the course's best.
        int& best = bestLap_[p];
        int* laps = playerLaps_[p];
        if (lap.time < best) {
            audio::Sound_PlaySe(best != kNoTime ? kSeBestLap : kSeLap);
            best = lap.time;
        } else {
            audio::Sound_PlaySe(kSeLap);
        }
        if (lap.laps >= 3) {
            laps[0] = laps[1];
            laps[1] = laps[2];
            laps[2] = lap.time;
        } else {
            laps[lap.laps] = lap.time;
        }
    }
    // Every player's and teammate's lap (car flags 0x73) goes into its best of the race (D_01787068).
    if ((p >= 0 || teammate_[i]) && !replay && lap.time < lap.best) lap.best = lap.time;
    lap.time = 0;
    ++lap.laps;
    if (solo_) return;   // race flag 0x800: no final lap, no finish
    if (lap.laps == laps_) {
        // Finished: its place is fixed (+0x247), and the AI drives it on at the finish's cap (flags 0x280).
        if (!replay) lap.order = finished_++;
        if ((p >= 0 || teammate_[i]) && !replay) lap.total = clock_;   // D_01786FF0[3 x side + member]
        lap.finished = true;
        ai_[i].cap = kFinishCap;
        // A player's finish sets their bit of Race_Task +0x25 and FINISH! in their half. With every player's bit
        // the race is complete: the fade out; in the replay, the replay's end. (2 Player sets the teammates' bits
        // too then, so the race doesn't wait for them.)
        if (p >= 0) {
            sounds_[p].StopHorn();   // (+0x245 + 0x8000)
            if (!replay) finish_[p] = 0;
            bool all = true;
            for (int k = 0; k < players_; ++k) all &= lap_[k].finished;
            if (all) phase_ = replay ? kReplayFade : kFinishFade;
        }
    } else if (lap.laps == laps_ - 1 && i == 0 && players_ == 1 && !replay) {
        // The final lap: race flag 0x10 and FINAL LAP; at the end of the frame Race_SetupTask saves the race for the
        // replay. 2 Player has neither (Race_CompleteLap checks !(mode & 2)).
        finalLap_ = kFinalLapBlink;
        replayPending_ = true;
    }
}

void Race::Replay_Save() {
    for (int i = 0; i < carCount_; ++i) {
        replayStart_.cars[i] = cars_[i];
        replayStart_.bodies[i] = bodies_[i];
        replayStart_.ai[i] = ai_[i];
        replayStart_.lap[i] = lap_[i];
    }
    replayStart_.finished = finished_;
    replayStart_.frame = frame_;
    replayBits_.clear();
    recording_ = true;
}

void Race::Replay_Restore() {
    for (int i = 0; i < carCount_; ++i) {
        cars_[i] = replayStart_.cars[i];
        cars_[i].physics = &bodies_[i];
        bodies_[i] = replayStart_.bodies[i];
        ai_[i] = replayStart_.ai[i];
        lap_[i] = replayStart_.lap[i];
    }
    finished_ = replayStart_.finished;
    frame_ = replayStart_.frame;
    replayPos_ = 0;
    replayGroup_ = replayCar_ = 0;
}

int Race::Replay_GroupCar(int group) const {
    if (group == 0) return 0;
    if (!adventure_) return -1;   // only the Adventure's races have teammates
    for (int i = 0; i < carCount_; ++i)
        if (cars_[i].entry == group && !cars_[i].out) return i;
    return -1;
}

// Obj_Before in Race_RankCars: more laps (a car short of the line's second strip counts one lap less), then further
// through the line's strips, then further along the AI's path (its zone's rank, +0x24B), then closer to the gate
// it aims for (+0x24C).
bool Race::Car_Ahead(int a, int b) const {
    const Lap &la = lap_[a], &lb = lap_[b];
    int ka = la.laps - (la.zone < 2), kb = lb.laps - (lb.zone < 2);
    if (ka != kb) return ka > kb;
    if (la.zone != lb.zone) return la.zone > lb.zone;
    if (ai_[a].rank != ai_[b].rank) return ai_[a].rank > ai_[b].rank;
    return ai_[a].dist <= ai_[b].dist;
}

void Race::Race_RankCars() {
    int racing[Cars::kMaxCars], n = 0;
    for (int i = 0; i < carCount_; ++i) {
        if (lap_[i].finished || cars_[i].out) continue;
        // Insertion in place: after every car ahead of it.
        int k = n++;
        while (k > 0 && !Car_Ahead(racing[k - 1], i)) {
            racing[k] = racing[k - 1];
            --k;
        }
        racing[k] = i;
    }
    for (int k = 0; k < n; ++k) lap_[racing[k]].order = finished_ + k;
}

void Race::Race_FillHud(int p, RaceHudState& s, MapMarker* markers) {
    const Lap& lap = lap_[p];
    s.frame = frame_;
    s.totalTime = clock_;
    // The lap rows: the last two laps (three once finished), each in the best lap's colour if it is the best, then the
    // lap being driven.
    s.lapRowCount = 0;
    int shown = lap.finished ? 3 : 2, first = lap.laps > shown ? lap.laps - shown : 0;
    for (int n = first; n < lap.laps; ++n) {
        int t = playerLaps_[p][lap.laps > 3 ? n - (lap.laps - 3) : n];
        s.lapRows[s.lapRowCount++] = {n + 1, t, t == bestLap_[p] ? kTimeBest : kTimeOther};
    }
    if (!lap.finished) s.lapRows[s.lapRowCount++] = {lap.laps + 1, lap.time, kTimeRunning};
    s.order = lap.order;
    s.cars = carCount_;
    s.meter = Meter_Read(bodies_[p]);
    s.meter.finsPart = (parts_[p].flags & kPartFins) != 0;
    s.meter.timeOfDay = dayClock_;
    s.meter.paused = paused_;
    // Meter_Draw's routine: player record i's +0x1A, the save's car 0's meter part (in every mode).
    s.meterPart = Save_Record(p).cars[0].parts[14];
    s.meterState = &meterState_[p];
    // Hud_SetPosition's teammate slots: the places of this player's teammates (car flags 0x10, 0x20), in car order;
    // hidden (-1) without them.
    s.teammateOrder[0] = s.teammateOrder[1] = -1;
    for (int i = 0, n = 0; i < carCount_ && n < 2; ++i)
        if (teammate_[i] && side_[i] == p) s.teammateOrder[n++] = lap_[i].order;
    for (int i = 0; i < carCount_; ++i) {
        MapMarker::Kind kind = PlayerOf(i) >= 0 ? MapMarker::kPlayer : teammate_[i] ? MapMarker::kTeammate
                                                                                    : MapMarker::kOther;
        markers[i] = {kind, cars_[i].pos[0], cars_[i].pos[2], cars_[i].yaw, side_[i]};
    }
    s.markers = markers;
    s.markerCount = carCount_;
    s.lights = &lights_;
    s.finished = lap.finished;
    s.finish = finish_[p];
    // Race_Task blinks the banners: FINAL LAP on bit 3 of its count down, WRONG WAY on bit 4 of the frame.
    s.finalLap = (finalLap_ & 8) != 0;
    s.wrongWay = phase_ == kRacing && !lap.finished && ai_[p].wrongWay >= kWrongWayFrames && (frame_ & 0x10);
    s.replay = phase_ == kReplay || phase_ == kReplayFade;
}

void Race::Race_Step(const uint32_t* pad) {
    bool replay = phase_ == kReplay || phase_ == kReplayFade;
    // Soccer's task makes no countdown task: its kick-offs do that work.
    if (demo_) {
    } else if (Race_StartLightsCountdown()) Race_CountdownTask();
    else if (Race_EventCountdown()) Race_EventCountdownTask();
    Car_SetClock(dayClock_);
    // The cars' tasks, then Cars_Collide's. Car_Input first tracks each car on the AI's path, then reads the pad for
    // the players (the recording in the replay; the AI once finished) and runs the AI for the others (steering once
    // they've reached the line).
    uint32_t bits[Cars::kMaxCars] = {};
    CarBody* all[Cars::kMaxCars];
    int index[Cars::kMaxCars], moving = 0;
    for (int i = 0; i < carCount_; ++i) {
        if (cars_[i].out) continue;   // Car_Update does nothing for a car out of the race
        int p = PlayerOf(i);
        // The Car Readouts extra: the debug byte's lines for the players' cars, the full set for player 1's driven
        // from the pad (car flags exactly 2).
        const bool readouts = Extras_On(kExtraReadouts) && p >= 0 && !demo_;
        const bool full = readouts && p == 0 && !replay && !lap_[i].finished && !AiDriven(i);
        if (readouts) Ai_SetDebugPrint(DebugText_Printf, full);
        int next = Ai_TrackZone(path_, ai_[i], bodies_[i]);
        Ai_SetDebugPrint(nullptr, false);
        if (p == 0 && replay) {
            bits[i] = replayPos_ < replayBits_.size() ? replayBits_[replayPos_++] : 0;
        } else if (p >= 0 && !lap_[i].finished && !AiDriven(i)) {
            bits[i] = pad[p];
            sounds_[p].Car_Horn(horn_[p]);   // the horn key (6), R1 by default: on while it's held
        } else if (task_ == kTaskSoccer) {
            bits[i] = Soccer_CarAi(i);   // the course row's AI routine (D_002BE1D8 +0xC)
        } else {
            // Car_Input's last argument: a teammate (flags 0x30) the AI drives learns into its saved table.
            // The Slow Rivals cheat: the players' opponents, not their teammates.
            Ai_SetSlow(p < 0 && !teammate_[i] && !demo_ && Extras_On(kCheatSlowRivals));
            bits[i] = Ai_Drive(path_, ai_[i], next, bodies_[i], lap_[i].zone != 0, go_, teammate_[i]);
            Ai_SetSlow(false);
        }
        Race_PitStop(i, next, &bits[i], replay);
        // Car_Input: flag 0x800 keeps the steering and holds the brake.
        if (braked_[i]) bits[i] = (bits[i] & 0xFF00) | kCarBrake;
        int8_t water = bodies_[i].water;
        // In 2 Player every car but the one in slot 0 (after the finish: the winner) has more drag.
        bool slowed = players_ > 1 && lap_[i].order != 0;
        if (full) Car_SetDebugPrint(DebugText_Printf);
        if (p >= 0 && !replay) Car_SetHeightKey(heightKey_[p]);
        // The cheats for the players' cars (the replay's too, which drives from the recorded pad).
        if (p >= 0 && !demo_) Car_SetCheats(Car_PlayerCheats());
        Car_Update(bodies_[i], parts_[i], collision_, bits[i], go_, id_ == kTunnelCourse, slowed);
        Car_SetDebugPrint(nullptr);
        Car_SetHeightKey(0);
        Car_SetCheats(0);
        if (cars_[i].partFlags != parts_[i].flags) {
            // The Flight Wing out or folded, in the car's parts record: in the Adventure the save's car.
            cars_[i].partFlags = parts_[i].flags;
            const int m = cars_[i].entry;
            if (adventure_ && m >= 0 && m <= 2 && id_ != kTunnelCourse)
                Save_Current().cars[m].partFlags = parts_[i].flags;
            if (i == 0) Car_SetPlayerFlags(parts_[0].flags);
        }
        index[moving] = i;
        all[moving++] = &bodies_[i];
        if (p >= 0) {
            // Car_Ground: a player's car running into water splashes (CQ_MAIN 0x28; not with race flags 0x48).
            if (water == 0 && bodies_[i].water < 0 && phase_ == kRacing && !lap_[i].finished)
                audio::Sound_PlaySe(kSeSplash);
            // Car_Crash: a player's car into the Tunnel Race's walls (not with race flags 0x48).
            if (bodies_[i].crashed && phase_ == kRacing) {
                audio::Sound_PlaySe(kSeCrashStop);
                audio::Sound_PlaySe(kSeCrash);
            }
            sounds_[p].Car_Sound(bodies_[i], bits[i], lap_[i].finished, replay);
            sounds_[p].Car_Jet(bodies_[i].jetSound, replay);
            // The pad's shake from a landing or a wall (a player's car: not finished or AI-driven, flags 0xFB0;
            // not in the replay, race flag 0x40), on the car's own pad (flags & 1).
            if (bodies_[i].shake && !replay && !lap_[i].finished && !AiDriven(i))
                Pad_Vibrate(p, bodies_[i].shake, bodies_[i].shakeStrength);
        }
    }
    if (recording_ && !replay) replayBits_.push_back(bits[0]);
    CarContact contacts[Cars::kMaxCars * 2];
    int touched = Cars_Collide(all, moving, contacts, Cars::kMaxCars * 2);
    if (touched > Cars::kMaxCars * 2) touched = Cars::kMaxCars * 2;
    for (int k = 0; k < touched; ++k) {
        contacts[k].a = index[contacts[k].a];
        contacts[k].b = index[contacts[k].b];
    }
    Cars_BumpSound(contacts, touched);
    for (int i = 0; i < carCount_; ++i) {
        Car_WorldPosition(bodies_[i], cars_[i].pos);
        cars_[i].yaw = (int16_t)bodies_[i].yaw * kPi / 32768.0f;
    }
    // Race_Task, until the players finish: the clock, each racing car at the line and its lap time (from its first
    // crossing), the order. The replay's task (Race_ReplayTask 0x0022EB78) only watches the line.
    if (task_ == kTaskRace && !demo_ && (phase_ == kRacing || replay)) {
        if (go_ && phase_ == kRacing) ++clock_;
        for (int i = 0; i < carCount_; ++i) {
            if (lap_[i].finished || cars_[i].out) continue;
            Race_CheckLapZones(i);
            if (lap_[i].zone) ++lap_[i].time;
        }
        if (!replay) Race_RankCars();
    }
    if (go_ && (phase_ == kRacing || phase_ == kFinishFade) && (id_ == 0x0F || id_ == 0x10)) Race_CheckFall();
    if (id_ == Course::kTestCourse) TestCourse_PutBack();
    if (go_ && phase_ == kRacing && id_ == kSingleLapCourse && !replay) Race_SingleLapTask();
    if (finalLap_) --finalLap_;
    // An event's task runs after the cars, as Race_Task would.
    if (task_ != kTaskRace && phase_ == kRacing && Race_EventTask()) phase_ = kEventDone;
    if (g_trace && g_traceCar < carCount_ && !replay) {
        const CarBody& c = bodies_[g_traceCar];
        const AiCar& a = ai_[g_traceCar];
        fprintf(g_trace, "%d,%u,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%.2f,%d,%.2f,%d,%d\n",
                frame_,
                bits[g_traceCar], c.fixed[0], c.fixed[1], c.fixed[2], c.vel[0], c.vel[1], c.vel[2], (int16_t)c.yaw,
                c.steerAngle, c.turn, c.wheel, c.rpm, c.gear, c.load[0], c.load[1], c.load[2], c.spring[0],
                c.spring[1], c.spring[2], c.slide, c.surface[0], a.zone, a.rank, a.dist, c.water, c.probe[0][3],
                c.effect, c.squeal);
        fflush(g_trace);
    }
    ++frame_;
    if (replayPending_) {
        replayPending_ = false;
        Replay_Save();
    }
}

void Race::Cars_BumpSound(const CarContact* contacts, int count) {
    bool replay = phase_ == kReplay || phase_ == kReplayFade;
    for (int k = 0; k < count; ++k) {
        // Each player's car in the pair hears it (two players' cars: both, each against the other's count).
        int pair[2] = {contacts[k].a, contacts[k].b};
        bool hit[2] = {false, false};
        for (int n = 0; n < 2; ++n) {
            int me = pair[n], p = PlayerOf(me);
            if (p < 0) continue;
            CarBody& other = bodies_[pair[n ^ 1]];
            hit[n ^ 1] = true;
            if (replay || other.bump != 0 || lap_[me].finished) continue;
            // The pad shakes by how hard the cars met: their speeds' difference in 1/32768ths x 1000 (full from a
            // quarter of a unit a frame).
            float d[3], sq = 0;
            for (int c = 0; c < 3; ++c) d[c] = (other.vel[c] - bodies_[me].vel[c]) / 32768.0f, sq += d[c] * d[c];
            if (!AiDriven(me)) Pad_Vibrate(p, kShakeBump, (int)(sqrtf(sq) * 1000.0f));
            audio::Sound_PlaySe(kSeBump);
        }
        for (int n = 0; n < 2; ++n)
            if (hit[n]) bodies_[pair[n]].bump = 8;
    }
}

RaceResult Race::RetireResult() const {
    RaceResult r = result_;
    r.course = id_;
    r.place = -1;
    r.cars = carCount_;
    for (int i = 0; i < carCount_; ++i) {
        int o = lap_[i].order;
        if (i < 3) {
            o = -1;
        } else if (o >= 0) {
            int ahead = 0;
            for (int j = 0; j < 3 && j < carCount_; ++j)
                if (lap_[j].order >= 0 && lap_[j].order < lap_[i].order) ++ahead;
            o -= ahead;
        }
        r.order[i] = o;
        r.total[i] = r.best[i] = kNoTime;
        r.name[i] = cars_[i].record->name;
        r.group[i] = cars_[i].town;
        r.index[i] = cars_[i].index;
    }
    return r;
}

void Race::Race_AfterFinish() {
    // Race_Task copies the places (D_01787008); the times are the player's.
    RaceResult& r = result_;
    r.course = id_;
    r.place = lap_[0].order;
    r.totalTime = clock_;
    for (int i = 0; i < 3; ++i) r.laps[i] = playerLaps_[0][i];
    r.bestLap = bestLap_[0];
    r.raceBestLap = kNoTime;
    for (int t : playerLaps_[0])
        if (t < r.raceBestLap) r.raceBestLap = t;
    r.cars = carCount_;
    for (int i = 0; i < carCount_; ++i) {
        r.order[i] = lap_[i].order;
        r.total[i] = lap_[i].total;
        r.best[i] = lap_[i].best;
        r.name[i] = cars_[i].record->name;
        r.group[i] = cars_[i].town;
        r.index[i] = cars_[i].index;
    }
    for (auto& side : r.versusCar)
        for (int& c : side) c = -1;
    for (int i = 0; i < carCount_ && (players_ > 1 || adventure_); ++i)
        if (cars_[i].entry >= 0) r.versusCar[side_[i]][cars_[i].entry % 3] = i;

    fade_ = 0;
    for (int& f : finish_) f = -1;
    finalLap_ = 0;
    audio::Sound_StopSong();
    // 2 Player has no replay: Race_SetupTask goes to its results.
    if (players_ == 1 && r.place < kReplayPlaces && id_ != 0x0F && id_ != 0x10 && recording_) {
        // Race_SetupTask: REPLAY's song, race flags 0x10044 (green, the replay, no map or meter), the saved race.
        Replay_Restore();
        recording_ = false;
        phase_ = kReplay;
        audio::Sound_LoadSong(kReplaySong);
        audio::Sound_StartSong();
    } else {
        for (CarSounds& c : sounds_) c.Stop();
        phase_ = kDone;
    }
}

void Race::Race_SingleLapTask() {
    const float kStart[2][3] = {{819.0f, 300.0f, 695.0f}, {819.0f, 300.0f, 700.0f}};   // D_003331C8, D_003331CC
    const float kLow = 280.0f;
    switch (singleLap_) {
    case -1:   // the task's first frame: its work area
        singleLap_ = 0;
        break;
    case 0:
        for (int i = 0; i < 2 && i < carCount_; ++i) Car_SetPlace(bodies_[i], kStart[i], (uint16_t)-0x4000);
        singleLap_ = 1;
        // falls through: the same frame watches the heights
    case 1:
        if (bodies_[0].pos[1] < kLow) {
            result_.outcome = 1;
            braked_[0] = true;
            singleLap_ = 2;
        }
        if (carCount_ > 1 && bodies_[1].pos[1] < kLow) {
            result_.outcome = 2;
            braked_[0] = true;
            singleLap_ = 2;
        }
        break;
    case 2:   // race flags (& ~0x100) | 8: over, no pause
        noPause_ = true;
        singleLap_ = 3;
        break;
    case 3:
        if (singleLapFade_ < 0xC0) ++singleLapFade_;
        else phase_ = kEventDone;   // -1: black, race flag 8, Race_Task told -1
        break;
    }
}

void Race::Race_CheckFall() {
    for (int i = 0; i < carCount_; ++i) {
        if (cars_[i].out || lap_[i].finished || cars_[i].pos[1] >= kFallHeight) continue;
        if (PlayerOf(i) < 0) {
            cars_[i].out = true;
            lap_[i].order = -1;
            continue;
        }
        audio::Sound_FadeOutSong();
        sounds_[0].Mute();
        phase_ = kFallOut;
        fade_ = 0;
        return;   // the task deletes itself
    }
}

// The tube's floor under (x, z): Collision_Ground from far below. The tube's upper half is layer surfaces (a ceiling
// for what's under them), so that is the highest ground under the tube's top: its floor, not the start box's quad
// under it. False where there's no ground (outside the tube, or in its gap over the box: the box).
bool Race::TestCourse_Floor(float x, float z, float* height) const {
    float p[4] = {x, -1000.0f, z, 1.0f}, ceiling;
    if (collision_.Collision_Ground(p, &ceiling) < 0) return false;
    *height = p[1];
    return true;
}

void Race::TestCourse_PutBack() {
    const float kBelow = 3.0f, kAbove = 4.0f;
    if (path_.Empty()) return;
    for (int i = 0; i < carCount_; ++i) {
        if (cars_[i].out) continue;
        CarBody& b = bodies_[i];
        const AiZone& z = path_.Zone(ai_[i].zone);
        const AiLine &in = path_.Line(z.in), &g = path_.Line(z.out);
        float p[3] = {(g.x0 + g.x1) * 0.5f, 0.0f, (g.z0 + g.z1) * 0.5f}, back = 0.0f;
        if (!TestCourse_Floor(p[0], p[2], &p[1])) continue;
        if (!TestCourse_Floor((in.x0 + in.x1) * 0.5f, (in.z0 + in.z1) * 0.5f, &back)) continue;
        // Out: 3 lower than the floor at both of its zone's gates (on the box under the gap, or fallen outside). In
        // three laps driven by the AI a car is never more than 0.15 under the lower.
        if (b.pos[1] > (p[1] < back ? p[1] : back) - kBelow) continue;
        // The road's heading there: square to the gate (Ai_TrackZone's), as a 16-bit angle.
        const uint16_t yaw = (uint16_t)((int32_t)(atan2f(g.x0 - g.x1, g.z0 - g.z1) * 10430.378f) - 0x4000);
        const float pos[3] = {p[0], p[1] + kAbove, p[2]};
        if (b.jet && PlayerOf(i) >= 0) sounds_[PlayerOf(i)].Car_Jet(kJetStop, false);
        Car_Reset(b, pos, yaw);
        Car_WorldPosition(b, cars_[i].pos);
    }
}

// Race_SetupTask's lighting, each frame: on Endurance Run (course 0x11) the clock runs, a frame at a time while the
// race does, and the time of day sets the fog too, by the player's car's height. Elsewhere the clock stands and the
// course's own fog replaces the time of day's.
void Race::Race_UpdateLight() {
    if (id_ == kClockCourse) {
        if (!paused_) {
            // The Clock Scrub extra (the debug byte): L2 forward 0x168 frames (6 s) a frame, L1 back, each wrapping
            // as the debug build's did (back from under 0: + 0x34A58).
            if (clockScrub_ > 0) {
                dayClock_ += 0x168;
                if (dayClock_ > kClockHour) dayClock_ -= kClockHour;
            } else if (clockScrub_ < 0) {
                int32_t old = dayClock_;
                dayClock_ -= 0x168;
                if (dayClock_ < 0) dayClock_ = old + 0x34A58;
            }
            int32_t t = dayClock_++;
            if (t > kClockHour) dayClock_ -= kClockHour;
        }
        Light_TimeOfDay(light_, dayClock_, bodies_[0].pos[1]);
    } else {
        Light_TimeOfDay(light_, dayClock_, 0.0f);
        Light_CourseFog(light_, id_);
    }
}

void Race::Race_Pause(int pad) {
    sounds_[0].Mute();   // Sound_MuteCars: every car's sounds
    audio::Sound_PlaySe(kSePause);
    audio::Sound_MuteSong();
    paused_ = true;
    zoneTable_ = false;
    pause_.Open((g_rankingItem || adventure_) && id_ < kRankingCourses && players_ == 1, pad,
                Extras_On(kExtraPauseItems) && players_ == 1 && !demo_, meterHidden_);
}

void Race::Race_Resume() {
    paused_ = false;
    sounds_[0].Loud();   // Sound_LoudCars: both engines
    audio::Sound_UnmuteSong();
}

// Start pauses the race from the song's start until it's over (Race_SetupTask); in the replay Start ends it. After
// the player's finish, and at the replay's end (the player's finish again, or Start), the screen fades to black over
// 0x80 frames while the cars drive on (Race_Task's Fade_SetOverlay, Race_ReplayFadeOut 0x0022EE00).
namespace {
uint64_t g_infoFrames = 0;
}  // namespace

uint64_t Info_Frames() { return g_infoFrames; }
void Info_Tick() { ++g_infoFrames; }

Race::Result Race::Update(const platform::Input& input) {
    if (phase_ == kDone) return kOver;
    // --perf: where the player's car is on the course.
    PerfWhere& where = Perf_Where();
    where.mode = "race";
    where.id = id_;
    for (int k = 0; k < 3; ++k) where.pos[k] = bodies_[0].pos[k];
    where.yaw = bodies_[0].yaw;
    where.clock = dayClock_;
    if (demo_) {
        // Camera_ChaseTask's second frame hands over to Camera_TrackSide on car 4 (0x0021E778), for good; race
        // flag 0x10000 and no Race_Task: nothing over the view.
        const int kDemoCar = 4;
        Race_Step(nullptr);
        Race_UpdateLight();
        bool out[Cars::kMaxCars] = {};
        objects_.Update(CourseCars{bodies_, out, carCount_, false, false});
        const int car = kDemoCar < carCount_ ? kDemoCar : 0;
        Race_DrawView(cameras_.Camera_TrackSide(cars_[car].pos, cars_[car].yaw), -1);
        return kRunning;
    }
    // In 2 Player either pad's Start pauses (pad 2's if both); the pad that did runs the menu.
    int startPad = -1;
    for (int p = 0; p < players_; ++p)
        if (input.Pressed(p) & platform::kStart) startPad = p;
    if (paused_) {
        // Everything stands still under race flag 0x8000; the menu runs below, over the drawn race.
    } else if (startPad >= 0 && (phase_ == kRacing || phase_ == kFinishFade) &&
               ((countdownStart_ >= 0 && frame_ - countdownStart_ >= kCountdownSong) || !Race_HasCountdown() ||
                solo_) &&
               !noPause_) {
        // The race's tasks run after Race_SetupTask, so this frame is already still.
        Race_Pause(startPad);
    } else if (phase_ == kReplay && startPad >= 0) {
        phase_ = kReplayFade;
    }
    // The chase camera while racing (Triangle, the camera button, switches its view); from the finish the trackside
    // cameras follow the player's car. Race_Setup creates the camera's task before the cars', so it frames the car
    // where the last frame left it, and the cars then move before they're drawn. In 2 Player each player's camera
    // keeps chasing to the end (Camera_FinishTask needs !(mode & 2)).
    bool chase = phase_ == kRacing || phase_ == kFallOut || players_ > 1;
    // Race flag 0x2000 (Roulette's board) stills the camera and leaves out the 3D view.
    // The Free Camera extra (the debug byte): controller 2 moves player 1's chase camera, in a race for one.
    ChaseCamera::DebugMove move = {};
    const bool freeCamera = Extras_On(kExtraFreeCamera) && players_ == 1;
    if (freeCamera) {
        move.held = input.Held(1);
        for (int p = 0; p < 2; ++p)
            for (int k = 0; k < 2; ++k) {
                int v = input.Analog(p) ? input.Stick(p, k) - 0x80 : 0;
                move.stick[p][k] = v > -24 && v < 24 ? 0 : v;   // (a dead zone: the PC's)
            }
    }
    chase_[0].SetDebugMove(freeCamera ? &move : nullptr);
    if (chase && !hideView_) {
        for (int p = 0; p < players_; ++p) {
            bool toggle = phase_ == kRacing && (input.Pressed(p) & Car_CameraButton(p)) != 0;
            chase_[p].Camera_ChaseTask(bodies_[p], cars_[p].pos, collision_, toggle, paused_);
        }
    }
    chase_[0].SetDebugMove(nullptr);
    uint32_t pads[kMaxPlayers] = {};
    for (int p = 0; p < players_; ++p) {
        pads[p] = Car_ReadPad(input, p);
        horn_[p] = Car_HornHeld(input, p);
        // The debug byte's ride height (Car_Ground): the d-pad's Up held, else Down.
        uint32_t held = Extras_On(kExtraHeightKeys) ? input.Held(p) : 0;
        heightKey_[p] = (held & platform::kUp) ? 1 : (held & platform::kDown) ? -1 : 0;
    }
    // The debug byte's clock scrub (Race_SetupTask on course 0x11): pad 1's L2 and L1 held, the PC's Wing Down and
    // Wing Up (L2 and L1 in the PS2's key setup).
    uint32_t scrub = Extras_On(kExtraClockScrub) ? input.Held(0) : 0;
    clockScrub_ = ((scrub & platform::kWingDown) ? 1 : 0) - ((scrub & platform::kWingUp) ? 1 : 0);
    if (!paused_) Race_Step(pads);
    Race_UpdateLight();
    bool out[Cars::kMaxCars];
    for (int i = 0; i < carCount_; ++i) out[i] = cars_[i].out;
    // Race flags 0x48: the replay, or the event over.
    bool quiet = phase_ == kReplay || phase_ == kReplayFade || phase_ == kEventDone || phase_ == kDone;
    objects_.Update(CourseCars{bodies_, out, carCount_, players_ > 1, quiet});
    if (id_ == kGateCourse) braked_[0] = objects_.HoldPlayer();   // A16_GateTask sets and clears car flag 0x800
    if (hideView_) {
    } else if (players_ == 1) {
        // Camera_ReplayTask (0x0021E328): Triangle on pad 1 (or the followed car gone) moves on to the next group
        // with a car, the teammates', trying one group on then two.
        int follow = 0;
        if (phase_ == kReplay || phase_ == kReplayFade) {
            if (!paused_ && ((input.Pressed(0) & platform::kTriangle) || cars_[replayCar_].out)) {
                for (int step = 1; step <= 2; ++step) {
                    const int group = (replayGroup_ + step) % 3, car = Replay_GroupCar(group);
                    if (car < 0) continue;
                    replayGroup_ = group;
                    replayCar_ = car;
                    break;
                }
            }
            follow = replayCar_;
        }
        gfx::Camera3D camera =
            chase ? RaceView(0) : cameras_.Camera_TrackSide(cars_[follow].pos, cars_[follow].yaw);
        Race_DrawView(camera, chase && chase_[0].Inside() && !g_eye && !overhead_ ? 0 : -1);
    } else {
        // RaceView_Init(1) and (2): the top half, then the bottom (D_0029D020's SCISSOR_2 by split mode), each with
        // its own camera's highlight; then Race_DrawView puts back the whole frame.
        for (int p = 0; p < players_; ++p) {
            d_.SetView3D(gfx::Rect{0, p * gfx::Draw2D::kHeight / 2, gfx::Draw2D::kWidth,
                                   (p + 1) * gfx::Draw2D::kHeight / 2});
            Race_DrawView(RaceView(p), chase_[p].Inside() ? p : -1);
        }
        d_.ResetView3D();
    }
    // The Map Tuner extra (the debug byte): pad 2 changes the course map's view as it's drawn (RaceMap_Tune), in a
    // race for one player (the PC's limit: in 2 Player pad 2 drives).
    RaceMap_SetTuner(Extras_On(kExtraMapTuner) && players_ == 1, input.Held(1), input.Pressed(1));
    // Race_Task draws the HUD canvas; Race_FallOutTask doesn't.
    if (phase_ != kFallOut) {
        RaceHudState hud[kMaxPlayers];
        MapMarker markers[Cars::kMaxCars + 1];   // and Soccer's ball
        for (int p = 0; p < players_; ++p) Race_FillHud(p, hud[p], markers);
        if (task_ == kTaskSoccer) {
            // Race flag 0x10000 hides the meters (Meter_Draw) and the task's map and canvas: the map with the ball
            // (func_00243398: a yellow marker), then the canvas, which the pause hides (HudCanvas_SetHidden).
            if (soccer_.hud && !meterHidden_) {
                hud_.DrawEventMeters(d_, hud, players_);
                markers[carCount_] = {MapMarker::kBall, soccer_.ball.pos[0], soccer_.ball.pos[2], 0, 0};
                hud_.DrawMap(d_, markers, carCount_ + 1);
            }
            Event_DrawFades(true);
            if (soccer_.hud && !paused_) {
                RaceHud::DrawEventElements(d_, elements_, kElemWin);
                const SoccerScore& s = soccer_.score;
                gfx::AnchorScope anchor(d_, Hud_AnchorAt((float)s.x));
                Hud_DrawScore(d_, s.mode, s.x, s.y, s.s0, s.s1);
                RaceHud::DrawEventElements(d_, elements_ + kElemWin, kElems - kElemWin);
            }
        } else if (task_ != kTaskRace) {
            // Race flag 0x10000 (Rainbow Jump's end) hides the meter and the canvas; Rainbow Jump also hides its
            // canvas while paused (HudCanvas_SetHidden).
            bool hidden = (task_ >= kTaskDrag && game_.hidden) || meterHidden_;
            // Roulette draws its canvas only in the frames it turns the wheel (and on the result's board, where
            // race flag 0x10000 doesn't hide it).
            bool canvas = !hidden && !((task_ == kTaskRainbow || task_ == kTaskGolf) && paused_);
            if (task_ == kTaskGolf && !golf_.drawHud) canvas = false;
            if (task_ == kTaskRoulette) canvas = roulette_.drawCanvas && (!hidden || roulette_.state == 9);
            if (!hidden) hud_.DrawEventMeters(d_, hud, players_);
            // Roulette's and Figure 8's screens (the tasks' children) come under the canvas: WIN! and LOSE go up
            // over Roulette's result board. Figure 8's map is the task's own, before its canvas.
            if (phase_ == kRacing) Race_DrawGameScreens(input);
            if (task_ == kTaskFigure8 && !hidden) Figure8_DrawMap();
            if (task_ == kTaskGolf && !hidden && golf_.drawHud) Golf_DrawMap();
            Event_DrawFades(true);
            if (canvas) {
                // HudCanvas_Draw: the start lights (the Drag Race's and Ski Jumping's countdown), the TIME heading and
                // its row, then the elements.
                if (Race_StartLightsCountdown()) Hud_DrawStartLights(d_, lights_, 320, 64);
                if (eventTime_ >= 0) {
                    gfx::AnchorScope left(d_, gfx::Anchor::Left);
                    Hud_DrawLabel(d_, kLabelTime, 40, 20);
                    Hud_DrawTimeRow(d_, 40, 52, eventTime_, kTimeRunning);
                }
                // The shared elements, the games' own pieces (+0x844C Curling's board, +0x8470 Ski Jumping's
                // distance, +0x8480 Fishing's count), then each player's banners.
                RaceHud::DrawEventElements(d_, elements_, kElemWin);
                Race_DrawGamePieces();
                RaceHud::DrawEventElements(d_, elements_ + kElemWin, kElems - kElemWin);
            }
        } else if (meterHidden_) {
            // The debug pause item 3: race flag 0x10000, no map or meters.
        } else if (players_ == 1) {
            hud_.Draw(d_, hud[0]);
            if (adventureRace_ == 24 && phase_ == kRacing) Race_DrawPitBanners();
        } else {
            hud_.DrawVersus(d_, hud);
        }
    }
    if (task_ != kTaskRace) Event_DrawFades(false);
    if (singleLapFade_ > 0x40)   // SingleLap_FallTask's fade (Fade_SetOverlay)
        d_.Fill(0, 0, gfx::Draw2D::kWidth, gfx::Draw2D::kHeight, (uint32_t)(singleLapFade_ - 0x40) << 24);
    if (phase_ == kEventDone) {
        // The event's end: Sound_MuteCars, black, race flag 8; Race_SetupTask then goes to the mode's results.
        d_.Fill(0, 0, gfx::Draw2D::kWidth, gfx::Draw2D::kHeight, 0x80000000);
        Race_EventResult();
        audio::Sound_StopSong();
        for (CarSounds& c : sounds_) c.Stop();
        phase_ = kDone;
        return kOver;
    }
    for (int& f : finish_)
        if (f >= 0 && !paused_) ++f;   // the zooming element counts its frames as it draws
    // Fade_SetOverlay from the countdown task: black over everything, clearing over the first 0x40 frames.
    const int countdown = frame_ - countdownStart_;
    if (countdown < kCountdownFade && Race_HasCountdown() && countdownStart_ >= 0)
        d_.Fill(0, 0, gfx::Draw2D::kWidth, gfx::Draw2D::kHeight, (uint32_t)((kCountdownFade - countdown) * 2) << 24);
    // The debug readouts the cars printed this frame (the Car Readouts extra), over the race.
    DebugText_Draw(d_);
    RaceMap_SetTuner(false, 0, 0);
    if (paused_ && zoneTable_) {
        // Pause item 6's zone table editor, in the menu's place (its own overlay).
        zoneTable_ = Ai_ZoneTableTask(input);
    } else if (paused_) {
        // The pause's overlay replaces the finish's fade while it's up; then the menu.
        d_.Fill(0, 0, gfx::Draw2D::kWidth, gfx::Draw2D::kHeight, kPauseOverlay);
        RankingRow rows[Cars::kMaxCars];
        const char* name = Save_Record(0).name;
        for (int i = 0; i < carCount_; ++i) {
            const char* driver = cars_[i].record ? cars_[i].record->name : "";
            if (i == 0) driver = name;
            rows[i] = {lap_[i].order, driver, Car_HomeTown(cars_[i])};
        }
        switch (pause_.Update(d_, input, rows, carCount_)) {
        case PauseMenu::kNone:
            break;
        case PauseMenu::kContinue:
            Race_Resume();
            break;
        case PauseMenu::kHideMeter:
            audio::Sound_PlaySe(0x1A);
            meterHidden_ = !meterHidden_;
            pause_.SetMeterHidden(meterHidden_);
            break;
        case PauseMenu::kFullTank:
            audio::Sound_PlaySe(0x1A);
            bodies_[0].fuel = 0x40000;
            break;
        case PauseMenu::kAiDrive:
            aiDriven_[0] = !aiDriven_[0];
            break;
        case PauseMenu::kZoneTable:
            zoneTable_ = true;   // from the next frame
            break;
        case PauseMenu::kRetire:
            // Race_SetupTask ends the race (result 0); the mode loads what follows over this frame.
            audio::Sound_StopSong();
            for (CarSounds& c : sounds_) c.Stop();
            return kRetired;
        }
    } else if (phase_ == kFallOut) {
        // Race_FallOutTask, then Race_SetupTask: black, and the race ends as Retire does (result 0).
        if (fade_ < kFallOutFrames) {
            d_.Fill(0, 0, gfx::Draw2D::kWidth, gfx::Draw2D::kHeight, (uint32_t)fade_++ << 25);
        } else {
            d_.Fill(0, 0, gfx::Draw2D::kWidth, gfx::Draw2D::kHeight, 0x80000000);
            cars_[0].out = true;
            lap_[0].order = -1;
            audio::Sound_StopSong();
            for (CarSounds& c : sounds_) c.Stop();
            return kRetired;
        }
    } else if (phase_ == kFinishFade || phase_ == kReplayFade) {
        d_.Fill(0, 0, gfx::Draw2D::kWidth, gfx::Draw2D::kHeight, (uint32_t)fade_ << 24);
        if (fade_ < kFadeOut) {
            ++fade_;
        } else if (phase_ == kFinishFade) {
            Race_AfterFinish();
        } else {
            audio::Sound_StopSong();
            for (CarSounds& c : sounds_) c.Stop();
            phase_ = kDone;
        }
    }
    return phase_ == kDone ? kOver : kRunning;
}

void Race::Race_DrawView(const gfx::Camera3D& camera, int hidden) {
    Light_SetHighlight(light_, camera.forward);
    course_.Race_DrawView(d_, camera, light_, [&]() {
        carModels_.Cars_Draw(d_, camera, light_, cars_, carCount_, hidden, Course::MirrorsCars(id_));
        objects_.Course_DrawObjects(d_, camera, light_);
    });
    // Race_DrawView's last step, Cars_DrawEffects (func_00225120): each car's jet flame (cars 0-2) and effect, then the
    // particles. The cars' animations run 6 frames apart. With two views this runs for each, as on the PS2.
    for (int i = 0; i < carCount_; ++i) {
        if (cars_[i].out) continue;
        if (i < 3 && bodies_[i].jet && cars_[i].record)
            effects_.Car_DrawJet(d_, camera, bodies_[i], cars_[i].record->body);
        effects_.Car_Effects(d_, camera, bodies_[i], i, frame_, frame_ + 6 * i, paused_);
    }
    effects_.Effects_Draw(d_, camera, frame_ + 6 * Cars::kMaxCars, paused_);
}

}  // namespace game
