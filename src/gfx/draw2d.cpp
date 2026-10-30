#include "draw2d.h"

#include <stdio.h>
#include <string.h>

#include <algorithm>

namespace gfx {

bool PixelsTranslucent(const std::vector<uint32_t>& px) {
    for (uint32_t c : px)
        if ((c >> 24) < 254) return true;
    return false;
}

namespace {
float g_guardBandX = 512.0f;
}

float GuardBandX() { return g_guardBandX; }

namespace {
bool g_lowDetail = false;
}

void SetLowDetail(bool on) { g_lowDetail = on; }
bool LowDetail() { return g_lowDetail; }
float ViewEnd() { return g_lowDetail ? 600.0f : 800.0f; }

Draw2D::Draw2D(Renderer& renderer, const TextureSet* textures)
    : renderer_(renderer), textures_(textures), current_(0), view3D_{0, 0, kWidth, kHeight} {
    MakeFrames();
}

void Draw2D::MakeFrames() {
    frames_[0] = renderer_.CreateTarget(frameWidth_, kHeight);
    frames_[1] = renderer_.CreateTarget(frameWidth_, kHeight);
}

void Draw2D::SetFrameWidth(int width) {
    width = width < kWidth ? kWidth : (width + 1) & ~1;
    if (width == frameWidth_) return;
    renderer_.DestroyTarget(frames_[0]);
    renderer_.DestroyTarget(frames_[1]);
    frameWidth_ = width;
    MakeFrames();
    // The farthest a frame edge gets from a projection's centre is the frame less the centre's 320 (under a side
    // anchor), with a little to spare.
    g_guardBandX = std::max(512.0f, (float)(frameWidth_ - kWidth / 2 + 16));
    SetAnchor(anchor_);
}

void Draw2D::SetEdgeAnchors(bool on) {
    edgeAnchors_ = on;
    SetAnchor(anchor_);
}

void Draw2D::SetAnchor(Anchor a) {
    anchor_ = a;
    int margin = (frameWidth_ - kWidth) / 2;
    if (!edgeAnchors_ || pillarbox_ || a == Anchor::Center) origin_ = margin;
    else origin_ = a == Anchor::Left ? 0 : 2 * margin;
}

void Draw2D::Pillarbox() {
    pillarbox_ = true;
    SetAnchor(anchor_);
}

Rect Draw2D::Widen(const Rect& r) const {
    Rect w = r;
    if (!pillarbox_ && r.x0 <= 0 && r.x1 >= kWidth) {
        w.x0 = -origin_;
        w.x1 = frameWidth_ - origin_;
    }
    return w;
}

Draw2D::~Draw2D() {
    for (auto& kv : cache_) renderer_.DestroyTexture(kv.second.id);
    renderer_.DestroyTarget(frames_[0]);
    renderer_.DestroyTarget(frames_[1]);
}

const Texture& Draw2D::Get(const std::string& source, int number, int palette) {
    char key[300];
    snprintf(key, sizeof(key), "%s#%d#%d", source.c_str(), number, palette);
    auto it = cache_.find(key);
    if (it != cache_.end()) return it->second;
    Texture t = {0, 1, 1, false};
    const TextureSet::Entry* e = textures_ ? textures_->Find(source, number, palette) : nullptr;
    std::vector<uint32_t> px;
    int w = 0, h = 0;
    if (e && textures_->ReadImage(*e, px, w, h)) {
        t.id = renderer_.CreateTexture(w, h, px.data());
        t.width = e->width;
        t.height = e->height;
        t.translucent = PixelsTranslucent(px);
    }
    return cache_[key] = t;
}

bool Draw2D::ReadPixels(const std::string& source, int number, int palette, std::vector<uint32_t>& pixels,
                        int& width, int& height) const {
    const TextureSet::Entry* e = textures_ ? textures_->Find(source, number, palette) : nullptr;
    return e && textures_->ReadImage(*e, pixels, width, height);
}

namespace {

// The cache's name for a file: backslashes, with "." and "dir\.." taken out.
std::string FileKey(const std::string& path) {
    std::vector<std::string> parts;
    std::string part;
    for (size_t i = 0; i <= path.size(); ++i) {
        char c = i < path.size() ? path[i] : '\\';
        if (c != '/' && c != '\\') {
            part += c;
            continue;
        }
        if (part == ".") {
        } else if (part == ".." && !parts.empty() && parts.back() != ".." && !parts.back().empty()) {
            parts.pop_back();
        } else if (!part.empty() || parts.empty()) {
            parts.push_back(part);
        }
        part.clear();
    }
    std::string key = "file:";
    for (size_t i = 0; i < parts.size(); ++i) key += (i ? "\\" : "") + parts[i];
    return key;
}

}  // namespace

void Draw2D::WriteFile(const std::string& path, const uint32_t* pixels) {
    auto it = cache_.find(FileKey(path));
    if (it != cache_.end() && it->second.id) renderer_.WriteTexture(it->second.id, pixels);
}

const Texture& Draw2D::GetFile(const std::string& path) {
    std::string key = FileKey(path);
    auto it = cache_.find(key);
    if (it != cache_.end()) return it->second;
    std::vector<uint32_t> px;
    int w = 0, h = 0;
    if (!LoadDds(path, px, w, h)) w = h = 0;
    return GetFile(path, px, w, h);
}

const Texture& Draw2D::GetFile(const std::string& path, const std::vector<uint32_t>& px, int w, int h) {
    std::string key = FileKey(path);
    auto it = cache_.find(key);
    if (it != cache_.end()) return it->second;
    Texture t = {0, 1, 1, false};
    if (w > 0 && h > 0 && px.size() >= (size_t)w * h) {
        t.id = renderer_.CreateTexture(w, h, px.data());
        t.width = w;
        t.height = h;
        t.translucent = PixelsTranslucent(px);
    }
    return cache_[key] = t;
}

RenderState Draw2D::BaseState() const {
    RenderState s;
    memset(&s, 0, sizeof(s));
    s.texFunc = TexFunc::Modulate;
    s.sampler.wrapU = s.sampler.wrapV = Wrap::Clamp;
    s.sampler.linear = true;
    s.blend.enable = true;   // (Cs - Cd) * As + Cd
    s.blend.a = BlendInput::Source;
    s.blend.b = BlendInput::Dest;
    s.blend.c = BlendFactor::SourceAlpha;
    s.blend.d = BlendInput::Dest;
    s.depth.func = Compare::Always;
    s.colorMask = 0xF;
    s.scissor.x1 = frameWidth_;
    s.scissor.y1 = kHeight;
    s.originX = (float)origin_;
    return s;
}

RenderState Draw2D::FrameState() const {
    RenderState s = BaseState();
    s.originX = 0;
    return s;
}

void Draw2D::BeginFrame(uint32_t rgba) {
    pillarbox_ = false;
    SetAnchor(anchor_);
    renderer_.ClearDepth();
    RenderState s = FrameState();
    s.blend.enable = false;
    Vertex v[6];
    float x[2] = {0, (float)frameWidth_}, y[2] = {0, (float)kHeight};
    static const int kCorner[6][2] = {{0, 0}, {1, 0}, {0, 1}, {1, 0}, {1, 1}, {0, 1}};
    for (int i = 0; i < 6; ++i) v[i] = {x[kCorner[i][0]], y[kCorner[i][1]], 0, Color(rgba), 0, 0, 1, 1};
    DrawBatch b = {frames_[current_], PrimKind::Triangles, s, v, 6};
    renderer_.Draw(b);
}

void Draw2D::BeginFrameFromLast(float shift) {
    pillarbox_ = false;
    SetAnchor(anchor_);
    renderer_.ClearDepth();
    RenderState s = FrameState();
    s.blend.enable = false;
    s.textureTarget = frames_[current_ ^ 1];
    s.texFunc = TexFunc::Decal;
    float du = shift / frameWidth_, dv = shift / kHeight;
    Vertex v[6];
    static const float kC[6][2] = {{0, 0}, {1, 0}, {0, 1}, {1, 0}, {1, 1}, {0, 1}};
    for (int i = 0; i < 6; ++i)
        v[i] = {kC[i][0] * frameWidth_, kC[i][1] * kHeight, 0, 0x80808080, kC[i][0] - du, kC[i][1] - dv, 1, 1};
    DrawBatch b = {frames_[current_], PrimKind::Triangles, s, v, 6};
    renderer_.Draw(b);
}

void Draw2D::EndFrame(uint8_t blend) {
    if (pillarbox_ && frameWidth_ > kWidth) {
        // The sides black, before the previous frame's blend (whose sides are too).
        RenderState s = FrameState();
        s.blend.enable = false;
        const uint32_t c = 0xFF000000u;
        float m = (float)((frameWidth_ - kWidth) / 2), r = (float)(frameWidth_ - m);
        Vertex q[12] = {
            {0, 0, 0, c, 0, 0, 1, 1}, {m, 0, 0, c, 0, 0, 1, 1}, {0, kHeight, 0, c, 0, 0, 1, 1},
            {m, 0, 0, c, 0, 0, 1, 1}, {m, kHeight, 0, c, 0, 0, 1, 1}, {0, kHeight, 0, c, 0, 0, 1, 1},
            {r, 0, 0, c, 0, 0, 1, 1}, {(float)frameWidth_, 0, 0, c, 0, 0, 1, 1}, {r, kHeight, 0, c, 0, 0, 1, 1},
            {(float)frameWidth_, 0, 0, c, 0, 0, 1, 1}, {(float)frameWidth_, kHeight, 0, c, 0, 0, 1, 1},
            {r, kHeight, 0, c, 0, 0, 1, 1},
        };
        DrawBatch b = {frames_[current_], PrimKind::Triangles, s, q, 12};
        renderer_.Draw(b);
    }
    if (blend && !g_lowDetail) {
        // (Cs - Cd) * FIX + Cd with the previous frame as Cs.
        RenderState s = FrameState();
        s.textureTarget = frames_[current_ ^ 1];
        s.texFunc = TexFunc::Decal;
        s.sampler.linear = false;
        s.blend.c = BlendFactor::Fixed;
        s.blend.fix = blend;
        s.colorMask = 7;
        Vertex v[6];
        static const float kC[6][2] = {{0, 0}, {1, 0}, {0, 1}, {1, 0}, {1, 1}, {0, 1}};
        for (int i = 0; i < 6; ++i)
            v[i] = {kC[i][0] * frameWidth_, kC[i][1] * kHeight, 0, 0x80808080, kC[i][0], kC[i][1], 1, 1};
        DrawBatch b = {frames_[current_], PrimKind::Triangles, s, v, 6};
        renderer_.Draw(b);
    }
    Rect r = {0, 0, frameWidth_, kHeight};
    renderer_.Present(frames_[current_], r, Aspect());
    current_ ^= 1;
}

void Draw2D::Sprite(const Texture& t, float x, float y, float w, float h, float u, float v, float uw, float vh,
                    uint32_t rgba, bool linear) {
    const float xs[4] = {x, x + w, x, x + w}, ys[4] = {y, y, y + h, y + h};
    Quad(t, xs, ys, u, v, uw, vh, rgba, linear);
}

void Draw2D::Quad(const Texture& t, const float x[4], const float y[4], float u, float v, float uw, float vh,
                  uint32_t rgba, bool linear) {
    if (!t.id) return;
    RenderState s = BaseState();
    s.texture = t.id;
    s.sampler.linear = linear;
    s.texAlpha = true;
    float s0 = u / t.width, t0 = v / t.height, s1 = (u + uw) / t.width, t1 = (v + vh) / t.height;
    uint32_t c = Color(rgba);
    Vertex q[6] = {
        {x[0], y[0], 0, c, s0, t0, 1, 1}, {x[1], y[1], 0, c, s1, t0, 1, 1}, {x[2], y[2], 0, c, s0, t1, 1, 1},
        {x[1], y[1], 0, c, s1, t0, 1, 1}, {x[3], y[3], 0, c, s1, t1, 1, 1}, {x[2], y[2], 0, c, s0, t1, 1, 1},
    };
    // The region: the rectangle's first and last texel centres on each axis (flipped rectangles too). A rectangle
    // under a texel wide (a strip stretched from its middle) keeps to its middle.
    auto Region = [](float a, float b, float size, float* lo, float* hi) {
        float l = (a < b ? a : b) + 0.5f, h = (a < b ? b : a) - 0.5f;
        if (l > h) l = h = (l + h) * 0.5f;
        *lo = l / size;
        *hi = h / size;
    };
    float r[4];
    Region(u, u + uw, (float)t.width, &r[0], &r[2]);
    Region(v, v + vh, (float)t.height, &r[1], &r[3]);
    for (Vertex& p : q) memcpy(p.region, r, sizeof r);
    DrawBatch b = {frames_[current_], PrimKind::Triangles, s, q, 6};
    renderer_.Draw(b);
}

void Draw2D::Fill(float x, float y, float w, float h, uint32_t rgba) {
    if (!pillarbox_ && x <= 0 && x + w >= kWidth) {
        x = Left();
        w = Right() - Left();
    }
    RenderState s = BaseState();
    uint32_t c = Color(rgba);
    Vertex q[6] = {
        {x, y, 0, c, 0, 0, 1, 1},         {x + w, y, 0, c, 0, 0, 1, 1}, {x, y + h, 0, c, 0, 0, 1, 1},
        {x + w, y, 0, c, 0, 0, 1, 1},     {x + w, y + h, 0, c, 0, 0, 1, 1},
        {x, y + h, 0, c, 0, 0, 1, 1},
    };
    DrawBatch b = {frames_[current_], PrimKind::Triangles, s, q, 6};
    renderer_.Draw(b);
}

// D3D9 has no destination alpha test, so the frame is sampled (the backend copies it first) and its alpha tested
// instead. HIGHLIGHT2 with a black vertex makes the colour the vertex alpha, v / 128, and FIX 64 brings that to
// v / 255 taken off: (0 - Cs) x FIX + Cd.
void Draw2D::Darken(uint8_t v) {
    RenderState s = FrameState();
    s.textureTarget = frames_[current_];
    s.texFunc = TexFunc::Highlight2;
    s.texAlpha = true;
    s.sampler.linear = false;
    s.alphaTest.enable = true;
    s.alphaTest.func = Compare::GreaterEqual;
    s.alphaTest.ref = 0x80;
    s.alphaTest.fail = AlphaFail::Keep;
    s.blend.a = BlendInput::Zero;
    s.blend.b = BlendInput::Source;
    s.blend.c = BlendFactor::Fixed;
    s.blend.fix = 64;
    s.blend.d = BlendInput::Dest;
    s.colorMask = 7;
    const uint32_t c = (uint32_t)v << 24;
    Vertex q[6];
    static const float kC[6][2] = {{0, 0}, {1, 0}, {0, 1}, {1, 0}, {1, 1}, {0, 1}};
    for (int i = 0; i < 6; ++i) q[i] = {kC[i][0] * frameWidth_, kC[i][1] * kHeight, 0, c, kC[i][0], kC[i][1], 1, 1};
    DrawBatch b = {frames_[current_], PrimKind::Triangles, s, q, 6};
    renderer_.Draw(b);
}

void Draw2D::Triangles(const Vertex* v, uint32_t count) {
    RenderState s = BaseState();
    s.blend.enable = false;
    s.depth.test = true;
    s.depth.func = Compare::GreaterEqual;
    s.depth.write = true;
    DrawBatch b = {frames_[current_], PrimKind::Triangles, s, v, count};
    renderer_.Draw(b);
}

// The state of Triangles' 3D triangles; false if nothing would be drawn.
bool Draw2D::State3D(const Texture* t, bool clampU, bool clampV, const Rect& clip, const Draw3DState& state,
                     RenderState& s) const {
    s = BaseState();
    if (t) {
        if (!t->id) return false;
        s.texture = t->id;
        s.texAlpha = true;
        s.sampler.wrapU = clampU ? Wrap::Clamp : Wrap::Repeat;
        s.sampler.wrapV = clampV ? Wrap::Clamp : Wrap::Repeat;
        s.alphaTest.enable = true;
        s.alphaTest.func = Compare::Greater;
        s.alphaTest.ref = 0;
        s.alphaTest.fail = AlphaFail::Keep;
    }
    if (state.gsAlphaTest) {
        s.alphaTest.enable = true;
        s.alphaTest.func = Compare::GreaterEqual;
        s.alphaTest.ref = 0x7F;
        s.alphaTest.fail = state.alphaPasses ? AlphaFail::Keep : AlphaFail::RgbOnly;
        if (state.onePass && !state.alphaPasses) {
            // Every pixel the cue leaves any alpha blends and writes depth, as in a plain blended draw.
            s.alphaTest.func = Compare::Greater;
            s.alphaTest.ref = 0;
            s.alphaTest.fail = AlphaFail::Keep;
        }
    }
    if (state.additive) s.blend.b = BlendInput::Zero;
    if (state.alphaOnly) s.colorMask = 8;
    s.depth.test = state.depthTest;
    s.depth.func = state.depthTest ? Compare::GreaterEqual : Compare::Always;
    s.depth.write = state.depthWrite;
    s.fogColor = state.fogColor & 0xFFFFFF;
    // In the frame's pixels: the game's moved by the anchor, within the frame.
    Rect c = Widen(clip), v = Widen(view3D_);
    s.scissor.x0 = std::max(std::max(c.x0, v.x0) + origin_, 0);
    s.scissor.y0 = std::max(c.y0, v.y0);
    s.scissor.x1 = std::min(std::min(c.x1, v.x1) + origin_, frameWidth_);
    s.scissor.y1 = std::min(c.y1, v.y1);
    return s.scissor.x0 < s.scissor.x1 && s.scissor.y0 < s.scissor.y1;
}

void Draw2D::Triangles(const Vertex* v, uint32_t count, const Texture* t, bool clampU, bool clampV, const Rect& clip,
                       const Draw3DState& state) {
    RenderState s;
    if (!State3D(t, clampU, clampV, clip, state, s)) return;
    DrawBatch b = {frames_[current_], PrimKind::Triangles, s, v, count};
    renderer_.Draw(b);
}

void Draw2D::DrawMesh(MeshId mesh, uint32_t first, uint32_t count, const float (*constants)[4],
                      uint32_t constantCount, const Texture* t, bool clampU, bool clampV, const Rect& clip,
                      const Draw3DState& state) {
    RenderState s;
    if (!State3D(t, clampU, clampV, clip, state, s)) return;
    MeshBatch b = {frames_[current_], s, mesh, first, count, constants, constantCount};
    renderer_.DrawMesh(b);
}

}  // namespace gfx
