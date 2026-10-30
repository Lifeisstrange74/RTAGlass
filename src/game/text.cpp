#include "text.h"

#include <stdio.h>

namespace game {

namespace {

const char kFont[] = "SYS/FONTE.GSL";

// Glyph widths for characters 0x20-0x7F, from the game's table at 0x0029A860 (indexed by character code).
const signed char kWidth[96] = {
    8,  16, 6,  16, 16, 16, 16, 6,  8,  10, 16, 16, 10, 10, 12, 16,   //  !"#$%&'()*+,-./
    12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 16, 16, 16, 16, 16, 16,   // 0-9 :;<=>?
    16, 15, 14, 15, 16, 12, 12, 16, 14, 12, 16, 16, 16, 16, 16, 16,   // @A-O
    14, 16, 14, 14, 14, 16, 16, 16, 16, 14, 16, 16, 16, 16, 16, 16,   // P-Z [\]^_
    16, 14, 14, 14, 14, 14, 14, 14, 14, 8,  14, 14, 8,  16, 12, 13,   // `a-o
    14, 14, 12, 12, 12, 12, 12, 14, 14, 14, 13, 16, 16, 16, 16, 16,   // p-z {|}~
};

// Text_PrintClipped's top edge: nothing above this pixel row is drawn.
int g_clipTop = -0x10000;

// A glyph's sprite with the rows above g_clipTop cut off.
void GlyphSprite(gfx::Draw2D& d, const gfx::Texture& t, float x, float y, float w, float h, float u, float v,
                 uint32_t rgba) {
    float cut = (float)g_clipTop - y;
    if (cut >= h) return;
    if (cut > 0) {
        y += cut;
        v += cut;
        h -= cut;
    }
    d.Sprite(t, x, y, w, h, u, v, w, h, rgba);
}

// Accent escapes ('/', '`', '^', '~' before a letter) and two-byte characters take two bytes.
int CharBytes(const unsigned char* s) {
    return (*s == '/' || *s == '`' || *s == '^' || *s == '~' || *s >= 0x80) && s[1] ? 2 : 1;
}

// Two-byte (Shift-JIS) characters the menus use: the pad button symbols, 20x20 cells in the atlas
// (func_00200FE8's table), and the kanji row at v 0x140 (func_002012F8: codes over 0x889E), whose last cell is the
// map's car. The messages use no others (an unknown one is skipped).
bool WideCell(unsigned code, int* u, int* v) {
    *v = 0xE0;
    switch (code) {
    case 0x819B: *u = 0xA0; return true;   // circle
    case 0x817E: *u = 0xB4; return true;   // cross
    case 0x81A0: *u = 0xC8; return true;   // square
    case 0x81A2: *u = 0xDC; return true;   // triangle
    case 0x8192: *u = 0x50; *v = 0xB4; return true;   // a heart (func_00200FE8's jump table; the mails)
    case 0x81F4: *u = 0x64; *v = 0xB4; return true;   // a musical note
    case 0x8148: *u = 0x17C; *v = 0xA0; return true;   // a full-width '?' (the Sandpolis bartender's (3,4) #2)
    }
    static const unsigned kKanji[11] = {0x95FB, 0x8CFC, 0x88C8, 0x8FE3, 0x8BF3, 0x9765,
                                        0x97CA, 0x96A2, 0x937B, 0x8AF9, 0x8EA9};
    for (int i = 0; i < 11; ++i)
        if (code == kKanji[i]) {
            *u = 0x14 * i;
            *v = 0x140;
            return true;
        }
    return false;
}

// Text_DrawGlyph's accent escapes: the atlas cell (16x24) of `prefix` + `letter`, from the Latin-1 rows at v 0x60,
// 0x78 and 0x90. '/' only makes "/e" (an unknown "/x" draws the '/'); others fall back to the cell of the inverted
// question mark. '~' is the diaeresis, with ~C, ~c, ~s and ~R for the cedillas, sharp s and the registered sign.
void AccentCell(unsigned char prefix, unsigned char letter, int* u, int* v) {
    struct Accent {
        char prefix, letter;
        short u, v;
    };
    static const Accent kAccents[] = {
        {'/', 'e', 0x60, 0x90},  {'`', 'a', 0x1E0, 0x78}, {'`', 'e', 0x70, 0x90},  {'`', 'u', 0x180, 0x90},
        {'^', 'a', 0x00, 0x90},  {'^', 'e', 0x80, 0x90},  {'^', 'i', 0xC0, 0x90},  {'^', 'o', 0x120, 0x90},
        {'^', 'u', 0x190, 0x90}, {'~', 'A', 0x10, 0x78},  {'~', 'O', 0x130, 0x78}, {'~', 'U', 0x190, 0x78},
        {'~', 'a', 0x20, 0x90},  {'~', 'e', 0x90, 0x90},  {'~', 'i', 0xD0, 0x90},  {'~', 'o', 0x130, 0x90},
        {'~', 'u', 0x1A0, 0x90}, {'~', 'C', 0x40, 0x78},  {'~', 'c', 0x50, 0x90},  {'~', 's', 0x1C0, 0x78},
        {'~', 'R', 0xA0, 0x60},
    };
    for (const Accent& a : kAccents)
        if (a.prefix == (char)prefix && a.letter == (char)letter) {
            *u = a.u;
            *v = a.v;
            return;
        }
    if (prefix == '/') {
        *u = ('/' - 0x21) * 16;
        *v = 0;
    } else {
        *u = 0x1B0;
        *v = 0x60;
    }
}

// Draws one character (one or two bytes) at (x, y); returns the next x.
int DrawChar(gfx::Draw2D& d, int x, int y, const unsigned char* s, int n, int color, uint32_t rgba) {
    if (n == 2 && s[0] >= 0x80) {   // a two-byte character: 20 wide, drawn 2 lower
        int u, v;
        if (WideCell(s[0] << 8 | s[1], &u, &v))
            GlyphSprite(d, d.Get(kFont, 0, color), (float)x, (float)y + 2, 20, 20, (float)u, (float)v, rgba);
        return x + 20;
    }
    if (n == 2) {
        // An accented letter: its own cell, as wide as the plain letter (the '/' itself when it's drawn).
        int u, v;
        AccentCell(s[0], s[1], &u, &v);
        unsigned char base = s[0] == '/' && s[1] != 'e' ? '/' : s[1];
        if (base < 0x20 || base >= 0x80) return x;
        int w = kWidth[base - 0x20];
        GlyphSprite(d, d.Get(kFont, 0, color), (float)x, (float)y, (float)w, 24, (float)(u + (16 - w) / 2), (float)v,
                    rgba);
        return x + w;
    }
    return Text_DrawGlyph(d, x, y, s[0], color, rgba);
}

}  // namespace

int Text_DrawGlyph(gfx::Draw2D& d, int x, int y, unsigned char c, int color, uint32_t rgba) {
    if (c < 0x20 || c >= 0x80) return x;
    int i = c - 0x20, w = kWidth[i];
    if (c != ' ') {
        // Cells are 16x24, 31 to a row from '!'; a narrow glyph is centred in its cell.
        const gfx::Texture& t = d.Get(kFont, 0, color);
        int cell = c - 0x21;
        float u = (float)(cell % 31 * 16 + (16 - w) / 2), v = (float)(cell / 31 * 24);
        GlyphSprite(d, t, (float)x, (float)y, (float)w, 24, u, v, rgba);
    }
    return x + w;
}

int Text_PrintScaled(gfx::Draw2D& d, int x, int y, const char* str, int color, int scale, uint32_t rgba) {
    const gfx::Texture& t = d.Get(kFont, 0, color);
    for (const unsigned char* s = (const unsigned char*)str; *s; ++s) {
        unsigned char c = *s;
        if (c == ' ') {
            x += 10 * scale >> 8;
            continue;
        }
        if (c < 0x21 || c >= 0x80) continue;
        int w = kWidth[c - 0x20], cell = c - 0x21;
        int sw = w * scale >> 8, sh = 24 * scale >> 8;
        d.Sprite(t, (float)x, (float)y, (float)sw, (float)sh, (float)(cell % 31 * 16 + (16 - w) / 2),
                 (float)(cell / 31 * 24), (float)w, 24, rgba);
        x += sw;
    }
    return x;
}

int Text_PrintClipped(gfx::Draw2D& d, int x, int y, const char* str, int color, int top, uint32_t rgba) {
    int old = g_clipTop;
    g_clipTop = top;
    x = Text_PrintAt(d, x, y, str, color, rgba);
    g_clipTop = old;
    return x;
}

int Text_Print(gfx::Draw2D& d, int col, int row, const char* str, int color, uint32_t rgba) {
    return Text_PrintAt(d, col * 20 + 10, (row * 12 + 5) * 2, str, color, rgba);
}

int Text_PrintAt(gfx::Draw2D& d, int x, int y, const char* str, int color, uint32_t rgba) {
    for (const unsigned char* s = (const unsigned char*)str; *s;) {
        int n = CharBytes(s);
        x = DrawChar(d, x, y, s, n, color, rgba);
        s += n;
    }
    return x;
}

namespace {

// One large digit: 0-3 sit at (0x1B8, 0xC8) in the atlas, 4-9 at (0, 0xDC), 16 apart.
void DrawDigit(gfx::Draw2D& d, int x, int y, int digit, int color, uint32_t rgba) {
    float u = (float)(digit < 4 ? 0x1B8 + digit * 16 : digit * 16 - 0x40), v = digit < 4 ? 200.0f : 220.0f;
    d.Sprite(d.Get(kFont, 0, color), (float)x, (float)y, 16, 20, u, v, 16, 20, rgba);
}

}  // namespace

int Number_PrintRight(gfx::Draw2D& d, int x, int y, int value, int color, uint32_t rgba) {
    do {
        x -= 16;
        DrawDigit(d, x, y, value % 10, color, rgba);
        value /= 10;
    } while (value != 0);
    return x;
}

int Number_PrintDigits(gfx::Draw2D& d, int x, int y, int value, int digits, int color, uint32_t rgba) {
    for (int i = 0; i < digits; ++i) {
        x -= 16;
        DrawDigit(d, x, y, value % 10, color, rgba);
        value /= 10;
    }
    return x;
}

void Text_PrintSmall(gfx::Draw2D& d, int x, int y, const char* str, int color, uint32_t rgba) {
    for (const unsigned char* s = (const unsigned char*)str; *s; ++s, x += 10) {
        unsigned char c = *s;
        if (c == '\'' || c == '"' || c == '-') {
            Text_DrawGlyph(d, x, y, c, color, rgba);
        } else if (c != ' ') {
            int i = c - 1;
            d.Sprite(d.Get(kFont, 0, color), (float)x, (float)y, 10, 20, (float)(i % 51 * 10),
                     (float)(0xB4 + i / 51 * 20), 10, 20, rgba);
        }
    }
}

int Number_PrintSmallRight(gfx::Draw2D& d, int x, int y, int value, int color, uint32_t rgba) {
    char buf[2] = {0, 0};
    int n = 0;
    do {
        if (n == 3 || n == 6 || n == 9) {
            x -= 10;
            Text_PrintSmall(d, x, y, ",", color, rgba);
        }
        x -= 10;
        buf[0] = (char)('0' + value % 10);
        Text_PrintSmall(d, x, y, buf, color, rgba);
        value /= 10;
        ++n;
    } while (value != 0);
    return x;
}

void Time_Print(gfx::Draw2D& d, int x, int y, int frames, int color, uint32_t rgba) {
    char buf[16];
    if (frames == 0x7FFFFFFF) {
        Text_PrintSmall(d, x, y, "--'--\"--", color, rgba);
    } else if (frames >= 0x57E40) {
        Text_PrintSmall(d, x, y, "99'59\"99", color, rgba);
    } else {
        snprintf(buf, sizeof(buf), "%2d'%02d\"%02d", frames / 3600, frames / 60 % 60, frames % 60 * 100 / 60);
        Text_PrintSmall(d, x, y, buf, color, rgba);
    }
}

const char* Text_PlaceName(int place) {
    // D_0029FD30
    static const char* const kPlaces[24] = {" 1st", " 2nd", " 3rd", " 4th", " 5th", " 6th", " 7th", " 8th",
                                            " 9th", "10th", "11th", "12th", "13th", "14th", "15th", "16th",
                                            "17th", "18th", "19th", "20th", "21st", "22nd", "23rd", "24th"};
    return kPlaces[place >= 0 && place < 24 ? place : 23];
}

int Text_Width(const char* str) {
    int w = 0;
    for (const unsigned char* s = (const unsigned char*)str; *s;) {
        int n = CharBytes(s);
        unsigned char c = n == 2 && s[0] < 0x80 ? s[1] : s[0];
        if (c >= 0x80) w += 20;
        else if (c >= 0x20) w += kWidth[c - 0x20];
        s += n;
    }
    return w;
}

}  // namespace game
