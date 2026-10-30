#include "room_scene.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "../../audio/sound.h"
#include "../race/race.h"
#include "../save/save_data.h"
#include "messages.h"
#include "photo.h"
#include "../ui/window.h"

namespace game {

namespace {

const float kPi = 3.14159265f;
const float kAngle = kPi / 32768.0f;   // a 16-bit angle in radians
const gfx::Rect kFrame = {0, 0, gfx::Draw2D::kWidth, gfx::Draw2D::kHeight};
std::string g_dataDir;   // for Room_Open's look at the talks

// The room's camera (D_0029F190), as Camera_BuildMatrices takes it: the eye at (2.2, 1.1, -45) in the frame turned by
// the pitch about x and then the yaw about y, looking at the origin; focal length 3564.
const float kCameraEye[3] = {2.2f, 1.1f, -45.0f};
const float kCameraFocal = 3564.0f;
const int kCameraPitch = 0x1100, kCameraYaw = -0x1D80;

// Where the cars stand (D_0029F320): x, y, z and the yaw (radians). The player's car (slot 0), the person (slot 3,
// D_017524B0 + 0x90) and a teammate's car (slot 2, D_017525D0) where it waits to back onto the turntable.
const float kYouPlace[4] = {9.06f, 0.0f, 1.47f, 0.0f};
const float kPersonPlace[4] = {-1.75f, 0.0f, 2.1f, 1.963f};
const float kMatePlace[4] = {7.8f, 0.0f, 5.2f, 0.0f};
// The cars' speed and heading to begin with (D_00333680, copied to +0x8C): the player's, then the teammate's
// (D_00333688: facing -x).
const int16_t kYouSpeed = 0x0834, kYouHeading = (int16_t)0xC271;
const int16_t kMateSpeed = 0x0992, kMateHeading = -0x4000;

// The floor (D_0029F1B0): a 20 x 20 quad at y 0 from (-10, 10) to (10, -10), its s from 0.1 to 1.1 and t from 0.2 to
// 2.2 of the 1024 x 512 texture page, with CLAMP's region repeat on the picture's 64 x 64 tile at (512, 320).
const float kFloorTexel0 = 102.4f, kTexelsPerUnit = 51.2f;
const float kTileU = 512.0f, kTileV = 320.0f, kTile = 64.0f;
// The two decals, centred on (1.2, 0, 5.2) (D_00332E08 and D_00332E0C): x, y, z, width, depth and the picture's
// rectangle (texels). The first (D_0029F230) turns with +0x26 (the garage's turntable); the second (D_0029F1F0)
// doesn't.
struct Decal {
    float x, y, z, w, d;
    float u0, v0, u1, v1;
};
const Decal kTurntable = {-1.875f, 0.01f, 1.875f, 3.75f, -3.75f, 576, 256, 640, 320};
const Decal kTurntableTop = {-1.875f, 0.02f, 1.875f, 3.75f, -3.75f, 576, 320, 640, 384};
const float kDecalX = 1.2f, kDecalZ = 5.2f;

// The cars' paths (D_0029F300): steps of {frames, speed change (its sign: +-0x23 a frame), steer}.
struct PathStep {
    int16_t frames;
    int8_t accel, steer;
};
const PathStep kPath0[] = {{60, -2, 0}, {10, 0, 0}, {30, 1, 1}, {32, 0, 1}, {20, -2, 1}, {10, -2, 0}, {0, 0, 0}};
const PathStep kPath1[] = {{50, 1, 0}, {50, -2, 0}, {0, 0, 0}};
const PathStep kPath2[] = {{50, -1, 0}, {50, 2, 0}, {0, 0, 0}};
const PathStep kPath3[] = {{10, 1, 0}, {25, 1, -1}, {35, 0, -1}, {50, 1, 0}, {0, 0, 0}};
const PathStep kPath4[] = {{70, -2, 0}, {0, 0, 0}};
const PathStep kPath5[] = {{70, 1, 0}, {0, 0, 0}};
const PathStep kPath6[] = {{4, 0, 0}, {60, -2, 0}, {0, 0, 0}};
const PathStep kPath7[] = {{20, -3, -4}, {20, 0, -3}, {20, 2, 0}, {40, 1, 2}, {20, 1, 0}, {0, 0, 0}};
const PathStep* const kPaths[8] = {kPath0, kPath1, kPath2, kPath3, kPath4, kPath5, kPath6, kPath7};

// The room's file (D_0029B290 by town): town 0 the garage, town n SHOP/T(n-1).
std::string RoomFile(int town) {
    if (town == 0) return "SHOP/GARAGE.BIN";
    char name[32];
    sprintf(name, "SHOP/T%02d.BIN", town - 1);
    return name;
}

float WrapAngle(float a) {   // func_00207F38: back into -pi..pi
    while (a > kPi) a -= 2 * kPi;
    while (a < -kPi) a += 2 * kPi;
    return a;
}

// A world point in the camera's view space.
void ToView(const gfx::Camera3D& c, const float* p, float* v) {
    float d[3] = {p[0] - c.eye[0], p[1] - c.eye[1], p[2] - c.eye[2]};
    v[0] = d[0] * c.right[0] + d[1] * c.right[1] + d[2] * c.right[2];
    v[1] = d[0] * c.up[0] + d[1] * c.up[1] + d[2] * c.up[2];
    v[2] = d[0] * c.forward[0] + d[1] * c.forward[1] + d[2] * c.forward[2];
}

// A projected vertex with texture coordinates (s, t) (0-1 of the texture), the colour as is (TFX decal).
gfx::Vertex Project(const gfx::Camera3D& c, const float* p, float s, float t) {
    float v[3];
    ToView(c, p, v);
    float q = 1.0f / v[2];
    gfx::Vertex o;
    o.x = c.centerX + c.focalX * v[0] * q;
    o.y = c.centerY - c.focalY * v[1] * q;
    o.z = q;
    o.color = gfx::Draw2D::Color(0x80808080);
    o.s = s * q;
    o.t = t * q;
    o.q = q;
    o.fog = 1;
    return o;
}

}  // namespace

int RoomScene::Room_SceneKind(int town, int place) {
    // func_0022AD70: 1 a Parts Shop (place 1), 2 a Body Shop (place 2), 3 a Paint Shop (place 3), 4 My City's
    // Recycle Shop, 5 its Snow Bank.
    if (town == 9 && place == 5) return 4;
    if (town == 9 && place == 7) return 5;
    if (place == 1 && town <= 9 && town != 4) return 1;
    if (place == 2 && town <= 9 && town != 4 && town != 5) return 2;
    if (place == 3 && (town == 1 || town == 2 || town == 3 || town == 6 || town == 8 || town == 9)) return 3;
    return 0;
}

bool RoomScene::Room_Open(int town, int place) {
    if (place == 0 && town >= 1 && town <= 9) return true;   // Q's Factory
    // Any other place with a talk, of the towns 1-9 or the stops (10-21), and a shop screen (0x13) only in a shop
    // (Room_SceneKind 1, 2 or 4): every table passes. (My City's doors open by Field_CheckDoors' rule.) A stop's
    // building is its place 0.
    if (town < 1 || town > 21 || place < 0 || (place == 0 && town <= 9)) return false;
    static bool loaded = false;
    if (!loaded) loaded = Messages_Load(g_dataDir);
    const MessageTable* table = Messages_Table(town, place);
    if (!table || table->size() < 2) return false;
    uint32_t codes = MessageTask::Message_EndCodes(*table);
    const int kind = Room_SceneKind(town, place);
    return !(codes & 1u << 0x13) || kind == 1 || kind == 2 || kind == 4;
}

// The room's picture: the file's 640 x 384 textures are its rooms in order.
const gfx::Texture* RoomScene::Room_Picture(gfx::Draw2D& d, int town, int place) {
    const std::string file = RoomFile(town);
    for (int n = 0, room = 0; n < 256; ++n) {
        const gfx::Texture& t = d.Get(file, n);
        if (!t.id) break;
        if (t.width == 640 && room++ == place) return &t;
    }
    return nullptr;
}

RoomScene::RoomScene(gfx::Draw2D& draw, const std::string& dataDir)
    : d_(draw), dataDir_((g_dataDir = dataDir)), town_(0), place_(0), picture_(nullptr), mateCar_(1), team_(),
      message_(draw), parts_(draw, dataDir, cars_), inParts_(false), paint_(draw), age_(0), carState_(0), request_(0),
      turn_(0), fade_(0), ending_(-1), talked_(false), parentResult_(0) {}

// The team's car `car` as the save has it now: paint, body and its driver's name (the player's, or the teammate's
// driver record, Save_GetCarName).
void RoomScene::Room_SetTeamCar(int car) {
    const SaveRecord& r = Save_Current();
    team_[car].paint = r.cars[car].paint & 0xFFFFFF;
    team_[car].body = r.cars[car].body;
    team_[car].wheelColor = r.cars[car].wheelColor;
    team_[car].wheel = r.cars[car].wheel;
    team_[car].billboard = r.cars[car].billboard;
    team_[car].name = car == 0 ? r.name : Car_DriverRecordName(r.teammates[car - 1][0], r.teammates[car - 1][1]);
}

void RoomScene::Room_SetCarMatrix(RoomCar& c, float yaw) {
    float s = sinf(yaw), co = cosf(yaw);
    const float right[3] = {co, 0, -s}, up[3] = {0, 1, 0}, fwd[3] = {s, 0, co};
    for (int k = 0; k < 3; ++k) {
        c.body.m[0][k] = right[k];
        c.body.m[1][k] = up[k];
        c.body.m[2][k] = fwd[k];
    }
    c.car.yaw = yaw;
}

void RoomScene::Room_PlaceCar(RoomCar& c, const float* place, float yaw, int16_t speed, int16_t heading) {
    memset(&c.body, 0, sizeof c.body);
    for (int k = 0; k < 3; ++k) {
        c.body.body[k][k] = 1.0f;
        c.car.pos[k] = c.body.pos[k] = place[k];
    }
    // The shadow lies on the floor (y 0) and the car takes the room's colour set 0.
    for (int i = 0; i < 7; ++i) c.body.probe[i][1] = c.body.probe[i][3] = 0.0f;
    c.speed = speed;
    c.heading = heading;
    Room_SetCarMatrix(c, yaw);
    c.car.physics = &c.body;
}

void RoomScene::Start(int town, int place, const CarRecord* you, uint16_t partFlags) {
    static bool loaded = false;
    if (!loaded) loaded = Messages_Load(dataDir_);
    town_ = town;
    place_ = place;
    endingOut_ = -1;
    picture_ = Room_Picture(d_, town, place);
    // Camera_BuildMatrices: the columns of Ry(yaw) Rx(pitch) are the camera's axes.
    const float a = kCameraPitch * kAngle, b = kCameraYaw * kAngle;
    const float ca = cosf(a), sa = sinf(a), cb = cosf(b), sb = sinf(b);
    const float right[3] = {cb, 0, -sb}, up[3] = {sb * sa, ca, cb * sa}, fwd[3] = {sb * ca, -sa, cb * ca};
    for (int k = 0; k < 3; ++k) {
        camera_.right[k] = right[k];
        camera_.up[k] = up[k];
        camera_.forward[k] = fwd[k];
        camera_.eye[k] = kCameraEye[0] * right[k] + kCameraEye[1] * up[k] + kCameraEye[2] * fwd[k];
    }
    camera_.focalX = kCameraFocal;
    camera_.focalY = kCameraFocal * 0.94f;
    camera_.centerX = gfx::Draw2D::kWidth / 2.0f;
    camera_.centerY = gfx::Draw2D::kHeight / 2.0f;
    Light_Setup(light_, 1);
    Light_SetHighlight(light_, camera_.forward);

    // Q's Factory and the Paint Shops show the team's cars as the save has them, so the parts fitted and the paint
    // show at once.
    // Shop_SceneTask's setup (0x0022B2CC): the player's Flight Wing folded away (part flag 8 becomes 4) on the save's
    // car, so it doesn't stay out after the field or a race.
    uint16_t& wing = Save_Current().cars[0].partFlags;
    if (wing & kPartWingOut) wing = (uint16_t)((wing & ~kPartWingOut) | kPartWingFolded);
    if (partFlags & kPartWingOut) partFlags = (uint16_t)((partFlags & ~kPartWingOut) | kPartWingFolded);
    const bool team = Room_HasTeam();
    if (team)
        for (int i = 0; i < 3; ++i) Room_SetTeamCar(i);
    you_.car = RaceCar();
    you_.car.record = team ? &team_[0] : you;
    you_.car.town = -1;
    you_.car.partFlags = team ? Save_Current().cars[0].partFlags : partFlags;
    Room_PlaceCar(you_, kYouPlace, kYouPlace[3], kYouSpeed, kYouHeading);
    person_.car = RaceCar();
    person_.car.record = Car_DriverRecord(town, place);
    person_.car.town = town;
    Room_PlaceCar(person_, kPersonPlace, kPersonPlace[3], 0, 0);
    mate_.car = RaceCar();
    mate_.car.record = &team_[1];
    mate_.car.town = -1;
    mateCar_ = 1;
    Room_PlaceCar(mate_, kMatePlace, kMatePlace[3], kMateSpeed, kMateHeading);
    Room_LoadCars();

    // The song and the cars' first state (+0x27). Q's Factory (place 0 of a town below 10) has song 0x0A0B0101 and
    // state 0: the player's car drives in on path 0 and onto the turntable. The shops and the bank (Room_SceneKind
    // not 0) have song 0x0A0B0103 and state 0xE, the car driving in on path 6 to stand there, but a Paint Shop's
    // drives onto the turntable (0). Elsewhere it's 0xE too, with song 0x0A0B0102 (the president's office its own,
    // ROOM_1 entry 4).
    audio::Sound_PlaySe(0x32);
    const int kind = Room_SceneKind(town, place);
    if (town < 10 && place == 0) {
        carState_ = 0;
        audio::Sound_LoadSong(0x0A0B0101);
    } else if (kind != 0) {
        carState_ = kind == 3 ? 0 : 0xE;
        audio::Sound_LoadSong(0x0A0B0103);
    } else {
        carState_ = 0xE;
        audio::Sound_LoadSong(town == 8 && place == 9 ? 0x0A0B0104 : 0x0A0B0102);
    }
    audio::Sound_StartSong();
    request_ = 0;
    turn_ = 0;
    age_ = 0;
    fade_ = 0;
    ending_ = -1;
    talked_ = false;
    parentResult_ = 0;
    inParts_ = false;
    inShop_ = false;
    paint_.Stop();
    inPaint_ = false;
    message_.blip = 0;
}

bool RoomScene::Room_HasTeam() const { return (town_ < 10 && place_ == 0) || Room_SceneKind(town_, place_) == 3; }

// The models of the cars the scene can show: the player's, the person's and, in Q's Factory and the Paint Shops,
// the teammates'.
void RoomScene::Room_LoadCars() {
    RaceCar cars[5] = {you_.car, person_.car};
    int n = person_.car.record ? 2 : 1;
    const SaveRecord& r = Save_Current();
    if (Room_HasTeam()) {
        for (int t = 0; t < 2; ++t) {
            if (!r.teammates[t][0]) continue;
            cars[n] = mate_.car;
            cars[n++].record = &team_[t + 1];
        }
    }
    cars_.Load(d_, dataDir_, cars, n);
}

// func_0022A408: the car on step `frame` of `path`. Each frame of a step changes the speed by 0x23 (by the step's
// sign), turns the heading by speed x steer / 4 and the front wheels towards the steer (0.1 a frame, up to 0.628;
// back to straight without one), then moves the car speed / 12800 along its heading and turns its wheels. False
// once the path is over.
bool RoomScene::Room_CarPath(RoomCar& c, int path, int frame) {
    const PathStep* s = kPaths[path];
    int t = frame;
    while (s->frames && t >= s->frames) {
        t -= s->frames;
        ++s;
    }
    if (!s->frames) return false;
    if (s->accel > 0) c.speed = (int16_t)(c.speed + 0x23);
    if (s->accel < 0) c.speed = (int16_t)(c.speed - 0x23);
    float& steer = c.body.wheelAngle[1];
    if (s->steer == 0) {
        if (steer < -0.1f) steer += 0.1f;
        else if (steer > 0.1f) steer -= 0.1f;
        else steer = 0.0f;
    } else {
        c.heading = (int16_t)(c.heading + c.speed * s->steer / 4);
        if (s->steer > 0) {
            if (steer < 0.6283185f) steer += 0.1f;
        } else if (steer > -0.6283185f) {
            steer -= 0.1f;
        }
    }
    // The step's accel byte & 2 is the car's lights for the frame: the brake lights (a step of -2 or -1, slowing).
    c.car.lights = (uint8_t)(s->accel & kLightsBrake);
    const float yaw = c.heading * kAngle, v = c.speed / 12800.0f;
    Room_SetCarMatrix(c, yaw);
    c.car.pos[0] = c.body.pos[0] += sinf(yaw) * v;
    c.car.pos[2] = c.body.pos[2] += cosf(yaw) * v;
    c.body.wheelAngle[2] = WrapAngle(c.body.wheelAngle[2] + v / 0.38f);
    c.body.wheelAngle[0] = WrapAngle(c.body.wheelAngle[0] + v / 0.33f);
    return true;
}

// A turntable step of func_0022A6B8: the heading's high byte one step (256) nearer `facing` (0x80: facing the
// camera), the turntable's angle (+0x26) along with it; the car turns in place. True once it's there.
bool RoomScene::Room_TurnCar(RoomCar& c, int facing) {
    int h = ((uint32_t)(c.heading < 0 ? c.heading + 0xFF : c.heading) >> 8) & 0xFF;
    if (h < facing) {
        ++h;
        turn_ = (turn_ + 1) & 0xFF;
    } else if (h > facing) {
        --h;
        turn_ = (turn_ - 1) & 0xFF;
    }
    c.heading = (int16_t)(h << 8);
    Room_SetCarMatrix(c, c.heading * kAngle);
    return h == facing;
}

// Shop_SceneTask's state 2 (the parts window or the paint screen open), with pad 1 in analog mode (info+0x0C == 7):
// a stick turns the car on the turntable, the player's in car state 0xD or the teammate's in 5. Its byte (+0x3E,
// pad 1's left stick X) through D_0029AD80 gives -3..3, halved towards 0: the turntable's angle (+0x26) and the
// heading's high byte go by -1, 0 or 1 a frame, and the car is turned in place. On the PC the left stick is the
// d-pad's Left and Right, which the screens use, so the right stick turns it; a controller only.
void RoomScene::Room_StickTurn(const platform::Input& input) {
    static const int8_t kStickSteps[16] = {-3, -2, -2, -1, -1, 0, 0, 0, 0, 0, 0, 1, 1, 2, 2, 3};   // D_0029AD80
    if (!(inParts_ || inPaint_) || !input.Analog(0)) return;
    RoomCar* c = carState_ == 0xD ? &you_ : carState_ == 5 ? &mate_ : nullptr;
    if (!c) return;
    const int step = kStickSteps[input.Stick(0, 0) >> 4] / 2;
    if (!step) return;
    turn_ = (turn_ + step) & 0xFF;
    const int h = (c->heading < 0 ? c->heading + 0xFF : c->heading) >> 8;
    c->heading = (int16_t)((h + step) << 8);
    Room_SetCarMatrix(*c, c->heading * kAngle);
}

// func_0022ABA0: what the scene asks of the cars (+0x02), each request waiting for a state that takes it.
// - 1, the player's car (its parts, or the parts window left): on the turntable (0xD) to 0xC, which turns it to face
//   the camera (it does already); with a teammate's car there (4 or 5), to 6, which swaps them back. SE 0x35.
// - 2 or 3, teammate 1 or 2: from 0xD, that car (+0x20) waits in slot 2 at its place, facing +z, with its own speed
//   and heading (D_00333688), and state 2 starts the swap.
// - 4, the talk ended plainly: the player's car drives out, from the turntable (0xD) to 9, SE 0x34; from standing
//   (0xF) to 0x10, SE 0x34; a teammate's car on the turntable (5) first goes back (6).
void RoomScene::Room_CarRequest() {
    if (request_ == 1) {
        if (carState_ == 0xD) {
            request_ = 0;
            carState_ = 0xC;
            audio::Sound_PlaySe(0x35);
        }
        if (carState_ == 4 || carState_ == 5) {
            request_ = 0;
            carState_ = 6;
            audio::Sound_PlaySe(0x35);
        }
    } else if (request_ == 4) {
        if (carState_ == 5) {
            carState_ = 6;
        } else if (carState_ == 0xD) {
            request_ = 0;
            carState_ = 9;
            audio::Sound_PlaySe(0x34);
        } else if (carState_ == 0xF) {
            request_ = 0;
            carState_ = 0x10;
            age_ = 0;
            audio::Sound_PlaySe(0x34);
        }
    } else if (request_ != 0 && carState_ == 0xD) {
        carState_ = 2;
        mateCar_ = request_ - 1;
        mate_.car.record = &team_[mateCar_];
        mate_.car.partFlags = Save_Current().cars[mateCar_].partFlags;
        Room_PlaceCar(mate_, kMatePlace, kMatePlace[3], kMateSpeed, kMateHeading);
        request_ = 0;
    }
}

// func_0022A6B8, the car states, in its order (a state can lead to the next in the same frame):
// - 0: Q's Factory, the car drives in on path 0 onto the turntable; then 0xC, the turntable's SE 0x35.
// - 2, 9 and 0xC: the turntable turns the car to face the camera, then the next state (3, 0xA, 0xD) and SE 0x8035
//   (the turntable's sound stops). 0xD: the car stands on it while the person talks.
// - 3: the player's car drives off the turntable on path 1, the teammate's backs on 30 frames later on path 4;
//   then 4 (SE 0x35), where the turntable turns it to face the camera, and 5 (SE 0x8035): it stands there.
// - 6: the teammate's car turns to face +x (0x40) (SE 0x8035); 7: it drives off on path 5 while the player's car
//   backs on on path 2; then 0xC (SE 0x35).
// - 0xA: the car drives off the turntable on path 3; then 0xB, the end.
// - 0xE: the car drives in on path 6 and stops (0xF); 0x10 drives out on path 7, then 0xB.
void RoomScene::Room_DriveCars() {
    you_.car.lights = mate_.car.lights = 0;   // (sp40/sp44, cleared each frame; a path's step lights the car)
    if (carState_ == 0 && !Room_CarPath(you_, 0, age_)) {
        carState_ = 0xC;
        audio::Sound_PlaySe(0x35);
    }
    if (carState_ == 6 && Room_TurnCar(mate_, 0x40)) {
        audio::Sound_PlaySe(0x8035);
        carState_ = 7;
        age_ = 0;
    }
    if (carState_ == 7) {
        Room_CarPath(mate_, 5, age_);
        if (!Room_CarPath(you_, 2, age_)) {
            carState_ = 0xC;
            audio::Sound_PlaySe(0x35);
        }
    }
    if ((carState_ == 2 || carState_ == 9 || carState_ == 0xC) && Room_TurnCar(you_, 0x80)) {
        audio::Sound_PlaySe(0x8035);
        age_ = 0;
        ++carState_;
    }
    if (carState_ == 3) {
        if (age_ >= 0x1E) Room_CarPath(mate_, 4, age_ - 0x1E);
        if (!Room_CarPath(you_, 1, age_)) {
            carState_ = 4;
            audio::Sound_PlaySe(0x35);
        }
    }
    if (carState_ == 4 && Room_TurnCar(mate_, 0x80)) {
        audio::Sound_PlaySe(0x8035);
        carState_ = 5;
    }
    if (carState_ == 0xA && !Room_CarPath(you_, 3, age_)) carState_ = 0xB;
    if (carState_ == 0xE && !Room_CarPath(you_, 6, age_)) carState_ = 0xF;
    if (carState_ == 0x10 && !Room_CarPath(you_, 7, age_)) carState_ = 0xB;
}

void RoomScene::Room_DrawFloor(const gfx::Camera3D& cam) {
    static std::vector<gfx::Vertex> v;
    v.clear();
    const float tw = (float)picture_->width, th = (float)picture_->height;
    const float end = kFloorTexel0 + 20.0f * kTexelsPerUnit;
    // One quad per piece of tile: cut where the texel coordinates cross a multiple of 64.
    for (float v0 = kFloorTexel0; v0 < end;) {
        float base = floorf(v0 / kTile) * kTile, v1 = base + kTile < end ? base + kTile : end;
        for (float u0 = kFloorTexel0; u0 < end;) {
            float ubase = floorf(u0 / kTile) * kTile, u1 = ubase + kTile < end ? ubase + kTile : end;
            float x0 = (u0 - kFloorTexel0) / kTexelsPerUnit - 10, x1 = (u1 - kFloorTexel0) / kTexelsPerUnit - 10;
            float z0 = 10 - (v0 - kFloorTexel0) / kTexelsPerUnit, z1 = 10 - (v1 - kFloorTexel0) / kTexelsPerUnit;
            float s0 = (kTileU + u0 - ubase) / tw, s1 = (kTileU + u1 - ubase) / tw;
            float t0 = (kTileV + v0 - base) / th, t1 = (kTileV + v1 - base) / th;
            const float p[4][3] = {{x0, 0, z0}, {x1, 0, z0}, {x0, 0, z1}, {x1, 0, z1}};
            gfx::Vertex a = Project(cam, p[0], s0, t0), b = Project(cam, p[1], s1, t0);
            gfx::Vertex c = Project(cam, p[2], s0, t1), e = Project(cam, p[3], s1, t1);
            v.push_back(a);
            v.push_back(b);
            v.push_back(c);
            v.push_back(b);
            v.push_back(e);
            v.push_back(c);
            u0 = u1;
        }
        v0 = v1;
    }
    gfx::Draw3DState st;
    st.depthTest = st.depthWrite = false;
    d_.Triangles(v.data(), (uint32_t)v.size(), picture_, true, true, kFrame, st);
}

namespace {

// A decal quad turned by `angle` about the vertical and moved to (1.2, 0, 5.2), clamped to its rectangle.
void Room_DrawDecal(gfx::Draw2D& d, const gfx::Texture& picture, const gfx::Camera3D& cam, const Decal& q,
                    float angle) {
    const float s = sinf(angle), c = cosf(angle);
    const float corners[4][2] = {{q.x, q.z}, {q.x + q.w, q.z}, {q.x, q.z + q.d}, {q.x + q.w, q.z + q.d}};
    const float uv[4][2] = {{q.u0, q.v0}, {q.u1, q.v0}, {q.u0, q.v1}, {q.u1, q.v1}};
    gfx::Vertex v[4];
    for (int i = 0; i < 4; ++i) {
        const float p[3] = {kDecalX + c * corners[i][0] + s * corners[i][1], q.y,
                            kDecalZ - s * corners[i][0] + c * corners[i][1]};
        v[i] = Project(cam, p, uv[i][0] / picture.width, uv[i][1] / picture.height);
    }
    const gfx::Vertex tri[6] = {v[0], v[1], v[2], v[1], v[3], v[2]};
    gfx::Draw3DState st;
    st.depthTest = st.depthWrite = false;
    d.Triangles(tri, 6, &picture, true, true, kFrame, st);
}

}  // namespace

// The room: the floor, the decals, the picture's top 225 rows as a sprite over them (blended by its alpha; the
// furniture and walls), then the cars in front of all of it.
void RoomScene::Room_Draw() {
    // The room is a 4:3 picture: a wide screen's sides are black.
    d_.Pillarbox();
    // Shop_SceneTask each frame: a car on the big tires has its body 0.85 up (+0x74, D_00332E14-E1C).
    for (RoomCar* c : {&you_, &person_, &mate_})
        c->body.bodyHeight = (c->car.partFlags & kPartBigTires) ? kBigTiresLift : 0.0f;
    if (picture_) {
        Room_DrawFloor(camera_);
        Room_DrawDecal(d_, *picture_, camera_, kTurntable, ((((turn_ + 0x80) & 0xFF) / 128.0f) - 1.0f) * kPi);
        Room_DrawDecal(d_, *picture_, camera_, kTurntableTop, 0.0f);
        d_.Sprite(*picture_, 0, 0, 640, 224, 0, 0, 640, 224);
    }
    d_.ClearDepth();
    // The person's headlights are on while info+0x18 is (the message's typing blips set it to 2), and it counts down
    // after each drawing: the lights flash with the talk.
    person_.car.lights = message_.blip ? kLightsHead : 0;
    RaceCar cars[3] = {you_.car, person_.car, mate_.car};
    int n = person_.car.record ? 2 : 1;
    // The teammate's car (slot 2) is there in states 3-7.
    if (carState_ >= 3 && carState_ <= 7) cars[n++] = mate_.car;
    cars_.Cars_Draw(d_, camera_, light_, cars, n);
    if (message_.blip) --message_.blip;
}

bool RoomScene::Room_AfterEnding() {
    // Shop_SceneTask's state 3 once the ending's sequencer tells it (state 1 again). Once president (stamp 100),
    // the cinema's talk (D_002A7ED0[9][8]) from its start, the room back at once without a fade. The first time:
    // stamp 100 queued, and the car straight out (no drive-out) at the Secretary's door (8, 8) at 6:00 (54000).
    endingOut_ = -1;
    if (Stamp_Earned(100)) {
        const MessageTable* cinema = Messages_Table(9, 8);
        if (cinema) message_.Start(*cinema, 9, 8);
        fade_ = 32;
        return true;
    }
    Stamp_Queue(100);
    leaveTown_ = 8;
    leavePlace_ = 8;
    newClock_ = 54000;
    return false;
}

int RoomScene::Update(const platform::Input& input) {
    if (endingOut_ >= 0) {
        // State 3 under Fade_OutTask (t << 25 over 0x40 frames); then the parent runs the ending.
        d_.BeginFrame(0x80000000);
        Room_Draw();
        d_.Fill(0, 0, gfx::Draw2D::kWidth, gfx::Draw2D::kHeight, (uint32_t)(endingOut_ * 2) << 24);
        d_.EndFrame(Race::kFrameBlend);
        if (endingOut_ < 0x40) {
            ++endingOut_;
            return 0;
        }
        return kEnding;
    }
    // func_0022C418: the last picture held under a faint black overlay for 64 frames, darkening it; then the song
    // stops and the scene tells its parent 5.
    if (ending_ >= 0) {
        d_.BeginFrameFromLast(0.0f);
        d_.Fill(0, 0, gfx::Draw2D::kWidth, gfx::Draw2D::kHeight, 0x08000000);
        d_.EndFrame(0);
        if (++ending_ < 0x40) return 0;
        audio::Sound_StopSong();
        return parentResult_;
    }
    d_.BeginFrame(0x80000000);
    // At 0x41 frames the person starts talking (once: state +0x25 becomes 1; the age restarts with some car states).
    if (age_ == 0x41 && !talked_) {
        const MessageTable* table = Messages_Table(town_, place_);
        if (table) message_.Start(*table, town_, place_);
        talked_ = true;
    }
    Room_StickTurn(input);
    Room_CarRequest();
    Room_DriveCars();
    Room_Draw();
    bool quit = false;
    int leave = 0;
    if (message_.Running()) {
        // The talk's results (Shop_SceneTask's state 1). 5, a plain end: the parent is told 10 (on to the town) and
        // the cars move on (+0x02 = 4). 6, quit for today: the parent is told 6 and the scene ends at once
        // (func_0022C418) without the car driving out. 9 | car << 8, a car's parts: state 2, the parts window
        // (func_0022BB88) for that car, and the car is asked onto the turntable (+0x02 = car + 1). 4, a race:
        // func_0023C7B0 sets it up, the parent is told 4 and the scene is gone at once (no fade).
        int r = message_.Update(input);
        // Photo_ViewTask's take mode: the save's car 0 (just copied into the photo) over the photo.
        if (message_.PhotoShown() >= 0) {
            const SaveCar& c = Save_Current().cars[0];
            PhotoCar photo;
            Photo_SetCar(photo, c.paint, c.body, c.wheelColor, c.wheel, c.billboard, c.partFlags);
            Photo_DrawCar(d_, cars_, photo);
        }
        // Shop_SceneTask marks the place found as the talk ends (Place_MarkFound): a bar's first talk is its longer
        // one.
        if (r == 4 || r == 5) Place_MarkFound(town_, place_);
        if (r == 5) {
            parentResult_ = kDriveOut;
            request_ = 4;
            // Pre-code 0x14 (result 12): Field_Setup at the place it named, so the car comes out there.
            leaveTown_ = town_;
            leavePlace_ = place_;
            if (message_.Leaving()) message_.Destination(&leaveTown_, &leavePlace_);
        } else if (r == 6) {
            parentResult_ = kQuit;
            quit = true;
        } else if (r == 0x13) {
            // Code 0x13: the shop's screen over the room while the talk waits (func_00269E88).
            inShop_ = true;
            parts_.StartShop(Room_SceneKind(town_, place_), town_, picture_);
        } else if ((r & 0xFF) == 9) {
            inParts_ = true;
            request_ = (r >> 8) + 1;
            parts_.Start(r >> 8, picture_);
        } else if (r == 4) {
            leave = kRace;
        } else if ((r & 0xFF) == 8) {
            // 8 | car << 8, the Paint Shop's car pick: state 2, the paint screen (PaintShop_Task) for that car, which
            // is asked onto the turntable (+0x02 = car + 1).
            request_ = (r >> 8) + 1;
            const std::string file = RoomFile(town_);
            const gfx::Texture &beam = d_.Get(file, 4), &glow = d_.Get(file, 5);
            paint_.Start(r >> 8, picture_, beam.id ? &beam : nullptr, glow.id ? &glow : nullptr);
            inPaint_ = true;
        } else if (r == 11) {
            // Code 0x14: on to the place it names (the parent opens that building).
            message_.Destination(&goTown_, &goPlace_);
            parentResult_ = kGoTo;
            quit = true;
        } else if (r == 15) {
            // Code 0x18, the ending: Shop_SceneTask's state 3. The song fades and Fade_OutTask darkens the room
            // (still drawn while the state's age is under 0x41); then the ending runs, Ending_Task in Cloud Hill
            // and the replay (func_0026D2C0) in My City Cinemas, and Room_AfterEnding does what follows.
            audio::Sound_FadeOutSong();
            endingOut_ = 0;
        }
        // A part fitted (0x15) or the team changed (0x10): the cars as the save has them now. Outside the team's
        // rooms the player's car was the caller's record, which still has the old billboard sponsor (+7): it takes
        // the save's from here on.
        bool team = message_.TakeTeamChanged(), car = message_.TakeCarChanged();
        if (team || car) {
            for (int i = 0; i < 3; ++i) Room_SetTeamCar(i);
            you_.car.record = &team_[0];
            you_.car.partFlags = Save_Current().cars[0].partFlags;
            Room_LoadCars();
        }
    }
    // Shop_SceneTask's state 1 in the shops and the bank (Room_SceneKind, func_0022AD70): the money in its box at the
    // top right (func_0022E7C0).
    if (Room_SceneKind(town_, place_) != 0 && message_.Running())
        Money_Draw(d_, 0x1B8, 0x1A, Save_Current().money, Save_Current().currency);
    if (inShop_ && parts_.Update(input) < 0) {
        // Left: the talk goes on. A body bought is in the garage, not on the car: nothing to rebuild.
        inShop_ = false;
        message_.Message_ShopDone();
    }
    if (inParts_) {
        // State 2. A part that shows on the car rebuilds it (the original reloads a changed body, func_0020AD18).
        // Leaving the window restarts the talk on the same table (Q's #1 goes to "Whose parts" while flag 0 is set)
        // and asks for the player's car back on the turntable (+0x02 = 1).
        int r = parts_.Update(input);
        if (parts_.Changed()) {
            for (int i = 0; i < 3; ++i) Room_SetTeamCar(i);
            you_.car.partFlags = Save_Current().cars[0].partFlags;
            mate_.car.partFlags = Save_Current().cars[mateCar_].partFlags;
            Room_LoadCars();
        }
        if (r < 0) {
            inParts_ = false;
            request_ = 1;
            const MessageTable* table = Messages_Table(town_, place_);
            if (table) message_.Start(*table, town_, place_);
        }
    }
    if (paint_.Active()) {
        // State 2 with the paint screen: the car shows each change. Once it's left the talk starts over and the
        // player's car is asked back (+0x02 = 1), the lamp still sliding away.
        const int r = paint_.Update(input, carState_);
        if (paint_.Changed())
            for (int i = 0; i < 3; ++i) Room_SetTeamCar(i);
        if (r < 0) {
            inPaint_ = false;
            request_ = 1;
            const MessageTable* table = Messages_Table(town_, place_);
            if (table) message_.Start(*table, town_, place_);
        }
    }
    // Fade_InTask: the scene comes out of black over 32 frames.
    if (fade_ < 32) d_.Fill(0, 0, gfx::Draw2D::kWidth, gfx::Draw2D::kHeight, (uint32_t)((32 - fade_) * 4) << 24);
    ++fade_;
    d_.EndFrame(Race::kFrameBlend);
    ++age_;
    if (leave) {
        audio::Sound_StopSong();
        return leave;
    }
    if (carState_ == 0xB || quit) {
        // The scene ends: the song fades out and func_0022C418 takes over.
        audio::Sound_FadeOutSong();
        ending_ = 0;
    }
    return 0;
}

}  // namespace game
