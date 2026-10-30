// Drawing for the reimplemented screens, on top of gfx::Renderer, in the game's 640x448 frame coordinates.
//
// Colours are PS2 RGBA words as the game writes them (0xAABBGGRR, 0x80 = 1.0), so constants from the original
// code carry over unchanged. Textures come from the PC texture set by game file and number.
//
// Draw2D also owns the frame: two render targets, and the previous-frame blend the game applies after every
// frame (Frame_BlendPrevious, 0x00208120): the last frame drawn over the new one at a fixed weight, which gives
// the logos and title their soft trail.
//
// Wide screens (the PC's Display settings): the frame can be wider than 640 (FrameWidth, 448 high as ever, shown
// at FrameWidth / 480 wide to 1 high), the 640-wide picture the game draws in its middle by default. Everything
// keeps the game's coordinates; an anchor (AnchorScope) moves what's drawn under it to the frame's left or right
// edge instead (the HUD's corners, the menus' side windows), and the frame's edges are at Left() and Right() in
// them. A fill across the whole 640 (a fade), and a 3D window or clip across it, reach the frame's edges, so the
// 3D views see more to the sides at the same focal (Widen). A screen made of 4:3 pictures (the title's, a room's)
// calls Pillarbox instead: that frame keeps to the 640 with black at the sides.
#pragma once

#include <stdint.h>

#include <map>
#include <string>
#include <vector>

#include "render/renderer.h"
#include "textures/texture_set.h"

namespace gfx {

struct Texture {
    TextureId id;
    int width, height;   // texel size the game addresses (the manifest size, even if a mod's DDS is larger)
    // Some texel isn't opaque enough for the race's alpha test (0x7F), fully transparent ones included: filtering
    // makes the edges of a cut-out in-between too. Such pixels blend without writing depth, so what's drawn after them
    // matters (gfx::Model keeps their parts in order).
    bool translucent;
};
// Texture::translucent for B,G,R,A pixels (alpha 255 = 1.0, so the alpha test's 0x7F is 254).
bool PixelsTranslucent(const std::vector<uint32_t>& pixels);

// GS state for 3D triangles beyond the texture.
struct Draw3DState {
    bool gsAlphaTest = false;   // the race's TEST: alpha >= 0x7F draws fully, lower alpha draws colour only (no depth)
    bool depthTest = true, depthWrite = true;
    uint32_t fogColor = 0;      // PS2 RGBA word, for vertices with fog
    bool additive = false;      // adds the colour by its alpha (ALPHA 0x48: Cs x As + Cd) instead of blending over
    bool alphaPasses = false;   // with gsAlphaTest: every pixel's alpha is known to pass (no colour-only pass needed)
    // With gsAlphaTest, Graphics Low (LowDetail): a part without see-through texels, which only the distance cue can
    // fail, draws its faded pixels in its one pass, blended and writing depth, instead of in a colour-only second one.
    bool onePass = false;
    bool alphaOnly = false;     // writes only the frame's alpha (the headlight beams' marks for Darken)
};

// Where an AnchorScope puts the game's 640-wide picture in a wider frame.
enum class Anchor { Left, Center, Right };

class Draw2D {
public:
    static const int kWidth = 640;
    static const int kHeight = 448;

    Draw2D(Renderer& renderer, const TextureSet* textures);
    ~Draw2D();

    // A texture from the texture set by game file, number within it and palette; loaded once. id is 0 if missing.
    const Texture& Get(const std::string& source, int number, int palette = 0);
    // The pixels (B,G,R,A) and size of a texture-set entry, for a picture made from them (GetFile with pixels).
    // False if it's missing.
    bool ReadPixels(const std::string& source, int number, int palette, std::vector<uint32_t>& pixels, int& width,
                    int& height) const;
    // A texture from a DDS file (e.g. one a model's material names); loaded once. id is 0 if missing. Paths that
    // name the same file ("a\b\..\c" and "a/c") share it.
    const Texture& GetFile(const std::string& path);
    // GetFile with the DDS's pixels already read (LoadDds, on another thread, say): the texture is made from them
    // unless that file is in already.
    const Texture& GetFile(const std::string& path, const std::vector<uint32_t>& pixels, int width, int height);
    // Replaces the pixels of a texture GetFile loaded (B,G,R,A, alpha 255 = 1.0, the DDS's own size): what the
    // GS's local copies do to the course's animated textures. Every model drawing with it sees the change.
    void WriteFile(const std::string& path, const uint32_t* pixels);

    // The frame's width (640 or more, even), its targets made again at it (empty) when it changes; and its aspect as
    // shown.
    void SetFrameWidth(int width);
    int FrameWidth() const { return frameWidth_; }
    float Aspect() const { return frameWidth_ / 480.0f; }
    // Whether the left and right anchors go to the frame's edges (the Display settings' HUD: Edges), or stay with
    // the centre's (Centred).
    void SetEdgeAnchors(bool on);
    // The anchor drawing is under (AnchorScope sets it), and the frame's edges in the game's x under it.
    Anchor GetAnchor() const { return anchor_; }
    void SetAnchor(Anchor a);
    float Left() const { return (float)-origin_; }
    float Right() const { return (float)(frameWidth_ - origin_); }
    // `r` in the game's coordinates, or the frame's whole width if it spans the 640 (x0 <= 0, x1 >= 640).
    Rect Widen(const Rect& r) const;
    // This frame keeps to the 640 in the middle, black at the sides: no widening, every anchor the centre's. Each
    // frame starts without (BeginFrame).
    void Pillarbox();
    bool Pillarboxed() const { return pillarbox_; }

    // Starts a frame cleared to `rgba`.
    void BeginFrame(uint32_t rgba);
    // Starts a frame from the last one shown instead, as the game does while it loads (frame flag 0x400000: the
    // display keeps the old picture and new drawing goes over it). The copy is filtered and moved `shift` pixels
    // right and down; alternating +-0.5 blurs the picture a little more each frame without moving it.
    void BeginFrameFromLast(float shift);
    // Blends the previous frame over this one with weight `blend` (0x80 = 1.0, 0 = off) and shows it.
    void EndFrame(uint8_t blend);

    // A textured rectangle: (u, v, uw, vh) is the source rectangle in texels. `rgba` modulates (0x80 = 1.0).
    // Filtering keeps to the rectangle's texels (to half a texel inside its edges), as the game's sprites do with the
    // GS's REGION_CLAMP (Sprite_Emit): an atlas piece's neighbours never bleed in, whatever the window's scale.
    // `linear` false samples the nearest texel (pixel fonts drawn at a whole multiple of their size).
    void Sprite(const Texture& t, float x, float y, float w, float h, float u, float v, float uw, float vh,
                uint32_t rgba = 0x80808080, bool linear = true);
    // The same source rectangle on any four corners (top left, top right, bottom left, bottom right): a turned or
    // stretched sprite, such as a gauge's needle.
    void Quad(const Texture& t, const float x[4], const float y[4], float u, float v, float uw, float vh,
              uint32_t rgba = 0x80808080, bool linear = true);
    // A rectangle of colour, alpha blended. One across the whole 640 reaches the frame's edges.
    void Fill(float x, float y, float w, float h, uint32_t rgba);
    // The field's night (func_002207D0's last sprite): (v, v, v) taken off every pixel whose frame alpha is 0x80 or
    // more, as the GS's (0 - Cs) x Ad + Cd under DATE does (Ad is then 1). Pixels under 0x80 (the beams' 0, water,
    // the cars' shadows) keep their colour.
    void Darken(uint8_t v);
    // Triangles already in frame coordinates (z: 0..1, larger = nearer), depth tested and written.
    void Triangles(const Vertex* v, uint32_t count);
    // Triangles for 3D drawing (gfx::Model): depth tested and written, clipped to `clip`, alpha blended. With a
    // texture (null = untextured) the texture modulates the colour, its fully transparent texels are skipped and
    // each axis repeats or clamps. `state` changes the alpha test and depth and sets the fog colour.
    void Triangles(const Vertex* v, uint32_t count, const Texture* t, bool clampU, bool clampV, const Rect& clip,
                   const Draw3DState& state = Draw3DState());
    // Meshes kept on the graphics card (gfx::Model), and triangles [first, first + count) of one drawn as Triangles
    // above draws, positioned and coloured by the mesh vertex shader's constants (render.hlsl's VSMesh).
    MeshId CreateMesh(const MeshVertex3D* v, uint32_t count) { return renderer_.CreateMesh(v, count); }
    void DestroyMesh(MeshId id) { renderer_.DestroyMesh(id); }
    void DrawMesh(MeshId mesh, uint32_t first, uint32_t count, const float (*constants)[4], uint32_t constantCount,
                  const Texture* t, bool clampU, bool clampV, const Rect& clip, const Draw3DState& state);
    void ClearDepth() { renderer_.ClearDepth(); }
    // The 3D triangles' scissor (the GS's SCISSOR_2, which Race_DrawView sets for each half of a split screen): every
    // clip above is cut to it. ResetView3D puts back the whole frame.
    void SetView3D(const Rect& r) { view3D_ = r; }
    void ResetView3D() { view3D_ = Rect{0, 0, kWidth, kHeight}; }

    Renderer& GetRenderer() { return renderer_; }
    // The target EndFrame last showed.
    TargetId Shown() const { return frames_[current_ ^ 1]; }

    // Vertex colour from a PS2 RGBA word.
    static uint32_t Color(uint32_t rgba) {
        return (rgba & 0xFF00FF00u) | ((rgba & 0xFF) << 16) | ((rgba >> 16) & 0xFF);
    }

private:
    RenderState BaseState() const;
    // BaseState for a draw over the whole frame in its own pixels (no anchor).
    RenderState FrameState() const;
    void MakeFrames();
    bool State3D(const Texture* t, bool clampU, bool clampV, const Rect& clip, const Draw3DState& state,
                 RenderState& s) const;

    Renderer& renderer_;
    const TextureSet* textures_;
    std::map<std::string, Texture> cache_;
    TargetId frames_[2];
    int current_;
    Rect view3D_;
    int frameWidth_ = kWidth;
    Anchor anchor_ = Anchor::Center;
    bool edgeAnchors_ = true;
    bool pillarbox_ = false;
    int origin_ = 0;   // the frame's x of the game's x = 0 under the anchor
};

// Draws in scope are under `a`; the anchor before comes back after.
class AnchorScope {
public:
    AnchorScope(Draw2D& d, Anchor a) : d_(d), was_(d.GetAnchor()) { d.SetAnchor(a); }
    ~AnchorScope() { d_.SetAnchor(was_); }
    AnchorScope(const AnchorScope&) = delete;
    AnchorScope& operator=(const AnchorScope&) = delete;

private:
    Draw2D& d_;
    Anchor was_;
};

// How far from the projection's centre (frame pixels across) the 3D clippers' guard band reaches: the programs'
// 512, or more where a wide frame's edge is farther (render.hlsl, model.cpp's AddTriangle).
float GuardBandX();

// The Display settings' Graphics row: Low draws less for slow machines (docs/display.md, "Graphics"): the 3D views
// fade out by ViewEnd() (600 units, the PS2's 800 otherwise), the far faded scenery takes one pass (Draw3DState::
// onePass), EndFrame leaves out the previous frame's trail, and the cars skip their reflections and take their
// simpler bodies sooner.
void SetLowDetail(bool on);
bool LowDetail();
float ViewEnd();

}  // namespace gfx
