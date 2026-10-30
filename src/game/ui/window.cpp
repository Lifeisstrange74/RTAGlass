#include "window.h"

#include "../../audio/sound.h"
#include "../text.h"
#include "../../game_text.inc"   // the game's text, GT_<address>_<length>, made from the ELF

namespace game {

namespace {

const char kFont[] = "SYS/FONTE.GSL";

// The hints Help_Draw shows (D_0029A8E0): five a language, English, French and German (the title's language;
// the fourth row is empty). French keeps "Apply" and "Select", German "Select".
const char* const kHelp[3][5] = {
    {"\x81\x7E" GT_002EA03A_12, "\x81\x7E" GT_002EA02A_12, "\x81\xA2" GT_002EA01A_11, "\x81\xA2" GT_002EA00A_11,
     GT_002E9FF8_9},
    {"\x81\x7E" GT_002E9FEA_6, "\x81\x7E" GT_002E9FDA_7, "\x81\xA2" GT_002E9FCA_12, "\x81\xA2" GT_002E9FCA_12, GT_002E9FF8_9},
    {"\x81\x7E" GT_002E9FEA_6, "\x81\x7E" GT_002E9FBA_8, "\x81\xA2" GT_002E9FAA_8, "\x81\xA2" GT_002E9FAA_8, GT_002E9FF8_9},
};

// A piece of the font atlas. Pieces the original tiles from an 8-texel strip are stretched from its middle
// instead, which looks the same because the strips are uniform along the stretch.
void Piece(gfx::Draw2D& d, const gfx::Texture& t, int x, int y, int w, int h, int u, int v, int uw, int vh,
           uint32_t rgba) {
    float fu = (float)u, fv = (float)v, fw = (float)uw, fh = (float)vh;
    if (w != uw) fu += uw / 2 - 0.5f, fw = 1;
    if (h != vh) fv += vh / 2 - 0.5f, fh = 1;
    d.Sprite(t, (float)x, (float)y, (float)w, (float)h, fu, fv, fw, fh, rgba);
}

}  // namespace

// Window_DrawCursor (0x00203CD0): the horn, palette 13.
void Window_DrawCursor(gfx::Draw2D& d, int x, int y) {
    d.Sprite(d.Get(kFont, 0, 13), (float)x, (float)y - 2, 38, 28, 0x150, 0xE0, 38, 28);
}

// Menu_MoveCursor (0x00206D28).
int Menu_MoveCursor(const platform::Input& input, int pad, int sel, int count, bool noWrap) {
    return Menu_MoveCursorGrid(input, pad, sel, 1, count, noWrap);
}

int Menu_MoveCursorGrid(const platform::Input& input, int pad, int sel, int cols, int rows, bool noWrap) {
    uint32_t pressed = input.Pressed(pad), repeated = input.Repeated(pad);
    int col = sel % cols, row = sel / cols;
    // One axis: a press wraps, a repeat stops at the end.
    auto Step = [&](int at, int n, uint32_t back, uint32_t on, int* to) {
        if (n < 2) return false;
        if (!noWrap && (pressed & (back | on))) {
            *to = pressed & back ? (at > 0 ? at - 1 : n - 1) : (at < n - 1 ? at + 1 : 0);
            return true;
        }
        if ((repeated & back) && at > 0) *to = at - 1;
        else if ((repeated & on) && at < n - 1) *to = at + 1;
        return *to != at;
    };
    int to = col;
    if (Step(col, cols, platform::kLeft, platform::kRight, &to)) {
        audio::Sound_PlaySe(0x1D);
        return row * cols + to;
    }
    to = row;
    if (Step(row, rows, platform::kUp, platform::kDown, &to)) {
        audio::Sound_PlaySe(0x1D);
        return to * cols + col;
    }
    return sel;
}

Window::Window()
    : x(0x140), y(0x70), w(0), h(0), indent(0), items(0), state(kOpen), style(0), sel(0), pad(0), startConfirms(true),
      noWrap(false), lines_(0), firstItem_(0), fade_(0), built_(false), list_(nullptr), top_(0), hidden_(0),
      blink_(0) {}

void Window::SetList(const char* const* list, int count, int shown) {
    for (int i = 0; i < count && i < shown; ++i) AddLine(list[i], kLineSelectable);
    // The rows below are counted, and the window widened about its centre to the widest of them.
    for (int i = shown; i < count; ++i) {
        ++hidden_;
        int width = Text_Width(list[i]) + 20;
        if (w < width) {
            int grow = (width - w) / 2;
            w = (short)width;
            x -= (short)grow;
            indent += (short)grow;
        }
        if ((w - width) / 2 < indent) indent = (short)((w - width) / 2);
    }
    list_ = list;
    top_ = 0;
    noWrap = true;
}

void Window::AddLine(const char* text, int flags) {
    if (lines_ == 15) return;
    if (lines_ == 0) flags &= ~kLineSameLine;
    bool item = (flags & kLineSelectable) != 0, same = (flags & kLineSameLine) != 0;
    if (item && !same && ++items == 1) {
        firstItem_ = 0;
        for (int i = 0; i < lines_; ++i)
            if (!(flags_[i] & kLineSameLine)) ++firstItem_;
        sel = 0;
        indent = w / 2;
    }
    text_[lines_] = text;
    flags_[lines_] = (unsigned char)flags;
    // A continued line is as wide as all its pieces.
    int width = 0;
    for (int i = lines_;; --i) {
        width += Text_Width(text_[i]);
        if (!(flags_[i] & kLineSameLine)) break;
    }
    if (item) width += 20;
    if (w < width) {
        int grow = (width - w) / 2;
        w = (short)width;
        x -= (short)grow;
        indent += (short)grow;
    }
    if (item && (w - width) / 2 < indent) indent = (short)((w - width) / 2);
    ++lines_;
    if (!same) {
        y -= 6;
        h += 12;
    }
}

void Window::Place(int flags) {
    switch (flags & 3) {
    case 0: x = 0x140 - w / 2; break;
    case 1: x = 0x34; break;
    case 2: x = 0x24C - w; break;
    }
    switch (flags & 0xC) {
    case 0: y = 0x70 - h / 2; break;
    case 4: y = 0x18; break;
    case 8:
        y = 0xA8 - h / 2;
        if (y + h > 0xD8) y = 0xD8 - h;
        break;
    }
}

int Window::Update(gfx::Draw2D& d, const platform::Input& input) {
    if (!built_) {
        built_ = true;
        return 0;
    }
    // Opening fades in over 8 frames, closing fades out over 16.
    int alpha = 0x80;
    if (state == kOpening) {
        if (fade_ >= 16) state = kOpen;
        else fade_ += 2, alpha = fade_ * 8;
    } else if (state == kClosing) {
        if (fade_ > 0) --fade_;
        alpha = fade_ * 8;
    }
    uint32_t rgba = (uint32_t)alpha << 24 | 0x808080;
    Window_DrawFrame(d, x, y, w, h, style, rgba);

    if (items) {
        if (state == kOpen) sel = (signed char)Menu_MoveCursor(input, pad, sel, items + hidden_, noWrap);
        // A scroll list follows the cursor a row at a time and shows the rows from top_.
        if (list_) {
            if (sel < top_) --top_;
            else if (sel >= top_ + items) ++top_;
            for (int i = 0; i < items; ++i) text_[firstItem_ + i] = list_[top_ + i];
        }
        int row = firstItem_ + sel - top_;
        Window_DrawBar(d, x + indent + 20, (y + row * 12) * 2, w - indent * 2 - 20, rgba);
        if (state == kOpen) Window_DrawCursor(d, x + indent - 20, (y + row * 12) * 2);
    }

    // An item's line is palette 6 disabled, 5 the current setting, 0 (white) under the cursor; the rest the
    // style's: 10 (yellow) in style 2, else 0.
    int lineY = y - 11, px = x, base = style == 2 ? 10 : 0, row = -1, cursorRow = firstItem_ + sel - top_;
    for (int i = 0; i < lines_; ++i) {
        int f = flags_[i];
        if (!(f & kLineSameLine)) {
            px = f & kLineRight ? x + w : f & kLineSelectable ? x + indent + 20 : x;
            lineY += 12;
            ++row;
        }
        int color = !items                 ? base
                    : f & kLineDisabled    ? 6
                    : f & kLineCurrent     ? 5
                    : row == cursorRow     ? 0
                                           : base;
        if (f & kLineRight) {
            // Right-aligned to the window's right edge, on a continued line too (func_00201C80 at x + w).
            px = Text_PrintAt(d, x + w - Text_Width(text_[i]), lineY * 2, text_[i], color, rgba);
        } else {
            px = Text_PrintAt(d, px, lineY * 2, text_[i], color, rgba);
        }
    }

    if (list_ && state == kOpen && (blink_ & 0x10)) {
        if (top_ != 0) Icon_Draw(d, x + w / 2 - 10, y * 2 - 20, 2);
        if (top_ < hidden_) Icon_Draw(d, x + w / 2 - 10, (y + h) * 2, 3);
    }
    ++blink_;

    if (state == kOpen) {
        uint32_t p = input.Pressed(pad);
        if ((p & platform::kCross) || (startConfirms && (p & platform::kStart))) return 1;
        if (p & platform::kTriangle) return -1;
        return 0;
    }
    if (state == kClosing && fade_ == 0) return -2;
    return 0;
}

void Icon_Draw(gfx::Draw2D& d, int x, int y, int n) {
    d.Sprite(d.Get(kFont, 0, 0), (float)x, (float)y, 20, 20, (float)(n * 20 + 120), (float)0xB4, 20, 20);
}

void Money_Draw(gfx::Draw2D& d, int x, int y, int value, const char* unit) {
    // Its caps (5 texels at u 0x1AB and 0x1C8) and two middles, each repeating an 8-texel column (REGION_REPEAT):
    // the figure's from u 0x1B0, 0x60 wide, and the unit's from 0x1C0, 0x40 wide.
    const gfx::Texture& t = d.Get(kFont, 0, 9);
    d.Sprite(t, (float)(x - 5), (float)y, 5, 32, (float)0x1AB, (float)0xDD, 5, 32);
    d.Sprite(t, (float)x, (float)y, 0x60, 32, (float)0x1B0 + 0.5f, (float)0xDD, 7, 32);
    d.Sprite(t, (float)(x + 0x60), (float)y, 0x40, 32, (float)0x1C0 + 0.5f, (float)0xDD, 7, 32);
    d.Sprite(t, (float)(x + 0xA0), (float)y, 5, 32, (float)0x1C8, (float)0xDD, 5, 32);
    Number_PrintRight(d, x + 0x60, y + 6, value, 0);
    Text_PrintAt(d, x + 0x62, (y / 2 + 3) * 2, unit, 0);
}

void Window_DrawFrame(gfx::Draw2D& d, int x, int y, int w, int h, int style, uint32_t rgba) {
    // A 28x32 nine-slice in the atlas: 12-texel corners and edges around an 8x8 middle. Style 0 is at u 0x12C,
    // palette 8; styles 1 and 2 share the piece at u 0x104, palettes 14 and 7.
    int u = style == 0 ? 0x138 : 0x110;
    const gfx::Texture& t = d.Get(kFont, 0, style == 2 ? 7 : style == 1 ? 14 : 8);
    int top = y * 2, bottom = (y + h) * 2, mid = h * 2;
    Piece(d, t, x, top, w, mid, u, 0xE8, 8, 8, rgba);
    Piece(d, t, x - 12, top - 12, 12, 12, u - 12, 0xDC, 12, 12, rgba);
    Piece(d, t, x, top - 12, w, 12, u, 0xDC, 8, 12, rgba);
    Piece(d, t, x + w, top - 12, 12, 12, u + 8, 0xDC, 12, 12, rgba);
    Piece(d, t, x - 12, top, 12, mid, u - 12, 0xE8, 12, 8, rgba);
    Piece(d, t, x + w, top, 12, mid, u + 8, 0xE8, 12, 8, rgba);
    Piece(d, t, x - 12, bottom, 12, 12, u - 12, 0xF0, 12, 12, rgba);
    Piece(d, t, x, bottom, w, 12, u, 0xF0, 8, 12, rgba);
    Piece(d, t, x + w, bottom, 12, 12, u + 8, 0xF0, 12, 12, rgba);
}

void Window_DrawBar(gfx::Draw2D& d, int x, int y, int w, uint32_t rgba) {
    const gfx::Texture& t = d.Get(kFont, 0, 11);
    Piece(d, t, x - 8, y - 1, 8, 28, 0x180, 0xE0, 8, 28, rgba);
    Piece(d, t, x, y - 1, w, 28, 0x188, 0xE0, 8, 28, rgba);
    Piece(d, t, x + w, y - 1, 8, 28, 0x190, 0xE0, 8, 28, rgba);
}

void Window_DrawCellFrame(gfx::Draw2D& d, int col, int row, int col2, int row2, int style, uint32_t rgba) {
    Window_DrawFrame(d, col * 20 + 30, row * 12 + 17, (col2 - col - 1) * 20, (row2 - row - 1) * 12, style, rgba);
}

// "OK" + cross at u 0x154 and "BACK" + triangle at u 0x100, both at v 0x118.
void Help_DrawButton(gfx::Draw2D& d, int x, int y, HelpButton button, uint32_t rgba) {
    float u = button == kButtonOk ? (float)0x154 : (float)0x100;
    d.Sprite(d.Get(kFont, 0, 2), (float)x, (float)y, 84, 32, u, (float)0x118, 84, 32, rgba);
}

void Help_Draw(gfx::Draw2D& d, int mode, uint32_t rgba, int language) {
    const char* const* help = kHelp[language];
    Window_DrawFrame(d, 0x1AE, 0x1D, 0xB4, mode == 0 ? 0xC : 0x18, 0, rgba);
    Text_PrintAt(d, 0x1B6, 0x3A, help[mode == 3 ? 0 : 1], 1, rgba);
    const char* second = mode == 1 ? help[2] : mode == 2 || mode == 3 ? help[3] : mode == 4 ? help[4] : nullptr;
    if (second) Text_PrintAt(d, 0x1B6, 0x52, second, 1, rgba);
}

}  // namespace game
