# Controls (PC)

The PC's own control setup and the screen that edits it. This is a PC feature, not a port of anything: the PS2's
Settings screen (field_menu_settings.md) maps the car's actions to pad buttons; the PC maps every action straight
to a keyboard key and a controller button. Code: `src/platform/controls.{h,cpp}` (the setup and controls.json),
`src/platform/input.cpp` (applying it), `src/game/ui/controls_screen.{h,cpp}` (the screen).

## The model

- **Actions** (`platform::Action`): the car's (Steer Left, Steer Right, Gas Pedal, Brake Pedal, Reverse, Jet Fire,
  Change View, Horn & Lights, Wing Up, Wing Down, Navigator) and the menus' (Up, Down, Left, Right, Confirm =
  Cross, Back = Triangle, Start). The port's screens read no other pad buttons (Circle, Square, L1/R1/L2/R2 and
  Select only ever mattered through the PS2's key setup), so those have no menu action.
- Each player has a **key** (a Windows virtual-key code) and a **controller input** (`platform::PadInput`: XInput's
  A B X Y LB RB LT RT Back Start LS RS and the d-pad) for each action, or none. Player 1 is the keyboard and the
  first controller; player 2 the second controller, plus any keys given to them (none by default).
- `Input::Update` turns them into the pad words the game reads: the menu actions into the PS2 bits (`kCross` ...),
  the car's into their own bits (`kGas` ... `kNavigator`, bits 16-26). `Car_ReadPad`, `Car_HornHeld`,
  `Car_CameraButton` and `Car_MapButton` (race.cpp) read the car's bits; on the PS2 they read the pad through the
  record's key setup (save +0x12E8), which the PC keeps only for the memory card (save conversion).
- The left stick always steers and moves Left/Right in menus, as before. Scripted presses (`--press`) are PS2
  buttons; they also hold the actions the PS2's default key setup gives them (Cross gas, Square brake ...).

Defaults (the PS2's default key setup on the old keyboard layout): steering and the d-pad on the arrows, Z gas and
Confirm, A brake, X reverse, 3 jet, C view and Back, E horn, Q / 1 wing up / down, Tab navigator, Enter Start. The
controller by position: A gas/Confirm, X brake, B reverse, RT jet, Y view/Back, RB horn, LB / LT the wings, Back the
navigator, Start, the d-pad. The old extra keys (K, L, J, I, Space, Backspace) are gone: an action has one key.

## controls.json

Next to the executable, written by Apply (through a temporary file), read at start. Not part of the saves: a
Quick Race starts new records and the title has none loaded, so a setup kept in the record wouldn't stay put.
Scripted runs (`--shot`) neither read nor write it, so checks always see the defaults.

```
{ "format": "RTAGlass controls", "version": 1, "notes": "...",
  "player1": { "keyboard": { "steerLeft": "Left", "gas": "Z", ... }, "controller": { "gas": "A", ... } },
  "player2": { ... } }
```

Key names: `A`-`Z`, `0`-`9`, `F1`-`F24`, `Num0`-`Num9`, `Num*` `Num+` `Num-` `Num.` `Num/`, `Space` `Enter` `Tab`
`BkSp` `Shift` `Ctrl` `Alt` `Caps` `Ins` `Del` `Home` `End` `PgUp` `PgDn` `Up` `Down` `Left` `Right`, the
punctuation keys as their US characters, and `#XX` (hex) for any other. Controller names: `A B X Y LB RB LT RT Back
Start LS RS DpadUp DpadDown DpadLeft DpadRight`. `""` is none. A missing file, entry or unknown name keeps the
default. Player 1's menu actions always end up with a key (a missing one gets its default), so the keyboard can
always work the menus. Esc, F10, the Windows and menu keys and Print Screen can't be bound.

## The screen

Reached from the title's Options (a "Controls" item before Extras and Exit) and from the field menu's Settings,
whose list (Controls, Extras, Other; extras.md) it's the first item of, in the PS2 key setup's place. It keeps the PS2 screen's parts and workings (field_menu_settings.md): the list window (style 2) with
the bar and horn cursor, the help window, OPTION2's tiled background and button icons, the 3D pad (OPTION.BIN)
leaning towards the button with the red/yellow box turning round it, Initial settings / Cancel change / Apply, the
warnings in palette 6 and Apply refused (SE 0x1E) while any remain, Triangle with changes asking "Cancel?" (No
first), the sounds, and in the field the title car with "Settings" driving in and out. On the title the panel
says "Controls" instead.

What differs:
- Rows are 22 lines apart (the PS2's 24) and the pad's centre is at x 500 (470): there are more rows and two
  columns. The first row is the page, `< 1P Driving >`, `< 1P Menus >`, `< 2P Driving >`, `< 2P Menus >`
  (Left/Right), with the columns' heads, Key and Pad, beside it.
- On an action row Left/Right picks the column; Cross then waits up to 10 s for a key (Key) or a button on that
  player's controller (Pad): Esc cancels, Delete clears. The cell blinks "?" meanwhile. A still box marks the
  chosen cell. Controller buttons show OPTION2's PS2 icons by position (A as Cross ...; Back as SELECT); Start,
  the sticks and d-pad Left/Right, which have no icon, are written out.
- The pad leans to and boxes the row's controller button. The box places for Start, LS/RS and d-pad Left/Right
  aren't in the PS2's table (D_0029C538); they were measured off the drawn model.
- Warnings: a key two actions share (both players' keys together, driving and menu actions apart), a controller
  button two of a player's actions share, and player 1's menu action without a key. The page row turns palette 6
  when another page has one.
- Initial settings resets only the page shown; Apply sets the setup in use and writes controls.json.
- Esc normally quits; while the screen waits for a key it cancels instead (`Controls_Capturing`), and Esc's
  auto-repeat never quits. Alt can be bound (the window procedure passes WM_SYSKEYDOWN/UP on and keeps Alt and
  F10 from opening the window menu).
