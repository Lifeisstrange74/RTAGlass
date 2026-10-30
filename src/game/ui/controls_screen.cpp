#include "controls_screen.h"

#include <windows.h>

#include <math.h>
#include <string.h>

#include "../../audio/sound.h"
#include "../field/field_menu_draw.h"
#include "../race/race_light.h"
#include "../text.h"
#include "../../game_text.inc"   // the game's text, GT_<address>_<length>, made from the ELF

namespace game {

using namespace menu;
using namespace platform;

namespace {

// The rows' labels. The driving page's are the PS2 Settings' (D_0029C618) with the steering first.
const char* const kActionLabels[kActionCount] = {
    "Steer Left", "Steer Right", GT_002EA9C8_9, GT_002EA9B8_11, GT_00333518_7, GT_002EA9A8_8, GT_002EA998_11,
    GT_002EA988_13, GT_00333510_7, GT_002EA978_9, GT_002EA968_9,
    "Up", GT_002EA97D_4, "Left", GT_002EA28D_5, "Confirm \x81\x7E", "Back \x81\xA2", GT_00334EA0_5,
};
const char* const kPageNames[4] = {"< 1P Driving >", "< 1P Menus >", "< 2P Driving >", "< 2P Menus >"};

// The layout (frame lines). The PS2's list is 24 lines a row from y 88; this one has more rows, so 22 a row, the
// page and the columns' heads share the first, and the pad sits further right to make room for the two columns.
const int kListX = 40, kListTop = 84, kListW = 324, kListBottom = 422;
const int kLabelX = 60, kKeyX = 258, kPadX = 330;   // the columns' centres
const int kPageY = 86, kHeadY = 86, kFirstY = 112, kRowStep = 22, kActionsY = 352;
const int kHelpX = 380, kHelpTop = 280, kHelpW = 240, kHelpBottom = 400;
const float kPadCentreX = 500, kPadCentreY = 164;
const int kCaptureFrames = 600;   // ten seconds to press the key

// The controller's buttons on the pad: the box's place from its centre and the lean towards it (D_0029C538's, by
// the PS2 button in the same place), and OPTION2's icon (-1: the name instead). Left, Right, Start and the sticks
// have no entry in the PS2 table; their places were measured off the drawn model.
struct PadPlace {
    int dx, dy;
    int16_t rot[3];
    int icon;
};
const PadPlace kPadPlaces[kPadInputCount] = {
    {0, 0, {0, 0, 0}, -1},
    {79, 18, {0, 0, 0}, 1},                // A: cross
    {98, 0, {0, 0, 0}, 0},                 // B: circle
    {60, 0, {0, 0, 0}, 2},                 // X: square
    {79, -18, {0, 0, 0}, 3},               // Y: triangle
    {-78, -28, {-8192, -1024, 0}, 6},      // LB: L1
    {78, -28, {-8192, 1024, 0}, 4},        // RB: R1
    {-80, -32, {-12288, -1024, 0}, 7},     // LT: L2
    {80, -32, {-12288, 1024, 0}, 5},       // RT: R2
    {-24, 0, {0, 0, 0}, 10},               // Back: SELECT
    {24, 0, {0, 0, 0}, -1},                // Start
    {-46, 34, {0, 0, 0}, -1},              // LS
    {41, 34, {0, 0, 0}, -1},               // RS
    {-79, -14, {0, 0, 0}, 8},              // d-pad up
    {-79, 14, {0, 0, 0}, 9},               // d-pad down
    {-96, 0, {0, 0, 0}, -1},               // d-pad left
    {-64, 0, {0, 0, 0}, -1},               // d-pad right
};

struct HelpLine {
    const char* text;
    int color;
};
// D_0029C9B0's, and the PC's own.
const HelpLine kHelpPage[] = {{"Left and Right:", 0}, {"player 1 or 2,", 0}, {"driving or", 0}, {"menus", 0}, {}};
const HelpLine kHelpKey[] = {{"Press \x81\x7E, then", 0}, {"the new key", 0}, {}};
const HelpLine kHelpPad1[] = {{"Press \x81\x7E, then", 0}, {"the new button", 0}, {"on controller 1", 0}, {}};
const HelpLine kHelpPad2[] = {{"Press \x81\x7E, then", 0}, {"the new button", 0}, {"on controller 2", 0}, {}};
const HelpLine kHelpClash[] = {{GT_002EA9F8_14, 6}, {GT_002EA9E8_9, 0}, {GT_002EA9D8_12, 0}, {}};
const HelpLine kHelpNoKey[] = {{GT_002EA9F8_14, 6}, {"No key set", 0}, {}};
const HelpLine kHelpPressKey[] = {{"Press a key", 0}, {"Esc: cancel", 0}, {"Del: clear", 0}, {}};
const HelpLine kHelpPressPad1[] = {{"Press a button", 0}, {"on controller 1", 0}, {"Esc: cancel", 0}, {"Del: clear", 0}, {}};
const HelpLine kHelpPressPad2[] = {{"Press a button", 0}, {"on controller 2", 0}, {"Esc: cancel", 0}, {"Del: clear", 0}, {}};
const HelpLine kHelpInitial[] = {{"Changes this", 0}, {"page back to", 0}, {GT_00308330_11, 0}, {"setting", 0}, {}};
const HelpLine kHelpCancel[] = {{GT_00333528_7, 0}, {GT_002EAA68_13, 0}, {GT_002EAA58_8, 0}, {}};
const HelpLine kHelpApply[] = {{GT_002EAA48_13, 0}, {}};
const HelpLine kHelpNoApply[] = {
    {GT_002EA9F8_14, 6}, {GT_002EA9E8_9, 0}, {GT_002EA9D8_12, 0}, {GT_002EAA08_12, 0}, {}};
const HelpLine kHelpNoApplyKey[] = {{GT_002EA9F8_14, 6}, {"No key set", 0}, {GT_002EAA08_12, 0}, {}};

const int kCapture = 0x70;   // the state waiting for the key or button (the PS2 screen has none)

// Text_PrintAt with the pad symbols ("~" ...) in palette 1, grey buttons as Help_Draw has them, whatever
// the text's colour.
int PrintWithSymbols(gfx::Draw2D& d, int x, int y, const char* s, int color) {
    char part[64];
    while (*s) {
        bool symbol = (unsigned char)*s >= 0x80 && s[1];
        size_t n = symbol ? 2 : 0;
        if (!symbol)
            while (s[n] && (unsigned char)s[n] < 0x80 && n + 1 < sizeof part) ++n;
        memcpy(part, s, n);
        part[n] = 0;
        x = Text_PrintAt(d, x, y, part, symbol ? 1 : color);
        s += n;
    }
    return x;
}

bool Driving(int page) { return (page & 1) == 0; }
int FirstAction(int page) { return Driving(page) ? 0 : kDrivingActions; }
int ActionCount(int page) { return Driving(page) ? kDrivingActions : kActionCount - kDrivingActions; }

}  // namespace

void ControlsScreen::Start() {
    state_ = count_ = carStep_ = 0;
    page_ = row_ = col_ = 0;
    horn_ = false;
    cancelOpen_ = false;
    rot_[0] = rot_[1] = rot_[2] = 0;
    edit_ = Controls_Get();
    CheckClashes();
}

int ControlsScreen::Rows() const { return 1 + ActionCount(page_) + 3; }

int ControlsScreen::RowAction(int row) const {
    return row >= 1 && row <= ActionCount(page_) ? FirstAction(page_) + row - 1 : -1;
}

int ControlsScreen::RowY(int row) const {
    int n = ActionCount(page_);
    return row == 0 ? kPageY : row <= n ? kFirstY + kRowStep * (row - 1) : kActionsY + kRowStep * (row - n - 1);
}

// As Settings_Update's check: a key or button that two actions share is a clash. Keys are checked across both
// players (one keyboard), buttons within a player (a controller each), the driving and the menu actions apart.
// Player 1's menu actions need a key: without one the keyboard couldn't work the menus.
void ControlsScreen::CheckClashes() {
    memset(warn_, 0, sizeof warn_);
    for (int p = 0; p < 2; ++p)
        for (int a = 0; a < kActionCount; ++a) {
            bool driving = a < kDrivingActions;
            int first = driving ? 0 : kDrivingActions, last = driving ? kDrivingActions : kActionCount;
            int key = edit_.player[p].key[a], in = edit_.player[p].pad[a];
            for (int q = 0; q < 2; ++q)
                for (int b = first; b < last; ++b) {
                    if (q == p && b == a) continue;
                    if (key && edit_.player[q].key[b] == key) warn_[p][a][0] = 1;
                    if (q == p && in && edit_.player[q].pad[b] == in) warn_[p][a][1] = 1;
                }
            if (p == 0 && !driving && !key) warn_[p][a][0] = 2;
        }
}

bool ControlsScreen::AnyWarning() const {
    for (int p = 0; p < 2; ++p)
        for (int a = 0; a < kActionCount; ++a)
            if (warn_[p][a][0] || warn_[p][a][1]) return true;
    return false;
}

bool ControlsScreen::Changed() const { return memcmp(&edit_, &Controls_Get(), sizeof edit_) != 0; }

// Settings_Update's machine (func_00214470), the rows Left and Right acted on now pages and columns, and Cross on
// an action's key or button waiting for the new one.
int ControlsScreen::UpdateState(const platform::Input& input) {
    carStep_ = 0;
    switch (state_) {
    case 0:
    case 1:
        count_ = 16;
        state_ = 2;
    // fall through
    case 2:
        carStep_ = -kDrive;
        if (--count_ == 0) state_ = kListReady;
        break;
    case kListReady:
        horn_ = true;
        state_ = kListIdle;
    // fall through
    case kListIdle: {
        row_ = Menu_MoveCursor(input, 0, row_, Rows(), false);
        uint32_t pressed = input.Pressed(0);
        int n = ActionCount(page_);
        if (pressed & kTriangle) {
            audio::Sound_PlaySe(0x1B);
            horn_ = false;
            if (!Changed()) {
                state_ = kListExit;
            } else {
                cancel_ = Window();
                cancel_.AddLine(GT_00333558_7, 0);
                cancel_.AddLine("Yes", kLineSelectable);
                cancel_.AddLine("No", kLineSelectable);
                cancel_.sel = 1;
                cancel_.y = 0x9E;
                cancel_.x = (short)(320 - cancel_.w / 2);
                cancelOpen_ = true;
                row_ = n + 2;
                state_ = kListMove;
            }
        } else if (row_ == 0) {
            page_ = Menu_MoveCursorGrid(input, 0, page_, 4, 1, false);
        } else if (row_ <= n) {
            col_ = Menu_MoveCursorGrid(input, 0, col_, 2, 1, false);
            if (pressed & kCross) {
                audio::Sound_PlaySe(0x1A);
                captureFrames_ = kCaptureFrames;
                Controls_SetCapturing(true);
                state_ = kCapture;
            }
        } else if (pressed & kCross) {
            if (row_ == n + 1) {   // Initial settings: this page
                audio::Sound_PlaySe(0x1A);
                Controls_DefaultPage(edit_, page_ >> 1, Driving(page_));
                CheckClashes();
            } else if (row_ == n + 2) {   // Cancel change
                audio::Sound_PlaySe(0x1A);
                edit_ = Controls_Get();
                CheckClashes();
            } else if (AnyWarning()) {   // Apply
                audio::Sound_PlaySe(0x1E);
            } else {
                audio::Sound_PlaySe(0x1A);
                Controls_Apply(edit_);
                horn_ = false;
                state_ = kListExit;
            }
        }
        break;
    }
    case kCapture:
        Capture(input);
        break;
    case kListMove:
        // The Cancel window: Yes leaves with the changes lost, No (or Triangle) goes back to the list.
        if (!cancelOpen_) {
            if (cancel_.sel == 0) {
                audio::Sound_PlaySe(0x1A);
                state_ = kListExit;
            } else {
                audio::Sound_PlaySe(0x1B);
                state_ = kListReady;
            }
        }
        break;
    case kListExit:
        count_ = 16;
        state_ = kListExitOut;
    // fall through
    case kListExitOut:
        carStep_ = kDrive;
        if (--count_ == 0) state_ = kListLeft;
        break;
    default:
        state_ = count_ = 0;
        break;
    }
    return state_;
}

// Waiting for the key (any but Esc, which cancels; Delete clears it) or the player's controller's button.
void ControlsScreen::Capture(const platform::Input& input) {
    int player = page_ >> 1, action = RowAction(row_);
    int key = input.KeyPressed(), got = -1;
    if (key == VK_ESCAPE || --captureFrames_ == 0) {
        audio::Sound_PlaySe(0x1B);
    } else if (key == VK_DELETE) {
        got = 0;
    } else if (col_ == 0 && key && Key_Bindable(key)) {
        got = key;
    } else if (col_ == 1 && input.ControllerPressed(player)) {
        uint32_t bits = input.ControllerPressed(player);
        got = 1;
        while (!(bits & 1)) bits >>= 1, ++got;
    } else {
        return;
    }
    if (got >= 0) {
        audio::Sound_PlaySe(0x1A);
        if (col_ == 0) edit_.player[player].key[action] = (uint8_t)got;
        else edit_.player[player].pad[action] = (uint8_t)got;
        CheckClashes();
    }
    Controls_SetCapturing(false);
    state_ = kListIdle;
}

// The pad as Settings_DrawPad has it (func_00216738): the menus' light, a camera 2 back looking along +z with focal
// 3564; it leans towards the row's controller button a step (0x200) a frame on each axis, M = Rx Ry Rz.
void ControlsScreen::DrawPad(gfx::Draw2D& d, int input) {
    if (!padLoaded_) {
        pad_.Load(dataDir_ + "\\models\\SYS\\OPTION\\0.obj", d);
        padLoaded_ = true;
    }
    const int16_t* target = kPadPlaces[input].rot;
    for (int k = 0; k < 3; ++k) {
        if (rot_[k] < target[k]) rot_[k] = (int16_t)(rot_[k] + 0x200);
        else if (rot_[k] > target[k]) rot_[k] = (int16_t)(rot_[k] - 0x200);
    }
    float c[3], s[3];
    for (int k = 0; k < 3; ++k) {
        float a = rot_[k] * 3.14159265f / 32768.f;
        c[k] = cosf(a);
        s[k] = sinf(a);
    }
    const float rx[3][3] = {{1, 0, 0}, {0, c[0], -s[0]}, {0, s[0], c[0]}};
    const float ry[3][3] = {{c[1], 0, s[1]}, {0, 1, 0}, {-s[1], 0, c[1]}};
    const float rz[3][3] = {{c[2], -s[2], 0}, {s[2], c[2], 0}, {0, 0, 1}};
    float xy[3][3];
    float transform[12];
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j) {
            xy[i][j] = 0;
            for (int k = 0; k < 3; ++k) xy[i][j] += rx[i][k] * ry[k][j];
        }
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            transform[i * 4 + j] = 0;
            for (int k = 0; k < 3; ++k) transform[i * 4 + j] += xy[i][k] * rz[k][j];
        }
        transform[i * 4 + 3] = 0;
    }
    gfx::Camera3D cam = {};
    cam.eye[2] = -2.0f;
    cam.right[0] = cam.up[1] = cam.forward[2] = 1.0f;
    cam.focalX = 3564.0f;
    cam.focalY = 3564.0f * 0.94f;
    cam.centerX = kPadCentreX;
    cam.centerY = kPadCentreY;
    RaceLight light;
    Light_SetupMenu(light);
    const float forward[3] = {0, 0, 1};
    Light_SetHighlight(light, forward);
    gfx::Lighting lit = Light_CarSet(light, 0);
    gfx::DrawStyle style;
    style.transform = transform;
    style.lighting = &lit;
    pad_.Draw(d, cam, gfx::Rect{0, 0, gfx::Draw2D::kWidth, gfx::Draw2D::kHeight}, style);
}

void ControlsScreen::Draw(gfx::Draw2D& d) {
    int player = page_ >> 1, n = ActionCount(page_), first = FirstAction(page_), action = RowAction(row_);
    const Bindings& b = edit_.player[player];
    // The list (style 2) and the help (style 0), as the PS2's, the list wider and taller.
    Window_DrawFrame(d, kListX, kListTop / 2, kListW, (kListBottom - kListTop) / 2, 2);
    Window_DrawFrame(d, kHelpX, kHelpTop / 2, kHelpW, (kHelpBottom - kHelpTop) / 2, 0);
    Window_DrawBar(d, 60, RowY(row_) - 2, kListW - 20);
    if (horn_) Window_DrawCursor(d, 20, RowY(row_) - 2);
    // The page, with a clash on another page shown in its colour.
    bool otherWarn = false;
    for (int p = 0; p < 4; ++p)
        for (int a = FirstAction(p); a < FirstAction(p) + ActionCount(p); ++a)
            if (p != page_ && (warn_[p >> 1][a][0] || warn_[p >> 1][a][1])) otherWarn = true;
    Text_PrintAt(d, kLabelX, kPageY, kPageNames[page_], otherWarn ? 6 : 0);
    Text_PrintAt(d, kKeyX - Text_Width("Key") / 2, kHeadY, "Key", 10);
    Text_PrintAt(d, kPadX - Text_Width("Pad") / 2, kHeadY, "Pad", 10);
    const gfx::Texture& icons = d.Get("SYS/OPTION2.GSL", 0);
    bool blink = (frame_ & 0x10) != 0;
    for (int r = 1; r <= n; ++r) {
        int a = first + r - 1, y = RowY(r);
        bool waiting = state_ == kCapture && r == row_;
        PrintWithSymbols(d, kLabelX, y, kActionLabels[a], warn_[player][a][0] || warn_[player][a][1] ? 6 : 0);
        const char* key = waiting && col_ == 0 ? (blink ? "?" : "") : Key_Name(b.key[a]);
        if (!*key && !(waiting && col_ == 0)) key = "-";
        Text_PrintAt(d, kKeyX - Text_Width(key) / 2, y, key, warn_[player][a][0] ? 6 : 0);
        int in = b.pad[a];
        if (waiting && col_ == 1) {
            if (blink) Text_PrintAt(d, kPadX - Text_Width("?") / 2, y, "?", 0);
        } else if (kPadPlaces[in].icon >= 0) {
            int id = kPadPlaces[in].icon;
            d.Sprite(icons, kPadX - 20.0f, (float)y, 40, 20, 128.0f + 40 * (id / 6), 20.0f * (id % 6), 40, 20);
        } else {
            const char* name = in ? PadInput_ShortName(in) : "-";
            Text_PrintAt(d, kPadX - Text_Width(name) / 2, y, name, warn_[player][a][1] ? 6 : 0);
        }
    }
    static const char* const kActions[3] = {GT_002EA950_16, GT_002EA940_13, GT_00333508_5};
    for (int k = 0; k < 3; ++k)
        Text_PrintAt(d, kLabelX, RowY(n + 1 + k), kActions[k], k == 2 && AnyWarning() ? 6 : 0);
    // The help for the row.
    bool noKey = false, clash = false;
    for (int p = 0; p < 2; ++p)
        for (int a = 0; a < kActionCount; ++a)
            for (int c = 0; c < 2; ++c) {
                if (warn_[p][a][c] == 1) clash = true;
                if (warn_[p][a][c] == 2) noKey = true;
            }
    const HelpLine* help;
    if (state_ == kCapture) help = col_ == 0 ? kHelpPressKey : player ? kHelpPressPad2 : kHelpPressPad1;
    else if (row_ == 0) help = kHelpPage;
    else if (action >= 0 && warn_[player][action][col_] == 2) help = kHelpNoKey;
    else if (action >= 0 && warn_[player][action][col_]) help = kHelpClash;
    else if (action >= 0) help = col_ == 0 ? kHelpKey : player ? kHelpPad2 : kHelpPad1;
    else if (row_ == n + 1) help = kHelpInitial;
    else if (row_ == n + 2) help = kHelpCancel;
    else help = clash ? kHelpNoApply : noKey ? kHelpNoApplyKey : kHelpApply;
    for (int k = 0; help[k].text; ++k)
        PrintWithSymbols(d, kHelpX + 20, kHelpTop + 12 + 24 * k, help[k].text, help[k].color);
    // The pad, leaning towards the row's button, and the box turning round that button (as the PS2's, a turn every
    // 64 frames from -pi); a still box round the chosen key or button in the list.
    int in = action >= 0 ? b.pad[action] : 0;
    DrawPad(d, in);
    static const float kBox[5][2] = {{-10, -10}, {-10, 10}, {10, 10}, {10, -10}, {-10, -10}};
    static const uint32_t kBoxColor[5] = {0x800000FF, 0x8000FFFF, 0x800000FF, 0x8000FFFF, 0x800000FF};
    float px[5], py[5];
    if (in) {
        float an = ((frame_ & 0x3F) - 32) * 3.14159265f / 32.f, ca = cosf(an), sa = sinf(an);
        for (int k = 0; k < 5; ++k) {
            px[k] = kPadCentreX + kPadPlaces[in].dx + (float)(int)(kBox[k][0] * ca - kBox[k][1] * sa);
            py[k] = kPadCentreY + kPadPlaces[in].dy + (float)(int)(kBox[k][0] * sa + kBox[k][1] * ca);
        }
        Line_DrawStrip(d, px, py, kBoxColor, 5);
    }
    if (action >= 0 && horn_) {
        float cx = col_ == 0 ? (float)kKeyX : (float)kPadX, cy = RowY(row_) + 10.0f;
        for (int k = 0; k < 5; ++k) {
            px[k] = cx + kBox[k][0] * 3.3f;
            py[k] = cy + kBox[k][1] * 1.1f;
        }
        Line_DrawStrip(d, px, py, kBoxColor, 5);
    }
}

int ControlsScreen::Update(gfx::Draw2D& d, const platform::Input& input) {
    int state = UpdateState(input);
    Draw(d);
    // The Cancel window over it all (Settings_CancelTask, a child task that runs after the screen).
    if (cancelOpen_) {
        int r = cancel_.Update(d, input);
        if (r != 0) {
            if (r == -1) cancel_.sel = 1;
            cancelOpen_ = false;
        }
    }
    ++frame_;
    return state;
}

}  // namespace game
