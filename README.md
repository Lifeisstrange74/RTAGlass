# RTAGlass

> **Unofficial fan project.** Road Trip and Choro Q are trademarks of their owners. This project is not
> affiliated with or endorsed by them.

RTAGlass is a native Windows port of **Road Trip** (PS2, SLUS-203.98; Choro Q HG 2 in Japan, Road Trip Adventure in Europe). It is a high-level
reimplementation in C++. It runs on Windows XP SP2 and later.

**This repository contains none of the game.** It holds the port's own code, tools and research. The build makes
everything else from your own copy of the disc: the textures, models and sound, the disassembly, and the tables the
code reads from the game's executable.

## Building

You need:

- **Road Trip Game Files** as a `.cue`/`.bin` (or `.iso`). Only the USA release (SLUS-203.98) works, and
  the build checks every file on it.
- **Windows 10 or 11** to build. The game it makes runs on XP and up.
- **Visual Studio 2022** (Community is enough) with these individual components: **MSVC v141 - VS 2017 C++ x64/x86
  build tools** and **C++ Windows XP Support for VS 2017 (v141) tools**.
- **DirectX SDK (June 2010)**, at its default path or given with `--dxsdk`. On Windows 10/11, install .NET
  Framework 3.5 first. An S1023 error at the end of its setup is harmless.
- **Python 3.11 or later.** The build installs the packages it needs (`requirements.txt`) into a private `.venv`.

Then run:

```
build.bat "D:\Dumps\Road Trip (USA).cue"
```

With no argument, it uses the one `.cue` it finds in the repo's folder or the folder above it. The build takes a few
minutes. It finishes with the game in **`dist\RTAGlass`**, a folder you can copy anywhere and play from. For sound,
players need the DirectX End-User Runtime (see `package/README.txt`, which is copied into that folder).

What `build.bat` does, step by step (each step's log is in `obj\build`):

1. **check**: the tools above, with what to install when one is missing.
2. **disc**: extracts the disc to `game\` and checks every file's SHA-1 against `scripts/build/disc_files.txt`.
3. **decomp**: splat's disassembly of the executable into `reference/asm` (config: `reference/splat`), and the VU1
   microprograms into `reference/vu`.
4. **generate**: the tables the code compiles in, from the executable (`scripts/elf/gen_*.py`,
   `scripts/gen_parts_tables.py`, writing `src/**/*.inc`), and the VU1 microcode C++ the converters use
   (`scripts/vu/vu2cpp.py`). The game's own text (names, menus, messages) isn't in the sources either: they name it
   by where it is in the executable, `GT_<address>_<length>`, and `scripts/elf/gen_game_text.py` defines each name
   from your copy in `src/game_text.inc`.
5. **tools**: builds the converters.
6. **data**: converts the disc into `data\` (texconv, modelconv, sndconv, and `scripts/course/make_c16.py`).
7. **game**: makes the icon from the title screen, builds the solution, and assembles `dist\RTAGlass`.
8. **verify**: compares everything it made with the reference build (`scripts/build/expected.txt`).

## After the first build

Once `build.bat` has run, the repo is a normal Visual Studio solution. Open `RTAGlass.sln`, or from the repo root:

```
"C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" RTAGlass.sln ^
    -p:Configuration=Release -p:Platform=x86
```

Add `-t:RTAGlass` (or `modelconv`, ...) to build one project. Output goes to `bin\Release`, and the game's Release
build assembles `dist\RTAGlass` again: the exe, the player README, a mirror of `data` with the textures, models and
collision data packed into `.rpk` files, and `tools\rpak.exe` and `tools\adpconv.exe` for modders.

To rerun one converter or generator by hand, point it at the extracted disc in `game`:

```
bin\Release\texconv.exe game data          (texconv and sndconv keep existing files; --force rewrites them)
set RTAGLASS_GAME=game
.venv\Scripts\python -I scripts\elf\gen_golf.py
```

Formats and converter flags are in [`data/README.md`](data/README.md).

## Running the game

The game can be ran by executing RTAGlass.exe from the dist folder. Please ensure that the `data` folder remains next to the executable.

## Documentation index (`docs`)

| Area | Docs |
|---|---|
| Overview | `names.csv` (every function the port reimplements), `debug.md` (the original's leftover debug features), `extras.md` (PC switches for unused content, debug features and cheats) |
| Graphics | `renderer.md`, `textures.md`, `vu1-microcode.md`, `cars.md`, `effects.md` |
| Title and menus | `title.md`, `new_game.md`, `course_select.md`, `pause.md`, `controls.md` (the PC's control setup), `display.md` (the PC's display settings and wide screens) |
| Races | `race.md`, `car_physics.md`, `ai.md`, `hud.md`, `finish.md`, `events.md`, `minigames.md`, `two_player.md` |
| Adventure | `field.md`, `buildings.md`, `messages.md`, `parts.md`, `paint.md`, `field_menu*.md`, `save.md` |
| Sound | `sound.md` |

## Releasing

`dist\RTAGlass` is the player folder. Make sure it holds no `save1.json`/`save2.json`, `controls.json` or other
settings files (the exe makes its own; ones there are someone's test files), then archive it. **It contains the
converted game data**, so share it only where you have the right to; anyone with the disc can build their own.
