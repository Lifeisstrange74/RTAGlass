# The Paint Shops

The Paint Shops (place 3 of towns 1, 2, 3, 6, 8 and 9; Room_SceneKind 3) repaint a car of the team and its wheels.
The talk asks "Who do you want to paint?" (end code 6, the car pick). Picking a car gives the scene result
8 | car << 8. Shop_SceneTask then goes to state 2 (+0x25 = 2), asks for that car on the turntable
(+0x02 = car + 1) and starts **PaintShop_Task** (func_00256958) for it. The port has all of it in
`src/game/story/paint_screen.cpp` (PaintScreen, run by RoomScene).

## The scene

These are Shop_SceneTask's first-frame choices (Room_SetupTask), and the port now follows them:

| Place | Song | Car state |
|---|---|---|
| Q's Factory (place 0, town < 10) | 0x0A0B0101 | 0: the car drives in on path 0 onto the turntable |
| A shop or the bank (Room_SceneKind 1-5) | 0x0A0B0103 | 0xE (drives in on path 6 and stands), except a Paint Shop (kind 3): state 0, the turntable |
| Any other place | 0x0A0B0102 (the president's office, (8, 9): 0x0A0B0104) | 0xE |

Kinds 1 and 4 also start func_0020DC50(0x380000) (a load, as Q's Factory does), and kind 2 clears D_00335720.

As in Q's Factory, the Paint Shop shows the team's cars as the save has them (`RoomScene::Room_HasTeam`). A
teammate's car comes onto the turntable through the same swap (states 2-5). When the screen ends (+4 = -1),
state 2 restarts Message_Task on the place's table and asks for the player's car back (+0x02 = 1).

**The stick.** In state 2 (the parts window or the paint screen), the car on the turntable turns with a stick:
the player's car in car state 0xD, a teammate's in 5. This needs info+0x0C == 7, pad 1's mode as the pad reader
(func_0020A8A8) has it from libpad's scePadInfoMode: 7 analog, 4 digital. In analog mode, func_0020AB10 maps each of
the pad's stick bytes 4-7 (right X, right Y, left X, left Y) through D_0029AD80[byte >> 4] (-3 -2 -2 -1 -1 0 0 0 0 0
0 1 1 2 2 3) into +0x3C + 4 pad. The scene reads +0x3E, pad 1's left stick X, halved towards 0 (-1, 0 or 1 a frame),
and adds it to the turntable's angle (+0x26) and to the car's heading's high byte. It then rebuilds the car's matrix
in place. Leaving the screen asks for the car back (+0x02 = 1), and state 0xC turns it to face the camera again.

The port has this in `RoomScene::Room_StickTurn`, on a controller only. It uses the **right** stick, because the
port's left stick also works as the d-pad's Left and Right, which the screens use for the levels and the carousel.

## The screen

Every piece is in the Paint Shop's room picture, the 640×384 one at TBP 0x1F2E, with its UI below row 224. Two
textures of the same room file are used as well: texture 4, the 128×128 wave at TBP 0x22F2, for the light, and
texture 5, the 32×32 glow at 0x2313. Each town's file has them at the same index. PaintShop_InitSprites
(func_00255070) builds the canvas D_0175B570, and PaintShop_Draw (func_002558B8) updates it each frame.

**The lamp.** 0xD6×0x38 from (0x120, 0x140) at (0x178, y), drawn alone while it slides.

**The panels.** Each is drawn in three pieces: the left end, 0xDC of an 8-texel strip repeated by REGION_REPEAT,
and the right end. The short ones (Colors and the bottom row, 0x28 high) come from (0x50, 0x132), the tall ones
(0x4E high) from (0, 0x132). Each panel's orange tab is 0x64×0x1A from (0, 0x116), with its label 3 pixels right and
down: "Colors" (0x80, 0xF0), "Body" (0x80, 0x104), "Wheel" (0x80, 0x118).

| y | Panel | What's on it |
|---|---|---|
| 0x34 | Colors | "1" and "2", 0x40×0x18 at x 0x6C and 0xCC. The chosen one is orange (v 0xF8), the other dark (v 0xE0) |
| 0x76 | Body (paint 1) | the R, G and B rows |
| 0xC4 | paint 2 (no tab) | grey and faint (0x60404040) when the car has one colour |
| 0x12C | Wheel | the 12 wheel colours, 0xE4×0x2A from (0x118, 0xE0) at (0x66, 0x13E) |
| 0x180 | bottom | OK at x 0x51 and Cancel at 0xBA, 0x60×0x18. OK chosen: OK (0xB0, 0x160) orange, Cancel (0xB0, 0x130) dark; else OK (0xB0, 0x148), Cancel (0x50, 0x160) |

**A colour row** (rows j = 0-5: paint 1's R, G, B, then paint 2's):
- Its ball: 16×16 from (0x100, 0xF0 + 0x10 (j % 3)) at (0x5B, 0x83 / 0x96 / 0xA9 / 0xD1 / 0xE4 / 0xF7). It's bright on the cursor's row; otherwise 0x80404040, or 0x40404040 on paint 2 with one colour.
- 16 squares of 10×10 from (0x80, 0xE0) at x 0x70 + 10 i, y 0x85 / 0x98 / 0xAB / 0xD3 / 0xE6 / 0xF9. Each is coloured by D_0029D190 (0x19 … 0xD8): the row's channel at level i, the other two at the paint's levels. Their alpha is 0x40 on paint 2 with one colour.
- A bar 2 high, 10 × level + 10 long, at 10 below the squares, in red, green or blue.
- The level in Number_PrintSmallRight at (0x128, 0x81 / 0x94 / 0xA7 / 0xCF / 0xE2 / 0xF5).

**The wheel colour.** A ring (0x20×0x1C from (0x1B0, 0x120)) sits on the car's wheel colour f, at x = f × 27 / 2 + 0x65 and
y = 0x13E + 18 (f & 1). The even colours are the top row and the odd ones the bottom. On the Wheel row the pointing hand
(0x20×0x20 from (0x1D5, 0x11C)) is 0x18 above the ring. On other rows Sprite_Hide (func_00207998) turns the hand's
XYZ2 registers into XYZ3, so it isn't drawn.

**The cursor.** Window_DrawCursor at x 0x20, y by row 0-8: 0x3E, 0x8A, 0x92, 0x9A, 0xD8, 0xE0, 0xE8, 0x148, 0x18A.
Rows 1-3 and 4-6 are only 8 apart, not one colour row (0x13) apart. That's how the game has it.

**The light.** A quad (0x190, 0x10) (0x236, 0x10) (0x170, 0x100) (0x256, 0x100) of the wave texture, UVs
(u, 0)-(u + 128, 128) with u = (frame × 4) & 0x7C (it repeats). Its colour (+0x3148) is 0xFF in the cursor row's
channel (rows 1-6; none on the others), with alpha +0x02 × 4.

**The glows.** Two of them (PaintShop_DrawGlows, func_002557A0), colour (0, 0x80, 0x80): cyan. While +0x02 isn't 0
they sit at D_002AC600[(frame / 2) & 7] and the place 4 on (round the lamp); otherwise at entry 8, (0, -0x40), out of
sight.

The light and the glows are added to the frame (ALPHA 0x48). Everything else blends normally (0x44).

## The tasks

**PaintShop_Task** (func_00256958), on its first frame:
- The car's +0x00 (paint) top byte becomes +0x05, the wheel colour (0-11). +0x20 keeps the whole word, for Cancel and the cost.
- +0x27, two colours, is set unless paint 2's nibbles 3-5 equal paint 1's 0-2.
- It waits for the parent's car state: 0xC or 0xD for the player's car, 4 for a teammate's. Then +0x02 = -0x38 and PaintShop_SlideInTask.

**PaintShop_SlideInTask** (func_00256A78): the lamp 4 lower a frame. At 0, PaintShop_MenuTask.

**PaintShop_MenuTask** (func_00256640), each frame:
1. PaintShop_Input (func_002560F0), on pad 1:
   - Up and Down (repeating) move the row 0-8, SE 0x1D. With one colour, rows 4 and 6 send the cursor to 7 and 3 (it skips paint 2), and that frame takes no Left or Right.
   - Left and Right (repeating) on rows 1-6 step the level 0-15 with no sound. A step sets +0x02 = 0x20 (the flash) and +0x25 = 1 (OK).
   - On row 7 they step the wheel colour 0-11 (both +0x05 and the paint's top byte), SE 0x1D, OK.
   - A press of Left or Right on row 0 switches one or two colours; on row 8 it switches OK and Cancel. Both SE 0x1D.
   - With one colour, paint 2 copies paint 1 afterwards.
2. PaintShop_Draw, then the light's colour and scroll. +0x02 counts down, then PaintShop_DrawGlows.
3. Cross on row 8, or Triangle anywhere (which picks Cancel), ends the menu:
   - If the paint word equals +0x20, the screen leaves at once (+4 = -1 to the parent, PaintShop_SlideOutTask).
   - Else SE 0x1A, then a window, and PaintShop_AnswerTask:
     - Cancel: Notice_AskTask (func_0023BB88) "Cancel?".
     - OK, if the money covers the cost: "OK?".
     - OK, if it doesn't: Notice_Task "Need more money" (+0x25 = 0, so it acts as Cancel).
4. The canvas, then Wallet_Draw: the money now and after the cost. The cost is 100 if bits 0-23 changed, plus 100 if the top byte (the wheels) did.

**Notice_AskTask** (func_0023BB88): a centred window with the text, then D_002A2AF0's "Yes" and "No" (either pad in
2 Player). It gives the parent the answer (+4: 0 Yes, 1 No; Triangle is No) and +0x24 = 1.

**PaintShop_AnswerTask** (func_002564E8): the screen and the wallet as they were (the light and glows held) until
the answer.
- Yes with OK: Money_Pay(cost), SE 0x305; +0x20 = the paint.
- Yes with Cancel (and after Need more money, whose answer is 0): +0x00 = +0x20 and +0x05 = its top byte.

  Either way the parent is told (+4 = -1, +0x24 = 1) and PaintShop_SlideOutTask follows.
- No: back to the menu, SE 0x1B.

**PaintShop_SlideOutTask** (func_002568D8): the lamp 4 higher a frame until -0x38, then the task ends. The talk has
already started over.

## The wheel colour on the car

The save car's +0x05 is the wheels' colour. Car_DrawTires draws TIRE.BIN's picture in CLUT +0x05 (of 16; 13-15
are the same as 12). The close-up wheels (func_00221570, WHEEL.BIN) take D_0029D1A0[+0x05], three nibble levels into
D_0029D150, as their paint. The port passes the palette through `CarRecord::wheelColor` and
`gfx::DrawStyle::texture` (car.cpp's TirePicture).

**The other cars.** Car_Draw passes Car_DrawTires the record D_0177EA50[slot] points to: the save's car for the
team, otherwise a driver record (D_002C22C8). All 443 driver records have +0x05 = 0 and a 0 top byte on the paint
(checked in the ELF), and nothing else writes +0x05. Team_Join copies only the paint word and the body, so a car
that joins keeps its slot's wheel colour. So every car outside the team has palette 0, which is what the port draws.
The tires' TBP is also 0x1C44 + 2 × +0x06 (the Wheels part), which picks the hub tile (cars.md). The big tires
(part flag 0x400) take +0x05 as their CLUT too, at TBP 0x1C44 itself.

## Checked in PCSX2

The recipe:
1. Load `captures/adventure/field_peach.p2s`.
2. Teleport to the door over PINE: fixed position (195303047, 639630, 177924320), yaw 5950. That's (512.8, 30.5, 484.1) in Peach Town's block, computed from the car's fixed and float positions.
3. Press Cross through Barthou's talk, hold Cross to drive in, press through the talk, and pick the car.

The captures:
- `paint_menu.p2s` is the menu; `paint_menu_ps2.png` and `paint_menu_port.png` compare it.
- `paint_flash.gs` and `paint_flash_ps2.png` show the red light at alpha 40 with the glows. The GS dump confirms the sprite list above: ALPHA 0x48 for the light and the glows, UVs (8, 0)-(136, 128).
- `paint_wheel_ps2.png` and `paint_wheel_port.png` show red wheels.
- `paint_cancel_port.png` is the port's Cancel → Yes, which puts the car and the money back.

The port's test: `--field 1 --place 512.8 35 484.1 5950 --press 200:x:60,700:x,900:x,1250:x,1400:x`. The screen is
up by frame 1450; the starting car is random.
