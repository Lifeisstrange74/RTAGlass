// The race's course map: the track's outline seen from above in the top left corner, with an arrow for the player's
// car and a dot for every other car. A high-level reimplementation of RaceMap_Draw (0x002427D0), which draws the map
// model of the course file (D_0177AB78, program 0x50) through a top-down camera, and RaceMap_DrawCars (0x00242D20).
// The model is data/models/COURSE/Cnn/map.obj (tools/modelconv). See docs/hud.md.
//
// The view is data (MapView), so the Adventure's field map (func_00242438), drawn the same way in the bottom left with
// each world block's map model in its own colours, uses this too (game/field/field_map).
#pragma once

#include <string>
#include <vector>

#include "../../gfx/draw2d.h"
#include "../../gfx/mesh.h"

namespace game {

// Where a map goes on the screen and how it is turned (D_002A4050 by course id: x, y, scale, angle). The world point
// (800, 800) lands at (x, y) in the frame, (x - 1728 and y - 2048 in the game's GS coordinates, y in field lines);
// the map is scale / 200 the world's size, turned by `angle` radians about the vertical.
struct MapView {
    float x, y, scale, angle;
};
// The race's view of course `id` (0-17).
const MapView& RaceMap_View(int id);
// The same entry to change (the Map Tuner extra; the table lives for the run, as the game's data did).
MapView& RaceMap_EditView(int id);

// The Map Tuner extra (the debug byte in RaceMap_Draw 0x002427D0 and RaceMap_DrawStep 0x00242A88): the race sets,
// each frame it draws its HUD, whether the tuner is on and pad 2's held and pressed buttons (platform::Input's
// bits); off, RaceMap_Tune does nothing.
void RaceMap_SetTuner(bool on, uint32_t held, uint32_t pressed);
// Applies the tuner to `view` as the map is drawn and prints it (DebugText_Printf); returns it.
const MapView& RaceMap_Tune(MapView& view);

// A car on the map.
struct MapMarker {
    // kOther: the other cars (and the field's town cars), green; kBall: Soccer's ball (func_00243398) and kCoin: the
    // field's ChoroQ coins (func_00241E90), yellow.
    enum Kind { kPlayer, kTeammate, kOther, kBall, kCoin };
    Kind kind;
    float x, z;   // world position
    float yaw;    // radians: 0 faces +z, pi/2 +x (only the player's arrow shows it)
    int side;     // car flag 0x40, 2 Player's player 2 and their teammates: cyan, not red
};

class MiniMap {
public:
    // Loads dataDir\models\<dir>\map.obj (dir "COURSE\C00"). False (and nothing is drawn) if the course has none.
    bool Load(const std::string& dataDir, const std::string& dir);
    // The same from a map model already read (no graphics: any thread may do this).
    bool Load(const gfx::Mesh& mesh);
    bool Empty() const { return indices_.empty(); }

    // RaceMap_Draw: the map, blue and see-through; then RaceMap_DrawCars: the cars, the player's arrow first.
    void Draw(gfx::Draw2D& d, const MapView& view, const MapMarker* markers, int count) const;
    // The same cut to `clip` (func_00242A88's SCISSOR_2 on Golf's and Figure 8's courses).
    void Draw(gfx::Draw2D& d, const MapView& view, const MapMarker* markers, int count, const gfx::Rect& clip) const;

    // The map alone, its vertex colours times `tint` (the colour func_00226A18 sets, vf17, as a PS2 RGBA word: 0xFF a
    // colour at full, alpha 0x80 opaque), moved by (dx, dz) units, cut to `clip`.
    void DrawShape(gfx::Draw2D& d, const MapView& view, uint32_t tint, float dx, float dz,
                   const gfx::Rect& clip) const;
    // RaceMap_DrawCars' markers, cut to `clip`: the player's arrows first, then the dots.
    static void DrawMarkers(gfx::Draw2D& d, const MapView& view, const MapMarker* markers, int count,
                            const gfx::Rect& clip);

private:
    std::vector<float> xz_;          // the map's vertices (x, z); it lies flat at y 0
    std::vector<float> rgb_;         // and their colours
    std::vector<uint32_t> indices_;
};

}  // namespace game
