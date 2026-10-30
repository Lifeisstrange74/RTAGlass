// The Adventure's world: 64 blocks of 1600 x 1600 units, FLD/<abc>.BIN, each in a race course's layout (its field of
// 8 x 8 cells of 200 and block 64, its collision data and objects; data/models/FLD/<abc>, data/collision/FLD). The
// blocks lie in rows of eight along x, eight rows along z, the odd rows half a block (800) along, so each block
// touches six others; the world wraps both ways. A high-level reimplementation of how the field holds it: the block
// ids (func_0021C8D0, func_0021C6A0), the blocks around the player's car (func_002201C0 streams up to four into the
// loader's slots; the PC keeps those near the car, read on threads of their own), the ground query across their edges
// (func_00207280, the field's D_00335710) and the view (func_00220938 with func_0021FB38's offsets, D_0029D080). See
// docs/field.md.
//
// Positions are the game's: a car's fixed point (CarBody::fixed, 0x2000000 a block) is the world's; its float
// position is in its own block, 0-1600 (Car_WorldPosition).
#pragma once

#include <stdint.h>

#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "../../gfx/draw2d.h"
#include "../../gfx/mesh.h"
#include "../../gfx/model.h"
#include "../hud/minimap.h"
#include "../race/collision.h"
#include "../race/course.h"
#include "../race/race_light.h"

namespace game {

class FieldWorld {
public:
    static const int kBlocks = 64;
    static const int32_t kBlockFixed = 0x2000000;   // a block's width in fixed point (1600 units)

    // func_0021C8D0: the block holding the fixed-point position (x, z): id = row / 2 x 16 + column x 2 + row & 1.
    static int Field_BlockAt(int32_t x, int32_t z);
    // func_0021C6A0's origin: the block's corner in fixed point (an odd row's column starts half a block along).
    static void Field_BlockOrigin(int block, int32_t* x, int32_t* z);
    // The block's file, FLD/<abc>.BIN: digits id / 16, (id / 4) & 3 and id & 3 with its bits swapped (func_002201C0's
    // debug print).
    static std::string Field_BlockFile(int block);
    // Block `to`'s corner seen from block `from`'s, in units, the nearer way round the world.
    static void Field_BlockOffset(int from, int to, float* dx, float* dz);

    FieldWorld(gfx::Draw2D& draw, const std::string& dataDir);
    ~FieldWorld();
    FieldWorld(const FieldWorld&) = delete;
    FieldWorld& operator=(const FieldWorld&) = delete;

    // The field's sky (func_00210158's SYS/SORA.GSL).
    void Field_LoadSky();
    // Cloud Hill (town 8): Field_Setup gives it course 0x22 (ACTION/A16) instead of the world's blocks, the race's
    // ground query (Collision_Ground on the course's PMP) and its own view (func_00220B20). The course's place is
    // block 64's (info+0x54 = 0x40), its origin (0, 0), its positions the course's own (0-1600, no wrapping).
    // Field_SetCourse(0x22) loads it (once) and makes this a course field; -1 the world again. Meanwhile the world's
    // blocks aren't read or drawn, Loaded is true of block 64 alone, and the ground and posts are the course's.
    static const int kCourseBlock = 64;
    void Field_SetCourse(int course);
    bool OnCourse() const { return course_ >= 0; }
    // Whether a point in block `block` has ground to ask (the course's everywhere).
    bool Field_HasGround(int block) const { return OnCourse() || Loaded(block); }
    // The blocks near the fixed-point position (x, z), the player's car's: those it could soon see are wanted, nearest
    // first, and read on the loading threads; those well out of sight are let go. kWaitHere: the block the point is
    // in is built at once if it isn't in; kWaitAll (behind a loading screen): every wanted block is.
    enum Wait { kNoWait, kWaitHere, kWaitAll };
    void Field_LoadAround(int32_t x, int32_t z, Wait wait = kWaitHere);
    // Builds what the loading threads have read (the pictures and models up to the graphics card) for about
    // `budgetMs` of this frame; a block is in once all of it is.
    void Field_Stream(double budgetMs);
    bool Loaded(int block) const { return OnCourse() ? block == kCourseBlock : blocks_.count(block) != 0; }

    // The view from block `block` (the player's car's) through `camera`, in that block's units: the sky, the loaded
    // blocks' cells in view, then `cars` (the cars and objects; its argument is how far from the eye anything can
    // still show), then their blocks 64.
    void Field_DrawView(gfx::Draw2D& d, const gfx::Camera3D& camera, const RaceLight& light, int block,
                        const std::function<void(float)>& cars) const;

    // func_00207280: the ground query for a point given in block `block`'s units; a point past the block's edge is
    // asked of the block it's in. -1 (nothing) for a block that isn't loaded.
    int32_t Field_Ground(int block, float* p, float* ceiling) const;
    // Car_HitPosts on block `block`'s posts.
    uint32_t Field_Posts(int block, const float* pos, uint16_t yaw) const;

    // func_00242438's maps: each loaded block's map model (the file's map section, D_0177AB70 + slot; models/FLD/
    // <abc>/map.obj) through `view`, in block `block`'s units, its colours times `tint`, cut to `clip`.
    void Field_DrawMaps(gfx::Draw2D& d, const MapView& view, int block, uint32_t tint, const gfx::Rect& clip) const;

    // Block `block`'s object model `n` (its file's objects section: D_0177F580 + 0x18 x slot + 4n; models/FLD/<abc>/
    // objN.obj), or null if the block isn't in or has no such model. The field's handlers place them
    // (FieldObjects).
    static const int kObjects = 6;   // "MISC OBJ COUNT OVER !"
    const gfx::Model* Field_Object(int block, int n) const;

private:
    static const int kCells = 64;   // the 200-unit cells; the file's block 64 is drawn after the cars
    // Block 11 (My City, My Garage) and its 0x34 more model tables: table 2k My City's resident k's lot before they
    // move in, 2k + 1 their building (models/FLD/023/city_II.obj, its block 64 city_II_64.obj), with their own
    // collision data for the building (collision/FLD/023/city_KK.pmp). Loader_LoadField's block-0xB branch.
    static const int kCityBlock = 11;
    static const int kResidents = 26;
    static const int kCityTables = 2 * kResidents;

    struct Block {
        gfx::Model cells;   // section c = cell c
        gfx::Model last;    // block 64
        gfx::Model objects[kObjects];
        Collision ground;
        MiniMap map;
        gfx::Model city, cityLast;   // block 11: section i = table i's cell, its block 64
        std::vector<Collision> cityGround;
    };
    // A block as a loading thread reads it: its models made ready for the card, the pictures they use that weren't
    // read before, and its collision data.
    struct Picture {
        std::string path;
        std::vector<uint32_t> pixels;
        int width, height;
    };
    struct Read {
        int block;
        gfx::PreparedModel cells, last, city, cityLast;
        std::vector<Collision> cityGround;
        gfx::Mesh objects[kObjects];   // kept on the CPU too (FieldObjects draws them there)
        std::vector<Picture> pictures;
        Collision ground;
        MiniMap map;
    };
    // A block being built from what was read, a step at a time: its pictures, its cells, block 64.
    struct Building {
        std::unique_ptr<Read> read;
        std::unique_ptr<Block> block;
        int next;   // the next step
    };
    struct Loader;   // the loading threads (field_world.cpp)

    std::unique_ptr<Read> Field_Read(int block) const;   // the files of one block (any thread)
    // Block 11's tables to draw: one of each resident's pair, by whether they've moved in (func_0023DE30).
    static void Field_CityTables(bool* wanted);
    // Draws block 11's tables (Field_CityTables) of `model` (its cells or their blocks 64) within reach.
    static void Field_DrawCity(gfx::Draw2D& d, const gfx::Camera3D& camera, const gfx::DrawStyle& style,
                               const gfx::Model& model, float dx, float dz, float reach2);
    // The collision data a ground query runs over in turn: the block's, then block 11's moved-in buildings'.
    static int Field_GroundList(const Block& b, const Collision** list);
    // Takes up to `steps` more steps of building `b`; true once it's built.
    bool Field_Build(Building& b, int steps);
    // Moves what the threads have read into building_ (what's no longer near is thrown away).
    void Field_TakeReads();
    // Builds block `w` at once (reading it here, or waiting for the thread reading it).
    void Field_LoadNow(int w);
    // The pictures of `b` not yet on the card are read again by the next block that uses them.
    void Field_Forget(const Building& b);

    gfx::Draw2D& d_;
    std::string dataDir_;
    std::map<int, std::unique_ptr<Block>> blocks_;
    std::map<int, Building> building_;
    float distance_[kBlocks];   // each block's distance from the car's position (Field_LoadAround), in units
    std::set<int> asked_;       // asked of the loading threads and not back yet
    std::unique_ptr<Loader> loader_;
    gfx::Model sky_;   // models/FLD/sky.obj: the race's sky with SYS/SORA.GSL's picture
    // Sky_DrawRings' rings (D_002962E0, D_00298A50: the ELF's, modelconv writes them with Endurance Run's C20): the
    // clouds and stars, and the band below the horizon.
    gfx::Model clouds_, horizon_;
    int course_ = -1;           // info+0x22: -1 the world, 0x22 Cloud Hill's course
    int courseLoaded_ = -1;     // the course in courseWorld_ and courseGround_
    Course courseWorld_;
    Collision courseGround_;
};

// The world as the cars' physics and the chase camera ask it, from the block a car is in at the time of asking (its
// fixed point, which the physics moves mid-update: the game's queries take the car's slot as it is then).
class FieldGround : public Ground {
public:
    FieldGround(const FieldWorld& world, const int32_t* fixed) : world_(world), fixed_(fixed) {}
    bool Empty() const override { return !world_.Field_HasGround(Block()); }
    int32_t Collision_Ground(float* p, float* ceiling) const override {
        return world_.Field_Ground(Block(), p, ceiling);
    }
    uint32_t Collision_Posts(const float* pos, uint16_t yaw) const override {
        return world_.Field_Posts(Block(), pos, yaw);
    }

private:
    int Block() const { return FieldWorld::Field_BlockAt(fixed_[0], fixed_[2]); }

    const FieldWorld& world_;
    const int32_t* fixed_;   // the car's position (CarBody::fixed)
};

}  // namespace game
