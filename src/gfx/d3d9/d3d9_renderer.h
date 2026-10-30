// Direct3D 9 backend for gfx::Renderer (Shader Model 2.0; see shaders/render.hlsl).
//
// Render targets are textures (so they can be sampled, which the game does every frame for its motion-blur
// feedback) stored at a multiple of their PS2 size (whole, or under 1 on slow machines: the Display screen's Render
// Scale). One depth buffer is shared by all targets.
//
// Meshes are managed vertex buffers drawn with render.hlsl's VSMesh; other draws send their vertices each time
// (DrawPrimitiveUP) through VSMain. A scene stays open across draws and closes before copies, reads and Present.
// Consecutive triangle draws to the same target with the same state go as one draw call (a screen's sprites, the
// HUD): the order is kept, and anything else that draws, reads or changes a texture or target sends them first.
#pragma once

#include <windows.h>
#include <d3d9.h>

#include <string>
#include <vector>

#include "../render/renderer.h"

namespace gfx {

class D3D9Renderer : public Renderer {
public:
    D3D9Renderer();
    ~D3D9Renderer();

    // Creates the device for a window whose client area is backWidth x backHeight (or, `fullscreen`, the screen
    // in that mode at `refresh` Hz, 0 = the adapter's choice). Targets are stored at `scale` times their PS2 size.
    // Without vsync, Present doesn't wait (for tests).
    bool Init(HWND hwnd, int backWidth, int backHeight, float scale, bool vsync = true, bool fullscreen = false,
              int refresh = 0);
    void Shutdown();
    // The same for a device already made (the Display screen's changes): the targets are made again at the new
    // scale, empty. A full screen mode the screen refuses falls back to the adapter's refresh, then to a window.
    // False if the device couldn't take it.
    bool Reset(int backWidth, int backHeight, float scale, bool fullscreen, int refresh);
    bool Fullscreen() const { return !pp_.Windowed; }
    // Whether Present waits for the vertical blank, from the next Reset on.
    void SetVsync(bool on) { vsync_ = on; }
    // The largest scale a target `width` x `height` can be stored at on this card (its largest texture), up to 8.
    int MaxScale(int width, int height) const;
    float Scale() const { return scale_; }
    // `v` PS2 pixels in the targets' stored pixels.
    int Px(int v) const { return (int)(v * scale_ + 0.5f); }

    // The adapter's display modes in the desktop's format, each size once at its highest refresh rate, smallest
    // first; and the desktop's own.
    struct Mode {
        int width, height, refresh;
    };
    std::vector<Mode> Modes() const;
    Mode DesktopMode() const;

    TextureId CreateTexture(int width, int height, const uint32_t* pixels) override;
    void DestroyTexture(TextureId id) override;
    void WriteTexture(TextureId id, const uint32_t* pixels) override;
    TargetId CreateTarget(int width, int height) override;
    void DestroyTarget(TargetId id) override;
    void WriteTarget(TargetId id, const uint32_t* pixels) override;
    void CopyTarget(TargetId from, TargetId to) override;
    void ClearDepth() override;
    void Draw(const DrawBatch& batch) override;
    MeshId CreateMesh(const MeshVertex3D* vertices, uint32_t count) override;
    void DestroyMesh(MeshId id) override;
    void DrawMesh(const MeshBatch& batch) override;
    void Present(TargetId id, const Rect& source, float aspect = 4.0f / 3.0f) override;

    // Copies a target to memory at its stored resolution (B,G,R,A). For screenshots and tests.
    bool ReadTarget(TargetId id, std::vector<uint32_t>& pixels, int& width, int& height);

    // What the frames so far cost (--perf). The caller zeroes it.
    struct Stats {
        int calls;                                // D3D draw calls
        int callsBy[(int)DrawSource::Count];      // the same by what was drawn
        int secondPasses;                         // of them, alpha tests' second passes (the failing pixels)
        int vertices;                             // drawn
        int copies;                               // of the bound target, for draws that sample it
        int meshesMade, texturesMade, texturesWritten;
        unsigned long long drawTicks;             // time stamp counter ticks in Draw, DrawMesh and their flush
        unsigned long long uploadTicks;           // the same in CreateMesh, CreateTexture and WriteTexture
        double presentMs;                         // in Present: waiting for the card, or the vertical blank
    };
    Stats stats;

    // The card, its driver and how the device was made, for --perf's log.
    std::string Describe() const;

private:
    struct Target {
        IDirect3DTexture9* tex;
        int width, height;   // PS2 pixels
    };

    bool CreateDeviceObjects();
    void ReleaseDeviceObjects();
    bool CreateTargetTexture(Target& t);
    bool EnsureDepth(int width, int height);
    bool HandleLostDevice();
    void BindTarget(TargetId id);
    // Sets c0 for the bound target with the vertices' x moved by `origin` (RenderState::originX).
    void SetView(float origin);
    void FillPresentParams(int backWidth, int backHeight, bool fullscreen, int refresh);
    void DrawCopy(IDirect3DBaseTexture9* tex, float x0, float y0, float x1, float y1, float u1, float v1,
                  bool linear);
    void SetBlend(const BlendState& b);
    bool SetState(TargetId target, const RenderState& s, bool mesh);
    void DrawNow(const DrawBatch& batch);
    void FlushDraws();
    void DrawPasses(const RenderState& s, const DrawBatch* batch, const MeshBatch* mesh);
    void CountCall(DrawSource source, bool secondPass, uint32_t vertices);
    void DrawPass(const RenderState& s, const DrawBatch* batch, const MeshBatch* mesh, bool inverted,
                  bool writeColor, bool writeAlpha, bool writeDepth);
    IDirect3DBaseTexture9* SourceTexture(const RenderState& s);
    bool OpenScene();
    void CloseScene();
    void UseMeshShader(bool mesh);
    // The device's state as last set, so only changes reach it (the runtime's cost per call adds up over a frame's
    // hundreds of draws on older CPUs). Forgotten when the device is reset.
    void ForgetState();
    void SetRS(D3DRENDERSTATETYPE state, DWORD value);
    void SetSampler(D3DSAMPLERSTATETYPE state, DWORD value);
    void SetTex(IDirect3DBaseTexture9* tex);
    void SetPS(IDirect3DPixelShader9* ps);
    void SetPSConstant(UINT reg, const float* v);
    void SetScissor(const RECT& r);

    IDirect3D9* d3d_;
    IDirect3DDevice9* device_;
    D3DPRESENT_PARAMETERS pp_;
    static const uint32_t kMeshConstants = 19;   // VSMesh's, c1 on (render.hlsl)

    D3DCAPS9 caps_;
    bool separateAlpha_;
    bool softwareVertices_;   // the device runs vertex shaders on the CPU
    bool vsync_;
    float scale_;             // 0.25 up
    float viewOrigin_;        // the origin the bound target's c0 has
    bool inScene_;
    bool meshShader_;         // VSMesh and its declaration are set (else VSMain's)

    IDirect3DVertexDeclaration9* decl_;
    IDirect3DVertexShader9* vs_;
    IDirect3DPixelShader9* psColor_;
    IDirect3DPixelShader9* psTexture_;
    IDirect3DPixelShader9* psCopy_;
    IDirect3DVertexDeclaration9* meshDecl_;
    IDirect3DVertexShader9* vsMesh_;
    IDirect3DPixelShader9* psMeshColor_;
    IDirect3DPixelShader9* psMeshTexture_;

    IDirect3DSurface9* backBuffer_;
    IDirect3DSurface9* depth_;
    int depthWidth_, depthHeight_;
    Target scratch_;          // copy of the bound target when a draw samples it

    std::vector<IDirect3DTexture9*> textures_;   // index = id - 1
    std::vector<Target> targets_;               // index = id - 1; tex == null and width == 0 when free
    std::vector<IDirect3DVertexBuffer9*> meshes_;   // index = id - 1; null when free
    DrawBatch pending_ = {};            // the draws waiting to go as one: target, primitive and state
    std::vector<Vertex> pendingVertices_;
    DrawSource pendingSource_ = DrawSource::Other;   // what they are of (--perf)
    int drawDepth_ = 0, uploadDepth_ = 0;              // Ticks' nesting

    struct StateCache {
        DWORD rs[256];
        bool rsSet[256];
        DWORD sampler[16];
        bool samplerSet[16];
        IDirect3DBaseTexture9* tex;
        IDirect3DPixelShader9* ps;
        bool texSet, psSet;
        float psc[6][4];
        bool pscSet[6];
        RECT scissor;
        bool scissorSet;
        IDirect3DVertexBuffer9* stream;   // stream 0's mesh
        float vsc[kMeshConstants][4];     // the mesh shader's constants from c1
        uint32_t vscCount;
    } cache_;
    TargetId bound_;
};

}  // namespace gfx
