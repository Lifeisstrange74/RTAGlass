# The game's leftover debug features

The retail game still has code for a developers' debug mode. None of it can be reached in normal play, but it shows
what the developers used to build the races, and it's a model for a debug mode in the port. Everything here is read from
the decomp; what was checked on the PS2 says so.

Two switches turn it on:

- **The debug byte**, pad +0x0B (0x0033590B in the shared task state at 0x00335900; Main_Loop passes it to every
  task). It's 0 in retail, and nothing in the retail code sets it. It turns on the readouts, the car height keys, the
  free camera and more (below).
- **The pause menu's extra items** (Race_PauseTask, 0x0026BC08). The code handles items 3-6, but the retail window
  only has 2 or 3 lines, so they can't be chosen. A debug build must have added lines for them. Their text isn't in
  the retail executable.

## The pause menu's debug items

| Item | Effect | Sound |
|---|---|---|
| 3 | Toggles race flag 0x10000: the course map and the speedometer are hidden (the replay uses the same flag) | 0x1A |
| 4 | Fills the player's tank: car +0x23C (0x0177AE8C) = 0x40000. Then line 4 gets kLineDisabled (window +0x54 \|= 1), so it greys out: once per pause | 0x1A |
| 5 | Toggles car flag 0x80 on the player's car (+0x198). Car_Input then drives it with the AI (D_003356DC, Ai_Drive), as for the opponents and the attract demo. The port's `--autodrive 0` does the same | none |
| 6 | Opens the AI zone table editor (Ai_ZoneTableTask, func_0026D020, below) in place of the menu | none |

### The AI zone table editor (Ai_ZoneTableTask, 0x0026D020)

The AI's speed table for the course (ai.md, "Speeds") with the player's car as the cursor:

- **Ai_DrawZoneTable** (0x00250738) shows the zones in columns of 20 (x = 80 × (column & 7) + 32, y = 8 × row + 64
  half-lines, the debug font, colour 15): `"%c %3d %3d"`, the mark of the last pass (O clean, X steered, blank
  none), the zone's speed × 5 and the mark's speed × 5, for every zone whose speed isn't 0. A `>` at x = 80 ×
  column + 24 marks the zone the player's car is in (+0x24A); it isn't wrapped like the text, so from zone 160 it's
  off the screen. Only the current column and those either side are drawn (13 columns: the last reads 4 bytes past
  the 256-byte table).
- **Up / Down** (pad 1 pressed, +0x30) add 1 to or take 1 from that zone's table entry (car +0x258 is the table,
  +0x24A the zone; a byte, so 0 − 1 is 255). The table is shared: every car not on a team points at the same one.
- **Cross / Triangle** go back to the pause menu (task +0xC = Race_PauseTask; the window shows again the next frame);
  else **Select** saves the table to the development PC: **Ai_SaveZoneTable** (0x0020E5B8) opens `host:ai_g%02d.c`
  (the race number, info +0x20; mode 0x602) and writes one line per zone up to the last non-zero one,
  `"\t%d,\t/* zone %2d */\n"`, printing `AI Save %s` before, then `done` (or `File open error` instead;
  `write error!` for each failed line) on the console. The tables were written as C source and compiled into the game.
- Fade_SetOverlay(0x80006666) every frame: (0x66, 0x66, 0), opaque, over the race.

With item 5 (the AI drives your car) and the marks the AI leaves per zone (ai.md, "Marks"), this looks like the tool
the courses' speed tables were tuned with: let the AI drive, watch where it has to steer, adjust, save.

## The debug byte

| Where | With the byte set |
|---|---|
| Car_Update (0x0021BA70) | For the player-1 car (flags & 3 = 2), D_003356D0/D4 turn on readouts at x 300: `LAST` (last frame's velocity), `LACC` (local acceleration), `WSPD` (world velocity), `LSPD` (local velocity), each x y z and w |
| Car_Engine (0x00218EE0) | `FX` (the engine force and the slip), `TIRE` and `RPM` (+0x1BC, +0x1D0), `SQUEAL` |
| Car_Update, end | Debug_PrintCar (0x0026D700) for the player-1 car: `BLOCK` (its collision cell), `X:` `Y:` `Z:`, `ATR:` (the surface flags under it), `WARN:` |
| Car_Ground (0x0021B3D0) | For a player's car, the d-pad on its pad overrides the height (+0xA4): Down holds the car a unit (0x51EB) above where the suspension puts it, Up 0x1000 (0.2 units) below. Probably for checking the ground and clearances |
| Car_Input (0x0021A990) | The argument that lets an AI-driven car change its speed table (ai.md, "Marks") takes car flag bit 0 (player 2's pad) as well as the teammates' 0x30 |
| Ai_TrackZone | For players' cars: `ZONE n IN/OUT ...` as zones are passed; for player 1 `TARGET LINE`, `MYROT` (yaw), `DIST` (it prints the float 100.0's bits: a leftover bug), and `REVERSE` while +0x248 is set |
| func_002201C0 (course drawing) | Two cells, race state +0x60 and +0x64, at (400, 32) as `[abc]` (or `[---]`) |
| Camera_ChaseTask | **A free camera on pad 2** (Camera_DebugMove, 0x0021F198): d-pad Up/Down and Left/Right move it, L2/L1 move it along the third axis, the analog sticks turn it, Triangle and Square zoom (the focal length × or ÷ a factor, Square down to 500). func_0026D880 prints it. The chase camera's collision (func_0021E070) is off |
| Race_SetupTask, course 0x11 | L2 / L1 held (pad +0x2C bits 1 and 4) move the course's clock (+0x14, an hour of 0x34BC0 frames that wraps) by 0x168 frames (6 seconds) a frame, forward or back: scrubbing the time of day |
| Adventure_Task, state 0 (0x00210CF4) | The same L2 / L1 scrub of the field's clock, while race flag 0x8000 is clear (not in a talk, state 8) |
| RaceMap_Draw (0x002427D0), RaceMap_DrawStep (0x00242A88) | **A map tuner on pad 2**: held (+0x2E) Right/Left x ± 1, Up/Down y ± 1, R2 scale × 1.01162, R1 ÷ 1.01162; pressed (+0x32) L1 angle + π/2, L2 − π/2, of D_002A4050[course] (Figure 8: D_002A42E0[step]); then func_0026DE80 prints `X:%f Y:%f Z:%f W:%f` at (16, 208) |

All readouts are func_00203A50 (debug text, colour 15) on the 640×448 screen.

## Trying it on the PS2

Memory writes over PCSX2's PINE interface are enough:
- the debug byte: write 1 to 0x0033590B during a race;
- the pause items can't be reached without patching the window: Race_PauseTask's `Window_AddLine` calls, or the cursor
  limit (window +0x0B, the item count).

None of this has been tried on the PS2 yet.

## In the port

The Debug page of the PC's Extras screen (extras.md) turns these on one by one: the pause items 3-6 (with the zone
table editor, which saves to data/ai/*.speeds.txt), the readouts, the free camera, the ride height, the clock scrub
on the Endurance Run and in the field, and the map tuner. Read
again from the asm for that, which corrects this file in places (extras.md has the details):
- the Car_Update and Car_Engine readouts need car flags exactly 2 (player 1's car, driven from the pad, not in the
  field); the vectors print z, x, y, w;
- `DIST` is the distance x 100 converted to an integer (cvt.w.s), not a leftover bug;
- `ZONE` prints every frame for every player's car with LEFT / RIGHT / EITHER (the fork's side), not IN / OUT;
- the `[abc]` cells are func_002201C0's in the Adventure field (four loader slots, their places and the block
  origin), not a race's;
- `X:%f Y:%f Z:%f W:%f` belongs to a course-map tuner on pad 2 (RaceMap_Draw 0x002427D0, RaceMap_DrawStep
  0x00242A88: Right/Left and Up/Down move the map, R2/R1 scale it, L1/L2 turn it a quarter), not Debug_PrintCar;
- the field has the same L2/L1 clock scrub (Adventure_Task's state 0, while race flag 0x8000 is clear);
- Ai_SaveZoneTable's file is named by the race number (info +0x20), not the pad's.
The labels of pause items 3 and 4 are in the unused Japanese pause menu func_0026B018. A third Adventure-menu
line (result 2) opened the developers' test menu, Debug_TestMenuTask (0x0026B848): the Test Menu extra (extras.md).

## For a debug mode in the port (the first plan)

The port already has development options (`--autodrive`, `--cartrace`, `--keylog`, `--perf`, `--ranking`;
package/README.txt).
A debug mode could follow the original:
- **One switch**, a `--debug` option (the debug byte), which also adds the menu items.
- **Pause items:** hide the map and meter, refuel, AI drives my car (`--autodrive` in a menu), the zone table editor.
  The editor could save to `data/ai/Cnn.txt`, the port's own AI files, instead of `host:` C source. That makes it a
  modding tool.
- **Readouts:** the same values as text, from `CarBody` (velocity, accel, rpm, squeal, cell, surface flags), the AI's
  zone and target, as `--cartrace` writes them.
- **The free camera**, on a second pad or keys, and course 0x11's clock scrub.
- **Height override** for checking collisions.
