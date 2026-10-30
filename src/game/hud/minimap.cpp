#include "minimap.h"

#include <math.h>

#include "../../gfx/mesh.h"
#include "../../platform/input.h"
#include "../debug_text.h"

namespace game {

namespace {

const float kPi = 3.14159265f;

// D_002A4050, by course id. Not const: the Map Tuner extra changes it, as the debug byte changed the game's.
// (PC) 0x27, the Test Course (C16, which has no map: scripts/course/make_c16.py makes one), at scale 40
// unturned: x 40-196, y 130-287 on the frame, where the race courses' maps are.
const int kMapViews = 0x28;
MapView kRaceMapView[kMapViews] = {
    {-5, 2014, 50, -kPi / 2},  {-5, 2014, 50, -kPi / 2},  {244, 2086, 50, kPi / 2},  {212, 1989, 50, 0},
    {188, 2071, 50, kPi / 2},  {176, 2017, 50, 0},        {92, 2047, 50, kPi / 2},   {129, 2011, 50, 0},
    {75, 2048, 50, -kPi / 2},  {75, 2048, 50, -kPi / 2},  {-35, 2092, 50, kPi},      {92, 2047, 50, kPi / 2},
    {-19, 1996, 50, -kPi / 2}, {80, 2043, 50, kPi / 2},   {112, 2040, 50, kPi / 2},  {122, 2048, 40, -kPi / 2},
    {122, 2048, 40, -kPi / 2}, {125, 2064, 50, -kPi / 2}, {320, 2048, 50, 0},        {260, 2089, 50, kPi / 2},
    {70, 2090, 70, kPi / 2},   {320, 2048, 50, 0},        {172, 2023, 50, 0},        {142, 2063, 33.5f, -kPi / 2},
    {320, 2048, 50, 0},        {-188, 1910, 105, 0},      {-299, 1860, 133.5f, -kPi / 2}, {320, 2048, 50, 0},
    {102, 2048, 50, 0},        {320, 2048, 50, 0},        {320, 2048, 50, 0},        {320, 2048, 50, 0},
    {320, 2048, 50, 0},        {320, 2048, 50, 0},        {320, 2048, 50, 0},        {320, 2048, 50, 0},
    {320, 2048, 50, 0},        {81, 2022, 50, -kPi / 2},  {98, 2037, 75, kPi / 2},   {197, 2006, 40, 0},
};

// Colours (vf17, D_002A4350-D_002A4390) as PS2 RGBA words.
const uint32_t kMapColor = 0x59993333;      // (0.2, 0.2, 0.6), alpha 0.7
const uint32_t kRed = 0x800000FF;     // D_002A4370: player 1's arrow and teammates
const uint32_t kCyan = 0x80B3B300;    // D_002A4380, (0, 0.7, 0.7): player 2's (car flag 0x40)
const uint32_t kOtherColor = 0x4000FF00;    // green, alpha 0.5
const uint32_t kBallColor = 0x8000FFFF;     // D_002A43A0, (1, 1, 0): Soccer's ball and the field's coins

// The markers' models (D_00290870, D_00290940): an arrow 30 wide and 40 long pointing along +z, and a 14-unit square,
// both in world units at scale 50.
const float kArrow[3][2] = {{0, 20}, {-15, -20}, {15, -20}};
const float kDot[4][2] = {{-7, 7}, {7, 7}, {-7, -7}, {7, -7}};

// The map's camera (RaceMap_Draw): the world moved by -(800, 200, 800), turned by the view's angle about y, then
// seen straight down from 200 above with focal length `scale` (func_0021F920): +x of the turned world is right on the
// screen, +z up; field lines are 0.47 of a pixel high (0.94 in the frame).
struct MapProjection {
    float cx, cy, k, c, s;
    explicit MapProjection(const MapView& v)
        : cx(v.x), cy((v.y - 2048.0f) * 2 + 224.0f), k(v.scale / 200.0f), c(cosf(v.angle)), s(sinf(v.angle)) {}
    void Project(float x, float z, float& sx, float& sy) const {
        float a = x - 800.0f, b = z - 800.0f;
        sx = cx + k * (a * c + b * s);
        sy = cy - 0.94f * k * (b * c - a * s);
    }
};

const gfx::Rect kFrame = {0, 0, gfx::Draw2D::kWidth, gfx::Draw2D::kHeight};

void DrawTriangles(gfx::Draw2D& d, const gfx::Vertex* v, uint32_t count, const gfx::Rect& clip) {
    gfx::Draw3DState state;
    state.depthTest = state.depthWrite = false;
    d.Triangles(v, count, nullptr, false, false, clip, state);
}

gfx::Vertex At(float x, float y, uint32_t color) {
    gfx::Vertex v = {x, y, 0, gfx::Draw2D::Color(color), 0, 0, 1, 1};
    return v;
}

}  // namespace

const MapView& RaceMap_View(int id) { return kRaceMapView[id >= 0 && id < kMapViews ? id : 0]; }
MapView& RaceMap_EditView(int id) { return kRaceMapView[id >= 0 && id < kMapViews ? id : 0]; }

namespace {
bool g_tunerOn = false;
uint32_t g_tunerHeld = 0, g_tunerPressed = 0;
}  // namespace

void RaceMap_SetTuner(bool on, uint32_t held, uint32_t pressed) {
    g_tunerOn = on;
    g_tunerHeld = held;
    g_tunerPressed = pressed;
}

// RaceMap_Draw (0x00242818-0x00242938) and RaceMap_DrawStep (0x00242B04-0x00242C10) with the debug byte, before
// the map is drawn: pad 2's held (pad +0x2E) Right / Left add 1.0 to / take it off x, Up / Down the same to y (field
// lines: Up moves the map down the screen), R2 multiplies the scale by 1.01162 (D_00332F04), R1 divides it by the
// same (D_00332F08); pressed (+0x32) L1 adds pi/2 to the angle (D_00332F0C), L2 takes pi/2 off (D_00332F10). Then
// func_0026DE80 prints the view in the debug font at (16, 0x1A x 8): x, y, scale, angle. On the PC R2, R1, L1 and
// L2 are pad 2's Jet, Horn, Wing Up and Wing Down.
const MapView& RaceMap_Tune(MapView& v) {
    if (!g_tunerOn) return v;
    const float kScaleStep = 1.01162f, kQuarter = 1.5707964f;
    if (g_tunerHeld & platform::kRight) v.x += 1.0f;
    if (g_tunerHeld & platform::kLeft) v.x -= 1.0f;
    if (g_tunerHeld & platform::kUp) v.y += 1.0f;
    if (g_tunerHeld & platform::kDown) v.y -= 1.0f;
    if (g_tunerHeld & platform::kJet) v.scale *= kScaleStep;
    if (g_tunerHeld & platform::kHorn) v.scale /= kScaleStep;
    if (g_tunerPressed & platform::kWingUp) v.angle += kQuarter;
    if (g_tunerPressed & platform::kWingDown) v.angle -= kQuarter;
    DebugText_Printf(16, 0x1A * 8, "X:%f Y:%f Z:%f W:%f", v.x, v.y, v.scale, v.angle);
    return v;
}

bool MiniMap::Load(const std::string& dataDir, const std::string& dir) {
    gfx::Mesh mesh;
    if (!gfx::LoadObj(dataDir + "\\models\\" + dir + "\\map.obj", mesh)) {
        Load(gfx::Mesh());
        return false;
    }
    return Load(mesh);
}

bool MiniMap::Load(const gfx::Mesh& mesh) {
    xz_.clear();
    rgb_.clear();
    indices_.clear();
    for (const gfx::MeshVertex& v : mesh.vertices) {
        xz_.push_back(v.pos[0]);
        xz_.push_back(v.pos[2]);
        rgb_.insert(rgb_.end(), v.color, v.color + 3);
    }
    for (uint32_t i : mesh.indices)
        if (i >= mesh.vertices.size()) return false;
    indices_ = mesh.indices;
    return !indices_.empty();
}

void MiniMap::Draw(gfx::Draw2D& d, const MapView& view, const MapMarker* markers, int count) const {
    Draw(d, view, markers, count, kFrame);
}

void MiniMap::Draw(gfx::Draw2D& d, const MapView& view, const MapMarker* markers, int count,
                   const gfx::Rect& clip) const {
    if (indices_.empty()) return;
    DrawShape(d, view, kMapColor, 0, 0, clip);
    DrawMarkers(d, view, markers, count, clip);
}

void MiniMap::DrawShape(gfx::Draw2D& d, const MapView& view, uint32_t tint, float dx, float dz,
                        const gfx::Rect& clip) const {
    if (indices_.empty()) return;
    MapProjection p(view);
    // The VU program's colour: the vertex's times vf17 (the race's map is white, so it comes out as vf17).
    uint32_t color[3];
    std::vector<uint32_t> colors(xz_.size() / 2);
    for (size_t i = 0; i < colors.size(); ++i) {
        for (int k = 0; k < 3; ++k) color[k] = (uint32_t)(rgb_[i * 3 + k] * ((tint >> (8 * k)) & 0xFF) + 0.5f);
        colors[i] = (tint & 0xFF000000u) | color[2] << 16 | color[1] << 8 | color[0];
    }
    std::vector<gfx::Vertex> v;
    v.reserve(indices_.size());
    for (uint32_t i : indices_) {
        float x, y;
        p.Project(xz_[i * 2] + dx, xz_[i * 2 + 1] + dz, x, y);
        v.push_back(At(x, y, colors[i]));
    }
    DrawTriangles(d, v.data(), (uint32_t)v.size(), clip);
}

void MiniMap::DrawMarkers(gfx::Draw2D& d, const MapView& view, const MapMarker* markers, int count,
                          const gfx::Rect& clip) {
    MapProjection p(view);
    // The markers keep their size whatever the map's scale.
    float size = 50.0f / view.scale;
    for (int pass = 0; pass < 2; ++pass) {
        for (int m = 0; m < count; ++m) {
            const MapMarker& mk = markers[m];
            if ((mk.kind == MapMarker::kPlayer) != (pass == 0)) continue;
            gfx::Vertex t[6];
            uint32_t n;
            if (mk.kind == MapMarker::kPlayer) {
                float c = cosf(mk.yaw), s = sinf(mk.yaw);
                for (int i = 0; i < 3; ++i) {
                    float ax = kArrow[i][0] * size, az = kArrow[i][1] * size, x, y;
                    p.Project(mk.x + ax * c + az * s, mk.z - ax * s + az * c, x, y);
                    t[i] = At(x, y, mk.side ? kCyan : kRed);
                }
                n = 3;
            } else {
                gfx::Vertex q[4];
                for (int i = 0; i < 4; ++i) {
                    float x, y;
                    p.Project(mk.x + kDot[i][0] * size, mk.z + kDot[i][1] * size, x, y);
                    uint32_t color = mk.kind == MapMarker::kBall || mk.kind == MapMarker::kCoin ? kBallColor
                                     : mk.kind != MapMarker::kTeammate ? kOtherColor
                                     : mk.side ? kCyan : kRed;
                    q[i] = At(x, y, color);
                }
                t[0] = q[0], t[1] = q[1], t[2] = q[2], t[3] = q[1], t[4] = q[3], t[5] = q[2];
                n = 6;
            }
            DrawTriangles(d, t, n, clip);
        }
    }
}

}  // namespace game
