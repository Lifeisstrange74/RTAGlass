// The field menu's screens past the main list's: Items (func_002161E0), with the title-car machine they share
// (func_00213A78). See field_menu.h and docs/field_menu.md.
#include <math.h>
#include <stdio.h>
#include <string.h>

#include <vector>

#include "../../audio/sound.h"
#include "../save/save_data.h"
#include "../story/parts_screen.h"
#include "../story/race_list.h"
#include "../race/car.h"
#include "../race/race_light.h"
#include "../text.h"
#include "field_menu.h"
#include "field_menu_draw.h"
#include "../../game_text.inc"   // the game's text, GT_<address>_<length>, made from the ELF

namespace game {

using namespace menu;

namespace {

// D_002F0810: the items (kind 15's bits 0-47), each a name and a description of lines split by '\n'.
struct ItemText {
    const char* name;
    const char* desc;
};
const ItemText kItems[48] = {
    {GT_002F1230_15, GT_002F1210_28},
    {GT_002F1200_14, GT_002F11D0_43},
    {GT_002F11C0_11, GT_002F11A0_30},
    {GT_002F1190_14, GT_002F1160_44},
    {GT_002F1150_11, GT_002F1120_44},
    {GT_002F1108_16, GT_002F10E0_38},
    {GT_002F10C8_17, GT_002F1098_42},
    {GT_002F1088_13, GT_002F1060_39},
    {GT_002F1050_14, GT_002F1020_40},
    {GT_002F1010_14, GT_002F0FE0_41},
    {GT_002F0FC8_16, GT_002F0F98_44},
    {GT_002F0F88_10, GT_002F0F68_31},
    {GT_002F0F58_13, GT_002F0F38_29},
    {GT_002F0F20_16, GT_002F0EF8_39},
    {GT_002F0EE8_10, GT_002F0EB8_44},
    {GT_00333F38_7, GT_002F0EA8_12},
    {GT_002F0E90_19, GT_002F0E68_38},
    {GT_002F0E58_11, GT_002F0E30_39},
    {GT_002F0E20_14, GT_002F0E08_23},
    {GT_002F0DF8_10, GT_002F0DB8_56},
    {GT_002F0DA8_14, GT_002F0D88_24},
    {GT_002F0D78_13, GT_002F0D50_39},
    {GT_002F0D40_9, GT_002F0D10_46},
    {GT_00333F30_6, GT_002F0CF0_27},
    {GT_00333F28_7, GT_002F0CC0_47},
    {GT_002F0CB0_15, GT_002F0C90_26},
    {GT_002F0C80_11, GT_002F0C70_15},
    {GT_00333F20_6, GT_002F0C50_29},
    {GT_002F0C38_19, GT_002F0C20_23},
    {GT_002F0C10_9, GT_002F0BF0_29},
    {GT_002F0BE0_15, GT_002F0BB8_32},
    {GT_002F0BA8_8, GT_002F0B88_25},
    {GT_002F0B78_13, GT_002F0B60_20},
    {GT_00333F18_7, GT_002F0B60_20},
    {GT_00333F10_4, GT_002F0B60_20},
    {GT_00333F08_5, GT_002F0B60_20},
    {GT_002F0B50_10, GT_002F0B60_20},
    {GT_002F0B40_9, GT_002F0B60_20},
    {GT_002F0B30_8, GT_002F0B60_20},
    {GT_002F0B20_11, GT_002F0AF8_35},
    {GT_002F0AE8_12, GT_002F0AC8_26},
    {GT_002F0AB8_11, GT_002F0A98_26},
    {GT_002F0A88_11, GT_002F0A68_29},
    {GT_002F0A58_15, GT_002F0A38_25},
    {GT_002F0A28_12, GT_002F0A08_27},
    {GT_002F09F8_10, GT_002F09D0_33},
    {GT_00333F00_7, GT_002F09B0_29},
    {GT_00333EF8_5, GT_002F0990_29},
};
// D_0029C518: the right box's rows: the mileage, the heading, then each billboard sponsor's (Options items 4-8).
const char* const kMileRows[7] = {GT_00333500_7, GT_002EA930_13, GT_003334F8_5, GT_002EA920_12, GT_003334F0_7, GT_002EA910_10,
                                  GT_002EA900_13};

const char kNote[] = "SYS/NOTE.GSL";
const char kStamp[] = "SYS/STAMP.GSL";
const char* const kStampText[100] = {
#include "field_stamps.inc"
};

// func_0024F928's sprite: (x, y), w x h on the screen (negative: mirrored), the texels u..u + sw by v..v + sh
// repeated across it (GS REGION_REPEAT; sw or sh 0, the texels as big as the sprite).
struct Tile {
    int x, y, u, v, w, h, sw, sh;
};
void Sprite_DrawRepeat(gfx::Draw2D& d, const gfx::Texture& t, const Tile& p, int ox = 0, int oy = 0) {
    int aw = p.w < 0 ? -p.w : p.w, ah = p.h < 0 ? -p.h : p.h;
    int tw = p.sw ? p.sw : aw, th = p.sh ? p.sh : ah;
    for (int y = 0; y < ah; y += th)
        for (int x = 0; x < aw; x += tw) {
            int w = aw - x < tw ? aw - x : tw, h = ah - y < th ? ah - y : th;
            float u = (float)p.u, v = (float)p.v, uw = (float)w, vh = (float)h;
            if (p.w < 0) {
                u += w;
                uw = -uw;
            }
            if (p.h < 0) {
                v += h;
                vh = -vh;
            }
            d.Sprite(t, (float)(p.x + ox + x), (float)(p.y + oy + y), (float)w, (float)h, u, v, uw, vh);
        }
}

// D_002DBDD0: the open book's 29 pieces (the cover's edges, the spine, the two pages).
const Tile kBook[29] = {
    {8, 32, 0, 48, 32, 32, 0, 0},       {8, 64, 0, 80, 32, 320, 32, 16},    {8, 384, 0, 96, 32, 36, 0, 0},
    {40, 32, 32, 48, 232, 32, 16, 32},  {40, 384, 32, 96, 232, 36, 16, 36}, {272, 32, 48, 48, 48, 32, 0, 0},
    {272, 384, 48, 96, 48, 36, 0, 0},   {320, 32, 48, 48, -48, 32, 0, 0},   {320, 384, 48, 96, -48, 36, 0, 0},
    {368, 32, 32, 48, 232, 32, 16, 32}, {368, 384, 32, 96, 232, 36, 16, 36}, {600, 32, 0, 48, -32, 32, 0, 0},
    {600, 64, 0, 80, -32, 320, 32, 16}, {600, 384, 0, 96, -32, 36, 0, 0},   {304, 40, 32, 0, 32, 16, 0, 0},
    {304, 56, 32, 16, 32, 336, 32, 16}, {304, 392, 32, 32, 32, 16, 0, 0},   {40, 40, 0, 0, 16, 16, 0, 0},
    {40, 56, 0, 16, 16, 336, 16, 16},   {40, 392, 0, 32, 16, 16, 0, 0},     {56, 40, 16, 0, 248, 16, 16, 16},
    {56, 56, 16, 16, 248, 336, 16, 16}, {56, 392, 16, 32, 248, 16, 16, 16}, {584, 40, 80, 0, 16, 16, 0, 0},
    {584, 56, 80, 16, 16, 336, 16, 16}, {584, 392, 80, 32, 16, 16, 0, 0},   {336, 40, 64, 0, 248, 16, 16, 16},
    {336, 56, 64, 16, 248, 336, 16, 16}, {336, 392, 64, 32, 248, 16, 16, 16},
};
// D_002DC258: a stamp page's rounded frame round the picture, at x + 288 on the right page.
const Tile kStampFrame[9] = {
    {80, 80, 96, 0, 32, 32, 0, 0},      {112, 80, 128, 0, 128, 32, 16, 32},   {240, 80, 144, 0, 32, 32, 0, 0},
    {80, 112, 96, 32, 32, 112, 32, 16}, {112, 112, 128, 32, 128, 112, 16, 16}, {240, 112, 144, 32, 32, 112, 32, 16},
    {80, 224, 96, 48, 32, 32, 0, 0},    {112, 224, 128, 48, 128, 32, 16, 32}, {240, 224, 144, 48, 32, 32, 0, 0},
};

// The book's ground: the pencil tile (u 128 v 128, 64 x 64) over the frame.
void Book_DrawGround(gfx::Draw2D& d, const gfx::Texture& t) {
    Sprite_DrawRepeat(d, t, Tile{0, 0, 128, 128, 640, 448, 64, 64});
}

// func_00264FF0: the open book, and its 12 headers (80 x 12 from u 96, v 80 + 12 x `header`: 0 the stamps').
void Book_Draw(gfx::Draw2D& d, const gfx::Texture& t, int header) {
    for (const Tile& p : kBook) Sprite_DrawRepeat(d, t, p);
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 3; ++j)
            d.Sprite(t, 56.0f + 288 * (i / 2) + 80 * j, 42.0f + 352 * (i % 2), 80, 12, 96, 80.0f + 12 * header, 80,
                     12);
}

// func_00264D50: the turning page, hinged on the spine, `angle` 0 lying on the right, 0x8000 on the left: its
// board and its paper, the edge away from the spine taller (by 0.2 at most) about the frame's middle line as it
// lifts.
void Book_DrawTurn(gfx::Draw2D& d, const gfx::Texture& t, uint16_t angle) {
    float th = angle * 3.14159265f / 32768.f, c = cosf(th), s = sinf(th);
    struct Leaf {
        int x, y, u, v, uw, vh;
        float vx[4], vy[4];
    };
    static const Leaf kLeaves[2] = {
        {320, 40, 32, 56, 56, 64, {4, 276, 4, 276}, {0, 0, 368, 368}},   // D_0032EFC0, the board
        {320, 56, 8, 16, 32, 16, {4, 268, 4, 268}, {0, 0, 336, 336}},    // D_0032F030, the paper
    };
    for (const Leaf& l : kLeaves) {
        float qx[4], qy[4], y0 = l.y - 224.f;
        for (int k = 0; k < 4; ++k) {
            float fx = l.vx[k];
            qx[k] = l.x + fx * c;
            qy[k] = l.y + (l.vy[k] + y0) * (fx * s * 0.2f / 320.f + 1) - y0;
        }
        d.Quad(t, qx, qy, (float)l.u, (float)l.v, (float)l.uw, (float)l.vh);
    }
}

// D_0032F0A0 and D_0032F0C0: the green diamond's halves beside the pages, while there are more spreads that way.
void Book_DrawArrows(gfx::Draw2D& d, const gfx::Texture& t, bool left, bool right) {
    if (left) d.Sprite(t, 30, 204, 22, 40, 82, 152, 22, 40);
    if (right) d.Sprite(t, 588, 204, 22, 40, 104, 152, 22, 40);
}

// Text split after each '#' (func_0024EA00's fields), a line every 24 from (x, y).
void Book_PrintLines(gfx::Draw2D& d, int x, int y, const char* text, int color) {
    char line[64];
    for (const char* p = strchr(text, '#'); p; y += 24) {
        const char* e = strchr(p + 1, '#');
        size_t n = e ? (size_t)(e - p - 1) : strlen(p + 1);
        if (n >= sizeof line) n = sizeof line - 1;
        memcpy(line, p + 1, n);
        line[n] = 0;
        Text_PrintAt(d, x, y, line, color);
        p = e;
    }
}

}  // namespace

const char* Item_Name(int n) { return n >= 0 && n < 48 ? kItems[n].name : ""; }

// Settings (FieldMenu_Settings, func_00216738) on the PC: the title car alone with "Settings" drives in as the
// PS2's, over Settings' background, and a list of the PC's setting screens: Controls (the PS2's key setup's
// place: ui/controls_screen.h), Display (ui/display_screen.h), Extras (ui/extras_screen.h), with extras.json's
// "cheats" Cheats (the same screen), and Other, the title Options' Vibration, Speaker and Sound Volume. Triangle on
// the list drives the car back out.
void FieldMenu::FieldMenu_Settings(gfx::Draw2D& d, const platform::Input& input) {
    enum { kControlsItem, kDisplayItem, kExtrasItem, kCheatsItem, kOtherItem };
    static const char* const kSettingsNames[5] = {"Controls", "Display", "Extras", "Cheats", "Other"};
    static const char* const kSettingsHelp[5][4] = {
        {"Change the keys", "and controller", "buttons", ""},
        {"Window, screen,", "resolution and", "wide pictures", ""},
        {"Turn on unused", "content and the", "debug features", ""},
        {"The PC's cheats", "", "", ""},
        {"Vibration,", "speaker and", "sound volume", ""},
    };
    // The list's items (Cheats only when it's offered).
    const int kItems[5] = {kControlsItem, kDisplayItem, kExtrasItem, kCheatsItem, kOtherItem};
    const int kItemsNoCheats[4] = {kControlsItem, kDisplayItem, kExtrasItem, kOtherItem};
    const int* items = Cheats_Enabled() ? kItems : kItemsNoCheats;
    const int itemCount = Cheats_Enabled() ? 5 : 4;
    static const char* settingsItems[5];
    static const char* const kOtherHelp[3][4] = {
        {"The controllers'", "vibration", "", ""},
        {"Stereo or mono", "", "", ""},
        {"The sound's", "volume", "", ""},
    };
    static const char* const kOtherItems[3] = {GT_002EA128_9, GT_00333340_7, GT_002EA178_12};
    if (frame_ == 0) {
        title_ = TitleMenu();
        List title = {};
        title.labels[0] = 5;
        Menu_PlaceCars(title);
        for (int k = 0; k < itemCount; ++k) settingsItems[k] = kSettingsNames[items[k]];
        list_ = Window();
        list_.SetList(settingsItems, itemCount, itemCount);
        list_.style = 2;
        list_.x = 0x28;
        list_.y = 0x2C;
        list_.w = 0x104;
        list_.h = (short)(0xC * itemCount);
        list_.startConfirms = false;
        settingsChild_ = kSettingsList;
    }
    Menu_DrawRoad(d, roadX_, roadY_, 5);
    Menu_DrawPanel(d);
    Menu_DrawCars(d);
    switch (settingsChild_) {
    case kSettingsControls:
        if (controls_.Update(d, input) == kListLeft) settingsChild_ = kSettingsList;
        break;
    case kSettingsExtras:
        if (extras_.Update(d, input) == kListLeft) settingsChild_ = kSettingsList;
        break;
    case kSettingsDisplay:
        if (display_.Update(d, input) == kListLeft) settingsChild_ = kSettingsList;
        break;
    default: {
        if (settingsChild_ == kSettingsList && Menu_UpdateTitle(input, &list_) == kListLeft) next_ = kScreenMain;
        // The help for the item under the cursor (Other's while it's open), hidden under a setting's window.
        if (settingsChild_ != kSettingsOption) {
            bool other = settingsChild_ == kSettingsOther;
            int sel = other ? other_.sel : list_.sel;
            if (sel < 0 || sel >= (other ? 3 : itemCount)) sel = 0;
            Window_DrawFrame(d, 0x154, 0x8C, 0x104, 0x3C, 0);
            for (int k = 0; k < 4; ++k)
                Text_PrintAt(d, 360, 292 + 24 * k, other ? kOtherHelp[sel][k] : kSettingsHelp[items[sel]][k], 0);
        }
        int r = list_.Update(d, input);
        if (settingsChild_ == kSettingsList && r == 1) {
            audio::Sound_PlaySe(0x1A);
            const int item = list_.sel >= 0 && list_.sel < itemCount ? items[list_.sel] : kOtherItem;
            if (item == kControlsItem) {
                controls_.SetDataDir(dataDir_);
                controls_.Start();
                settingsChild_ = kSettingsControls;
            } else if (item == kDisplayItem) {
                display_.Start();
                settingsChild_ = kSettingsDisplay;
            } else if (item == kExtrasItem || item == kCheatsItem) {
                extras_.Start(item == kCheatsItem);
                settingsChild_ = kSettingsExtras;
            } else {
                list_.state |= Window::kInactive;
                other_ = Window();
                other_.pad = platform::kEitherPad;
                for (const char* item : kOtherItems) other_.AddLine(item, kLineSelectable);
                other_.state = Window::kOpening;
                other_.x = 0x154;
                other_.y = 0x2C;
                other_.w = 0x104;
                settingsChild_ = kSettingsOther;
            }
        }
        if (settingsChild_ == kSettingsOther || settingsChild_ == kSettingsOption) {
            int o = other_.Update(d, input);
            if (settingsChild_ == kSettingsOther) {
                if (o == 1) {
                    audio::Sound_PlaySe(0x1A);
                    other_.state |= Window::kInactive;
                    option_.Open((OptionWindows::Kind)other_.sel);
                    settingsChild_ = kSettingsOption;
                } else if (o == -1) {
                    audio::Sound_PlaySe(0x1B);
                    other_.state = Window::kClosing;
                } else if (o == -2) {
                    list_.state &= ~Window::kInactive;
                    settingsChild_ = kSettingsList;
                }
            } else if (!option_.Update(d, input)) {
                other_.state &= ~Window::kInactive;
                settingsChild_ = kSettingsOther;
            }
        }
        break;
    }
    }
}

int FieldMenu::Menu_UpdateTitle(const platform::Input& input, Window* w) {
    TitleMenu& m = title_;
    switch (m.state) {
    case 0:
        if (w) w->state |= Window::kInactive;
        m.state = 1;
    // fall through
    case 1:
        m.count = 16;
        m.state = 2;
    // fall through
    case 2:
        cars_[0].x -= kDrive;
        if (--m.count == 0) m.state = kListReady;
        break;
    case kListReady:
        if (w) w->state &= ~Window::kInactive;
        m.state = kListIdle;
    // fall through
    case kListIdle:
        if (input.Pressed(0) & platform::kTriangle) {
            audio::Sound_PlaySe(0x1B);
            if (w) w->state |= Window::kInactive;
            m.state = kListExit;
        }
        break;
    case kListChosen:
        break;
    case kListExit:
        m.count = 16;
        m.state = kListExitOut;
    // fall through
    case kListExitOut:
        cars_[0].x += kDrive;
        if (--m.count == 0) m.state = kListLeft;
        break;
    default:
        m.state = 0;
        m.count = 0;
        break;
    }
    return m.state;
}

void FieldMenu::FieldMenu_Items(gfx::Draw2D& d, const platform::Input& input) {
    const SaveRecord& save = Save_Current();
    if (frame_ == 0) {
        // The items the save has, in their order (Part_NthOwned on kind 15); none is a list of one.
        listNames_.clear();
        listDescs_.clear();
        for (int id = 0; id < 48; ++id)
            if (Save_HasItem(15, id)) {
                listNames_.push_back(kItems[id].name);
                listDescs_.push_back(kItems[id].desc);
            }
        if (listNames_.empty()) {
            listNames_.push_back(GT_002EA8F0_13);
            listDescs_.push_back(GT_002EA8E0_10);
        }
        title_ = TitleMenu();
        List title = {};
        title.labels[0] = 4;
        Menu_PlaceCars(title);
        // The list: 8 rows, the cyan window with orange edges at (40, 88), 260 x 192.
        list_ = Window();
        list_.SetList(listNames_.data(), (int)listNames_.size(), 8);
        list_.style = 2;
        list_.x = 0x28;
        list_.y = 0x2C;
        list_.w = 0x104;
        list_.h = 0x60;
        list_.startConfirms = false;
    }
    if (Menu_UpdateTitle(input, &list_) == kListLeft) next_ = kScreenMain;
    Menu_DrawRoad(d, roadX_, roadY_, 4);
    Menu_DrawPanel(d);
    Menu_DrawCars(d);
    Window_DrawFrame(d, 0x28, 0x98, 0x104, 0x30, 0);
    Window_DrawFrame(d, 0x154, 0x2C, 0x104, 0x9C, 0);
    // The window draws itself and moves the cursor while it's active; what it returns (Cross) isn't used.
    list_.Update(d, input);
    // The description under the cursor, a line every 24 from (40, 304).
    int sel = list_.sel >= 0 && list_.sel < (int)listDescs_.size() ? list_.sel : 0;
    char line[64];
    const char* p = listDescs_[sel];
    for (int i = 0; *p; ++i) {
        const char* e = strchr(p, '\n');
        size_t n = e ? (size_t)(e - p) : strlen(p);
        if (n >= sizeof line) n = sizeof line - 1;
        memcpy(line, p, n);
        line[n] = 0;
        Text_PrintAt(d, 0x28, 304 + 24 * i, line, 0);
        p += n + (e ? 1 : 0);
    }
    Money_Draw(d, 0x186, 42, save.money, save.currency);
    // The coins found: 100 less the pickups still out.
    Text_PrintAt(d, 0x168, 88, GT_002EACA8_13, 0);
    Number_PrintSmallRight(d, 0x244, 90, 100 - Pickups_Left(), 0);
    // The mileage, then each billboard sponsor's miles not yet paid (the first under the heading), in miles of
    // 1609 metres.
    int label = 0x72, value = 0x70;
    bool heading = false;
    const int mileX = 0x244 - Text_Width(GT_00333578_5);
    for (int i = 0; i < 6; ++i) {
        int metres = save.distance;
        if (i > 0) {
            if (!Save_HasItem(11, i + 3)) continue;
            if (!heading) {
                Text_PrintAt(d, 0x168, value / 2 * 2, kMileRows[1], 0);
                value += 22;
                label += 22;
                heading = true;
            }
            metres = save.sponsorDistance[i - 1];
        }
        Text_PrintAt(d, 0x168, label / 2 * 2, kMileRows[i == 0 ? 0 : i + 1], 0);
        value += 22;
        label += 44;
        Text_PrintAt(d, mileX, value / 2 * 2, GT_00333578_5, 0);
        Number_PrintSmallRight(d, mileX, value / 2 * 2, metres / 1609, 0);
        value += 22;
    }
}

int FieldMenu::Notebook_Update(const platform::Input& input) {
    NotebookMenu& m = notebook_;
    switch (m.state) {
    case 0:
    case 1:
        m.count = 16;
        m.state = 2;
    // fall through
    case 2:
        cars_[0].x -= kDrive;
        if (--m.count == 0) m.state = kListReady;
        break;
    case kListReady:
        m.state = kListIdle;
    // fall through
    case kListIdle: {
        uint32_t pressed = input.Pressed(0);
        if (pressed & platform::kCross) {
            audio::Sound_PlaySe(0x1A);
            m.state = kListChosen;
        } else if (pressed & platform::kTriangle) {
            audio::Sound_PlaySe(0x1B);
            m.state = kListExit;
        } else {
            m.cursor = Menu_MoveCursorGrid(input, 0, m.cursor, 1, 3, false);
        }
        break;
    }
    case kListChosen:
        m.count = 16;
        m.state = kListChosenOut;
    // fall through
    case kListChosenOut:
        // The title's car drives off left.
        cars_[0].x -= kDrive;
        if (--m.count == 0) m.state = kListEntered;
        break;
    case kListExit:
        m.count = 16;
        m.state = kListExitOut;
    // fall through
    case kListExitOut:
        cars_[0].x += kDrive;
        if (--m.count == 0) m.state = kListLeft;
        break;
    default:
        m.state = 0;
        m.count = 0;
        break;
    }
    return m.state;
}

void FieldMenu::FieldMenu_Notebook(gfx::Draw2D& d, const platform::Input& input) {
    if (frame_ == 0) {
        notebook_.state = notebook_.count = 0;
        List title = {};
        title.labels[0] = 2;
        Menu_PlaceCars(title);
    }
    // (The PS2 draws and reads nothing while the loader is busy: NOTE.GSL, after the main screen's Cross.)
    int r = Notebook_Update(input);
    if (r == kListEntered) {
        // Stamps and Pictures load their pictures first (func_0020DC50 0x40017, 0x40018), Results doesn't.
        static const int kBooks[3] = {kScreenStamps, kScreenPictures, kScreenResults};
        next_ = kBooks[notebook_.cursor];
    } else if (r == kListLeft) {
        next_ = kScreenMain;
    }
    Menu_DrawRoad(d, roadX_, roadY_, 2);
    Menu_DrawPanel(d);
    Menu_DrawCars(d);
    // The notebook (NOTE picture 1; the Class C card is on its cover), the license's card over that card (Class B,
    // A, Super A: save +0x651), and the tabs (picture 2), the chosen one light with its label on the pin.
    const gfx::Texture& book = d.Get(kNote, 1);
    d.Sprite(book, 120, 104, 216, 270, 152, 0, 216, 270);
    int license = Save_Current().license;
    if (license > 0) d.Sprite(book, 156, 136, 152, 104, 0, 104.0f * (license - 1), 152, 104);
    const gfx::Texture& tabs = d.Get(kNote, 2);
    for (int i = 0; i < 3; ++i) {
        bool on = i == notebook_.cursor;
        d.Sprite(tabs, 336, 128.0f + 84 * i, 26, 56, on ? 352.0f : 384.0f, 0, 26, 56);
        d.Sprite(tabs, 336, 136.0f + 84 * i, 176, 40, on ? 176.0f : 0, 40.0f * i, 176, 40);
    }
}

void FieldMenu::Stamp_DrawPage(gfx::Draw2D& d, int n) {
    const SaveRecord& save = Save_Current();
    const gfx::Texture& t = d.Get(kStamp, 0);
    int side = n & 1, ox = 288 * side;
    // The number's corner (mirrored on the right) and the number, right-aligned, in dark blue.
    if (side)
        d.Sprite(t, 528, 56, 64, 56, 64, 136, -64, 56);
    else
        d.Sprite(t, 48, 56, 64, 56, 0, 136, 64, 56);
    char num[8];
    sprintf(num, "%d", n + 1);
    Text_PrintAt(d, (side ? 0x242 : 0x54) - 10 * (int)strlen(num), 68, num, 3);
    for (const Tile& p : kStampFrame) Sprite_DrawRepeat(d, t, p, ox);
    // The text's ruled lines: 4 rows of 12.
    for (int r = 0; r < 4; ++r)
        for (int k = 0; k < 12; ++k) d.Sprite(t, (side ? 0x158 : 0x38) + 20.0f * k, 0x110 + 24.0f * r, 20, 24, 96, 128, 20, 24);
    // The stamp once it's earned (STAMP picture n + 1, twice its size), its text once the talk has shown it.
    if ((save.stampEarned[n >> 6] >> (n & 63)) & 1)
        d.Sprite(d.Get(kStamp, n + 1), 112.0f + ox, 102, 128, 128, 0, 0, 64, 64);
    if ((save.stampShown[n >> 6] >> (n & 63)) & 1) Book_PrintLines(d, 60 + ox, 272, kStampText[n], 3);
}

void FieldMenu::FieldMenu_Stamps(gfx::Draw2D& d, const platform::Input& input) {
    SaveRecord& save = Save_Current();
    BookTask& b = book_;
    if (frame_ == 0) {
        b = BookTask();
        b.oldPage = save.stampCursor;
        b.state = 2;   // (0, 1: until STAMP.GSL is in)
    }
    if (b.state == 2) {
        // Left and Right turn a spread (repeating, stopping at the ends); the save keeps the left page.
        int spread = save.stampCursor / 2;
        int to = Menu_MoveCursorGrid(input, 0, spread, 50, 1, true);
        save.stampCursor = (int16_t)(to * 2);
        if (to > spread) {
            b.speed = 0x800;
            b.angle = 0;
            b.state = 3;
        } else if (to < spread) {
            b.angle = 0x8000;
            b.speed = -0x800;
            b.state = 3;
        } else if (input.Pressed(0) & platform::kTriangle) {
            // Back to the Notebook (the PS2 loads NOTE.GSL again first, func_0020DC50 0x40005).
            audio::Sound_PlaySe(0x1B);
            b.state = 4;
        }
    } else if (b.state == 3) {
        b.angle = (uint16_t)(b.angle + b.speed);
        if (b.angle > 0x8000) {
            b.speed = 0;
            b.state = 2;
            b.oldPage = save.stampCursor;
            b.angle = 0;
        }
    } else if (b.state == 4) {
        next_ = kScreenNotebook;
    }
    // The book from STAMP picture 0, whose headers are Japanese ("sutanpu"): it takes NOTE picture 0's place in the
    // GS, English headers and all.
    const gfx::Texture& t = d.Get(kStamp, 0);
    Book_DrawGround(d, t);
    Book_Draw(d, t, 0);
    int cursor = save.stampCursor;
    if (b.state == 3) {
        // One page shows beside the turning one: the old spread's until the page is upright, then the new one's.
        int page = b.speed > 0 ? (b.angle < 0x4000 ? b.oldPage & ~1 : cursor | 1)
                               : (b.angle >= 0x4001 ? b.oldPage | 1 : cursor);
        Stamp_DrawPage(d, page);
        Book_DrawTurn(d, t, b.angle);
    } else {
        if (frame_ & 0x20) Book_DrawArrows(d, t, cursor > 0, cursor < 98);
        Stamp_DrawPage(d, cursor);
        Stamp_DrawPage(d, cursor | 1);
    }
}

bool StampBook::Start(bool field) {
    // The first frame: the first stamp waiting, or the task ends at once (nothing loaded or shown).
    int id = Stamp_Dequeue();
    if (!id) return false;
    field_ = field;
    Save_Current().stampCursor = (int16_t)(id - 1);
    oldPage_ = id - 1;
    state_ = 2;   // (0 and 1 load SYS/STAMP.GSL)
    count_ = 0;
    level_ = 0;
    return true;
}

bool StampBook::Update(gfx::Draw2D& d, const platform::Input& input) {
    SaveRecord& save = Save_Current();
    int cursor = save.stampCursor;
    // The book, drawn before the frame's step: the ground, the stamps' book and the spread (state 5 the turning
    // page over one of them; state 3 the stamp coming down on its page, with its text).
    if (state_ < 7) {
        const gfx::Texture& t = d.Get(kStamp, 0);
        Book_DrawGround(d, t);
        Book_Draw(d, t, 0);
        if (state_ == 5) {
            uint16_t angle = (uint16_t)count_;
            int page = level_ > 0 ? (angle < 0x4000 ? oldPage_ & ~1 : cursor | 1)
                                  : (angle >= 0x4001 ? oldPage_ | 1 : cursor & ~1);
            FieldMenu::Stamp_DrawPage(d, page);
            Book_DrawTurn(d, t, angle);
        } else {
            FieldMenu::Stamp_DrawPage(d, cursor & ~1);
            FieldMenu::Stamp_DrawPage(d, cursor | 1);
        }
        if (state_ == 3) {
            // func_00265128: the stamp (STAMP picture cursor + 1, the whole 64 x 64) centred over its page's frame,
            // (level / 5 + 1) x 64 either way of the centre, and the stamp's text.
            float half = (level_ / 5.0f + 1.0f) * 64.0f, cx = 176.0f + 288 * (cursor & 1), cy = 166.0f;
            d.Sprite(d.Get(kStamp, cursor + 1), cx - half, cy - half, 2 * half, 2 * half, 0, 0, 64, 64);
            Book_PrintLines(d, 60 + 288 * (cursor & 1), 272, kStampText[cursor], 3);
        }
    }
    switch (state_) {
    case 2:
        // 12 frames of the page, then the stamp from 10 up.
        if (count_++ >= 11) {
            level_ = 10;
            count_ = 0;
            state_ = 3;
        }
        break;
    case 3:
        if (--level_ <= 0) {
            Stamp_Earn(cursor);
            audio::Sound_PlaySe(0x21);
            count_ = 0;
            state_ = 4;
        }
        break;
    case 4:
        if (input.Pressed(0) & (platform::kCross | platform::kTriangle)) {
            audio::Sound_PlaySe(0x1A);
            int id = Stamp_Dequeue();
            if (!id) {
                count_ = 0;
                state_ = 6;
                break;
            }
            int p = id - 1;
            if (p / 2 > cursor / 2) {
                level_ = 0x800;
                count_ = 0;
                state_ = 5;
            } else if (p / 2 < cursor / 2) {
                count_ = 0x8000;
                level_ = -0x800;
                state_ = 5;
            } else {
                count_ = 0;
                state_ = 2;
            }
            save.stampCursor = (int16_t)p;
        }
        break;
    case 5:
        count_ = (uint16_t)(count_ + level_);
        if (count_ > 0x8000) {
            level_ = 0;
            count_ = 0;
            oldPage_ = cursor;
            state_ = 2;
        }
        break;
    case 6:
        // 10 frames more; then the field's ends, a building's darkens first.
        if (++count_ >= 10) {
            if (field_) return true;
            count_ = 0;
            state_ = 7;
        }
        break;
    case 7:
        if (++count_ >= 129) return true;
        break;
    }
    return false;
}

namespace {

const char kPuti[] = "SYS/PUTI.GSL";

// D_002DBD30: the album's ten frames on a spread (left page 0-4, right 5-9): the cursor's moves Up, Down, Left and
// Right (Left from the left column and Right from the right turn the page), and the frame's top left.
struct PhotoSlot {
    signed char move[4];
    int x, y;
};
const PhotoSlot kPhotoSlots[10] = {
    {{4, 2, -5, 5}, 186, 90},   {{2, 2, -4, 1}, 72, 186},   {{-2, 2, -1, 4}, 186, 186}, {{-2, -2, -4, 1}, 72, 282},
    {{-2, -4, -1, 4}, 186, 282}, {{3, 1, -5, 5}, 360, 90},  {{-1, 2, -4, 1}, 360, 186}, {{2, 2, -1, 4}, 474, 186},
    {{-2, -3, -4, 1}, 360, 282}, {{-2, -2, -1, 4}, 474, 282},
};

bool Photo_Taken(const SaveRecord& r, int n) { return (r.photoTaken[(n - 1) >> 6] >> ((n - 1) & 63)) & 1; }

// The album's digits (PUTI picture 1, 20 x 20 from u 20 x digit, v 108), centred on x.
void Pictures_DrawNumber(gfx::Draw2D& d, const gfx::Texture& t, int value, int cx, int y) {
    char s[8];
    int n = sprintf(s, "%d", value);
    int x = cx - 10 * n;
    for (int i = 0; i < n; ++i) d.Sprite(t, x + 20.0f * i, (float)y, 20, 20, 20.0f * (s[i] - '0'), 108, 20, 20);
}

// D_0029FDB0: the race rows' classes.
const char* const kResultRanks[5] = {GT_002EC550_8, GT_002EC540_8, GT_002EC530_8, GT_002EC520_9, GT_00333868_7};
const char kLocked[] = "      ????????";

// A mini-game's record is hidden while its place's bit (areaHidden) is set: the word, the bit.
bool Area_Hidden(const SaveRecord& r, int word, uint32_t bit) { return (r.areaHidden[word] & bit) != 0; }

}  // namespace

// func_00265FE8 (the cursor) and the states of func_002665B8.
void FieldMenu::FieldMenu_Pictures(gfx::Draw2D& d, const platform::Input& input) {
    SaveRecord& save = Save_Current();
    BookTask& b = book_;
    if (frame_ == 0) {
        b = BookTask();
        b.state = 2;   // (0, 1: until PUTI.GSL is in)
    }
    if (b.state == 2) {
        int old = save.photoCursor, to = old;
        uint32_t rep = input.Repeated(0);
        static const uint32_t kDirs[4] = {platform::kUp, platform::kDown, platform::kLeft, platform::kRight};
        for (int k = 0; k < 4; ++k)
            if (rep & kDirs[k]) {
                to = old + kPhotoSlots[old % 10].move[k];
                break;
            }
        if (to != old && to >= 0 && to < 100) {
            audio::Sound_PlaySe(0x1D);
            save.photoCursor = (int16_t)to;
        } else {
            to = old;
        }
        if (to / 10 > old / 10) {
            b.speed = 0x800;
            b.angle = 0;
            b.state = 5;
        } else if (to / 10 < old / 10) {
            b.angle = 0x8000;
            b.speed = -0x800;
            b.state = 5;
        } else if (input.Pressed(0) & platform::kCross) {
            if (Photo_Taken(save, to + 1)) {
                audio::Sound_PlaySe(0x1A);
                b.photo = to;
                b.state = 4;
                // Photo_ViewTask: the car the photo was taken with, its body loaded into car slot 3.
                const SavePhoto& ph = save.photos[to];
                Photo_SetCar(photoCar_, ph.paint, ph.body, ph.wheelColor, ph.wheel, ph.billboard, ph.partFlags);
                photoCars_.Load(d, dataDir_, &photoCar_.car, 1);
            } else {
                audio::Sound_PlaySe(0x1C);
            }
        } else if (input.Pressed(0) & platform::kTriangle) {
            // (The PS2 loads NOTE.GSL and the meter's pictures back first.)
            audio::Sound_PlaySe(0x1B);
            b.state = 6;
        }
    } else if (b.state == 4) {
        // Photo_ViewTask: until Cross or Triangle.
        if (frame_ > 0 && (input.Pressed(0) & (platform::kCross | platform::kTriangle))) {
            audio::Sound_PlaySe(0x1A);
            b.state = 2;
        }
    } else if (b.state == 5) {
        b.angle = (uint16_t)(b.angle + b.speed);
        if (b.angle > 0x8000) {
            b.angle = 0;
            b.speed = 0;
            b.state = 2;
        }
    } else if (b.state >= 6) {
        next_ = kScreenNotebook;
    }
    // PUTI picture 0, whose labels are Japanese ("puchikura"), as NOTE's English ones are overwritten on the PS2.
    const gfx::Texture& t = d.Get(kPuti, 0);
    Book_DrawGround(d, t);
    if (b.state == 4) {
        // The photo (SYS/PUTI.BIN's picture, which the viewer loads), twice its size, and the car it was taken with
        // over it.
        d.Sprite(d.Get("SYS/PUTI.BIN", b.photo), 64, 40, 512, 368, 0, 0, 256, 184);
        Photo_DrawCar(d, photoCars_, photoCar_);
        return;
    }
    Book_Draw(d, t, 1);
    int s = save.photoCursor / 10;
    if (b.speed > 0) {
        Pictures_DrawPage(d, 2 * s - 2);
        Pictures_DrawPage(d, 2 * s + 1);
        Book_DrawTurn(d, t, b.angle);
    } else if (b.speed < 0) {
        Pictures_DrawPage(d, 2 * s);
        Pictures_DrawPage(d, 2 * s + 3);
        Book_DrawTurn(d, t, b.angle);
    } else {
        if (frame_ & 0x20) Book_DrawArrows(d, t, s > 0, s < 9);
        Pictures_DrawPage(d, 2 * s);
        Pictures_DrawPage(d, 2 * s + 1);
        // The red outline round the cursor's frame, 94 x 65.
        const PhotoSlot& o = kPhotoSlots[save.photoCursor % 10];
        const float px[5] = {(float)o.x, o.x + 93.0f, o.x + 93.0f, (float)o.x, (float)o.x};
        const float py[5] = {(float)o.y, (float)o.y, o.y + 64.0f, o.y + 64.0f, (float)o.y};
        const uint32_t red[5] = {0x800000FF, 0x800000FF, 0x800000FF, 0x800000FF, 0x800000FF};
        Line_DrawStrip(d, px, py, red, 5);
    }
}

void FieldMenu::Pictures_DrawPage(gfx::Draw2D& d, int p) const {
    const SaveRecord& save = Save_Current();
    const gfx::Texture& t = d.Get(kPuti, 1);
    bool right = p & 1;
    // The corner's circle and curve (mirrored on the right) with the page's number in it.
    if (right)
        d.Sprite(t, 420, 42, 180, 104, 180, 0, -180, 104);
    else
        d.Sprite(t, 40, 42, 180, 104, 0, 0, 180, 104);
    Pictures_DrawNumber(d, t, p + 1, right ? 542 : 98, 78);
    for (int i = 0; i < 5; ++i) {
        const PhotoSlot& o = kPhotoSlots[(right ? 5 : 0) + i];
        int photo = 5 * p + i + 1;
        d.Sprite(t, (float)o.x, (float)o.y, 94, 66, 180, 0, 94, 66);
        Pictures_DrawNumber(d, t, photo, o.x + 47, o.y + 70);
        // A photo taken: its thumbnail (PUTI picture photo + 1, 64 x 47) over the frame.
        if (Photo_Taken(save, photo)) d.Sprite(d.Get(kPuti, photo + 1), (float)o.x, (float)o.y, 94, 66, 0, 0, 64, 47);
    }
}

void FieldMenu::Results_DrawPage(gfx::Draw2D& d, int page) const {
    const SaveRecord& save = Save_Current();
    bool right = page & 1;
    if (page < 26) {
        // An Adventure race: its course and class, then for the player and the two teammates (if they've raced it)
        // the name, the best place, the best lap and the best time.
        const int c = right ? 16 : 2;
        const AdventureRace& race = AdventureRace_Get(page);
        Text_Print(d, c, 2, Race_CourseName(race.course), 3);
        Text_Print(d, c + 7, 3, kResultRanks[race.rank < 5 ? race.rank : 4], 3);
        for (int k = 0; k < 3; ++k) {
            int place = save.adventurePlace[k][page];
            if (place == 0xFF) continue;
            int R = 4 + 4 * k;
            const char* name = save.name;
            if (k > 0) {
                const uint8_t* who = save.adventureTeammates[page][k - 1];
                name = who[0] ? Car_DriverRecordName(who[0], who[1]) : "";
            }
            Text_Print(d, c, R, name ? name : "", 3);
            Text_Print(d, c + 1, R + 1, GT_002EC4F0_9, 3);
            Text_Print(d, c + 10, R + 1, Text_PlaceName(place), 3);
            Text_Print(d, c + 1, R + 2, GT_002EC500_8, 3);
            Time_Print(d, 20 * c + 170, 24 * R + 58, save.adventureLap[k][page], 3);
            Text_Print(d, c + 1, R + 3, GT_002EC510_9, 3);
            Time_Print(d, 20 * c + 170, 24 * R + 82, save.adventureTotal[k][page], 3);
        }
        return;
    }
    char s[24];
    // A value right-aligned to x + 240 as if `w` a character.
    auto Value = [&](int x, int y, int w, const char* v) {
        Text_PrintAt(d, x + 240 - w * (int)strlen(v), y, v, 3);
    };
    if (page == 26) {
        const int x = 50;
        if (Area_Hidden(save, 3, 0x40)) {
            Text_PrintAt(d, x, 58, kLocked, 3);
        } else {
            Text_PrintAt(d, x, 58, GT_003284F8_8, 3);
            for (int i = 1; i <= 5; ++i) {
                sprintf(s, GT_0032F270_10, i);
                Text_PrintAt(d, 70, 58 + 24 * i, s, 3);
                Time_Print(d, 210, 58 + 24 * i, save.figure8Best[i - 1], 3);
            }
        }
        if (Area_Hidden(save, 6, 0x80)) {
            Text_PrintAt(d, x, 250, kLocked, 3);
        } else {
            Text_PrintAt(d, x, 250, GT_00328478_11, 3);
            Text_PrintAt(d, 70, 274, GT_00335310_6, 3);
            sprintf(s, "%dm  ", save.skiJumpBest);
            Value(x, 274, 10, s);
        }
        if (Area_Hidden(save, 6, 0x40000)) {
            Text_PrintAt(d, x, 346, kLocked, 3);
        } else {
            Text_PrintAt(d, x, 346, GT_00334548_7, 3);
            Text_PrintAt(d, 70, 370, GT_00335310_6, 3);
            sprintf(s, "%d P  ", save.curlingBest);
            Value(x, 370, 10, s);
        }
    } else {
        const int x = 350;
        if (Area_Hidden(save, 5, 8)) {
            Text_PrintAt(d, x, 58, kLocked, 3);
        } else {
            Text_PrintAt(d, x, 58, GT_00334558_4, 3);
            for (int i = 0; i < 10; ++i) {
                if (i < 9) sprintf(s, GT_0032F280_10, i + 1);
                Text_PrintAt(d, 370, 82 + 24 * i, i < 9 ? s : GT_00335338_5, 3);
                int v = i < 9 ? save.golfHole[i] : save.golfTotal;
                char t[8];
                if (v == 0x7F) strcpy(t, "---");
                else if (v == 0) strcpy(t, "0");
                else sprintf(t, "%+d", v);
                Value(x, 82 + 24 * i, 16, t);
            }
        }
        if (Area_Hidden(save, 9, 0x100000)) {
            Text_PrintAt(d, x, 346, kLocked, 3);
        } else {
            Text_PrintAt(d, x, 346, GT_0032F260_9, 3);
            Text_PrintAt(d, 370, 370, GT_00335310_6, 3);
            int v = save.rallyBest;
            if (v == 0x7FFFFFFF) strcpy(s, "--:--");
            else sprintf(s, "%d:%02d", v / 9000, v % 9000 / 150);
            Value(x, 370, 16, s);
        }
    }
}

void FieldMenu::FieldMenu_Results(gfx::Draw2D& d, const platform::Input& input) {
    BookTask& b = book_;
    if (frame_ == 0) {
        b = BookTask();
        b.state = 2;
    }
    if (b.state == 2) {
        // Left and Right over the 14 spreads: a press wraps, a repeat stops at the ends.
        int to = Menu_MoveCursorGrid(input, 0, b.spread, 14, 1, false);
        if (to > b.spread) {
            b.speed = 0x800;
            b.angle = 0;
            b.state = 3;
        } else if (to < b.spread) {
            b.angle = 0x8000;
            b.speed = -0x800;
            b.state = 3;
        } else if (input.Pressed(0) & platform::kTriangle) {
            audio::Sound_PlaySe(0x1B);
            next_ = kScreenNotebook;
            b.state = 4;
        }
        b.spread = to;
    } else if (b.state == 3) {
        b.angle = (uint16_t)(b.angle + b.speed);
        if (b.angle > 0x8000) {
            b.angle = 0;
            b.speed = 0;
            b.state = 2;
        }
    }
    const gfx::Texture& t = d.Get(kNote, 0);
    Book_DrawGround(d, t);
    Book_Draw(d, t, 3);
    int s = b.spread;
    if (b.speed != 0) {
        Book_DrawTurn(d, t, b.angle);
    } else if (frame_ & 0x20) {
        Book_DrawArrows(d, t, s > 0, 2 * s < 26);
    }
    // The text goes over the sprites, the turning page too: only the page it isn't over is printed (none past the
    // book's ends, as after a wrap).
    int page = -1;
    if (b.speed > 0) page = b.angle < 0x4000 ? 2 * s - 2 : 2 * s + 1;
    else if (b.speed < 0) page = b.angle > 0x4000 ? 2 * s + 3 : 2 * s;
    if (b.speed == 0) {
        Results_DrawPage(d, 2 * s);
        Results_DrawPage(d, 2 * s + 1);
    } else if (page >= 0 && page < 28) {
        Results_DrawPage(d, page);
    }
}

}  // namespace game
