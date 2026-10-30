// 3D models for the reimplemented screens: a mesh from data/models with its textures, drawn through a camera
// into a window of the frame. The mesh is kept on the graphics card, where render.hlsl's VSMesh transforms, lights
// and projects it and the card cuts it at the near plane. Shadows (DrawStyle::cornerY) and DrawWorldTriangles still
// do that on the CPU and hand Draw2D frame-space triangles with perspective-correct texture coordinates (STQ, as the
// GS draws them).
#pragma once

#include <functional>
#include <string>
#include <vector>

#include "draw2d.h"
#include "mesh.h"

namespace gfx {

// A camera in world space: `right`, `up` and `forward` are its axes (forward = into the screen), `focalX/Y` are
// pixels per unit at distance 1 and `centerX/Y` is where the view axis meets the frame (640x448 coordinates).
struct Camera3D {
    float eye[3];
    float right[3], up[3], forward[3];
    float focalX, focalY;
    float centerX, centerY;
};

// The cars' lighting, VU1 program 0x20: the vertex normal, turned into the world with the model, is dotted with
// three world directions (the light matrix vf05-07). Lights 1 and 2 shine by the positive dot products d1 and d2,
// the highlight by d3 to the 8th power times the vertex's specular strength:
//   colour = min(vertex colour x paint x (light 1 x d1 + light 2 x d2 + ambient) + highlight x specular x d3^8, 1)
// with the specular strength on the 0-255 scale (so divided by 255 here).
struct Lighting {
    float dir[3][3];     // light 1, light 2, highlight
    float color[4][3];   // light 1, light 2, highlight, ambient (vf09-12)
    float paint[2][3];   // paint colours 1 and 2 (vf17, vf18), for parts with paint (mesh.h); others take white
};

// How a model is drawn beyond its mesh: what the VU1 programs and the GS state around a draw add. The defaults draw
// a model as the course select does (plain vertex colour, alpha 1, depth cleared first).
struct DrawStyle {
    float colorScale = 1.0f;   // vf09.x: weights the vertex colour and alpha (clamped to 255 and 1)
    // vf09 of programs 0x30/0x40 when set: the vertex colour and alpha are weights[0] x day + weights[1] x dusk +
    // weights[2] x night (mesh.h), clamped to 255 and 1. Meshes without the dusk and night sets take the sum of the
    // weights x their colour.
    const float* weights = nullptr;
    // Distance cue on the view depth w, as programs 0x30/0x40 work it out from vf11 and vf10 (x, y, z, w) =
    // (alphaMax, fogMax, scale, end): alpha *= clamp((end - w) * scale, 0, alphaMax) / 128 and, with fog,
    // F = clamp((fogEnd - w) * fogScale, 0, fogMax) / 255 (1 = no fog); vertices with MeshVertex::cueFog take end
    // and scale for fogEnd and fogScale (their strips' headers pick vf11 over vf10).
    bool cue = false;
    float alphaMax = 128.0f, fogMax = 255.0f, scale = 0.5f, end = 800.0f;
    bool fog = false;
    float fogScale = 0.5f, fogEnd = 800.0f;
    uint32_t fogColor = 0;   // PS2 RGBA word (FOGCOL)
    Draw3DState state;       // alpha test and depth
    bool clearDepth = true;
    float offset[3] = {0, 0, 0};   // added to every position (the sky follows the camera)
    // The model's place: 3x4, rows (x, y, z) of [rotation | translation], so world = transform x (pos, 1). Null:
    // positions are already the world's.
    const float* transform = nullptr;
    const Lighting* lighting = nullptr;   // light the vertices as program 0x20 does (the mesh needs normals)
    float alpha = 1.0f;                   // scales the vertex alpha
    float tint[3] = {1, 1, 1};            // scales the vertex colour (program 0x60's vf31.xyz)
    // Program 0x50's corner heights (vf18): when set, each vertex's world height is replaced by one of four, picked by
    // its side and end in the model (front left, front right, rear left, rear right: x < 0 or not, z > 0 or not).
    // The cars' shadows lie on the ground under the car's corners this way.
    const float* cornerY = nullptr;
    // Transform on the CPU (a model loaded from a Mesh): its triangles go as the renderer's joinable draws, so many
    // small models drawn alike (the field's palm fronds) make one draw call instead of one each.
    bool cpu = false;
    // Draws the textured parts with this picture instead of their own (the tires' palette by the wheel colour).
    const Texture* texture = nullptr;
};

// A vertex for triangles built each frame in world space (the race's particles): colour and alpha 0-1 on the mesh
// scale (0.5 = texture as is, mesh.h).
struct WorldVertex {
    float pos[3];
    float uv[2];
    float color[4];
};

// Draws triangles given in world space as Model::Draw draws a part: cut at the near plane, projected through the
// camera, clipped to `window`, with the style's colour scale, alpha and state (its transform, lighting and distance
// cue aside, and no depth clear).
void DrawWorldTriangles(Draw2D& draw, const Camera3D& camera, const Rect& window, const WorldVertex* v, uint32_t count,
                        const Texture* texture, const DrawStyle& style);

// A model's vertices made ready for the graphics card without it (Model::Prepare, which can run on another thread),
// as one or more sections that can be drawn on their own: what Model::Load(PreparedModel&&) puts on the card as one
// vertex buffer.
struct PreparedModel {
    struct Part {
        uint32_t first, count;   // of `vertices`
        std::string texture;     // DDS file; empty = untextured
        bool clampU, clampV;
        bool opaque;             // no see-through texels and every vertex's alpha 1
        bool ordered;            // keeps its place among the section's parts (translucent texels or vertices)
        uint32_t section;        // of `sections`
    };
    struct Section {
        uint32_t firstPart, parts;   // of `parts`
        float min[3], max[3];        // bounds
    };
    std::vector<MeshVertex3D> vertices;   // triangle list
    std::vector<Part> parts;
    std::vector<Section> sections;
    bool timeColors = false;
    bool merged = false;   // Model::MergeSections: the parts by texture across the sections (Section::firstPart unused)
};

class Model {
public:
    Model() {}
    ~Model();
    Model(Model&& o);
    Model& operator=(Model&& o);
    Model(const Model&) = delete;
    Model& operator=(const Model&) = delete;

    // Loads an OBJ (and its .mtl and textures) and puts the mesh on the graphics card. False if the file is missing
    // or empty.
    bool Load(const std::string& path, Draw2D& draw);
    // The same with a mesh LoadObj has already read (on another thread, say): its textures and the upload. False if
    // it's empty.
    bool Load(Mesh&& mesh, Draw2D& draw);
    // The CPU's half of loading `mesh` (whose textures it names, `translucent` telling which have see-through texels):
    // its vertices in drawing order, added to `out` as a section of its own. It needs no graphics device, so it can
    // run on another thread. False if the mesh's faces are broken.
    static bool Prepare(const Mesh& mesh, const std::function<bool(const std::string&)>& translucent,
                        PreparedModel& out);
    // Reorders a prepared model of many sections (the field's cells) for drawing many at once: the parts whose order
    // doesn't matter grouped by texture and wrap across the sections, each group in section order, then the ordered
    // parts section by section. DrawSections then draws a group's parts in view as one call.
    static void MergeSections(PreparedModel& prepared);
    // The other half: the prepared sections' textures and one upload. The model keeps no copy of the mesh on the CPU,
    // so it draws only on the card (no DrawStyle::cornerY, GetMesh empty). False if nothing went to the card.
    bool Load(PreparedModel&& prepared, Draw2D& draw);
    bool Empty() const { return mesh_.indices.empty() && gpuParts_.empty(); }
    const Mesh& GetMesh() const { return mesh_; }
    int Sections() const { return (int)sections_.size(); }
    // A section's bounds (the whole model's for -1); false if it has nothing.
    bool Bounds(int section, float* lo, float* hi) const;

    // Draws with depth testing, clipped to `window`. Vertex colours are the PS2's (colour 0.5 = texture as is, see
    // mesh.h); textured parts blend by the texture's alpha. Billboard vertices turn about their anchor to face the
    // camera's direction. Nothing is drawn if the model's bounds are outside the view.
    void Draw(Draw2D& draw, const Camera3D& camera, const Rect& window, const DrawStyle& style = DrawStyle()) const;
    // Draws one section of a prepared model (Load(PreparedModel&&)) as Draw does, if its bounds are in view.
    void DrawSection(Draw2D& draw, const Camera3D& camera, const Rect& window, const DrawStyle& style,
                     int section) const;
    // Draws the sections with `wanted` set that are in view, of a model loaded from a merged PreparedModel
    // (MergeSections): each group's parts from its first section in view to its last as one draw (what's between and
    // out of view the card clips), then the ordered parts of those sections. `exact`: a section not wanted is never
    // drawn, so a run stops at it (sections that stand in for one another, My City's lots and buildings).
    void DrawSections(Draw2D& draw, const Camera3D& camera, const Rect& window, const DrawStyle& style,
                      const bool* wanted, bool exact = false) const;

private:
    // Whether the box (lo, hi) can be in the window; `farthest` (if given) gets its farthest corner's view depth.
    bool Visible(const Camera3D& c, const Rect& window, const DrawStyle& st, const float* lo, const float* hi,
                 float* farthest = nullptr) const;
    void Upload(PreparedModel&& prepared, Draw2D& draw);
    void Reset();
    void Release();
    // `farthest`: the parts' farthest view depth, which tells whether the distance cue can have started.
    void DrawGpu(Draw2D& draw, const Camera3D& c, const Rect& window, const DrawStyle& st, uint32_t firstPart,
                 uint32_t parts, float farthest) const;
    // VSMesh's constants for drawing with `st` through `c` (render.hlsl, from c1).
    void MeshConstants(const Camera3D& c, const DrawStyle& st, float (*k)[4]) const;
    // The CPU's way, for what the mesh shader doesn't do (cornerY) or a mesh the card didn't take.
    void DrawCpu(Draw2D& draw, const Camera3D& c, const Rect& window, const DrawStyle& st) const;

    // The draws of the mesh on the card, as few as can be with the same picture: the parts whose order doesn't matter
    // (no translucent texels, vertex alpha 1) share one draw per texture and wrap, paint aside (it's per vertex there),
    // within each run of them between translucent parts, which keep their place.
    struct GpuPart {
        uint32_t first, count;   // vertices of gpu_
        const Texture* texture;
        bool clampU, clampV, opaque, ordered;
        uint32_t section;
    };

    Draw2D* draw_ = nullptr;   // where the mesh was made
    MeshId gpu_ = 0;           // the parts' vertices in gpuParts_ order
    std::vector<GpuPart> gpuParts_;
    struct Section {
        uint32_t firstPart, parts;   // of gpuParts_
        float min[3], max[3];
    };
    std::vector<Section> sections_;
    bool merged_ = false;   // loaded from a merged PreparedModel
    Mesh mesh_;   // empty for a prepared model
    bool timeColors_ = false;   // dusk and night colours (Mesh::hasTimeColors)
    std::vector<const Texture*> textures_;   // per part; null = untextured
    std::vector<uint8_t> paint_;             // per vertex: its part's paint (0 = none)
    float min_[3], max_[3];                   // bounds (billboards: with room to turn)
};

}  // namespace gfx
