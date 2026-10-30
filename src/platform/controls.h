// The PC's control setup: for each player, every action's keyboard key and controller button. The PS2 maps the
// car's actions to pad buttons in the save (+0x12E8, the field menu's Settings); the PC binds the actions straight
// to keys and controller buttons instead, and the menus' buttons too. The Controls screen (game/ui/controls_screen)
// edits it, and it's kept in controls.json next to the executable, apart from the saves (a Quick Race starts new
// records, and the title has none loaded). See docs/controls.md.
#pragma once

#include <stdint.h>

#include <string>

namespace platform {

// The actions. The car's first (the Controls screen's Driving page, the PS2 Settings' rows plus the steering), then
// the menus' (the pad buttons the game's screens read).
enum Action {
    kActSteerLeft, kActSteerRight, kActGas, kActBrake, kActReverse, kActJet, kActView, kActHorn, kActWingUp,
    kActWingDown, kActNavigator,
    kActUp, kActDown, kActLeft, kActRight, kActConfirm, kActBack, kActStart,
    kActionCount
};
const int kDrivingActions = kActUp;   // the car's actions are 0 .. kDrivingActions - 1

// A controller's inputs (XInput), 0 none. The left stick also always steers and moves Left and Right in menus.
enum PadInput {
    kPadNone, kPadA, kPadB, kPadX, kPadY, kPadLB, kPadRB, kPadLT, kPadRT, kPadBack, kPadStart, kPadLS, kPadRS,
    kPadUp, kPadDown, kPadLeft, kPadRight,
    kPadInputCount
};

// One player's: a Windows virtual-key code (0 none) and a PadInput for each action. Player 1 has the keyboard and
// the first controller, player 2 the second controller (and keys, if set).
struct Bindings {
    uint8_t key[kActionCount];
    uint8_t pad[kActionCount];
};
struct Controls {
    Bindings player[2];
};

// The defaults: player 1's keys are the PS2 layout's (Z Cross, X Circle, A Square, C Triangle, Q L1, E R1, 1 L2,
// 3 R2, Tab Select, Enter Start, the arrows the d-pad) through the PS2's default key setup; player 2 has no keys.
// Both controllers' buttons by position (A Cross, B Circle, X Square, Y Triangle, LB/RB/LT/RT L1/R1/L2/R2, Back
// Select).
void Controls_Defaults(Controls& c);
void Controls_DefaultPage(Controls& c, int player, bool driving);   // one page of it back to the defaults

// The setup in use (what Input reads each frame).
const Controls& Controls_Get();
// Sets it, and writes controls.json (if Controls_Load gave it a path). False if the write failed.
bool Controls_Apply(const Controls& c);
// Reads `path` (the defaults for a missing file or entry) and keeps it for Controls_Apply; an empty path keeps the
// defaults and never writes (the scripted checks).
void Controls_Load(const std::string& path);

// Names, as controls.json and the screen have them: "Z", "Enter", "Num5", "#BA" for a key without one; "A",
// "LB", "DpadUp" ... for the controller ("Up" ... on screen, PadInput_ShortName). "" for none.
const char* Key_Name(int vk);
int Key_FromName(const std::string& name);   // -1 if it isn't one
const char* PadInput_Name(int input);
const char* PadInput_ShortName(int input);
int PadInput_FromName(const std::string& name);   // -1 if it isn't one
// Keys a binding can't have: Esc (it quits, and cancels a binding), the Windows keys, and the like.
bool Key_Bindable(int vk);

// While the Controls screen waits for a key, Esc cancels that instead of quitting.
void Controls_SetCapturing(bool on);
bool Controls_Capturing();

}  // namespace platform
