# Renderer: the D3D9 backend

The game draws through a renderer interface of draw batches with its own render-state types, modelled on the GS's,
and a D3D9 backend. Nothing is emulated at run time: the game builds its batches directly.

## Code

The renderer is game code in `src/gfx`. GS memory and texture decoding, used by the converters, is in
`tools/common/gs`.

| File | What it does |
|---|---|
| `gfx/render/renderer.h` | The renderer interface and its render-state types: blend `(A-B)*C+D`, alpha test with fail modes, depth (larger = nearer), texture function, wrap/region clamp, fog, colour mask, scissor. Modelled on GS semantics, but nothing in it is a GS register, so later stages can call it directly. Each vertex also carries a region clamp of its own (`Vertex::region`, open by default): `Draw2D`'s sprites set it to their source rectangle's texel centres, as the game's sprite packets set REGION_CLAMP, so atlas neighbours never bleed in when filtering above 1x. Per vertex, it doesn't split batches the way a state change would. |
| `gfx/d3d9/d3d9_renderer.*` | The D3D9 backend: render targets as textures at an integer scale, one shared D24S8 depth buffer, a blend-equation mapper, two-pass alpha-fail, a scratch copy when a draw samples its own target, and present at the picture's aspect (4:3, or a wide frame's). Device-lost handling is included, and a reset to a new back buffer, scale or full screen mode (display.md). |
| `gfx/d3d9/shaders/render.hlsl` | Two vs_2_0 shaders (VSMain for frame-space vertices, VSMesh for the models, below) and five ps_2_0 (colour, texture, copy, and the models' colour and texture). The texture shader takes about 30 of ps_2_0's 64 instruction slots. The DXSDK's fxc compiles them to bytecode headers at build time, so there is no D3DX dependency on XP. |

## The models on the graphics card

The game's 3D models (`gfx::Model`: courses, cars, parts, the course select's miniatures, the rooms) used to be
transformed, lit, clipped at the near plane and projected on the CPU every frame, then sent to the card again with
`DrawPrimitiveUP`. On an XP test machine the course select (up to ten miniatures, ~98k vertices a frame) and races
with the field in view (~136k) lagged, while the pre-race screen (one miniature, ~14k) ran well.

Now `Model::Load` puts each mesh in a managed vertex buffer once (`Renderer::CreateMesh`; the vertices in index order,
so each material part is a run of them), and `Model::Draw` only fills VSMesh's constants (model-to-view transform,
billboard turn, projection, program 0x20's lights in the model's space, the day/dusk/night weights, tint, distance
cue and fog) and draws each part (`Renderer::DrawMesh`). The card cuts the triangles at view depth 1 (clip z = 1,
w = depth, so the depth test still sees 1/depth). Colour and fog are interpolated straight across the screen as on
the GS: VSMesh sends them times w with w beside them, and PSMeshColor/PSMeshTexture divide. Shadows (the corner
heights) and `DrawWorldTriangles` (particles) keep the CPU path.

### Fog and the guard band

Two things programs 0x30/0x40 do that the port first missed, found from Island Bridge going black in places
by night (field.md, "The time of day"). Both were checked against a PCSX2 GS dump of the bridge at 22:00.

- **Two fog sets.** F = clamp((w.end - w) x scale, 0, max) takes vf10 (the time of day's fog: by night end 300,
  scale 1) for most strips, but vf11 (the distance cue's 800 and 0.5) where the strip's header has bit 1 of its y
  word set (`reference/vu/mpg_0028E748.vsm`, 0x0118-0x0138 and 0x0490-0x04B0: vf12 = vf10, or vf11). About one strip
  in six is flagged (FLD/220.BIN, the bridge: 1,493 of 8,725), among them the bridge's towers and cables; in the dump
  their F equals their alpha in the fade band. modelconv writes the flag as `cuefog` (mesh.h, `MeshVertex::cueFog`);
  VSMesh gets it in the anchor's unused y, and the CPU path in `ViewVertex::cueFog`.
- **The guard band.** The programs clip a triangle with a corner outside |x| <= w, |y| <= w of their clip space,
  which is the frame's centre +-512 pixels across and +-481 down at the field's focal of 512 (in the dump every
  clipper fan's corners lie within those, many on them). The clipper (0x0720-0x0EE0) interpolates position, colour
  and STQ in 3D and fogs each new corner by its own w; the GS interpolates straight across the screen within what's
  left. Straight across a whole big triangle from corners far off the screen, fog and colour come out far from that:
  the other carriageway, past the night fog's 300 at its far corners, drew in the fog colour. The CPU path now clips
  to the band as the programs do (`AddTriangle`). The card can't make new corners, so each mesh vertex carries its
  triangle's other two (`MeshVertex3D::corners`, 96 bytes a vertex from 72), and VSMesh sends a triangle that crosses
  the band with its colour and fog interpolated along with w instead: the clipped fan's corners are where those
  values are exact, so it's the nearest the card comes. Draw calls are unchanged.

The backend also keeps one scene open across a frame's draws (it used to begin and end one per draw) and only sends
render states, sampler states, the texture, the pixel shader, its constants and the scissor when they change.

A model can also be made in two halves: `Model::Prepare` does the CPU's part (the parts grouped by
texture, the vertices in drawing order) without the device, so on a loading thread, and `Model::Load(PreparedModel&&)`
makes the textures and one vertex buffer. A prepared model can hold several sections, drawn one at a time
(`Model::DrawSection`) from the one buffer: the field's 64 cells of a world block are one model. Such a model keeps
no copy of the mesh on the CPU. The backend now also leaves out the stream source and VSMesh's constants when they
are the same as the draw before (the sections of a block share both), and `CreateMesh` no longer ends the scene. A
vertex buffer cost about 0.45 ms to create on the VM whatever its size, so a block in 65 of them (one a cell) cost 30
ms of the main thread; in two it's a few. `LoadObj` reads the whole file at once and parses its numbers itself
(sscanf was most of a world block's 60-200 ms of reading).

Measured with `--perf` on the development PC (a VMware VM; its Present times are VMware's translation, so only the
CPU side is meaningful for real hardware): the course select from 6.5 to 1.8 ms a frame, the pre-race screen from 2.2
to 1.2, a 24-car grid from 12.9 to 4.5, of which the game and its draw calls are under 1 ms. Screenshots match the
CPU path (Endurance Run day, dawn and night, Night Glow, courses 3, 9 and 13, the opening, the course select) to a few edge pixels; the split screen was checked by eye.
The one known difference: a triangle that crosses the near plane gets its cut corner's colour perspective weighted
by the card, where the CPU took it straight along the edge; it shows as faint shading on the floor just in front of
the camera (course 3's wooden floor at the start, under 32 of 255).

Follow-up for the older XP machine (lag when cars came on screen, random spikes):
- **The cars' distance detail, as the PS2 does it** (Car_Draw 0x00221788, Cars_Draw 0x002228A8; cars.md): the near
  body only under view depth / focal 0.03 and for at most six cars, else the far body (and the far propellers);
  the four tires separately only under 0.05, else TIRE.BIN section 3 (all four in one); no car under 0.003. The port
  had drawn every car near. A 24-car grid went from ~141k vertices and 714 draw calls to ~96k and 627.
- **The sound lock:** the audio thread held it while it made a whole 1024-frame buffer, so the game's sound calls
  (every car's engine, every frame) could wait that long. It now takes it per 64 frames (the synth's own block, so
  the output is the same).
- **Managed textures and meshes are preloaded** to video memory when made, not on their first draw.

A `--perf` log from that machine: menus a steady 16.7 ms (vsync); in a race the time before
Present followed the draw calls, about 0.022 ms each (a regression over 3500 frames: R2 0.44 on calls, the vertices'
share nil, so the card does run the vertex shader), 14 ms at ~630 calls on the grid. Frames that went over the vertical
blank showed as 28-33 ms: the "random" spikes. So the draws were cut further, keeping the picture:
- **Consecutive 2D draws with the same state go as one** (the backend holds them until something else draws): the
  HUD and effects from ~143 calls to ~35 on the grid.
- **A model's parts group by texture on the card** (gfx::Model's GpuPart): paint is per vertex in VSMesh, so a car
  body's plain, paint 1 and paint 2 parts are one draw. Parts with translucent texels (anything under the alpha
  test's 0x7F, cut-outs too: filtering makes their edges in-between) keep their place, and the others only group
  between them, so the order that shows is the PS2's.
- Peach Raceway's grid: ~704 calls a frame to ~487 (with the cars' distance detail), screenshots unchanged but for
  the far cars.

### The `--perf` log

`--perf FILE` writes a CSV a line a frame, after `#` lines saying what it ran on: the build's date, the command line,
the Windows version, the processor and memory, and the card (D3D9's adapter description, driver and version, hardware
or software vertex processing, shader versions, texture memory, back buffer, scale, vsync, the desktop's mode). The
columns:

- `frame_ms`: from the frame's start to the one before's (16.7 with vsync when nothing's late).
- The update split by where its time went: `game_ms` the game (logic, and the CPU-projected 3D path's projecting),
  `draw_ms` in the renderer's Draw and DrawMesh (state changes and D3D draw calls, the driver's share included),
  `upload_ms` making meshes and textures and writing textures (streaming), `present_ms` in Present (waiting for the
  card or the vertical blank). The renderer times its parts with the time stamp counter (QueryPerformanceCounter can
  cost microseconds a call on old machines), scaled by the update's own ticks.
- `calls`, the D3D draw calls, and the same by what was drawn (`gfx::DrawSourceScope` at the drawing code):
  `world_calls` (the field's blocks and sky, a course's), `car_calls`, `object_calls` (coins, the field's and
  courses' objects), `effect_calls` (car effects, jets, particles, the field's night lights), `other_calls` (2D: the
  HUD, menus, text). `second_passes`: of the calls, the GS alpha test's second passes. `vertices`: drawn, each pass.
  `copies`: of the bound target, for draws that sample it.
- `meshes_made`, `textures_made`, `textures_written`: the uploads, which with `upload_ms` tell streaming spikes from
  drawing.
- Where the player is: `place` `field` with `id` the town and `x,y,z,yaw` --place's arguments, so `--field ID
  --place X Y Z YAW --clock CLOCK` puts the car back there (from that town's Q's Factory block, Peach Town's or Cloud
  Hill's; y a unit above the car); `race` with `id` the course and its own coordinates; empty on other screens.
  `clock` is the time of day as --clock takes it.

Tried and left out: merging the course's blocks into one model. Grouping its parts freely (~380 calls) changed the
cut-outs' edges where the game draws a solid part after one (course 13's thatch fringes, Endurance Run's fences); keeping the order
across all blocks lost the per-block culling and drew more (~595).

## How it works

- **Frame buffers become render targets,** keyed by FBP and sized by FBW and the scissor. A texture whose
  TBP0/TBW matches a target samples that target. The game does this every frame: it draws the other buffer
  over the new frame with `(Cs-Cd)*FIX+Cd`, FIX 0x20-0x40, which is a motion-blur trail.
  - That's Main_Loop's Frame_BlendPrevious (packets D_0029A930), which runs every frame unless race flag 0x8000 (the
    pause) is up. TEX0 is TFX DECAL, so the RGBAQ it inherits doesn't matter.
  - It uses FIX 0x20, drawn at (0, 0)-(639, 223.5). On a frame with pad +0x28 bit 0x400000 it uses FIX 0x40 at
    (2, 1)-(637, 222.5). Main_Loop clears that bit every frame; the tasks set it as they cut to a new scene.
  - The port's game passes the FIX to Draw2D::EndFrame: 0x20 on the field, in the rooms, the races and their
    results, and 0 while a race is paused (Race::FrameBlend). The 0x40 frames and the slight shrink are left out.
- **Uploads over a target** (the boot clears memory with uploads) mark it stale. It's reloaded from GS memory
  before its next use.
- **Textures are found in three steps:**
  1. a render target;
  2. the texture set, when an upload is tracked at TBP0 (same PSM and width) and, for T8/T4, one at CBP.
     Uploads are tracked per 256-byte block, and an upload is forgotten when its base block is overwritten.
     That rule matters because the game puts CLUTs in the block right before the next texture, and that
     texture's upload overwrites the CLUT's unused right half;
  3. otherwise, decoding GS memory. Decoded textures are cached until any block they were read from is
     written.
- **Alpha:** the shaders keep the PS2 scale (0x80 = 1.0) for alpha and for vertex colour that modulates a
  texture. Untextured RGB is used as is. Doubling it was a bug, found when the TAKARA text came out brighter
  than PCSX2 at rest; the VU1 lighting sends fronts at 197 and sides at 102. Blends with C = FIX
  output FIX as alpha. The GS alpha test runs in the shader on the integer alpha. Fail modes other than KEEP
  take a second pass that draws the failing pixels with only the writes AFAIL allows.
- **Display:** at vsync, the target named by DISPFB (read circuit 2 here) is shown. In interlaced field mode
  (SMODE2 INT and FFMD) the buffer is half height: the game renders 640x224 and the presenter stretches it
  to 4:3.

## Findings

- **The font is `SYS/FONTE.GSL`** (`FONT.GSL` is the Japanese one): T4 512x384 at 0x1A40 with a CT16 16x16
  CLUT at 0x1BC0 holding 16 palettes. The game writes TEX0 as **T8** with CLD=1, which loads all 256 CLUT
  entries. It then switches to T4 with **TEX2** and picks a palette with CSA (0 for white text, 4 for the
  orange "PRESS START BUTTON"). An inventory that ignores TEX2 reports these draws as T8.
- **GS dump state layout:** GS memory is the last 4 MB of the state before 84 bytes of GIF path state and Q.
  That puts it at offset **425** with PCSX2 2.8. Being 8 bytes off still
  shows textures as almost right but scrambles CLUT entries. It was found by matching FONTE.GSL's CLUT bytes.
- **TEXA** is written every frame as 0x0000004000008080, and the dump's saved TEXA is 0.
