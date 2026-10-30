#include "display_screen.h"

#include <stdio.h>
#include <string.h>

#include "../../audio/sound.h"
#include "../field/field_menu_draw.h"
#include "../text.h"
#include "../../game_text.inc"   // the game's text, GT_<address>_<length>, made from the ELF

namespace game {

using namespace menu;
using namespace platform;

namespace {

struct HelpLine {
    const char* text;
    int color;
};
const char* const kLabels[9] = {"Display",      "Resizable", "Resolution",    "Aspect Ratio", "HUD",
                                "Render Scale", "VSync",     "Frame Limiter", "Graphics"};
const HelpLine kHelp[9][9] = {
    {{"Window, Full", 0}, {"Screen (the", 0}, {"screen changes to", 0}, {"the resolution),", 0},
     {"or Borderless (a", 0}, {"window over the", 0}, {"whole desktop).", 0}, {"Alt+Enter: window", 0},
     {"or Borderless.", 0}},
    {{"On: drag the", 0}, {"window's edges to", 0}, {"any size, or", 0}, {"maximize it. The", 0},
     {"size is kept for", 0}, {GT_0032C2A0_10, 0}},
    {{"The window's size,", 0}, {"or the screen's", 0}, {"mode. Auto: the", 0}, {"largest 4:3 window", 0},
     {"that fits, or the", 0}, {"desktop's size.", 0}},
    {{"4:3 is the PS2's", 0}, {"picture. Wider", 0}, {"shows more of the", 0}, {"road to the sides.", 0},
     {"Auto: the window's", 0}, {"or screen's shape.", 0}, {"Pictures stay 4:3.", 0}},
    {{"On a wide picture:", 0}, {"Edges puts the", 0}, {"HUD's corners at", 0}, {"its edges, Centered", 0},
     {"keeps them to the", 0}, {"4:3 middle.", 0}},
    {{"The picture is", 0}, {"drawn at this many", 0}, {"times 640 x 448.", 0}, {"Higher is sharper,", 0},
     {"under 1x faster", 0}, {"on slow machines.", 0}, {"Auto fits the", 0}, {GT_0032964A_7, 0}},
    {{"On: each picture", 0}, {"waits for the", 0}, {"screen's refresh,", 0}, {"so it never tears.", 0},
     {"Off: shown at once", 0}, {"(may tear).", 0}},
    {{"On: 60 frames a", 0}, {"second, the game's", 0}, {"own speed.", 0}, {"Off: as fast as the", 0},
     {"PC goes. The game", 0}, {"itself speeds up,", 0}, {"like an emulator's", 0}, {"turbo.", 0}},
    {{"Full: the PS2's", 0}, {"picture. Low, for", 0}, {"slow PCs: a shorter", 0}, {"view, no motion", 0},
     {"trail or car", 0}, {"reflections, and", 0}, {"simpler cars", 0}, {"farther away.", 0}},
};
const HelpLine kHelpInitial[] = {{"The PS2's picture:", 0}, {"a 4:3 window", 0}, {}};
const HelpLine kHelpCancel[] = {{GT_00333528_7, 0}, {GT_002EAA68_13, 0}, {GT_002EAA58_8, 0}, {}};
const HelpLine kHelpApply[] = {{GT_002EAA48_13, 0}, {}};

// The layout (frame lines), the Extras screen's.
const int kListX = 40, kListTop = 84, kListW = 300, kListBottom = 422;
const int kLabelX = 60, kValueX = 280;   // the value's centre
const int kFirstY = 86, kRowStep = 24, kActionsY = 346;
const int kHelpX = 360, kHelpTop = 84, kHelpW = 260, kHelpBottom = 422;

void PrintHelp(gfx::Draw2D& d, int y, const HelpLine* help, int n) {
    for (int k = 0; k < n && help[k].text; ++k, y += 24)
        Text_PrintAt(d, kHelpX + 20, y, help[k].text, help[k].color);
}

// The Render Scale row's choice the setting has.
int ScaleIndex(float scale) {
    for (int i = 0; i < kRenderScaleCount; ++i)
        if (kRenderScales[i] == scale) return i;
    return 0;
}

}  // namespace

void DisplayScreen::Start() {
    state_ = count_ = carStep_ = 0;
    row_ = 0;
    horn_ = false;
    cancelOpen_ = false;
    edit_ = Display_Get();
}

int DisplayScreen::RowY(int row) const {
    return row < kSettingRows ? kFirstY + kRowStep * row : kActionsY + 24 * (row - kSettingRows);
}

bool DisplayScreen::Changed() const { return !Display_Same(edit_, Display_Get()); }

std::vector<DisplaySize> DisplayScreen::Sizes() const {
    std::vector<DisplaySize> sizes = Display_Sizes(edit_.mode);
    if (edit_.mode != kDisplayBorderless) sizes.insert(sizes.begin(), DisplaySize{0, 0});
    return sizes;
}

int DisplayScreen::SizeIndex(const std::vector<DisplaySize>& sizes) const {
    for (size_t i = 0; i < sizes.size(); ++i)
        if (sizes[i].width == edit_.width && sizes[i].height == edit_.height) return (int)i;
    return 0;
}

void DisplayScreen::ChangeValue(int row, int step) {
    auto Cycle = [&](int v, int n) { return (v + step + n) % n; };
    switch (row) {
    case kRowResizable: edit_.resizable = !edit_.resizable; break;
    case kRowMode: {
        edit_.mode = Cycle(edit_.mode, kDisplayModeCount);
        // A size the new mode doesn't offer goes back to Auto.
        std::vector<DisplaySize> sizes = Sizes();
        if (edit_.mode == kDisplayBorderless || SizeIndex(sizes) == 0) edit_.width = edit_.height = 0;
        break;
    }
    case kRowSize: {
        std::vector<DisplaySize> sizes = Sizes();
        if (sizes.size() < 2) return;
        const DisplaySize& s = sizes[Cycle(SizeIndex(sizes), (int)sizes.size())];
        edit_.width = s.width;
        edit_.height = s.height;
        break;
    }
    case kRowAspect: edit_.aspect = Cycle(edit_.aspect, kAspectCount); break;
    case kRowHud: edit_.edgeHud = !edit_.edgeHud; break;
    case kRowVsync: edit_.vsync = !edit_.vsync; break;
    case kRowLimit: edit_.frameLimit = !edit_.frameLimit; break;
    case kRowGraphics: edit_.lowGraphics = !edit_.lowGraphics; break;
    case kRowScale: edit_.renderScale = kRenderScales[Cycle(ScaleIndex(edit_.renderScale), kRenderScaleCount)]; break;
    }
    audio::Sound_PlaySe(0x1D);
}

// The Extras screen's machine: the title car in, the rows, "Cancel?" when leaving with changes, the title car out.
int DisplayScreen::UpdateState(const platform::Input& input) {
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
        uint32_t pressed = input.Pressed(0), repeated = input.Repeated(0);
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
                row_ = kSettingRows + 1;
                state_ = kListMove;
            }
        } else if (row_ < kSettingRows) {
            if (repeated & kLeft) ChangeValue(row_, -1);
            else if ((repeated & kRight) || (pressed & kCross)) ChangeValue(row_, 1);
        } else if (pressed & kCross) {
            audio::Sound_PlaySe(0x1A);
            if (row_ == kSettingRows) {   // Initial settings
                edit_ = Display_Defaults();
            } else if (row_ == kSettingRows + 1) {   // Cancel change
                edit_ = Display_Get();
            } else {   // Apply
                if (Changed()) Display_Apply(edit_);
                horn_ = false;
                state_ = kListExit;
            }
        }
        break;
    }
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

void DisplayScreen::Draw(gfx::Draw2D& d) {
    Window_DrawFrame(d, kListX, kListTop / 2, kListW, (kListBottom - kListTop) / 2, 2);
    Window_DrawFrame(d, kHelpX, kHelpTop / 2, kHelpW, (kHelpBottom - kHelpTop) / 2, 0);
    Window_DrawBar(d, 60, RowY(row_) - 2, kListW - 20);
    if (horn_) Window_DrawCursor(d, 20, RowY(row_) - 2);
    // The values, with the arrows of a row that changes.
    const DisplaySize size = Display_Resolve(edit_);
    const float aspect = Display_AspectValue(edit_.aspect, size);
    char sizeText[32], scaleText[32];
    if (edit_.width || edit_.mode == kDisplayBorderless)
        snprintf(sizeText, sizeof sizeText, "%dx%d", size.width, size.height);
    else
        snprintf(sizeText, sizeof sizeText, GT_002F0C42_4);
    if (edit_.renderScale > 0) snprintf(scaleText, sizeof scaleText, "%gx", (double)edit_.renderScale);
    else snprintf(scaleText, sizeof scaleText, "Auto (%gx)", (double)Display_RenderScale(edit_, size, aspect));
    const char* values[kSettingRows] = {Display_ModeName(edit_.mode), edit_.resizable ? "On" : "Off", sizeText,
                                        Display_AspectName(edit_.aspect), edit_.edgeHud ? "Edges" : "Centered",
                                        scaleText, edit_.vsync ? "On" : "Off", edit_.frameLimit ? "On" : "Off",
                                        edit_.lowGraphics ? "Low" : GT_00335190_4};
    // As the Controls screen's columns: the values in white, those the Display row makes moot greyed.
    for (int r = 0; r < kSettingRows; ++r) {
        int y = RowY(r);
        bool moot = (r == kRowSize && edit_.mode == kDisplayBorderless) ||
                    (r == kRowResizable && edit_.mode != kDisplayWindow);
        Text_PrintAt(d, kLabelX, y, kLabels[r], 0);
        Text_PrintAt(d, kValueX - Text_Width(values[r]) / 2, y, values[r], moot ? 10 : 0);
    }
    static const char* const kActions[3] = {GT_002EA950_16, GT_002EA940_13, GT_00333508_5};
    for (int k = 0; k < 3; ++k) Text_PrintAt(d, kLabelX, RowY(kSettingRows + k), kActions[k], 0);
    // The help for the row; under Resolution, what Auto comes to.
    int y = kHelpTop + 16;
    if (row_ < kSettingRows) {
        PrintHelp(d, y, kHelp[row_], 9);
        if (row_ == kRowSize && !edit_.width && edit_.mode != kDisplayBorderless) {
            char line[40];
            snprintf(line, sizeof line, "Now: %dx%d", size.width, size.height);
            Text_PrintAt(d, kHelpX + 20, y + 24 * 7, line, 10);
        }
    } else {
        PrintHelp(d, y, row_ == kSettingRows ? kHelpInitial : row_ == kSettingRows + 1 ? kHelpCancel : kHelpApply, 9);
    }
}

int DisplayScreen::Update(gfx::Draw2D& d, const platform::Input& input) {
    int state = UpdateState(input);
    Draw(d);
    // The Cancel window over it all, as the Extras screen's.
    if (cancelOpen_) {
        int r = cancel_.Update(d, input);
        if (r != 0) {
            if (r == -1) cancel_.sel = 1;
            cancelOpen_ = false;
        }
    }
    return state;
}

}  // namespace game
