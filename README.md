# RTAGlass

> Road Trip and Choro Q are trademarks of their owners. This project is an **unofficial fan project that is 
> not affiliated with or endorsed by them.**

RTAGlass is a native Windows port of **Road Trip** for the PS2, (known as Choro Q HG 2 in Japan and Road Trip Adventure in Europe). 
It's a high-level reimplementation in C++ that will run on any PC using Windows XP SP2 or above, provided you have a DirectX9.0-compatible GPU.

This repository contains the tools, research, and code for RTAGlass. The build script draws the textures, models and sound, the disassembly, 
and in-game text the game uses from your OWN backup of the game. **THIS REPOSITORY DOESN'T HAVE THE GAME ITSELF.**

## How to build

You need:

- **Road Trip** as a `.cue`/`.bin` (or `.iso`). Only the USA release (SLUS-20398) works, and
  the script verifies you're using the right version.
- **Windows 10 or 11** to build.
- **Visual Studio 2022** (Community is fine) with the **MSVC v141 - VS 2017 C++ x64/x86 build tools**
  and **C++ Windows XP Support for VS 2017 (v141) tools**.
- **DirectX SDK from June 2010** either at its default path or given with `--dxsdk`. On Windows 10/11,
  install .NET Framework 3.5 first. The S1023 error after installation is safe to ignore.
- **Python 3.11 or later.** The build installs the packages it needs (`requirements.txt`) into a private `.venv`.

Then run:

```
build.bat "<the path to>\Road Trip (USA).cue"
```

Without an argument, the script uses the one `.cue` it finds in the repo's folder or the folder above it. The build takes a few
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

After `build.bat` finishes, the repo is a normal VS solution. Open `RTAGlass.sln`, or from the repo root:

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

## How to use your new recomp

Run RTAGlass.exe from the `dist\RTAGlass` folder the project generates. If the `data` folder is not in the same folder as the .exe itself, it won't run.

## Documentation index (`docs`)

| Area | Docs |
|---|---|
| Overview | `names.csv` (every function the port reimplements), `debug.md` (unearthed leftover debug features), `extras.md` (menu f unused content, debug features and cheats) |
| Graphics | `renderer.md`, `textures.md`, `vu1-microcode.md`, `cars.md`, `effects.md` |
| Title and menus | `title.md`, `new_game.md`, `course_select.md`, `pause.md`, `controls.md` (the PC's default control setup), `display.md` (the PC's default display settings) |
| Races | `race.md`, `car_physics.md`, `ai.md`, `hud.md`, `finish.md`, `events.md`, `minigames.md`, `two_player.md` |
| Adventure | `field.md`, `buildings.md`, `messages.md`, `parts.md`, `paint.md`, `field_menu*.md`, `save.md` |
| Sound | `sound.md` |

