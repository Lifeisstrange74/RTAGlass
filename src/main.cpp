// RTAGlass - Windows port of Road Trip (PS2).
// Entry point: raw Win32 window, the D3D9 renderer, input and the game's screens at a fixed 60 Hz.
//
//   RTAGlass.exe [--scale N] [--aspect A] [--hud centered] [--graphics low]
//     The reimplemented boot sequence, title and main menu (game/title), New Game's name entry (game/flow) and
//     the Quick Race course select (game/quickrace) and the race (game/race). Keys and controller buttons are set
//     on the Controls screen (Options on the title, Settings in the field menu) and kept in controls.json next to
//     the executable (the Extras screen's switches in extras.json, the Display screen's in display.json); by
//     default Enter Start, arrows d-pad and steering, Z Cross/gas, A brake, X reverse, C
//     Triangle/view, Q and 1 the wings, E horn, 3 jet, Tab navigator; or the first XInput controller. The second
//     XInput controller is player 2's pad. Esc quits; Alt+Enter switches between a window and Borderless. With
//     the Display screen's Resizable on, the window can be dragged to any size and maximized.
//     --scale N (0.5 and 0.75 too) makes a window N x 480 high and draws the frame at N times 640 x 448 (the
//     Display screen's Render Scale); --aspect A (auto, 4:3, 16:10, 16:9 or 21:9) and --hud centered set the
//     Display screen's Aspect Ratio and HUD, --graphics low its Graphics; all for this run (display.json isn't
//     written).
//   --shot N[,N...] PREFIX   writes PREFIX_<N>.png of frame N (as shown, at the render scale), then exits. The
//                            default controls are used (controls.json isn't read or written).
//   --press N[:K][,N[:K]...] presses a button on frame N (for scripted checks of the screens): K is s Start (the
//                            default), x Cross, o Circle, t Triangle, q Square, u d l r the d-pad, m Select, and 2
//                            before it presses it on pad 2 (N:2x). N:K:M holds it for M frames.
//   --race ID                starts straight in a Quick Race on course ID (decimal, e.g. 0 = Peach Raceway).
//   --clock N                races and the Adventure start at time of day N (frames, 0-0x34BC0, 9000 an hour).
//   --factory                starts a New Game straight at Q's Factory (after the opening), with a new save.
//   --advrace N              --factory, then straight into Adventure race N (the race list's row, 0-27), and back to
//                            that race's town's Q's Factory after it.
//   --field N                --factory, then straight out onto the field at town N's Q's Factory (1-9).
//   --room T P               --factory, then straight into building P of town T (any with a talk, 1-21; 0 0 My
//                            Garage).
//   --ending N               --factory, then straight into the ending: 0 the whole one (from the president's
//                            office), 1 My City Cinemas' replay.
//   --allparts               with --factory: the save owns five of every part and bodies 0-7, has the Super A
//                            license and two teammates (the 2 Player preset's), for checking the parts and races.
//   --coinradar              with --factory: the save has the Coin Radar (the field map shows the ChoroQ coins).
//   --beaten                 with --factory: every driver has been beaten in a race (the talks' teammate offers).
//   --mycity HEX             with --factory: My City's residents who have moved in (save +0x670, e.g. 3FFFFFF all).
//   --stamp N[,N...]         with --factory: stamps N earned (1-100); --flag N[,N...] event flags N set;
//                            --item N[,N...] kind 15's items N owned (decimal); --won every town race won;
//                            --gpstage N the World Grand Prix's stages done (save +0x3310, 0-6).
//   --meter N                the save's car 0 has meter part N (0-10): the race's and the field's meter routine.
//   --partflags HEX          the save's car 0 has part flags HEX (save car +8: e.g. 3004 the Flight Wing, fins and jets).
//   --cartrace FILE          writes the player's car's physics each race frame to FILE (CSV).
//   --tracecar N             traces car N instead (1-23: the opponents, numbered as the game's car records).
//   --autodrive N            the AI drives the player's car, held to N x 2.5 km/h (0: as fast as it goes).
//   --ranking                the pause menu has the Adventure's Ranking in a Quick Race too.
//   --extra NAME[,NAME...]   turns on extras (extras.json's names: lifeMeter, qWave, mail5, testCourse, body149,
//                            demoSong, duckQuiz, language, pauseItems, readouts, freeCamera, heightKeys,
//                            clockScrub, testMenu, mapTuner; and cheats with the cheats' names: uncappedSpeed,
//                            infiniteFuel, powerBoost, superGrip, reduceDrag, lowGravity, slowRivals, maxMoney, allParts) for
//                            this run, as the Extras and Cheats screens would.
//   --keylog FILE            writes every key message the window gets (time, message, key, lParam) to FILE.
//   --wav FILE               with --shot: records the sound of the scripted frames (800 samples each) to a WAV.
//   --perf FILE              writes what each frame cost to FILE (CSV, after '#' lines on the machine and the
//                            card): the screen, the milliseconds of the frame, the game, the renderer's draws and
//                            uploads and Present, the draw calls by what was drawn, the uploads, and where the
//                            player is, as --field/--place and --clock take it (docs/renderer.md).
//   RTAGlass.exe --soundtest ID[,ID...] SECONDS OUT.wav
//     Development: renders sound requests (hex, as the game sends them, e.g. 328 = the TAKARA jingle, or
//     FFSSEE = song entry EE of file FF in slot SS, e.g. 0C0201 = BGM_01) to a WAV
//     file without opening a window or the sound device. A music entry is started as the song.
//   RTAGlass.exe --save-convert IN OUT
//     Converts a save between the PC's JSON file (.json) and the PS2 card's BASLUS-20398 file (any other name): IN is
//     either, OUT is written by its extension. --save-new P1 P2 writes the two records a Quick Race starts with.
//
// These options are also listed for players in package/README.txt (copied to the dist folder); keep both in step.
//
// GS dumps of the original play in a separate development tool, tools/gsplay; the game itself doesn't run any GS
// or VU emulation.

#include <windows.h>
#include <intrin.h>
#include <mmsystem.h>
#include <shellapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "audio/sound.h"
#include "game/extras.h"
#include "game/flow/new_game.h"
#include "game/pad/vibration.h"
#include "game/perf_where.h"
#include "game/race/race.h"
#include "game/save/save_data.h"
#include "game/save/save_file.h"
#include "game/title/title_sequence.h"
#include "gfx/d3d9/d3d9_renderer.h"
#include "gfx/draw2d.h"
#include "gfx/textures/texture_set.h"
#include "platform/controls.h"
#include "platform/display.h"
#include "platform/input.h"
#include "util/data_pack.h"
#include "util/files.h"
#include "util/image_write.h"

namespace {

const wchar_t kWindowClass[] = L"RTAGlassWindow";
const wchar_t kWindowTitle[] = L"RTAGlass";

const DWORD kWindowStyle = WS_OVERLAPPEDWINDOW & ~(WS_THICKFRAME | WS_MAXIMIZEBOX);
const DWORD kResizableStyle = WS_OVERLAPPEDWINDOW;
const int kMinClientW = 320, kMinClientH = 240;   // a resizable window's smallest inside

bool g_running = true;
platform::Input* g_input = nullptr;
FILE* g_keyLog = nullptr;   // --keylog
bool g_hideCursor = false;  // full screen and Borderless
bool g_placing = false;     // PlaceWindow is moving the window (its WM_SIZE isn't the player's)
bool g_resized = false;     // the player resized the window (a drag ended, or maximize or restore)
bool g_maximized = false;
bool g_frameLimit = true;   // the Display screen's Frame Limiter (never for scripted runs)

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (g_keyLog && (msg == WM_KEYDOWN || msg == WM_KEYUP || msg == WM_CHAR || msg == WM_SYSKEYDOWN ||
                     msg == WM_SYSKEYUP)) {
        const char* name = msg == WM_KEYDOWN ? "down" : msg == WM_KEYUP ? "up" : msg == WM_CHAR ? "char"
                         : msg == WM_SYSKEYDOWN ? "sysdown" : "sysup";
        fprintf(g_keyLog, "%lu,%s,0x%02X,0x%08lX\n", (unsigned long)GetMessageTime(), name, (unsigned)wp,
                (unsigned long)lp);
        fflush(g_keyLog);
    }
    switch (msg) {
    case WM_KEYDOWN:
        // Esc quits, unless the Controls screen is waiting for a key (there it cancels). Not its auto-repeat: a
        // held Esc that cancelled mustn't quit.
        if (wp == VK_ESCAPE && !(lp & 0x40000000) && !platform::Controls_Capturing()) DestroyWindow(hwnd);
        if (g_input) g_input->OnKey(wp, true);
        return 0;
    case WM_KEYUP:
        if (g_input) g_input->OnKey(wp, false);
        return 0;
    case WM_SYSKEYDOWN:
    case WM_SYSKEYUP:
        // Alt+Enter: a window, or Borderless from one (Full Screen goes to a window). Enter isn't pressed for the game.
        if (wp == VK_RETURN) {
            if (msg == WM_SYSKEYDOWN && !(lp & 0x40000000)) {
                platform::DisplaySettings s = platform::Display_Get();
                s.mode = s.mode == platform::kDisplayWindow ? platform::kDisplayBorderless : platform::kDisplayWindow;
                platform::Display_Apply(s);
            }
            return 0;
        }
        // Alt (and keys held with it) can be bound too. Alt alone and F10 don't go on to open the window's menu,
        // which would stop the game until the next key; Alt+F4 and the like still do what they do.
        if (g_input) g_input->OnKey(wp, msg == WM_SYSKEYDOWN);
        if (wp == VK_MENU || wp == VK_F10) return 0;
        break;
    case WM_CHAR:
        if (g_input) g_input->OnChar(wp);
        return 0;
    case WM_GETMINMAXINFO: {
        RECT rc = {0, 0, kMinClientW, kMinClientH};
        AdjustWindowRect(&rc, (DWORD)GetWindowLongW(hwnd, GWL_STYLE), FALSE);
        MINMAXINFO* mm = (MINMAXINFO*)lp;
        mm->ptMinTrackSize.x = rc.right - rc.left;
        mm->ptMinTrackSize.y = rc.bottom - rc.top;
        return 0;
    }
    case WM_EXITSIZEMOVE:
        g_resized = true;
        return 0;
    case WM_SIZE:
        // A drag sends many; its end comes as WM_EXITSIZEMOVE. Maximize and restore come only as these.
        if (!g_placing && wp != SIZE_MINIMIZED) {
            if (wp == SIZE_MAXIMIZED || (wp == SIZE_RESTORED && g_maximized)) g_resized = true;
            g_maximized = wp == SIZE_MAXIMIZED;
        }
        return 0;
    case WM_SETCURSOR:
        if (g_hideCursor && LOWORD(lp) == HTCLIENT) {
            SetCursor(nullptr);
            return TRUE;
        }
        break;
    case WM_KILLFOCUS:
        if (g_input) g_input->ReleaseKeys();   // key-ups go to the other window
        return 0;
    case WM_DESTROY:
        g_running = false;
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

HWND CreateMainWindow(HINSTANCE inst) {
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    // The exe's icon (rtaglass.rc), at the sizes the title bar and the taskbar want.
    wc.hIcon = (HICON)LoadImageW(inst, MAKEINTRESOURCEW(1), IMAGE_ICON, GetSystemMetrics(SM_CXICON),
                                 GetSystemMetrics(SM_CYICON), 0);
    wc.hIconSm = (HICON)LoadImageW(inst, MAKEINTRESOURCEW(1), IMAGE_ICON, GetSystemMetrics(SM_CXSMICON),
                                   GetSystemMetrics(SM_CYSMICON), 0);
    wc.lpszClassName = kWindowClass;
    if (!RegisterClassExW(&wc)) return nullptr;

    return CreateWindowExW(0, kWindowClass, kWindowTitle, kWindowStyle, CW_USEDEFAULT, CW_USEDEFAULT, 640, 480,
                           nullptr, nullptr, inst, nullptr);
}

// The display settings worked out on this machine: the back buffer, the mode's refresh rate, the frame's width and
// the render scale.
struct DisplayPlan {
    int mode;
    platform::DisplaySize size;
    int refresh;
    int frameWidth;
    float scale;
};

DisplayPlan PlanDisplay(const platform::DisplaySettings& s, const std::vector<gfx::D3D9Renderer::Mode>& modes) {
    DisplayPlan p;
    p.mode = s.mode;
    p.size = platform::Display_Resolve(s);
    p.refresh = 0;
    for (const gfx::D3D9Renderer::Mode& m : modes)
        if (m.width == p.size.width && m.height == p.size.height) p.refresh = m.refresh;
    float aspect = platform::Display_AspectValue(s.aspect, p.size);
    p.frameWidth = platform::Display_FrameWidth(aspect);
    p.scale = platform::Display_RenderScale(s, p.size, aspect);
    return p;
}

// The window for the plan: a framed window of its size in the work area's middle (with a sizing border and maximize
// box when resizable), or a borderless one over the screen (also under Full Screen, whose mode D3D sets).
void PlaceWindow(HWND hwnd, const DisplayPlan& p, bool resizable) {
    bool framed = p.mode == platform::kDisplayWindow;
    DWORD style = !framed ? WS_POPUP : resizable ? kResizableStyle : kWindowStyle;
    g_placing = true;
    if (IsZoomed(hwnd)) ShowWindow(hwnd, SW_RESTORE);
    g_maximized = false;
    SetWindowLongW(hwnd, GWL_STYLE, style | (IsWindowVisible(hwnd) ? WS_VISIBLE : 0));
    RECT rc = {0, 0, p.size.width, p.size.height};
    AdjustWindowRect(&rc, style, FALSE);
    int w = rc.right - rc.left, h = rc.bottom - rc.top, x = 0, y = 0;
    RECT work;
    if (framed && SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0)) {
        x = work.left + std::max(0L, (work.right - work.left - w) / 2);
        y = work.top + std::max(0L, (work.bottom - work.top - h) / 2);
    }
    SetWindowPos(hwnd, framed ? HWND_NOTOPMOST : HWND_TOP, x, y, w, h, SWP_FRAMECHANGED);
    g_placing = false;
    g_resized = false;
    g_hideCursor = !framed;
}

// Makes the display settings so on a running game: the device reset to the new back buffer and mode (before the
// window changes when leaving full screen, after when entering it), then the frame. `place` false leaves the window
// as it is (the player has just resized it).
void ApplyDisplay(HWND hwnd, gfx::D3D9Renderer& renderer, gfx::Draw2D& draw,
                  const std::vector<gfx::D3D9Renderer::Mode>& modes, bool place = true) {
    const platform::DisplaySettings& s = platform::Display_Get();
    DisplayPlan p = PlanDisplay(s, modes);
    float scale = std::min(p.scale, (float)renderer.MaxScale(p.frameWidth, gfx::Draw2D::kHeight));
    bool full = p.mode == platform::kDisplayFullscreen;
    renderer.SetVsync(s.vsync);
    g_frameLimit = s.frameLimit;
    if (full && place) PlaceWindow(hwnd, p, s.resizable);
    renderer.Reset(p.size.width, p.size.height, scale, full, p.refresh);
    if (!full && place) PlaceWindow(hwnd, p, s.resizable);
    draw.SetFrameWidth(p.frameWidth);
    draw.SetEdgeAnchors(s.edgeHud);
    gfx::SetLowDetail(s.lowGraphics);
}

// A resizable window the player resized: its new inside becomes the setting's size (kept in display.json) and the
// back buffer and frame follow it.
void TakeResize(HWND hwnd, gfx::D3D9Renderer& renderer, gfx::Draw2D& draw,
                const std::vector<gfx::D3D9Renderer::Mode>& modes) {
    if (!g_resized) return;
    g_resized = false;
    platform::DisplaySettings s = platform::Display_Get();
    RECT rc;
    if (s.mode != platform::kDisplayWindow || !s.resizable || !GetClientRect(hwnd, &rc)) return;
    if (rc.right <= 0 || rc.bottom <= 0) return;
    platform::DisplaySize now = platform::Display_Resolve(s);
    if (rc.right == now.width && rc.bottom == now.height) return;
    s.width = rc.right;
    s.height = rc.bottom;
    platform::Display_Apply(s);
    platform::Display_TakeChange();
    ApplyDisplay(hwnd, renderer, draw, modes, false);
}

std::string Narrow(const wchar_t* s) {
    char buf[MAX_PATH * 2];
    WideCharToMultiByte(CP_ACP, 0, s, -1, buf, sizeof(buf), nullptr, nullptr);
    return buf;
}

bool DirExists(const std::string& path) {
    DWORD a = GetFileAttributesA(path.c_str());
    return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY);
}

// A data folder has the texture set, loose (development) or packed (a Release build's dist).
bool HasTextures(const std::string& data) {
    return DirExists(data + "\\textures") ||
           GetFileAttributesA((data + "\\textures.rpk").c_str()) != INVALID_FILE_ATTRIBUTES;
}

// A player's copy has the models and collision data packed too (data\models.rpk, data\collision.rpk; the textures'
// pack is TextureSet's): mounts them over their folders, where loose files still win. The first pack that won't open,
// or empty.
std::string MountPacks(const std::string& data) {
    for (const char* folder : {"models", "collision"}) {
        std::string pack = data + "\\" + folder + ".rpk";
        if (GetFileAttributesA(pack.c_str()) != INVALID_FILE_ATTRIBUTES &&
            !util::Pack_Mount(pack, data + "\\" + folder))
            return pack;
    }
    return std::string();
}

// The data folder: next to the executable, or data when running from bin\<config>.
std::string FindDataDir() {
    char exe[MAX_PATH];
    GetModuleFileNameA(nullptr, exe, MAX_PATH);
    std::string dir = exe;
    dir = dir.substr(0, dir.find_last_of("\\/"));
    if (HasTextures(dir + "\\data")) return dir + "\\data";
    if (HasTextures(dir + "\\..\\..\\data")) return dir + "\\..\\..\\data";
    return std::string();
}

std::vector<int> ParseList(const wchar_t* p) {
    std::vector<int> v;
    while (*p) {
        v.push_back(_wtoi(p));
        while (*p && *p != L',') ++p;
        if (*p) ++p;
    }
    return v;
}

// A scripted button press: from frame `frame` for `frames` frames, on pad `pad`.
struct Press {
    int frame, frames, pad;
    uint32_t button;
};

// "N[:[2]K[:M]],..." for --press.
std::vector<Press> ParsePresses(const wchar_t* p) {
    std::vector<Press> v;
    while (*p) {
        Press press = {_wtoi(p), 1, 0, platform::kStart};
        while (*p && *p != L',' && *p != L':') ++p;
        if (*p == L':' && p[1]) {
            if (p[1] == L'2') press.pad = 1, ++p;   // p[1] is the button now
            switch (p[1]) {
            case L'x': press.button = platform::kCross; break;
            case L'o': press.button = platform::kCircle; break;
            case L't': press.button = platform::kTriangle; break;
            case L'q': press.button = platform::kSquare; break;
            case L'u': press.button = platform::kUp; break;
            case L'd': press.button = platform::kDown; break;
            case L'l': press.button = platform::kLeft; break;
            case L'r': press.button = platform::kRight; break;
            case L'j': press.button = platform::kR2; break;
            case L'h': press.button = platform::kR1; break;
            case L'w': press.button = platform::kL1; break;
            case L'v': press.button = platform::kL2; break;
            case L'm': press.button = platform::kSelect; break;
            }
            p += p[1] ? 2 : 1;
            if (*p == L':') press.frames = _wtoi(++p);
            while (*p && *p != L',') ++p;
        }
        v.push_back(press);
        if (*p) ++p;
    }
    return v;
}

struct Options {
    float scale = 0;
    std::string aspect;
    bool hudCentered = false;
    bool lowGraphics = false;
    std::vector<int> shots;
    std::vector<Press> presses;
    int race = -1;
    bool factory = false;
    int advRace = -1;
    int fieldTown = -1;
    int roomTown = -1, roomPlace = 0;
    bool allParts = false;
    bool coinRadar = false;
    int myCity = -1;
    std::vector<int> stamps, flags, items;
    bool won = false;
    int gpStage = -1;
    int meter = -1;
    int partFlags = -1;
    bool beaten = false;
    std::string carTrace;
    int traceCar = 0;
    int autoDrive = -1;
    bool ranking = false;
    std::vector<std::string> extras;
    int clock = -1;
    bool place = false;
    int gate = -1;
    float placePos[3] = {};
    int placeYaw = 0;
    bool eye = false;
    float eyePos[3] = {};
    int eyeYaw = 0, eyePitch = 0;
    std::string keyLog;
    std::string perf;
    std::string wav;
    std::string shotPrefix;
    std::vector<uint32_t> sounds;
    int soundSeconds = 0;
    std::string soundOut;
    std::string saveIn, saveOut;   // --save-convert; --save-new leaves saveIn empty
};

Options ParseOptions() {
    Options o;
    int argc = 0;
    wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    for (int i = 1; i < argc; ++i) {
        std::wstring a = argv[i];
        if (a == L"--scale" && i + 1 < argc) {
            o.scale = (float)_wtof(argv[++i]);
        } else if (a == L"--aspect" && i + 1 < argc) {
            o.aspect = Narrow(argv[++i]);
        } else if (a == L"--hud" && i + 1 < argc) {
            o.hudCentered = Narrow(argv[++i]) == "centered";
        } else if (a == L"--graphics" && i + 1 < argc) {
            o.lowGraphics = Narrow(argv[++i]) == "low";
        } else if (a == L"--shot" && i + 2 < argc) {
            o.shots = ParseList(argv[++i]);
            o.shotPrefix = Narrow(argv[++i]);
        } else if (a == L"--press" && i + 1 < argc) {
            o.presses = ParsePresses(argv[++i]);
        } else if (a == L"--race" && i + 1 < argc) {
            o.race = _wtoi(argv[++i]);
        } else if (a == L"--factory") {
            o.factory = true;
        } else if (a == L"--advrace" && i + 1 < argc) {
            o.factory = true;
            o.advRace = _wtoi(argv[++i]);
        } else if (a == L"--field" && i + 1 < argc) {
            o.factory = true;
            o.fieldTown = _wtoi(argv[++i]);
        } else if (a == L"--room" && i + 2 < argc) {
            o.factory = true;
            o.roomTown = _wtoi(argv[++i]);
            o.roomPlace = _wtoi(argv[++i]);
        } else if (a == L"--ending" && i + 1 < argc) {
            o.factory = true;
            game::NewGame::SetEnding(_wtoi(argv[++i]));
        } else if (a == L"--allparts") {
            o.allParts = true;
        } else if (a == L"--coinradar") {
            o.coinRadar = true;
        } else if (a == L"--mycity" && i + 1 < argc) {
            o.myCity = (int)wcstoul(argv[++i], nullptr, 16);
        } else if (a == L"--stamp" && i + 1 < argc) {
            o.stamps = ParseList(argv[++i]);
        } else if (a == L"--flag" && i + 1 < argc) {
            o.flags = ParseList(argv[++i]);
        } else if (a == L"--item" && i + 1 < argc) {
            o.items = ParseList(argv[++i]);
        } else if (a == L"--won") {
            o.won = true;
        } else if (a == L"--gpstage" && i + 1 < argc) {
            o.gpStage = _wtoi(argv[++i]);
        } else if (a == L"--meter" && i + 1 < argc) {
            o.meter = _wtoi(argv[++i]);
        } else if (a == L"--partflags" && i + 1 < argc) {
            o.partFlags = (int)wcstoul(argv[++i], nullptr, 16);
        } else if (a == L"--beaten") {
            o.beaten = true;
        } else if (a == L"--cartrace" && i + 1 < argc) {
            o.carTrace = Narrow(argv[++i]);
        } else if (a == L"--tracecar" && i + 1 < argc) {
            o.traceCar = _wtoi(argv[++i]);
        } else if (a == L"--autodrive" && i + 1 < argc) {
            o.autoDrive = _wtoi(argv[++i]);
        } else if (a == L"--ranking") {
            o.ranking = true;
        } else if (a == L"--extra" && i + 1 < argc) {
            std::string list = Narrow(argv[++i]);
            for (size_t p = 0; p <= list.size();) {
                size_t e = list.find(',', p);
                if (e == std::string::npos) e = list.size();
                o.extras.push_back(list.substr(p, e - p));
                p = e + 1;
            }
        } else if (a == L"--clock" && i + 1 < argc) {
            o.clock = (int)wcstol(argv[++i], nullptr, 0);
        } else if (a == L"--gate" && i + 1 < argc) {
            o.gate = _wtoi(argv[++i]);
        } else if (a == L"--place" && i + 4 < argc) {
            o.place = true;
            for (int k = 0; k < 3; ++k) o.placePos[k] = (float)_wtof(argv[++i]);
            o.placeYaw = _wtoi(argv[++i]);
        } else if (a == L"--eye" && i + 5 < argc) {
            o.eye = true;
            for (int k = 0; k < 3; ++k) o.eyePos[k] = (float)_wtof(argv[++i]);
            o.eyeYaw = (int)wcstol(argv[++i], nullptr, 0);
            o.eyePitch = (int)wcstol(argv[++i], nullptr, 0);
        } else if (a == L"--keylog" && i + 1 < argc) {
            o.keyLog = Narrow(argv[++i]);
        } else if (a == L"--perf" && i + 1 < argc) {
            o.perf = Narrow(argv[++i]);
        } else if (a == L"--wav" && i + 1 < argc) {
            o.wav = Narrow(argv[++i]);
        } else if (a == L"--soundtest" && i + 3 < argc) {
            for (const wchar_t* q = argv[++i]; *q;) {
                wchar_t* e;
                o.sounds.push_back((uint32_t)wcstoul(q, &e, 16));
                q = *e ? e + 1 : e;
            }
            o.soundSeconds = _wtoi(argv[++i]);
            o.soundOut = Narrow(argv[++i]);
        } else if ((a == L"--save-convert" || a == L"--save-new") && i + 2 < argc) {
            o.saveIn = Narrow(argv[++i]);
            o.saveOut = Narrow(argv[++i]);
            if (a == L"--save-new") o.saveIn.insert(0, "*");
        }
    }
    LocalFree(argv);
    return o;
}

bool Contains(const std::vector<int>& v, int x) {
    for (int i : v)
        if (i == x) return true;
    return false;
}

// Writes part of a target to PREFIX_<frame>.png.
void SaveShot(gfx::D3D9Renderer& renderer, gfx::TargetId id, const gfx::Rect& r, const std::string& prefix,
              int frame) {
    std::vector<uint32_t> px;
    int w = 0, h = 0;
    if (!id || !renderer.ReadTarget(id, px, w, h)) return;
    int x0 = renderer.Px(r.x0), y0 = renderer.Px(r.y0);
    int cw = std::min(renderer.Px(r.x1) - x0, w - x0), ch = std::min(renderer.Px(r.y1) - y0, h - y0);
    std::vector<uint32_t> out((size_t)cw * ch);
    for (int y = 0; y < ch; ++y)
        for (int x = 0; x < cw; ++x) {
            uint32_t c = px[(size_t)(y0 + y) * w + x0 + x];   // B,G,R,A -> R,G,B, opaque
            out[(size_t)y * cw + x] = 0xFF000000u | (c & 0xFF00) | ((c >> 16) & 0xFF) | ((c & 0xFF) << 16);
        }
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s_%d.png", prefix.c_str(), frame);
    WritePng(path, out.data(), cw, ch);
}

// Writes 48 kHz 16-bit stereo samples as a WAV file.
bool WriteWav(const std::string& path, const std::vector<int16_t>& pcm) {
    FILE* fp = fopen(path.c_str(), "wb");
    if (!fp) return false;
    uint32_t bytes = (uint32_t)pcm.size() * 2;
    uint32_t hdr[11] = {0x46464952, 36 + bytes, 0x45564157, 0x20746D66, 16, 0x00020001, 48000, 48000 * 4, 0x00100004,
                        0x61746164, bytes};
    fwrite(hdr, 4, 11, fp);
    fwrite(pcm.data(), 2, pcm.size(), fp);
    fclose(fp);
    return true;
}

// --soundtest: renders the requests offline to a 48 kHz stereo WAV.
int SoundTest(const Options& opt, const std::string& data) {
    if (data.empty() || !audio::Sound_Init(data, false)) return 1;
    for (uint32_t id : opt.sounds) {
        if (id >> 16) audio::Sound_LoadSong(id);   // FFSSEE: file FF of the game's table into slot SS, entry EE
        else audio::Sound_Request(id);
        audio::Sound_StartSong();   // only does something if a song was selected
    }
    std::vector<int16_t> pcm((size_t)opt.soundSeconds * 48000 * 2);
    audio::Sound_Render(pcm.data(), (int)(pcm.size() / 2));
    audio::Sound_Shutdown();
    return WriteWav(opt.soundOut, pcm) ? 0 : 1;
}

// Runs `frame` until it returns false or the window closes: 60 a second while g_frameLimit is on and `paced`, else
// as fast as they go.
template <typename F> void RunFrames(bool paced, F frame) {
    LARGE_INTEGER freq, last, now;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&last);
    const LONGLONG period = freq.QuadPart / 60;
    MSG msg = {};
    while (g_running) {
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) g_running = false;
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        if (!g_running) break;
        if (paced && g_frameLimit) {
            QueryPerformanceCounter(&now);
            if (now.QuadPart - last.QuadPart < period) {
                Sleep(1);
                continue;
            }
            last.QuadPart += period;
            if (now.QuadPart - last.QuadPart > 4 * period) last = now;   // fell behind (debugger, drag): resync
        }
        if (!frame()) break;
    }
}

bool WriteSave(const std::string& path, const game::SaveRecord& r) {
    bool text = path.size() > 5 && _stricmp(path.c_str() + path.size() - 5, ".json") == 0;
    return text ? game::Save_WriteFile(path, r) : game::Save_WriteRaw(path, r);
}

// --perf: the log's first lines, '#' and then what the frames ran on: the build, the command line, Windows, the
// processor and memory, and the card and how the device was made.
void PerfHeader(FILE* f, const gfx::D3D9Renderer& renderer) {
    // The build: the exe's PE timestamp, which /Brepro (Directory.Build.props) makes a hash of its contents, so the
    // same sources give the same number on any machine.
    const BYTE* base = (const BYTE*)GetModuleHandleW(nullptr);
    const IMAGE_NT_HEADERS* nt = (const IMAGE_NT_HEADERS*)(base + ((const IMAGE_DOS_HEADER*)base)->e_lfanew);
    fprintf(f, "# RTAGlass --perf log, build %08lX\n", (unsigned long)nt->FileHeader.TimeDateStamp);
    fprintf(f, "# command line: %s\n", Narrow(GetCommandLineW()).c_str());
    // RtlGetVersion: GetVersionEx stops at 6.2 on Windows 8.1 and later for programs not manifested for them.
    OSVERSIONINFOEXW os = {};
    os.dwOSVersionInfoSize = sizeof(os);
    typedef LONG(WINAPI * RtlGetVersionFn)(OSVERSIONINFOEXW*);
    RtlGetVersionFn getVersion = (RtlGetVersionFn)GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "RtlGetVersion");
    if (getVersion) getVersion(&os);
    fprintf(f, "# windows: %lu.%lu build %lu %s\n", os.dwMajorVersion, os.dwMinorVersion, os.dwBuildNumber,
            Narrow(os.szCSDVersion).c_str());
    int regs[4];
    char brand[49] = {};
    __cpuid(regs, 0x80000000);
    if ((unsigned)regs[0] >= 0x80000004u)
        for (int k = 0; k < 3; ++k) {
            __cpuid(regs, 0x80000002 + k);
            memcpy(brand + 16 * k, regs, 16);
        }
    const char* name = brand;
    while (*name == ' ') ++name;
    for (size_t n = strlen(brand); n && brand[n - 1] == ' '; --n) brand[n - 1] = 0;
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    MEMORYSTATUSEX mem = {};
    mem.dwLength = sizeof(mem);
    GlobalMemoryStatusEx(&mem);
    fprintf(f, "# cpu: %s, %lu logical processors; memory %llu MB\n", *name ? name : "unknown",
            si.dwNumberOfProcessors, mem.ullTotalPhys >> 20);
    fprintf(f, "# gpu: %s\n", renderer.Describe().c_str());
}

// --save-convert IN OUT, --save-new P1 P2 (saveIn "*P1").
int SaveTool(const Options& opt) {
    int bad = game::Save_CheckFields();
    if (bad >= 0) {
        fprintf(stderr, "save fields: byte 0x%04X not covered once\n", bad);
        return 2;
    }
    if (opt.saveIn[0] == '*') {
        game::Save_InitBoth();
        bool ok = WriteSave(opt.saveIn.substr(1), game::Save_Record(0));
        return ok && WriteSave(opt.saveOut, game::Save_Record(1)) ? 0 : 1;
    }
    static game::SaveRecord r;
    if (!game::Save_ReadFile(opt.saveIn, r)) {
        fprintf(stderr, "can't read %s\n", opt.saveIn.c_str());
        return 1;
    }
    return WriteSave(opt.saveOut, r) ? 0 : 1;
}

}  // namespace

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE, LPWSTR, int showCmd) {
    Options opt = ParseOptions();
    if (!opt.sounds.empty()) return SoundTest(opt, FindDataDir());
    if (!opt.saveIn.empty()) return SaveTool(opt);
    bool scripted = !opt.shots.empty();
    int lastShot = 0;
    for (int f : opt.shots) lastShot = f > lastShot ? f : lastShot;

    // The display settings: display.json next to the executable, the defaults for scripted checks; the options'
    // for this run on top.
    {
        RECT work = {0, 0, 640, 480};
        SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
        platform::Display_SetMachine({}, {GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN)},
                                     {work.right - work.left, work.bottom - work.top});
    }
    platform::Display_Load(scripted ? std::string() : util::ExeDir() + "\\display.json");
    platform::DisplaySettings display = platform::Display_Get();
    if (opt.scale > 0) display.renderScale = opt.scale;
    static const char* const kAspectOptions[platform::kAspectCount] = {"auto", "4:3", "16:10", "16:9", "21:9"};
    for (int a = 0; a < platform::kAspectCount; ++a)
        if (opt.aspect == kAspectOptions[a]) display.aspect = a;
    if (opt.hudCentered) display.edgeHud = false;
    if (opt.lowGraphics) display.lowGraphics = true;
    if (scripted) display.mode = platform::kDisplayWindow;
    if (opt.scale > 0) {
        // --scale N: a window N x 480 high, as wide as the aspect makes it (4:3 for Auto).
        float aspect = platform::Display_AspectValue(display.aspect, {640, 480});
        display.mode = platform::kDisplayWindow;
        display.height = (int)(480 * opt.scale + 0.5f);
        display.width = (int)(display.height * aspect + 0.5f);
    }
    DisplayPlan plan = PlanDisplay(display, {});

    HWND hwnd = CreateMainWindow(inst);
    if (!hwnd) {
        MessageBoxW(nullptr, L"Failed to create window.", kWindowTitle, MB_ICONERROR);
        return 1;
    }
    PlaceWindow(hwnd, plan, display.resizable);
    std::unique_ptr<gfx::D3D9Renderer> renderer(new gfx::D3D9Renderer);
    bool full = plan.mode == platform::kDisplayFullscreen;
    g_frameLimit = display.frameLimit;
    bool made = renderer->Init(hwnd, plan.size.width, plan.size.height, plan.scale, !scripted && display.vsync, full);
    if (!made && full) {
        // The screen refused the mode: a window instead.
        renderer.reset(new gfx::D3D9Renderer);
        display.mode = platform::kDisplayWindow;
        display.width = display.height = 0;
        plan = PlanDisplay(display, {});
        PlaceWindow(hwnd, plan, display.resizable);
        made = renderer->Init(hwnd, plan.size.width, plan.size.height, plan.scale, !scripted && display.vsync);
    }
    if (!made) {
        MessageBoxW(hwnd, L"Failed to initialize Direct3D 9 (Shader Model 2.0 is required).", kWindowTitle,
                    MB_ICONERROR);
        DestroyWindow(hwnd);
        return 1;
    }
    // The card's modes for the Display screen, and a scale its largest texture allows.
    const std::vector<gfx::D3D9Renderer::Mode> modes = renderer->Modes();
    {
        std::vector<platform::DisplaySize> sizes;
        for (const gfx::D3D9Renderer::Mode& m : modes) sizes.push_back({m.width, m.height});
        gfx::D3D9Renderer::Mode desk = renderer->DesktopMode();
        RECT work = {0, 0, desk.width, desk.height};
        SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
        platform::Display_SetMachine(sizes, {desk.width, desk.height},
                                     {work.right - work.left, work.bottom - work.top});
    }
    float maxScale = (float)renderer->MaxScale(plan.frameWidth, gfx::Draw2D::kHeight);
    if (plan.scale > maxScale) renderer->Reset(plan.size.width, plan.size.height, maxScale, full, plan.refresh);

    gfx::TextureSet textures;
    std::string data = FindDataDir();
    std::string badPack = data.empty() ? std::string() : MountPacks(data);
    bool haveTextures = !data.empty() && badPack.empty() && textures.Load(data);
    timeBeginPeriod(1);
    ShowWindow(hwnd, showCmd);
    UpdateWindow(hwnd);

    if (!badPack.empty()) {
        std::wstring msg = L"A game data file is damaged:\n" + std::wstring(badPack.begin(), badPack.end()) +
                           L"\n\nCopy it again from the download.";
        MessageBoxW(hwnd, msg.c_str(), kWindowTitle, MB_ICONERROR);
        DestroyWindow(hwnd);
        return 1;
    }
    if (!haveTextures) {
        MessageBoxW(hwnd,
                    L"Game data not found. Keep the \"data\" folder next to RTAGlass.exe "
                    L"(a Release build puts both in dist\\RTAGlass).",
                    kWindowTitle, MB_ICONERROR);
        DestroyWindow(hwnd);
        return 1;
    }
    bool recording = scripted && !opt.wav.empty();
    if (!scripted) audio::Sound_Init(data);
    if (recording) audio::Sound_Init(data, false);
    std::vector<int16_t> recorded;
    // The controls: controls.json next to the executable, but the defaults for scripted checks.
    platform::Controls_Load(scripted ? std::string() : util::ExeDir() + "\\controls.json");
    // The extras: extras.json, all off for scripted checks but those --extra turns on.
    game::Extras_Load(scripted ? std::string() : util::ExeDir() + "\\extras.json");
    for (const std::string& name : opt.extras) {
        int e = game::Extra_FromName(name);
        if (e >= 0) game::Extras_Force((game::Extra)e);
    }
    platform::Input input;
    g_input = &input;
    std::unique_ptr<gfx::Draw2D> draw(new gfx::Draw2D(*renderer, &textures));
    draw->SetFrameWidth(plan.frameWidth);
    draw->SetEdgeAnchors(display.edgeHud);
    gfx::SetLowDetail(display.lowGraphics);
    std::unique_ptr<game::TitleSequence> title(new game::TitleSequence(*draw, data));
    if (!opt.carTrace.empty()) game::Race::SetTrace(opt.carTrace, opt.traceCar);
    if (opt.autoDrive >= 0) game::Race::SetAutoDrive(opt.autoDrive);
    if (opt.ranking) game::Race::SetRankingItem(true);
    if (opt.clock >= 0) {
        game::Race::SetClock(opt.clock);
        game::NewGame::SetClock(opt.clock);
    }
    if (opt.place) game::Race::SetPlace(opt.placePos[0], opt.placePos[1], opt.placePos[2], opt.placeYaw);
    if (opt.gate >= 0) game::Race::SetGate(opt.gate);
    if (opt.eye) game::Race::SetEye(opt.eyePos[0], opt.eyePos[1], opt.eyePos[2], opt.eyeYaw, opt.eyePitch);
    if (!opt.keyLog.empty() && (g_keyLog = fopen(opt.keyLog.c_str(), "w")) != nullptr)
        fprintf(g_keyLog, "time,message,key,lparam\n");
    if (opt.race >= 0) title->StartRace(opt.race);
    if (opt.factory) title->StartFactory(opt.advRace, opt.fieldTown, opt.roomTown, opt.roomPlace);
    if (opt.factory && opt.allParts) {
        // Five copies of every part, bodies 0-7, the Super A license, and player 1's teammates and their cars as
        // in 2 Player's preset (Save_InitVersusPreset).
        game::SaveRecord& r = game::Save_Record(0);
        for (int k = 1; k < 15; ++k)
            for (int j = 0; j < 5; ++j) r.items[k][j] = 0xFFFF;
        r.items[0][0] |= 0xFF;
        r.license = 3;
        static const int8_t kTeammates[2][2] = {{1, 0x21}, {7, 0x0B}};
        static const uint8_t kBodies[2] = {0x49, 0x68};
        for (int t = 0; t < 2; ++t) {
            r.teammates[t][0] = kTeammates[t][0];
            r.teammates[t][1] = kTeammates[t][1];
            r.cars[t + 1].body = kBodies[t];
            r.cars[t + 1].paint = r.bodyPaint[kBodies[t]];
        }
    }
    if (opt.factory && opt.coinRadar) game::Save_Record(0).items[15][0x2D >> 5] |= 1u << (0x2D & 31);
    if (opt.factory && opt.myCity >= 0) game::Save_Record(0).myCity = (uint32_t)opt.myCity;
    if (opt.factory)
        for (int s : opt.stamps) game::Stamp_Earn(s - 1);
    if (opt.factory)
        for (int f : opt.flags) game::Save_SetEventFlag(f, true);
    if (opt.factory)
        for (int n : opt.items) game::Item_Give(15, n);
    if (opt.factory && opt.gpStage >= 0) game::Save_Record(0).gpxStage = opt.gpStage;
    if (opt.meter >= 0) game::Save_Record(0).cars[0].parts[14] = (uint8_t)opt.meter;
    if (opt.partFlags >= 0) game::Save_Record(0).cars[0].partFlags = (uint16_t)opt.partFlags;
    if (opt.factory && opt.won)
        for (int k = 0; k < 26; ++k) game::Save_Record(0).adventurePlace[0][k] = 0;
    if (opt.factory && opt.beaten)
        for (uint64_t& w : game::Save_Record(0).beaten) w = ~0ull;
    int frame = 0;
    const char* shown = nullptr;
    FILE* perf = opt.perf.empty() ? nullptr : fopen(opt.perf.c_str(), "w");
    if (perf) {
        PerfHeader(perf, *renderer);
        fprintf(perf, "frame,screen,frame_ms,game_ms,draw_ms,upload_ms,present_ms,calls,world_calls,car_calls,"
                      "object_calls,effect_calls,other_calls,second_passes,vertices,copies,meshes_made,"
                      "textures_made,textures_written,place,id,x,y,z,yaw,clock\n");
    }
    LARGE_INTEGER perfFreq, perfLast;
    QueryPerformanceFrequency(&perfFreq);
    QueryPerformanceCounter(&perfLast);
    RunFrames(!scripted, [&]() {
        LARGE_INTEGER t0, t1;
        QueryPerformanceCounter(&t0);
        unsigned long long tsc0 = __rdtsc();
        // The Display screen applied new settings (or Alt+Enter): made so before the frame.
        TakeResize(hwnd, *renderer, *draw, modes);
        if (platform::Display_TakeChange()) ApplyDisplay(hwnd, *renderer, *draw, modes);
        renderer->stats = gfx::D3D9Renderer::Stats();
        game::Perf_Where() = game::PerfWhere();
        // The root task (func_00205088) steps the pads' vibration before it reads them and the game runs, so a
        // shake asked for this frame starts on the next.
        game::Pad_VibrationTask();
        for (int pad = 0; pad < 2; ++pad) {
            uint8_t smallMotor, largeMotor;
            game::Pad_Motors(pad, smallMotor, largeMotor);
            input.SetMotors(pad, smallMotor, largeMotor);
        }
        input.Update();
        for (const Press& p : opt.presses) {
            if (p.frames <= 1 && p.frame == frame)
                input.Script(p.pad, p.button, true), input.Update(), input.Script(p.pad, p.button, false);
            if (p.frames > 1 && p.frame == frame) input.Script(p.pad, p.button, true), input.Update();
            if (p.frames > 1 && p.frame + p.frames == frame) input.Script(p.pad, p.button, false), input.Update();
        }
        title->Update(input);
        if (perf) {
            // The update (the game, its draws and Present) split by where its time went: the renderer's draws and
            // uploads in time stamp counter ticks, scaled by the update's own; Present by the clock; the game the
            // rest. frame_ms is from this frame's start to the last one's.
            QueryPerformanceCounter(&t1);
            unsigned long long tsc = __rdtsc() - tsc0;
            const gfx::D3D9Renderer::Stats& s = renderer->stats;
            double updateMs = (t1.QuadPart - t0.QuadPart) * 1000.0 / perfFreq.QuadPart;
            double perTick = tsc ? updateMs / (double)tsc : 0;
            double drawMs = s.drawTicks * perTick, uploadMs = s.uploadTicks * perTick;
            double gameMs = updateMs - drawMs - uploadMs - s.presentMs;
            fprintf(perf, "%d,%s,%.2f,%.2f,%.2f,%.2f,%.2f,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d", frame,
                    title->StateName(), (t0.QuadPart - perfLast.QuadPart) * 1000.0 / perfFreq.QuadPart,
                    gameMs > 0 ? gameMs : 0, drawMs, uploadMs, s.presentMs, s.calls,
                    s.callsBy[(int)gfx::DrawSource::World], s.callsBy[(int)gfx::DrawSource::Cars],
                    s.callsBy[(int)gfx::DrawSource::Objects], s.callsBy[(int)gfx::DrawSource::Effects],
                    s.callsBy[(int)gfx::DrawSource::Other], s.secondPasses, s.vertices, s.copies, s.meshesMade,
                    s.texturesMade, s.texturesWritten);
            const game::PerfWhere& w = game::Perf_Where();
            if (w.mode)
                fprintf(perf, ",%s,%d,%.1f,%.1f,%.1f,%d,%d\n", w.mode, w.id, w.pos[0], w.pos[1], w.pos[2], w.yaw,
                        (int)w.clock);
            else
                fprintf(perf, ",,,,,,,\n");
            perfLast = t0;
        }
        if (recording) {
            size_t at = recorded.size();
            recorded.resize(at + 800 * 2);
            audio::Sound_Render(&recorded[at], 800);
        }
        gfx::Rect r = {0, 0, draw->FrameWidth(), gfx::Draw2D::kHeight};
        if (Contains(opt.shots, frame))
            SaveShot(*renderer, draw->Shown(), r, opt.shotPrefix, frame);
        if (scripted && frame >= lastShot) return false;
        // With --perf the window's title names the screen, to match up with the file's rows.
        if (perf && title->StateName() != shown) {
            shown = title->StateName();
            wchar_t t[128];
            swprintf(t, 128, L"RTAGlass - %S", shown);
            SetWindowTextW(hwnd, t);
        }
        ++frame;
        return true;
    });
    if (perf) fclose(perf);
    title.reset();
    draw.reset();
    g_input = nullptr;
    audio::Sound_Shutdown();
    if (recording) WriteWav(opt.wav, recorded);

    timeEndPeriod(1);
    renderer.reset();
    if (IsWindow(hwnd)) DestroyWindow(hwnd);
    return 0;
}
