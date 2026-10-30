# Boot sequence and title screen

What the original code does, as worked out from the decomp and the `boot_01.gs` capture, and how
`src/game/title` reimplements it. The reimplementation is high level: it keeps what the player sees
(order, timings, placement, fades, the trail effect) and draws it straight through `gfx::Draw2D` from the PC
data set, with no GS emulation. Functions we've named are listed in `docs/names.csv`; renamed decomp files
keep their original address in the header.

## Structure in the original

- **`Main_Loop` (0x002003B8):** boot (IOP modules, memory card, pads), then once per frame:
  1. run the task tree (`Task_RunAll`, 0x00204F98);
  2. `Frame_BlendPrevious`;
  3. swap the display.
- **Tasks:** a pool of 48 (`Task_Create` 0x00204C48, `Task_Delete` 0x00204DE8). Each task has:
  - +0x08: age in frames, incremented after each run;
  - +0x0C: function;
  - +0x18: parent;
  - +0x1C: argument;
  - +0x24: "child finished" flag;
  - +0x25: state.
- **`func_002052D0`, the root task:** starts the loader (`func_0020DF98`), `func_0020DC50` and
  **`TitleSeq_Task`**.
- **`TitleSeq_Task` (0x0020A2B0):** a state machine at +0x25 that runs one child screen at a time:

  | State | Child | Next |
  |---|---|---|
  | 0 | `Logo_ConspiracyTask` | 1 |
  | 1 | `Logo_TakaraTask` | 2 → title |
  | 2/3 | `Title_PressStartTask` | 4 |
  | 4 | timed out: `Title_AttractDemoTask` (NOW LOADING, then the attract demo; any button returns) | title |
  | 5 | Start on the title (pad mask 0x08000800): `Title_MainMenuTask` (0x00209CB0) | — |

  The first child is `func_0020A520`, the memory card check, which runs before state 0.

## Screens

Frame counts are at 60 Hz. Colours are PS2 RGBA words, where 0x80 = 1.0.

- **Conspiracy logo, `Logo_ConspiracyTask` (0x002086B0):** `SYS/LOGO.GSL` #0, T8 384x384, drawn at (128, 32)
  on black.
  - Fade-in: `Fade_InTask`, overlay alpha (32 - t) * 4 for 32 frames.
  - At frame 212 (0xD4): `Fade_OutTask`, alpha t * 4.
  - It ends at frame 244.
- **TAKARA logo, `Logo_TakaraTask` (0x00208848):** `SYS/TAKARA.E3D`, a 3D mesh of 1,428 triangles, red, lit.
  The background is 0x80FCFCFC. Camera struct `D_0029ABD0`:
  - Position: (0, 0, -50).
  - +0x10, projection scale: (64 - t) * 781.25 + 10000 for t < 65, so it zooms out from 60,000.
  - +0x14 / +0x16, angles in 1/65536 of a turn: 0x1C00 and 0x1100, then (128 - t) * 0x70 and (128 - t) * 0x44
    for t = 65..128, ending at 0.
  - It ends at frame 165.
  - The reimplementation applies the angles as yaw and roll, which matches the capture well enough.
- **Title, `Title_PressStartTask` (0x00208A88):**
  - `SYS/TITLE.GSL` #0, the 640x448 background, at (0, 0), and #1, the 440x220 plate, at (16, 24).
  - Fade-in: alpha (15 - t) * 8 over 16 frames.
  - "PRESS START BUTTON" is printed with palette 4 at text cell (10, 11) while `t & 0x20`, until 1800 frames.
    The strings are at 0x002EA090 (EN), 0x002EA0A8 (FR) and 0x002EA0C8 (DE), chosen by `D_00333284`.
  - At 1816 frames it hands over to the attract demo (below).
  - Start goes to the main menu (below).
- **Every frame, `Frame_BlendPrevious` (0x00208120):** the previous frame is drawn over the new one with
  `(Cs-Cd)*FIX+Cd`, FIX 0x20 (0x40 on the boot loading screen). `gfx::Draw2D::EndFrame` does the same with
  two render targets.

## The attract demo

`Title_AttractDemoTask` (0x0020A150) is a race with no player: race mode 8 (info+0x0A) on Adventure row 24, the
Endurance Run (course 0x11, C20), to REPLAY.TSQ (song 0x0A180201). Nothing is recorded or replayed and nothing is
drawn over it; it is the same race every time. (The PC's Demo Song extra plays DEMO.TSQ's song instead:
extras.md.)

- **Its first frame:** mode 8, the last picture held (info+0x28 0x400000), `D_0177EA50[0]` = save record 0's car 0,
  Race_Setup with arg 0x18 (the row), Sound_LoadSong.
- **While the loader works** (info+0x0F): the held picture under a black 8/128 each frame, "NOW LOADING..." at text
  cell (12, 10). The race's loader jobs (0x370317 the cars, 0x750000 the course) carry 0x100000, so the loader draws
  nothing of its own. The first idle frame starts the song.
- **Frame 3600:** Sound_FadeOutSong; then black at alpha 2n over 64 frames; at 3664 (loading included) the task ends
  and TitleSeq goes back to the title, which counts to the demo again.
- **Any button** newly pressed on either pad (info+0x30), once loaded, in TitleSeq's state 3: Sound_StopSong, the
  demo and the race deleted before they run, the title (from its fade-in), info+0x28 = 0, SE 0x1F.

Mode 8 is tested in three places only:
- **Race_Setup (0x0020F950):** no HUD file, Meter_Draw, loading-fade task or Race_Task (so no countdown, course song,
  laps, order, finish or replay); race flags 0x104 at once (green); its own car loop (0x0020FE18): list entries 22
  down to 1 as cars 0-21, each in the grid slot of its entry, lane entry + 3, computer-driven (0x80) on the row's
  parts. Entry 0 is loaded but doesn't race.
- **Car_Init (0x00218458):** lap zone +0x19A = 1 (0x00218E84), so the AI steers from the first frame.
- **Camera_ChaseTask (0x0021E690):** on its second frame it becomes Camera_TrackSide on car 4 (Mien) for good.

No car sounds (they need a player's car). Course 0x11's clock runs from 0x23280, lit by car 0's height.

**In the port:** `TitleSequence::Title_AttractDemoTask` and `Race::StartDemo` (`Race_CreateDemoCars`), on the
Quick Race's Race. GUESS: the PS2's load time is unmeasured; the PC shows NOW LOADING for 64 frames and loads the
course on the second. Save record 0's teammates (the AI caps) and car 0's part flags are whatever record 0 holds, as
on the PS2 (0 at a cold boot). Checked in the port by letting the title time out, and with a press partway; not
against the PS2.

## Text

- **`Text_Print` (0x00204310):** prints at x = col * 20 + 10, y = (row * 12 + 5) * 2. The colour argument is
  the font palette (CSA), set with a TEX2 write.
- **`Text_PrintAt` (0x00203FC8):** prints at pixel x and half-line y (it doubles y).
- **`Text_DrawGlyph` (0x002017B0):** `SYS/FONTE.GSL` is 16x24 cells, 31 to a row, starting at character
  0x21 (`!`). Each glyph is centred in its cell with its width from the table at 0x0029A860, indexed by
  character code. The game uses the T4 atlas with 16 palettes. (The first reimplementation assumed 32 a row from
  0x20. Uppercase letters land on the same cells either way, so "PRESS START BUTTON" hid the mistake.)
- Accent escapes (`/`, `` ` ``, `^`, `~`) pick other cells (text.cpp, AccentCell).
- **Two-byte characters** (Shift-JIS, first byte 0x81 and up) are 20x20 cells, drawn 2 pixels lower, from the
  table in `func_00200FE8`. The menus use the pad buttons: 0x819B circle (u 0xA0), 0x817E cross (0xB4), 0x81A0
  square (0xC8), 0x81A2 triangle (0xDC), all at v 0xE0.

## Main menu and windows

What Start on the title opens. Reference captures of the original are in `captures/menu`.

**Windows (`core/winman`).** Every menu is a window: up to 15 text lines, some of them items. Coordinates are
x in pixels and y in half-lines (the game doubles y; a line is 12 half-lines).
- `WindowAlloc` (0x00205C20) starts one empty, centred on (320, 112).
- `Window_AddLine` (0x002058A8) adds a line. The window grows around its centre to the widest line, and each
  line moves it up 6 and makes it 12 taller. Line flags:
  - 0x10: an item;
  - 0x80: continues the line before;
  - 2: the current setting, drawn in palette 5 (green);
  - 1: palette 6;
  - 8: right-aligned.
  Items are drawn 20 pixels in, plus an indent that centres them under a wider heading.
- `Window_Place` (0x00205AD0): bits 0-1 place it horizontally (centre, x 0x34, or right edge at 0x24C); bits 2-3
  vertically (centre on 0x70, top at 0x18, or centre on 0xA8 with the bottom no lower than 0xD8).
- `Window_Update` (0x00205C08 → `Window_UpdateList` 0x00205350) runs it every frame.
  - It returns 1 for Cross (also Start, with mode bit 3), -1 for Triangle, and -2 when a closing window is gone.
  - State +0x0E: 0 open, 1 opening (fades in over 8 frames), 2 closing (fades out over 16). Bit 0x80 marks it
    inactive while a child menu runs: it shows its bar but not the cursor, and ignores the pad.
  - The cursor (`Menu_MoveCursor`, 0x00206D28) moves on d-pad presses and wraps at the ends. The pad's repeat
    moves it too, but stops at the ends. Each move plays 0x1D.
- **Drawing** is all from the font atlas, each piece with its own palette (CSA):
  - `Window_DrawFrame` (0x00202350) is a 28x32 nine-slice: style 0 at u 0x12C v 0xDC with palette 8 (blue,
    used by every menu here); styles 1 and 2 at u 0x104 with palettes 14 and 7. The edges are tiled from
    8-texel strips with a region-repeat CLAMP.
  - `Window_DrawBar` (0x00203098), the highlight: u 0x180-0x198, v 0xE0, 28 tall, palette 11.
  - `Window_DrawCursor` (0x00203CD0), the horn: u 0x150 v 0xE0, 38x28, palette 13.
  - Text is palette 0, or 10 in style-2 windows.
- **`Help_Draw` (0x00203240):** the hint box at (0x1AE, 0x1D), 180 wide, text in palette 1. Mode 0 shows
  "×button Enter"; 1 adds "△button Back"; 2 "△button Exit"; 3 shows "×button Apply" and "△button Exit".

**The menus.** Strings are tables of 5 per language, indexed by `D_00333284`.
- **`Title_MainMenuTask` (0x00209CB0):** the title picture with the window placed at 0xA (lower right), help
  mode 0. The items are Adventure, Quick Race, 2 Player, Options and Results (D_0029AD20).
  - 2 Player is only added while a second pad is connected; the window is rebuilt when pads come or go. With no
    pad at all it shows "There is no controller." The PC version always shows 2 Player.
  - Only pad 1 can pick Adventure and Quick Race (pad 2 gets the 0x1C buzz).
  - A pick plays 0x1A. Adventure, Quick Race and 2 Player return the choice to `TitleSeq_Task` (+0x04, flag
    0x400000), which leaves the title. Options and Results start child tasks and make the window inactive.
- **`Options_MenuTask` (0x002092D8):** Vibration, Speaker, Sound Volume, Exit. The table also has Language, but the
  US build leaves it out (the PC's Language extra puts it back: extras.md). The window is centred and set to 180
  wide after the lines; help mode 2, or 3 while a child runs. Triangle moves the cursor to Exit and takes it (0x1B,
  then the window closes).
- **`Options_VibrationTask` (0x00208C98):** the heading "Vibration", then On, Off, ANALOG mode button and TEST.
  - The mode lives in the pad state at +0x0E: 4 on, 0 off, 2 the ANALOG button decides. The current one is green.
  - TEST rumbles both motors (`func_0020ACB8`). Picks play 0x1A and keep the window open; Triangle closes it.
- **`Options_SpeakerTask` (0x00208F50):** Stereo or Mono. It's read with `Sound_GetSpeaker` (0x00257BA8) and set
  with `Sound_SetSpeaker` (0x00257B00), driver RPC 8. Plays 0x1F and closes.
- **`Options_VolumeTask` (0x002090C0):** 5(MAX) down to 0(mute). The step goes in `D_00333290`;
  `Sound_SetMasterVolume` (0x002583F0) gets (5 - step) * 0x3FFF / 5. Plays 0x2F and closes.
- **`Title_ResultsTask` (0x0020A580):** darkens the picture (black overlay, alpha 2t up to 0x40, kept under
  everything after) and runs the memory card screens (`Mc_LoadSlotWindowTask` 0x00228D10: pick a slot, the load into
  record 0, or "There is no data."). A save loaded (result 1) starts `VersusRecords_Task`; anything else, or that
  ending, brings the main menu back.
- **`VersusRecords_Task` (func_00269F48):** the loaded save's 2 Player records, not the Quick Race's or the
  Adventure's. A centred menu {Oppose, Course} (D_002DC918); with no rivals (the leading `rivals[]` with a name)
  Oppose is overprinted in colour 6 and Cross on it buzzes (SE 0x1C); Triangle leaves (SE 0x1B).
  - **Oppose:** the rivals' names (a list at (20, 22), 200 wide, 10 shown) and a panel (Window_DrawFrame 240, 22,
    380, 176) of "WIN :" and "PLAY" over the ten 2 Player course rows (D_002DC920), "%3d:%3d" right-aligned at x 600
    on half-line 33 + 29i/2, and "Total".
  - **Course:** six courses (D_002DC930: Endurance Run, Desert, Night Glow, River, Sunny Beach, Tunnel Race; the
    last is row 6) at (40, 22), and a panel (40, 110, 560, 88): BEST LAP and BEST TIME, 1ST-3RD with a time and a
    NAME line each, from `versus[row].topLaps` / `topTotals` (bits 0-23 the time, 0 empty; byte 3 whose, -1 the
    player). The PS2's BEST TIME column names a rival by the lap entry's owner (asm 0x0026A800, a bug); the port
    uses the total's.
  - Triangle in either goes back to the menu. The original ignores the list windows' own Cross/Triangle and makes a
    fresh window for each state (no close fade).

**In the port** (`src/game/ui/window`, `game/title/title_menu`): the window system as above.
- Options works: the volume and speaker settings drive the synth's master level and a mono mix. The PC adds
  Controls (controls.md) and Extras (extras.md) before Exit; the setting windows are `ui/option_windows`, which the
  field menu's Settings > Other opens too.
- Vibration is remembered and drives the controller's motors; TEST runs pattern 7 (effects.md).
- Results loads a save as Continue does (`AdventureLoad`) and shows its 2 Player records (`VersusRecords`,
  `game/title/title_results`).
- Adventure opens New Game / Continue and the name entry (see `new_game.md`). Continue loads save1.json or save2.json
  (save.md, "When the game saves") and goes on to the saved town's Q's Factory. Quick Race opens the course select
  (see `course_select.md`). 2 Player opens its own menu, then its course select
  (two_player.md).
- Start (Enter) also chooses in every window, a PC change: the original only allows it with mode bit 3.
- Keyboard (the defaults; Options > Controls changes them, controls.md): arrows, Z Cross, C Triangle. Pad repeat follows the game's pad reader (`func_0020AB10`):
  the first repeat 21 frames after the press, then every 3 frames.

**Timing and fades, checked frame by frame against PCSX2**:
- **Windows appear a frame late.** A menu task makes its window on its first frame and starts updating it on the
  next, so nothing shows on the frame it's made. Some examples:
  - title → main menu: a hard cut; the hint box shows on the menu's first frame, the window on the second;
  - Options: picked on frame N, the window starts fading in on N + 1.
  `Window::Update` reproduces this by doing nothing on its first call.
- **Opening and closing.** A window opens over 8 frames (alpha 16, 32 ... 128), with the cursor once it's fully
  open. It closes over 16 frames (120 ... 0), with Triangle's move to Exit already shown on the first.
  - The fade is a TEXA write (`func_00203990`: TA0 = x, TA1 = x/2). The menu pieces' CT16 colours are expanded with
    the game's usual TEXA 0x0000004000008080, so this scales every texel's alpha by x/0x80. In the port that's
    the sprites' vertex alpha.
- **The Adventure menu doesn't fade.** Its window appears whole; only the black overlay ramps (alpha 2t, up to
  0x40 at 32 frames), under the windows. Results does the same. Back is a hard cut to the main menu.
- **New Game.** The PS2 holds the menu with "NOW LOADING..." while it reads the opening scene (about 10 frames
  in PCSX2), then cuts to black. The previous-frame blend (FIX 0x20) leaves a 2-frame afterimage.
  - The name entry then fades in over 16 frames.
  - The port skips the loading hold, since it has nothing to load.

## Data the reimplementation uses

- **Textures:** `data/textures` via `TextureSet::Find(source, number, palette)`: `SYS/LOGO.GSL` 0,
  `SYS/TITLE.GSL` 0 and 1, and `SYS/FONTE.GSL` 0 with palettes 0-15 (text and the window pieces).
- **Model:** `data/models/SYS/TAKARA.obj`, made by `tools/modelconv`. The E3D lit-strip
  layout is described in that tool. (`SYS/S00.E3D` and `S01.E3D`, the course miniatures, use the model-table
  layout: see `course_select.md`.)
