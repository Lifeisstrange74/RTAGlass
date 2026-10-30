#include "field_menu.h"

#include <math.h>
#include <string.h>

#include <algorithm>
#include <vector>

#include "../../audio/sound.h"
#include "../extras.h"
#include "../save/save_data.h"
#include "../text.h"
#include "field_menu_draw.h"
#include "../../game_text.inc"   // the game's text, GT_<address>_<length>, made from the ELF

namespace game {

namespace menu {

void Piece_Draw(gfx::Draw2D& d, const gfx::Texture& t, const Piece& p, int x, int y) {
    float u = (float)p.u, v = (float)p.v, w = (float)p.w, h = (float)p.h;
    float uw = w, vh = h;
    if (p.flip & 1) {
        u += w;
        uw = -w;
    }
    if (p.flip & 2) {
        v += h;
        vh = -h;
    }
    d.Sprite(t, (float)(x + p.x), (float)(y + p.y), w, h, u, v, uw, vh);
}

// Menu_DrawPanel (0x002119A8): the dark panel behind the title (top left) and the yellow borders with their lights along the top
// and the bottom (POSE picture 0).
void Menu_DrawPanel(gfx::Draw2D& d) {
    const gfx::Texture& t = d.Get(kPose, 0);
    d.Sprite(t, 0, 0, 24, 73, 184, 0, 24, 73);
    for (int i = 0; i < 30; ++i) d.Sprite(t, 24.0f + 8 * i, 0, 8, 73, 208, 0, 8, 73);
    d.Sprite(t, 264, 0, 32, 73, 216, 0, 32, 73);
    d.Sprite(t, 296, 0, 24, 33, 184, 73, 24, 33);
    // The border, 64 pixels to a light: two plain strips, the light's left, two strips, its right.
    auto Border = [&](int x, int y) {
        d.Sprite(t, (float)x, (float)y, 8, 33, 208, 73, 8, 33);
        d.Sprite(t, x + 8.0f, (float)y, 8, 33, 208, 73, 8, 33);
        d.Sprite(t, x + 16.0f, (float)y, 14, 33, 216, 73, 14, 33);
        d.Sprite(t, x + 30.0f, (float)y, 8, 33, 208, 73, 8, 33);
        d.Sprite(t, x + 38.0f, (float)y, 8, 33, 208, 73, 8, 33);
        d.Sprite(t, x + 46.0f, (float)y, 18, 33, 230, 73, 18, 33);
    };
    for (int i = 0; i < 5; ++i) Border(320 + 64 * i, 0);
    for (int i = 0; i < 10; ++i) Border(64 * i, 0x1A0);
}

// Menu_DrawRoad (0x00212190), a screen's background from (x, y) by its kind. Kinds 0 and 1, the main screen's and
// Warp's road: POSE picture 2 palette 13, 10 x 9 tiles of the blue ground, 15 white bars down the left and 6 x 11
// lane dashes. Kind 3, the Radio's: 10 x 7 tiles of radio waves (picture 3 palette 5). Kinds 2, 4 and 5, the
// Notebook's, Items' and Settings', below.
void Menu_DrawRoad(gfx::Draw2D& d, int x, int y, int kind) {
    if (kind == 3) {
        const gfx::Texture& w = d.Get(kPose, 3, 5);
        for (int r = 0; r < 7; ++r)
            for (int c = 0; c < 10; ++c) d.Sprite(w, x + 64.0f * c, y + 66.0f * r, 64, 66, 192, 48, 64, 66);
        return;
    }
    if (kind == 4) {
        // Items': 14 x 10 tiles of the green palms (POSE picture 3 palette 5, D_002EAB48).
        const gfx::Texture& p = d.Get(kPose, 3, 5);
        for (int r = 0; r < 10; ++r)
            for (int c = 0; c < 14; ++c) d.Sprite(p, x + 48.0f * c, y + 48.0f * r, 48, 48, 144, 48, 48, 48);
        return;
    }
    if (kind == 2) {
        // Notebook's: 10 x 7 tiles of pencils (D_0029C030 entry 5, SYS/NOTE.GSL's first picture; D_002EAB08).
        const gfx::Texture& n = d.Get("SYS/NOTE.GSL", 0);
        for (int r = 0; r < 7; ++r)
            for (int c = 0; c < 10; ++c) d.Sprite(n, x + 64.0f * c, y + 64.0f * r, 64, 64, 128, 128, 64, 64);
        return;
    }
    if (kind == 5) {
        // Settings': 5 x 4 tiles of pads and buttons (entry 4, SYS/OPTION2.GSL), 128 x 128 from its corner.
        const gfx::Texture& o = d.Get("SYS/OPTION2.GSL", 0);
        for (int r = 0; r < 4; ++r)
            for (int c = 0; c < 5; ++c) d.Sprite(o, x + 128.0f * c, y + 128.0f * r, 128, 128, 0, 0, 128, 128);
        return;
    }
    const gfx::Texture& t = d.Get(kPose, 2, 13);
    for (int r = 0; r < 9; ++r)
        for (int c = 0; c < 10; ++c) d.Sprite(t, x + 80.0f * c, y + 80.0f * r, 80, 80, 104, 0, 80, 80);
    for (int k = 0; k < 15; ++k) d.Sprite(t, (float)x, y + 56.0f + 40 * k, 100, 20, 0, 68, 100, 20);
    for (int r = 0; r < 11; ++r)
        for (int c = 0; c < 6; ++c) d.Sprite(t, x + 125.0f + 100 * c, y + 85.0f + 56 * r, 75, 7, 104, 80, 75, 7);
}

void Line_DrawStrip(gfx::Draw2D& d, const float* px, const float* py, const uint32_t* rgba, int n) {
    std::vector<gfx::Vertex> v;
    for (int k = 0; k + 1 < n; ++k) {
        float dx = px[k + 1] - px[k], dy = py[k + 1] - py[k], len = sqrtf(dx * dx + dy * dy);
        float nx = len ? -dy / len * 0.5f : 0, ny = len ? dx / len * 0.5f : 0;
        uint32_t c0 = gfx::Draw2D::Color(rgba[k]), c1 = gfx::Draw2D::Color(rgba[k + 1]);
        const float cx[4] = {px[k] + nx, px[k] - nx, px[k + 1] + nx, px[k + 1] - nx};
        const float cy[4] = {py[k] + ny, py[k] - ny, py[k + 1] + ny, py[k + 1] - ny};
        const uint32_t cc[4] = {c0, c0, c1, c1};
        static const int kTri[6] = {0, 1, 2, 1, 3, 2};
        for (int t : kTri) {
            gfx::Vertex q = gfx::Vertex();
            q.x = cx[t] + 0.5f;
            q.y = cy[t] + 0.5f;
            q.z = 1;
            q.color = cc[t];
            q.q = 1;
            q.fog = 1;
            v.push_back(q);
        }
    }
    gfx::Draw3DState st;
    st.depthTest = st.depthWrite = false;
    d.Triangles(v.data(), (uint32_t)v.size(), nullptr, false, false,
                gfx::Rect{0, 0, gfx::Draw2D::kWidth, gfx::Draw2D::kHeight}, st);
}

}  // namespace menu

using namespace menu;

namespace {

const int kMainItems = 6, kRows = 6;
const int32_t kClockDay = 0x34BC0;   // the clock's day: 24 hours of 9000 frames

const Piece kSign[7] = {
    {0, 0, 0, 0, 88, 88, 0},    {88, 0, 0, 0, 88, 88, 1},     {0, 88, 0, 0, 88, 88, 2}, {88, 88, 0, 0, 88, 88, 3},
    {26, 176, 88, 0, 64, 48, 0}, {90, 176, 88, 48, 64, 48, 0}, {72, 224, 152, 0, 32, 64, 0},
};
const int kSignX = 0x1B8, kSignY = 0xA0;
// D_0029C300: the scroll arrows above and below the list (picture 0).
const Piece kArrows[2] = {{40, 94, 0, 88, 38, 22, 2}, {40, 374, 0, 88, 38, 22, 0}};
// D_0029C360 and D_0029C448: the banner's palette (grey, white) and the label's (grey, chosen, the title).
const int kBannerPalette[2] = {15, 14};
const int kLabelPalette[3] = {1, 0, 2};

// D_0029C1A0: the sign's four lines for each label (7-11 are left over: Japanese names for Stamps, Memo, Pictures
// and two more).
const char* const kSignText[22][4] = {
    {GT_003334E0_5, "", "", ""},
    {GT_003334D0_7, GT_003334C8_6, GT_002EA8D0_8, "visited"},
    {GT_003334B8_6, GT_002EA8C0_8, GT_002EA8B0_8, "info"},
    {GT_003334A8_6, "radio", "stations", ""},
    {GT_00333498_5, "your", "treasure", ""},
    {GT_003334A8_6, "button", "settings", ""},
    {GT_00333498_5, "your", "current", "position"},
    {"", "", "", ""}, {"", "", "", ""}, {"", "", "", ""}, {"", "", "", ""}, {"", "", "", ""},
    {GT_003334D0_7, "your", "garage", "anytime"},
    {GT_00333458_4, GT_00333450_7, "peach", "trees"},
    {GT_002EA830_8, GT_00333438_5, "castle", ""},
    {GT_00333428_7, "glaring", "neons", "lights"},
    {GT_00333428_7, "steep", "rocky", "valleys"},
    {GT_002EA820_8, "grows", "around", GT_002EA810_8},
    {GT_002EA800_8, "covered", "with", "snow"},
    {GT_003333C8_6, "with", "natural", "beauty"},
    {GT_002EA7F0_8, "island", "with", "rainbow"},
    {GT_002EA7E0_8, GT_003333A0_7, "develop", ""},
};

}  // namespace

void FieldMenu::Open(uint16_t townsUnvisited) {
    memset(lists_, 0, sizeof lists_);
    // The main screen: "Pause", then the items 1-6 (D_0029CA30); Start leaves it too.
    List& m = lists_[0];
    for (int i = 0; i <= kMainItems; ++i) m.labels[i] = i;
    m.items = kMainItems;
    m.rows = kRows;
    m.startExits = true;
    // Warp: "Warp", then My Garage and the towns (D_0029CA50: labels 12-21) whose bit is clear.
    List& w = lists_[1];
    w.labels[0] = 1;
    int n = 1;
    for (int i = 0; i < 10; ++i)
        if (!((townsUnvisited >> i) & 1)) w.labels[n++] = 12 + i;
    w.items = n - 1;
    w.rows = kRows;
    screen_ = next_ = kScreenMain;
    frame_ = 0;
    done_ = false;
    savedTop_ = 0;
    frames_ = 0;
    warpTown_ = 0;
    notebook_ = NotebookMenu();
    memset(cars_, 0, sizeof cars_);
    roadX_ = roadY_ = 0;
    upArrow_ = downArrow_ = false;
    sign_ = 0;
}

void FieldMenu::Menu_PlaceCars(const List& l) {
    Car& t = cars_[0];
    t = Car{true, 0x2A2, 12, 0, 1, 2, l.labels[0], true};
    for (int i = 0; i < l.items; ++i) {
        bool at = l.sel == i;
        // D_0029C388: the car's palette is its id (1 on), 0-7 over.
        cars_[i + 1] = Car{true, 0x2E4 + 20 * i, 0x50 + 0x38 * i, (i + 1) & 7, at, at, l.labels[i + 1], at};
    }
}

void FieldMenu::Menu_ScrollStep(int dir) {
    roadX_ += 5 * dir;
    roadY_ += 14 * dir;
    for (int i = 1; i < 11; ++i) {
        if (!cars_[i].on) continue;
        cars_[i].x += 5 * dir;
        cars_[i].y += 14 * dir;
    }
}

int FieldMenu::Menu_UpdateList(List& l, const platform::Input& input) {
    auto Scroll = [&]() {
        if (l.steps < 0) {
            ++l.steps;
            Menu_ScrollStep(1);
        } else if (l.steps > 0) {
            --l.steps;
            Menu_ScrollStep(-1);
        }
    };
    auto Drive = [&](int from, int except, int dx) {
        for (int i = from; i <= l.items; ++i)
            if (i != except) cars_[i].x += dx;
    };
    int maxTop = l.items - l.rows > 0 ? l.items - l.rows : 0;
    switch (l.state) {
    case kListInit: {
        Car& c = cars_[l.sel + 1];
        c.lit = c.text = 1;
        c.blink = true;
        sign_ = c.label;
        c.x -= 20;
        // Back to the top row the last list was left at, then scrolled from there to this one's.
        for (int k = 0; k < savedTop_ * 4; ++k) Menu_ScrollStep(-1);
        l.state = kListScroll;
        l.steps = (l.top - savedTop_) * 4;
    }
    // fall through
    case kListScroll:
        Scroll();
        if (l.steps == 0) {
            l.count = 10;
            l.state = kListTitleIn;
        }
        break;
    case kListTitleIn:
        cars_[0].x -= kDrive;
        if (--l.count == 0) {
            l.state = kListAllIn;
            l.count = 6;
        }
        break;
    case kListAllIn:
        Drive(0, -1, -kDrive);
        if (--l.count == 0) {
            l.count = 10;
            l.state = kListItemsIn;
        }
        break;
    case kListItemsIn:
        Drive(1, -1, -kDrive);
        if (--l.count == 0) l.state = kListReady;
        break;
    case kListReady:
        upArrow_ = l.top != 0;
        downArrow_ = l.top != maxTop;
        l.state = kListIdle;
    // fall through
    case kListIdle: {
        uint32_t pressed = input.Pressed(0);
        if (pressed & platform::kCross) {
            audio::Sound_PlaySe(0x1A);
            upArrow_ = downArrow_ = false;
            l.state = kListChosen;
            break;
        }
        if ((pressed & platform::kTriangle) || ((pressed & platform::kStart) && l.startExits)) {
            audio::Sound_PlaySe(0x1B);
            upArrow_ = downArrow_ = false;
            l.state = kListExit;
            break;
        }
        int to = l.items > 0 ? Menu_MoveCursor(input, 0, l.sel, l.items, false) : l.sel;
        if (to == l.sel) break;
        Car& now = cars_[to + 1];
        Car& was = cars_[l.sel + 1];
        now.lit = now.text = 1;
        now.blink = true;
        sign_ = now.label;
        was.lit = was.text = 0;
        was.blink = false;
        l.prev = l.sel;
        l.sel = to;
        l.count = 4;
        l.steps = 0;
        // Two rows kept above and below the cursor while there are more that way.
        if (to < l.prev) {
            while (l.top != 0 && l.sel < l.top + 2) {
                --l.top;
                l.steps -= 4;
            }
        } else {
            while (l.top != maxTop && l.sel >= l.top + l.rows - 2) {
                ++l.top;
                l.steps += 4;
            }
        }
        upArrow_ = l.top != 0;
        downArrow_ = l.top != maxTop;
        l.state = kListMove;
        break;
    }
    case kListMove:
        if (l.count != 0) {
            --l.count;
            cars_[l.sel + 1].x -= 5;
            cars_[l.prev + 1].x += 5;
        }
        Scroll();
        if (l.count == 0 && l.steps == 0) l.state = kListReady;
        break;
    case kListChosen:
        l.state = kListChosenOut;
        l.count = 10;
    // fall through
    case kListChosenOut:
        cars_[l.sel + 1].x -= kDrive;
        if (--l.count == 0) {
            l.count = 6;
            l.state = kListAllOut;
        }
        break;
    case kListAllOut:
        Drive(0, -1, -kDrive);
        if (--l.count == 0) {
            l.count = 10;
            l.state = kListRestOut;
        }
        break;
    case kListRestOut:
        Drive(0, l.sel + 1, -kDrive);
        if (--l.count == 0) l.state = kListGone;
        break;
    case kListGone:
        savedTop_ = l.top;
        l.state = kListEntered;
        break;
    case kListExit:
        l.count = 16;
        l.state = kListExitOut;
    // fall through
    case kListExitOut:
        Drive(0, -1, kDrive);
        if (--l.count == 0) l.state = kListExitGone;
        break;
    case kListExitGone:
        savedTop_ = l.top;
        l.state = kListLeft;
        break;
    default:
        l.count = 0;
        l.state = kListInit;
        break;
    }
    return l.state;
}

void FieldMenu::Menu_Draw(gfx::Draw2D& d, int label, bool help) const {
    // The road is the farthest, then the panel, the sign, and the arrows and the cars: the PS2 draws them in
    // another order with each sprite call's z (its last argument) sorting them. (func_0024F728 starts a batch of
    // that many sprites.)
    Menu_DrawRoad(d, roadX_, roadY_);
    Menu_DrawPanel(d);
    Menu_DrawSign(d);
    // The arrows blink, 16 frames on and 16 off.
    const gfx::Texture& t0 = d.Get(kPose, 0);
    if (frame_ & 0x10) {
        if (upArrow_) Piece_Draw(d, t0, kArrows[0], 0, 0);
        if (downArrow_) Piece_Draw(d, t0, kArrows[1], 0, 0);
    }
    Menu_DrawCars(d);
    if (help) Help_Draw(d, 2);
    // Menu_DrawSign's lines (D_0029C1A0), white, 24 apart, over everything. During Q's Rally (race flag 0x1000)
    // Warp's signs (labels 1 and 12-21) warn instead, 12 higher, "Warning!" in colour 6.
    if (Rally_Running() && (label == 1 || (label >= 12 && label <= 21))) {
        static const char* const kWarning[4] = {GT_002EAAE8_8, GT_00333548_7, GT_002EAAF8_8, "rally"};
        for (int k = 0; k < 4; ++k) Text_PrintAt(d, kSignX + 0x1E, kSignY + 0x1C + 24 * k, kWarning[k], k ? 0 : 6);
    } else if (label >= 0 && label < 22) {
        for (int k = 0; k < 4; ++k) Text_PrintAt(d, kSignX + 0x1E, kSignY + 0x28 + 24 * k, kSignText[label][k], 0);
    }
}

// Menu_DrawSign (0x00211F68): the sign at (0x1B8, 0xA0) (D_01727B90 + 0x81B4). Its lines are drawn last.
void FieldMenu::Menu_DrawSign(gfx::Draw2D& d) const {
    const gfx::Texture& t = d.Get(kPose, 0);
    for (const Piece& p : kSign) Piece_Draw(d, t, p, kSignX, kSignY);
}

// Menu_DrawCars (0x00212720): each car (POSE picture 2) towing its banner, the banner's lamps (palette 12; a lit
// car's turn every 16 frames) and its label (picture 1).
void FieldMenu::Menu_DrawCars(gfx::Draw2D& d) const {
    const gfx::Texture& labels = d.Get(kPose, 1, 0);
    for (const Car& c : cars_) {
        if (!c.on) continue;
        d.Sprite(d.Get(kPose, 2, kBannerPalette[c.lit]), c.x + 83.0f, c.y + 4.0f, 156, 40, 0, 88, 156, 40);
        d.Sprite(d.Get(kPose, 2, c.colour), (float)c.x, (float)c.y, 83, 50, 0, 0, 83, 50);
        const gfx::Texture& lamps = d.Get(kPose, 2, 12);
        int f = c.blink ? frame_ >> 4 : 0;
        d.Sprite(lamps, c.x + 94.0f, c.y + 10.0f, 6, 24, 160.0f + 6 * (f & 3), 104, 6, 24);
        d.Sprite(lamps, c.x + 226.0f, c.y + 8.0f, 6, 24, 160.0f + 6 * ((f + 1) & 3), 104, 6, 24);
        const gfx::Texture& name = kLabelPalette[c.text] == 0 ? labels : d.Get(kPose, 1, kLabelPalette[c.text]);
        d.Sprite(name, c.x + 99.0f, c.y + 10.0f, 128, 22, (float)(c.label / 11 * 128), (float)(c.label % 11 * 22),
                 128, 22);
    }
}

void FieldMenu::FieldMenu_Main(gfx::Draw2D& d, const platform::Input& input) {
    List& l = lists_[0];
    if (frame_ == 0) {
        l.state = kListInit;
        l.count = 0;
        Menu_PlaceCars(l);
    }
    int r = Menu_UpdateList(l, input);
    if (r == kListEntered) {
        // The chosen item's id is its screen. (On kListChosen the PS2 starts loading Notebook's, Settings' and the
        // Map's files: func_0020DC50 with 0x340005, 0x3C0300 and 0x34000E, 0x340016.)
        int id = l.labels[l.sel + 1];
        if (id >= kScreenWarp && id <= kScreenMap) {
            next_ = id;
        } else {
            done_ = true;
        }
    } else if (r == kListLeft) {
        done_ = true;
    }
    Menu_Draw(d, sign_, true);
}

FieldMenu::Result FieldMenu::FieldMenu_Warp(gfx::Draw2D& d, const platform::Input& input) {
    List& l = lists_[1];
    if (frame_ == 0) {
        l.state = kListInit;
        l.count = 0;
        Menu_PlaceCars(l);
    }
    int r = Menu_UpdateList(l, input);
    Menu_Draw(d, sign_, false);
    if (r == kListChosen) {
        // Cross warps at once (the task's parent gets the town, labels 12-21 less 12): Adventure_Task's state 2
        // takes it to state 3, the loader and the town's Q's Factory.
        int label = l.labels[l.sel + 1];
        warpTown_ = label >= 12 && label < 22 ? label - 12 : 0;
        return kWarp;
    }
    if (r == kListEntered) done_ = true;
    if (r == kListLeft) next_ = kScreenMain;
    return kOpen;
}

// The radio's buttons: OFF and the stations, Peach FM and E-Radio, and with the Q-Wave extra Q-Wave (SNDMOD's
// station 2, which the PS2's radio has no button for; docs/extras.md).
namespace {
int Radio_Buttons() { return Extras_On(kExtraQWave) ? 4 : 3; }
}  // namespace

void Radio_Start(int32_t clock) {
    SaveRecord& r = Save_Current();
    int level = r.radioVolume * 0x3FFF / 128;
    audio::Sound_RadioVolume(level, level);
    if (r.radioStation >= Radio_Buttons()) r.radioStation = 0;   // Q-Wave, its extra since turned off
    if (r.radioStation > 0) {
        audio::Sound_RadioTune(r.radioStation - 1, clock);
        audio::Sound_RadioPlay();
    }
}

int FieldMenu::Radio_UpdateMenu(const platform::Input& input, int32_t clock) {
    RadioMenu& m = radio_;
    SaveRecord& save = Save_Current();
    switch (m.state) {
    case 0:
        // The radio as the save has it, playing from the clock.
        Radio_Start(clock);
        m.cursor = m.playing = save.radioStation;
        m.volume = save.radioVolume;
        m.state = 1;
    // fall through
    case 1:
        m.count = 16;
        m.state = 2;
    // fall through
    case 2:
        // The title's car drives in, 16 frames.
        cars_[0].x -= kDrive;
        if (--m.count == 0) m.state = kListReady;
        break;
    case kListReady:
        m.state = kListIdle;
    // fall through
    case kListIdle: {
        uint32_t pressed = input.Pressed(0), held = input.Held(0);
        if (pressed & platform::kCross) {
            // Cross turns the button under the cursor on: the station from the clock, or OFF.
            audio::Sound_PlaySe(0x1A);
            if (m.cursor != m.playing) {
                m.playing = m.cursor;
                save.radioStation = (int8_t)m.cursor;
                if (m.playing) {
                    audio::Sound_RadioTune(m.playing - 1, clock);
                    audio::Sound_RadioPlay();
                } else {
                    audio::Sound_RadioStop();
                }
            }
            break;
        }
        if (pressed & platform::kTriangle) {
            audio::Sound_PlaySe(0x1B);
            m.state = kListExit;
            break;
        }
        // Down held turns the volume down, Up up, a step a frame (0-0x80, kept in the save); otherwise Left and
        // Right move the cursor over the three buttons (four with Q-Wave).
        if ((held & platform::kDown) && m.volume > 0) {
            save.radioVolume = (int16_t)--m.volume;
        } else if ((held & platform::kUp) && m.volume < 0x80) {
            save.radioVolume = (int16_t)++m.volume;
        } else {
            m.cursor = Menu_MoveCursorGrid(input, 0, m.cursor, Radio_Buttons(), 1, false);
        }
        int level = m.volume * 0x3FFF / 128;
        audio::Sound_RadioVolume(level, level);
        break;
    }
    case kListExit:
        m.count = 16;
        m.state = kListExitOut;
    // fall through
    case kListExitOut:
        // Gone: the radio stops with the screen (0x18F falls into OFF's Sound_RadioStop), so the rest of the menu is
        // quiet until it closes and Radio_Start plays it again.
        cars_[0].x += kDrive;
        if (--m.count == 0) {
            m.state = kListLeft;
            audio::Sound_RadioStop();
        }
        break;
    default:
        m.state = 0;
        m.count = 0;
        break;
    }
    return m.state;
}

void FieldMenu::Radio_Draw(gfx::Draw2D& d, int32_t clock) const {
    const RadioMenu& m = radio_;
    const int x = 0xA5, y = 0xA0;   // the radio's top left
    // The body (POSE picture 3, palette 0): its left end with the knob, 22 strips of its middle, its right end.
    const gfx::Texture& body = d.Get(kPose, 3, 0);
    d.Sprite(body, (float)x, (float)y, 96, 128, 0, 0, 96, 128);
    for (int i = 0; i < 22; ++i) d.Sprite(body, x + 96.0f + 8 * i, (float)y, 8, 128, 96, 0, 8, 128);
    d.Sprite(body, x + 272.0f, (float)y, 38, 128, 104, 0, 38, 128);
    // Picture 0: the cursor's orange arrow over its button.
    const gfx::Texture& t0 = d.Get(kPose, 0);
    d.Sprite(t0, (float)(x + 0x68 + 0x2E * m.cursor), y - 30.0f, 38, 22, 0, 88, 38, 22);
    // The clock, HH:MM in the display's digits (10 x 14 from u 70; 10 is the colon), the minutes' last digit at x + 250.
    int hours = clock / 9000, minutes = clock % 9000 / 150;
    const int digits[5] = {minutes % 10, minutes / 10, 10, hours % 10, hours / 10 % 10};
    for (int i = 0; i < 5; ++i)
        d.Sprite(t0, x + 250.0f - 10 * i, y + 58.0f, 10, 14, 70.0f + 10 * digits[i], 96, 10, 14);
    // The knob's mark, turned by the volume over 240 degrees (D_00332B34, D_00332B38: -2/3 to 2/3 of pi), 16 pixels
    // out from its middle at (x + 54, y + 62).
    float a = (m.volume * (4.f / 3.f) / 128.f - 2.f / 3.f) * 3.14159265f, c = cosf(a), s = sinf(a);
    const float corner[4][2] = {{-3, -19}, {3, -19}, {-3, -13}, {3, -13}};
    float qx[4], qy[4];
    for (int i = 0; i < 4; ++i) {
        qx[i] = x + 54 + corner[i][0] * c - corner[i][1] * s;
        qy[i] = y + 62 + corner[i][0] * s + corner[i][1] * c;
    }
    d.Quad(t0, qx, qy, 71, 97, 8, 12);
    if (m.playing) {
        // The station's name (Peach FM, E-Radio, Q-Wave: 72 x 18 at u 72 x station) and its equaliser: eight columns of up to
        // seven bars, each a table's height (D_0029C458, a row every 32 frames) plus the frames % 3.
        d.Sprite(t0, x + 112.0f, y + 56.0f, 72, 18, 72.0f * (m.playing - 1), 110, 72, 18);
        static const signed char kEq[2][4][8] = {
            {{7, 5, 2, 3, 3, 3, 2, 2}, {6, 4, 1, 2, 2, 4, 1, 1}, {5, 3, 0, 1, 1, 2, 3, 2}, {4, 7, 1, 0, 2, 4, 2, 2}},
            {{2, 3, 2, 1, 4, 6, 5, 5}, {1, 4, 1, 2, 2, 7, 4, 6}, {2, 2, 4, 1, 3, 4, 3, 4}, {1, 1, 1, 0, 1, 5, 6, 7}},
        };
        const signed char* row = kEq[(m.playing - 1) & 1][(frame_ >> 5) & 3];
        for (int col = 0; col < 8; ++col) {
            int n = std::max(0, std::min(7, row[col] + frame_ % 3));
            for (int j = 0; j < n; ++j)
                d.Sprite(t0, x + 108.0f + 20 * col, y + 108.0f - 4 * j, 16, 2, 216, 112, 16, 2);
        }
    }
    // The buttons (picture 3: palette 1 and u 144 the one on, palette 2 and u 192 the others) and their labels: OFF,
    // 1 and 2, and with Q-Wave the labels' unused 3.
    for (int k = 0; k < Radio_Buttons(); ++k) {
        bool on = k == m.playing;
        const gfx::Texture& b = d.Get(kPose, 3, on ? 1 : 2);
        d.Sprite(b, x + 100.0f + 46 * k, y + 8.0f, 46, 36, on ? 144.0f : 192.0f, 0, 46, 36);
        if (k == 0)
            d.Sprite(b, x + 107.0f, y + 20.0f, 32, 12, 144, on ? 96.0f : 112.0f, 32, 12);
        else
            d.Sprite(b, x + 114.0f + 46 * k, y + 20.0f, 16, 12, (on ? 0x80 : 0xB0) + 16.0f * k, 36, 16, 12);
    }
}

void FieldMenu::FieldMenu_Radio(gfx::Draw2D& d, const platform::Input& input, int32_t& clock) {
    if (frame_ == 0) {
        // The title's car alone ("Radio"), over the radio waves.
        radio_.state = 0;
        radio_.count = 0;
        List title = {};
        title.labels[0] = 3;
        Menu_PlaceCars(title);
    }
    // Field_TickClock (0x002114E0): the clock runs here, a frame at a time round the day, so the station plays on
    // in step with it; Q's Rally's timer too.
    if (clock++ > kClockDay) clock -= kClockDay;
    Rally_Tick();
    if (Radio_UpdateMenu(input, clock) == kListLeft) next_ = kScreenMain;
    Menu_DrawRoad(d, roadX_, roadY_, 3);
    Menu_DrawPanel(d);
    Menu_DrawCars(d);
    Radio_Draw(d, clock);
}

void FieldMenu::FieldMenu_Map(gfx::Draw2D& d, const platform::Input& input) {
    if (frame_ == 0) mapState_ = 0;
    if (mapState_ == 0 || mapState_ == kListReady) mapState_ = kListIdle;
    if (mapState_ == kListIdle && (input.Pressed(0) & platform::kTriangle)) {
        audio::Sound_PlaySe(0x1B);
        mapState_ = kListLeft;
    }
    if (mapState_ == kListLeft) next_ = kScreenMain;
    d.Sprite(d.Get("SYS/WORLDMAP.GSL", 0), 0, 0, 640, 448, 0, 0, 640, 448);
    // The car's mark (D_00333580: the font's little car, Shift-JIS 8E A9) in palette 4, 32 frames on and 32 off:
    // the world's eight blocks across, wrapped from a block in, and its rows up from y 0x1A2, 594240 fixed to a
    // pixel; not on Cloud Hill (course 0x22). func_0024F530 halves y; Text_PrintAt doubles it.
    if ((frames_ & 0x20) && carShown_) {
        int x = (int)((((uint32_t)carX_ + 0x02000000u) & 0x0FFFFFFFu) / 594240) + 0x82;
        int y = 0x1A2 - carZ_ / 594240;
        Text_PrintAt(d, x, y / 2 * 2, "\x8E\xA9", 4);
    }
}

void FieldMenu::Book_Open(Book book) {
    static const int kScreens[3] = {kScreenStamps, kScreenPictures, kScreenResults};
    screen_ = next_ = kScreens[book];
    frame_ = frames_ = 0;
    done_ = false;
}

// The book's own screen runs until it asks for the Notebook (where Triangle sends it back in the field menu).
bool FieldMenu::Book_Update(gfx::Draw2D& d, const platform::Input& input) {
    if (next_ != screen_) return false;
    switch (screen_) {
    case kScreenStamps: FieldMenu_Stamps(d, input); break;
    case kScreenPictures: FieldMenu_Pictures(d, input); break;
    case kScreenResults: FieldMenu_Results(d, input); break;
    }
    ++frame_;
    ++frames_;
    return true;
}

FieldMenu::Result FieldMenu::Update(gfx::Draw2D& d, const platform::Input& input, int32_t& clock) {
    // A new screen starts on the next frame with its frames at 0 and the cars, the road and the arrows cleared
    // (func_0027E840 on D_01727B90 + 0x81A4, 0x1F4 bytes).
    if (screen_ != next_) {
        screen_ = next_;
        frame_ = 0;
        memset(cars_, 0, sizeof cars_);
        roadX_ = roadY_ = 0;
        upArrow_ = downArrow_ = false;
    }
    // Its screens are laid out on 4:3 grounds: a wide screen's sides are black.
    d.Pillarbox();
    Result result = kOpen;
    switch (screen_) {
    case kScreenMain:
        FieldMenu_Main(d, input);
        break;
    case kScreenWarp:
        result = FieldMenu_Warp(d, input);
        break;
    case kScreenRadio:
        FieldMenu_Radio(d, input, clock);
        break;
    case kScreenNotebook:
        FieldMenu_Notebook(d, input);
        break;
    case kScreenItems:
        FieldMenu_Items(d, input);
        break;
    case kScreenSettings:
        FieldMenu_Settings(d, input);
        break;
    case kScreenStamps:
        FieldMenu_Stamps(d, input);
        break;
    case kScreenPictures:
        FieldMenu_Pictures(d, input);
        break;
    case kScreenResults:
        FieldMenu_Results(d, input);
        break;
    case kScreenMap:
        FieldMenu_Map(d, input);
        break;
    }
    ++frame_;
    ++frames_;
    if (result == kWarp) return kWarp;
    return done_ ? kResume : kOpen;
}

}  // namespace game
