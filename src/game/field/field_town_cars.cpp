#include "field_town_cars.h"

#include <stdio.h>
#include <string.h>

#include "../save/save_data.h"
#include "field.h"

namespace game {

namespace {

const int kTowns = 22;
const int kMyCity = 9, kCloudHill = 8;
const float kFixed = 20971.52f;      // D_00332B80
const int32_t kWorldMask = 0x0FFFFFFF;
const float kPi = 3.14159265f;
// Car_Init's speed cap +0x246 (2.5 km/h): 50 km/h, 100 with the record's part 0x100 (the water ski).
const int kCap = 0x14, kCapWaterSki = 0x28;
const uint16_t kPartWaterSki = 0x100;
// Cars_BumpSound: a bumped car stands this long (+0x210), and Ai_Drive gives it nothing meanwhile.
const int16_t kBumpFrames = 0xB4;

// The town's start and loop of each car, as modelconv wrote them.
struct Spec {
    int index;
    float pos[3];
    uint16_t yaw, flags;
    AiPath path;
};
std::vector<Spec> g_specs;

}  // namespace

const uint8_t kTownCarParts[16] = {0, 7, 0, 0, 0, 2, 0, 0, 6, 0, 8, 0, 10, 0, 12, 0};

bool FieldTownCars::Field_TownCarsLoad(int town) {
    g_specs.clear();
    const FieldTown& t = Field_Town(town);
    for (int i = 0; i < t.cars; ++i) {
        char name[64];
        snprintf(name, sizeof name, "\\ai\\FLD\\T%02d_%02d.txt", town, i);
        std::string path = dataDir_ + name;
        FILE* f = fopen(path.c_str(), "r");
        if (!f) return false;
        Spec s;
        unsigned yaw = 0, flags = 0;
        char row[256];
        bool found = false;
        while (!found && fgets(row, sizeof row, f))
            found = sscanf(row, "car %d %f %f %f %x %x", &s.index, &s.pos[0], &s.pos[1], &s.pos[2], &yaw, &flags) == 6;
        fclose(f);
        if (!found) return false;
        s.yaw = (uint16_t)yaw;
        s.flags = (uint16_t)flags;
        s.path.Load(path);   // none: the car stands (Ai_Drive gives nothing without a path)
        g_specs.push_back(s);
    }
    return true;
}

bool FieldTownCars::Town_Absent(int town, int index, int32_t clock) {
    // Town_Absent (0x0023CD58): a My City resident who has moved in has left their town.
    int k = Resident_Of(town, index);
    if (k >= 0) return Resident_MovedIn(k);
    auto flag = [](int f) { return (Save_Current().eventFlags[f >> 6 & 1] >> (f & 63) & 1) != 0; };
    switch (town) {
    case 2:
        if (index == 0x1D) return flag(22) && clock > 0x32898;   // Fuji City's Guard, after 23:00
        break;
    case 6:
        if (index == 0x0A) return (uint32_t)(clock - 0x1A5E0) > 0xAFC8;   // 12:00-17:00 only
        if (index == 0x11) return !flag(67);
        if (index == 0x20) return flag(63);   // Shirley
        break;
    case 7:
        if (index == 0x0A) return flag(56);
        break;
    case 9:
        // My City's own six cars come once their resident has moved in (jtbl_002EC8A0).
        if (index >= 0x17 && index <= 0x1C) {
            static const int kBits[6] = {15, 16, 13, 14, 12, 17};
            return !Resident_MovedIn(kBits[index - 0x17]);
        }
        break;
    case 13:
        if (flag(85)) return index == 0 || index == 5;
        if (index == 0) return (uint32_t)(clock - 0xAFC9) > 0x1C906;   // the Ruins' person, 5:00-18:00
        if (index == 5) return (uint32_t)(clock - 0xAFC9) <= 0x1C906;  // Benji, only at night
        break;
    }
    return false;
}

void FieldTownCars::Field_TownCarsCreate(int32_t clock) {
    cars_.clear();
    const FieldTown& t = Field_Town(town_);
    int32_t ox, oz;
    FieldWorld::Field_BlockOrigin(t.block & 0x3F, &ox, &oz);
    for (size_t i = 0; i < g_specs.size(); ++i) {
        const Spec& s = g_specs[i];
        if (Town_Absent(town_, s.index, clock)) continue;
        TownCar c;
        // D_0177EA50[4 + i]: the driver record, or a teammate's own car as the save has it (Team_Find).
        const CarRecord* r = Car_DriverRecord(town_, s.index);
        if (!r) continue;
        c.record = *r;
        uint16_t flags = s.flags;
        if (int mate = Team_Find(town_, s.index)) {
            const SaveCar& sc = Save_Current().cars[mate];
            c.record.paint = sc.paint & 0xFFFFFF;
            c.record.body = sc.body;
            c.record.wheelColor = sc.wheelColor;
            c.record.wheel = sc.wheel;
            c.record.billboard = sc.billboard;
            flags = sc.partFlags;
        }
        c.index = s.index;
        c.path = s.path;
        c.car = RaceCar();
        c.car.town = town_;
        c.car.index = s.index;
        c.car.partFlags = flags;
        c.parts = Car_SetParts(kTownCarParts);
        c.parts.flags = flags;
        // Car_Init: the start as the table has it (no drop to the ground), from the town block's corner.
        Car_Place(c.body, s.pos, s.yaw);
        c.body.fixed[0] = (c.body.fixed[0] + ox) & kWorldMask;
        c.body.fixed[2] = (c.body.fixed[2] + oz) & kWorldMask;
        // Ai_Init: the lane is +0x194 & 3 (4 + i), the cap by the record's water ski.
        Ai_Init(c.ai, (int)i + 4, 0, 0, true);
        c.ai.cap = (flags & kPartWaterSki) ? kCapWaterSki : kCap;
        cars_.push_back(c);
    }
}

void FieldTownCars::Cars_RequestModels(gfx::Draw2D& d, Cars& models, int town) {
    // Loader job 0x370400 | count: each car's body (CARn/Qnn.BIN), a teammate's as the save has it.
    std::vector<CarRecord> records(g_specs.size());
    std::vector<RaceCar> list;
    for (size_t i = 0; i < g_specs.size(); ++i) {
        const CarRecord* r = Car_DriverRecord(town, g_specs[i].index);
        if (!r) continue;
        records[i] = *r;
        if (int mate = Team_Find(town, g_specs[i].index)) records[i].body = Save_Current().cars[mate].body;
        RaceCar c = RaceCar();
        c.record = &records[i];
        list.push_back(c);
    }
    models.Cars_Request(d, dataDir_, list.data(), (int)list.size());
}

void FieldTownCars::Start(gfx::Draw2D& d, Cars& models, int town) {
    // The first frame: My Garage's cars are My City's (the same block). The PS2 has the first town's bodies loaded
    // with the field (Field_Setup's loader job) and makes the cars at once; here they come out once their bodies
    // are on the card, which Preload began during the loading screen.
    town_ = town == 0 ? kMyCity : town;
    cars_.clear();
    state_ = kScan;
    // (Cloud Hill's are its task's state 3: all out at once and never gone, its course being block 64's for good.)
    if (Field_Town(town_).cars <= 0 || !Field_TownCarsLoad(town_)) {
        town_ = -1;
        return;
    }
    Cars_RequestModels(d, models, town_);
    state_ = kLoading;
}

void FieldTownCars::Preload(gfx::Draw2D& d, Cars& models, int town) {
    int t = town == 0 ? kMyCity : town;
    if (Field_Town(t).cars <= 0 || !Field_TownCarsLoad(t)) return;
    Cars_RequestModels(d, models, t);
}

void FieldTownCars::Update(gfx::Draw2D& d, Cars& models, const FieldWorld& world, int32_t clock, bool frozen) {
    // Field_TownCarsTask's states.
    if (state_ == kOut && !world.Loaded(Field_Town(town_).block)) {
        // "kill block %d": the cars go; another town's may come.
        cars_.clear();
        state_ = kScan;
    }
    if (state_ == kScan) {
        // "new block %d": the first town with cars whose block is in. A town other than the last loads its cars'
        // bodies (loader job 0x370400 | count) and its records.
        for (int t = 1; t < kTowns; ++t) {
            const FieldTown& town = Field_Town(t);
            if (town.cars <= 0 || t == kCloudHill || !world.Loaded(town.block)) continue;
            if (t != town_) {
                if (!Field_TownCarsLoad(t)) continue;
                town_ = t;
                Cars_RequestModels(d, models, t);
            }
            state_ = kLoading;
            break;
        }
    }
    if (state_ == kLoading && models.Cars_Stream(d, 3.0)) {
        // The loader done (info+0x0F 0): the Car_Init loop, the cars at their starts.
        Field_TownCarsCreate(clock);
        state_ = kOut;
    }
    if (frozen) return;
    for (TownCar& c : cars_) {
        FieldGround ground(world, c.body.fixed);
        if (ground.Empty()) continue;   // off the loaded blocks: it waits
        // Car_Input: Ai_TrackZone and Ai_Drive in the town block's units (the paths'), wherever the car is.
        float pos[4];
        memcpy(pos, c.body.pos, sizeof pos);
        int32_t ox, oz;
        FieldWorld::Field_BlockOrigin(Field_Town(town_).block & 0x3F, &ox, &oz);
        c.body.pos[0] = ((c.body.fixed[0] - ox) & kWorldMask) / kFixed;
        c.body.pos[2] = ((c.body.fixed[2] - oz) & kWorldMask) / kFixed;
        if (c.body.pos[0] > 0x07FFFFFF / kFixed) c.body.pos[0] -= (kWorldMask + 1) / kFixed;
        if (c.body.pos[2] > 0x07FFFFFF / kFixed) c.body.pos[2] -= (kWorldMask + 1) / kFixed;
        uint32_t bits = 0;
        if (!c.path.Empty()) {
            int next = Ai_TrackZone(c.path, c.ai, c.body);
            // A bumped car stands while +0x210 runs down (Ai_Drive on the field).
            if (c.body.bump == 0) bits = Ai_Drive(c.path, c.ai, next, c.body, true, true);
        }
        memcpy(c.body.pos, pos, sizeof pos);
        Car_Update(c.body, c.parts, ground, bits, true);
        c.car.partFlags = c.parts.flags;   // (a teammate's Flight Wing)
        c.body.fixed[0] &= kWorldMask;
        c.body.fixed[2] &= kWorldMask;
    }
}

void FieldTownCars::Cars_Place(int block, RaceCar* out) const {
    for (size_t i = 0; i < cars_.size(); ++i) {
        const TownCar& c = cars_[i];
        RaceCar& r = out[i];
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
}

bool FieldTownCars::Cars_Bumped(int i, int* town, int* place) {
    TownCar& c = cars_[i];
    bool talk = c.body.bump == 0;
    if (talk) {
        *town = town_;
        *place = c.index;
        Car_Stop(c.body);
    }
    c.body.bump = kBumpFrames;
    return talk;
}

void FieldTownCars::Cars_Refresh() {
    for (TownCar& c : cars_) {
        const CarRecord* r = Car_DriverRecord(town_, c.index);
        if (!r) continue;
        c.record = *r;
        if (int mate = Team_Find(town_, c.index)) {
            const SaveCar& sc = Save_Current().cars[mate];
            c.record.paint = sc.paint & 0xFFFFFF;
            c.record.body = sc.body;
            c.record.wheelColor = sc.wheelColor;
            c.record.wheel = sc.wheel;
            c.record.billboard = sc.billboard;
        }
    }
}

void FieldTownCars::Cars_MapDots(int block, std::vector<MapMarker>& out) const {
    for (const TownCar& c : cars_) {
        float pos[3], dx, dz;
        Car_WorldPosition(c.body, pos);
        FieldWorld::Field_BlockOffset(block, FieldWorld::Field_BlockAt(c.body.fixed[0], c.body.fixed[2]), &dx, &dz);
        MapMarker m = {MapMarker::kOther, pos[0] + dx, pos[2] + dz, 0, 0};
        out.push_back(m);
    }
}

}  // namespace game
