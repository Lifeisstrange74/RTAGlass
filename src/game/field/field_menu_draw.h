// The pieces the field menu's screens share (field_menu.cpp, field_menu_screens.cpp; the Controls screen too): POSE.GSL's panel and
// backgrounds, Menu_UpdateList's states, the sprite tables' pieces.
#pragma once

#include "../../gfx/draw2d.h"

namespace game {
namespace menu {

const char kPose[] = "SYS/POSE.GSL";

// Menu_UpdateList's states. The cars drive 40 pixels a frame.
enum {
    kListInit = 0,          // the cursor's car lit and out, the list scrolled to where it was left
    kListScroll = 1,        // scrolling there, then the cars drive in: the title's,
    kListTitleIn = 2,       //   10 frames,
    kListAllIn = 3,         //   then all of them 6 frames,
    kListItemsIn = 4,       //   then the items' 10 more
    kListReady = 0x64,      // the scroll arrows
    kListIdle = 0x65,       // the pad
    kListMove = 0x66,       // the cursor's move: 4 frames, the new car 5 a frame out, the old one back
    kListChosen = 0xC8,     // Cross (Sound 0x1A): the chosen car drives off 10 frames,
    kListChosenOut = 0xC9,
    kListAllOut = 0xCA,     //   then all of them 6,
    kListRestOut = 0xCB,    //   then the rest 10
    kListGone = 0xCC,
    kListEntered = 0x12B,   // the result after Cross
    kListExit = 0x12C,      // Triangle (Sound 0x1B; Start too on the main screen): all drive back out right, 16
    kListExitOut = 0x12D,   //   frames
    kListExitGone = 0x12E,
    kListLeft = 0x18F,      // the result after Triangle
};
const int kDrive = 40;
// A piece of a sprite table (D_0029C0C0, D_0029C300 ...: x, y from the entry's place, u, v, w, h, and bit 0 a
// mirror left to right, bit 1 top to bottom).
struct Piece {
    int x, y, u, v, w, h, flip;
};
// D_0029C0C0: the sign (POSE picture 0), its four corners one picture mirrored, then the arrow's two halves and the
// post, at (0x1B8, 0xA0) (D_01727B90 + 0x81B4).
void Piece_Draw(gfx::Draw2D& d, const gfx::Texture& t, const Piece& p, int x, int y);
// Menu_DrawPanel (0x002119A8): the dark panel behind the title (top left) and the yellow borders with their lights
// along the top and the bottom (POSE picture 0).
void Menu_DrawPanel(gfx::Draw2D& d);
// Menu_DrawRoad (0x00212190): a screen's background from (x, y) by its kind (see field_menu.cpp).
void Menu_DrawRoad(gfx::Draw2D& d, int x, int y, int kind = 0);
// func_0024F148 (Prim_DrawLineStrip): a Gouraud line strip a pixel wide through n points (PS2 RGBA colours), each
// segment a thin quad. Settings' box and the Map's mark.
void Line_DrawStrip(gfx::Draw2D& d, const float* px, const float* py, const uint32_t* rgba, int n);

}  // namespace menu
}  // namespace game
