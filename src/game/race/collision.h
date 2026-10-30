// A course's collision data (PMP, data/collision/Cnn.pmp, written by tools/modelconv from COURSE/Cnn.BIN) and the
// ground query the cars use (Collision_Ground 0x002084D0, the race's D_00335710). See docs/car_physics.md.
//
// The course is a 16x16 grid of 100-unit cells (x, z wrap every 1600). Header: u32 offset[256] to each cell's first
// surface, s16 count[256] at 0x400, u32 end of data at 0x600. A surface is a triangle strip: u32 vertex count n
// (low 15 bits), two unused words, u32 flags, n vertices (x, y, z, 1), then a unit normal (x, y, z, 1) per triangle.
// Flags: bit 31 a layer that can be under or over (a bridge: it's a ceiling for what's below), bit 28 water (its
// height is the water level), low bits the surface type (bits 0-2 pick the tires' grip). At 0x600 the posts: u32
// offset and u32 count of (x, y, z, bottom) floats, things a car's corners can hit (trees, lamp posts, pillars).
#pragma once

#include <stdint.h>

#include <string>
#include <vector>

namespace game {

// What the cars stand on, as the cars' physics and the chase camera ask it: a course's collision data (Collision,
// the race's D_00335710), or the Adventure's world of blocks (FieldWorld, func_00207280).
class Ground {
public:
    virtual ~Ground() {}
    virtual bool Empty() const = 0;
    virtual int32_t Collision_Ground(float* p, float* ceiling) const = 0;
    virtual uint32_t Collision_Posts(const float* pos, uint16_t yaw) const = 0;
};

class Collision : public Ground {
public:
    bool Load(const std::string& path);
    bool Empty() const override { return data_.empty(); }

    // Collision_Ground (0x002084D0): the highest surface under p (x, z) that is below the lowest layer surface above
    // it. If one is found, p[1] becomes its height and p[3] the water level (0 if none); *ceiling is the lowest layer
    // surface above p (10000 if none) either way. Returns the surface's flags, or -1.
    int32_t Collision_Ground(float* p, float* ceiling) const override;
    // The same, also giving the winning surface triangle's normal record (x, y, z, 1; scaled so y is 1), as the third
    // argument of the game's query does (Golf's ball); null where there's none.
    int32_t Collision_Ground(float* p, float* ceiling, const float** normal) const;

    // Collision_Posts (0x00219CB0, from Car_HitPosts 0x00219ED8): the car at `pos` facing `yaw` against the posts. A
    // post counts while the car's height is between its bottom - 1 and its top (y); turned into the car's frame,
    // one within 1.5 ahead (or behind) and 0.9 to the left (or right) is on that corner. Returns Car_Ground's
    // corner bits: 1 front left, 2 front right, 4 rear left, 8 rear right.
    uint32_t Collision_Posts(const float* pos, uint16_t yaw) const override;

    // Field_Ground's block 11 (0x00207458) and Car_HitPosts' (0x00219FEC): the ground query run over `count` PMPs in
    // turn (the block's own, then each of My City's buildings that's there) with one ground, ceiling, water and
    // result; and their posts' corner bits together.
    static int32_t Collision_GroundAll(const Collision* const* list, int count, float* p, float* ceiling);
    static uint32_t Collision_PostsAll(const Collision* const* list, int count, const float* pos, uint16_t yaw);

private:
    // Collision_Strips (0x00206F28): runs `count` surfaces from `at` past (x, z) = p[0], p[2].
    int32_t Collision_Strips(uint32_t at, int count, const float* p, float* ground, float* ceiling,
                             float* water, const float** groundNormal = nullptr) const;

    std::vector<uint8_t> data_;
};

}  // namespace game
