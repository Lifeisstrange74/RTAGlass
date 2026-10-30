#include "test_menu.h"

#include <string.h>

#include "../../audio/sound.h"
#include "../field/field.h"
#include "../field/field_town_cars.h"
#include "../save/save_data.h"
#include "../story/messages.h"
#include "../story/parts_screen.h"
#include "../story/race_list.h"
#include "../story/room_scene.h"
#include "../text.h"
#include "../../game_text.inc"   // the game's text, GT_<address>_<length>, made from the ELF

namespace game {

namespace {

// Debug_TestMenuTask's lines (D_003355F8, D_0032F920-D_0032F9A0, D_00335600, D_0032F9B0), in jtbl_0032F9C0's order.
const char* const kItems[12] = {GT_003355F8_7,        GT_0032F920_11, GT_0032F930_9,   GT_0032F940_15,
                                GT_0032F950_12,   GT_0032F960_10,  GT_0032F970_14, GT_0032F980_11,
                                GT_0032F990_10,     GT_0032F9A0_11, GT_00335600_7,     GT_0032F9B0_13};
enum Item {
    kGoTown, kRaceCourse, kRaceSolo, kWorldGrandPrix, kActionStage, kStampNote, kPhotoBook, kResultNote,
    kNameEntryItem, kShowEnding, kToShop, kGoBackTitle
};
// Debug_EndingMenuTask's lines (D_0032FA30-D_0032FAF0), Japanese in the ELF ("endingu 1" ... "endingu 10",
// "endingu furu", "shiataa"), translated.
const char* const kEndingItems[12] = {"Ending 1", "Ending 2", "Ending 3", "Ending 4",  "Ending 5",    "Ending 6",
                                      "Ending 7", "Ending 8", "Ending 9", "Ending 10", "Ending Full", "Theater"};
// D_002DCC78: the course files by course id (the race list's first column).
const char* const kCourseFiles[18] = {"C00", "C01", "C02", "C03", "C04", "C05", "C06", "C07", "C08",
                                      "C09", "C10", "C11", "C12", "C13", "C14", "C18", "C19", "C20"};
// D_002DCCC0: the rank letters by a race row's +3.
const char* const kRanks[5] = {"C", "B", "A", "S", "X"};
// D_002DCCD8: the event rows' (0x23 on) stage names.
const char* const kActionFiles[20] = {"A01", "A02", "A04", "A10", "A19", "A20", "A00", "A03", "A05", "A06",
                                      "A07", "A08", "A09", "A11", "A12", "A13", "A14", "A15", "A17", "A18"};
const int kTownCount = 22;        // D_002BE438's rows
const int kTownRows = 0x0B;       // the town list's rows shown
const int kPlaceRows = 0x0F;      // the place list's
const int kRaceRows = 14;         // the race list: rows 0-27 in two columns of 14
const int kActionRows = 10;       // the event list: rows 0x23-0x36 in two of 10
const int kFirstGrandPrix = 0x1C, kGrandPrixStages = 7;
const int kMyCity = 9;
// info+0x50, the frame's clear colour, which the menu and its lists set each frame they run.
const uint32_t kBackground = 0x8033CC66;
const int32_t kClock = 0x13C68;   // info+0x14 as the menu opens: 9:00

const char* CourseFile(int course) { return course >= 0 && course < 18 ? kCourseFiles[course] : ""; }

// func_00203D90: the highlight bar `w` wide for text at (x, y) and the horn cursor 40 to its left.
void Window_DrawBarCursor(gfx::Draw2D& d, int x, int y, int w) {
    Window_DrawBar(d, x, y, w);
    Window_DrawCursor(d, x - 40, y);
}

// Menu_MoveCursor (0x00206D28) as the race lists call it, with `cols` columns of `rows` and wrapping presses: the
// items number down each column (the item / rows is its column); Left and Right go across, Up and Down within one.
int Menu_MoveColumns(const platform::Input& input, int sel, int cols, int rows) {
    int t = Menu_MoveCursorGrid(input, 0, sel % rows * cols + sel / rows, cols, rows, false);
    return t % cols * rows + t / cols;
}

}  // namespace

void Save_InitComplete() {
    Save_SetCurrent(0);
    SaveRecord& r = Save_Record(0);
    Save_SetEventFlag(1, true);
    Save_SetEventFlag(4, true);
    Save_SetEventFlag(0x5A, true);
    // Every photo taken (+0x508) and every stamp's text shown (+0x528): 100 bits each; every opponent beaten.
    r.photoTaken[0] = r.stampShown[0] = ~0ull;
    r.photoTaken[1] = r.stampShown[1] = 0xFFFFFFFFFull;
    for (uint64_t& w : r.beaten) w = ~0ull;
    r.money = 10000;
    r.bank = 100000;
    r.license = 3;
    memcpy(r.name, GT_00333600_4, 5);       // D_00333600, its 5 bytes over "UnnamedRacer"
    memcpy(r.currency, "euro", 5);   // D_00333608
    // The bodies: all of 0-127 and 128-150 but 149 (0x5FFFFF); every town visited.
    for (int j = 0; j < 4; ++j) r.items[0][j] = ~0u;
    r.items[0][4] = 0x5FFFFF;
    r.townsUnvisited = 0;
    // The parts (kinds 1-14, a word a copy): three copies of the tires, engines, chassis, transmissions and steering,
    // one of the rest but the special parts: three of 1 and 2, one of 0.
    for (int j = 0; j < 3; ++j) {
        r.items[1][j] = 0x1FFF;
        r.items[2][j] = 0xFFF;
        r.items[3][j] = 0x1F;
        r.items[4][j] = 0x3F;
        r.items[5][j] = 0xF;
    }
    r.items[6][0] = 0xF;
    r.items[7][0] = 0x7FFF;
    r.items[8][0] = 7;
    r.items[9][0] = 3;
    r.items[10][0] = 7;
    r.items[10][1] = 6;
    r.items[10][2] = 6;
    r.items[11][0] = 0x1FF;
    r.items[12][0] = 3;
    r.items[13][0] = 0x7FFF;
    r.items[14][0] = 0x3BF;
    // Part_Fit (func_0023C418): the player's car's chassis 4, transmission 4, steering 2, brakes 3, special part 2;
    // each teammate's car tires 8, engine 9, chassis 3, transmission 4, steering 2, special part 1; then the player's
    // wing set 1 and option 2.
    SaveCar& you = r.cars[0];
    Part_Fit(you, 3, 4);
    Part_Fit(you, 4, 4);
    Part_Fit(you, 5, 2);
    Part_Fit(you, 6, 3);
    Part_Fit(you, 10, 2);
    for (int m = 1; m <= 2; ++m) {
        SaveCar& c = r.cars[m];
        Part_Fit(c, 1, 8);
        Part_Fit(c, 2, 9);
        Part_Fit(c, 3, 3);
        Part_Fit(c, 4, 4);
        Part_Fit(c, 5, 2);
        Part_Fit(c, 10, 1);
    }
    Part_Fit(you, 9, 1);
    Part_Fit(you, 11, 2);
    // The teammates (6, 8) and (7, 8) of D_002C22C8, in bodies 0x37 and 0x6E with those bodies' own paint
    // (D_002ED720 + 4 + 0x44 x body, which Save_InitBoth put in the paint table).
    r.cars[2].body = 0x6E;
    r.teammates[0][0] = 6;
    r.teammates[0][1] = 8;
    r.teammates[1][0] = 7;
    r.teammates[1][1] = 8;
    r.cars[1].paint = r.bodyPaint[0x37];
    r.cars[1].body = 0x37;
    r.cars[2].paint = r.bodyPaint[0x6E];
    // Item_Give (func_0023D2C0): one more copy of part 0 of kinds 1-7; Mail_Send every mail, 0-127.
    for (int k = 1; k < 8; ++k) Item_Give(k, 0);
    for (int m = 0; m < 0x80; ++m) Mail_Send(m);
}

TestMenu::TestMenu(gfx::Draw2D& draw, const std::string& dataDir) : d_(draw), dataDir_(dataDir), entry_(draw) {
    books_.SetDataDir(dataDir);
    // D_002BE438: the towns' names, the first word of each 8-byte row.
    for (int t = 0; t < kTownCount; ++t) townNames_.push_back(Save_TownName(t));
    for (const std::string& s : townNames_) townList_.push_back(s.c_str());
}

// Debug_TestMenuTask's first frame: the clock (info+0x14) 0x13C68 (the New Game flow keeps it), the loader's jobs
// (the meter's 0xA0000 + part, 0x300001; on the PC the files load as they're needed), Save_InitBoth and
// Save_InitComplete, D_0177EA50 record 0's three cars (0x70003 loads their bodies), and the window of the twelve
// lines (WindowAlloc's: centred on (320, 112)).
void TestMenu::Start() {
    Save_InitBoth();
    Save_InitComplete();
    Save_SetCurrent(0);
    Save_SetTestRecord(true);
    main_ = Window();
    for (const char* item : kItems) main_.AddLine(item, kLineSelectable);
    state_ = kMain;
    frame_ = 0;
}

TestMenu::Action TestMenu::Update(const platform::Input& input) {
    Action action = kRunning;
    d_.BeginFrame(kBackground);
    switch (state_) {
    case kMain: Debug_TestMenuTask(input, &action); break;
    case kTowns: Debug_TownListTask(input); break;
    case kPlaces: Debug_PlaceListTask(input, &action); break;
    case kRaceList: Debug_RaceListTask(input, &action); break;
    case kGrandPrix: Debug_GrandPrixListTask(input, &action); break;
    case kActionList: Debug_ActionListTask(input, &action); break;
    case kEndings: Debug_EndingMenuTask(input, &action); break;
    case kStamps:
    case kPictures:
    case kResults:
        // Stamp_BookTask, Pictures_Task, Results_BookTask: until Triangle closes the book.
        if (!books_.Book_Update(d_, input)) state_ = kMain;
        break;
    case kNameEntry:
        // NameEntry_Task with argument 0: "Enter your name." (+0x62C, which it writes into record 0 when applied).
        if (entry_.Update(input)) state_ = kMain;
        break;
    case kAway: break;
    }
    ++frame_;
    d_.EndFrame(0x20);
    if (action != kRunning) state_ = kAway;
    return action;
}

void TestMenu::Resume(bool up) {
    state_ = up && away_ == kPlaces ? kTowns : away_;
    frame_ = 1;
}

void TestMenu::SetPlace(int town, int place) {
    if (town != town_) {
        town_ = town;
        OpenPlaces();
    }
    place_ = place;
}

// The window; Cross (SE 0x1A) starts the item's child task, and the window stays still (not drawn) until it ends.
// Triangle does nothing.
void TestMenu::Debug_TestMenuTask(const platform::Input& input, Action* action) {
    if (main_.Update(d_, input) != 1) return;
    audio::Sound_PlaySe(0x1A);
    frame_ = 0;
    switch (main_.sel) {
    case kGoTown:
    case kToShop:
        // Debug_TownListTask with 0 or 1: a scroll list of the 22 towns of D_002BE438, 11 rows shown.
        shop_ = main_.sel == kToShop;
        towns_ = Window();
        towns_.SetList(townList_.data(), kTownCount, kTownRows);
        state_ = kTowns;
        break;
    case kRaceCourse:
    case kRaceSolo:
        solo_ = main_.sel == kRaceSolo;
        cursor_ = 0;
        state_ = kRaceList;
        break;
    case kWorldGrandPrix:
        // Debug_GrandPrixListTask: a window of the World Grand Prix's seven stages (rows 0x1C-0x22), their names.
        grandPrix_ = Window();
        for (int i = 0; i < kGrandPrixStages; ++i)
            grandPrix_.AddLine(Race_CourseName(AdventureRace_Get(kFirstGrandPrix + i).course), kLineSelectable);
        state_ = kGrandPrix;
        break;
    case kActionStage:
        cursor_ = 0;
        state_ = kActionList;
        break;
    case kStampNote:
        // (The loader's NOTE.GSL, 0x40005, and the stamps' 0x40017 first.)
        books_.Book_Open(FieldMenu::kBookStamps);
        state_ = kStamps;
        break;
    case kPhotoBook:
        books_.Book_Open(FieldMenu::kBookPictures);   // (after 0x40005 and 0x40018)
        state_ = kPictures;
        break;
    case kResultNote:
        books_.Book_Open(FieldMenu::kBookResults);   // (after 0x40005)
        state_ = kResults;
        break;
    case kNameEntryItem:
        entry_.Start(NameEntry::kName);
        state_ = kNameEntry;
        break;
    case kShowEnding:
        endings_ = Window();
        for (const char* item : kEndingItems) endings_.AddLine(item, kLineSelectable);
        state_ = kEndings;
        break;
    case kGoBackTitle:
        // The task ends with -1; the root (state 2) goes back to the title, TitleSeq_Task with 3.
        *action = kTitle;
        break;
    }
}

// The town list: Triangle ends it, back to the menu; a town opens its place list, but My Garage (town 0) goes straight
// on (no sound either way).
void TestMenu::Debug_TownListTask(const platform::Input& input) {
    int r = towns_.Update(d_, input);
    if (r == -1) {
        state_ = kMain;
    } else if (r == 1) {
        town_ = towns_.sel;
        OpenPlaces();
        state_ = kPlaces;
        frame_ = 0;
    }
}

// Debug_PlaceListTask's first frame for a town: a scroll list (15 rows shown) of the speakers' names of its message
// tables, D_002A7ED0[town][p][0]. Go Town lists its buildings (D_002BE438's +6, Field_Town().places); To Shop
// every table, the town's cars' talks on the field after its buildings.
void TestMenu::OpenPlaces() {
    placeNames_.clear();
    placeList_.clear();
    if (!Messages_Table(1, 0)) Messages_Load(dataDir_);
    const int doors = Field_Town(town_).places;
    for (int p = 0; shop_ || p < doors; ++p) {
        const MessageTable* table = Messages_Table(town_, p);
        if (!table) break;
        placeNames_.push_back(table->empty() ? std::string() : (*table)[0]);
    }
    for (const std::string& s : placeNames_) placeList_.push_back(s.c_str());
    places_ = Window();
    if (!placeList_.empty()) places_.SetList(placeList_.data(), (int)placeList_.size(), kPlaceRows);
}

// Each frame each row's line: white, green (the current-setting colour) for a place found (Place_Hidden clear),
// grey (disabled) for a person away at this time (Town_Absent with info+0x14) or a place of My City not open
// (MyCity_PlaceOpen); all can be chosen. Triangle goes back to the towns. A place chosen (info+0x21) runs Go Town's
// Adventure from it, or To Shop's scene of it, whose end comes back here.
void TestMenu::Debug_PlaceListTask(const platform::Input& input, Action* action) {
    if (town_ == 0) {
        // My Garage has no list: info+0x21 = 0 and Room_Start at once (func_00211808 / func_00211708).
        place_ = 0;
        away_ = kTowns;
        *action = shop_ ? kShop : kAdventure;
        return;
    }
    for (int k = 0; k < places_.items; ++k) {
        int p = places_.Top() + k;
        int flags = Place_Hidden(town_, p) ? kLineSelectable : kLineSelectable | kLineCurrent;
        if (FieldTownCars::Town_Absent(town_, p, kClock)) flags = kLineSelectable | kLineDisabled;
        if (town_ == kMyCity && !MyCity_PlaceOpen(p)) flags = kLineSelectable | kLineDisabled;
        places_.SetLineFlags(k, flags);
    }
    int r = places_.Update(d_, input);
    if (r == -1 || placeList_.empty()) {
        state_ = kTowns;
        return;
    }
    if (r != 1) return;
    place_ = places_.sel;
    away_ = kPlaces;
    if (!shop_) {
        *action = kAdventure;
    } else if (place_ < Field_Town(town_).places && RoomScene::Room_Open(town_, place_)) {
        *action = kShop;
    } else {
        // A field talk (Team_Find's car in slot 3 and Room_SetupTask with 1, the scene without its room) isn't in
        // the PC; nor is a place whose talk the PC doesn't open.
        audio::Sound_PlaySe(0x1C);
    }
}

// Two cell frames, columns 0-15 and 15-30 by rows 0-15, of the Adventure's town races (rows 0-27) by column: the
// course file (D_002DCC78), the race's name (D_002BE398) and the rank (D_002DCCC0, palette 5). The cursor moves
// over both columns (Left and Right across). Cross (SE 0x1A) runs the race: Race_Setup with the row, Race Solo with
// race flag 0x800 first; Triangle (SE 0x1B) goes back. Neither is read on the list's first frame.
void TestMenu::Debug_RaceListTask(const platform::Input& input, Action* action) {
    Window_DrawCellFrame(d_, 0, 0, 0xF, 0xF, 0);
    Window_DrawCellFrame(d_, 0xF, 0, 0x1E, 0xF, 0);
    cursor_ = Menu_MoveColumns(input, cursor_, 2, kRaceRows);
    if (cursor_ < kRaceRows) Window_DrawBarCursor(d_, 0x32, cursor_ * 24 + 0x22, 0x104);
    else Window_DrawBarCursor(d_, 0x15E, (cursor_ - kRaceRows) * 24 + 0x22, 0x104);
    for (int i = 0; i < kRaceRows; ++i) {
        const AdventureRace& a = AdventureRace_Get(i);
        const AdventureRace& b = AdventureRace_Get(i + kRaceRows);
        Text_Print(d_, 2, i + 1, CourseFile(a.course), 0);
        Text_Print(d_, 4, i + 1, Race_CourseName(a.course), 0);
        Text_Print(d_, 0xE, i + 1, kRanks[a.rank < 5 ? a.rank : 4], 5);
        Text_Print(d_, 0x11, i + 1, CourseFile(b.course), 0);
        Text_Print(d_, 0x13, i + 1, Race_CourseName(b.course), 0);
        Text_Print(d_, 0x1D, i + 1, kRanks[b.rank < 5 ? b.rank : 4], 5);
    }
    if (frame_ == 0) return;
    if (input.Pressed(0) & platform::kCross) {
        audio::Sound_PlaySe(0x1A);
        race_ = cursor_;
        away_ = kRaceList;
        *action = kRace;
    } else if (input.Pressed(0) & platform::kTriangle) {
        audio::Sound_PlaySe(0x1B);
        state_ = kMain;
    }
}

// The World Grand Prix's stages in a window: Cross (SE 0x1A) runs Race_Setup with row 0x1C + the stage, Triangle
// (SE 0x1B) goes back.
void TestMenu::Debug_GrandPrixListTask(const platform::Input& input, Action* action) {
    int r = grandPrix_.Update(d_, input);
    if (r == 1) {
        audio::Sound_PlaySe(0x1A);
        race_ = kFirstGrandPrix + grandPrix_.sel;
        solo_ = false;
        away_ = kGrandPrix;
        *action = kRace;
    } else if (r == -1) {
        audio::Sound_PlaySe(0x1B);
        state_ = kMain;
    }
}

// As the race list, for the events (rows 0x23-0x36) in two columns of 10 (cell frames to row 11): the stage
// (D_002DCCD8) and its name; the bar from x 0x1E or 0x14A, 0x118 wide.
void TestMenu::Debug_ActionListTask(const platform::Input& input, Action* action) {
    Window_DrawCellFrame(d_, 0, 0, 0xF, 0xB, 0);
    Window_DrawCellFrame(d_, 0xF, 0, 0x1E, 0xB, 0);
    cursor_ = Menu_MoveColumns(input, cursor_, 2, kActionRows);
    if (cursor_ < kActionRows) Window_DrawBarCursor(d_, 0x1E, cursor_ * 24 + 0x22, 0x118);
    else Window_DrawBarCursor(d_, 0x14A, (cursor_ - kActionRows) * 24 + 0x22, 0x118);
    for (int i = 0; i < kActionRows; ++i) {
        Text_Print(d_, 2, i + 1, kActionFiles[i], 0);
        Text_Print(d_, 4, i + 1, Race_CourseName(AdventureRace_Get(kFirstEvent + i).course), 0);
        Text_Print(d_, 0x11, i + 1, kActionFiles[i + kActionRows], 0);
        Text_Print(d_, 0x13, i + 1, Race_CourseName(AdventureRace_Get(kFirstEvent + kActionRows + i).course), 0);
    }
    if (frame_ == 0) return;
    if (input.Pressed(0) & platform::kCross) {
        audio::Sound_PlaySe(0x1A);
        race_ = kFirstEvent + cursor_;
        solo_ = false;
        away_ = kActionList;
        *action = kRace;
    } else if (input.Pressed(0) & platform::kTriangle) {
        audio::Sound_PlaySe(0x1B);
        state_ = kMain;
    }
}

// Ending 1-10 run that field scene (Ending_FieldScene with 0-9) until Start; Ending Full is Ending_Task, Theater the
// cinema's replay (Ending_ReplayTask). Start ends any of them (Sound_MuteCars, Sound_MuteSong) and the window comes
// back, as when they end. Triangle ends the menu, without a sound.
void TestMenu::Debug_EndingMenuTask(const platform::Input& input, Action* action) {
    int r = endings_.Update(d_, input);
    if (r == -1) {
        state_ = kMain;
    } else if (r == 1) {
        audio::Sound_PlaySe(0x1A);
        away_ = kEndings;
        scene_ = endings_.sel;
        *action = scene_ < 10 ? kEndingScene : scene_ == 10 ? kEndingFull : kTheater;
    }
}

}  // namespace game
