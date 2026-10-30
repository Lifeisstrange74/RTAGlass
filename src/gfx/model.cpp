#include "model.h"

#include <math.h>
#include <string.h>

#include <algorithm>

namespace gfx {

namespace {

const float kNear = 1.0f;   // view distance where geometry is cut, and where depth is 1

// A vertex in view space with what gets interpolated along a clipped edge.
struct ViewVertex {
    float x, y, z, u, v;
    float r, g, b, a;
    float cueFog;   // 1: fogs by the cue's numbers (MeshVertex::cueFog)
};

ViewVertex Lerp(const ViewVertex& a, const ViewVertex& b, float t) {
    ViewVertex o;
    o.x = a.x + (b.x - a.x) * t;
    o.y = a.y + (b.y - a.y) * t;
    o.z = a.z + (b.z - a.z) * t;
    o.u = a.u + (b.u - a.u) * t;
    o.v = a.v + (b.v - a.v) * t;
    o.r = a.r + (b.r - a.r) * t;
    o.g = a.g + (b.g - a.g) * t;
    o.b = a.b + (b.b - a.b) * t;
    o.a = a.a + (b.a - a.a) * t;
    o.cueFog = a.cueFog;
    return o;
}

Vertex Project(const ViewVertex& p, const Camera3D& c, const DrawStyle& st) {
    float q = 1.0f / p.z;
    Vertex o;
    o.x = c.centerX + c.focalX * p.x * q;
    o.y = c.centerY - c.focalY * p.y * q;
    o.z = kNear * q;
    float alpha = 128.0f * std::min(1.0f, p.a * st.colorScale * st.alpha);
    o.fog = 1;
    if (st.cue) {
        float d = (st.end - p.z) * st.scale;
        alpha *= std::min(std::max(d, 0.0f), st.alphaMax) / 128.0f;
        // Programs 0x30/0x40 fog a strip flagged in its header by vf11, the cue's end and scale, else by vf10.
        float end = p.cueFog > 0 ? st.end : st.fogEnd, scale = p.cueFog > 0 ? st.scale : st.fogScale;
        if (st.fog) o.fog = std::min(std::max((end - p.z) * scale, 0.0f), st.fogMax) / 255.0f;
    }
    float k = 255.0f * st.colorScale;
    uint32_t r = (uint32_t)std::min(255.0f, p.r * k + 0.5f), g = (uint32_t)std::min(255.0f, p.g * k + 0.5f),
             b = (uint32_t)std::min(255.0f, p.b * k + 0.5f), a = (uint32_t)(alpha + 0.5f);
    o.color = (a << 24) | (r << 16) | (g << 8) | b;
    o.s = p.u * q;
    o.t = p.v * q;
    o.q = q;
    return o;
}

// Adds a triangle, cut against the near plane and, as programs 0x30/0x40 clip (0x0720-0x0EE0), the guard band: x and
// y within w of their clip space, the frame's centre +-512 pixels across and +-481 down at a focal of 512 (across,
// farther where a wide frame's edge is: GuardBandX). The corners a cut makes get the colour interpolated in 3D and
// their own fog (Project); the GS then interpolates straight across the screen within what's left (render.hlsl, "the
// guard band"). A triangle inside is kept whole.
void AddTriangle(const ViewVertex* in, const Camera3D& c, const DrawStyle& st, std::vector<Vertex>& out) {
    const float gx = GuardBandX() / c.focalX, gy = 481.28f / c.focalY;
    auto distance = [&](const ViewVertex& v, int plane) {   // >= 0 inside
        switch (plane) {
        case 0: return v.z - kNear;
        case 1: return v.z * gx - v.x;
        case 2: return v.z * gx + v.x;
        case 3: return v.z * gy - v.y;
        default: return v.z * gy + v.y;
        }
    };
    ViewVertex poly[2][9];   // a triangle cut by five planes has at most eight corners
    int n = 3, cur = 0;
    for (int i = 0; i < 3; ++i) poly[0][i] = in[i];
    for (int plane = 0; plane < 5 && n >= 3; ++plane) {
        const ViewVertex* from = poly[cur];
        ViewVertex* to = poly[cur ^ 1];
        int m = 0;
        bool cut = false;
        for (int i = 0; i < n; ++i) {
            const ViewVertex& a = from[i];
            const ViewVertex& b = from[(i + 1) % n];
            float da = distance(a, plane), db = distance(b, plane);
            if (da >= 0) to[m++] = a;
            else cut = true;
            if ((da >= 0) != (db >= 0)) to[m++] = Lerp(a, b, da / (da - db));
        }
        if (!cut) continue;
        n = m;
        cur ^= 1;
    }
    for (int i = 2; i < n; ++i) {
        out.push_back(Project(poly[cur][0], c, st));
        out.push_back(Project(poly[cur][i - 1], c, st));
        out.push_back(Project(poly[cur][i], c, st));
    }
}

// The world position of `p` (a 3x4 transform, or none) plus `offset`.
void ToWorld(const float* m, const float* offset, const float* p, float* out) {
    for (int k = 0; k < 3; ++k) {
        float v = m ? m[k * 4] * p[0] + m[k * 4 + 1] * p[1] + m[k * 4 + 2] * p[2] + m[k * 4 + 3] : p[k];
        out[k] = v + offset[k];
    }
}

// A vertex's colour under program 0x20's lighting (see Lighting), 0-1.
void Light(const Lighting& l, const float* m, const MeshVertex& v, int paint, float* rgb) {
    float n[3];
    for (int k = 0; k < 3; ++k)
        n[k] = m ? m[k * 4] * v.normal[0] + m[k * 4 + 1] * v.normal[1] + m[k * 4 + 2] * v.normal[2] : v.normal[k];
    float d[3];
    for (int i = 0; i < 3; ++i) d[i] = std::max(0.0f, l.dir[i][0] * n[0] + l.dir[i][1] * n[1] + l.dir[i][2] * n[2]);
    float h = d[2] * d[2];
    h *= h;
    h *= h * v.specular / 255.0f;
    for (int c = 0; c < 3; ++c) {
        float p = paint ? l.paint[paint - 1][c] : 1.0f;
        float diffuse = l.color[0][c] * d[0] + l.color[1][c] * d[1] + l.color[3][c];
        rgb[c] = std::min(1.0f, v.color[c] * p * diffuse + l.color[2][c] * h);
    }
}

// A vertex's colour and alpha (0-1) under programs 0x30/0x40's weights (DrawStyle::weights).
void Blend(const float* w, const MeshVertex& v, bool timed, float* rgba) {
    for (int c = 0; c < 4; ++c) {
        float x = timed ? w[0] * v.color[c] + w[1] * v.dusk[c] + w[2] * v.night[c] : (w[0] + w[1] + w[2]) * v.color[c];
        rgba[c] = std::min(1.0f, x);
    }
}

}  // namespace

Model::~Model() { Release(); }

Model::Model(Model&& o)
    : draw_(o.draw_), gpu_(o.gpu_), gpuParts_(std::move(o.gpuParts_)), sections_(std::move(o.sections_)),
      mesh_(std::move(o.mesh_)), timeColors_(o.timeColors_), textures_(std::move(o.textures_)),
      paint_(std::move(o.paint_)) {
    memcpy(min_, o.min_, sizeof(min_));
    memcpy(max_, o.max_, sizeof(max_));
    o.gpu_ = 0;
}

Model& Model::operator=(Model&& o) {
    if (this == &o) return *this;
    Release();
    draw_ = o.draw_;
    gpu_ = o.gpu_;
    gpuParts_ = std::move(o.gpuParts_);
    sections_ = std::move(o.sections_);
    mesh_ = std::move(o.mesh_);
    timeColors_ = o.timeColors_;
    textures_ = std::move(o.textures_);
    paint_ = std::move(o.paint_);
    memcpy(min_, o.min_, sizeof(min_));
    memcpy(max_, o.max_, sizeof(max_));
    o.gpu_ = 0;
    return *this;
}

void Model::Release() {
    if (gpu_ && draw_) draw_->DestroyMesh(gpu_);
    gpu_ = 0;
    gpuParts_.clear();
}

namespace {

uint32_t ColorWord(const float* c) {
    uint32_t b[4];
    for (int k = 0; k < 4; ++k) b[k] = (uint32_t)(std::min(std::max(c[k], 0.0f), 1.0f) * 255.0f + 0.5f);
    return (b[3] << 24) | (b[0] << 16) | (b[1] << 8) | b[2];
}

// The mesh's bounds; a billboard vertex can turn to any side of its anchor.
void MeshBounds(const Mesh& mesh, float* lo, float* hi) {
    for (int k = 0; k < 3; ++k) {
        lo[k] = 1e30f;
        hi[k] = -1e30f;
    }
    for (const MeshVertex& v : mesh.vertices) {
        float r = 0;
        if (v.billboard) r = sqrtf((v.pos[0] - v.anchor[0]) * (v.pos[0] - v.anchor[0]) +
                                   (v.pos[2] - v.anchor[2]) * (v.pos[2] - v.anchor[2]));
        for (int k = 0; k < 3; ++k) {
            float c = v.billboard && k != 1 ? v.anchor[k] : v.pos[k];
            lo[k] = std::min(lo[k], c - (k != 1 ? r : 0));
            hi[k] = std::max(hi[k], c + (k != 1 ? r : 0));
        }
    }
}

// LoadObj gives every face its own vertices, so each vertex belongs to one part.
std::vector<uint8_t> PaintOf(const Mesh& mesh) {
    std::vector<uint8_t> paint(mesh.vertices.size(), 0);
    for (const MeshPart& p : mesh.parts)
        for (uint32_t i = p.first; i < p.first + p.count && i < mesh.indices.size(); ++i)
            if (mesh.indices[i] < paint.size()) paint[mesh.indices[i]] = (uint8_t)p.paint;
    return paint;
}

}  // namespace

bool Model::Prepare(const Mesh& mesh, const std::function<bool(const std::string&)>& translucent,
                    PreparedModel& out) {
    static const std::string kNone;
    // The parts as runs of `indices`, and whether each keeps its place.
    struct Run {
        uint32_t first, count;
        const std::string* texture;
        bool clampU, clampV, ordered;
        bool opaque;   // not ordered, and the dusk and night alphas 1 too
    };
    uint32_t total = (uint32_t)mesh.indices.size();
    std::vector<Run> runs;
    if (mesh.parts.empty()) runs.push_back({0, total, &kNone, false, false, false, true});
    for (const MeshPart& part : mesh.parts) {
        Run r = {part.first, part.first < total ? std::min(part.count, total - part.first) : 0, &part.texture,
                 part.clampU, part.clampV, !part.texture.empty() && translucent(part.texture), true};
        for (uint32_t k = r.first; k < r.first + r.count && !r.ordered; ++k)
            r.ordered = mesh.indices[k] < mesh.vertices.size() && mesh.vertices[mesh.indices[k]].color[3] < 0.995f;
        r.opaque = !r.ordered;
        for (uint32_t k = r.first; k < r.first + r.count && r.opaque && mesh.hasTimeColors; ++k) {
            const MeshVertex& v = mesh.vertices[mesh.indices[k]];
            r.opaque = v.dusk[3] >= 0.995f && v.night[3] >= 0.995f;
        }
        runs.push_back(r);
    }
    PreparedModel::Section section;
    section.firstPart = (uint32_t)out.parts.size();
    MeshBounds(mesh, section.min, section.max);
    size_t base = out.vertices.size();
    std::vector<uint32_t> order;   // indices of `indices`, in drawing order
    order.reserve(total);
    auto add = [&](const Run& r) {
        for (uint32_t k = r.first; k < r.first + r.count; ++k) order.push_back(k);
    };
    // The translucent parts keep their place; between two of them, the other parts group by texture and wrap (their
    // pixels all pass the alpha test and write depth, so their order among themselves doesn't show).
    std::vector<bool> done(runs.size(), false);
    for (size_t i = 0; i < runs.size(); ++i) {
        if (done[i]) continue;
        PreparedModel::Part g = {(uint32_t)(base + order.size()), 0, *runs[i].texture, runs[i].clampU,
                                 runs[i].clampV, runs[i].opaque, runs[i].ordered,
                                 (uint32_t)out.sections.size()};
        add(runs[i]);
        done[i] = true;
        for (size_t j = i + 1; j < runs.size() && !runs[i].ordered && !runs[j].ordered; ++j) {
            const Run& r = runs[j];
            if (done[j] || *r.texture != g.texture || r.clampU != g.clampU || r.clampV != g.clampV) continue;
            g.opaque = g.opaque && r.opaque;
            add(r);
            done[j] = true;
        }
        g.count = (uint32_t)(base + order.size()) - g.first;
        out.parts.push_back(g);
    }
    std::vector<uint8_t> paint = PaintOf(mesh);
    out.vertices.resize(base + order.size());
    for (size_t i = 0; i < order.size(); ++i) {
        uint32_t n = mesh.indices[order[i]];
        if (n >= mesh.vertices.size()) {
            out.vertices.resize(base);
            out.parts.resize(section.firstPart);
            return false;
        }
        const MeshVertex& m = mesh.vertices[n];
        MeshVertex3D& o = out.vertices[base + i];
        memcpy(o.pos, m.pos, sizeof(o.pos));
        memcpy(o.normal, m.normal, sizeof(o.normal));
        o.color = ColorWord(m.color);
        // Without dusk and night colours of its own a vertex keeps the day's at every time, which Draw's weights
        // make the same as a mesh without them; so meshes with and without can share a model.
        o.dusk = ColorWord(mesh.hasTimeColors ? m.dusk : m.color);
        o.night = ColorWord(mesh.hasTimeColors ? m.night : m.color);
        memcpy(o.uv, m.uv, sizeof(o.uv));
        memcpy(o.anchor, m.anchor, sizeof(m.anchor));
        o.anchor[1] = m.cueFog ? 1.0f : 0.0f;   // (a billboard turns about its anchor's vertical: y isn't needed)
        o.anchor[3] = m.billboard ? 1.0f : 0.0f;
        o.extra[0] = m.specular / 255.0f;
        o.extra[1] = paint[n] == 1 ? 1.0f : 0.0f;
        o.extra[2] = paint[n] == 2 ? 1.0f : 0.0f;
        // The triangle's other two corners, so the vertex shader can tell whether it crosses the guard band.
        uint32_t k = order[i], t = k - k % 3;
        for (int j = 1; j < 3; ++j) {
            uint32_t other = t + 2 < total ? mesh.indices[t + (k % 3 + j) % 3] : n;
            const float* p = other < mesh.vertices.size() ? mesh.vertices[other].pos : m.pos;
            memcpy(o.corners + 3 * (j - 1), p, 3 * sizeof(float));
        }
    }
    section.parts = (uint32_t)out.parts.size() - section.firstPart;
    out.sections.push_back(section);
    out.timeColors |= mesh.hasTimeColors;
    return true;
}

void Model::Upload(PreparedModel&& p, Draw2D& draw) {
    for (const PreparedModel::Part& part : p.parts)
        gpuParts_.push_back({part.first, part.count, part.texture.empty() ? nullptr : &draw.GetFile(part.texture),
                             part.clampU, part.clampV, part.opaque, part.ordered, part.section});
    sections_.clear();
    for (const PreparedModel::Section& s : p.sections) {
        Section o = {s.firstPart, s.parts};
        memcpy(o.min, s.min, sizeof(o.min));
        memcpy(o.max, s.max, sizeof(o.max));
        sections_.push_back(o);
        for (int k = 0; k < 3; ++k) {
            min_[k] = std::min(min_[k], s.min[k]);
            max_[k] = std::max(max_[k], s.max[k]);
        }
    }
    timeColors_ = p.timeColors;
    merged_ = p.merged;
    draw_ = &draw;
    gpu_ = p.vertices.empty() ? 0 : draw.CreateMesh(p.vertices.data(), (uint32_t)p.vertices.size());
    if (!gpu_) gpuParts_.clear();
}

void Model::Reset() {
    Release();
    textures_.clear();
    paint_.clear();
    sections_.clear();
    merged_ = false;
    mesh_ = Mesh();
    timeColors_ = false;
    for (int k = 0; k < 3; ++k) {
        min_[k] = 1e30f;
        max_[k] = -1e30f;
    }
}

bool Model::Load(const std::string& path, Draw2D& draw) {
    Mesh mesh;
    if (!LoadObj(path, mesh)) {
        Load(Mesh(), draw);
        return false;
    }
    return Load(std::move(mesh), draw);
}

bool Model::Load(Mesh&& mesh, Draw2D& draw) {
    Reset();
    mesh_ = std::move(mesh);
    if (mesh_.indices.empty()) return false;
    for (const MeshPart& p : mesh_.parts)
        textures_.push_back(p.texture.empty() ? nullptr : &draw.GetFile(p.texture));
    paint_ = PaintOf(mesh_);
    PreparedModel p;
    if (Prepare(mesh_, [&](const std::string& t) { return draw.GetFile(t).translucent; }, p)) {
        Upload(std::move(p), draw);
    } else {
        MeshBounds(mesh_, min_, max_);   // drawn on the CPU
    }
    timeColors_ = mesh_.hasTimeColors;
    return true;
}

bool Model::Load(PreparedModel&& prepared, Draw2D& draw) {
    Reset();
    Upload(std::move(prepared), draw);
    return !gpuParts_.empty();
}

// False if the bounding box is all behind the near plane or all beyond one edge of the window.
bool Model::Visible(const Camera3D& c, const Rect& window, const DrawStyle& st, const float* lo,
                    const float* hi, float* farthest) const {
    // The window's edges as slopes x/z and y/z in view space.
    float left = (window.x0 - c.centerX) / c.focalX, right = (window.x1 - c.centerX) / c.focalX;
    float top = (c.centerY - window.y0) / c.focalY, bottom = (c.centerY - window.y1) / c.focalY;
    int out[5] = {};
    float deepest = -1e30f, shallowest = 1e30f;
    for (int i = 0; i < 8; ++i) {
        float corner[3] = {i & 1 ? hi[0] : lo[0], i & 2 ? hi[1] : lo[1], i & 4 ? hi[2] : lo[2]};
        float p[3];
        ToWorld(st.transform, st.offset, corner, p);
        float d[3] = {p[0] - c.eye[0], p[1] - c.eye[1], p[2] - c.eye[2]};
        float x = d[0] * c.right[0] + d[1] * c.right[1] + d[2] * c.right[2];
        float y = d[0] * c.up[0] + d[1] * c.up[1] + d[2] * c.up[2];
        float z = d[0] * c.forward[0] + d[1] * c.forward[1] + d[2] * c.forward[2];
        deepest = std::max(deepest, z);
        shallowest = std::min(shallowest, z);
        out[0] += z < kNear;
        out[1] += x < left * z;
        out[2] += x > right * z;
        out[3] += y > top * z;
        out[4] += y < bottom * z;
    }
    if (farthest) *farthest = deepest;
    for (int k = 0; k < 5; ++k)
        if (out[k] == 8) return false;
    // Wholly past the distance cue's end, every vertex's alpha is 0: under the GS alpha test no pixel passes, and the
    // failing ones write colour only, blended by that 0 alpha, so nothing changes (the world's far blocks, a draw
    // call or two each).
    if (st.cue && st.state.gsAlphaTest && st.scale > 0 && shallowest >= st.end) return false;
    return true;
}

void Model::Draw(Draw2D& draw, const Camera3D& c, const Rect& window, const DrawStyle& st) const {
    float farthest;
    if (Empty() || !Visible(c, draw.Widen(window), st, min_, max_, &farthest)) return;
    if (st.cornerY || (st.cpu && !mesh_.indices.empty()) || !gpu_) {
        DrawCpu(draw, c, window, st);
        return;
    }
    DrawGpu(draw, c, window, st, 0, (uint32_t)gpuParts_.size(), farthest);
}

bool Model::Bounds(int section, float* lo, float* hi) const {
    if (section < 0) {
        memcpy(lo, min_, sizeof(min_));
        memcpy(hi, max_, sizeof(max_));
        return !Empty();
    }
    if (section >= (int)sections_.size() || !sections_[section].parts) return false;
    memcpy(lo, sections_[section].min, sizeof(min_));
    memcpy(hi, sections_[section].max, sizeof(max_));
    return true;
}

void Model::DrawSection(Draw2D& draw, const Camera3D& c, const Rect& window, const DrawStyle& st,
                        int section) const {
    if (!gpu_ || section < 0 || section >= (int)sections_.size()) return;
    const Section& s = sections_[section];
    float farthest;
    if (s.parts && Visible(c, draw.Widen(window), st, s.min, s.max, &farthest))
        DrawGpu(draw, c, window, st, s.firstPart, s.parts, farthest);
}

void Model::MeshConstants(const Camera3D& c, const DrawStyle& st, float (*k)[4]) const {
    memset(k, 0, 19 * sizeof(k[0]));
    // Model to view: the camera's axes times the model's transform; the translation brings in the offset and eye.
    static const float kIdentity[12] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0};
    const float* m = st.transform ? st.transform : kIdentity;
    const float* axes[3] = {c.right, c.up, c.forward};
    for (int r = 0; r < 3; ++r) {
        for (int j = 0; j < 4; ++j)
            k[r][j] = axes[r][0] * m[j] + axes[r][1] * m[4 + j] + axes[r][2] * m[8 + j];
        for (int i = 0; i < 3; ++i) k[r][3] += axes[r][i] * (st.offset[i] - c.eye[i]);
    }
    float fx = c.forward[0], fz = c.forward[2], len = sqrtf(fx * fx + fz * fz);
    k[3][0] = len > 0 ? fx / len : 0;
    k[3][1] = len > 0 ? fz / len : 1;
    k[3][2] = GuardBandX();
    k[4][0] = c.focalX;
    k[4][1] = c.focalY;
    k[4][2] = c.centerX;
    k[4][3] = c.centerY;
    k[5][0] = st.colorScale;
    k[5][1] = st.alpha;
    if (st.lighting) {
        const Lighting& l = *st.lighting;
        k[5][2] = 1;
        // The normal turns with the model, so the lights turn the other way: dir . (M n) = (M^T dir) . n.
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j)
                k[6 + i][j] = l.dir[i][0] * m[j] + l.dir[i][1] * m[4 + j] + l.dir[i][2] * m[8 + j];
        for (int i = 0; i < 4; ++i) memcpy(k[9 + i], l.color[i], sizeof(l.color[i]));
        for (int i = 0; i < 2; ++i) memcpy(k[13 + i], l.paint[i], sizeof(l.paint[i]));
    }
    if (st.weights && timeColors_) {
        memcpy(k[15], st.weights, 3 * sizeof(float));
    } else {
        k[15][0] = st.weights ? st.weights[0] + st.weights[1] + st.weights[2] : 1.0f;
    }
    memcpy(k[16], st.tint, sizeof(st.tint));
    if (st.cue) {
        k[17][0] = st.alphaMax;
        k[17][1] = st.scale;
        k[17][2] = st.end;
        k[17][3] = 1;
        if (st.fog) {
            k[18][0] = st.fogMax;
            k[18][1] = st.fogScale;
            k[18][2] = st.fogEnd;
            k[18][3] = 1;
        }
    }
}

// Whether every pixel of an opaque part nearer than `farthest` passes the GS alpha test: the failing pixels (drawn
// colour only) can only come from see-through texels, a vertex alpha under 1 or the distance cue, so the renderer can
// leave out the pass that draws them (a draw call each) for an opaque part wholly nearer than where the cue starts
// (alpha 0x80 down to 0x7F).
static bool FullAlpha(const DrawStyle& st, float farthest) {
    float weight = st.weights ? st.weights[0] + st.weights[1] + st.weights[2] : 1.0f;
    return st.colorScale * st.alpha * weight >= 1.0f &&
           (!st.cue || (st.alphaMax >= 127.0f && st.scale > 0 && farthest <= st.end - st.alphaMax / st.scale));
}

void Model::DrawGpu(Draw2D& draw, const Camera3D& c, const Rect& window, const DrawStyle& st, uint32_t firstPart,
                    uint32_t parts, float farthest) const {
    // VSMesh's constants (render.hlsl), from c1.
    float k[19][4];
    MeshConstants(c, st, k);
    if (st.clearDepth) draw.ClearDepth();
    Draw3DState state = st.state;
    state.fogColor = st.fogColor;
    bool fullAlpha = FullAlpha(st, farthest);
    for (uint32_t i = firstPart; i < firstPart + parts && i < gpuParts_.size(); ++i) {
        const GpuPart& p = gpuParts_[i];
        uint32_t count = p.count - p.count % 3;
        state.alphaPasses = fullAlpha && p.opaque;
        state.onePass = p.opaque && LowDetail();
        const Texture* t = p.texture && st.texture ? st.texture : p.texture;
        if (count) draw.DrawMesh(gpu_, p.first, count, k, 19, t, p.clampU, p.clampV, window, state);
    }
}

void Model::MergeSections(PreparedModel& p) {
    // The groups' order is the parts' first appearance; within a group, section order (Prepare added them so).
    struct Key {
        const std::string* texture;
        bool clampU, clampV, opaque;
    };
    std::vector<Key> keys;
    std::vector<std::vector<uint32_t>> groups;   // parts of each key
    std::vector<uint32_t> ordered;
    for (uint32_t i = 0; i < p.parts.size(); ++i) {
        const PreparedModel::Part& part = p.parts[i];
        if (part.ordered) {
            ordered.push_back(i);
            continue;
        }
        size_t g = 0;
        while (g < keys.size() && !(*keys[g].texture == part.texture && keys[g].clampU == part.clampU &&
                                    keys[g].clampV == part.clampV && keys[g].opaque == part.opaque))
            ++g;
        if (g == keys.size()) {
            keys.push_back({&part.texture, part.clampU, part.clampV, part.opaque});
            groups.emplace_back();
        }
        groups[g].push_back(i);
    }
    // The ordered parts last, in their sections' order (a translucent part drawn after the other sections' opaque
    // ones, which have written their depth, only shows more of what's behind it).
    groups.push_back(ordered);
    std::vector<MeshVertex3D> vertices;
    vertices.reserve(p.vertices.size());
    std::vector<PreparedModel::Part> parts;
    parts.reserve(p.parts.size());
    for (const std::vector<uint32_t>& g : groups) {
        for (uint32_t i : g) {
            PreparedModel::Part part = p.parts[i];
            uint32_t first = part.first;
            part.first = (uint32_t)vertices.size();
            vertices.insert(vertices.end(), p.vertices.begin() + first, p.vertices.begin() + first + part.count);
            parts.push_back(std::move(part));
        }
    }
    p.vertices.swap(vertices);
    p.parts.swap(parts);
    p.merged = true;
}

void Model::DrawSections(Draw2D& draw, const Camera3D& c, const Rect& window, const DrawStyle& st,
                         const bool* wanted, bool exact) const {
    if (!gpu_ || !merged_) return;
    // Which sections are in view, and how deep each reaches.
    std::vector<float> depth(sections_.size(), -1.0f);   // -1: not drawn
    const Rect wide = draw.Widen(window);
    for (size_t s = 0; s < sections_.size(); ++s) {
        float farthest;
        if (wanted[s] && sections_[s].parts && Visible(c, wide, st, sections_[s].min, sections_[s].max, &farthest))
            depth[s] = std::max(0.0f, farthest);
    }
    float k[19][4];
    MeshConstants(c, st, k);
    if (st.clearDepth) draw.ClearDepth();
    Draw3DState state = st.state;
    state.fogColor = st.fogColor;
    // A run: parts of one group from its first section in view to its last, drawn as one.
    const GpuPart* run = nullptr;
    uint32_t end = 0;
    float runDepth = 0;
    auto flush = [&]() {
        if (!run) return;
        uint32_t count = (end - run->first) - (end - run->first) % 3;
        state.alphaPasses = run->opaque && FullAlpha(st, runDepth);
        state.onePass = run->opaque && LowDetail();
        const Texture* t = run->texture && st.texture ? st.texture : run->texture;
        if (count) draw.DrawMesh(gpu_, run->first, count, k, 19, t, run->clampU, run->clampV, window, state);
        run = nullptr;
    };
    for (const GpuPart& p : gpuParts_) {
        bool sameGroup = run && !run->ordered && !p.ordered && p.texture == run->texture &&
                         p.clampU == run->clampU && p.clampV == run->clampV && p.opaque == run->opaque;
        if (!sameGroup) flush();
        float d = p.section < depth.size() ? depth[p.section] : -1.0f;
        if (d < 0) {   // out of view: drawn only if a later part of its group is in view
            if (exact && p.section < depth.size() && !wanted[p.section]) flush();
            continue;
        }
        if (run && sameGroup) {
            end = p.first + p.count;
            runDepth = std::max(runDepth, d);
        } else {
            run = &p;
            end = p.first + p.count;
            runDepth = d;
        }
        if (p.ordered) flush();
    }
    flush();
}

void Model::DrawCpu(Draw2D& draw, const Camera3D& c, const Rect& window, const DrawStyle& st) const {
    // Billboards turn about the vertical to face the way the camera looks.
    float fx = c.forward[0], fz = c.forward[2], len = sqrtf(fx * fx + fz * fz);
    if (len > 0) {
        fx /= len;
        fz /= len;
    } else {
        fz = 1;
    }
    std::vector<ViewVertex> view(mesh_.vertices.size());
    for (size_t i = 0; i < view.size(); ++i) {
        const MeshVertex& m = mesh_.vertices[i];
        float w[3] = {m.pos[0], m.pos[1], m.pos[2]};
        if (m.billboard) {
            // Stored facing -z: local x runs along the camera's right, local z along its forward.
            float lx = m.pos[0] - m.anchor[0], lz = m.pos[2] - m.anchor[2];
            w[0] = m.anchor[0] + lx * fz + lz * fx;
            w[2] = m.anchor[2] - lx * fx + lz * fz;
        }
        float p[3];
        ToWorld(st.transform, st.offset, w, p);
        if (st.cornerY) p[1] = st.cornerY[(m.pos[2] > 0 ? 0 : 2) + (m.pos[0] < 0 ? 0 : 1)] + st.offset[1];
        float d[3] = {p[0] - c.eye[0], p[1] - c.eye[1], p[2] - c.eye[2]};
        ViewVertex& v = view[i];
        v.x = d[0] * c.right[0] + d[1] * c.right[1] + d[2] * c.right[2];
        v.y = d[0] * c.up[0] + d[1] * c.up[1] + d[2] * c.up[2];
        v.z = d[0] * c.forward[0] + d[1] * c.forward[1] + d[2] * c.forward[2];
        v.u = m.uv[0];
        v.v = m.uv[1];
        v.cueFog = m.cueFog ? 1.0f : 0.0f;
        if (st.lighting) {
            float rgb[3];
            Light(*st.lighting, st.transform, m, paint_[i], rgb);
            v.r = rgb[0];
            v.g = rgb[1];
            v.b = rgb[2];
            v.a = m.color[3];
        } else if (st.weights) {
            float rgba[4];
            Blend(st.weights, m, mesh_.hasTimeColors, rgba);
            v.r = rgba[0];
            v.g = rgba[1];
            v.b = rgba[2];
            v.a = rgba[3];
        } else {
            v.r = m.color[0];
            v.g = m.color[1];
            v.b = m.color[2];
            v.a = m.color[3];
        }
        v.r *= st.tint[0];
        v.g *= st.tint[1];
        v.b *= st.tint[2];
    }
    if (st.clearDepth) draw.ClearDepth();
    Draw3DState state = st.state;
    state.fogColor = st.fogColor;
    std::vector<Vertex> out;
    size_t parts = mesh_.parts.empty() ? 1 : mesh_.parts.size();
    for (size_t p = 0; p < parts; ++p) {
        uint32_t first = 0, count = (uint32_t)mesh_.indices.size();
        const Texture* tex = nullptr;
        bool clampU = false, clampV = false;
        if (!mesh_.parts.empty()) {
            const MeshPart& part = mesh_.parts[p];
            first = part.first;
            count = part.count;
            tex = textures_[p] && st.texture ? st.texture : textures_[p];
            clampU = part.clampU;
            clampV = part.clampV;
        }
        out.clear();
        for (uint32_t i = first; i + 3 <= first + count && i + 3 <= mesh_.indices.size(); i += 3) {
            ViewVertex tri[3] = {view[mesh_.indices[i]], view[mesh_.indices[i + 1]], view[mesh_.indices[i + 2]]};
            AddTriangle(tri, c, st, out);
        }
        if (!out.empty()) draw.Triangles(out.data(), (uint32_t)out.size(), tex, clampU, clampV, window, state);
    }
}

void DrawWorldTriangles(Draw2D& draw, const Camera3D& c, const Rect& window, const WorldVertex* in, uint32_t count,
                        const Texture* texture, const DrawStyle& st) {
    std::vector<Vertex> out;
    for (uint32_t i = 0; i + 3 <= count; i += 3) {
        ViewVertex tri[3];
        for (int k = 0; k < 3; ++k) {
            const WorldVertex& w = in[i + k];
            float d[3] = {w.pos[0] - c.eye[0], w.pos[1] - c.eye[1], w.pos[2] - c.eye[2]};
            ViewVertex& v = tri[k];
            v.x = d[0] * c.right[0] + d[1] * c.right[1] + d[2] * c.right[2];
            v.y = d[0] * c.up[0] + d[1] * c.up[1] + d[2] * c.up[2];
            v.z = d[0] * c.forward[0] + d[1] * c.forward[1] + d[2] * c.forward[2];
            v.u = w.uv[0];
            v.v = w.uv[1];
            v.r = w.color[0];
            v.g = w.color[1];
            v.b = w.color[2];
            v.a = w.color[3];
            v.cueFog = 0;
        }
        AddTriangle(tri, c, st, out);
    }
    Draw3DState state = st.state;
    state.fogColor = st.fogColor;
    if (!out.empty()) draw.Triangles(out.data(), (uint32_t)out.size(), texture, true, true, window, state);
}

}  // namespace gfx
