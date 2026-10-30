#include "extras_screen.h"

#include <string.h>

#include "../../audio/sound.h"
#include "../field/field_menu_draw.h"
#include "../text.h"
#include "../../game_text.inc"   // the game's text, GT_<address>_<length>, made from the ELF

namespace game {

using namespace menu;
using namespace platform;

namespace {

const char* const kPageNames[3] = {"< Unused Content >", "< Debug >", "Cheats"};
const int kCheatsPage = 2;

struct HelpLine {
    const char* text;
    int color;
};
struct ExtraRow {
    const char* label;
    HelpLine help[10];
};
// Each extra's row and what the help window says about it.
const ExtraRow kRows[kExtraCount] = {
    {GT_002F0788_10,
     {{"My City's Parts", 0}, {"Shop stocks the", 0}, {"Life Meter once", 0}, {"you've cleared", 0},
      {"the Which-way?", 0}, {"Race. The game", 0}, {"checks for it,", 0}, {"then forgets to.", 0}}},
    {"Q-Wave",
     {{"A third station", 0}, {"on the car radio,", 0}, {"Q-Wave. Its music", 0}, {"and its button's", 0},
      {"name are on the", 0}, {"disc, but the", 0}, {"radio has no", 0}, {"button for it.", 0}}},
    {"Endurance Mail",
     {{"Wonder Realty's", 0}, {"\"Now Open!\" mail", 0}, {"comes when the", 0}, {GT_00328578_13, 0},
      {"opens. Nothing", 0}, {"in the game ever", 0}, {"sent it.", 0}}},
    {"Test Course",
     {{"A course on the", 0}, {"disc that no race", 0}, {"uses, in Quick", 0}, {"Race: drive its", 0},
      {"tube alone", 0}, {"against the clock.", 0}}},
    {"Body 149",
     {{"A car body on the", 0}, {"disc that no shop", 0}, {"or driver has.", 0}, {"My City's Body", 0},
      {"Shop sells it, as", 0}, {"the Q Remote.", 0}}},
    {"Demo Song",
     {{"DEMO, a song", 0}, {"nothing plays,", 0}, {"for the title's", 0}, {"attract demo.", 0}}},
    {"Duck Quiz",
     {{"The Cloud Hill", 0}, {"duck's quiz has", 0}, {"a line no one", 0}, {"could reach. Give", 0},
      {"up on his second", 0}, {"or third question", 0}, {GT_00311C2E_14, 0}}},
    {GT_002EA168_8,
     {{"The title's", 0}, {"Options get the", 0}, {"Language item the", 0}, {"US game left out:", 0},
      {"its menus in", 0}, {"French or German,", 0}, {"for this run.", 0}}},
    {"Pause Items",
     {{"The race pause", 0}, {"menu's debug", 0}, {"items: hide the", 0}, {"map and meter,", 0},
      {"fill the tank,", 0}, {"let the AI drive", 0}, {"your car, and the", 0}, {"AI's zone table.", 0}}},
    {"Car Readouts",
     {{"The developers'", 0}, {"numbers on the", 0}, {"race screen: the", 0}, {"car's speeds,", 0},
      {"engine, place", 0}, {"and the AI's", 0}, {"zones.", 0}}},
    {"Free Camera",
     {{"Controller 2", 0}, {"flies the race", 0}, {"camera: d-pad", 0}, {"moves, L1/L2 up", 0},
      {"and down, sticks", 0}, {"turn, \x81\xA2/\x81\xA0 zoom.", 0}}},
    {"Height Keys",
     {{"In a race the", 0}, {"d-pad's Up and", 0}, {"Down hold the", 0}, {"car lower or", 0},
      {"higher, to check", 0}, {"the ground.", 0}}},
    {"Clock Scrub",
     {{"On the Endurance", 0}, {"Run and in the", 0}, {"field, hold L1 or", 0}, {"L2 to run the", 0},
      {"time of day", 0}, {"forward or back.", 0}}},
    {"Test Menu",
     {{"The developers'", 0}, {"test menu, as a", 0}, {"third line under", 0}, {"Adventure: any", 0},
      {"town, race, book", 0}, {"or ending.", 0}}},
    {"Map Tuner",
     {{"Controller 2", 0}, {"moves, scales and", 0}, {"turns the race's", 0}, {"course map.", 0}}},
    {"", {}},   // the cheats' master switch: extras.json's alone
    {"Uncapped Speed",
     {{"Your car revs on", 0}, {"past the limiter", 0}, {"in top gear, and", 0}, {"the jet's push", 0},
      {"isn't held back.", 0}}},
    {"Infinite Fuel",
     {{"Your tank stays", 0}, {"full, and the jet", 0}, {"never runs dry.", 0}}},
    {"Power Boost",
     {{"Your engine is", 0}, {"twice as strong.", 0}}},
    {"Super Grip",
     {{"Your tires grip", 0}, {"twice as well on", 0}, {"every surface.", 0}}},
    {"Reduce Drag",
     {{"The air holds", 0}, {"your car back a", 0}, {"quarter as much:", 0}, {"a higher top", 0}, {"speed.", 0}}},
    {"Low Gravity",
     {{"Your car feels a", 0}, {"third of the", 0}, {"gravity: higher", 0}, {"and longer jumps.", 0}}},
    {"Slow Rivals",
     {{"The computer's", 0}, {"cars aim for", 0}, {"three quarters of", 0}, {"their usual speed.", 0},
      {GT_0031D63B_14, 0}, {"don't slow.", 0}}},
    {"Max Money",
     {{"Your money is", 0}, {"held at 999999", 0}, {"in the field.", 0}}},
    {"All Parts",
     {{"Every shop sells", 0}, {"everything, in", 0}, {"every town, from", 0}, {"the start: each", 0},
      {"Parts Shop every", 0}, {"part, each Body", 0}, {"Shop every body.", 0}}},
};
const HelpLine kHelpPage[] = {{"Left and Right:", 0}, {"unused content", 0}, {"or the debug", 0}, {"features", 0}, {}};
const HelpLine kHelpPageNote[] = {{"Off is the", 0}, {"original game", 0}, {}};
const HelpLine kHelpCheats[] = {{"The PC's cheats", 0}, {"for your car,", 0}, {"the rivals and", 0}, {"the shops", 0}, {}};
const HelpLine kHelpInitial[] = {{"Turns this", 0}, {"page's extras", 0}, {"off", 0}, {}};
const HelpLine kHelpCancel[] = {{GT_00333528_7, 0}, {GT_002EAA68_13, 0}, {GT_002EAA58_8, 0}, {}};
const HelpLine kHelpApply[] = {{GT_002EAA48_13, 0}, {}};

// The layout (frame lines), the Controls screen's list and the help window filling the pad's place.
const int kListX = 40, kListTop = 84, kListW = 300, kListBottom = 422;
const int kLabelX = 60, kValueX = 300;   // the value's centre
const int kPageY = 86, kFirstY = 118, kRowStep = 24, kActionsY = 346;
const int kHelpX = 360, kHelpTop = 84, kHelpW = 260, kHelpBottom = 422;

int FirstExtra(int page) { return page == kCheatsPage ? kExtraFirstCheat : page ? kExtraFirstDebug : 0; }
int ExtraCount(int page) {
    return page == kCheatsPage ? kExtraCount - kExtraFirstCheat : page ? kExtraCheats - kExtraFirstDebug
                                                                       : kExtraFirstDebug;
}

void PrintHelp(gfx::Draw2D& d, int y, const HelpLine* help) {
    for (int k = 0; help[k].text; ++k, y += 24) Text_PrintAt(d, kHelpX + 20, y, help[k].text, help[k].color);
}

}  // namespace

void ExtrasScreen::Start(bool cheats) {
    state_ = count_ = carStep_ = 0;
    page_ = cheats ? kCheatsPage : 0;
    row_ = 0;
    horn_ = false;
    cancelOpen_ = false;
    edit_ = Extras_Get();
}

int ExtrasScreen::Rows() const { return 1 + ExtraCount(page_) + 3; }

int ExtrasScreen::RowExtra(int row) const {
    return row >= 1 && row <= ExtraCount(page_) ? FirstExtra(page_) + row - 1 : -1;
}

int ExtrasScreen::RowY(int row) const {
    int n = ExtraCount(page_);
    return row == 0 ? kPageY : row <= n ? kFirstY + kRowStep * (row - 1) : kActionsY + kRowStep * (row - n - 1);
}

bool ExtrasScreen::Changed() const { return memcmp(&edit_, &Extras_Get(), sizeof edit_) != 0; }

// The Controls screen's machine (Settings_Update's, func_00214470): the title car in, the rows, "Cancel?" when
// leaving with changes, the title car out.
int ExtrasScreen::UpdateState(const platform::Input& input) {
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
        int n = ExtraCount(page_), extra = RowExtra(row_);
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
            int page = page_ == kCheatsPage ? page_ : Menu_MoveCursorGrid(input, 0, page_, 2, 1, false);
            if (page != page_) page_ = page;
        } else if (extra >= 0) {
            int on = edit_.on[extra] ? 1 : 0;
            on = Menu_MoveCursorGrid(input, 0, on, 2, 1, false);
            if (pressed & kCross) {
                audio::Sound_PlaySe(0x1D);
                on ^= 1;
            }
            edit_.on[extra] = on != 0;
        } else if (pressed & kCross) {
            audio::Sound_PlaySe(0x1A);
            if (row_ == n + 1) {   // Initial settings: this page off
                for (int e = FirstExtra(page_); e < FirstExtra(page_) + n; ++e) edit_.on[e] = false;
            } else if (row_ == n + 2) {   // Cancel change
                edit_ = Extras_Get();
            } else {   // Apply
                Extras_Apply(edit_);
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

void ExtrasScreen::Draw(gfx::Draw2D& d) {
    int n = ExtraCount(page_), first = FirstExtra(page_), extra = RowExtra(row_);
    Window_DrawFrame(d, kListX, kListTop / 2, kListW, (kListBottom - kListTop) / 2, 2);
    Window_DrawFrame(d, kHelpX, kHelpTop / 2, kHelpW, (kHelpBottom - kHelpTop) / 2, 0);
    Window_DrawBar(d, 60, RowY(row_) - 2, kListW - 20);
    if (horn_) Window_DrawCursor(d, 20, RowY(row_) - 2);
    Text_PrintAt(d, kLabelX, kPageY, kPageNames[page_], 0);
    for (int r = 1; r <= n; ++r) {
        int e = first + r - 1, y = RowY(r);
        bool on = edit_.on[e];
        Text_PrintAt(d, kLabelX, y, kRows[e].label, 0);
        const char* value = on ? "On" : "Off";
        Text_PrintAt(d, kValueX - Text_Width(value) / 2, y, value, on ? 5 : 0);
    }
    static const char* const kActions[3] = {GT_002EA950_16, GT_002EA940_13, GT_00333508_5};
    for (int k = 0; k < 3; ++k) Text_PrintAt(d, kLabelX, RowY(n + 1 + k), kActions[k], 0);
    // The help for the row.
    int y = kHelpTop + 16;
    if (row_ == 0) {
        PrintHelp(d, y, page_ == kCheatsPage ? kHelpCheats : kHelpPage);
        PrintHelp(d, y + 24 * 5, kHelpPageNote);
    } else if (extra >= 0) {
        PrintHelp(d, y, kRows[extra].help);
    } else {
        PrintHelp(d, y, row_ == n + 1 ? kHelpInitial : row_ == n + 2 ? kHelpCancel : kHelpApply);
    }
}

int ExtrasScreen::Update(gfx::Draw2D& d, const platform::Input& input) {
    int state = UpdateState(input);
    Draw(d);
    // The Cancel window over it all, as the Controls screen's.
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
