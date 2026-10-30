#include "debug_text.h"

#include <stdarg.h>
#include <stdio.h>

#include <string>
#include <vector>

namespace game {

namespace {

struct Line {
    int x, y;
    std::string text;
};
std::vector<Line> g_lines;

}  // namespace

void DebugText_Printf(int x, int y, const char* format, ...) {
    char buf[128];
    va_list args;
    va_start(args, format);
    vsnprintf(buf, sizeof buf, format, args);
    va_end(args);
    if (g_lines.size() < 256) g_lines.push_back(Line{x, y, buf});
}

void DebugText_Draw(gfx::Draw2D& d) {
    const gfx::Texture& font = d.Get("SYS/FONTE.GSL", 0, 15);
    for (const Line& l : g_lines) {
        float x = (float)l.x;
        for (unsigned char c : l.text) {
            if (c >= 'a' && c <= 'z') c = (unsigned char)(c - 0x20);
            if (c > 0x20 && c < 0x60)
                d.Sprite(font, x, l.y * 2.0f, 8, 16, 448.0f + (c & 7) * 8, 224.0f + (c & 0x78), 8, 8, 0x80808080,
                         false);
            x += 8;
        }
    }
    g_lines.clear();
}

void DebugText_Clear() { g_lines.clear(); }

}  // namespace game
