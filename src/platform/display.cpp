#include "display.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include <algorithm>

#include "../util/files.h"
#include "../util/json.h"

namespace platform {

namespace {

const char kFormat[] = "RTAGlass display";
const int kVersion = 1;

const char* const kModeNames[kDisplayModeCount] = {"window", "fullscreen", "borderless"};
const char* const kModeLabels[kDisplayModeCount] = {"Window", "Full Screen", "Borderless"};
const char* const kAspectNames[kAspectCount] = {"auto", "4:3", "16:10", "16:9", "21:9"};
const float kAspects[kAspectCount] = {0, 4.0f / 3.0f, 16.0f / 10.0f, 16.0f / 9.0f, 64.0f / 27.0f};

// The window sizes offered besides the screen's modes.
const DisplaySize kWindowSizes[] = {
    {640, 480},   {800, 600},   {960, 720},   {1024, 768},  {1280, 720},  {1280, 800},  {1280, 960},
    {1366, 768},  {1440, 900},  {1600, 900},  {1600, 1200}, {1680, 1050}, {1920, 1080}, {1920, 1200},
    {1920, 1440}, {2560, 1080}, {2560, 1440}, {2560, 1600}, {3440, 1440}, {3840, 2160},
};
// What a window's frame and title bar take beyond its inside (about; XP's and later's).
const int kFrameW = 16, kFrameH = 40;

DisplaySettings g_settings = Display_Defaults();
std::string g_path;
bool g_changed = false;
std::vector<DisplaySize> g_modes;
DisplaySize g_desktop = {640, 480}, g_work = {640, 480};

int Find(const char* const* names, int count, const std::string& s) {
    for (int i = 0; i < count; ++i)
        if (s == names[i]) return i;
    return -1;
}

}  // namespace

DisplaySettings Display_Defaults() {
    return DisplaySettings{kDisplayWindow, false, 0, 0, kAspect4x3, true, 0, true, true, false};
}

const DisplaySettings& Display_Get() { return g_settings; }

bool Display_Same(const DisplaySettings& a, const DisplaySettings& b) {
    return a.mode == b.mode && a.resizable == b.resizable && a.width == b.width && a.height == b.height &&
           a.aspect == b.aspect && a.edgeHud == b.edgeHud && a.renderScale == b.renderScale && a.vsync == b.vsync &&
           a.frameLimit == b.frameLimit && a.lowGraphics == b.lowGraphics;
}

bool Display_Apply(const DisplaySettings& s) {
    g_settings = s;
    g_changed = true;
    if (g_path.empty()) return true;
    return util::WriteReplacing(g_path, [&](FILE* f) {
        fprintf(f, "{\n    \"format\": \"%s\",\n    \"version\": %d,\n", kFormat, kVersion);
        fprintf(f, "    \"notes\": \"mode window, fullscreen or borderless; resizable true or false (a window); "
                   "width and height 0 pick; aspect auto, 4:3, 16:10, 16:9 or 21:9; hud edges or centered; "
                   "renderScale 0 picks, else 0.5, 0.75, 1, 2, 3 or 4; vsync and frameLimit true or false (false: "
                   "the game runs faster than the PS2); graphics full or low (less drawn, for slow machines)\",\n");
        fprintf(f, "    \"mode\": \"%s\",\n", kModeNames[s.mode]);
        fprintf(f, "    \"resizable\": %s,\n", s.resizable ? "true" : "false");
        fprintf(f, "    \"width\": %d,\n    \"height\": %d,\n", s.width, s.height);
        fprintf(f, "    \"aspect\": \"%s\",\n", kAspectNames[s.aspect]);
        fprintf(f, "    \"hud\": \"%s\",\n", s.edgeHud ? "edges" : "centered");
        fprintf(f, "    \"renderScale\": %g,\n", (double)s.renderScale);
        fprintf(f, "    \"vsync\": %s,\n", s.vsync ? "true" : "false");
        fprintf(f, "    \"frameLimit\": %s,\n", s.frameLimit ? "true" : "false");
        fprintf(f, "    \"graphics\": \"%s\"\n}\n", s.lowGraphics ? "low" : "full");
    });
}

bool Display_TakeChange() {
    bool c = g_changed;
    g_changed = false;
    return c;
}

void Display_Load(const std::string& path) {
    g_path = path;
    g_settings = Display_Defaults();
    std::vector<uint8_t> data;
    if (path.empty() || !util::ReadAll(path, data) || data.empty()) return;
    util::JsonParser parser = {(const char*)&data[0], (const char*)&data[0] + data.size()};
    util::Json root = parser.Value(0);
    const util::Json* format = root.Get("format");
    if (!parser.ok || !format || format->text != kFormat) return;
    DisplaySettings& s = g_settings;
    if (const util::Json* v = root.Get("mode")) {
        int m = Find(kModeNames, kDisplayModeCount, v->text);
        if (m >= 0) s.mode = m;
    }
    if (const util::Json* v = root.Get("resizable")) s.resizable = v->text == "true";
    if (const util::Json* v = root.Get("vsync")) s.vsync = v->text != "false";
    if (const util::Json* v = root.Get("frameLimit")) s.frameLimit = v->text != "false";
    const util::Json* w = root.Get("width");
    const util::Json* h = root.Get("height");
    if (w && h && w->kind == util::Json::kNumber && h->kind == util::Json::kNumber) {
        s.width = atoi(w->text.c_str());
        s.height = atoi(h->text.c_str());
        if (s.width < 640 || s.height < 480 || s.width > 16384 || s.height > 16384) s.width = s.height = 0;
    }
    if (const util::Json* v = root.Get("aspect")) {
        int a = Find(kAspectNames, kAspectCount, v->text);
        if (a >= 0) s.aspect = a;
    }
    if (const util::Json* v = root.Get("hud")) s.edgeHud = v->text != "centered";
    if (const util::Json* v = root.Get("graphics")) s.lowGraphics = v->text == "low";
    if (const util::Json* v = root.Get("renderScale")) {
        float r = (float)atof(v->text.c_str());
        for (float k : kRenderScales)
            if (fabsf(r - k) < 0.01f) s.renderScale = k;
    }
}

void Display_SetMachine(const std::vector<DisplaySize>& modes, DisplaySize desktop, DisplaySize work) {
    g_modes = modes;
    g_desktop = desktop;
    g_work = work;
}

std::vector<DisplaySize> Display_Sizes(int mode) {
    std::vector<DisplaySize> out;
    if (mode == kDisplayBorderless) {
        out.push_back(g_desktop);
        return out;
    }
    if (mode == kDisplayFullscreen) return g_modes;
    auto add = [&](DisplaySize s) {
        if (s.width + kFrameW > g_work.width || s.height + kFrameH > g_work.height) return;
        for (const DisplaySize& o : out)
            if (o.width == s.width && o.height == s.height) return;
        out.push_back(s);
    };
    for (const DisplaySize& s : kWindowSizes) add(s);
    for (const DisplaySize& s : g_modes) add(s);
    if (out.empty()) out.push_back({640, 480});
    std::sort(out.begin(), out.end(), [](const DisplaySize& a, const DisplaySize& b) {
        return a.width != b.width ? a.width < b.width : a.height < b.height;
    });
    return out;
}

DisplaySize Display_Resolve(const DisplaySettings& s) {
    if (s.mode == kDisplayBorderless) return g_desktop;
    if (s.width && s.height) return {s.width, s.height};
    if (s.mode == kDisplayFullscreen) return g_desktop;
    // The largest whole multiple of 640x480 that fits the work area (at most 3).
    int k = 1;
    while (k < 3 && 640 * (k + 1) + kFrameW <= g_work.width && 480 * (k + 1) + kFrameH + 24 <= g_work.height) ++k;
    return {640 * k, 480 * k};
}

float Display_AspectValue(int aspect, DisplaySize size) {
    if (aspect > kAspectAuto && aspect < kAspectCount) return kAspects[aspect];
    float a = size.height > 0 ? (float)size.width / size.height : 4.0f / 3.0f;
    return a < 4.0f / 3.0f ? 4.0f / 3.0f : a;
}

int Display_FrameWidth(float aspect) {
    int w = (int)floorf(480.0f * aspect / 2.0f + 0.5f) * 2;
    return w < 640 ? 640 : w;
}

float Display_RenderScale(const DisplaySettings& s, DisplaySize size, float aspect) {
    if (s.renderScale > 0) return s.renderScale;
    // The picture's height as shown: the whole height, or less with bars above and below.
    float shown = std::min((float)size.height, size.width / aspect);
    int k = (int)ceilf(shown / 448.0f - 0.15f);
    return (float)(k < 1 ? 1 : k > 4 ? 4 : k);
}

const char* Display_ModeName(int mode) { return mode >= 0 && mode < kDisplayModeCount ? kModeLabels[mode] : ""; }
const char* Display_AspectName(int aspect) {
    return aspect == kAspectAuto ? "Auto" : aspect > 0 && aspect < kAspectCount ? kAspectNames[aspect] : "";
}

}  // namespace platform
