RTAGlass Manual
Last updated 10/30/2026
=====================================================

RTAGlass.exe is a program that lets you go on a Road Trip on Windows. 
This is built off the decompiled PS2 code, but this is **NOT** a static "recomp" or a wonky repackaged build of PCSX2, it's a genuine PC port.

This port is not affiliated with or endorsed by Takara Tomy or Conspiracy
Entertainment. Road Trip and Choro Q are their trademarks. 


How to play
---------------
1. Unpack the folder anywhere you can write to, e.g. your Desktop or C:\Games\RTAGlass. Avoid
   "Program Files": the game saves next to RTAGlass.exe and Program Files usually causes headaches.
2. Keep the "data" folder next to RTAGlass.exe.
3. Run RTAGlass.exe.

Requirements
  - Windows XP SP2 or later (x86 or x64). Should work on Linux with WINE/Proton, but this is untested.
  - A CPU with x87 instruction support.
  - DirectX9.0 compliant hardware, including a GPU with full Shader Model 2.0 support 
    (GeForce FX, a DirectX9.0 **compliant** ATI Radeon card or any AMD graphics card, Intel GMA 900 or later).
  - The DirectX End-User Runtime from June 2010.


Features
------------------
Everything from the original PS2 version like:
  - Adventure: The full Road Trip story from New Game to the credits. Every town and building, every
    talk, shop, race and minigame, the World Grand Prix, Cloud Hill, My City, My Garage, all 100 stamps,
    100 photos, ChoroQ coins, gems, mail, car radio stations and the day and night cycle is accounted for.
  - Quick Race: All the original courses, with records kept for the session.
  - 2 Player: Race Right-Away, Random Race and Custom Race in split screen, with the five events (Highway
    Race, Tunnel Race, Sliding Door Race, Obstacle Course and Soccer), and Results on the title menu.
  - Options: vibration, stereo or mono audio, sound volume, and Controls.
  - The attract mode will play if you idle on the title screen.
+ New exclusive features tailored for the PC:
  - Extras menu with leftover debug features, unused content and more.
  - New graphics settings to enhance the game's visuals beyond what the original PS2 could provide + widescreen resolution support 
  - In-depth user-definable Controls.
  - Mod support. Load custom music, replace textures, custom cars... the sky's the limit!


Controls
--------
  The defaults:

  Action            Keyboard        XInput (Xbox) controller
  ---------------   -------------   ---------------------
  Menus: move       Arrow keys      D-pad/Left Analog Stick
  Menus: confirm    Z               A
  Menus: back       C               Y
  Start / Pause     Enter           Start
  Steer             Left / Right    D-pad/Left Analog Stick
  Gas / brake       Z / A           A / X
  Reverse           X               B
  Jet fire          3               RT/Right Trigger
  Change view       C               Y
  Horn & lights     E               RB/Right Shoulder
  Wing up / down    Q / 1           LB / LT (Left Bumper/Left Trigger)
  Navigator (map)   Tab             Back

  Of course, you're free to rebind it as you like. This port was made by someone who plays video games and not by people trying to farm clout on X.com. 
  Each action has a key and a controller button: choose one with Left/Right, press Confirm,
  then the new key or button (Esc cancels, Delete clears). Left/Right on the top row switches between
  Player 1's and Player 2's driving and menu pages. Apply to save your changes. Your settings are in controls.json next to
  RTAGlass.exe. If you want to restore the defaults, delete controls.json.
  
  Extras (Title screen: Options > Extras; while in Adventure: Start > Settings > Extras) turns on what the
  original game has on the disc but aren't accessible, and the developers' debug features. They're
  all off by default, like the original game. Any changed settings are stored in extras.json next to
  RTAGlass.exe.
  - Unused Content: The Life Meter in My City's Parts Shop (after the Which-way? Race), Q-Wave (a third
    radio station), Wonder Realty's Endurance Run mail, body 149 ("Q Remote") in My City's Body Shop,
    the DEMO song on the title's demo, the Cloud Hill duck's "try again?" line (give up on his second or
    third question, then come back), a Language item in the title's Options (French or German
    title menus, for that run), and the Test Course: an unused track on the disc, a tube you race alone
    against the clock in Quick Race (The start, laps, map, song and AI path are taken from Peach Raceway so it can be played properly).
  - Debug: the Developer test menu (a third line under Adventure: any town, race, event, book, ending
    or building, with everything owned; it doesn't save unless you do), pause menu items (hide the map
    and meter, fill the tank, let the computer drive, and the AI's zone table, which Navigator saves to
    data\ai\Cnn.speeds.txt: delete it for the original), the car readouts, a free camera on controller 2
    (its d-pad, shoulder buttons, Y and X zoom, right sticks), a course map tuner on controller 2, ride
    height on Up/Down, and L1/L2 (Wing Up/Down) scrubbing the clock on the Endurance Run and in the field.
    Start > Settings > Other adjusts Vibration, Speaker and Sound Volume.

  Display (title screen: Options > Display; while in Adventure: Start > Settings > Display) lets you adjust:
  - Display: Window, Full Screen or Borderless Fullscreen. Alt+Enter switches
    between Window and Borderless at any time.
  - Resizable: When it's on, you can drag the window's edges to any size or maximize it.
  - Resolution: The window's size or the screen mode. Auto is the largest 4:3 window that fits, or the
    desktop's size.
  - Aspect Ratio: 4:3 is the original aspect ratio from the PS2. 16:10, 16:9 and 21:9 (or Auto, the window's or screen's
    shape) let you play in widescreen resolutions. Screens made of 4:3 pictures (title screen,
    rooms, the pause menu's pages) are pillarboxed when you play with widescreen aspect ratios.
  - HUD: Edges adjusts your HUD to fit your current aspect ratio. Centered keeps HUD elements in the 4:3 space.
  - Render Scale: how sharp the picture is drawn (in multiples of 0.5x-4x of the PS2's 640x448). Auto fits the screen;
    Go above 1.0x for a clearer image, or drop it down to 0.75x-0.5x if your performance is slow.
  - VSync: On forces your GPU to wait for the screen's refresh before displaying the next frame (no tearing); 
    Off will let your GPU render a frame the second it's ready (tears, but can reduce input lag).
  - Frame Limiter: On caps the game to the original 60FPS. Off will make the game run as fast as your PC can, and
    game logic is not currently compensated for this so it'll play too fast; With VSync on it'll run at your screen's refresh rate.
  - Graphics: Full is identical to the PS2 version. Low is if your PC is struggling to run the game: 
    On Low, the 3D view fades out sooner, soft motion trails and shiny floor reflections are disabled, and cars LOD is more aggressive.
  Apply keeps your settings in display.json next to RTAGlass.exe; delete it to reset your Display settings back to defaults.

  **WARNING: ESC QUITS AT ONCE WITHOUT ASKING, SO MAKE SURE TO SAVE FIRST!**

  Player 1 is assigned to the keyboard and first controller, Player 2 is assigned to the second controller, as well as any keys 
  you assign to Player 2. Any plugged in controller will vibrate unless Options > Vibration is Off.


Saves
-----
The Adventure and 2 Player save to save1.json and save2.json next to RTAGlass.exe. These correspond to the two memory
card slots on the PS2. Quick Race does not save anything. Back your files up to keep your progress.

RTAGlass includes a helpful feature that will let you convert your original PS2 saves (the BASLUS-20398 file on your memory card) 
to RTAGlass's save format. You can convert your save from the RTAGlass format to the original game's format from the command prompt with:
    RTAGlass.exe --save-convert BASLUS-20398 save1.json (PS2 to PC)
    RTAGlass.exe --save-convert save1.json BASLUS-20398 (PC to PS2


Troubleshooting
---------------
  "Game data not found": The "data" folder must be next to RTAGlass.exe, with its subfolders.
  "Failed to initialize Direct3D 9": Your graphics card or driver lacks Shader Model 2.0 support. You may need to update your drivers.
  My audio/controller doesn't work: You have to install the DirectX End-User Runtime. If you don't already have it from another application,
  get it from the web installer at https://www.microsoft.com/en-us/download/details.aspx?id=35. Your controller must be XInput-compatible to work with RTAGlass.
  At the time of writing, DirectInput-only devices (like Sidewinder Freestyle, Precision Pro joysticks) are unsupported currently.
  The game won't save: Move the folder out of "Program Files" or any other read-only place where RTAGlass won't be able to write files to.
  Window size: By default, the window is the largest multiple of 640x480 that fits your screen, at most
    3x. Options > Display changes it; --scale N will force it to a specified scale (e.g. RTAGlass.exe --scale 2).
  Fullscreen mode isn't showing: Delete your display.json, or start with --scale 1 and pick another.
  Game is lagging: Try Options > Display > Graphics Low, and a lower Render Scale. If that doesn't help, 
  open Task Manager with Ctrl+Shift+Esc or however, and kill any applications fighting for CPU/GPU usage.
  RTAGlass will not run on my Athlon XP/Pentium 3, your port sucks!: You built the SSE2 version meant for Pentium 4 and newer. 
  Follow the build script instructions and choose the correct version, either x87 or SSE for your processor.
  My DirectX9.0 compatible ATI Radeon card gives "Failed to initialize Direct3D 9": 
  DirectX9.0 "compatible" ATI cards like the Radeon 9200 do not support Shader Model 2.0, so they won't work on RTAGlass. Radeon 9500 and above is OK.

About modding the game
-------
Everything RTAGlass loads is in "data":
  data\textures.rpk   every texture, packed into one file. To mod textures, place your texture in data\textures. It will replace the
                  packed one of the same name. To get the textures out, run this from a command prompt in this folder:
                      tools\rpak.exe unpack data\textures.rpk data\textures
                  They are uncompressed 32-bit DDS (A8R8G8B8), with a .txt list for each game file. Edit or
                  replace one with an image of the same format and aspect ratio, and delete the ones you
                  didn't change (or keep them all: the loose copies are used either way). To pack your
                  edits back in instead: tools\rpak.exe pack data\textures data\textures.rpk
                  (Backup a copy of the original first).
  data\models.rpk     3D models as OBJ/MTL with vertex colours (you can import them into Blender), packed the same
                  way: tools\rpak.exe unpack data\models.rpk data\models gets them out, and a loose
                  file in data\models replaces the packed one.
  data\collision.rpk  the ground and walls the cars drive on (.pmp), packed the same way.
  data\sound      sound banks as SoundFont 2 (.sf2), music and effects as MIDI (.mid). The radio's two
                  stations are in data\sound\radio as 1CH.adp (Peach FM) and 3CH.adp (E-Radio), in the
                  PS2's PS-ADPCM format. An hour is a whole in-game day; a shorter 
                  station will loop. To replace one, either put a 16-bit WAV named 1CH.wav or 3CH.wav next
                  to it, or convert your WAV to the game's format:
                      tools\adpconv.exe encode mystation.wav data\sound\radio\1CH.adp
                  (Backup a copy of the original first). To get a station out as a WAV:
                      tools\adpconv.exe decode data\sound\radio\1CH.adp peachfm.wav
                  Any rate works; the original files are 12000 Hz stereo.
  data\story      messages.txt: The dialogue of everybody you meet in the Adventure. You can edit the text 
                  but keep the control codes (\xHH).
  data\ai         the opponents' racing lines and speeds, as text.
  data\camera     the trackside replay cameras, as text.


Debug options
-------------
These are for testing and/or debugging. Run them from a command prompt in this folder or create a shortcut with these in the shortcut's target.
Options marked (factory) need --factory, and start a new save that isn't written unless you save.
  --scale N                  Window size: N times 640x480 (wider with --aspect), drawn at N times (0.5, 0.75 too).
  --aspect A                 Force aspect ratio: auto, 4:3, 16:10, 16:9 or 21:9.
  --hud centered             Forces your HUD to be within 4:3 safezones.
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
  --autodrive N              The AI will drive your car, held to N x 2.5 km/h (set it to 0 for Big Rigs:Over the Road Trip racing).
  --ranking                  The pause menu in Quick Races will now have the Ranking option.
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
                             what it ran on (Windows OS version + your CPU and GPU). Useful for benchmarking.
  --cartrace FILE            Write your car's physics each race frame to FILE (CSV); --tracecar N
                             traces opponent N instead.
  --keylog FILE              Write every key message the window gets to FILE.
  --soundtest ID[,ID] SECONDS OUT.wav
                             Render sound requests to a WAV file without a window.
Credits
-------------
RTAGlass - Developer, documentation, build scripts.
lifesabeach - README.MD, README.txt.
Special thanks to the PCSX2 developers.
