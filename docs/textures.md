# Textures

How the game stores its textures, and the converter that dumps them all to PC formats
(`tools/texconv`). The game loads these PC-format files (DDS) instead
of decoding emulated GS memory.

## Storage on disc

Every texture on the disc is stored as a **ready-to-send GS upload**: the data is exactly what the game
DMAs to the GS.

```
DMA tag   cnt, QWC = whole upload             (upper 64 bits: junk)
GIF tag   NLOOP=4, PACKED, NREG=1, REGS=A+D   (unused fields: junk)
A+D       BITBLTBUF (0x50)  DBP / DBW / DPSM
A+D       TRXPOS    (0x51)  DSAX / DSAY (always 0,0)
A+D       TRXREG    (0x52)  width / height
A+D       TRXDIR    (0x53)  0 = host -> local
GIF tag   IMAGE, NLOOP = data size in qwords (every upload on the disc fits in one packet)
data
```

The "junk" is stack garbage from the original authoring tool (values like `0012F9xx`). It is harmless,
but means only the defined bit fields can be trusted.

- **`.GSL` files** (SYS, ITEM) are nothing but a chain of these uploads, ending exactly at end of file.
  Their DBPs are **relative to a load base**. For example, METER0.GSL puts its T4 at 0x84 with the CLUT at
  0xA4, and the race capture shows that pair at 0x23FE/0x241E (base 0x237A) and at 0x1C44/0x1C64
  (base 0x1BC0).
- **`.BIN` containers** (COURSE, FLD, ACTION, SHOP, CAR*, CARS, SYS) embed the same upload packets
  between their other data. In the course and field files the DBPs are **absolute** and match the race
  capture's TEX0s directly. SHOP/T*.BIN are about 96% texture.
- The **E3D files** (SYS/S00, S01, TAKARA) and CARS/WHEEL.BIN contain none. Their textures, if any, are
  stored some other way (to investigate).

Across the disc there are 8,944 uploads.

### CLUT pairing

A paletted texture (T8/T4) is normally followed directly by its CLUT, a small CT32 or CT16 image. The
exceptions:

- One CLUT can follow several textures (FLD: two T8s share a CLUT uploaded before them).
- Some T4s in field/action files have **no CLUT of their own**. They're probably palette-animated (water)
  or use a shared CLUT.
- A single CT32 T4 palette is uploaded as **16x2**: the left 8x2 cell is the palette (CSA 0), and the right
  half is padding (junk in 2,960 of 2,981 cases).
- A T4 with a **16x16 CT16 CLUT** really has up to 16 palettes (CSA groups): UI and font colour sets.

The authoritative pairing is the TEX0 of each draw. Most TEX0s live inside mesh data, which texconv
doesn't read (modelconv does, for the models), so texconv uses rules:

1. A CLUT serves the pending textures it can serve (T8 needs 256 entries). A 256-entry CLUT with a T8
   pending is that T8's.
2. Textures it can't serve take the most recent compatible CLUT in the same file, and are marked
   `guess`.

**Check against PCSX2:** the 49 textures of a Peach Raceway race capture:

- **29 match an upload's DBP**, and every one of them has the same CLUT as texconv chose.
- **The other 20 aren't stored on disc at all:**
  - 13 are the opponents' 128x128 T8 car textures, packed every 0x44 blocks from 0x243E. They're built at
    run time: the car files only hold a T4 decal sheet, and the paint is applied by the game.
  - 5 are meter textures at their relocated addresses.
  - 1 is the framebuffer (CT24 1024x256).

## The texture set (data/textures)

The converter is `tools/texconv`, built with the solution. Its output is the
game's texture set in `data/textures`, which the build makes from the disc.
Usage and modding notes are in `data/README.md`; the format is in
`src/gfx/textures/texture_set.h`.

```
bin\Release\texconv.exe game data [--force] [--png DIR]
(--png DIR also writes DIR\index.csv, which lists every image)
```

How texconv works:

- It reuses `tools/common/gs/gs_memory`. The upload path (`Transfer`, `LocalMemory::WriteImage`) and
  `DecodeRect` were added for it, and the in-game GS translator will use them too.
- It scans every .GSL/.BIN/.E3D for the A+D register pattern and checks the GIF tags; two false matches
  inside pixel data are rejected that way. It then replays each upload into an emulated GS memory and
  decodes each texture with its CLUT, using the game's TEXA (0x0000004000008080 from the captures:
  TA0=0x80, AEM=1, TA1=0x40).
- **Output:** `textures/<DIR>/<FILE.EXT>/<n>[_p<k>].dds`, uncompressed A8R8G8B8, plus a manifest
  `textures/<DIR>/<FILE.EXT>.txt` per game file. The extension is kept because SYS has both PUTI.BIN and
  PUTI.GSL. n is the texture's number within the file, and p<k> its palette when there are several.
- **Alpha is converted to 0-255:** PS2 0x80 becomes 255, and anything lower is doubled.
- **Identical images are written once**, keyed by a hash of the size and pixels. Later manifest lines
  point to the first copy.
- **Existing .dds files are kept** (they may be mods) unless `--force` is given. Manifests are always
  rewritten.
- `--png DIR` also writes PNGs and `DIR/index.csv`, with one row per (texture, palette): file, offset,
  number, palette, PSM, size, DBP/DBW, CLUT PSM/CBP, clut_match, palette count, both upload hashes and the
  image path.

**Run-time lookup key:** FNV-1a 64 of the texture's upload data, the same hash of its CLUT's upload data
(0 for direct colour), and the palette (CSA). The game DMAs the file bytes to the GS unchanged, so the GS
translator can compute the same hashes from what it receives. That makes the key independent of load
address (GSL relocation) and of which file the data came from. Whatever isn't in the set falls back to
decoding GS memory: run-time textures, CLUT animation, and wrong `guess` pairings, whose real pair
simply misses.

Results:

| | Count |
|---|---|
| Textures | 5,758 |
| Distinct lookup keys | 4,024 (23 from guessed CLUTs) |
| Unique images | 2,946 |
| Guessed CLUTs | 130 |
| DDS on disk | 356 MB |
| PNG review copies | 53 MB |

- FLD has many identical files, and the 32 SCOREA files share most of their images.
- The full run takes about 14 s.
- Loading all manifests takes 47 ms.

## The packed set (data/textures.rpk)

A player's copy has the set as one file, `data\textures.rpk`, made by `tools/rpak` in the Release build's
post-build step (the repo and development builds keep the loose files). Format: `src/util/data_pack.h`.

- **One file, memory-mapped**, with a table of contents read once: no folder scan or 3,300 file opens. The
  texture set mounts it over `data\textures`, and every reader (`LoadDds`, the manifests, tex_anim's
  `u<DBP>.dds` check) goes through `util::Data_*`/`Pack_Find`: a loose file wins, else the pack's copy. That keeps
  modding as before, and the MTL files' relative paths work unchanged.
- **Images as palette indices.** 2,961 of the 2,974 images have at most 256 colours (they were T4/T8 on the
  PS2): 2,216 are stored as 4-bit indices, 745 as 8-bit, 13 as A8R8G8B8. The palette is in order of first
  appearance, so the palette variants of one T4 (`_p<k>.dds`) get identical indices and share them. Lossless,
  and it changes nothing on the GPU: the loader expands to A8R8G8B8 as before.
- **Deflate** (RFC 1951) for every blob: `image_write`'s compressor (LZ77 with lazy matching, then dynamic
  Huffman blocks of up to 16,384 tokens, fixed ones where they come out smaller), and a complete inflate in
  `data_pack.cpp` (stored, fixed and dynamic blocks), so streams from zlib load too. A CRC-32 of each blob's data
  catches damage, which deflate alone mostly doesn't. No third-party code.

| | Size |
|---|---|
| Loose DDS | 376.7 MB |
| Indices + palettes, stored | 64.8 MB |
| Indices + palettes, deflated (the pack) | 20.8 MB |
| (the first encoder: greedy matching, fixed Huffman only) | 24.2 MB |
| (zlib level 9 instead, for comparison) | 20.4 MB |

Checks: unpack gives back every file byte for byte; zlib decodes every blob, and the pack rebuilt
with zlib's streams (dynamic, stored, multi-block) unpacks identically; `rpak test` decodes all 65 MB of blobs
in about 0.2-0.3 s; flipped bits are caught (the only misses decode to the same data); texset_test passes on
the pack alone, with the manifests loading in 15 ms instead of 62; and title, race and field frames render the
same from the pack as from loose files, with a loose DDS overriding the packed one.

The models and collision data are packed the same way (`data\models.rpk`, `data\collision.rpk`,
mounted by main.cpp; `LoadObj`, its MTL reader and `Collision::Load` read through `util::Data_ReadFile`). They are
raw blobs, deflated:

| | Loose | First encoder | Now | Pack time (first → now) |
|---|---|---|---|---|
| textures | 376.7 MB, 3,305 files | 24.2 MB | 20.8 MB | 2.0 → 2.5 s |
| models | 555.7 MB, 14,509 files | 93.5 MB | 74.2 MB | 8.9 → 12.4 s |
| collision | 54.3 MB, 130 files | 10.2 MB | 9.2 MB | 0.6 → 0.9 s |

Unpacking costs the same with either encoder (`rpak test`: models 1.7 s for 556 MB, textures 0.3 s). Python's zlib
decodes every blob of all three to the right CRC, collision unpacks byte for byte, and race (courses 0 and 13) and
field frames from the dist folder (packs only) are identical to the loose files'. On the VM with the loose files
in the cache, the field's heaviest loading frame took 178 ms from the pack and 165 ms loose; a race's start was the
same.

## Next

- **Use the set in the renderer.** The GS translator tracks which upload (hash) sits at each block address. A
  draw's TEX0 (TBP0, CBP, CSA) then gives the key, and the D3D9 backend creates the texture from the DDS
  instead of decoding GS memory.
- **Mipmaps.** Optionally build mip chains into the DDS files for better PC filtering.
- **Runtime-built textures** (car paint, the title's animated upload, palette animation) stay on the
  GS-memory path until their code is reversed.
- **Confirm the `guess` pairings** from the mesh data's TEX0s once the course/field formats are reversed,
  or from more PCSX2 captures (town, shop, minigames).
- **E3D textures.**
