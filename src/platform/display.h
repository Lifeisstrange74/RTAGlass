// The PC's display settings: the window or the screen mode, the picture's aspect (wider than the PS2's 4:3 shows
// more of the 3D views to the sides: gfx::Draw2D's wide frame), where the HUD and menus' side pieces go in a wide
// picture, and the resolution the frame is drawn at. The Display screen (game/ui/display_screen) edits them, from
// the title's Options and the field menu's Settings; they're kept in display.json next to the executable, as
// controls.json is. main.cpp makes them so (the window, the device and the frame). See docs/display.md.
#pragma once

#include <string>
#include <vector>

namespace platform {

enum DisplayMode { kDisplayWindow, kDisplayFullscreen, kDisplayBorderless, kDisplayModeCount };
// The picture's aspect: Auto is the window's or the screen's (square pixels), the rest fixed (bars where the window
// or screen is another shape). 21:9 is 64:27, as the screens sold as 21:9 are.
enum DisplayAspect { kAspectAuto, kAspect4x3, kAspect16x10, kAspect16x9, kAspect21x9, kAspectCount };

// The Render Scale row's fixed choices (0 is Auto): under 1 draws fewer pixels than the PS2, for slow machines.
const int kRenderScaleCount = 7;
const float kRenderScales[kRenderScaleCount] = {0, 0.5f, 0.75f, 1, 2, 3, 4};

struct DisplaySettings {
    int mode;              // DisplayMode
    bool resizable;        // a window the player can drag to any size and maximize (its size kept as width/height)
    int width, height;     // the window's inside, or the full screen mode; 0 x 0 picks (Display_Resolve)
    int aspect;            // DisplayAspect
    bool edgeHud;          // the HUD's side pieces at the picture's edges (Edges), else in the 4:3 middle (Centered)
    float renderScale;     // the frame drawn at this multiple of 640 x 448 (kRenderScales), 0 = picked for the size
    bool vsync;            // Present waits for the screen's refresh (no tearing)
    // The game held to 60 frames a second, its speed (each frame is one step of the PS2's 60 Hz game); off, it runs
    // as fast as the machine goes, faster than the PS2 (an emulator's turbo).
    bool frameLimit;
    // Graphics Low: less drawn for slow machines (a shorter view, no frame trail or reflections, simpler far cars;
    // gfx::SetLowDetail). Full is the PS2's picture.
    bool lowGraphics;
};
bool Display_Same(const DisplaySettings& a, const DisplaySettings& b);

// The defaults are the PS2's picture: a 4:3 window of the largest multiple of 640 x 480 that fits, and the HUD
// where it was.
DisplaySettings Display_Defaults();
const DisplaySettings& Display_Get();
// Sets them and writes display.json (if Display_Load gave it a path), and marks them for main.cpp to make so
// (Display_TakeChange). False if the write failed.
bool Display_Apply(const DisplaySettings& s);
// True once after Display_Apply: the window, the device and the frame are to be made again.
bool Display_TakeChange();
// Reads `path` (missing or bad entries take the defaults) and keeps it for Display_Apply; an empty path keeps the
// defaults and never writes (the scripted checks).
void Display_Load(const std::string& path);

// What the machine offers, from main.cpp: the screen's modes (each size once), the desktop's size and the work
// area's (the desktop less the taskbar).
struct DisplaySize {
    int width, height;
};
void Display_SetMachine(const std::vector<DisplaySize>& modes, DisplaySize desktop, DisplaySize work);
// The sizes the Display screen offers for `mode`: the screen's modes for Full Screen; for a window the common sizes
// and the screen's modes that fit the work area (the frame and title bar allowed for); for Borderless the desktop.
std::vector<DisplaySize> Display_Sizes(int mode);
// The size `s` comes to: its own, or for 0 x 0 the window's largest 640 x 480 multiple that fits (up to 3x) or the
// desktop.
DisplaySize Display_Resolve(const DisplaySettings& s);
// The picture's aspect (width / height as seen) on a window or screen of `size`.
float Display_AspectValue(int aspect, DisplaySize size);
// The frame's width for that aspect: 448 high, 480 to 1 as seen, at least 640, even.
int Display_FrameWidth(float aspect);
// The render scale `s` comes to on `size`: its own, or the smallest whole one that draws the picture at least about
// as large as it's shown (up to 4).
float Display_RenderScale(const DisplaySettings& s, DisplaySize size, float aspect);

const char* Display_ModeName(int mode);
const char* Display_AspectName(int aspect);

}  // namespace platform
