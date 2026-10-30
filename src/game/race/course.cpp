#include "course.h"

#include <stdio.h>

#include <algorithm>

namespace game {

namespace {

// Course file by course id (the loader's table D_0029BE50): C00-C14, then C18-C20 (C16 is on the disc but unused),
// then from 0x12 the ACTION files A00-A20. (PC) 0x27 is C16, the Test Course.
const char* const kCourseFile[Course::kCourses] = {
    "C00", "C01", "C02", "C03", "C04", "C05", "C06", "C07", "C08", "C09", "C10", "C11", "C12", "C13",
    "C14", "C18", "C19", "C20", "A00", "A01", "A02", "A03", "A04", "A05", "A06", "A07", "A08", "A09",
    "A10", "A11", "A12", "A13", "A14", "A15", "A16", "A17", "A18", "A19", "A20", "C16"};
const int kFirstAction = 0x12;

// D_002A7F28.
const GridStart kGrid[Course::kCourses] = {
    {572.2f, 1.1f, 561.3f, 1, 0},   {572.2f, 1.1f, 561.3f, 1, 0},   {721.3f, 60.0f, 166.5f, 2, 1},
    {121.5f, 10.0f, 302.5f, 0, 0},  {437.5f, 10.0f, 253.8f, 3, 0},  {588.8f, 5.0f, 380.0f, 0, 1},
    {742.7f, 1.0f, 588.7f, 3, 0},   {617.6f, 173.0f, 285.5f, 3, 0}, {778.5f, 42.5f, 913.8f, 1, 0},
    {778.5f, 12.5f, 913.8f, 1, 0},  {451.4f, 30.0f, 461.7f, 2, 1},  {742.7f, 1.0f, 588.7f, 3, 0},
    {532.5f, 10.0f, 343.8f, 1, 1},  {880.5f, 2.0f, 704.3f, 3, 0},   {869.5f, 3.3f, 526.3f, 3, 0},
    {711.0f, 180.0f, 1186.3f, 1, 0}, {711.0f, 180.0f, 1186.3f, 1, 0}, {770.5f, 1.0f, 1095.3f, 1, 0},
    {50.0f, 20.0f, 20.0f, 0, 0},    {717.5f, 6.0f, 95.0f, 3, 1},    {412.0f, 53.0f, 871.0f, 3, 1},
    {106.6f, 1.5f, 99.3f, 1, 0},    {415.0f, 24.5f, 388.0f, 0, 0},  {166.7f, 10.1f, 634.4f, 0, 0},
    {400.0f, 5.25f, 400.0f, 0, 0},  {180.0f, 1.0f, 117.0f, 0, 0},   {180.0f, 0.0f, 200.0f, 1, 0},
    {652.8f, 400.0f, 1198.5f, 1, 0}, {544.0f, 76.1f, 693.0f, 0, 1},  {165.0f, 134.0f, 102.5f, 0, 0},
    {99.9f, 101.0f, 55.8f, 0, 0},   {208.75f, 105.0f, 200.0f, 0, 0}, {875.0f, 31.94f, 792.5f, 2, 0},
    {750.0f, 1.0f, 815.0f, 0, 0},   {832.5f, 300.0f, 647.1f, 0, 0}, {790.0f, 364.0f, 1290.0f, 2, 0},
    {800.0f, -38.5f, 800.0f, 3, 0}, {349.0f, 103.5f, 767.2f, 3, 0}, {824.0f, 300.0f, 695.0f, 3, 0},
    // (PC) The Test Course: on C16's start box (x 410-460, z 808-864, an untextured quad at y 20.4 under the road),
    // facing -x. The road is a closed tube whose two ends meet over the box with a gap at x 427-431, the floor at 27.1
    // on the +x side and 24.0 on the -x: driven -x it's a step down. The car stands past the gap, dropped onto the
    // floor (the tube's top is at 54 there).
    {420.0f, 40.0f, 836.0f, 3, 0},
};

// The view's edges for Camera_CellVisible-style culling: the whole frame.
const gfx::Rect kFrame = {0, 0, gfx::Draw2D::kWidth, gfx::Draw2D::kHeight};

}  // namespace

// The race's lighting and GS state. Vu1Packet_UploadLighting(D_0177F720) gives programs 0x30/0x40 the weights of
// the field's day, dusk and night colours (1.1, 0, 0 by day: the vertex colour brightened by a tenth), the fog
// (vf10) and the alpha fade (vf11: from 544 units away the world fades out, gone at 800). FOGCOL is D_0177F750.
// TEST_2 is 0x537FB for the blocks (D_0029CFC0) and 0x737FB for block 64 (D_0029CFF0): alpha >= 0x7F draws fully,
// lower alpha draws colour without depth.
gfx::DrawStyle Course_WorldStyle(const RaceLight& light) {
    gfx::DrawStyle s;
    s.weights = light.weights;
    s.cue = true;
    s.alphaMax = 128.0f;
    s.fogMax = 255.0f;
    s.scale = 0.5f;
    s.end = gfx::ViewEnd();   // 800, or Graphics Low's 600
    s.fog = true;
    s.fogScale = light.fogScale;
    s.fogEnd = light.fogEnd;
    s.fogColor = light.fogColor;
    s.state.gsAlphaTest = true;
    s.clearDepth = false;
    return s;
}

bool Course::Load(gfx::Draw2D& draw, const std::string& dataDir, int id) {
    if (id < 0 || id >= kCourses) return false;
    std::string dir = dataDir + "\\models\\" + ModelDir(id) + "\\";
    bool any = false;
    for (int b = 0; b < kBlocks; ++b) {
        char name[16];
        snprintf(name, sizeof name, "%02d.obj", b);
        blocks_[b] = gfx::Model();
        any |= blocks_[b].Load(dir + name, draw);
    }
    sky_ = gfx::Model();
    // (PC) C16 has no sky picture (only its road's texture; the PS2 would show whatever was left at TBP 0x3CFE): the
    // Test Course borrows Peach Raceway's sky.
    sky_.Load(id == kTestCourse ? dataDir + "\\models\\" + ModelDir(0) + "\\sky.obj" : dir + "sky.obj", draw);
    clouds_ = gfx::Model();
    horizon_ = gfx::Model();
    if (id == kClockCourse) {
        clouds_.Load(dir + "clouds.obj", draw);
        horizon_.Load(dir + "horizon.obj", draw);
    }
    id_ = id;
    return any;
}

const GridStart& Course::Grid(int id) { return kGrid[id >= 0 && id < kCourses ? id : 0]; }

const char* Course::File(int id) { return kCourseFile[id >= 0 && id < kCourses ? id : 0]; }

std::string Course::ModelDir(int id) {
    return std::string(id >= kFirstAction && id != kTestCourse ? "ACTION\\" : "COURSE\\") + File(id);
}

// Sky_Draw (0x00225608) first: the sky's ceiling and ring follow the camera across the ground (its translation is
// the eye with y = 0), opaque, unfogged and behind everything. Then the blocks in view with the race's lighting,
// the cars and the course's objects (`cars`), and block 64 last. RaceView_DrawMirrored (func_00220FE8), the view of
// courses 0x12, 0x1C and 0x20, is the same but for the sky, which only 0x20 draws, and the cars (MirrorsCars).
void Course::Race_DrawView(gfx::Draw2D& d, const gfx::Camera3D& camera, const RaceLight& light,
                           const std::function<void()>& cars) const {
    gfx::DrawSourceScope source(d.GetRenderer(), gfx::DrawSource::World);
    gfx::DrawStyle sky;
    sky.offset[0] = camera.eye[0];
    sky.offset[2] = camera.eye[2];
    sky.state.depthTest = false;
    sky.state.depthWrite = false;
    if (id_ == kClockCourse) Light_SkyTint(light, sky.tint);
    if (!MirrorsCars(id_) || id_ == 0x20) sky_.Draw(d, camera, kFrame, sky);
    // Sky_DrawRings (0x002256A8): Endurance Run's rings, blended over the sky: the clouds and stars by the night weight, the band
    // below the horizon by the day's.
    sky.tint[0] = sky.tint[1] = sky.tint[2] = 1;
    sky.alpha = light.weights[2];
    clouds_.Draw(d, camera, kFrame, sky);
    sky.alpha = light.weights[0];
    horizon_.Draw(d, camera, kFrame, sky);
    gfx::DrawStyle world = Course_WorldStyle(light);
    for (int b = 0; b < kBlocks - 1; ++b) blocks_[b].Draw(d, camera, kFrame, world);
    if (cars) cars();
    blocks_[kBlocks - 1].Draw(d, camera, kFrame, world);
}

void Course::Field_LoadClouds(gfx::Draw2D& draw, const std::string& dataDir) {
    clouds_ = gfx::Model();
    clouds_.Load(dataDir + "\\models\\" + ModelDir(id_) + "\\clouds.obj", draw);
}

void Course::Cloud_DrawView(gfx::Draw2D& d, const gfx::Camera3D& camera, const RaceLight& light,
                            const std::function<void()>& cars) const {
    gfx::DrawStyle sky;
    sky.offset[0] = camera.eye[0];
    sky.offset[2] = camera.eye[2];
    sky.state.depthTest = false;
    sky.state.depthWrite = false;
    Light_SkyTint(light, sky.tint);
    sky_.Draw(d, camera, kFrame, sky);
    sky.tint[0] = sky.tint[1] = sky.tint[2] = 1;
    sky.alpha = light.weights[2];
    clouds_.Draw(d, camera, kFrame, sky);
    gfx::DrawStyle world = Course_WorldStyle(light);
    for (int b = 0; b < kBlocks - 1; ++b) blocks_[b].Draw(d, camera, kFrame, world);
    if (cars) cars();
    blocks_[kBlocks - 1].Draw(d, camera, kFrame, world);
}

void Course::Surfaces(float x, float z, std::vector<float>& heights) const {
    heights.clear();
    // Each block holds the ground of its own cell, but a surface can reach a little into the next one.
    for (int b = 0; b < 64; ++b) {
        float cx = (b & 7) * 200.0f, cz = (b >> 3) * 200.0f;
        if (x < cx - 20 || x > cx + 220 || z < cz - 20 || z > cz + 220) continue;
        const gfx::Mesh& m = blocks_[b].GetMesh();
        for (size_t i = 0; i + 3 <= m.indices.size(); i += 3) {
            const float* p0 = m.vertices[m.indices[i]].pos;
            const float* p1 = m.vertices[m.indices[i + 1]].pos;
            const float* p2 = m.vertices[m.indices[i + 2]].pos;
            // Barycentric coordinates of (x, z) in the triangle's projection on the ground.
            float d = (p1[2] - p2[2]) * (p0[0] - p2[0]) + (p2[0] - p1[0]) * (p0[2] - p2[2]);
            if (d > -1e-6f && d < 1e-6f) continue;
            float u = ((p1[2] - p2[2]) * (x - p2[0]) + (p2[0] - p1[0]) * (z - p2[2])) / d;
            float v = ((p2[2] - p0[2]) * (x - p2[0]) + (p0[0] - p2[0]) * (z - p2[2])) / d;
            float w = 1.0f - u - v;
            if (u < 0 || v < 0 || w < 0) continue;
            heights.push_back(u * p0[1] + v * p1[1] + w * p2[1]);
        }
    }
    std::sort(heights.begin(), heights.end());
}

}  // namespace game
