# VU1 microcode

The VU1 microprograms in the ELF's `.vutext`. The build writes the listings to `reference/vu/`; to rebuild them by
hand:

```
python -I scripts/vu/vutext.py   # .vutext DMA/VIF walk -> vutext_chain.txt, mpg_*.vsm
python -I scripts/vu/vuflow.py   # per-entry control-flow summary -> programs.md
```

`scripts/vu/vudis.py` is a VU micro-mode disassembler. Its opcode tables follow PCSX2's `VUops.cpp`,
and it decodes all 959 instructions with no unknown opcodes.

Notation: `M[a..b]` means VU1 data-memory qwords a to b (16 bytes each). `top` is the input double buffer
(XTOP). The status markers are: **[solid]** read straight from the code; **[inferred]** an
interpretation that still needs confirming against PCSX2 dumps or the game code that fills the buffers.

## What .vutext contains (0x0028E730, 0xC100 bytes)

It's a DMA chain built by dvp-as:

- **One `cnt` packet with four VIF `MPG` uploads.** These make up a single microcode image: VU1 micro
  memory 0x0000-0x1DF8, **959 instructions**, about 7.5 KB. It's loaded once at boot.
- **Nine `ret` packets of prebuilt mesh data**, about 41 KB. Each one is:
  - `STCYCL 1,1`;
  - `UNPACK V4-32` of 1 qword to top+0 (the header);
  - `UNPACK V3-32` of the vertex qwords;
  - `MSCALF` into a microprogram slot.

  The game calls these with `Vu1PacketAddCall(addr)`, which emits a DMA `call` tag (0x50000000) to the
  packet. Asset files very likely carry the same kind of VIF packets for models.
- The game's VIF double-buffering is set up with `BASE 0x8D`, `OFFSET 0x1AE`, so the input buffers are
  `M[0x8D..0x23A]` and `M[0x23B..0x3E8]`.

## VU1 data memory map

| Qwords | Contents |
|---|---|
| 0-3 | matrix A. Setup 0x10 overwrites it with A×B ("screen" matrix) [solid] |
| 4-7 | matrix B [solid]; likely clip → screen (viewport scale/offset) [inferred] |
| 8-11 | matrix C [solid]; after setup it's composed into vf13-16 = C×A, the matrix used for clipping [solid] |
| 12-15 | screen matrix loaded directly into vf01-04 by setup 0x00 [solid] |
| 16-18 | 3×3 rotation, composed into vf05-07 [solid] |
| 19 | vf08: fog parameters for programs 0x20/0x50/0x60 (x = min, y = max, z = scale, w = far) [solid ops, inferred names] |
| 20-23 | vf09-12. vf09 = colour-blend weights (programs 0x30/0x40) or a light matrix row (0x20); vf10/vf11 = fog parameter sets for 0x30/0x40 [inferred] |
| 24-25 | vf17/vf18: colour scale and light/offset constants [inferred] |
| 26 | vf31: position offset added to every vertex (programs 0x30/0x40) [solid] |
| 0x1D-0x1F | GIF tag area for clipped output [solid] |
| 0x20-0x3C | clipped-polygon output vertices, 3 qwords each (STQ, RGBA, XYZF) [solid] |
| 0x3D-0x64, 0x65-0x8C | the clipper's two ping-pong polygon buffers, 3 qwords per vertex [solid] |
| 0x8D-0x23A / 0x23B-0x3E8 | input double buffers. Each has a header at +0, vertices from +1 (or +2), a GS state packet at +0x192, and output written after the input [solid] |
| top of memory, down from 0x400 | register save stack (vi14) used around clipping [solid] |

## Common conventions [solid]

- **Header qword (top+0)**, a modified GIF tag:
  - x & 0x7FFF = vertex count (NLOOP);
  - y bit 1 = a game flag (in 0x30/0x40 it selects the fog parameter set vf11 instead of vf10);
  - w = the real GIF tag word 1 (PRIM/NREG), copied into y on output;
  - z = REGS.

  The tag is copied to the output buffer.
- **GS state packet at top+0x192.** If its first word isn't zero, the program kicks it (XGKICK) before
  the primitives and then clears that word. It's an optional per-batch packet of register settings
  (texture etc.).
- **Output** is a GIF PACKED packet of 3 qwords per vertex: **STQ** (perspective-divided texture
  coordinates, Q = 1/w), **RGBA** (ftoi0), **XYZF** (ftoi4: 12.4 fixed-point screen coordinates, F = fog).
- **Fog:** F = clamp((far − w) × scale, 0, max) on the projected w [solid ops].
- **Rejection:**
  - The 3-, 4- and 5-qword programs track the clip flags of the last three strip vertices (CLIP
    instruction).
  - A triangle with a vertex outside gets bit 15 (ADC, "don't draw") set in its XYZF w word.
  - The clipped programs then run the triangle through the clipper and draw it separately; the unclipped
    ones just drop it.

## The 8 programs

| Slot | Entry | Kind | Input per vertex | Notes |
|---|---|---|---|---|
| 0x00 | 0x1268 | setup | - | loads screen matrix M[12-15] → vf01-04, M[8-11] → vf13-16, M[16-21] → vf05-07/vf09-11; resets the save-stack pointer and both GS-state flags |
| 0x10 | 0x1438 | setup | - | composes vf01-04 = B×A, vf05-07 = R×A, vf13-16 = C×A; writes vf01-04 back to M[0-3]; loads M[20-25] |
| 0x70 | 0x1318 | setup | - | like 0x10 but leaves vf01-04 as loaded from M[12-15]; clears M[28].xy and the second GS-state flag |
| 0x40 | 0x0080 | clipped strip | 5 qw: pos, c1, c2, c3, STQ | world-space positions + vf31 offset; colour = 3-way blend of the per-vertex colours weighted by vf09, the time of day's day/dusk/night weights (race.md, "Lighting"); alpha × distance fade |
| 0x30 | 0x03E8 | clipped strip | 5 qw, same as 0x40 | adds a per-batch local transform: rotation vf05-07 + batch position from header qword top+1 |
| 0x60 | 0x1AA0 | clipped strip | 3 qw: pos, colour, STQ | batch matrix vf20 (M[6]) × vf29 (M[25]) term; colour × vf31; backface cull by the cross product of consecutive edges, with winding parity alternating per vertex. Most .vutext packets use this |
| 0x20 | 0x1620 | unclipped strip | 4 qw: pos, normal, colour, STQ | directional light (normal rotated by vf05-07, light matrix vf09-12) plus specular raised to the 8th power; vf17/vf18 select the colour scale by header flags |
| 0x50 | 0x1898 | unclipped strip | 4 qw: pos, ?, colour, STQ | lit by vf10/vf11 with an offset from vf18 (rotated per vertex by MR32); colour × vf17 |

The clipped programs share these subroutines:

| Addr | Role |
|---|---|
| 0x1040 / 0x1150 | push / pop vf17-30 and vi02-15 on the stack |
| 0x0720 (0x0738) | build the triangle in the ping-pong buffer from the last three vertices (clip pos, colour converted back with itof0, STQ), then clip it against 6 planes in turn (Sutherland-Hodgman). Each plane uses a pair of clip-flag masks for the previous/current vertex |
| 0x0C50 | per-edge step: classify and emit 0, 1 or 2 vertices |
| 0x0DD8 | intersect an edge with a plane (t = d0 / (d0 − d1)) and interpolate all 3 qwords |
| 0x0C10 | close the polygon (copy the first vertex to the end) |
| 0x0EE0 | emit the clipped polygon: kick the pending GS state packet or the tag at 0x1D, then transform each vertex by vf01-04, divide, fog, convert to integers, write a fan to 0x20+ and kick it |

## In the port

- **`scripts/vu/vusim.py`** is a VU1 interpreter for exactly these 959 instructions. It runs without pipeline
  timing but with the flag behaviour the code relies on (CLIP history, MAC sign flags, Q from DIV). It's the
  reference, not shipped code.
- **`scripts/vu/vu2cpp.py`** translates the image statically into `tools/common/vu1/vu1_microcode.gen.cpp` (made by
  the build), one switch case per instruction pair, using the helpers in `vu1_ops.h`, which mirror `vusim.py`.
  The API (`vu1.h`) is `vu1::Run(state, entry, top, sink)`, with a `GifSink` that receives every XGKICK'd packet.
  It's faithful by construction, including the register state that leaks between programs: for example, 0x40
  uses vf17/vf18 as scratch while 0x20/0x50 read them as constants. Neither the game nor the converters
  run it: the game draws the models natively (renderer.md), and this is kept as the microcode's C++ reference.
- The C++ matched the interpreter bit for bit on 477 cases (synthetic calls covering every slot, and 170 real
  calls from a race frame), kicked packets and the full final VU1 state.

## PCSX2 validation

Checked against PCSX2 2.8.2 (MTVU off) on a Quick Race at Peach Raceway: a GS dump and a save state of the same
frame, with the frame's two VU1 lists replayed from the save state's EE RAM through the interpreter:
- the world list at 0x01730BF0 (`Vu1PacketOpen(1)`, kicked during the frame);
- the main list at 0x01750BF0 (`Vu1PacketOpen(0)`, kicked at the end of the main loop).

Every resulting GIF packet was looked for in the dump frame. **7,307 of 7,387 packets were bit-identical**; that
frame makes 6,742 microprogram calls covering all 8 slots.
- Every triangle packet from every slot matches: 0x20 (4,412), 0x40, 0x30, 0x50, 0x60, the clipper's fans,
  and all PATH2 DIRECT packets.
- The 80 that differ are all A+D GS-state packets kicked from +0x192. Their TEX0 values differ in the TBP/CBP
  fields, and those packets are uploaded from model data in the loaded-asset region (0x0EB0000-0x0F80000)
  through DMA calls. The game's texture cache evidently patches VRAM addresses into that model data each
  frame, and the save state was taken after the next frame's patches. This is a capture-timing artifact,
  not a VU1 difference: the state packets embedded in the list buffer itself all match.

What the validation settled (in the interpreter, and so in the C++):
- **Rounding is toward zero** (PCSX2 `VU1.Roundmode = chop`). Each op is computed exactly, then truncated
  to float32.
- **MAC flags have a 4-pair delay.** Slot 0x60's backface test runs FMAND 4 pairs after the OPMSUB that sets
  the sign flags, with other FMAC ops in between. It must see the OPMSUB's flags.
- **VIF double buffering:** OFFSET resets DBF and TOPS = BASE; MSCAL latches TOP and then toggles DBF; the
  VIF state carries over between lists in kick order.
- **The overflow edge of round-toward-zero** (an exact value just inside FLT_MAX).

Masked in comparisons: the ST w lane, which the GS ignores. UNPACK V3 fills W with alignment-dependent junk
that the microcode passes through.
