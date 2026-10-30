RTAGlass - Road Trip on Windows
Last modified: 10/30/2026
=====================================================

RTAGlass is a program that brings Everywhere Road Trip for the PS2, on your Windows PC.
RTAGlass was built from a decomp of the original code. This is a native port, NOT a "recomp".

This is an unofficial fan project. It is not affiliated with or endorsed by Takara Tomy or Conspiracy
Entertainment. Road Trip and Choro Q are their trademarks.


Getting started
---------------
1. Unpack the folder anywhere you can write to, e.g. your Desktop or C:\Games\RTAGlass. Avoid
   "Program Files": the game saves next to RTAGlass.exe and Program Files usually won't play nice, especially on modern Windows.
2. Keep the "data" folder next to RTAGlass.exe.
3. Run RTAGlass.exe.

Requirements
  - Windows XP SP2 or later (32-bit or 64-bit). Should work in WINE/CrossOver too, but that's not verified.
  - A CPU with a FPU (tested on a Pentium 4 with the SSE2 build script, but any SSE/x87 CPU will work).
  - A Direct3D 9 graphics card that supports Shader Model 2.0 (tested on Intel HD Graphics Nehalem and GeForce 9400M, but in theory it should be fine on GeForce FX, 
    any DirectX9 compliant ATI/AMD GPU, and Intel GMA 900 or later).
  - Sound and controller requires the DirectX End-User Runtime (June 2010 or the web installer), which provides both XAudio2 2.7 and XInput.


Features
------------------
Everything from the PS2 version:
  - Adventure: the full story from New Game to the ending and staff roll. Every town and building, every
    talk, shop, race and minigame, the World Grand Prix, Cloud Hill, My City, My Garage, the 100 collectable stamps,
    the 100 collectable photos, as well as all the ChoroQ coins, gems, mail, car radio stations and the day and night cycle.
  - Quick Race: every course, including record-keeping.
  - 2 Player: Race Right-Away, Random Race and Custom Race in split screen, with the five events (Highway
    Race, Tunnel Race, Sliding Door Race, Obstacle Course and Soccer), and Results on the title menu.
  - New Options menu tailored for the PC, including (but not limited to) vibration settings for XInput-compatible controllers, stereo or mono sound, sound volume, and a new Controls menu.
  - The attract mode will play if you leave the game on the title screen.


Controls
--------
  Defaults:

  Action            Keyboard        Xbox-style controller
  ---------------   -------------   ---------------------
  Menus: move       Arrow keys      D-pad (Left stick moves left and right too)
  Menus: confirm    Z               A
  Menus: back       C               Y
  Start / pause     Enter           Start
  Steer             Left / Right    D-pad (Left stick for analog steering)
  Gas / brake       Z / A           A / X
  Reverse           X               B
  Jet fire          3               Right trigger
  Change view       C               Y
  Horn & lights     E               Right shoulder
  Wing up / down    Q / 1           Left shoulder / left trigger
  Navigator (map)   Tab             Back

  Of course, you can rebind them as you wish. We actually play video games here instead of just farming ports for clout on X.com. 
  Each action has a key and a controller button: choose one with Left/Right, press Confirm,
  then the new key or button (Esc cancels, Delete clears). Left/Right on the top row switches between
  Player 1's and Player 2's driving and menu pages. Apply will keep them, in controls.json next to
  RTAGlass.exe; deleting them restores the defaults.

  Extras (title screen: Options > Extras; in the Adventure: Start > Settings > Extras) turns on what the
  original game has leftover on the disc but never lets you reach, and its developers' debug features. They're
  all off by default, which is how the game shipped. Apply keeps them in extras.json next to
  RTAGlass.exe.
  - Unused Content: the Life Meter in My City's Parts Shop (after the Which-way? Race), Q-Wave (a third
    radio station), Wonder Realty's Endurance Run mail, body 149 ("Q Remote") in My City's Body Shop,
    the DEMO song on the title's demo, the Cloud Hill duck's "try again?" line (give up on his second or
    third question, then come back), and a Language item in the title's Options (French or German
    title menus, for that run), and the Test Course: an unused track on the disc, a tube you race alone
    against the clock in Quick Race (this port makes it use the track logic from Peach Raceway so it's playable).
  - Debug: The developers' test menu (a third line under Adventure: any town, race, event, book, ending
    or building, with everything owned; it doesn't save unless you do), pause menu items (hide the map
    and meter, fill the tank, let the computer drive, and the AI's zone table, which Navigator saves to
    data\ai\Cnn.speeds.txt: delete it for the original), the car readouts, a freecam on controller 2
    (its d-pad, shoulder buttons, Y and X zoom, right sticks), a course map tuner on controller 2, ride
    height on Up/Down, and L1/L2 (Wing Up/Down) scrubbing the clock on the Endurance Run and in the field.
  Start > Settings > Other has the title's Vibration, Speaker and Sound Volume.

  Display (title screen: Options > Display; in the Adventure: Start > Settings > Display) sets:
  - Display: Window, Full Screen or Borderless Fullscreen, Alt+Enter switches
    between Window and Borderless at any time.
  - Resizable: When it's on, it lets you drag the window's edges to any size or maximize it; the size is kept.
  - Resolution: the window's size or the screen mode. Auto is the largest 4:3 window that fits, or the
    desktop's size.
  - Aspect Ratio: 4:3 is the PS2's picture. 16:10, 16:9 and 21:9 (or Auto, the window's or screen's
    shape) show more of the road and the town to the sides;  but screens made of 4:3 pictures (the title,
    the rooms, the pause menu's pages) are pillarboxed.
  - HUD: Edges adjusts the HUD for your selected aspect ratio; Centered puts it where it would be in 4:3.
  - Render Scale: Scales the pixel count of the image, from 0.5x to 4x of the PS2's 640x448. Auto fits the screen 
    and will make the game look clearer than the PS2 original; while 0.5x-0.75x will make it run better on weaker hardware.
  - VSync: On waits for the screen's refresh; Off shows each picture at once. 
  - Frame Limiter: On caps the game to 60FPS. Off runs it as fast as your PC can, and
    the game itself speeds up (like turbo on PCSX2); with VSync on it'll run at your monitor's refresh rate.
  - Graphics: Full is the original PS2 look. Low is for potatos: the 3D view fades out sooner, the soft
    motion trail and the cars' reflections on shiny floors are disabled, and cars LOD is more aggressive.
  Apply keeps them in display.json next to RTAGlass.exe; deleting them restores the defaults.

  Esc quits at once, without asking, so save first.

  Player 1 uses the keyboard and/or the first controller, player 2 uses the second controller (and any keys you
  give player 2). The controller vibrates as the PS2 pad did unless Options > Vibration is Off.


Saves
-----
The Adventure and 2 Player modes save to save1.json and save2.json next to RTAGlass.exe. These are the two memory
card slots. Quick Race, like the PS2 version, does not save. Back these files up to keep your progress.

RTAGlass includes a helpful feature to migrate your PS2 saves (the BASLUS-20398 file on your PS2 memory card). 
It will happily convert your save both ways from a command prompt:
    RTAGlass.exe --save-convert BASLUS-20398 save1.json
    RTAGlass.exe --save-convert save1.json BASLUS-20398


Troubleshooting
---------------
  "Game data not found": the "data" folder has to be next to RTAGlass.exe, with its subfolders.
  "Failed to initialize Direct3D 9": the graphics card or driver lacks Shader Model 2.0. You may need to update your drivers on certain cards.
  No sound: You didn't install the correct DirectX End-User Runtime, get it at https://www.microsoft.com/en-us/download/details.aspx?id=35.
  The game doesn't save: Move the game folder out of "Program Files" or any other read-only place.
  Window size: by default the window is the largest multiple of 640x480 that fits your screen, at most
    3x. Options > Display changes it; --scale N picks it for one run (e.g. RTAGlass.exe --scale 2).
  Fullscreen results in a black screen: Delete display.json to reset your display settings, or start with --scale 1 and pick another.
  Towns/Races are laggy: try Options > Display > Graphics Low, and a lower Render Scale.
  Controller fails to be recognized: See "No sound" if your controller is XInput-compatible. DirectInput devices are not supported as of the writing of this readme

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
                  at 44100 Hz is about 180 MB). 
  data\story      messages.txt: The dialogue that is used in Adventure mode. Edit the text 
                  but don't touch the control codes (\xHH).
  data\ai         the opponents' racing lines and speeds, as text.
  data\camera     the trackside replay cameras, as text.


Debug options
-------------
These are for testing. Run them from a command prompt in this folder, or create a shortcut. 
Options marked (factory) need --factory, and start a new save that isn't written unless you save.
Some of these are leftover from testing and probably aren't very useful, but they're documented regardless.
  --scale N                  Force Window size: N times 640x480 (wider with --aspect), drawn at N times.
  --aspect A                 Force aspect ratio: auto, 4:3, 16:10, 16:9 or 21:9.
  --hud centered             Force 4:3 HUD.
  --race ID                  Jumps straight into a Quick Race on course ID (0 = Peach Raceway).
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
  --clock N                  Start at specified time of day N (frames, 9000 an hour; 108000 is midday).
  --place X Y Z YAW          Start your car at a point of the world (world units, 16-bit angle).
  --eye X Y Z YAW PITCH      Watch a race from a fixed point.
  --gate N                   Obstacle Course: start on gate N. Highway Race: start in AI zone N.
  --autodrive N              The AI will drive your car, held to N x 2.5 km/h (0 if you want to make the AI play Big Rigs: Over the RoadTrip Racing).
  --ranking                  The pause menu will now have the Ranking in a Quick Race too.
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
                             what it ran on (Windows version, CPU, GPU). Useful for benchmarking.
  --cartrace FILE            Write your car's physics each race frame to FILE (CSV); --tracecar N
                             traces the physics of opponent N instead.
  --keylog FILE              Write every key message the window gets to FILE.
  --soundtest ID[,ID] SECONDS OUT.wav
                             Render sound requests to a WAV file without a window.
