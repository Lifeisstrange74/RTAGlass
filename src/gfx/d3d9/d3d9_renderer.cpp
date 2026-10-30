#include "d3d9_renderer.h"

#include <algorithm>

#include <intrin.h>
#include <stdio.h>
#include <string.h>

// Shader bytecode generated from shaders/render.hlsl at build time.
#include "render_ps_color.h"
#include "render_ps_copy.h"
#include "render_ps_mesh_color.h"
#include "render_ps_mesh_texture.h"
#include "render_ps_texture.h"
#include "render_vs.h"
#include "render_vs_mesh.h"

namespace gfx {

namespace {

// Adds its scope's time stamp counter ticks to a --perf total, counted once when scopes nest (Draw's flush):
// cheap enough to take round every draw (a QueryPerformanceCounter can cost microseconds on old machines), and the
// caller turns ticks into milliseconds against the frame's own.
class Ticks {
public:
    Ticks(unsigned long long& total, int& depth) : total_(total), depth_(depth), start_(depth++ ? 0 : __rdtsc()) {}
    ~Ticks() {
        if (--depth_ == 0) total_ += __rdtsc() - start_;
    }
    Ticks(const Ticks&) = delete;
    Ticks& operator=(const Ticks&) = delete;

private:
    unsigned long long& total_;
    int& depth_;
    unsigned long long start_;
};

template <typename T> void SafeRelease(T*& p) {
    if (p) {
        p->Release();
        p = nullptr;
    }
}

const D3DVERTEXELEMENT9 kVertexElements[] = {
    {0, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
    {0, 12, D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR, 0},
    {0, 16, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0},
    {0, 28, D3DDECLTYPE_FLOAT1, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 1},
    {0, 32, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 2},
    D3DDECL_END(),
};
static_assert(sizeof(Vertex) == 48, "Vertex must match kVertexElements");

const D3DVERTEXELEMENT9 kMeshElements[] = {
    {0, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
    {0, 12, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL, 0},
    {0, 24, D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR, 0},
    {0, 28, D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR, 1},
    {0, 32, D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR, 2},
    {0, 36, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0},
    {0, 44, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 1},
    {0, 60, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 2},
    {0, 72, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 3},
    {0, 84, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 4},
    D3DDECL_END(),
};
static_assert(sizeof(MeshVertex3D) == 96, "MeshVertex3D must match kMeshElements");

D3DCMPFUNC DepthFunc(Compare c) {
    switch (c) {
    case Compare::Never: return D3DCMP_NEVER;
    case Compare::Always: return D3DCMP_ALWAYS;
    case Compare::Less: return D3DCMP_LESS;
    case Compare::LessEqual: return D3DCMP_LESSEQUAL;
    case Compare::Equal: return D3DCMP_EQUAL;
    case Compare::GreaterEqual: return D3DCMP_GREATEREQUAL;
    case Compare::Greater: return D3DCMP_GREATER;
    case Compare::NotEqual: return D3DCMP_NOTEQUAL;
    }
    return D3DCMP_ALWAYS;
}

// Alpha test weights for render.hlsl's c_atest (keep if x*d + y*|d| + z >= 0, with d = alpha - ref an integer).
void AlphaWeights(Compare c, float w[3]) {
    static const float k[8][3] = {
        {0, 0, -1},       // Never
        {0, 0, 1},        // Always
        {-1, 0, -0.5f},   // Less
        {-1, 0, 0.5f},    // LessEqual
        {0, -1, 0.5f},    // Equal
        {1, 0, 0.5f},     // GreaterEqual
        {1, 0, -0.5f},    // Greater
        {0, 1, -0.5f},    // NotEqual
    };
    memcpy(w, k[(int)c], sizeof(k[0]));
}

// One side of the blend equation as k1 + ka * factor, mapped to a D3D blend factor and a sign.
struct BlendTerm {
    D3DBLEND factor;
    bool negative;
};

BlendTerm Term(int k1, int ka, bool destAlpha) {
    D3DBLEND f = destAlpha ? D3DBLEND_DESTALPHA : D3DBLEND_SRCALPHA;
    D3DBLEND inv = destAlpha ? D3DBLEND_INVDESTALPHA : D3DBLEND_INVSRCALPHA;
    if (ka == 0) return {k1 ? D3DBLEND_ONE : D3DBLEND_ZERO, false};
    if (k1 == 0) return {f, ka < 0};
    if (ka < 0) return {inv, false};
    return {D3DBLEND_ONE, false};   // 1 + factor: not expressible; rare
}

}  // namespace

D3D9Renderer::D3D9Renderer()
    : d3d_(nullptr), device_(nullptr), pp_(), caps_(), separateAlpha_(false), softwareVertices_(false), vsync_(false),
      scale_(1), viewOrigin_(0),
      inScene_(false), meshShader_(false), decl_(nullptr), vs_(nullptr), psColor_(nullptr), psTexture_(nullptr),
      psCopy_(nullptr), meshDecl_(nullptr), vsMesh_(nullptr), psMeshColor_(nullptr), psMeshTexture_(nullptr),
      backBuffer_(nullptr), depth_(nullptr), depthWidth_(0),
      depthHeight_(0), scratch_(), bound_(0), stats() {}

D3D9Renderer::~D3D9Renderer() { Shutdown(); }

void D3D9Renderer::FillPresentParams(int backWidth, int backHeight, bool fullscreen, int refresh) {
    pp_.Windowed = fullscreen ? FALSE : TRUE;
    pp_.SwapEffect = D3DSWAPEFFECT_DISCARD;
    pp_.BackBufferFormat = fullscreen ? D3DFMT_X8R8G8B8 : D3DFMT_UNKNOWN;
    pp_.BackBufferWidth = backWidth;
    pp_.BackBufferHeight = backHeight;
    pp_.BackBufferCount = 1;
    pp_.FullScreen_RefreshRateInHz = fullscreen ? refresh : 0;
    pp_.PresentationInterval = vsync_ ? D3DPRESENT_INTERVAL_ONE : D3DPRESENT_INTERVAL_IMMEDIATE;
}

bool D3D9Renderer::Init(HWND hwnd, int backWidth, int backHeight, float scale, bool vsync, bool fullscreen,
                        int refresh) {
    scale_ = scale < 0.25f ? 0.25f : scale;
    vsync_ = vsync;
    d3d_ = Direct3DCreate9(D3D_SDK_VERSION);
    if (!d3d_) return false;

    FillPresentParams(backWidth, backHeight, fullscreen, refresh);
    pp_.hDeviceWindow = hwnd;

    // Prefer hardware vertex processing where the card runs vs_2_0; software still runs it on cards without.
    D3DCAPS9 caps;
    bool hardware = SUCCEEDED(d3d_->GetDeviceCaps(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, &caps)) &&
                    (caps.DevCaps & D3DDEVCAPS_HWTRANSFORMANDLIGHT) &&
                    caps.VertexShaderVersion >= D3DVS_VERSION(2, 0);
    const DWORD vpModes[] = {D3DCREATE_HARDWARE_VERTEXPROCESSING, D3DCREATE_SOFTWARE_VERTEXPROCESSING};
    for (DWORD vp : vpModes) {
        if (vp == D3DCREATE_HARDWARE_VERTEXPROCESSING && !hardware) continue;
        if (SUCCEEDED(d3d_->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hwnd, vp | D3DCREATE_FPU_PRESERVE, &pp_,
                                         &device_))) {
            softwareVertices_ = vp == D3DCREATE_SOFTWARE_VERTEXPROCESSING;
            break;
        }
    }
    if (!device_) return false;
    device_->GetDeviceCaps(&caps_);
    if (caps_.PixelShaderVersion < D3DPS_VERSION(2, 0)) return false;
    separateAlpha_ = (caps_.PrimitiveMiscCaps & D3DPMISCCAPS_SEPARATEALPHABLEND) != 0;

    if (FAILED(device_->CreateVertexDeclaration(kVertexElements, &decl_)) ||
        FAILED(device_->CreateVertexShader((const DWORD*)g_vsMain, &vs_)) ||
        FAILED(device_->CreatePixelShader((const DWORD*)g_psColor, &psColor_)) ||
        FAILED(device_->CreatePixelShader((const DWORD*)g_psTexture, &psTexture_)) ||
        FAILED(device_->CreatePixelShader((const DWORD*)g_psCopy, &psCopy_)) ||
        FAILED(device_->CreateVertexDeclaration(kMeshElements, &meshDecl_)) ||
        FAILED(device_->CreateVertexShader((const DWORD*)g_vsMesh, &vsMesh_)) ||
        FAILED(device_->CreatePixelShader((const DWORD*)g_psMeshColor, &psMeshColor_)) ||
        FAILED(device_->CreatePixelShader((const DWORD*)g_psMeshTexture, &psMeshTexture_)))
        return false;
    return CreateDeviceObjects();
}

std::string D3D9Renderer::Describe() const {
    if (!d3d_ || !device_) return "no device";
    D3DADAPTER_IDENTIFIER9 id = {};
    d3d_->GetAdapterIdentifier(D3DADAPTER_DEFAULT, 0, &id);
    D3DDISPLAYMODE mode = {};
    d3d_->GetAdapterDisplayMode(D3DADAPTER_DEFAULT, &mode);
    LARGE_INTEGER v = id.DriverVersion;
    char s[1024];
    snprintf(s, sizeof s,
             "%s (vendor %04lX device %04lX); driver %s %u.%u.%u.%u; %s vertex processing, vs %lu.%lu ps %lu.%lu, "
             "%u MB texture memory; back buffer %ux%u at scale %g, %s, vsync %s; desktop %ux%u at %u Hz",
             id.Description, id.VendorId, id.DeviceId, id.Driver, HIWORD(v.HighPart), LOWORD(v.HighPart),
             HIWORD(v.LowPart), LOWORD(v.LowPart), softwareVertices_ ? "software" : "hardware",
             D3DSHADER_VERSION_MAJOR(caps_.VertexShaderVersion), D3DSHADER_VERSION_MINOR(caps_.VertexShaderVersion),
             D3DSHADER_VERSION_MAJOR(caps_.PixelShaderVersion), D3DSHADER_VERSION_MINOR(caps_.PixelShaderVersion),
             device_->GetAvailableTextureMem() >> 20, pp_.BackBufferWidth, pp_.BackBufferHeight, (double)scale_,
             pp_.Windowed ? "windowed" : "full screen", vsync_ ? "on" : "off", mode.Width, mode.Height,
             mode.RefreshRate);
    return s;
}

bool D3D9Renderer::Reset(int backWidth, int backHeight, float scale, bool fullscreen, int refresh) {
    if (!device_) return false;
    FlushDraws();
    ReleaseDeviceObjects();
    scale_ = scale < 0.25f ? 0.25f : scale;
    FillPresentParams(backWidth, backHeight, fullscreen, refresh);
    bool ok = SUCCEEDED(device_->Reset(&pp_));
    if (!ok && fullscreen && refresh) {
        FillPresentParams(backWidth, backHeight, true, 0);
        ok = SUCCEEDED(device_->Reset(&pp_));
    }
    if (!ok && fullscreen) {
        FillPresentParams(backWidth, backHeight, false, 0);
        ok = SUCCEEDED(device_->Reset(&pp_));
    }
    return ok && CreateDeviceObjects();
}

int D3D9Renderer::MaxScale(int width, int height) const {
    int s = 1;
    while (s < 8 && (DWORD)(width * (s + 1)) <= caps_.MaxTextureWidth &&
           (DWORD)(height * (s + 1)) <= caps_.MaxTextureHeight)
        ++s;
    return s;
}

std::vector<D3D9Renderer::Mode> D3D9Renderer::Modes() const {
    std::vector<Mode> out;
    if (!d3d_) return out;
    UINT n = d3d_->GetAdapterModeCount(D3DADAPTER_DEFAULT, D3DFMT_X8R8G8B8);
    for (UINT i = 0; i < n; ++i) {
        D3DDISPLAYMODE m;
        if (FAILED(d3d_->EnumAdapterModes(D3DADAPTER_DEFAULT, D3DFMT_X8R8G8B8, i, &m))) continue;
        if (m.Width < 640 || m.Height < 480) continue;
        bool found = false;
        for (Mode& o : out)
            if (o.width == (int)m.Width && o.height == (int)m.Height) {
                if ((int)m.RefreshRate > o.refresh) o.refresh = (int)m.RefreshRate;
                found = true;
            }
        if (!found) out.push_back({(int)m.Width, (int)m.Height, (int)m.RefreshRate});
    }
    std::sort(out.begin(), out.end(), [](const Mode& a, const Mode& b) {
        return a.width != b.width ? a.width < b.width : a.height < b.height;
    });
    return out;
}

D3D9Renderer::Mode D3D9Renderer::DesktopMode() const {
    D3DDISPLAYMODE m = {};
    if (d3d_) d3d_->GetAdapterDisplayMode(D3DADAPTER_DEFAULT, &m);
    return {(int)m.Width, (int)m.Height, (int)m.RefreshRate};
}

void D3D9Renderer::Shutdown() {
    ReleaseDeviceObjects();
    for (auto& t : textures_) SafeRelease(t);
    textures_.clear();
    targets_.clear();
    for (auto& m : meshes_) SafeRelease(m);
    meshes_.clear();
    SafeRelease(meshDecl_);
    SafeRelease(vsMesh_);
    SafeRelease(psMeshColor_);
    SafeRelease(psMeshTexture_);
    SafeRelease(decl_);
    SafeRelease(vs_);
    SafeRelease(psColor_);
    SafeRelease(psTexture_);
    SafeRelease(psCopy_);
    SafeRelease(device_);
    SafeRelease(d3d_);
}

// Default-pool objects: lost on a device reset and recreated (empty) afterwards.
bool D3D9Renderer::CreateDeviceObjects() {
    if (FAILED(device_->GetRenderTarget(0, &backBuffer_))) return false;
    for (auto& t : targets_)
        if (t.width) CreateTargetTexture(t);
    device_->SetVertexDeclaration(decl_);
    device_->SetVertexShader(vs_);
    meshShader_ = false;
    ForgetState();
    SetRS(D3DRS_CULLMODE, D3DCULL_NONE);
    SetRS(D3DRS_LIGHTING, FALSE);
    SetRS(D3DRS_DITHERENABLE, FALSE);
    bound_ = 0;
    return true;
}

void D3D9Renderer::ReleaseDeviceObjects() {
    pendingVertices_.clear();
    CloseScene();
    if (device_) {
        device_->SetRenderTarget(0, nullptr);
        device_->SetDepthStencilSurface(nullptr);
        SetTex(nullptr);
    }
    for (auto& t : targets_) SafeRelease(t.tex);
    SafeRelease(scratch_.tex);
    scratch_.width = scratch_.height = 0;
    SafeRelease(depth_);
    depthWidth_ = depthHeight_ = 0;
    SafeRelease(backBuffer_);
    bound_ = 0;
}

bool D3D9Renderer::CreateTargetTexture(Target& t) {
    CloseScene();
    if (FAILED(device_->CreateTexture(Px(t.width), Px(t.height), 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8,
                                      D3DPOOL_DEFAULT, &t.tex, nullptr)))
        return false;
    IDirect3DSurface9* s = nullptr;
    if (SUCCEEDED(t.tex->GetSurfaceLevel(0, &s))) {
        device_->ColorFill(s, nullptr, D3DCOLOR_ARGB(0, 0, 0, 0));
        s->Release();
    }
    return true;
}

bool D3D9Renderer::EnsureDepth(int width, int height) {
    width = Px(width);
    height = Px(height);
    if (depth_ && width <= depthWidth_ && height <= depthHeight_) return true;
    if (width < depthWidth_) width = depthWidth_;
    if (height < depthHeight_) height = depthHeight_;
    device_->SetDepthStencilSurface(nullptr);
    SafeRelease(depth_);
    if (FAILED(device_->CreateDepthStencilSurface(width, height, D3DFMT_D24S8, D3DMULTISAMPLE_NONE, 0, FALSE, &depth_,
                                                  nullptr)))
        return false;
    depthWidth_ = width;
    depthHeight_ = height;
    device_->SetDepthStencilSurface(depth_);
    device_->Clear(0, nullptr, D3DCLEAR_ZBUFFER, 0, 0.0f, 0);
    return true;
}

TextureId D3D9Renderer::CreateTexture(int width, int height, const uint32_t* pixels) {
    Ticks ticks(stats.uploadTicks, uploadDepth_);
    ++stats.texturesMade;
    IDirect3DTexture9* tex = nullptr;
    if (FAILED(device_->CreateTexture(width, height, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &tex, nullptr)))
        return 0;
    D3DLOCKED_RECT lr;
    if (SUCCEEDED(tex->LockRect(0, &lr, nullptr, 0))) {
        for (int y = 0; y < height; ++y)
            memcpy((uint8_t*)lr.pBits + y * lr.Pitch, pixels + (size_t)y * width, width * 4);
        tex->UnlockRect(0);
    }
    // Managed resources reach video memory on their first draw unless asked now: a stall mid-race on slow machines.
    tex->PreLoad();
    for (size_t i = 0; i < textures_.size(); ++i)
        if (!textures_[i]) {
            textures_[i] = tex;
            return (TextureId)(i + 1);
        }
    textures_.push_back(tex);
    return (TextureId)textures_.size();
}

void D3D9Renderer::DestroyTexture(TextureId id) {
    FlushDraws();
    if (id && id <= textures_.size()) {
        SetTex(nullptr);
        SafeRelease(textures_[id - 1]);
    }
}

void D3D9Renderer::WriteTexture(TextureId id, const uint32_t* pixels) {
    FlushDraws();
    if (!id || id > textures_.size() || !textures_[id - 1]) return;
    Ticks ticks(stats.uploadTicks, uploadDepth_);
    ++stats.texturesWritten;
    IDirect3DTexture9* tex = textures_[id - 1];
    D3DSURFACE_DESC desc;
    D3DLOCKED_RECT lr;
    if (FAILED(tex->GetLevelDesc(0, &desc)) || FAILED(tex->LockRect(0, &lr, nullptr, 0))) return;
    for (UINT y = 0; y < desc.Height; ++y)
        memcpy((uint8_t*)lr.pBits + y * lr.Pitch, pixels + (size_t)y * desc.Width, desc.Width * 4);
    tex->UnlockRect(0);
}

TargetId D3D9Renderer::CreateTarget(int width, int height) {
    Target t = {nullptr, width, height};
    // While the device objects are gone (a reset that failed: full screen left with Alt+Tab), the texture is made
    // with them when the device is back (CreateDeviceObjects); draws to it until then are skipped.
    if (backBuffer_ && !CreateTargetTexture(t)) return 0;
    for (size_t i = 0; i < targets_.size(); ++i)
        if (!targets_[i].width) {
            targets_[i] = t;
            return (TargetId)(i + 1);
        }
    targets_.push_back(t);
    return (TargetId)targets_.size();
}

void D3D9Renderer::DestroyTarget(TargetId id) {
    FlushDraws();
    if (!id || id > targets_.size()) return;
    if (bound_ == id) {
        device_->SetRenderTarget(0, backBuffer_);
        bound_ = 0;
    }
    SetTex(nullptr);
    SafeRelease(targets_[id - 1].tex);
    targets_[id - 1].width = targets_[id - 1].height = 0;
}

void D3D9Renderer::BindTarget(TargetId id) {
    if (bound_ == id) return;
    Target& t = targets_[id - 1];
    IDirect3DSurface9* s = nullptr;
    if (!t.tex || FAILED(t.tex->GetSurfaceLevel(0, &s))) return;
    EnsureDepth(t.width, t.height);
    device_->SetRenderTarget(0, s);
    device_->SetDepthStencilSurface(depth_);
    s->Release();
    bound_ = id;
    viewOrigin_ = 1e30f;
    SetView(0);
}

void D3D9Renderer::SetView(float origin) {
    if (!bound_ || origin == viewOrigin_) return;
    const Target& t = targets_[bound_ - 1];
    // The vertex shader maps PS2 pixels to clip space, with D3D9's half-pixel offset at the stored resolution.
    float view[4] = {2.0f / t.width, -2.0f / t.height, -1.0f - 1.0f / Px(t.width) + origin * 2.0f / t.width,
                     1.0f + 1.0f / Px(t.height)};
    device_->SetVertexShaderConstantF(0, view, 1);
    viewOrigin_ = origin;
}

void D3D9Renderer::DrawCopy(IDirect3DBaseTexture9* tex, float x0, float y0, float x1, float y1, float u1, float v1,
                            bool linear) {
    Vertex v[4] = {
        {x0, y0, 0, 0xFFFFFFFF, 0, 0, 1, 1},
        {x1, y0, 0, 0xFFFFFFFF, u1, 0, 1, 1},
        {x0, y1, 0, 0xFFFFFFFF, 0, v1, 1, 1},
        {x1, y1, 0, 0xFFFFFFFF, u1, v1, 1, 1},
    };
    UseMeshShader(false);
    SetPS(psCopy_);
    SetTex(tex);
    D3DTEXTUREFILTERTYPE f = linear ? D3DTEXF_LINEAR : D3DTEXF_POINT;
    SetSampler(D3DSAMP_MINFILTER, f);
    SetSampler(D3DSAMP_MAGFILTER, f);
    SetSampler(D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
    SetSampler(D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
    SetRS(D3DRS_ALPHABLENDENABLE, FALSE);
    SetRS(D3DRS_ZENABLE, FALSE);
    SetRS(D3DRS_SCISSORTESTENABLE, FALSE);
    SetRS(D3DRS_COLORWRITEENABLE, 0xF);
    device_->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, v, sizeof(Vertex));
    cache_.stream = nullptr;
}

void D3D9Renderer::WriteTarget(TargetId id, const uint32_t* pixels) {
    FlushDraws();
    if (!device_ || !id || id > targets_.size() || !targets_[id - 1].tex) return;
    Target& t = targets_[id - 1];
    TextureId tmp = CreateTexture(t.width, t.height, pixels);
    if (!tmp) return;
    BindTarget(id);
    SetView(0);
    if (OpenScene()) DrawCopy(textures_[tmp - 1], 0, 0, (float)t.width, (float)t.height, 1, 1, false);
    DestroyTexture(tmp);
}

void D3D9Renderer::CopyTarget(TargetId from, TargetId to) {
    FlushDraws();
    if (!from || !to || from > targets_.size() || to > targets_.size()) return;
    Target& a = targets_[from - 1];
    Target& b = targets_[to - 1];
    CloseScene();
    IDirect3DSurface9 *sa = nullptr, *sb = nullptr;
    if (a.tex && b.tex && SUCCEEDED(a.tex->GetSurfaceLevel(0, &sa)) && SUCCEEDED(b.tex->GetSurfaceLevel(0, &sb))) {
        RECT r = {0, 0, Px(a.width < b.width ? a.width : b.width), Px(a.height < b.height ? a.height : b.height)};
        device_->StretchRect(sa, &r, sb, &r, D3DTEXF_POINT);
    }
    SafeRelease(sa);
    SafeRelease(sb);
}

void D3D9Renderer::ClearDepth() {
    FlushDraws();
    if (!depth_) return;
    device_->SetDepthStencilSurface(depth_);
    device_->Clear(0, nullptr, D3DCLEAR_ZBUFFER, 0, 0.0f, 0);
}

// The texture a draw samples. A draw that reads the target it renders to gets a copy taken first.
IDirect3DBaseTexture9* D3D9Renderer::SourceTexture(const RenderState& s) {
    if (s.texture) return s.texture <= textures_.size() ? textures_[s.texture - 1] : nullptr;
    if (!s.textureTarget || s.textureTarget > targets_.size()) return nullptr;
    Target& t = targets_[s.textureTarget - 1];
    if (s.textureTarget != bound_) return t.tex;
    if (scratch_.width != t.width || scratch_.height != t.height) {
        SafeRelease(scratch_.tex);
        scratch_.width = t.width;
        scratch_.height = t.height;
        if (!CreateTargetTexture(scratch_)) return nullptr;
    }
    CloseScene();
    IDirect3DSurface9 *a = nullptr, *b = nullptr;
    if (SUCCEEDED(t.tex->GetSurfaceLevel(0, &a)) && SUCCEEDED(scratch_.tex->GetSurfaceLevel(0, &b)))
        device_->StretchRect(a, nullptr, b, nullptr, D3DTEXF_POINT);
    ++stats.copies;
    SafeRelease(a);
    SafeRelease(b);
    return scratch_.tex;
}

void D3D9Renderer::SetBlend(const BlendState& b) {
    if (!b.enable) {
        SetRS(D3DRS_ALPHABLENDENABLE, FALSE);
        return;
    }
    // (A - B) * C + D, split into the coefficients of Cs and Cd.
    int s1 = (b.d == BlendInput::Source), sa = (b.a == BlendInput::Source) - (b.b == BlendInput::Source);
    int d1 = (b.d == BlendInput::Dest), da = (b.a == BlendInput::Dest) - (b.b == BlendInput::Dest);
    bool destAlpha = b.c == BlendFactor::DestAlpha;
    BlendTerm src = Term(s1, sa, destAlpha), dst = Term(d1, da, destAlpha);
    D3DBLENDOP op = D3DBLENDOP_ADD;
    if (src.negative && dst.negative) {
        src.factor = dst.factor = D3DBLEND_ZERO;   // result is never positive: clamps to 0
    } else if (dst.negative) {
        op = D3DBLENDOP_SUBTRACT;
    } else if (src.negative) {
        op = D3DBLENDOP_REVSUBTRACT;
    }
    SetRS(D3DRS_ALPHABLENDENABLE, TRUE);
    SetRS(D3DRS_SRCBLEND, src.factor);
    SetRS(D3DRS_DESTBLEND, dst.factor);
    SetRS(D3DRS_BLENDOP, op);
    if (separateAlpha_) {
        // The GS writes the source alpha unblended.
        SetRS(D3DRS_SEPARATEALPHABLENDENABLE, TRUE);
        SetRS(D3DRS_SRCBLENDALPHA, D3DBLEND_ONE);
        SetRS(D3DRS_DESTBLENDALPHA, D3DBLEND_ZERO);
        SetRS(D3DRS_BLENDOPALPHA, D3DBLENDOP_ADD);
    }
}

void D3D9Renderer::DrawPass(const RenderState& s, const DrawBatch* batch, const MeshBatch* mesh, bool inverted,
                            bool writeColor, bool writeAlpha, bool writeDepth) {
    float atest[4] = {0, 0, 1, 0};
    if (s.alphaTest.enable) {
        AlphaWeights(s.alphaTest.func, atest);
        atest[3] = s.alphaTest.ref;
        if (inverted)
            for (int i = 0; i < 3; ++i) atest[i] = -atest[i];
    }
    SetPSConstant(2, atest);
    DWORD mask = writeColor ? (s.colorMask & 7) : 0;
    if (writeAlpha) mask |= s.colorMask & 8;
    SetRS(D3DRS_COLORWRITEENABLE, mask);
    bool z = s.depth.test || writeDepth;
    SetRS(D3DRS_ZENABLE, z ? D3DZB_TRUE : D3DZB_FALSE);
    if (z) {
        SetRS(D3DRS_ZFUNC, s.depth.test ? DepthFunc(s.depth.func) : D3DCMP_ALWAYS);
        SetRS(D3DRS_ZWRITEENABLE, writeDepth);
    }
    if (mesh) {
        device_->DrawPrimitive(D3DPT_TRIANGLELIST, mesh->first, mesh->count / 3);
        CountCall(source_, inverted, mesh->count);
        return;
    }
    D3DPRIMITIVETYPE type = D3DPT_TRIANGLELIST;
    UINT count = batch->count / 3;
    if (batch->prim == PrimKind::Lines) {
        type = D3DPT_LINELIST;
        count = batch->count / 2;
    } else if (batch->prim == PrimKind::Points) {
        type = D3DPT_POINTLIST;
        count = batch->count;
    }
    if (!count) return;
    device_->DrawPrimitiveUP(type, count, batch->vertices, sizeof(Vertex));
    cache_.stream = nullptr;   // DrawPrimitiveUP lets go of stream 0
    CountCall(source_, inverted, batch->count);
}

void D3D9Renderer::CountCall(DrawSource source, bool secondPass, uint32_t vertices) {
    ++stats.calls;
    ++stats.callsBy[(int)source];
    stats.secondPasses += secondPass;
    stats.vertices += vertices;
}

// A failed alpha test can still write some buffers (GS AFAIL): draw the passing pixels with every write, then the
// failing ones with only the writes AFAIL allows.
void D3D9Renderer::DrawPasses(const RenderState& s, const DrawBatch* batch, const MeshBatch* mesh) {
    const AlphaTest& at = s.alphaTest;
    bool canPass = !at.enable || at.func != Compare::Never;
    bool canFail = at.enable && at.func != Compare::Always && at.fail != AlphaFail::Keep;
    if (canPass) DrawPass(s, batch, mesh, false, true, true, s.depth.write);
    if (canFail) {
        switch (at.fail) {
        case AlphaFail::ColorOnly: DrawPass(s, batch, mesh, true, true, true, false); break;
        case AlphaFail::DepthOnly: DrawPass(s, batch, mesh, true, false, false, s.depth.write); break;
        case AlphaFail::RgbOnly: DrawPass(s, batch, mesh, true, true, false, false); break;
        default: break;
        }
    }
}

void D3D9Renderer::ForgetState() { memset(&cache_, 0, sizeof(cache_)); }

void D3D9Renderer::SetRS(D3DRENDERSTATETYPE state, DWORD value) {
    if ((unsigned)state < 256) {
        if (cache_.rsSet[state] && cache_.rs[state] == value) return;
        cache_.rsSet[state] = true;
        cache_.rs[state] = value;
    }
    device_->SetRenderState(state, value);
}

void D3D9Renderer::SetSampler(D3DSAMPLERSTATETYPE state, DWORD value) {
    if ((unsigned)state < 16) {
        if (cache_.samplerSet[state] && cache_.sampler[state] == value) return;
        cache_.samplerSet[state] = true;
        cache_.sampler[state] = value;
    }
    device_->SetSamplerState(0, state, value);
}

void D3D9Renderer::SetTex(IDirect3DBaseTexture9* tex) {
    if (cache_.texSet && cache_.tex == tex) return;
    cache_.texSet = true;
    cache_.tex = tex;
    device_->SetTexture(0, tex);
}

void D3D9Renderer::SetPS(IDirect3DPixelShader9* ps) {
    if (cache_.psSet && cache_.ps == ps) return;
    cache_.psSet = true;
    cache_.ps = ps;
    device_->SetPixelShader(ps);
}

void D3D9Renderer::SetPSConstant(UINT reg, const float* v) {
    if (reg < 6) {
        if (cache_.pscSet[reg] && memcmp(cache_.psc[reg], v, sizeof(cache_.psc[reg])) == 0) return;
        cache_.pscSet[reg] = true;
        memcpy(cache_.psc[reg], v, sizeof(cache_.psc[reg]));
    }
    device_->SetPixelShaderConstantF(reg, v, 1);
}

void D3D9Renderer::SetScissor(const RECT& r) {
    if (cache_.scissorSet && memcmp(&cache_.scissor, &r, sizeof(r)) == 0) return;
    cache_.scissorSet = true;
    cache_.scissor = r;
    device_->SetScissorRect(&r);
}

bool D3D9Renderer::OpenScene() {
    if (!inScene_) inScene_ = SUCCEEDED(device_->BeginScene());
    return inScene_;
}

void D3D9Renderer::CloseScene() {
    if (!inScene_) return;
    device_->EndScene();
    inScene_ = false;
}

void D3D9Renderer::UseMeshShader(bool mesh) {
    if (meshShader_ == mesh) return;
    device_->SetVertexDeclaration(mesh ? meshDecl_ : decl_);
    device_->SetVertexShader(mesh ? vsMesh_ : vs_);
    meshShader_ = mesh;
}

// Binds the target and sets everything a draw's state asks for but the alpha test and the writes (DrawPass).
bool D3D9Renderer::SetState(TargetId target, const RenderState& s, bool mesh) {
    BindTarget(target);
    SetView(s.originX);
    IDirect3DBaseTexture9* tex = s.Textured() ? SourceTexture(s) : nullptr;
    if (!OpenScene()) return false;
    if (tex) {
        SetPS(mesh ? psMeshTexture_ : psTexture_);
        SetTex(tex);
        D3DTEXTUREFILTERTYPE f = s.sampler.linear ? D3DTEXF_LINEAR : D3DTEXF_POINT;
        SetSampler(D3DSAMP_MINFILTER, f);
        SetSampler(D3DSAMP_MAGFILTER, f);
        SetSampler(D3DSAMP_MIPFILTER, D3DTEXF_NONE);
        bool target = s.texture == 0;   // targets may be non-power-of-two: clamp only
        SetSampler(D3DSAMP_ADDRESSU,
                                 s.sampler.wrapU == Wrap::Repeat && !target ? D3DTADDRESS_WRAP : D3DTADDRESS_CLAMP);
        SetSampler(D3DSAMP_ADDRESSV,
                                 s.sampler.wrapV == Wrap::Repeat && !target ? D3DTADDRESS_WRAP : D3DTADDRESS_CLAMP);
        static const float kTfx[4][4] = {{1, 0, 1, 0}, {0, 0, 0, 0}, {1, 1, 0, 1}, {1, 1, 0, 0}};
        float texc[4] = {s.texAlpha ? 1.0f : 0.0f, 0, 0, 0};
        float region[4] = {-65536.0f, -65536.0f, 65536.0f, 65536.0f};
        if (s.sampler.wrapU == Wrap::Region) {
            region[0] = s.sampler.region[0];
            region[2] = s.sampler.region[2];
        }
        if (s.sampler.wrapV == Wrap::Region) {
            region[1] = s.sampler.region[1];
            region[3] = s.sampler.region[3];
        }
        SetPSConstant(0, kTfx[(int)s.texFunc]);
        SetPSConstant(1, texc);
        SetPSConstant(4, region);
    } else {
        SetPS(mesh ? psMeshColor_ : psColor_);
        SetTex(nullptr);
    }
    float fog[4] = {(s.fogColor & 0xFF) / 255.0f, ((s.fogColor >> 8) & 0xFF) / 255.0f,
                    ((s.fogColor >> 16) & 0xFF) / 255.0f, 0};
    float alpha[4] = {0, 0, 0, 0};
    if (s.blend.enable && s.blend.c == BlendFactor::Fixed) {
        alpha[0] = 1;
        alpha[1] = s.blend.fix / 128.0f;
    }
    SetPSConstant(3, fog);
    SetPSConstant(5, alpha);
    SetBlend(s.blend);
    SetRS(D3DRS_SCISSORTESTENABLE, TRUE);
    RECT sc = {Px(s.scissor.x0), Px(s.scissor.y0), Px(s.scissor.x1), Px(s.scissor.y1)};
    SetScissor(sc);
    return true;
}

void D3D9Renderer::Draw(const DrawBatch& batch) {
    if (!device_ || !batch.count || !batch.target || batch.target > targets_.size()) return;
    if (!targets_[batch.target - 1].tex) return;
    Ticks ticks(stats.drawTicks, drawDepth_);
    // A draw that samples its own target needs a copy of it as it is, so it goes alone.
    bool joins = batch.prim == PrimKind::Triangles && batch.state.textureTarget != batch.target;
    if (joins && !pendingVertices_.empty() && batch.target == pending_.target &&
        memcmp(&batch.state, &pending_.state, sizeof(RenderState)) == 0) {
        pendingVertices_.insert(pendingVertices_.end(), batch.vertices, batch.vertices + batch.count);
        return;
    }
    FlushDraws();
    if (!joins) {
        DrawNow(batch);
        return;
    }
    pending_ = batch;
    pendingSource_ = source_;
    pendingVertices_.assign(batch.vertices, batch.vertices + batch.count);
}

void D3D9Renderer::FlushDraws() {
    if (pendingVertices_.empty()) return;
    Ticks ticks(stats.drawTicks, drawDepth_);
    DrawBatch b = pending_;
    b.vertices = pendingVertices_.data();
    b.count = (uint32_t)pendingVertices_.size();
    DrawSource now = source_;
    source_ = pendingSource_;
    DrawNow(b);
    source_ = now;
    pendingVertices_.clear();
}

void D3D9Renderer::DrawNow(const DrawBatch& batch) {
    if (!SetState(batch.target, batch.state, false)) return;
    UseMeshShader(false);
    DrawPasses(batch.state, &batch, nullptr);
}

MeshId D3D9Renderer::CreateMesh(const MeshVertex3D* vertices, uint32_t count) {
    if (!device_ || !count) return 0;
    Ticks ticks(stats.uploadTicks, uploadDepth_);
    ++stats.meshesMade;
    IDirect3DVertexBuffer9* vb = nullptr;
    UINT bytes = count * sizeof(MeshVertex3D);
    DWORD usage = D3DUSAGE_WRITEONLY | (softwareVertices_ ? D3DUSAGE_SOFTWAREPROCESSING : 0);
    if (FAILED(device_->CreateVertexBuffer(bytes, usage, 0, D3DPOOL_MANAGED, &vb, nullptr))) return 0;
    void* p = nullptr;
    if (FAILED(vb->Lock(0, bytes, &p, 0))) {
        vb->Release();
        return 0;
    }
    memcpy(p, vertices, bytes);
    vb->Unlock();
    vb->PreLoad();
    for (size_t i = 0; i < meshes_.size(); ++i)
        if (!meshes_[i]) {
            meshes_[i] = vb;
            return (MeshId)(i + 1);
        }
    meshes_.push_back(vb);
    return (MeshId)meshes_.size();
}

void D3D9Renderer::DestroyMesh(MeshId id) {
    if (!id || id > meshes_.size() || !meshes_[id - 1]) return;
    device_->SetStreamSource(0, nullptr, 0, 0);
    cache_.stream = nullptr;
    SafeRelease(meshes_[id - 1]);
}

void D3D9Renderer::DrawMesh(const MeshBatch& batch) {
    Ticks ticks(stats.drawTicks, drawDepth_);
    FlushDraws();
    if (!device_ || batch.count < 3 || !batch.target || batch.target > targets_.size()) return;
    if (!targets_[batch.target - 1].tex || !batch.mesh || batch.mesh > meshes_.size() || !meshes_[batch.mesh - 1])
        return;
    if (!SetState(batch.target, batch.state, true)) return;
    UseMeshShader(true);
    // Sections of one model and the models drawn alike share these (the field's blocks): only changes go.
    IDirect3DVertexBuffer9* vb = meshes_[batch.mesh - 1];
    if (cache_.stream != vb) {
        device_->SetStreamSource(0, vb, 0, sizeof(MeshVertex3D));
        cache_.stream = vb;
    }
    uint32_t n = batch.constantCount;
    if (n > kMeshConstants || cache_.vscCount != n || memcmp(cache_.vsc, batch.constants[0], n * sizeof(cache_.vsc[0]))) {
        device_->SetVertexShaderConstantF(1, batch.constants[0], n);
        cache_.vscCount = n > kMeshConstants ? 0 : n;
        if (cache_.vscCount) memcpy(cache_.vsc, batch.constants[0], n * sizeof(cache_.vsc[0]));
    }
    DrawPasses(batch.state, nullptr, &batch);
}

void D3D9Renderer::Present(TargetId id, const Rect& source, float aspect) {
    if (!device_) return;
    FlushDraws();
    CloseScene();
    if (HandleLostDevice()) return;
    device_->SetRenderTarget(0, backBuffer_);
    device_->SetDepthStencilSurface(nullptr);   // may be smaller than the back buffer
    bound_ = 0;
    device_->Clear(0, nullptr, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0, 0, 0), 0.0f, 0);
    UseMeshShader(false);
    if (id && id <= targets_.size() && targets_[id - 1].tex && SUCCEEDED(device_->BeginScene())) {
        const Target& t = targets_[id - 1];
        // Fit the picture into the back buffer at its aspect. Within a pixel or two of filling it, it fills it.
        float bw = (float)pp_.BackBufferWidth, bh = (float)pp_.BackBufferHeight;
        float w = bw, h = bw / aspect;
        if (h > bh) {
            h = bh;
            w = bh * aspect;
        }
        if (bw - w < 2) w = bw;
        if (bh - h < 2) h = bh;
        float x0 = (bw - w) * 0.5f, y0 = (bh - h) * 0.5f;
        float view[4] = {2.0f / bw, -2.0f / bh, -1.0f - 1.0f / bw, 1.0f + 1.0f / bh};
        device_->SetVertexShaderConstantF(0, view, 1);
        Vertex v[4] = {
            {x0, y0, 0, 0xFFFFFFFF, (float)source.x0 / t.width, (float)source.y0 / t.height, 1, 1},
            {x0 + w, y0, 0, 0xFFFFFFFF, (float)source.x1 / t.width, (float)source.y0 / t.height, 1, 1},
            {x0, y0 + h, 0, 0xFFFFFFFF, (float)source.x0 / t.width, (float)source.y1 / t.height, 1, 1},
            {x0 + w, y0 + h, 0, 0xFFFFFFFF, (float)source.x1 / t.width, (float)source.y1 / t.height, 1, 1},
        };
        SetPS(psCopy_);
        SetTex(t.tex);
        SetSampler(D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
        SetSampler(D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
        SetSampler(D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
        SetSampler(D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
        SetRS(D3DRS_ALPHABLENDENABLE, FALSE);
        SetRS(D3DRS_ZENABLE, FALSE);
        SetRS(D3DRS_SCISSORTESTENABLE, FALSE);
        SetRS(D3DRS_COLORWRITEENABLE, 0xF);
        device_->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, v, sizeof(Vertex));
        cache_.stream = nullptr;
        device_->EndScene();
    }
    LARGE_INTEGER t0, t1, freq;
    QueryPerformanceCounter(&t0);
    if (device_->Present(nullptr, nullptr, nullptr, nullptr) == D3DERR_DEVICELOST) HandleLostDevice();
    QueryPerformanceCounter(&t1);
    QueryPerformanceFrequency(&freq);
    stats.presentMs += (t1.QuadPart - t0.QuadPart) * 1000.0 / freq.QuadPart;
}

// Returns true while the device can't be used (alt-tab, lock screen, mode change).
bool D3D9Renderer::HandleLostDevice() {
    HRESULT hr = device_->TestCooperativeLevel();
    if (hr == D3DERR_DEVICELOST) {
        Sleep(50);
        return true;
    }
    if (hr == D3DERR_DEVICENOTRESET) {
        ReleaseDeviceObjects();
        if (FAILED(device_->Reset(&pp_))) return true;
        CreateDeviceObjects();
    }
    return false;
}

bool D3D9Renderer::ReadTarget(TargetId id, std::vector<uint32_t>& pixels, int& width, int& height) {
    if (!id || id > targets_.size() || !targets_[id - 1].tex) return false;
    FlushDraws();
    CloseScene();
    const Target& t = targets_[id - 1];
    width = Px(t.width);
    height = Px(t.height);
    IDirect3DSurface9 *rt = nullptr, *mem = nullptr;
    bool ok = false;
    if (SUCCEEDED(t.tex->GetSurfaceLevel(0, &rt)) &&
        SUCCEEDED(device_->CreateOffscreenPlainSurface(width, height, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM, &mem,
                                                       nullptr)) &&
        SUCCEEDED(device_->GetRenderTargetData(rt, mem))) {
        D3DLOCKED_RECT lr;
        if (SUCCEEDED(mem->LockRect(&lr, nullptr, D3DLOCK_READONLY))) {
            pixels.resize((size_t)width * height);
            for (int y = 0; y < height; ++y)
                memcpy(&pixels[(size_t)y * width], (uint8_t*)lr.pBits + y * lr.Pitch, width * 4);
            mem->UnlockRect();
            ok = true;
        }
    }
    SafeRelease(rt);
    SafeRelease(mem);
    return ok;
}

}  // namespace gfx
