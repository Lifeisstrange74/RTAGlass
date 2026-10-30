# Game data

PC-format assets the port loads at run time. They are made from your copy of the original game; nothing
here comes from the disc as-is.

A Release build assembles the player folder `dist\RTAGlass`: `RTAGlass.exe`, `README.txt` (from
`package`) and a mirror of this folder, with `textures`, `models` and `collision` packed into `textures.rpk`,
`models.rpk` and `collision.rpk` (rpak, below). That folder is all a player needs, and it can be copied
anywhere.

## textures/

Every texture on the disc, converted to DDS (A8R8G8B8, uncompressed).

Players get this folder packed: the Release build writes `dist\RTAGlass\data\textures.rpk` with rpak (below)
instead of copying the 3,300 files. The game reads a loose file where there is one and the pack's copy otherwise,
so both layouts work, and the repo keeps the loose files.

Make or refresh it from the repo root after building the solution (Release shown):

```
bin\Release\texconv.exe game data
```

- `textures/<DIR>/<FILE>.txt` is the manifest for one game file, e.g. `COURSE/C00.BIN.txt`.
- `textures/<DIR>/<FILE>/<n>.dds` holds the images, numbered in upload order within that file. A T4
  texture with several palettes gets one image per palette, `<n>_p<k>.dds`.
- Identical images are stored once. A manifest line can point into another file's folder: the first one
  that contained the image.

Alpha is standard 0-255, so 255 is opaque. (The PS2 used 0-128; the converter rescales it.)

### Modding

Replace a `.dds` with your own image in the same format (A8R8G8B8). Any size works; keep the aspect ratio
so the UVs still line up. For example, GIMP's or paint.NET's DDS export with "uncompressed, ARGB 8888"
gives this format.

Running texconv again **keeps existing .dds files**, so your edits survive a reconversion.

Where a course uploads one picture to several places, modelconv gives each place its own copy, `u<DBP>.dds` (e.g.
`textures/COURSE/C12.BIN/u3150.dds`), because the race animates them apart. That course's models use the copies,
so edit those for it; delete them to have modelconv copy the shared picture again. Add `--force`
to rewrite everything from the original game. Add `--png DIR` to also get PNG copies to look at or edit.

Because an image is shared by every texture that uses it, replacing it changes all of them.

### The pack (rpak)

`bin\Release\rpak.exe` (also in the player folder's `tools`) packs a folder into one `.rpk` file and back. The
Release build packs `textures`, `models` and `collision` this way; the examples use the textures:

```
rpak pack data\textures textures.rpk [--update]     --update: only if something in the folder is newer
rpak unpack textures.rpk data\textures [--force]    keeps existing files unless --force
rpak list textures.rpk                              every file, its kind, size and colour count
rpak test textures.rpk                              decodes everything and checks the checksums
```

Packing is lossless: unpack gives every file back byte for byte. 376 MB of DDS packs to 21 MB, 556 MB of models to
74 MB and 54 MB of collision data to 9 MB. A DDS in texconv's layout is stored as palette indices (4-bit up to 16
colours, 8-bit up to 256, the pixels themselves above that) and deflated; anything else (the OBJ/MTL and .pmp
files, or a DDS from another tool) is deflated as it is, and the game reads it the same. On a player's copy a mod
can be loose files in the folder (`data\textures`, `data\models` or `data\collision`: they win over the pack's) or
a repacked `.rpk`. The format is in `src/util/data_pack.h`, the background in `docs/textures.md`.

### How the game finds them

The reimplemented screens ask for textures by game file, number and palette, e.g. `SYS/TITLE.GSL` 1 for the
title's licence plate. The GS translator, which plays the original game's GS output, finds them by upload
content instead. Each manifest line keys an image by a hash of the texture's raw GS upload, a hash of its
palette upload, and the palette number. When the game uploads a texture, the renderer hashes the same bytes and looks it
up. Anything not found decodes from emulated GS memory as before: textures the game builds at run time,
such as car paint, and any texture whose palette the converter guessed wrong. The format is described in
`src/gfx/textures/texture_set.h`; the background is in `docs/textures.md`.

## story/

`messages.txt`: the Adventure's message tables (who says what in each building), from the executable. modelconv
writes it with the models (below). One line per message, `N TEXT`, after a `table TOWN PLACE` line; message 0 is the
speaker's name, and control codes and other bytes outside plain ASCII are written `\xHH` (docs/messages.md). Edit
the text to change what people say; keep the codes.

## models/

3D models converted to OBJ, with per-vertex colour (`v x y z r g b`, then the alpha `a` when it isn't 1), triangles, and normals or texture
coordinates. Make or refresh them the same way, after the textures (models refer to them):

```
bin\Release\modelconv.exe game data
```

- `models/SYS/TAKARA.obj`: the TAKARA logo (lit, untextured).
- `models/SYS/OPTION/0.obj`: the field menu Settings screen's DualShock, from SYS/OPTION.BIN (car-file lit strips;
  its picture is `textures/SYS/OPTION.BIN/000.dds`). `modelconv GAME DATA --pad` makes just this one and the coin.
- `models/SYS/COIN/0.obj`: the field's ChoroQ coin, from SYS/COIN.BIN (the same layout; its picture is
  `textures/SYS/COIN.BIN/000.dds`).
- `models/SYS/S00/<id>.obj` and `S01/<id>.obj`: the course miniatures on the course select cards, by course id
  (S00 holds the Quick Race's eight). Each has a material library `<id>.mtl` whose materials point at the DDS files
  under `textures/SYS/S00.GSL/`; the vertex colours modulate the textures on the PS2's scale, so 0.5 leaves a
  texture as it is. Units are the game's: a course spans about 0..200 in x and z, y up.

- `models/COURSE/Cnn/<block>.obj`: the race courses, by course file (course id 0-14 is C00-C14, 15-17 are C18-C20;
  C16 is unused); `models/ACTION/Ann/` the events' courses the same way (course ids 0x12-0x26 are A00-A20). Blocks
  00-63 are the 200x200 cells of an 8x8 grid (block n covers x 200 (n & 7), z 200 (n / 8)), in world coordinates; block 64 holds the see-through things drawn last (fences, banners, trees). `sky.obj` is the
  sky, the same model for every course with that course's sky texture; the game moves it with the camera. `map.obj`
  is the course map the race HUD shows: the track's outline, flat at y 0 in world coordinates, white (the HUD tints it
  blue). C20 (Endurance Run) also has `clouds.obj`, a ring of clouds and stars that shows at night, and
  `horizon.obj`, a band below the horizon that shows by day; both follow the camera like the sky. `objN.obj` are the
  course's moving objects, at the origin (the race places and turns them): Temple Raceway's water wheel (C02) and
  Night Glow Raceway's ride and carousel rings (C05), the Sliding Door Race's door halves (ACTION/A01), the
  Obstacle Course's fence (A11), and in the Adventure's courses C03's doors, C13's cloths and A16's gate. Some
  textures of C02-C05, C10, C12-C14, A02, A11, A13 and A19 are animated in the race by copying a window of a taller
  picture over them, as the PS2 does; a replacement keeps its proportions.

- `models/FLD/<abc>/<block>.obj`: the Adventure's world, one folder per block file (`FLD/<abc>.BIN`, 1600 x 1600 units
  each), laid out like a course's: cells 00-63 of 200 x 200, 64 the see-through things, in the block's own coordinates
  (0-1600); `map.obj` as for the courses. `objN.obj` are the block's objects (up to six), at the origin, which the
  field places: block 39's (`213`) wind turbine sails, the palm fronds of blocks 40 and 42 (`220`, `221`), and the
  items and stamps that wait for the items to be in. `models/FLD/sky.obj` is the field's sky (the race's sky with
  `SYS/SORA.GSL`'s picture). How the blocks fit together is in `docs/field.md`.

- `models/CARS/PARTS/<n>.obj`: the parts (CARS/PARTS.BIN's 12 models): model n is drawn for part flag 1 << n
  (0-8) at a place each body has for it, in palette n of the parts picture; 6 and 7 are the propellers' bar and
  propeller, 11 the propellers seen from far away.
- `models/CARn/Qnn/<section>.obj` and `shadow.obj`: the car bodies (body n is in CAR{n / 30}, 150 in CARS), with
  normals. Section 0 is the body, 1 a lighter version for far away, 2-5 extra pieces chosen by the car's parts (3 the
  fins, 4 the jet turbines and 5 the sticker panels, 4 and 5 textured from CARS/PARTS.BIN's picture); the
  shadow is a quad using the bottom right quarter of the car's texture. `models/CARS/TIRE/<section>.obj`: the tires
  (0 front left, 1 front right, 2 the rear pair, 3 all four for far away). The model's front faces +z.

Other E3D layouts are reported and skipped. An OBJ can be edited in any modeller (Blender imports these with their
textures), as long as it keeps the vertex colours. A material's `map_Kd` can point at any A8R8G8B8 DDS; the
`wrap` line (repeat or clamp for each texture axis) is ours, and other tools ignore it. Two more extensions:
a vertex line can end in a fourth colour value, its alpha (where it isn't 1), and `billboard x y z` makes the vertices
that follow, up to `billboard off`, a sprite that turns about the vertical through (x, y, z) to face the camera
(stored facing -z). Course models are lit and fogged as the race does it (see `docs/race.md`): each vertex has
three colours, for day (the `v` line), dusk (a `vdusk r g b a` line) and night (`vnight r g b a`), one line of each
per vertex in order, and the race blends them by the course's time of day. A `cuefog` line makes the vertices that
follow, up to `cuefog off`, fog by the distance cue's numbers (fully faded at 800) instead of the time of day's fog
(at night 300): the strips the game flags so (`docs/renderer.md`, "Fog and the guard band"). The cars add
`vs s` lines, one per vertex in order, with each vertex's highlight strength (0-255 scale), and a material line
`paint N`: that material takes the car's paint colour 1 or 2 (from the race's car table), multiplied into the
vertex colour (`docs/cars.md`).

## collision/

`Cnn.pmp`: each race course's collision data (the ground and walls the cars drive on), copied from
`COURSE/Cnn.BIN` by modelconv. A 16x16 grid of 100-unit cells: u32 offset[256] and s16 count[256] (at 0x400) of each
cell's surfaces, then at 0x600 a u32 offset and at 0x604 a count of posts (x, y, z, bottom as floats: trees,
lamp posts and pillars the cars' corners hit); each surface is a triangle strip: u32 vertex count, two unused words, u32 flags,
the vertices (x, y, z, 1 as floats), then a unit normal per triangle. Flags: bit 31 a layer (bridges), bit 28
water, bits 0-2 the surface type (grip). Details in `docs/car_physics.md`. `FLD/<abc>.pmp` is each world
block's, in the same format.

## ai/

`Cnn.txt` is the path the opponents drive on each race course. modelconv writes it from the game's
executable, and running it again rewrites the files.

- `line x0 z0 x1 z1 splitX splitZ` rows are gates across the racing line. Where the path forks, the split point
  picks the branch.
- `zone in out prev prevAlt next nextAlt rank speed` rows are the stretches between two gates: their neighbours,
  their place in the race order and the speed the opponents hold there (in 5 km/h, 0 for none).

Edit a speed or move a gate to change how they drive. Details in `docs/ai.md`.

`FLD/Tnn_cc.txt` is town nn's car cc on the field: a `car RECORD X Y Z YAW FLAGS` row (its driver record in the
town's group, where it starts in the town's block, its heading, its part flags), then its loop of gates and zones
as above (none: the car stands). `modelconv GAME DATA --ai` writes just these and the race paths. Details in
`docs/field.md`, "Town cars".

## camera/

`Cnn.txt` holds the trackside cameras that follow your car after the finish and in the replay. modelconv writes
it from the game's executable. Each `camera x y z radius zoom eyeX eyeY eyeZ reach flags` row is used while the car
is within `radius` of (x, y, z), looking from the eye with a focal length of `zoom` per unit of distance. The last
row follows behind the car everywhere else. Details in `docs/finish.md`.

## sound/

The game's sound banks as SoundFont 2 files and its sequences (sound effects and music) as Standard MIDI
Files. Make or refresh them the same way:

```
bin\Release\sndconv.exe game data
```

- `sound/<BANK>.sf2`: `CQ_MAIN` (menus), `ACTION` (race and event sounds), `BGM` (instruments for the music).
  One preset per program, 16-bit samples with their loops, and the original envelopes.
- `sound/<SEQ>/<nn>.mid`: entry `nn` (hex) of a sequence file, e.g. `ACTION/28.mid`, the TAKARA jingle.
- `sound/<SEQ>.txt`: the manifest: which bank the file plays with, and each entry's kind (`se` or `music`),
  priority and loop.
- `sound/radio/1CH.adp` and `3CH.adp`: the car radio's stations, Peach FM and E-Radio: the disc's PS-ADPCM,
  stereo at 12 kHz, with seek points (the layout is in `src/audio/adpcm.h`). The game decodes them as they play,
  to the same samples as decoding the disc's VAGs. Each is an hour (50 MB): one in-game day, and the radio plays
  from where the clock is.

The game asks for a sound the way the original does, as `(slot << 8) | entry` (0x328 = slot 3, ACTION, entry
0x28). The details, including how the MIDI files use controllers, are in `docs/sound.md`.

### Modding

Edit or replace any `.sf2` in an SF2 editor (Polyphone, Vienna, ...) and any `.mid` in a DAW or MIDI editor.
Each track is one channel of the original; songs have more than 16, spread over MIDI ports with port-prefix
events, so keep tracks separate when editing. Songs loop between the `loopStart` and `loopEnd` markers. A
manifest's `bank` line can point a sequence file at a different SoundFont. Running sndconv again keeps
existing `.sf2`, `.mid` and `.adp` files unless you add `--force`; `--wav DIR` also writes every sample, and
each radio station (`radio_1CH.wav`, `radio_3CH.wav`), as a WAV.
A radio station can be replaced by any 16-bit PCM WAV (mono or stereo, any rate): put it next to the station's
file as `1CH.wav` or `3CH.wav` and the game plays it instead. Or convert it to the game's format, a quarter of the
size, with adpconv (`bin\Release\adpconv.exe`, also in the player folder's `tools`):

```
adpconv encode station.wav data\sound\radio\1CH.adp   any 16-bit WAV, mono or stereo, any rate; prints the SNR
adpconv decode data\sound\radio\1CH.adp station.wav   exactly the samples the game plays
adpconv info data\sound\radio\1CH.adp
```

PS-ADPCM is lossy: the game's own BGM rendered at 48 kHz encodes at 35.7 dB SNR, a test tone at 58 dB, and
re-encoding a station's decode at 71.7 dB. The time of day picks the place: midnight is the start and each hour
of the game's day is 2.5 minutes further in, so a full day is an hour of sound (exactly: `docs/sound.md`). A
shorter file wraps round.
