RTAGlass - a Windows port of Road Trip (PlayStation 2)
=====================================================

RTAGlass is a fan-made native Windows version of Road Trip (Takara / Conspiracy Entertainment, 2002; Choro Q HG 2
in Japan). The game was rebuilt from the original's code, so it isn't an emulator. It plays the same, and
runs on anything from Windows XP up.

This is an unofficial fan project. It is not affiliated with or endorsed by Takara Tomy or Conspiracy
Entertainment. Road Trip and Choro Q are their trademarks.


Getting started
---------------
1. Unpack the folder anywhere you can write to, e.g. your Desktop or C:\Games\RTAGlass. Avoid
   "Program Files": the game saves next to RTAGlass.exe.
2. Keep the "data" folder next to RTAGlass.exe.
3. Run RTAGlass.exe.

Requirements
  - Windows XP SP2 or later (32-bit or 64-bit).
  - A CPU with SSE2 (Pentium 4 / Athlon 64 or later).
  - A Direct3D 9 graphics card with Shader Model 2.0 (GeForce FX, Radeon 9500, Intel GMA 900 or later).
  - For sound: the DirectX End-User Runtime (June 2010 or the web installer), which provides XAudio2 2.7.
    Windows 10 and 11 don't include it. Without it the game runs silently.
    https://www.microsoft.com/en-us/download/details.aspx?id=35


What's in the game
------------------
Everything from the PS2 version:
  - Adventure: the full story from New Game to the ending and staff roll. Every town and building, every
    talk, shop, race and minigame, the World Grand Prix, Cloud Hill, My City, My Garage, the 100 stamps,
    the 100 photos, the ChoroQ coins, the gems, the mail, the car radio and the day and night.
  - Quick Race: every course, with records kept for the session.
  - 2 Player: Race Right-Away, Random Race and Custom Race in split screen, with the five events (Highway
    Race, Tunnel Race, Sliding Door Race, Obstacle Course and Soccer), and Results on the title menu.
  - Options: vibration, stereo or mono, sound volume, and Controls (your own keys and buttons).
  - Left alone on the title screen, the game plays its demo.


Controls
--------
  The defaults:

  Action            Keyboard        Xbox-style controller
  ---------------   -------------   ---------------------
  Menus: move       Arrow keys      D-pad (the left stick moves left and right too)
  Menus: confirm    Z               A
  Menus: back       C               Y
  Start / pause     Enter           Start
  Steer             Left / Right    D-pad (the left stick steers too)
  Gas / brake       Z / A           A / X
  Reverse           X               B
  Jet fire          3               Right trigger
  Change view       C               Y
  Horn & lights     E               Right shoulder
  Wing up / down    Q / 1           Left shoulder / left trigger
  Navigator (map)   Tab             Back

  Change them in Controls (title screen: Options > Controls; in the Adventure: Start > Settings >
  Controls). Each action has a key and a controller button: choose one with Left/Right, press Confirm,
  then the new key or button (Esc cancels, Delete clears). Left/Right on the top row switches between
  player 1's and player 2's driving and menu pages. Apply keeps them, in controls.json next to
  RTAGlass.exe; delete that file to get the defaults back.

  Extras (title screen: Options > Extras; in the Adventure: Start > Settings > Extras) turns on what the
  original game has on the disc but never lets you reach, and its developers' debug features. They're
  all off by default, which is the game as it shipped. Apply keeps them in extras.json next to
  RTAGlass.exe.
  - Unused Content: the Life Meter in My City's Parts Shop (after the Which-way? Race), Q-Wave (a third
    radio station), Wonder Realty's Endurance Run mail, body 149 ("Q Remote") in My City's Body Shop,
    the DEMO song on the title's demo, the Cloud Hill duck's "try again?" line (give up on his second or
    third question, then come back), and a Language item in the title's Options (French or German
    title menus, for that run), and the Test Course: an unused track on the disc, a tube you race alone
    against the clock in Quick Race (its start, laps, map, song and AI path are the PC's own).
  - Debug: the developers' test menu (a third line under Adventure: any town, race, event, book, ending
    or building, with everything owned; it doesn't save unless you do), pause menu items (hide the map
    and meter, fill the tank, let the computer drive, and the AI's zone table, which Navigator saves to
    data\ai\Cnn.speeds.txt: delete it for the original), the car readouts, a free camera on controller 2
    (its d-pad, shoulder buttons, Y and X zoom, right sticks), a course map tuner on controller 2, ride
    height on Up/Down, and L1/L2 (Wing Up/Down) scrubbing the clock on the Endurance Run and in the field.
  Start > Settings > Other has the title's Vibration, Speaker and Sound Volume.

  Display (title screen: Options > Display; in the Adventure: Start > Settings > Display) sets:
  - Display: Window, Full Screen or Borderless (a window over the whole desktop). Alt+Enter switches
    between Window and Borderless at any time.
  - Resizable: On lets you drag the window's edges to any size or maximize it; the size is kept.
  - Resolution: the window's size or the screen mode. Auto is the largest 4:3 window that fits, or the
    desktop's size.
  - Aspect Ratio: 4:3 is the PS2's picture. 16:10, 16:9 and 21:9 (or Auto, the window's or screen's
    shape) show more of the road and the town to the sides; screens made of 4:3 pictures (the title,
    the rooms, the pause menu's pages) keep to the middle with black at the sides.
  - HUD: Edges puts the race and field HUD's corners at a wide picture's edges; Centered keeps them in
    the 4:3 middle.
  - Render Scale: how sharp the picture is drawn (0.5x-4x of the PS2's 640x448). Auto fits the screen;
    lower it on a slow machine (0.5x and 0.75x draw fewer pixels than the PS2 did, blurrier but faster).
  - VSync: On waits for the screen's refresh (no tearing); Off shows each picture at once.
  - Frame Limiter: On holds the game to its 60 frames a second. Off runs it as fast as your PC can, and
    the game itself speeds up (like an emulator's turbo); with VSync on it runs at your screen's refresh.
  - Graphics: Full is the PS2's picture. Low is for slow PCs: the 3D view fades out sooner, the soft
    motion trail and the cars' reflections on shiny floors are left out, and far cars are simpler.
  Apply keeps them in display.json next to RTAGlass.exe; delete that file to get the defaults back.

  Esc quits at once, without asking, so save first.

  Player 1 uses the keyboard and the first controller, player 2 the second controller (and any keys you
  give player 2). The controller vibrates as the PS2 pad did unless Options > Vibration is Off.


Saves
-----
The Adventure and 2 Player save to save1.json and save2.json next to RTAGlass.exe. These are the two memory
card slots. The Quick Race doesn't save, as on the PS2. Back these files up to keep your progress.

A save copied off a PS2 memory card (the BASLUS-20398 file) converts both ways from a command prompt:
    RTAGlass.exe --save-convert BASLUS-20398 save1.json
    RTAGlass.exe --save-convert save1.json BASLUS-20398


Troubleshooting
---------------
  "Game data not found": the "data" folder has to be next to RTAGlass.exe, with its subfolders.
  "Failed to initialize Direct3D 9": the graphics card or driver lacks Shader Model 2.0. Update the driver.
  No sound: install the DirectX End-User Runtime (above).
  The game doesn't save: move the folder out of "Program Files" or any other read-only place.
  Window size: by default the window is the largest multiple of 640x480 that fits your screen, at most
    3x. Options > Display changes it; --scale N picks it for one run (e.g. RTAGlass.exe --scale 2).
  A full screen mode that doesn't show: delete display.json, or start with --scale 1 and pick another.
  Slow in the towns or races: try Options > Display > Graphics Low, and a lower Render Scale.


Modding
-------
Everything the game loads is in "data" in ordinary formats:
  data\textures.rpk   every texture, packed into one file. A loose file in data\textures replaces the
                  packed one of the same name, so a mod is just the files it changes. To get the textures
                  out, run this from a command prompt in this folder:
                      tools\rpak.exe unpack data\textures.rpk data\textures
                  They are uncompressed 32-bit DDS (A8R8G8B8), with a .txt list for each game file. Edit or
                  replace one with an image of the same format and aspect ratio, and delete the ones you
                  didn't change (or keep them all: the loose copies are used either way). To pack your
                  edits back in instead: tools\rpak.exe pack data\textures data\textures.rpk
                  (keep a copy of the original first).
  data\models.rpk     3D models as OBJ/MTL with vertex colours (Blender opens them), packed the same
                  way: tools\rpak.exe unpack data\models.rpk data\models gets them out, and a loose
                  file in data\models replaces the packed one.
  data\collision.rpk  the ground and walls the cars drive on (.pmp), packed the same way.
  data\sound      sound banks as SoundFont 2 (.sf2), music and effects as MIDI (.mid). The radio's two
                  stations are in data\sound\radio as 1CH.adp (Peach FM) and 3CH.adp (E-Radio), in the
                  PS2's own compressed format (PS-ADPCM). An hour is a whole in-game day; a shorter
                  station repeats. To replace one, either put a 16-bit WAV named 1CH.wav or 3CH.wav next
                  to it, or convert your WAV to the game's format, about a quarter of the size:
                      tools\adpconv.exe encode mystation.wav data\sound\radio\1CH.adp
                  (keep a copy of the original first). To get a station out as a WAV:
                      tools\adpconv.exe decode data\sound\radio\1CH.adp peachfm.wav
                  Any rate works; the originals are 12000 Hz stereo, which keeps the file small (an hour
                  at 44100 Hz is about 180 MB). The format loses a little, as the PS2's did.
  data\story      messages.txt: what everyone says in the Adventure. Edit the text but keep the control
                  codes (\xHH).
  data\ai         the opponents' racing lines and speeds, as text.
  data\camera     the trackside replay cameras, as text.


Debug options
-------------
These are for testing. Run them from a command prompt in this folder. Options marked (factory) need --factory,
and start a new save that isn't written unless you save.
  --scale N                  Window size: N times 640x480 (wider with --aspect), drawn at N times (0.5, 0.75 too).
  --aspect A                 Aspect ratio for this run: auto, 4:3, 16:10, 16:9 or 21:9.
  --hud centered             The HUD in the 4:3 middle for this run.
  --race ID                  Straight into a Quick Race on course ID (0 = Peach Raceway).
  --factory                  Straight to Q's Factory with a new save, as after the opening.
  --advrace N                (factory) Straight into Adventure race N (0-27, the race lists' rows).
  --field N                  (factory) Straight out onto the roads from town N (1 Peach Town ... 9 My City).
  --room T P                 (factory) Straight into building P of town T (0 0: My Garage).
  --ending N                 (factory) The ending: 0 the whole of it, 1 My City Cinemas' replay.
  --allparts                 (factory) Five of every part, eight bodies, the Super A license, two teammates.
  --coinradar, --beaten, --won, --mycity HEX, --stamp N[,N], --flag N[,N], --item N[,N], --gpstage N
                             (factory) Give the Coin Radar, beat every driver, win every town race, move
                             in My City's residents, earn stamps, set story flags, give items, finish
                             Grand Prix stages.
  --meter N                  Your car has meter N (0-10).
  --partflags HEX            Your car has part flags HEX (e.g. 3004: the Flight Wing, fins and jets).
  --clock N                  Start at time of day N (frames, 9000 an hour; 108000 is midday).
  --place X Y Z YAW          Start your car at a point of the world (world units, 16-bit angle).
  --eye X Y Z YAW PITCH      Watch a race from a fixed point.
  --gate N                   Obstacle Course: start on gate N. Highway Race: start in AI zone N.
  --autodrive N              The computer drives your car, held to N x 2.5 km/h (0: no limit).
  --ranking                  The pause menu has the Ranking in a Quick Race too.
  --extra NAME[,NAME]        Turn on extras for this run (lifeMeter, qWave, mail5, testCourse,
                             body149, demoSong, duckQuiz, language, pauseItems, readouts, freeCamera,
                             heightKeys, clockScrub, testMenu, mapTuner), as Options > Extras does.
  --shot N[,N] PREFIX        Save frame N as PREFIX_N.png, then quit after the last one.
  --press N[:K[:M]][,...]    Press button K on frame N (s Start, x Cross, o Circle, t Triangle, q Square,
                             j R2, h R1, w L1, v L2, m Select, u/d/l/r the d-pad; 2 before K for pad 2),
                             held M frames.
  --wav FILE                 With --shot: record the sound to a WAV file.
  --perf FILE                Write each frame's cost to FILE (CSV): the time it took and where it
                             went, the draw calls, and where you were. Its first lines say
                             what it ran on (Windows, the processor, the graphics card).
  --cartrace FILE            Write your car's physics each race frame to FILE (CSV); --tracecar N
                             traces opponent N instead.
  --keylog FILE              Write every key message the window gets to FILE.
  --soundtest ID[,ID] SECONDS OUT.wav
                             Render sound requests to a WAV file without a window.
