// The field's Start menu, "Pause": a column of little cars towing banners (Warp, Notebook, Radio, Items, Settings,
// Map) over a road, with a sign that says what the chosen one does. A high-level reimplementation of FieldMenu_Task
// (0x00217148, made by Adventure_Task's state 0 on Start), FieldMenu_Open (0x00216FF8, its lists), FieldMenu_Main
// (0x00214AD0), FieldMenu_Warp (0x00214EF8: the towns visited), FieldMenu_Radio (0x002156D0, with Radio_UpdateMenu
// 0x002137B0: the car radio, its station and volume) and the screens' shared list, Menu_PlaceCars
// (0x00212A00) and Menu_UpdateList (0x00212AC8), drawn by Menu_DrawPanel (0x002119A8: the top panel and the
// borders), Menu_DrawSign (0x00211F68), Menu_DrawRoad (0x00212190) and Menu_DrawCars (0x00212720). Everything is
// SYS/POSE.GSL, which the field has loaded. See docs/field_menu.md.
//
// The other screens (field_menu_screens.cpp): Notebook (func_00215220) and its books, Stamps (func_00265B30),
// Pictures (func_002665B8) and Results (func_00267248); Items (func_002161E0), Settings (func_00216738) and the Map
// (FieldMenu_Map, 0x00216E00).
//
#pragma once

#include <stdint.h>

#include <string>
#include <vector>

#include "../../gfx/draw2d.h"
#include "../../gfx/model.h"
#include "../../platform/input.h"
#include "../race/car.h"
#include "../story/photo.h"
#include "../ui/controls_screen.h"
#include "../ui/display_screen.h"
#include "../ui/extras_screen.h"
#include "../ui/option_windows.h"
#include "../ui/window.h"

namespace game {

class StampBook;

// Radio_Start (0x002590B0): the radio's level from the save (+0x12DE, 0-0x80), and its station (+0x12DD: 0 off,
// n station n - 1) tuned to `clock` and playing. The field's start (func_00210430) and its menu's end
// (Adventure_Task's state 2) call it, and so does the Radio screen as it opens.
void Radio_Start(int32_t clock);

class FieldMenu {
public:
    // FieldMenu_Open (0x00216FF8), on the task's first frame: both lists, the cursor on their first item. Warp lists the towns
    // visited (save +0x12DA's bits clear), My Garage first.
    void Open(uint16_t townsUnvisited);

    // FieldMenu_Task (0x00217148), once a frame; draws the frame itself. kWarp: the town (WarpTown) was chosen on Warp.
    // `clock` is the field's (info+0x14): the Radio screen runs it.
    enum Result { kOpen, kResume, kWarp };
    Result Update(gfx::Draw2D& d, const platform::Input& input, int32_t& clock);

    // The Notebook's books on their own, as the developers' test menu (Debug_TestMenuTask, 0x0026B848) runs them as
    // its children: Stamp_BookTask, Pictures_Task and Results_BookTask. Book_Update runs and draws one frame into a
    // frame already begun; false once Triangle has closed the book.
    enum Book { kBookStamps, kBookPictures, kBookResults };
    void Book_Open(Book book);
    bool Book_Update(gfx::Draw2D& d, const platform::Input& input);

    // Warp's town: 0 My Garage, 1-9 the towns (Cloud Hill 8, My City 9).
    int WarpTown() const { return warpTown_; }
    // The task's frames (+8): Adventure_Task's state 2 moves the clock on 1-4 hours by them after a warp.
    int Frames() const { return frames_; }
    // Where data/ is, for the Settings (Controls) screen's pad.
    void SetDataDir(const std::string& dir) { dataDir_ = dir; }
    // The player's car's fixed-point position (x, z: D_0177AC50 + 0xA0, + 0xA8), for the Map's mark; `shown` false
    // on Cloud Hill (course 0x22), where FieldMenu_Map leaves the mark out.
    void SetCar(int32_t x, int32_t z, bool shown = true) {
        carX_ = x;
        carZ_ = z;
        carShown_ = shown;
    }

private:
    // The screens (D_01727B90 +0, the task's switch). 1-6 are the main screen's items' ids.
    enum Screen {
        kScreenMain = 0,
        kScreenWarp = 1,
        kScreenNotebook = 2,
        kScreenRadio = 3,
        kScreenItems = 4,
        kScreenSettings = 5,
        kScreenMap = 6,
        kScreenStamps = 7,      // Notebook's books, child tasks of the menu's (screen 8 is 11's too; 10 unused)
        kScreenPictures = 9,
        kScreenResults = 11,
    };

    // An entry of the list's cars (D_01727B90 + 0x820C, 0x24 bytes; Menu_PlaceCars makes them): the title's (entry 0)
    // and each item's.
    struct Car {
        bool on;      // +0
        int x, y;     // +4, +8: the car's top left; y in pixels
        int colour;   // +0x10: the car's palette (D_0029C388: its id)
        int lit;      // +0x14: the banner white (D_0029C360: palette 14) rather than grey (15)
        int text;     // +0x18: the label's palette by D_0029C448: 0 grey (1), 1 chosen (0), 2 the title's (2)
        int label;    // +0x1C: the label (and the sign's text, D_0029C1A0): 0 Pause, 1-6 the items, 12-21 Warp's
        bool blink;   // +0x20: the banner's lamps blink
    };
    // A list (D_01727B90 + 0x8020 / 0x8088 ..., the state of Menu_UpdateList).
    struct List {
        int state;          // +0
        int count;          // +4: frames left of a move
        int sel;            // +8
        int steps;          // +0xC: scroll steps to go, 4 a row (negative: back)
        int prev;           // +0x10: the item before the move
        int top;            // +0x14: the first row shown
        int labels[11];     // +0x1C: the title's label, then the items'
        int items;          // +0x5C
        int rows;           // +0x60: rows shown at a time
        bool startExits;    // +0x64: Start leaves as Triangle does (the main screen)
    };

    // Menu_PlaceCars (0x00212A00): the title's car and the items' at their places off the right of the screen, the cursor's lit.
    void Menu_PlaceCars(const List& l);
    // Menu_UpdateList (0x00212AC8): one frame of the list: the cars driving in, the cursor (the chosen car pokes out 20 to the left),
    // the scrolling, and the cars driving out. Returns its state, or kChosen (Cross, that frame), kEntered (the cars
    // are gone after Cross) or kLeft (after Triangle).
    int Menu_UpdateList(List& l, const platform::Input& input);
    // Moves the item cars (and the road under them, func_00212190's place) a scroll step.
    void Menu_ScrollStep(int dir);
    // A frame of the main screen (FieldMenu_Main, 0x00214AD0) or Warp's (FieldMenu_Warp, 0x00214EF8): the list and
    // the drawing.
    void FieldMenu_Main(gfx::Draw2D& d, const platform::Input& input);
    Result FieldMenu_Warp(gfx::Draw2D& d, const platform::Input& input);
    // FieldMenu_Radio (0x002156D0): the clock on (Field_TickClock, 0x002114E0), Radio_UpdateMenu (0x002137B0: the
    // title's car in, the pad, the title's car out) and the radio drawn.
    void FieldMenu_Radio(gfx::Draw2D& d, const platform::Input& input, int32_t& clock);
    int Radio_UpdateMenu(const platform::Input& input, int32_t clock);
    void Radio_Draw(gfx::Draw2D& d, int32_t clock) const;
    // The Map (func_00216E00): the world map (SYS/WORLDMAP.GSL) over the whole frame, the car's mark blinking on it;
    // Triangle goes back.
    void FieldMenu_Map(gfx::Draw2D& d, const platform::Input& input);
    // Menu_UpdateTitle (func_00213A78), the screens with the title's car alone: it drives in (16 frames, `w`
    // inactive), then Triangle (SE 0x1B) sends it back out (16) and the result is kListLeft. Cross isn't read.
    int Menu_UpdateTitle(const platform::Input& input, Window* w);
    // FieldMenu_Items (func_002161E0): the items the save has (kind 15's bits), the one under the cursor's
    // description, the money, the coins found, the mileage and the billboards' sponsors' miles.
    void FieldMenu_Items(gfx::Draw2D& d, const platform::Input& input);
    // FieldMenu_Notebook (func_00215220) with Notebook_Update (func_002135C8): the notebook with the license's card,
    // and its three tabs, Stamps, Pictures and Results (Up and Down, wrapping); Cross opens one, Triangle goes back.
    void FieldMenu_Notebook(gfx::Draw2D& d, const platform::Input& input);
    int Notebook_Update(const platform::Input& input);
    // Stamp_BookTask (func_00265B30): the stamp book, a spread of two pages at a time (Left and Right turn them);
    // Triangle goes back to the Notebook.
    void FieldMenu_Stamps(gfx::Draw2D& d, const platform::Input& input);
    // Stamp_DrawPage (func_002652B8): page n (0-99) of the stamp book (the stamp-get book's too).
    static void Stamp_DrawPage(gfx::Draw2D& d, int n);
    friend class StampBook;
    // Pictures_Task (func_002665B8): the photo album, 100 frames, 10 a spread (the d-pad over them, turning the page
    // past the side); Cross on a photo taken shows it (Photo_ViewTask, func_0022BFD0), Triangle goes back.
    void FieldMenu_Pictures(gfx::Draw2D& d, const platform::Input& input);
    void Pictures_DrawPage(gfx::Draw2D& d, int p) const;   // func_002660F8
    // Results_BookTask (func_00267248): the records, 14 spreads: a page for each Adventure race, then the
    // mini-games'. Left and Right turn them (wrapping), Triangle goes back.
    void FieldMenu_Results(gfx::Draw2D& d, const platform::Input& input);
    void Results_DrawPage(gfx::Draw2D& d, int page) const;
    // FieldMenu_Settings (func_00216738): on the PS2 the key setup, each of the car's actions' pad button (see
    // docs/field_menu_settings.md); on the PC a list of Controls (ui/controls_screen.h, which binds the actions to
    // keys and controller buttons in the key setup's place and style), Extras (ui/extras_screen.h) and Other (the
    // title Options' Vibration, Speaker and Sound Volume).
    void FieldMenu_Settings(gfx::Draw2D& d, const platform::Input& input);
    // A screen's drawing: the road, the panel, the sign with `label`'s lines, the arrows, the cars, and with `help`
    // the button hints (Help_Draw(2), the main screen's).
    void Menu_Draw(gfx::Draw2D& d, int label, bool help) const;
    // func_00211F68 and func_00212720.
    void Menu_DrawSign(gfx::Draw2D& d) const;
    void Menu_DrawCars(gfx::Draw2D& d) const;

    List lists_[2];
    // The Radio screen's state (D_01727B90 + 0x8114).
    struct RadioMenu {
        int state;     // +0
        int count;     // +4
        int cursor;    // +8: the button under the cursor, 0 OFF, 1 and 2 the stations
        int playing;   // +0xC: the button that's on
        int volume;    // +0x10: 0-0x80
    } radio_;
    // The title-car screens' state (D_01727B90 + 0x8114: +0 state, +4 frames left).
    struct TitleMenu {
        int state, count;
    } title_;
    // Notebook's state (D_01727B90 + 0x80F0): its cursor stays while the menu is open.
    struct NotebookMenu {
        int state, count, cursor;
    } notebook_;
    // A book's state (the child task's: +0x25 state, +0x02 the page's turn, +0x20 its speed, +0x26 the spread it
    // turns from).
    struct BookTask {
        int state;
        uint16_t angle;
        int speed;
        int oldPage;
        int spread;     // Results' (+0x26)
        int photo;      // the photo Photo_ViewTask shows
    } book_;
    // Settings' screens: its list (list_), and what's open from it.
    enum SettingsChild {
        kSettingsList, kSettingsControls, kSettingsDisplay, kSettingsExtras, kSettingsOther, kSettingsOption
    };
    SettingsChild settingsChild_ = kSettingsList;
    ControlsScreen controls_;
    ExtrasScreen extras_;
    DisplayScreen display_;
    Window other_;           // Other: Vibration, Speaker, Sound Volume
    OptionWindows option_;   // ... and the one chosen
    std::string dataDir_;
    Cars photoCars_;      // Photo_ViewTask's car slot 3: the photo's record's body (loader job 0x70301)
    PhotoCar photoCar_;
    Window list_;                          // the task's window (task + 0x28): Items' list
    std::vector<const char*> listNames_, listDescs_;
    Car cars_[11];
    int screen_, next_;   // +0 and +4: the screen, and the one asked for (the switch starts it the next frame)
    int frame_;           // +8: the screen's frames
    bool done_;           // +0xC: the menu is over
    int roadX_, roadY_;   // +0x81CC, +0x81D0: the road's place (it scrolls with the list)
    bool upArrow_, downArrow_;   // +0x81EC, +0x8204
    int savedTop_;        // +0x10: the top row a list was left at
    int frames_;
    int32_t carX_ = 0, carZ_ = 0;
    bool carShown_ = true;
    int mapState_;        // the Map's state (D_01727B90 + 0x8114): 0x65 the pad, 0x18F done
    int warpTown_;
    int sign_;            // +0x81C4: the chosen car's label, for the sign
};

// The stamp book as it hands out the stamps a talk or a scene earned (save +0x3300's queue, Stamp_Queue): a
// high-level reimplementation of func_00265568 (func_0026D008's task). It opens at the first stamp's spread; after
// 12 frames the stamp lands on its page over 10 frames, from three times its size, and is earned there (Stamp_Earn,
// SE 0x21); a press (SE 0x1A) turns to the next one's spread (or stays on this one); after the last, 10 more
// frames. After a building (not `field`) the last picture then darkens for 129 frames. Drawn on the Notebook's
// stamp pages (SYS/STAMP.GSL).
class StampBook {
public:
    // False, with nothing to show, if the queue is empty.
    bool Start(bool field);
    // One frame, drawn (the whole screen). Holding(): this frame only darkens the last one (draw nothing, put a
    // 0x06000000 black over the held picture). True once it's over.
    bool Update(gfx::Draw2D& d, const platform::Input& input);
    bool Holding() const { return state_ == 7; }

private:
    bool field_ = true;
    int state_ = 6;     // +0x25
    int count_ = 0;     // +0x02: frames, or the turning page's angle
    int level_ = 0;     // +0x20: the stamp's height above its page (10 down to 0), or the turn's speed
    int oldPage_ = 0;   // +0x26: the page before a turn
};

}  // namespace game
