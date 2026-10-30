#include "field.h"

#include <math.h>
#include <string.h>

#include "../../audio/sound.h"
#include "../debug_text.h"
#include "../extras.h"
#include "../hud/speedometer.h"
#include "../pad/vibration.h"
#include "../perf_where.h"
#include "../race/race.h"
#include "../save/save_data.h"
#include "../story/messages.h"
#include "../text.h"
#include "../../game_text.inc"   // the game's text, GT_<address>_<length>, made from the ELF

namespace game {

namespace {

#include "field_towns.inc"

const float kPi = 3.14159265f;
const int kTownCount = 22;
const int kUfoTown = 12, kUfoBlock = 22;
const int kMyCity = 9;
const int kFujiBlock = 23;
const int kFuji = 2, kGuardRecord = 0x1D;   // Fuji City and its Guard (T02_00), MoneyBox_Task's car slot 1
const int kCloudHill = 8, kCloudCourse = 0x22;   // Cloud Hill's field is course 0x22, ACTION/A16
const float kCloudFall = 200.0f;                 // below this the car has jumped off
const int kPapaya = 7, kPollepolle = 0x11;       // where it lands: Pollepolle's building
const int kGateCars = 12;                        // the cars A16_GateTask turns back   // Fuji City's block, whose task has the shrine's offering box

// func_002224F8: a cone from each headlight, (+-0.5, 0.6, 1) in the body's frame, to a ring of 21 points (angles
// -pi to pi) of radius r around (+-(0.5 + s), y0) at z ahead. The Lights part (the parts record's +8) sizes them:
// 1 (s 6.5, z 29, r 34.9, y0 -31.8), 2 (4.7, 103, 22.9, -16.1), the rest (4, 50, 26, -22). Each is a tristrip
// (D_00299E54, D_00299458) of program 0x40 vertices whose day and night alphas (colour 2.x, STQ.z) are 0, blended
// and without writing depth (TEST_2 0x7180B): all a beam leaves is the frame's alpha 0 where it's nearer than the
// scene, which keeps those pixels out of the night's darkening.
void Car_DrawBeams(gfx::Draw2D& d, const gfx::Camera3D& camera, const float* bm, int lights) {
    float s = 4, z = 50, r = 26, y0 = -22;
    if (lights == 1) {
        s = 6.5f, z = 29, r = 34.9f, y0 = -31.8f;
    } else if (lights == 2) {
        s = 4.7f, z = 103, r = 22.9f, y0 = -16.1f;
    }
    auto place = [&](const float* p, float* out) {
        for (int k = 0; k < 3; ++k)
            out[k] = bm[4 * k] * p[0] + bm[4 * k + 1] * p[1] + bm[4 * k + 2] * p[2] + bm[4 * k + 3];
    };
    const int kRing = 21;
    gfx::WorldVertex v[2 * (kRing - 1) * 3] = {};
    int n = 0;
    for (int side = 0; side < 2; ++side) {
        float x = side ? -1.0f : 1.0f;
        const float tip[3] = {0.5f * x, 0.6f, 1.0f};
        float apex[3], ring[kRing][3];
        place(tip, apex);
        for (int i = 0; i < kRing; ++i) {
            float a = (i - 10) * kPi / 10;
            const float p[3] = {sinf(a) * r + x * (0.5f + s), cosf(a) * r + y0, z};
            place(p, ring[i]);
        }
        // The strip runs apex, ring 0, apex, ring 1 ...: every other triangle has the apex twice and draws nothing.
        for (int i = 0; i + 1 < kRing; ++i) {
            const float* corner[3] = {ring[i], apex, ring[i + 1]};
            for (int k = 0; k < 3; ++k) memcpy(v[n++].pos, corner[k], sizeof v[0].pos);
        }
    }
    gfx::DrawStyle st;
    st.clearDepth = false;
    st.state.depthWrite = false;
    st.state.alphaOnly = true;
    gfx::DrawWorldTriangles(d, camera, gfx::Rect{0, 0, gfx::Draw2D::kWidth, gfx::Draw2D::kHeight}, v, n, nullptr, st);
}
// The world's fixed point keeps eight blocks each way (the PC wraps the car's; the block ids wrap the same).
const int32_t kWorldMask = 0x0FFFFFFF;
// Fade_InTask after the field's loads (Adventure_Task state 6): black clearing over 32 frames, (32 - t) x 4.
const int kFadeIn = 32;
// The models the loading thread has read are built this long a frame (the PC's streaming).
const double kStreamMs = 4.0;
// func_002182B0: the car starts 20 below its door's way out and is put on the ground there (D_00335710).
const float kStartHeight = -20.0f;
const float kFixed = 20971.52f;   // D_00332C20: fixed point per unit
const int kCoinRadar = 0x2D;      // item 0x2D of kind 15, bought in White Mountain (FieldMap_Task's func_0023D488)

}  // namespace

// func_0023DEB8: a place of My City opens once its resident has moved in (D_002A2B58: the resident of each place,
// bit k of save +0x670), but for Wonder Realty (place 6) and places 23 on, which are always open.
bool MyCity_PlaceOpen(int place) {
    static const int8_t kResident[24] = {0, 1, 2, 3, 6, 8, -1, 10, 11, 12, 13, 14, 17, 18, 19, 20, 21, 22, 23, 24,
                                         25, 4, 5, 0};
    if (place == 6 || place >= 23) return true;
    return Resident_MovedIn(kResident[place]);
}

const FieldTown& Field_Town(int town) { return kTowns[town >= 0 && town < kTownCount ? town : 1]; }

int Field::Field_StartPoint(int town, int place, float* pos, uint16_t* yaw, int32_t* fixed) {
    // func_002182B0: halfway along the door's way out (corners 2 and 3), facing across it, away from the building
    // (atan2 of the side, less a quarter turn).
    const FieldTown& t = Field_Town(town);
    int block = t.block & 0x3F;
    pos[0] = 400;   // town 7's place 0x1F (not a door): the game's fixed start
    pos[1] = 200;
    pos[2] = 600;
    *yaw = 0;
    // (My City's list goes on to 29, the Big Tower's top, a way out with no door: Roberts's pre-code 0x14.)
    int ways = town == kMyCity ? t.places + t.cars + 1 : t.places;
    if (!(town == 7 && place == 0x1F) && place < ways && t.doors[place].corner[4] >= 0) {
        const float* d = t.doors[place].corner;
        float angle = atan2f(d[6] - d[4], d[7] - d[5]) * 32768.0f / kPi;
        *yaw = (uint16_t)(int16_t)(int)(angle - 16384.0f);
        pos[0] = (d[4] + d[6]) * 0.5f;
        pos[1] = kStartHeight;
        pos[2] = (d[5] + d[7]) * 0.5f;
    }
    FieldWorld::Field_BlockOrigin(block, &fixed[0], &fixed[1]);
    fixed[0] += (int32_t)(pos[0] * kFixed);
    fixed[1] += (int32_t)(pos[2] * kFixed);
    return block;
}

void Field::Preload(int town, int place) {
    float pos[3];
    uint16_t yaw;
    int32_t at[2];
    Field_StartPoint(town, place, pos, &yaw, at);
    // Field_Setup: Cloud Hill's course (loader job 0x350000), or the world's blocks round the start.
    world_.Field_SetCourse(town == kCloudHill ? kCloudCourse : -1);
    world_.Field_LoadSky();
    world_.Field_LoadAround(at[0], at[1], FieldWorld::kNoWait);
    // Field_Setup's loader job for the town's cars: their bodies, read meanwhile.
    townCars_.Preload(d_, carModels_, town);
}

void Field::Stream(double budgetMs) { world_.Field_Stream(budgetMs); }

Field::Field(gfx::Draw2D& draw, const std::string& dataDir)
    : d_(draw), dataDir_(dataDir), world_(draw, dataDir), record_(), car_(), body_(), parts_(), light_(),
      townCars_(dataDir), talk_(draw), talking_(false), stampsOpen_(false), talkTown_(-1), talkPlace_(0), town_(1), place_(0), clock_(0),
      frame_(0), doorsShut_(true), menuOpen_(false) {
    menu_.SetDataDir(dataDir);
}

void Field::Start(int town, int place, int32_t clock) {
    town_ = town;
    place_ = place;
    clock_ = clock;
    frame_ = 0;
    const SaveRecord& r = Save_Current();
    const SaveCar& c = r.cars[0];
    record_.paint = c.paint & 0xFFFFFF;
    record_.body = c.body;
    record_.wheelColor = c.wheelColor;
    record_.wheel = c.wheel;
    record_.billboard = c.billboard;
    record_.name = r.name;
    car_ = RaceCar();
    car_.record = &record_;
    car_.town = -1;
    car_.partFlags = c.partFlags;
    parts_ = Car_SetParts(c.parts);
    parts_.flags = c.partFlags;
    Car_SetPlayerFlags(parts_.flags);
    carModels_.Load(d_, dataDir_, &car_, 1);
    effects_.Load(d_);
    coins_.Load(d_, dataDir_);
    // func_00210430's Field_TownCarsTask: the town's cars (My Garage's are My City's).
    townCars_.Start(d_, carModels_, town);
    static bool messages = false;
    if (!messages) messages = Messages_Load(dataDir_);
    talking_ = false;
    stampsOpen_ = false;
    talkTown_ = -1;
    mileMail_ = r.distance / 6000;
    // func_00210430: Light_Setup(0, info+0x22), the course -1 on the field (the port's lights take course 0's), 0x22
    // on Cloud Hill, whose task is A16_GateTask.
    world_.Field_SetCourse(town == kCloudHill ? kCloudCourse : -1);
    Light_Setup(light_, world_.OnCourse() ? kCloudCourse : 0);
    if (world_.OnCourse()) {
        if (!gateLoaded_) courseObjects_.Load(d_, dataDir_, kCloudCourse);
        gateLoaded_ = true;
        courseObjects_.Course_ClearObjects();
    }
    // func_002182B0: the door's way out (Field_StartPoint), then down onto the ground.
    float pos[3];
    uint16_t yaw;
    int32_t at[2];
    int block = Field_StartPoint(town, place, pos, &yaw, at);
    if (Race::Placed(pos, &yaw)) {
        // Development (--place): anywhere, from the start block's corner.
        int32_t ox, oz;
        FieldWorld::Field_BlockOrigin(block, &ox, &oz);
        at[0] = (ox + (int32_t)(pos[0] * kFixed)) & kWorldMask;
        at[1] = (oz + (int32_t)(pos[2] * kFixed)) & kWorldMask;
        block = FieldWorld::Field_BlockAt(at[0], at[1]);
        FieldWorld::Field_BlockOrigin(block, &ox, &oz);
        pos[0] = ((at[0] - ox) & kWorldMask) / kFixed;
        pos[2] = ((at[1] - oz) & kWorldMask) / kFixed;
    }
    world_.Field_LoadSky();
    world_.Field_LoadAround(at[0], at[1], FieldWorld::kWaitAll);
    float p[4] = {pos[0], pos[1], pos[2], 0}, ceiling;
    if (world_.Field_Ground(block, p, &ceiling) >= 0) pos[1] = p[1];
    Car_Place(body_, pos, yaw);
    int32_t ox, oz;
    FieldWorld::Field_BlockOrigin(block, &ox, &oz);
    body_.fixed[0] += ox;   // the block's corner (func_002182B0's +0xA0 and +0xA8); the float position stays
    body_.fixed[2] += oz;
    if (parts_.flags & kPartBigTires) body_.bodyHeight = kBigTiresLift;   // Car_Init
    car_.physics = &body_;
    Car_WorldPosition(body_, car_.pos);
    car_.yaw = (int16_t)body_.yaw * kPi / 32768.0f;
    chase_.Reset(0, false, (parts_.flags & kPartBigTires) != 0);
    map_.Reset();
    objects_.Reset();
    objects_.Load(d_, dataDir_);
    meterState_ = MeterState();
    menuOpen_ = false;
    doorsShut_ = true;   // func_00210430's flag 0x400
    // Car_Init starts the player's engine; the horn is the parts' sound (+0xD).
    sounds_.Start(0x3C + 2 * c.parts[0xD]);
    // func_00210430: the radio on, from the clock.
    Radio_Start(clock_);
}

void Field::Field_SaveDistance() {
    Save_AddDistance(body_.odometer);
    body_.odometer = 0;
}

// --perf: where the car is, as --field and --place would put it back: from the Q's Factory block of Peach Town (or
// Cloud Hill's), the shorter way round the world, a unit up so that it lands on the same deck.
void Field::Perf_Place() const {
    const int town = world_.OnCourse() ? kCloudHill : 1;
    float start[3];
    uint16_t yaw;
    int32_t at[2], origin[2];
    FieldWorld::Field_BlockOrigin(Field_StartPoint(town, 0, start, &yaw, at), &origin[0], &origin[1]);
    PerfWhere& w = Perf_Where();
    w.mode = "field";
    w.id = town;
    for (int k = 0; k < 2; ++k) {
        int32_t d = (body_.fixed[2 * k] - origin[k]) & kWorldMask;
        if (d > kWorldMask / 2) d -= kWorldMask + 1;
        w.pos[2 * k] = d / kFixed;
    }
    w.pos[1] = body_.pos[1] + 1;
    w.yaw = body_.yaw;
    w.clock = clock_;
}

// func_002207D0, after the objects and before block 64 and the cars' effects: nothing from 6:00 to 18:00 (the
// clock 0xD2F0-0x278D0). Otherwise, with the player's headlights on (car +0x1FC), the beams (Car_DrawBeams), then the
// whole frame darkened by (v, v, v) but where the beams are (Draw2D::Darken): v is 0x20, fading in over the 0x800
// frames after 18:00 and out over those before 6:00.
void Field::Field_DrawNight(const gfx::Camera3D& camera) {
    gfx::DrawSourceScope source(d_.GetRenderer(), gfx::DrawSource::Effects);
    const int32_t kDawn = 0xD2F0, kDusk = 0x278D0, kFade = 0x800;
    if (clock_ >= kDawn && clock_ <= kDusk) return;
    if (body_.lights) {
        float m[12], bm[12];
        CarPlace(car_, false, m, bm);
        Car_DrawBeams(d_, camera, bm, Save_Current().cars[0].parts[8]);
    }
    int v = 0x20;
    if (clock_ > kDawn - kFade && clock_ < kDawn) v = (kDawn - clock_) / 64;
    else if (clock_ > kDusk && clock_ < kDusk + kFade) v = (clock_ - kDusk) / 64;
    d_.Darken((uint8_t)v);
}

bool Field::Field_CheckDoors() {
    int block = Block();
    // From town 0, My Garage, once it's been visited (bit 0 of save +0x12DA clear; its scene marks it), else town 1.
    // (On Cloud Hill info+0x54 is block 64: its own doors.)
    block = TownBlock();
    for (int t = Save_Current().townsUnvisited & 1; t < kTownCount; ++t) {
        const FieldTown& town = kTowns[t];
        if (town.block != block) continue;
        for (int i = 0; i < town.places; ++i) {
            if (t == kMyCity && !MyCity_PlaceOpen(i)) continue;
            // The UFO's place 0 opens only by night (the clock outside 0xE484-0x278CF), and not during Q's Rally
            // (event flag 0x50).
            if (t == kUfoTown && i == 0) {
                bool night = clock_ <= 0xE483 || clock_ > 0x278CF;
                if (!night || Save_EventFlag(0x50)) continue;
            }
            const float* c = town.doors[i].corner;
            float x = body_.pos[0], z = body_.pos[2];
            bool inside = true;
            for (int k = 0; k < 4 && inside; ++k) {
                const float* a = c + 2 * k;
                const float* b = c + 2 * ((k + 1) & 3);
                inside = (b[1] - a[1]) * (x - b[0]) - (b[0] - a[0]) * (z - b[1]) > 0.0f;
            }
            if (!inside) continue;
            float p[4] = {(c[4] + c[6]) * 0.5f, kStartHeight, (c[5] + c[7]) * 0.5f, 0}, ceiling;
            world_.Field_Ground(block, p, &ceiling);
            if (body_.pos[1] < p[1] + 1.0f) {
                town_ = t;
                place_ = i;
                // During Q's Rally (race flag 0x1000) only the destination's Q's Factory lets the car in; the others
                // say so each frame it sits in them.
                if (Rally_Running() && !Rally_AtDestination(t, i)) {
                    Text_PrintAt(d_, 130, 226, GT_00328780_36, 6);
                    Text_PrintAt(d_, 130, 250, GT_003287A8_31, 6);
                    return false;
                }
                return true;
            }
        }
    }
    return false;
}

Field::Result Field::Update(const platform::Input& input) {
    if (menuOpen_) {
        // Adventure_Task's state 2, the menu over the stopped field (the menu covers the whole frame).
        menu_.SetCar(body_.fixed[0], body_.fixed[2], !world_.OnCourse());
        FieldMenu::Result r = menu_.Update(d_, input, clock_);
        if (r == FieldMenu::kWarp) {
            Field_SaveDistance();
            // The radio stops for the factory, whose music the room's scene plays (the PS2's warp makes no radio
            // call of its own: the Radio screen may have left it playing; not checked).
            sounds_.Stop();
            audio::Sound_RadioStop();
            return kWarp;
        }
        if (r == FieldMenu::kResume) {
            // The task gone with -1: state 0 again, the field's tasks on (func_00204F18), the cars loud again and the
            // radio retuned to the clock (Radio_Start).
            menuOpen_ = false;
            sounds_.Loud();
            Radio_Start(clock_);
        }
        return kRunning;
    }
    if (stampsOpen_) {
        // State 10's func_0026D008: the stamp book over the whole screen, the field stopped behind it.
        if (clock_++ > kClockHour) clock_ -= kClockHour;
        if (stamps_.Update(d_, input)) {
            stampsOpen_ = false;
            talking_ = false;
            sounds_.Loud();
        }
        return kRunning;
    }
    // Adventure_Task's state 0 (and 8, a talk, which runs the clock too): the clock a frame at a time round the
    // hour, the time of day by the car's height; and Q's Rally's timer.
    // The Max Money cheat: the money held at the most Money_Pay gives.
    if (Extras_On(kCheatMaxMoney)) Save_Current().money = 999999;
    if (!talking_ && Extras_On(kExtraClockScrub)) {
        // The Clock Scrub extra: state 0 with the debug byte (0x00210CF4, while race flag 0x8000 is clear), before
        // the frame's tick, as Race_SetupTask does on the Endurance Run: pad 1's L2 held (+0x2C & 1) moves the clock
        // on 0x168 frames (6 s), round the hour; then L1 held (& 4) back as much, from under 0 to the old + 0x34A58.
        // Both held do both. On the PC L2 and L1 are Wing Down and Wing Up.
        const uint32_t held = input.Held(0);
        if (held & platform::kWingDown) {
            clock_ += 0x168;
            if (clock_ > kClockHour) clock_ -= kClockHour;
        }
        if (held & platform::kWingUp) {
            const int32_t old = clock_;
            clock_ -= 0x168;
            if (clock_ < 0) clock_ = old + 0x34A58;
        }
    }
    if (clock_++ > kClockHour) clock_ -= kClockHour;
    Rally_Tick();
    // The blocks near the car: the threads read those it nears, and the one it's in must be there for its ground.
    world_.Field_LoadAround(body_.fixed[0], body_.fixed[2]);
    int block = Block();
    FieldGround ground(world_, body_.fixed);
    // The camera's task runs before the car's, so it frames the car where the last frame left it. It starts as
    // func_0021EC58, which looks the car round and then lets the doors work (flag 0x400 cleared).
    if (doorsShut_) {
        doorsShut_ = !chase_.Camera_FieldStartTask(body_, car_.pos, frame_);
    } else {
        bool look = !talking_ && (input.Pressed(0) & Car_CameraButton(0)) != 0;
        chase_.Camera_ChaseTask(body_, car_.pos, ground, look, false);
    }
    // A talk (info+0x28 flag 0x40000) stops Car_Update and Cars_Collide for every car. Their headlights go by the
    // clock.
    Car_SetClock(clock_);
    uint32_t bits = 0;
    if (!talking_) {
        bits = Car_ReadPad(input, 0);
        // Car flag 0x800 (the offering box sliding back, Cloud Hill's gate opening): braking held.
        if (objects_.CarHeld() || (world_.OnCourse() && courseObjects_.HoldPlayer()))
            bits = (bits & 0xFF00) | kCarBrake;
        if (doorsShut_) {
            // Car_Update gives the player's car flag 0x100 while the field's 0x400 is up: Car_Input keeps only the
            // steering, and for the car's first 0x40 frames holds its wheels' speed between 0x1000 and 0x1800, so
            // the car rolls out of the door by itself and coasts to a stop.
            bits &= 0xFF00;
            if (frame_ < 0x40) {
                if (body_.wheel < 0x1000) bits |= kCarAccel;
                else if (body_.wheel > 0x1800) bits |= kCarBrake;
            }
        }
        sounds_.Car_Horn(Car_HornHeld(input, 0));
        if (input.Pressed(0) & Car_MapButton(0)) map_.Toggle();
        int8_t water = body_.water;
        // The debug byte's Debug_PrintCar (the Car Readouts extra; in the field the vectors are off) and ride
        // height (Height Keys).
        if (Extras_On(kExtraReadouts)) {
            Car_SetDebugPrint(DebugText_Printf, false);
            int b = Block();
            DebugText_Printf(32, 24, GT_0032FB18_12, b / 16, (b >> 2) & 3, ((b & 1) << 1) | ((b >> 1) & 1));
        }
        uint32_t held = Extras_On(kExtraHeightKeys) ? input.Held(0) : 0;
        Car_SetHeightKey((held & platform::kUp) ? 1 : (held & platform::kDown) ? -1 : 0);
        Car_SetCheats(Car_PlayerCheats());
        Car_Update(body_, parts_, ground, bits, true);
        Car_SetDebugPrint(nullptr);
        Car_SetHeightKey(0);
        Car_SetCheats(0);
        body_.fixed[0] &= kWorldMask;
        body_.fixed[2] &= kWorldMask;
        if (car_.partFlags != parts_.flags) {
            // The Flight Wing out or folded: the car's parts record is the save's car 0.
            car_.partFlags = Save_Current().cars[0].partFlags = parts_.flags;
            Car_SetPlayerFlags(parts_.flags);
        }
        if (water == 0 && body_.water < 0) audio::Sound_PlaySe(0x28);   // Car_Ground's splash
    }
    sounds_.Car_Sound(body_, bits, false, false);
    sounds_.Car_Jet(body_.jetSound, false);
    if (body_.shake && !talking_) Pad_Vibrate(0, body_.shake, body_.shakeStrength);
    Car_WorldPosition(body_, car_.pos);
    car_.yaw = (int16_t)body_.yaw * kPi / 32768.0f;
    Light_TimeOfDay(light_, clock_, body_.pos[1]);
    if (world_.OnCourse() && !talking_ && body_.pos[1] < kCloudFall) {
        // Adventure_Task's state 0 off the world (0x00210DE8): the car below y 200 has jumped off Cloud Hill, and
        // lands in Pollepolle's building on Papaya Island (7, 0x11), as if driven in at his door.
        Field_SaveDistance();
        sounds_.Stop();
        audio::Sound_RadioStop();
        town_ = kPapaya;
        place_ = kPollepolle;
        return kEnter;
    }

    // Field_TownCarsTask and the town cars' own tasks, then Cars_Collide's: a bump of the player's car into a town
    // car that isn't standing after the last one is a talk (Cars_BumpSound's field branch). Adventure_Task reads it
    // once the doors are open (flag 0x400 down).
    townCars_.Update(d_, carModels_, world_, clock_, talking_);
    // Cloud Hill's course task, A16_GateTask, over the player's car and the town cars (D_0177AC50's first 12).
    if (world_.OnCourse()) {
        CarBody bodies[kGateCars];
        bool out[kGateCars] = {};
        int n = 0;
        bodies[n++] = body_;
        for (int i = 0; i < townCars_.Count() && n < kGateCars; ++i) bodies[n++] = townCars_.Body(i);
        courseObjects_.Update(CourseCars{bodies, out, n, false, talking_});
        body_ = bodies[0];
        for (int i = 1; i < n; ++i) townCars_.Body(i - 1) = bodies[i];
        car_.physics = &body_;
    }
    // Field_ObjectsTask's block tasks: block 0x2B's Peach_FallTask, by the car's speed across (Vec_SpeedKmh), and
    // block 23's offering box (func_00240CD8) over the player's car and the town cars.
    {
        float vx = body_.vel[0] / 32768.0f, vz = body_.vel[2] / 32768.0f;
        objects_.Peach_Step(world_, Block(), body_.pos, sqrtf(vx * vx + vz * vz) * 216.0f);
        CarBody* box[Cars::kMaxCars];
        int n = 0;
        box[n++] = &body_;
        for (int i = 0; i < townCars_.Count() && n < Cars::kMaxCars; ++i) box[n++] = &townCars_.Body(i);
        // Car slot 1 is the town's first car: in Fuji City the Guard (T02_00, record 0x1D) unless he's gone home.
        const bool guard = townCars_.Town() == kFuji && townCars_.Count() && townCars_.Index(0) == kGuardRecord;
        objects_.MoneyBox_Step(world_.Loaded(kFujiBlock) ? Block() : -1, clock_, box, n,
                               guard ? &townCars_.Body(0) : nullptr);
        // Block 22's UFO task, then Field_ObjectsTask's tide and texture copies.
        objects_.Ufo_Step(world_.Loaded(kUfoBlock), clock_);
        objects_.Field_Scrolls(d_, world_, frame_);
    }
    if (!talking_) {
        CarBody* all[Cars::kMaxCars];
        int n = 0;
        all[n++] = &body_;
        for (int i = 0; i < townCars_.Count() && n < Cars::kMaxCars; ++i) all[n++] = &townCars_.Body(i);
        CarContact contacts[Cars::kMaxCars * 2];
        int touched = Cars_Collide(all, n, contacts, Cars::kMaxCars * 2);
        if (touched > Cars::kMaxCars * 2) touched = Cars::kMaxCars * 2;
        for (int k = 0; k < touched; ++k) {
            if (contacts[k].a != 0 || contacts[k].b < 1) continue;
            int town, place;
            if (townCars_.Cars_Bumped(contacts[k].b - 1, &town, &place) && talkTown_ < 0) {
                talkTown_ = town;
                talkPlace_ = place;
                Car_Stop(body_);
            }
        }
    }

    // The view from the car's block (it may have just driven into the next one); the blocks around it come in over
    // the next frames, a few milliseconds a frame.
    if (Block() != block) {
        world_.Field_LoadAround(body_.fixed[0], body_.fixed[2]);
        block = Block();
    }
    world_.Field_Stream(kStreamMs);
    Perf_Place();
    // Past the blocks the frame shows the background colour (pad +0x50), which the time of day sets.
    d_.Fill(0, 0, gfx::Draw2D::kWidth, gfx::Draw2D::kHeight, light_.background | 0x80000000u);
    gfx::Camera3D camera = chase_.View();
    Race::EyeView(&camera);
    Light_SetHighlight(light_, camera.forward);
    // The player's car (slot 0) and the town cars (slots 1 on).
    RaceCar cars[Cars::kMaxCars];
    int carCount = 1;
    cars[0] = car_;
    if (townCars_.Count() > 0) {
        townCars_.Cars_Place(block, cars + 1);
        carCount += townCars_.Count();
    }
    // After the cars, func_00220938's coins (func_00241D38) and objects: func_0025D028's moving scenery.
    world_.Field_DrawView(d_, camera, light_, block, [&](float reach) {
        carModels_.Cars_Draw(d_, camera, light_, cars, carCount, chase_.Inside() ? 0 : -1);
        if (world_.OnCourse()) {
            courseObjects_.Course_DrawObjects(d_, camera, light_);   // Cloud Hill's view: the gate, no coins
        } else {
            coins_.Coins_Draw(d_, camera, light_, world_, block, frame_, reach);
            objects_.Field_DrawObjects(d_, camera, light_, world_, block, frame_, reach, body_.pos, clock_);
        }
        Field_DrawNight(camera);
    });
    // Cars_DrawEffects (func_00225120, from func_00220938 here too): the jet's flame while it fires, then the effect.
    if (body_.jet) effects_.Car_DrawJet(d_, camera, body_, record_.body);
    effects_.Car_Effects(d_, camera, body_, 0, frame_, frame_, false);
    effects_.Effects_Draw(d_, camera, frame_ + 6, false);
    DebugText_Draw(d_);   // the Car Readouts extra's lines
    if (!talking_) {
        // func_00242FE8: the map in the bottom left, with the town cars' green squares and, with the Coin Radar
        // (item 0x2D of kind 15), the coins still out in yellow, every other 32 frames; then Meter_Draw: the
        // player's meter with the field's clock. A talk's flag 0x10000 hides both.
        markers_.clear();
        townCars_.Cars_MapDots(block, markers_);
        if (Save_HasItem(15, kCoinRadar) && (frame_ & 0x20)) FieldCoins::Coins_MapDots(world_, block, markers_);
        // (On a wide screen the map keeps to the left edge and the meter to the right: gfx::Draw2D's anchors.)
        {
            gfx::AnchorScope left(d_, gfx::Anchor::Left);
            map_.Draw(d_, world_, block, body_.pos, car_.yaw, clock_, markers_, frame_);
        }
        if (Rally_Running() && !world_.OnCourse()) FieldMap::Map_DrawRally(d_);
        MeterReadout meter = Meter_Read(body_);
        meter.finsPart = (parts_.flags & kPartFins) != 0;
        meter.clock = clock_;
        meter.timeOfDay = clock_;
        gfx::AnchorScope right(d_, gfx::Anchor::Right);
        Meter_Draw(d_, Save_Current().cars[0].parts[14], meter, meterState_, frame_);
    }
    if (frame_ < kFadeIn) d_.Fill(0, 0, gfx::Draw2D::kWidth, gfx::Draw2D::kHeight, (uint32_t)((kFadeIn - frame_) * 4) << 24);
    ++frame_;

    if (talking_) {
        // State 8: the talk over the stopped field. Result 11 (code 0x14) goes to the place the talk named, as a
        // door would; any other is func_0026D008's stamp book for the stamps the talk queued (if any), then state
        // 10: the flags down, the cars loud again, state 0.
        talk_.SetClock(clock_);
        int r = talk_.Update(input);
        if (talk_.TakeTeamChanged()) townCars_.Cars_Refresh();
        if (talk_.TakeCarChanged()) {
            const SaveCar& c = Save_Current().cars[0];
            car_.partFlags = parts_.flags = c.partFlags;
            Car_SetPlayerFlags(parts_.flags);
            chase_.SetBigTires((parts_.flags & kPartBigTires) != 0);
        }
        if (r == 11) {
            talk_.Destination(&town_, &place_);
            talking_ = false;
            sounds_.Stop();
            audio::Sound_RadioStop();
            return kEnter;
        }
        if (r) {
            if (stamps_.Start(true)) {
                stampsOpen_ = true;
            } else {
                talking_ = false;
                sounds_.Loud();
            }
        }
        return kRunning;
    }
    if (!doorsShut_ && talkTown_ >= 0) {
        // State 0's result 2: Message_Task on D_002A7ED0[town][place], flags 0x50000, the cars quiet.
        const MessageTable* table = Messages_Table(talkTown_, talkPlace_);
        if (table) {
            Field_SaveDistance();
            talk_.SetClock(clock_);
            talk_.Start(*table, talkTown_, talkPlace_);
            talking_ = true;
            sounds_.Mute();
        }
        talkTown_ = -1;
        return kRunning;
    }

    // func_00241BA8, after the car: the coins it drives through.
    coins_.Field_CheckCoins(TownBlock(), body_.pos);
    // Adventure_Task's state 0, with the doors working: every 6000 m driven (the save's distance and the car's
    // odometer) a mileage mail (func_0023DFC0) once the player has the garage (stamp 40), SE 0x31E. (Seeded by info+0's low
    // word, the frames since boot: 0x00210F04.)
    if (!doorsShut_ && mileMail_ < (Save_Current().distance + body_.odometer / 32768) / 6000) {
        if (Stamp_Earned(40)) {
            Mail_SendRandom(0x50, 0x2B, (int32_t)Info_Frames());
            audio::Sound_PlaySe(0x31E);
        }
        ++mileMail_;
    }

    if (!doorsShut_ && Field_CheckDoors()) {
        // State 0's door: the cars quiet and the radio off (Sound_RadioStop).
        Field_SaveDistance();
        sounds_.Stop();
        audio::Sound_RadioStop();
        return kEnter;
    }
    if (input.Pressed(0) & platform::kStart) {
        // State 0's Start: the cars quiet (Sound_MuteCars), the pause sound, the radio off (Sound_RadioStop), the
        // field's tasks stopped (func_00204EE0) and the menu's task (FieldMenu_Task), state 2.
        sounds_.Mute();
        audio::Sound_PlaySe(0x1F);
        audio::Sound_RadioStop();
        Field_SaveDistance();
        menu_.Open(Save_Current().townsUnvisited);
        menuOpen_ = true;
    }
    return kRunning;
}

}  // namespace game
