#include "field_map.h"

#include "../hud/minimap.h"
#include "../save/save_data.h"
#include "../text.h"

#include <cstdio>
#include "../../game_text.inc"   // the game's text, GT_<address>_<length>, made from the ELF

namespace game {

namespace {

// D_002A42C0: the near and far views (the frame point the car lands on comes out at (102, 366) in both), x and y as
// func_0021F920 takes them less the car's place, and the scale.
const MapView kZoom[2] = {{302, 89, 50, 0}, {202, 136, 25, 0}};
// D_002A43F0: SCISSOR_2 = x 45-158, field lines 156-207, the frame's inside.
const gfx::Rect kClip = {45, 312, 159, 416};
// func_00242438's colour for the maps (vf17): D_002A4340, (0.5, 0.5, 0.5) in the day (the clock 0xE484-0x278D0,
// when the UFO's door is shut), D_002A4330, (1, 1, 1) at night.
const int32_t kDayStart = 0xE484, kDayLength = 0x1944C;
const uint32_t kDayTint = 0x80808080, kNightTint = 0x80FFFFFF;

// func_00249208's frame: SYS/POSE.GSL picture 2 (TEX0 D_002A8FE0[0], the GS's 0x1E94), palette 10, at (30, 304).
const char kPose[] = "SYS/POSE.GSL";
const int kFrameX = 30, kFrameY = 304;

}  // namespace

void FieldMap::Draw(gfx::Draw2D& d, const FieldWorld& world, int block, const float* pos, float yaw,
                    int32_t clock, const std::vector<MapMarker>& markers, int age) const {
    // func_00249208: the frame's sides (D_003277B0, D_003277D0), and between them an 8-texel strip stretched 112 wide
    // (D_003277F0), the map's see-through background. In palette 11 its three lamps a side are lit: every other 32
    // frames, with stamp 40 (My Garage) and a mail not read yet (func_0023DFE0: bit 7 of a mail in the box clear).
    bool unread = false;
    const SaveRecord& r = Save_Current();
    for (int i = 0; i < r.mailCount && i < 128 && !unread; ++i) unread = !(r.mail[i] & 0x80);
    const bool lit = (age & 0x20) && Stamp_Earned(40) && unread;
    const gfx::Texture& frame = d.Get(kPose, 2, lit ? 11 : 10);
    d.Sprite(frame, kFrameX, kFrameY, 16, 120, 192, 0, 16, 120);
    d.Sprite(frame, kFrameX + 128.0f, kFrameY, 16, 120, 216, 0, 16, 120);
    d.Sprite(frame, kFrameX + 16.0f, kFrameY, 112, 120, 208, 0, 8, 120);

    // func_00242438: the camera follows the car (the world point (800, 800) is moved by the car's place, scale / 200
    // to a unit), seen straight down, north up.
    // Off the world (Cloud Hill's course) FieldMap_Task draws the frame alone: the maps, the arrow and the markers
    // are course -1's.
    if (world.OnCourse()) return;
    const MapView& z = kZoom[zoom_];
    float k = z.scale / 200.0f;
    MapView view = {z.x - pos[0] * k, z.y + 1936.0f + pos[2] * k * 0.47f, z.scale, 0};
    uint32_t tint = (uint32_t)(clock - kDayStart) > (uint32_t)kDayLength ? kNightTint : kDayTint;
    world.Field_DrawMaps(d, view, block, tint, kClip);

    // The car's arrow (D_00290870 in red), kept its size whatever the view's scale (50 / scale).
    MapMarker car = {MapMarker::kPlayer, pos[0], pos[2], yaw, 0};
    MiniMap::DrawMarkers(d, view, &car, 1, kClip);
    if (!markers.empty()) MiniMap::DrawMarkers(d, view, markers.data(), (int)markers.size(), kClip);
}

void FieldMap::Map_DrawRally(gfx::Draw2D& d) {
    // FieldMap_Task's lines while race flag 0x1000 is up on the world's field (course -1), all in colour 0: the
    // title, the destination town's name (D_002BE438) and the time since the last checkpoint as game-clock h:mm.
    int32_t t = Rally_Timer();
    char time[16];
    snprintf(time, sizeof time, "%2d:%02d", t / 9000, t % 9000 / 150);
    Text_PrintAt(d, 230, 34, GT_002ECA30_17, 0);
    {
        gfx::AnchorScope left(d, gfx::Anchor::Left);
        Text_PrintAt(d, 30, 58, GT_002ECA48_11, 0);
        Text_PrintAt(d, 30, 82, Save_TownName(Rally_Destination()), 0);
    }
    gfx::AnchorScope right(d, gfx::Anchor::Right);
    Text_PrintAt(d, 530, 34, GT_00333808_4, 0);
    Text_PrintAt(d, 530, 58, time, 0);
}

}  // namespace game
