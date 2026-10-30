// A race course's world as the race draws it: the field of COURSE/Cnn.BIN (or an event's ACTION/Ann.BIN) in
// 200-unit blocks, and the sky (data/models/COURSE/Cnn, made by tools/modelconv). A high-level reimplementation of
// the world part of Race_DrawView (0x00220D40): Sky_Draw (0x00225608), the blocks in view (Camera_CellVisible
// 0x0021FA38 picks them) and, after the cars, block 64. See docs/race.md.
#pragma once

#include <functional>
#include <string>
#include <vector>

#include "../../gfx/draw2d.h"
#include "../../gfx/model.h"
#include "race_light.h"

namespace game {

// Where a course's starting grid is (D_002A7F28, by course id): the first slot's position, the direction the cars
// face (0-3: a quarter turn each, 0 = +z, 1 = +x) and which side the odd slots sit on.
struct GridStart {
    float x, y, z;
    int dir, side;
};

// The race's lighting and GS state for the field's blocks (Race_DrawView, and the Adventure's world): the field's
// day, dusk and night colours by the light's weights, its fog, and the alpha fade from 544 units away to 800.
gfx::DrawStyle Course_WorldStyle(const RaceLight& light);

class Course {
public:
    static const int kCourses = 0x28;   // course ids: 0-0x11 the race courses, 0x12-0x26 the events' (ACTION)
    // (PC) 0x27: COURSE/C16.BIN, the course on the disc that no table of the game's names, raced as the Test Course
    // extra (docs/extras.md). Its grid, line, path, map and cameras are the PC's own.
    static const int kTestCourse = 0x27;

    // Loads course `id`'s blocks and sky from dataDir (the textures through `draw`). False if it has no field.
    bool Load(gfx::Draw2D& draw, const std::string& dataDir, int id);

    static const GridStart& Grid(int id);
    // The course's file name ("C00", "A10"), by id.
    static const char* File(int id);
    // Its folder under data/models: COURSE\C00 or ACTION\A10.
    static std::string ModelDir(int id);

    // The world seen through `camera` under `light`, filling the frame. `cars` draws what comes between the field
    // and block 64 (the cars and the course's objects).
    void Race_DrawView(gfx::Draw2D& d, const gfx::Camera3D& camera, const RaceLight& light,
                       const std::function<void()>& cars = nullptr) const;   // 0x00220D40

    // Cloud Hill's field (func_00220B20, RaceView_Init's view for course 0x22 in the Adventure): Race_DrawView's,
    // but the sky tinted by the background colour (Light_SkyTint), the clouds and stars ring always (its picture
    // the course's own, Field_LoadClouds) and no band below the horizon.
    void Field_LoadClouds(gfx::Draw2D& draw, const std::string& dataDir);
    void Cloud_DrawView(gfx::Draw2D& d, const gfx::Camera3D& camera, const RaceLight& light,
                        const std::function<void()>& cars) const;

    // Whether course `id` has a floor the cars show in: RaceView_Init (func_00221300) gives courses 0x12, 0x1C (Tunnel
    // Race) and 0x20 the view RaceView_DrawMirrored (func_00220FE8), which draws the cars with Cars_DrawMirrored.
    static bool MirrorsCars(int id) { return id == 0x12 || id == 0x1C || id == 0x20; }

    // The heights of every surface of the field above or below (x, z), lowest first.
    void Surfaces(float x, float z, std::vector<float>& heights) const;

private:
    static const int kBlocks = 65;   // 0-63: an 8x8 grid of 200-unit cells; 64: drawn after the cars

    gfx::Model blocks_[kBlocks];
    gfx::Model sky_;
    gfx::Model clouds_, horizon_;   // Endurance Run's rings (func_002256A8); empty elsewhere
    int id_ = -1;
};

}  // namespace game
