#include "field_coins.h"

#include <math.h>

#include <algorithm>

#include "../../audio/sound.h"
#include "../race/course_objects.h"
#include "../save/save_data.h"

namespace game {

namespace {

#include "field_coins.inc"

const int kCoinCount = 100;
const float kPickupReach = 1.4f;   // D_00332EEC
const float kPi = 3.14159265f;     // D_00332EF0
const int kMailCoins = 0x7E;       // sent when 70 coins are left
const int kMailAt = 70;
const gfx::Rect kFrame = {0, 0, gfx::Draw2D::kWidth, gfx::Draw2D::kHeight};

}  // namespace

const FieldCoin& Field_Coin(int i) { return kCoins[i >= 0 && i < kCoinCount ? i : 0]; }

void FieldCoins::Load(gfx::Draw2D& d, const std::string& dataDir) {
    if (model_.Empty()) model_.Load(dataDir + "\\models\\SYS\\COIN\\0.obj", d);
}

void FieldCoins::Field_CheckCoins(int block, const float* pos) {
    for (int i = 0; i < kCoinCount; ++i) {
        const FieldCoin& c = kCoins[i];
        if (c.block != block || Pickup_Taken(i + 1)) continue;
        float d2 = 0;
        for (int k = 0; k < 3; ++k) d2 += (pos[k] - c.pos[k]) * (pos[k] - c.pos[k]);
        if (sqrtf(d2) >= kPickupReach) continue;
        audio::Sound_PlaySe(0x29);
        Pickup_Take(i);
        if (Pickups_Left() == kMailAt) Mail_Send(kMailCoins);
    }
}

void FieldCoins::Coins_Draw(gfx::Draw2D& d, const gfx::Camera3D& camera, const RaceLight& light,
                            const FieldWorld& world, int block, int frame, float reach) {
    gfx::DrawSourceScope source(d.GetRenderer(), gfx::DrawSource::Objects);
    if (model_.Empty()) return;
    // The view task's frame count's low byte, less 0x80, turns them all alike: sceVu0RotMatrixY by (b - 128) pi / 128.
    float a = ((frame & 0xFF) - 0x80) * kPi / 128.0f, c = cosf(a), s = sinf(a);
    // func_00226D58's state (TEX0 0x2003AA0599405D48) with the light the objects before them set (D_0029D240).
    style_ = Course_ObjectStyle(light);
    lighting_ = Light_CarSet(light, 0);
    style_.lighting = &lighting_;
    style_.cpu = true;   // all the coins in view as one draw call
    const float radius = 0.71f;   // the model's reach from its origin (0.5 across, 0 to 1 up)
    for (int i = 0; i < kCoinCount; ++i) {
        const FieldCoin& k = kCoins[i];
        if (!world.Loaded(k.block) || Pickup_Taken(i + 1)) continue;
        // Field_SlotPlace: the coin's place moved by where its block lies from the car's.
        float dx, dz;
        FieldWorld::Field_BlockOffset(block, k.block, &dx, &dz);
        float rows[12] = {c, 0, s, k.pos[0] + dx, 0, 1, 0, k.pos[1], -s, 0, c, k.pos[2] + dz};
        float e2 = 0;
        for (int j = 0; j < 3; ++j) {
            float p = rows[j * 4 + 3] - camera.eye[j];
            e2 += p * p;
        }
        if (sqrtf(e2) - radius > reach) continue;
        style_.transform = rows;
        model_.Draw(d, camera, kFrame, style_);
    }
}

void FieldCoins::Coins_MapDots(const FieldWorld& world, int block, std::vector<MapMarker>& out) {
    for (int i = 0; i < kCoinCount; ++i) {
        const FieldCoin& k = kCoins[i];
        if (!world.Loaded(k.block) || Pickup_Taken(i + 1)) continue;
        float dx, dz;
        FieldWorld::Field_BlockOffset(block, k.block, &dx, &dz);
        MapMarker m = {MapMarker::kCoin, k.pos[0] + dx, k.pos[2] + dz, 0, 0};
        out.push_back(m);
    }
}

}  // namespace game
