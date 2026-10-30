// The ending's scenes: func_0026CA48 (0x0026CA48) with Field_Setup (0x00210158) for the towns 22-31 and
// Field_CreateTasks (0x00210430) in its ending mode (arg 1). See docs/finish.md, "The ending".
//
// Each scene is a field without its player: the player's car (slot 0, the save's car 0) is a town car (Car_Init
// town << 10 | 0x800000) that the AI drives along the scene's path, the town's cars (slots 1 on) stand where the
// table puts them, and Camera_FixedTask (0x0021F098) watches. The ending's info+0x28 flag 0x80000 makes Ai_Init
// take a car's path by its slot, so slot 0 gets path 0; the other slots have none. No radio, meter, map (0x10000),
// coins, doors (0x400 cleared), talks or menu.
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "../save/save_data.h"
#include "field.h"

namespace game {

namespace {

const float kPi = 3.14159265f;
const float kAngle = kPi / 32768.0f;   // a 16-bit angle in radians
const float kFixed = 20971.52f;
const int32_t kWorldMask = 0x0FFFFFFF;
const int kCloudCourse = 0x22;
const int kFirstTown = 22;   // scene n is town 22 + n (D_002BE438's rows 22-31)
// D_002BE438[22 + n] + 4: the scene's block (64: Cloud Hill's course).
const int kEndingBlock[Field::kEndingScenes] = {64, 47, 35, 5, 19, 43, 40, 23, 9, 7};
// D_002DCD28: the time of day each scene starts at (8:00 on Cloud Hill, 9:00 Papaya Island ... 21:00 Sandpolis).
const int32_t kEndingClock[Field::kEndingScenes] = {0x11940, 0x13C68, 0x1A5E0, 0x1C908, 0x1EC30,
                                                    0x20F58, 0x278D0, 0x27CB8, 0x29BF8, 0x2E248};
// Car_Init's speed cap (+0x246, 2.5 km/h) for the ending's towns: 5 on Cloud Hill (town 0x16), 0x0F elsewhere.
const int kCapCloud = 5, kCap = 0x0F;
// D_0029CCC0[town - 0x16]: Camera_FixedTask's view, {eye, focal, pitch, yaw}. Scene 0's eye is absolute (in the
// course's units) and its focal and pitch move (below); the others' eye is the player's car's position plus the
// eye turned by Ry(yaw) Rx(pitch), so the camera follows the car at a fixed angle.
struct FixedView {
    float eye[3];
    int16_t pitch;
    uint16_t yaw;
};
const FixedView kEndingView[Field::kEndingScenes] = {
    {{1308.5f, 309.0f, 848.0f}, 0x0880, 0x4000}, {{-3.4f, 2.6f, -10.6f}, 0x0780, 0xB280},
    {{-4.3f, 2.0f, -13.6f}, 0x0680, 0x2400},     {{-6.1f, 4.6f, -20.6f}, -0x80, 0x3980},
    {{-12.3f, 2.0f, -53.6f}, 0x1100, 0x0880},    {{-3.6f, 2.0f, -12.3f}, 0x0580, 0x8280},
    {{-2.6f, 2.0f, -8.7f}, -0x80, 0x7F80},       {{-3.0f, 4.7f, -20.0f}, 0x0300, 0x7700},
    {{-1.7f, 2.2f, -8.3f}, -0x780, 0xD000},      {{-4.9f, 3.6f, -15.1f}, -0x380, 0x5600},
};
const float kFocal = 500.0f;
// Scene 0: the focal starts at 5000 (D_00332C54) and is divided by 1.01162 (D_00332C58) each frame while it's over
// 500; the pitch tilts 5 a frame from 0x380 for 0x100 frames, to the table's 0x880.
const float kZoomFocal = 5000.0f, kZoomStep = 1.01162f;
const int kTiltFrom = 0x380, kTiltFrames = 0x100, kTiltStep = 5;

void Car_FromSave(CarRecord& r, const SaveCar& c, const char* name) {
    r.paint = c.paint & 0xFFFFFF;
    r.body = c.body;
    r.wheelColor = c.wheelColor;
    r.wheel = c.wheel;
    r.billboard = c.billboard;
    r.name = name;
}

}  // namespace

void Field::Ending_LoadCars(int town) {
    // Field_TownCarsTask's first frame and Car_Init for the town: data/ai/FLD/Tnn_ss.txt by slot (tools/modelconv).
    endCars_.clear();
    endPath_ = AiPath();
    const SaveRecord& save = Save_Current();
    int32_t ox, oz;
    FieldWorld::Field_BlockOrigin(kEndingBlock[town - kFirstTown] & 0x3F, &ox, &oz);
    for (int slot = 0;; ++slot) {
        char name[64];
        snprintf(name, sizeof name, "\\ai\\FLD\\T%02d_%02d.txt", town, slot);
        std::string path = dataDir_ + name;
        FILE* f = fopen(path.c_str(), "r");
        if (!f) break;
        int rec = 0;
        float pos[3];
        unsigned yaw = 0, flags = 0;
        char row[256];
        bool found = false;
        while (!found && fgets(row, sizeof row, f))
            found = sscanf(row, "car %d %f %f %f %x %x", &rec, &pos[0], &pos[1], &pos[2], &yaw, &flags) == 6;
        fclose(f);
        if (!found) continue;
        CarBody* body;
        if (slot == 0) {
            // The player's car on the AI: its path, the lane +0x194 & 3 = 0 (record 0) and the town's cap.
            endPath_.Load(path);
            Ai_Init(endAi_, 0, 0, 0, true);
            endAi_.cap = town == kFirstTown ? kCapCloud : kCap;
            body_ = CarBody();
            body = &body_;
        } else {
            EndingCar c;
            c.car = RaceCar();
            if (rec < 0) {
                // Cloud Hill's slots 1 and 2: the teammates' cars as the save has them (Field_TownCarsTask always
                // makes both, 0x0020E7A4; the World Grand Prix needs a full team, so the skip is only a PC guard).
                int mate = -1 - rec;
                if (save.teammates[mate - 1][0] == 0) continue;
                const SaveCar& sc = save.cars[mate];
                const int8_t* who = save.teammates[mate - 1];
                Car_FromSave(c.record, sc, Car_DriverRecordName(who[0], who[1]));
                flags = sc.partFlags;
            } else {
                const CarRecord* r = Car_DriverRecord(town, rec);
                if (!r) continue;
                c.record = *r;
                c.car.town = town;
                c.car.index = rec;
            }
            c.car.partFlags = (uint16_t)flags;
            c.parts = Car_SetParts(kTownCarParts);
            c.parts.flags = (uint16_t)flags;
            endCars_.push_back(c);
            body = &endCars_.back().body;
        }
        // Car_Init: the start as the table has it (no drop to the ground), from the town block's corner.
        Car_Place(*body, pos, (uint16_t)yaw);
        body->fixed[0] = (body->fixed[0] + ox) & kWorldMask;
        body->fixed[2] = (body->fixed[2] + oz) & kWorldMask;
    }
    for (EndingCar& c : endCars_) {
        c.car.record = &c.record;
        c.car.physics = &c.body;
    }
}

void Field::Ending_Start(int scene) {
    endScene_ = scene;
    endAge_ = -1;
    frame_ = 0;
    const int town = kFirstTown + scene;
    town_ = town;
    place_ = 0;
    clock_ = kEndingClock[scene];
    menuOpen_ = talking_ = stampsOpen_ = false;
    talkTown_ = -1;
    doorsShut_ = true;
    // Car_Init (town << 10 | 0x800000) of record 0, the save's car 0, with the town cars' parts record.
    const SaveRecord& r = Save_Current();
    const SaveCar& c = r.cars[0];
    Car_FromSave(record_, c, r.name);
    car_ = RaceCar();
    car_.record = &record_;
    car_.town = -1;
    car_.partFlags = c.partFlags;
    parts_ = Car_SetParts(kTownCarParts);
    parts_.flags = c.partFlags;
    Car_SetPlayerFlags(parts_.flags);
    // Field_Setup: Cloud Hill's course (town 0x16) or the world's blocks; Field_CreateTasks: Light_Setup and the
    // view's Course_ObjectsTask (A16's gate on Cloud Hill).
    world_.Field_SetCourse(scene == 0 ? kCloudCourse : -1);
    world_.Field_LoadSky();
    Light_Setup(light_, world_.OnCourse() ? kCloudCourse : 0);
    if (world_.OnCourse()) {
        if (!gateLoaded_) courseObjects_.Load(d_, dataDir_, kCloudCourse);
        gateLoaded_ = true;
        courseObjects_.Course_ClearObjects();
    }
    Ending_LoadCars(town);
    if (parts_.flags & kPartBigTires) body_.bodyHeight = kBigTiresLift;
    car_.physics = &body_;
    Car_WorldPosition(body_, car_.pos);
    car_.yaw = (int16_t)body_.yaw * kPi / 32768.0f;
    // The loader's jobs: the player's body now, the town's cars' bodies and the blocks read meanwhile (the PS2 shows
    // black until its loader is done).
    carModels_.Load(d_, dataDir_, &car_, 1);
    std::vector<RaceCar> list;
    for (const EndingCar& e : endCars_) list.push_back(e.car);
    if (!list.empty()) carModels_.Cars_Request(d_, dataDir_, list.data(), (int)list.size());
    world_.Field_LoadAround(body_.fixed[0], body_.fixed[2], FieldWorld::kNoWait);
    objects_.Reset();
    objects_.Load(d_, dataDir_);
    effects_.Load(d_);
}

void Field::Ending_Camera(gfx::Camera3D* camera) {
    const FixedView& v = kEndingView[endScene_];
    float focal = kFocal;
    int pitch = v.pitch;
    const float* at = nullptr;   // scene 0: no frame, the eye absolute
    if (endScene_ == 0) {
        // The task's age t (held until the release): the focal from 5000 down to 500, the pitch 0x380 to 0x880.
        if (endAge_ == 0) endFocal_ = kZoomFocal;
        else if (endFocal_ > kFocal) endFocal_ /= kZoomStep;
        focal = endFocal_;
        pitch = kTiltFrom + (endAge_ < kTiltFrames ? endAge_ : kTiltFrames) * kTiltStep;
    } else {
        at = car_.pos;   // the car's place as the last frame left it (the camera's task runs first)
    }
    const float a = (int16_t)pitch * kAngle, b = v.yaw * kAngle;
    const float ca = cosf(a), sa = sinf(a), cb = cosf(b), sb = sinf(b);
    const float right[3] = {cb, 0, -sb}, up[3] = {sb * sa, ca, cb * sa}, fwd[3] = {sb * ca, -sa, cb * ca};
    for (int k = 0; k < 3; ++k) {
        camera->right[k] = right[k];
        camera->up[k] = up[k];
        camera->forward[k] = fwd[k];
        camera->eye[k] = at ? at[k] + v.eye[0] * right[k] + v.eye[1] * up[k] + v.eye[2] * fwd[k] : v.eye[k];
    }
    camera->focalX = focal;
    camera->focalY = focal * 0.94f;
    camera->centerX = gfx::Draw2D::kWidth / 2.0f;
    camera->centerY = gfx::Draw2D::kHeight / 2.0f;
}

bool Field::Ending_Update() {
    world_.Field_LoadAround(body_.fixed[0], body_.fixed[2], FieldWorld::kNoWait);
    world_.Field_Stream(4.0);
    if (endAge_ < 0) {
        // func_0026CA48 waits for the loader (info+0x0F 0), then releases the field's tasks (func_00204F58) and
        // sets the clock from D_002DCD28. The PC waits for the car bodies and the car's block.
        bool cars = carModels_.Cars_Stream(d_, 4.0);
        if (!cars || !world_.Loaded(world_.OnCourse() ? FieldWorld::kCourseBlock : Block())) return false;
        endAge_ = 0;
        clock_ = kEndingClock[endScene_];
    }
    // Adventure_Task's state 4 runs the clock on under the scene; Light_TimeOfDay by it each frame.
    if (clock_++ > kClockHour) clock_ -= kClockHour;
    gfx::Camera3D camera;
    Ending_Camera(&camera);
    int block = Block();
    Car_SetClock(clock_);
    // The player's car: Ai_TrackZone and Ai_Drive in the town block's units (the path's), then Car_Update. The
    // town's cars have no path: they only settle where they stand.
    {
        FieldGround ground(world_, body_.fixed);
        uint32_t bits = 0;
        if (!ground.Empty() && !endPath_.Empty()) {
            float pos[4];
            memcpy(pos, body_.pos, sizeof pos);
            int32_t ox, oz;
            FieldWorld::Field_BlockOrigin(kEndingBlock[endScene_] & 0x3F, &ox, &oz);
            body_.pos[0] = ((body_.fixed[0] - ox) & kWorldMask) / kFixed;
            body_.pos[2] = ((body_.fixed[2] - oz) & kWorldMask) / kFixed;
            if (body_.pos[0] > 0x07FFFFFF / kFixed) body_.pos[0] -= (kWorldMask + 1) / kFixed;
            if (body_.pos[2] > 0x07FFFFFF / kFixed) body_.pos[2] -= (kWorldMask + 1) / kFixed;
            int next = Ai_TrackZone(endPath_, endAi_, body_);
            bits = Ai_Drive(endPath_, endAi_, next, body_, true, true);
            memcpy(body_.pos, pos, sizeof pos);
        }
        if (!ground.Empty()) Car_Update(body_, parts_, ground, bits, true);
        car_.partFlags = parts_.flags;
        body_.fixed[0] &= kWorldMask;
        body_.fixed[2] &= kWorldMask;
    }
    for (EndingCar& c : endCars_) {
        FieldGround ground(world_, c.body.fixed);
        if (ground.Empty()) continue;
        Car_Update(c.body, c.parts, ground, 0, true);
        c.car.partFlags = c.parts.flags;
        c.body.fixed[0] &= kWorldMask;
        c.body.fixed[2] &= kWorldMask;
    }
    CarBody* all[Cars::kMaxCars];
    int n = 0;
    all[n++] = &body_;
    for (size_t i = 0; i < endCars_.size() && n < Cars::kMaxCars; ++i) all[n++] = &endCars_[i].body;
    if (world_.OnCourse()) {
        // A16_GateTask over the cars, as on Cloud Hill's field.
        const int kGateCars = 12;
        CarBody bodies[kGateCars];
        bool out[kGateCars] = {};
        int m = n < kGateCars ? n : kGateCars;
        for (int i = 0; i < m; ++i) bodies[i] = *all[i];
        courseObjects_.Update(CourseCars{bodies, out, m, false, false});
        for (int i = 0; i < m; ++i) *all[i] = bodies[i];
    }
    Cars_Collide(all, n);
    // Field_ObjectsTask runs in the ending's fields too: the UFO's task, the tide and the texture copies.
    objects_.Ufo_Step(world_.Loaded(22), clock_);
    objects_.Field_Scrolls(d_, world_, frame_);
    Car_WorldPosition(body_, car_.pos);
    car_.yaw = (int16_t)body_.yaw * kPi / 32768.0f;
    Light_TimeOfDay(light_, clock_, body_.pos[1]);

    // The view (RaceView_Init(0): Field_DrawView, or Cloud Hill's), from the car's block.
    if (Block() != block) block = Block();
    d_.Fill(0, 0, gfx::Draw2D::kWidth, gfx::Draw2D::kHeight, light_.background | 0x80000000u);
    Light_SetHighlight(light_, camera.forward);
    RaceCar cars[Cars::kMaxCars];
    int count = 0;
    cars[count++] = car_;
    for (size_t i = 0; i < endCars_.size() && count < Cars::kMaxCars; ++i) {
        const EndingCar& c = endCars_[i];
        RaceCar& r = cars[count++];
        r = c.car;
        r.record = &c.record;
        r.physics = &c.body;
        Car_WorldPosition(c.body, r.pos);
        float dx, dz;
        FieldWorld::Field_BlockOffset(block, FieldWorld::Field_BlockAt(c.body.fixed[0], c.body.fixed[2]), &dx, &dz);
        r.pos[0] += dx;
        r.pos[2] += dz;
        r.yaw = (int16_t)c.body.yaw * kPi / 32768.0f;
    }
    world_.Field_DrawView(d_, camera, light_, block, [&](float reach) {
        carModels_.Cars_Draw(d_, camera, light_, cars, count);
        if (world_.OnCourse())
            courseObjects_.Course_DrawObjects(d_, camera, light_);
        else
            objects_.Field_DrawObjects(d_, camera, light_, world_, block, frame_, reach, body_.pos, clock_);
        Field_DrawNight(camera);
    });
    effects_.Car_Effects(d_, camera, body_, 0, frame_, frame_, false);
    effects_.Effects_Draw(d_, camera, frame_ + 6, false);
    ++frame_;
    ++endAge_;
    return true;
}

}  // namespace game
