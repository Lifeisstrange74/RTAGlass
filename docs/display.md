# Display settings and wide screens (PC)

A PC feature: the window or screen mode, the resolution, the picture's aspect and how sharp it's drawn. The
defaults are the PS2's picture (a 4:3 window, as before), and at 4:3 the frames are pixel for pixel what they were
before the feature (checked against the previous build: a race at 2x, the field, the title and main menu, the
course select, a room and the field menu). Code: `src/platform/display.{h,cpp}` (the settings and display.json),
`src/game/ui/display_screen.{h,cpp}` (the screen), `src/main.cpp` (PlanDisplay, PlaceWindow, ApplyDisplay),
`src/gfx/draw2d.{h,cpp}` (the wide frame and its anchors) and `src/gfx/d3d9/d3d9_renderer.cpp` (Reset, the
modes, the origin).

## The menus

- **Title:** Options is Vibration, Speaker, Sound Volume, Controls, **Display**, Extras, (Cheats), Exit. Display
  runs over OPTION2's background with its name on the panel, as Controls and Extras do.
- **Field menu:** Settings' list is Controls, **Display**, Extras, (Cheats), Other.
- **The Display screen** has the Extras screen's look and workings (list, help window, Initial settings / Cancel
  change / Apply, "Cancel?" on Triangle with changes), with its rows 24 lines apart from y 86 and the values in
  white as the Controls screen's columns (palette 10, grey, for a row the Display row makes moot). Left and Right
  step a row's value (Cross steps it on):
  - **Display:** Window, Full Screen (D3D9's exclusive mode at the resolution) or Borderless (a window without a
    frame over the whole desktop). Alt+Enter switches between Window and Borderless at any time (and from Full
    Screen to Window); Enter isn't passed to the game then.
  - **Resizable:** Off or On. On, the window has a sizing border and a maximize box: a drag (at its end,
    WM_EXITSIZEMOVE), maximize or restore makes the window's new inside the Resolution (written to display.json, so
    it's kept) and the back buffer and frame follow it without moving the window (`TakeResize`). The smallest inside
    is 320x240. With Aspect Ratio Auto the picture takes the window's shape; fixed, it gets bars.
  - **Resolution:** Auto, then the sizes for the mode: for a window the common sizes (640x480 to 3840x2160) and the
    screen's modes that fit the work area with the frame; for Full Screen the adapter's modes (each size once, at
    its highest refresh rate); Borderless is always the desktop's. Auto is the largest 640x480 multiple that fits
    (up to 3x, the old window) or the desktop.
  - **Aspect Ratio:** Auto (the window's or screen's shape, square pixels, never under 4:3), 4:3, 16:10, 16:9 or
    21:9 (64:27). A fixed aspect on another shape gets bars.
  - **HUD:** Edges or Centered (below).
  - **Render Scale:** Auto, or 0.5x, 0.75x, 1x-4x: the frame's targets are this many times 640x448 (times the
    frame's width across; under 1x fewer pixels than the PS2, for slow cards, and the stored sizes are rounded:
    `D3D9Renderer::Px`). Auto takes the smallest that draws the picture at least about as tall as it's shown:
    ceil(shown height / 448 - 0.15), so 480 lines take 1x, 720 and 960 2x, 1080 3x, 1440 4x. It's held to what the
    card's largest texture allows (`MaxScale`: a 21:9 frame at 4x is 4480 wide, past many cards' 4096).
  - **VSync:** On (the default, as before) or Off: Present waits for the screen's refresh or not
    (`D3DPRESENT_INTERVAL_ONE` / `IMMEDIATE`, set at the next device reset).
  - **Frame Limiter:** On (the default) or Off. The game advances one step of the PS2's 60 Hz game a frame, so the
    limiter is the game's speed: On, frames are paced to 60 a second (`RunFrames`); Off, they run as fast as the
    machine (and VSync) let them, and the game itself runs faster, as an emulator's turbo does. More frames a
    second at the game's own speed would need the drawing apart from the game's steps (interpolation), which the
    port doesn't have. With VSync on and the limiter off the game runs at the screen's refresh rate (2.4x on a
    144 Hz screen). Measured on the VM in a race: both on 60 fps, VSync off 60, the limiter off 62 (the VM's
    refresh), both off 347.
  - **Graphics:** Full (the default, the PS2's picture) or Low, for slow machines (below).
  - Apply writes display.json and the change is made before the next frame (`Display_TakeChange`). Initial settings
    is the defaults.

## display.json

Next to the executable, written by Apply (and Alt+Enter), read at start; scripted runs (`--shot`) neither read nor
write it. Entries: `mode` (window, fullscreen, borderless), `resizable` (true, false), `width` and `height` (0 =
Auto), `aspect` (auto, 4:3, 16:10, 16:9, 21:9), `hud` (edges, centered), `renderScale` (0 = Auto, 0.5, 0.75, 1-4), `vsync` and `frameLimit`
(true, false), `graphics` (full, low).
A missing or bad entry takes the default. Command line, for one run: `--scale N` (a window N x 480 high at the
aspect, Render Scale N; 0.5 and 0.75 too), `--aspect A`, `--hud centered`, `--graphics low`.

Checked: 0.5x on the title (applied from the screen) and 0.75x in a race and the field at 16:9 draw right; a
resizable window maximized and restored from outside (ShowWindow) took 1920x1009 and back to 1280x960 in
display.json while running. A drag by hand wasn't tried (the VM over AnyDesk).

## Graphics Low

A switch for slow machines (`gfx::SetLowDetail`, set with the rest in ApplyDisplay; `gfx::LowDetail()` is read
where it's drawn). It leaves the game itself alone: only what's drawn changes, and only where it costs most on the
user's XP machine (draw calls, renderer.md) or on a weak card (fill).

- **A shorter view.** The distance cue's end (`gfx::ViewEnd`) is 600 instead of the PS2's 800, the fade the same
  256 long (344 to 600), for the world, the cars and the objects (Course_WorldStyle, CarStyle, ObjectStyle). The
  culling past the end (Model::Visible) and the field's reach (ViewReach) follow it, so far blocks and sections
  aren't drawn. The strips that fog by the cue (`cuefog`) fog sooner with it.
- **One pass for the faded scenery.** A part without see-through texels under the GS alpha test fails it only by
  the distance cue; Full draws its faded pixels in a second, colour-only pass (renderer.md). Low draws them in the
  part's one pass, blended and writing depth (`Draw3DState::onePass`). A faded pixel then hides what's behind it
  and drawn later, which beyond 344 units is itself faded.
- **No frame trail.** EndFrame leaves out Frame_BlendPrevious's full-frame blend (renderer.md): one draw, but the
  whole frame's pixels read and blended, at the render scale.
- **No car reflections** on the mirrored courses (0x12, 0x1C, 0x20: Cars_DrawMirrored), which draw each car twice.
- **Simpler cars sooner.** The near body under 0.02 of the focal length (Full's 0.03) and for two cars at most
  (Full's six), the four tires as one beyond 0.03 (0.05).

Kept: the night's darkening (it's what the night looks like), the shadows, the particles (a pool of 64, joined
into few draws), the HUD.

Measured with `--perf` on the VM (frames after the first 200; calls a frame on average, then at most):

| Run | Full | Low |
|---|---|---|
| Island Bridge, `--field 1 --place -2200 200 -500 32768 --press 150:x:3000`, 3000 frames | 301 (471), 118k vertices | 215 (361), 72k |
| the same, its first 900 frames | 152 (185) | 130 (135) |
| Peach Raceway's grid, `--race 0`, 500 frames | 422 (455) | 389 (418) |
| course 0x12, `--race 18`, 500 frames | 365 | 193 |

The second passes fall most (107 to 46 a frame on the bridge). Pictures checked by eye: the bridge by day and at
22:00 (the far tower fades out sooner, the darkening is right), the grid, course 0x12.

## Making it so

At start main.cpp works out the plan (`PlanDisplay`: the size, the mode's refresh rate, the frame's width and the
scale), places the window (`PlaceWindow`: framed and centred in the work area, or a popup over the screen, the
cursor hidden over it) and makes the device for it; if the card refuses a full screen mode it falls back to a
window. On a change, `ApplyDisplay` resets the device (`D3D9Renderer::Reset`; a refused full screen mode tries the
adapter's refresh rate, then a window) before the window changes when going to a window, after when going full
screen, then sets the frame's width and the anchors. A reset makes the render targets again, empty; a target made
while the device is lost (Alt+Tab out of full screen) is made with the device objects when it's back.

## The wide frame

The game draws in the PS2's 640x448 frame. On a wider picture the frame is wider: `FrameWidth` = 480 x aspect,
even, 448 high (854 at 16:9, 768 at 16:10, 1138 at 21:9), shown at FrameWidth/480 to 1. The game's coordinates
don't change: Draw2D puts the 640 in the middle of the frame by moving every vertex's x by an origin
(`RenderState::originX`, which the backend adds into c0, the frame-to-clip transform, so meshes and sprites both
move and batching still works by state). Then:

- **3D views see more to the sides (Hor+).** A clip or window rectangle across the whole 640 (x0 <= 0, x1 >= 640:
  every view's `kFrame`, the split screen's halves) stands for the frame's whole width (`Draw2D::Widen`), for the
  scissor and for `Model::Visible`'s culling. The focal and the centre are the game's, so the middle 640 is
  exactly the 4:3 picture.
- **The guard band.** Programs 0x30/0x40 clip at +-512 pixels from the projection's centre (renderer.md, "Fog and
  the guard band"); past 21:9 a frame's edge is farther than that, so the band's reach across is `GuardBandX` =
  max(512, FrameWidth - 320 + 16) (VSMesh's c4.z, the CPU path's AddTriangle). Up to 16:9 it stays 512.
- **Fades.** A Fill across the whole 640 reaches the frame's edges (the fades, the pause's darkening). The frame's
  own passes (BeginFrame's clear, the previous frame's blend, the night's Darken) cover the whole frame.
- **Anchors.** `gfx::AnchorScope(d, Anchor::Left / Right)` moves what's drawn in it to the frame's left or right
  edge instead (origin 0 or twice the margin); `Left()`/`Right()` are the frame's edges in the game's x under the
  anchor. With HUD Centered every anchor is the centre's.
  - Race (`hud/race_hud.cpp`): TOTALTIME, the places, the map and REPLAY to the left; LAPTIME and its rows and the
    meters to the right; the start lights and banners in the middle. The same in 2 Player's halves and the events.
  - The canvas's elements, Soccer's score and the games' pieces (Curling's board, Ski Jumping's distance, Fishing's
    count, Golf's panel) go with the third of the screen their x is in (`Hud_AnchorAt`; each piece by its centre,
    and the games' groups never straddle two thirds).
  - Field: the map to the left, the meter and clock to the right, Q's Rally's DESTINATION left and TIME right.
- **Tiled backgrounds** go on to the frame's edges: the course select's clouds, the 2 Player results' checker,
  Figure 8's and Golf's menus.
- **Pillarbox.** A screen made of a 4:3 picture calls `Draw2D::Pillarbox()` for the frame: no widening, every anchor
  the centre's, and EndFrame paints the sides black. The title picture and the main menu with Options (and the
  Display screen over it), the field menu's screens (their grounds are laid out for 640), the rooms and Roulette's
  board. Every frame starts without it.

Checked by eye at 16:9 and 21:9 (`--aspect`): a race and its finish, the field, 2 Player's split screen, the
course select and its pre-race screen, the title, Options and the Display screen with Apply, the field menu, a
room, New Game's name entry.

Unchecked: the events' and minigames' own screens beyond the HUD pieces (Roulette's board is pillarboxed unseen; the
result screens draw over 4:3 pictures and may want Pillarbox too), the ending, the garage, the paint and parts screens inside the rooms
(pillarboxed with the room), the photo screens. Full Screen and Borderless were tested only on the development VM.
