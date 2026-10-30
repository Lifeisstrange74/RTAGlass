// The renderer interface: draw batches with our own render-state types (graphics plan, docs/graphics-plan.md).
// The game draws through it (gfx::Draw2D); the GS translator in tools/gsplay is another producer. The state is
// modelled on GS semantics so translated draws keep their exact meaning, but nothing here is a GS register.
//
// Units: positions are in render-target pixels (top-left origin), depth is 0..1 with larger = nearer, texture
// coordinates are normalized (s/q, t/q) over the bound texture. Colours are 8-bit with the PS2 scale: alpha, and
// vertex colour that modulates a texture, use 0x80 = 1.0; untextured RGB is used as is (0xFF = full). Texture
// alpha is 0..255 with 255 = 1.0.
#pragma once

#include <stdint.h>

namespace gfx {

typedef uint32_t TextureId;   // 0 = none
typedef uint32_t TargetId;    // 0 = none
typedef uint32_t MeshId;      // 0 = none

enum class PrimKind : uint8_t { Points, Lines, Triangles };

// Blend: (A - B) * C + D on colour, as on the GS.
enum class BlendInput : uint8_t { Source, Dest, Zero };
enum class BlendFactor : uint8_t { SourceAlpha, DestAlpha, Fixed };

struct BlendState {
    bool enable;
    BlendInput a, b, d;
    BlendFactor c;
    uint8_t fix;   // C when c == Fixed; 0x80 = 1.0
};

enum class Compare : uint8_t { Never, Always, Less, LessEqual, Equal, GreaterEqual, Greater, NotEqual };

// What a pixel that fails the alpha test still writes.
enum class AlphaFail : uint8_t { Keep, ColorOnly, DepthOnly, RgbOnly };

struct AlphaTest {
    bool enable;
    Compare func;
    uint8_t ref;   // PS2 scale (0x80 = 1.0)
    AlphaFail fail;
};

struct DepthState {
    bool test;
    Compare func;   // Never / Always / GreaterEqual / Greater (larger depth = nearer)
    bool write;
};

// How the texture combines with the vertex colour (GS TFX).
enum class TexFunc : uint8_t { Modulate, Decal, Highlight, Highlight2 };
enum class Wrap : uint8_t { Repeat, Clamp, Region };

struct Sampler {
    Wrap wrapU, wrapV;
    bool linear;
    float region[4];   // Region wrap: min s, min t, max s, max t (normalized)
};

struct Rect {
    int x0, y0, x1, y1;   // x1/y1 exclusive
};

struct RenderState {
    // Texture source: a texture, or a render target sampled as one (both 0 = untextured).
    TextureId texture;
    TargetId textureTarget;
    TexFunc texFunc;
    bool texAlpha;          // use the texture's alpha (GS TCC)
    Sampler sampler;
    BlendState blend;
    AlphaTest alphaTest;
    DepthState depth;
    uint32_t fogColor;      // 0x00BBGGRR
    uint8_t colorMask;      // bit 0..3 = write R, G, B, A
    Rect scissor;           // in the target's pixels
    // Added to every vertex's x (not the scissor's): where a wide frame puts the 640-wide picture's x = 0
    // (gfx::Draw2D's anchors).
    float originX;

    bool Textured() const { return texture != 0 || textureTarget != 0; }
};

struct Vertex {
    float x, y, z;
    uint32_t color;         // D3DCOLOR layout (0xAARRGGBB), PS2 scale
    float s, t, q;
    float fog;              // 1 = no fog, 0 = fog colour
    // A clamp on (s/q, t/q), normalized: min s, min t, max s, max t, as the GS's REGION_CLAMP per primitive (on top
    // of the state's Wrap::Region). Open unless set; Draw2D's sprites keep to their own texels with it. Being per
    // vertex rather than in RenderState, it doesn't stop sprites from one texture batching together.
    float region[4] = {-65536.0f, -65536.0f, 65536.0f, 65536.0f};
};

struct DrawBatch {
    TargetId target;
    PrimKind prim;
    RenderState state;
    const Vertex* vertices;
    uint32_t count;         // vertices; 1, 2 or 3 per primitive
};

// A vertex of a mesh kept by the renderer (gfx::Model): the model's own data, turned into frame coordinates and
// coloured on the graphics card by the mesh vertex shader (render.hlsl's VSMesh, which documents the constants).
struct MeshVertex3D {
    float pos[3];
    float normal[3];
    uint32_t color, dusk, night;   // D3DCOLOR layout (0xAARRGGBB), 0..1 as 0..255
    float uv[2];
    float anchor[4];   // a billboard's anchor (x, z); y = 1 if the vertex fogs by the cue's numbers
                       // (MeshVertex::cueFog); w = 1 for a billboard vertex, else 0
    float extra[3];    // specular strength (0-1), 1 if the part takes paint colour 1, 1 if paint colour 2
    float corners[6];  // the triangle's other two corners (the vertices aren't shared): the guard band test
};

// Triangles [first, first + count) of a mesh (a triangle list), drawn with `state` and the vertex shader's
// constants (c1 on, render.hlsl).
struct MeshBatch {
    TargetId target;
    RenderState state;
    MeshId mesh;
    uint32_t first, count;   // vertices
    const float (*constants)[4];
    uint32_t constantCount;
};

// What a draw is of, for --perf's draw calls by source: the drawing code says (DrawSourceScope), and the rest is 2D
// (the HUD, the menus, text).
enum class DrawSource { Other, World, Cars, Objects, Effects, Count };

class Renderer {
public:
    virtual ~Renderer() {}

    void SetSource(DrawSource s) { source_ = s; }
    DrawSource Source() const { return source_; }

    // Textures are A8R8G8B8 in B,G,R,A byte order, alpha 255 = 1.0.
    virtual TextureId CreateTexture(int width, int height, const uint32_t* pixels) = 0;
    virtual void DestroyTexture(TextureId id) = 0;
    // Replace a texture's pixels (its whole size, as created): an animated texture.
    virtual void WriteTexture(TextureId id, const uint32_t* pixels) = 0;

    // Render targets are colour buffers in PS2 pixels; the backend may store them at a higher resolution.
    virtual TargetId CreateTarget(int width, int height) = 0;
    virtual void DestroyTarget(TargetId id) = 0;
    // Replace a target's contents, e.g. from emulated GS memory (B,G,R,A, alpha 255 = 1.0).
    virtual void WriteTarget(TargetId id, const uint32_t* pixels) = 0;
    // Copy the shared part of one target into another (used when a target is resized).
    virtual void CopyTarget(TargetId from, TargetId to) = 0;
    virtual void ClearDepth() = 0;

    virtual void Draw(const DrawBatch& batch) = 0;

    // Meshes stay on the graphics card: made once, drawn with new constants each time.
    virtual MeshId CreateMesh(const MeshVertex3D* vertices, uint32_t count) = 0;
    virtual void DestroyMesh(MeshId id) = 0;
    virtual void DrawMesh(const MeshBatch& batch) = 0;

    // Show part of a target in the window, scaled to fit with its aspect kept at `aspect` (width / height as seen).
    virtual void Present(TargetId id, const Rect& source, float aspect = 4.0f / 3.0f) = 0;

protected:
    DrawSource source_ = DrawSource::Other;
};

// Draws made while one is in scope count as `source`; the one before comes back after.
class DrawSourceScope {
public:
    DrawSourceScope(Renderer& r, DrawSource source) : r_(r), was_(r.Source()) { r.SetSource(source); }
    ~DrawSourceScope() { r_.SetSource(was_); }
    DrawSourceScope(const DrawSourceScope&) = delete;
    DrawSourceScope& operator=(const DrawSourceScope&) = delete;

private:
    Renderer& r_;
    DrawSource was_;
};

}  // namespace gfx
