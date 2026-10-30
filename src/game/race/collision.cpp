#include "collision.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "../../util/data_pack.h"

namespace game {

namespace {

const float kCell = 100.0f;      // the grid's cell size
const float kNoCeiling = 10000.0f;   // D_00332B24
const float kNoGround = -10000.0f;   // D_00332B28
const float kPostSide = 0.9f;        // D_00332B90 (and -D_00332B8C): the corners' half width
const float kPostReach = 1.5f;       // how far ahead and behind the corners reach
const float kPi = 3.14159265f;

}  // namespace

bool Collision::Load(const std::string& path) {
    data_.clear();
    // Loose, or from a mounted pack (data\collision.rpk).
    if (!util::Data_ReadFile(path, data_) || data_.size() < 0x608) data_.clear();
    return !data_.empty();
}

int32_t Collision::Collision_Ground(float* p, float* ceiling) const { return Collision_Ground(p, ceiling, nullptr); }

int32_t Collision::Collision_Ground(float* p, float* ceiling, const float** normal) const {
    if (normal) *normal = nullptr;
    float ground = kNoGround, ceil = kNoCeiling, water = 0;
    int32_t flags = -1;
    if (!data_.empty()) {
        int cell = (((int)(p[2] / kCell) & 15) << 4) + ((int)(p[0] / kCell) & 15);
        uint32_t at;
        int16_t count;
        memcpy(&at, &data_[cell * 4], 4);
        memcpy(&count, &data_[0x400 + cell * 2], 2);
        flags = Collision_Strips(at, count, p, &ground, &ceil, &water, normal);
    }
    if (flags >= 0) {
        p[1] = ground;
        p[3] = water;
    }
    *ceiling = ceil;
    return flags;
}

int32_t Collision::Collision_GroundAll(const Collision* const* list, int count, float* p, float* ceiling) {
    float ground = kNoGround, ceil = kNoCeiling, water = 0;
    int32_t flags = -1;
    for (int i = 0; i < count; ++i) {
        const Collision& c = *list[i];
        if (c.data_.size() < 0x600) continue;
        int cell = (((int)(p[2] / kCell) & 15) << 4) + ((int)(p[0] / kCell) & 15);
        uint32_t at;
        int16_t n;
        memcpy(&at, &c.data_[cell * 4], 4);
        memcpy(&n, &c.data_[0x400 + cell * 2], 2);
        int32_t r = c.Collision_Strips(at, n, p, &ground, &ceil, &water);
        if (r >= 0) flags = r;
    }
    if (flags >= 0) {
        p[1] = ground;
        p[3] = water;
    }
    *ceiling = ceil;
    return flags;
}

uint32_t Collision::Collision_PostsAll(const Collision* const* list, int count, const float* pos, uint16_t yaw) {
    uint32_t hits = 0;
    for (int i = 0; i < count; ++i) hits |= list[i]->Collision_Posts(pos, yaw);
    return hits;
}

uint32_t Collision::Collision_Posts(const float* pos, uint16_t yaw) const {
    if (data_.size() < 0x608) return 0;
    uint32_t at, count;
    memcpy(&at, &data_[0x600], 4);
    memcpy(&count, &data_[0x604], 4);
    if (at > data_.size() || count > (data_.size() - at) / 16) return 0;
    // sceVu0RotMatrixY by -yaw, applied to the offset: rows (c, 0, -s), (0, 1, 0), (s, 0, c).
    float angle = -(int16_t)yaw * kPi / 32768.0f, s = sinf(angle), c = cosf(angle);
    float y = pos[1] + 1.0f;
    uint32_t hits = 0;
    const float* post = reinterpret_cast<const float*>(&data_[at]);
    for (uint32_t i = 0; i < count; ++i, post += 4) {
        float dx = post[0] - pos[0], dy = post[1] - pos[1], dz = post[2] - pos[2];
        if (y < post[3] || dy < 0) continue;
        float x = dx * c + dz * s, z = -dx * s + dz * c;
        bool left = -kPostSide <= x && x < 0, right = 0 <= x && x <= kPostSide;
        if (0 <= z && z <= kPostReach) {
            hits |= left ? 1 : right ? 2 : 0;
        } else if (-kPostReach <= z && z < 0) {
            hits |= left ? 4 : right ? 8 : 0;
        }
    }
    return hits;
}

// Each triangle (v0, v1, v2) of a strip that has (x, z) inside it gives the height
// h = v0.y - (n.x (x - v0.x) + n.z (z - v0.z)) from its normal n (the game leaves out the division by n.y). The
// strip's triangles alternate their winding, so the inside test flips sign every other one.
int32_t Collision::Collision_Strips(uint32_t at, int count, const float* p, float* ground, float* ceiling,
                                    float* water, const float** groundNormal) const {
    int32_t result = -1;
    const float px = p[0], pz = p[2];
    for (int s = 0; s < count; ++s) {
        if (at + 16 > data_.size()) break;
        uint32_t word;
        int32_t flags;
        memcpy(&word, &data_[at], 4);
        memcpy(&flags, &data_[at + 12], 4);
        int n = (int)(word & 0x7FFF);
        uint32_t next = at + (uint32_t)n * 32 - 16;
        if (n < 2 || next > data_.size()) break;
        const float* v = reinterpret_cast<const float*>(&data_[at + 16]);
        const float* normal = v + n * 4;
        for (int k = 0; k + 2 < n; ++k) {
            const float* a = v + k * 4;
            const float* b = a + 4;
            const float* c = b + 4;
            float e0 = (b[2] - a[2]) * (px - b[0]) - (b[0] - a[0]) * (pz - b[2]);
            float e1 = (c[2] - b[2]) * (px - c[0]) - (c[0] - b[0]) * (pz - c[2]);
            float dx = px - a[0], dz = pz - a[2];
            float e2 = (a[2] - c[2]) * dx - (a[0] - c[0]) * dz;
            bool inside = (k & 1) == 0 ? (e0 >= 0 && e1 >= 0 && e2 >= 0) : (e0 <= 0 && e1 <= 0 && e2 <= 0);
            if (!inside) continue;
            const float* nk = normal + k * 4;
            float h = a[1] - (dx * nk[0] + dz * nk[2]);
            if (flags < 0) {   // a layer: a ceiling for what's below it, ground for what's above
                if (p[1] < h && h < *ceiling) *ceiling = h;
                if (h < p[1] && *ground < h) *ground = h;
            } else if (flags & 0x10000000) {   // water
                *water = h;
            } else if (h < *ceiling && *ground < h) {
                *ground = h;
                result = flags;
                if (groundNormal) *groundNormal = nk;
            }
        }
        at = next;
    }
    return result;
}

}  // namespace game
