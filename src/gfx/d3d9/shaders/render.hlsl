// Shaders for the D3D9 backend (Shader Model 2.0). Compiled at build time by the DXSDK's fxc into
// $(IntDir)shaders\*.h (see RTAGlass.vcxproj), so the game needs no D3DX at run time.
//
// Colour scale: all alpha, and vertex colour where it modulates a texture, use the PS2 scale where 0x80 means
// 1.0. Vertex colours arrive as D3DCOLOR (0..1 for 0..255), so those are multiplied by 255/128. Untextured RGB
// is used as is. Texture alpha is already stored with 255 = 1.0.

// ---- vertex shader ----

// x, y: 2/W and -2/H for the target's size in PS2 pixels; z, w: the matching offsets, including D3D9's half
// pixel at the target's real (possibly upscaled) resolution.
float4 c_view : register(c0);

struct VSIn {
    float3 pos : POSITION;
    float4 color : COLOR0;
    float3 stq : TEXCOORD0;
    float fog : TEXCOORD1;
    float4 region : TEXCOORD2;   // the primitive's clamp on the texture coordinates (Vertex::region)
};

struct VSOut {
    float4 pos : POSITION;
    float4 color : COLOR0;
    float3 stq : TEXCOORD0;
    float fog : TEXCOORD1;
    float4 region : TEXCOORD2;
};

// w = 1 keeps every attribute linear in screen space, as the GS interpolates them; perspective comes from
// dividing s and t by q per pixel.
VSOut VSMain(VSIn i) {
    VSOut o;
    o.pos = float4(i.pos.x * c_view.x + c_view.z, i.pos.y * c_view.y + c_view.w, i.pos.z, 1);
    o.color = i.color;
    o.stq = i.stq;
    o.fog = i.fog;
    o.region = i.region;
    return o;
}

// ---- the models' vertex shader ----
// gfx::Model's meshes, kept on the card (Renderer::DrawMesh): what Model::Draw used to work out per vertex on the CPU
// (model.h has the meaning of each setting). Positions go to the view by c_modelView, then to the frame's pixels
// with w = the view depth, so the card cuts the triangles at depth 1 as the CPU did and the depth test sees 1/depth
// (larger = nearer). Colours come out as the CPU's vertices had them (alpha on the 0x80 = 1.0 scale).
//
// The GS interpolates colour and fog straight across the screen (only the texture coordinates are perspective
// correct), but the card interpolates everything along with w. So the colour and fog go out times w, with w beside
// them, and PSMeshColor/PSMeshTexture divide: the perspective cancels and leaves the straight interpolation.
//
// Except across the guard band. Programs 0x30/0x40 clip a triangle with a corner outside |x| <= w, |y| <= w of
// their clip space (the frame's centre +-512 pixels across, +-481 down, at a focal of 512) and draw what's left as a
// fan whose new corners take the colour and fog of where they are (the clipper at 0x0720-0x0EE0 interpolates in 3D,
// then fogs each corner by its own w). Interpolated straight across the screen from corners far outside, a big
// triangle's fog and colour come out far from that (Island Bridge's other carriageway black by night). So a triangle
// that crosses the band (each vertex has the other two corners) sends them as they are with 1 beside them:
// interpolated along with w, the nearest the card comes to the clipped fan (docs/renderer.md, "Fog and the guard
// band").

float4 c_modelView[3] : register(c1);   // rows x, y, z of the model-to-view transform (right, up, forward)
float4 c_billboard : register(c4);      // x, y: the camera's forward on the ground (fx, fz), normalised; z: the
                                        // guard band's reach across (512, more in a wide frame: gfx::GuardBandX)
float4 c_project : register(c5);        // focal x, focal y, centre x, centre y (pixels)
float4 c_colorScale : register(c6);     // x: colour scale, y: alpha scale, z: 1 = lit (program 0x20)
float4 c_lightDir[3] : register(c7);    // light 1, light 2, highlight, in the model's space
float4 c_lightColor[4] : register(c10); // light 1, light 2, highlight, ambient
float4 c_paint[2] : register(c14);      // paint colours 1 and 2
float4 c_weights : register(c16);       // the day, dusk and night sets' weights (1, 0, 0 for the plain colour)
float4 c_tint : register(c17);
float4 c_cue : register(c18);           // alpha max, scale, end, 1 = on
float4 c_cueFog : register(c19);        // fog max, scale, end, 1 = on

struct VSMeshIn {
    float3 pos : POSITION;
    float3 normal : NORMAL;
    float4 color : COLOR0;
    float4 dusk : COLOR1;
    float4 night : COLOR2;
    float2 uv : TEXCOORD0;
    float4 anchor : TEXCOORD1;
    float3 extra : TEXCOORD2;
    float3 corner1 : TEXCOORD3;   // the triangle's other two corners
    float3 corner2 : TEXCOORD4;
};

struct VSMeshOut {
    float4 pos : POSITION;
    float3 stq : TEXCOORD0;
    float2 fogW : TEXCOORD1;    // fog x w, w
    float4 colorW : TEXCOORD2;  // colour x w
};

// A model-space position in the view. A billboard turns about its anchor's vertical: local x along the camera's
// right, z along its forward.
float3 ToView(float3 pos, float4 anchor) {
    float lx = pos.x - anchor.x, lz = pos.z - anchor.z;
    float3 turned = float3(anchor.x + lx * c_billboard.y + lz * c_billboard.x, pos.y,
                           anchor.z - lx * c_billboard.x + lz * c_billboard.y);
    float4 p = float4(lerp(pos, turned, anchor.w), 1);
    return float3(dot(c_modelView[0], p), dot(c_modelView[1], p), dot(c_modelView[2], p));
}

// Greater than 0 if the view position is outside the guard band (or nearer than depth 1).
float OutsideBand(float3 v) {
    return max(max(abs(v.x) * c_project.x - c_billboard.z * v.z, abs(v.y) * c_project.y - 481.28 * v.z), 1 - v.z);
}

VSMeshOut VSMesh(VSMeshIn i) {
    float3 v = ToView(i.pos, i.anchor);
    float crosses = max(OutsideBand(v), max(OutsideBand(ToView(i.corner1, i.anchor)),
                                            OutsideBand(ToView(i.corner2, i.anchor)))) > 0;
    float wOut = lerp(v.z, 1, crosses);   // what the colour and fog go out times (above)

    // Program 0x20's lighting.
    float3 d = max(0, float3(dot(c_lightDir[0].xyz, i.normal), dot(c_lightDir[1].xyz, i.normal),
                             dot(c_lightDir[2].xyz, i.normal)));
    float h = d.z * d.z;
    h *= h;
    h *= h * i.extra.x;
    float3 paint = 1 + i.extra.y * (c_paint[0].rgb - 1) + i.extra.z * (c_paint[1].rgb - 1);
    float3 diffuse = c_lightColor[0].rgb * d.x + c_lightColor[1].rgb * d.y + c_lightColor[3].rgb;
    float4 lit = float4(min(1, i.color.rgb * paint * diffuse + c_lightColor[2].rgb * h), i.color.a);
    // Programs 0x30/0x40's colour sets.
    float4 blend = min(1, c_weights.x * i.color + c_weights.y * i.dusk + c_weights.z * i.night);
    float4 c = lerp(blend, lit, c_colorScale.z);
    c.rgb *= c_tint.rgb;

    float alpha = min(1, c.a * c_colorScale.x * c_colorScale.y) * (128.0 / 255.0);
    float cue = clamp((c_cue.z - v.z) * c_cue.y, 0, c_cue.x) / 128;
    // Programs 0x30/0x40 fog a strip flagged in its header (anchor.y) by vf11, the cue's end and scale, else by vf10.
    float2 fogSet = lerp(c_cueFog.zy, c_cue.zy, i.anchor.y);
    float fog = clamp((fogSet.x - v.z) * fogSet.y, 0, c_cueFog.x) / 255;

    VSMeshOut o;
    float2 pix = c_project.zw * v.z + float2(c_project.x * v.x, -c_project.y * v.y);
    o.pos = float4(pix.x * c_view.x + c_view.z * v.z, pix.y * c_view.y + c_view.w * v.z, 1, v.z);
    o.colorW = float4(min(1, c.rgb * c_colorScale.x), alpha * lerp(1, cue, c_cue.w)) * wOut;
    o.stq = float3(i.uv, 1);
    o.fogW = float2(lerp(1, fog, c_cueFog.w) * wOut, wOut);
    return o;
}

// ---- pixel shaders ----

sampler2D s_tex : register(s0);

// Texture function weights: rgb = tex * lerp(1, vc, x) + va * y; texture alpha = tex.a * lerp(1, va, z) + va * w.
//   MODULATE (1, 0, 1, 0)  DECAL (0, 0, 0, 0)  HIGHLIGHT (1, 1, 0, 1)  HIGHLIGHT2 (1, 1, 0, 0)
float4 c_tfx : register(c0);
// x: 1 = use the texture's alpha (TCC), 0 = the vertex alpha.
float4 c_tex : register(c1);
// Alpha test on the integer alpha a: with d = a - w, keep the pixel if x*d + y*|d| + z >= 0.
// The translator picks the weights per comparison, negated for the pass that draws the failing pixels.
float4 c_atest : register(c2);
// Fog colour (rgb).
float4 c_fog : register(c3);
// Region clamp: min s, min t, max s, max t.
float4 c_region : register(c4);
// x: 1 = output the fixed blend factor y as alpha instead of the pixel's alpha (blends with C = FIX).
float4 c_alpha : register(c5);

struct PSIn {
    float4 color : COLOR0;
    float3 stq : TEXCOORD0;
    float fog : TEXCOORD1;
    float4 region : TEXCOORD2;
};

static const float4 kOpenRegion = float4(-65536, -65536, 65536, 65536);

float4 Finish(float4 c, float fog) {
    float d = floor(c.a * 128 + 0.5) - c_atest.w;
    clip(c_atest.x * d + c_atest.y * abs(d) + c_atest.z);
    c.rgb = lerp(c_fog.rgb, c.rgb, fog);
    c.a = lerp(c.a, c_alpha.y, c_alpha.x);
    return c;
}

// Untextured: the GS uses vertex RGB as is (0xFF = full); only alpha is on the 0x80 = 1.0 scale.
float4 Untextured(float4 color, float fog) {
    return Finish(float4(color.rgb, color.a * (255.0 / 128.0)), fog);
}

// `region` is the primitive's own clamp (a sprite's texels), applied with the state's.
float4 Textured(float4 color, float3 stq, float fog, float4 region) {
    float4 vc = color * (255.0 / 128.0);
    float2 uv = clamp(stq.xy / stq.z, max(c_region.xy, region.xy), min(c_region.zw, region.zw));
    float4 t = tex2D(s_tex, uv);
    float4 c;
    c.rgb = t.rgb * lerp(1, vc.rgb, c_tfx.x) + vc.a * c_tfx.y;
    float ta = t.a * lerp(1, vc.a, c_tfx.z) + vc.a * c_tfx.w;
    c.a = lerp(vc.a, ta, c_tex.x);
    return Finish(c, fog);
}

float4 PSColor(PSIn i) : COLOR {
    return Untextured(i.color, i.fog);
}

float4 PSTexture(PSIn i) : COLOR {
    return Textured(i.color, i.stq, i.fog, i.region);
}

// The same for VSMesh's output.
struct PSMeshIn {
    float3 stq : TEXCOORD0;
    float2 fogW : TEXCOORD1;
    float4 colorW : TEXCOORD2;
};

float4 PSMeshColor(PSMeshIn i) : COLOR {
    float w = 1 / i.fogW.y;
    return Untextured(i.colorW * w, i.fogW.x * w);
}

float4 PSMeshTexture(PSMeshIn i) : COLOR {
    float w = 1 / i.fogW.y;
    return Textured(i.colorW * w, i.stq, i.fogW.x * w, kOpenRegion);
}

// Plain copy, for presenting a target and for writing pixels into one.
float4 PSCopy(PSIn i) : COLOR {
    return tex2D(s_tex, i.stq.xy);
}
