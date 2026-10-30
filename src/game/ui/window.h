// The game's menu windows (core/winman, 0x00205350-0x00206040): a framed box of text lines, some of them
// selectable, with a highlight bar and the horn cursor. Every menu in the game is built from these.
//
// Coordinates follow the original: x and widths in pixels, y and heights in half-lines (the game doubles them),
// so a text line is 12 half-lines (24 pixels) tall. The frame, bar, cursor and button symbols are pieces of the
// font atlas, SYS/FONTE.GSL, each drawn with its own palette. See docs/title.md.
#pragma once

#include "../../gfx/draw2d.h"
#include "../../platform/input.h"

namespace game {

// Line flags (the byte per line at +0x50 in the original).
enum WindowLine : int {
    kLineDisabled = 0x01,     // drawn in palette 6
    kLineCurrent = 0x02,      // the current setting: drawn in palette 5 (green)
    kLineRight = 0x08,        // right-aligned
    kLineSelectable = 0x10,   // a menu item
    kLineSameLine = 0x80,     // continues the previous line
};

class Window {
public:
    // How the window is in the frame (+0x0E). kInactive is a flag over kOpen: a child menu has the input.
    enum State { kOpen = 0, kOpening = 1, kClosing = 2, kInactive = 0x80 };

    // WindowAlloc (0x00205C20): empty, centred on (320, 112).
    Window();

    // Window_AddLine (0x002058A8): appends a line; the window grows around its centre to fit it.
    void AddLine(const char* text, int flags);
    // func_00205DA0: a scrolling list of `count` items from `list`, `shown` of them at a time. The window is as wide
    // as the widest of them and its cursor stops at the ends; Update scrolls it (Window_UpdateList, 0x00205350), with
    // blinking arrows above and below while there's more that way. `sel` then counts over the whole list.
    void SetList(const char* const* list, int count, int shown);
    // Window_Place (0x00205AD0): bits 0-1 horizontal (0 centre, 1 left, 2 right), bits 2-3 vertical (0 centre,
    // 4 top, 8 lower half).
    void Place(int flags);

    // Window_Update (0x00205C08; Window_UpdateList 0x00205350 without a scroll list): draws the window and runs its
    // cursor. The first call does nothing: the game's menu tasks make their window on their first frame and only
    // start updating it on the next, so a window always appears a frame after it's made. Returns 1 when an item is chosen (Cross), -1 on cancel (Triangle), -2 once a closing window has faded
    // out, otherwise 0.
    int Update(gfx::Draw2D& d, const platform::Input& input);

    void SetLineFlags(int line, int flags) { flags_[line] = (unsigned char)flags; }
    // A line's text, changed in place (it must stay valid: the window keeps the pointer).
    void SetLineText(int line, const char* text) { text_[line] = text; }
    int LineFlags(int line) const { return flags_[line]; }
    // The first line that is an item, for indexing items as lines.
    int FirstItemLine() const { return firstItem_; }
    // A scroll list's first row shown (+0x10).
    int Top() const { return top_; }

    short x, y, w, h;
    short indent;          // +0x08: items start at x + indent + 20
    unsigned char items;   // +0x0B: selectable lines
    unsigned char state;   // +0x0E: State
    unsigned char style;   // +0x5F: frame colour, 0 blue (menus), 1 and 2 cyan with orange edges
    signed char sel;       // +0x0D: the chosen item, counted over items only
    unsigned char pad;     // +0x0C bits 0-1: the pad it reads, 0 pad 1, 1 pad 2, 2 either (platform::kEitherPad)
    bool startConfirms;    // +0x0C bit 3: Start also chooses. On by default on PC, where Enter is Start
    bool noWrap;           // +0x0C bit 2: the cursor stops at the ends

private:
    const char* text_[15];
    unsigned char flags_[15];
    int lines_;            // +0x0F
    int firstItem_;        // +0x0A: lines before the first item
    int fade_;             // +0x13: 0-16 while opening or closing
    bool built_;           // the frame the window is made on has passed
    const char* const* list_;   // the scroll list (SetList), or null
    int top_;              // +0x10: the list's first row shown
    int hidden_;           // +0x11: the list's rows that don't fit
    int blink_;            // +0x12: frames, for the arrows
};

// func_00203E70: icon `n` of the font atlas's row of 20 x 20 icons at v 0xB4 (palette 0), top-left at (x, y): 2 and
// 3 the up and down arrows, 7 the message box's arrow.
void Icon_Draw(gfx::Draw2D& d, int x, int y, int n);

// Money_Draw (func_00202D10): the money box, 0xAA x 32 from x - 5 at pixel (x, y), FONTE palette 9: `value` in
// the large digits ending at x + 0x60, the currency unit after it.
void Money_Draw(gfx::Draw2D& d, int x, int y, int value, const char* unit);

// Window_DrawFrame (0x00202350): the frame around an inner box (x, y, w, h; y and h in half-lines), and its
// background. `rgba` modulates it (the open and close fades).
void Window_DrawFrame(gfx::Draw2D& d, int x, int y, int w, int h, int style, uint32_t rgba = 0x80808080);

// Window_DrawBar (0x00203098): the highlight bar for text at pixel (x, y), `w` wide, palette 11. It starts a
// pixel above y and has 8-pixel caps on both sides.
void Window_DrawBar(gfx::Draw2D& d, int x, int y, int w, uint32_t rgba = 0x80808080);

// Window_DrawCellFrame (0x00204568): a frame around text cells: columns col..col2 and rows row..row2 of the
// Text_Print grid, borders included, in Window_DrawFrame's `style` (the original passes it through in $t0 untouched).
void Window_DrawCellFrame(gfx::Draw2D& d, int col, int row, int col2, int row2, int style,
                          uint32_t rgba = 0x80808080);

// Menu_MoveCursor (0x00206D28) for one row of `count` items, on `pad`: a press moves the cursor and wraps at the ends
// unless `noWrap`; a held direction repeats but stops at the ends. Each move plays 0x1D. Returns the new item.
int Menu_MoveCursor(const platform::Input& input, int pad, int sel, int count, bool noWrap);
// The same for a grid of `cols` x `rows` items (row by row): Left and Right in the row, Up and Down in the column.
int Menu_MoveCursorGrid(const platform::Input& input, int pad, int sel, int cols, int rows, bool noWrap);

// Window_DrawCursor (0x00203CD0): the horn cursor, its top left at pixel (x, y), palette 13.
void Window_DrawCursor(gfx::Draw2D& d, int x, int y);
// Help_DrawButton (0x00200DC0): one of the green button plates, 84x32 at pixel (x, y), FONTE palette 2.
enum HelpButton { kButtonOk = 0, kButtonBack = 1 };
void Help_DrawButton(gfx::Draw2D& d, int x, int y, HelpButton button, uint32_t rgba = 0x80808080);

// Help_Draw (0x00203240): the box of button hints at the top right. 0: Cross Enter; 1: Cross Enter, Triangle
// Back; 2: Cross Enter, Triangle Exit; 3: Cross Apply, Triangle Exit; 4: Cross Enter, <> Select.
void Help_Draw(gfx::Draw2D& d, int mode, uint32_t rgba = 0x80808080, int language = 0);

}  // namespace game
