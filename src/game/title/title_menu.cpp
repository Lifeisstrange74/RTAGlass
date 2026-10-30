#include "title_menu.h"

#include "../../audio/sound.h"
#include "../extras.h"
#include "../save/save_data.h"
#include "../save/save_file.h"
#include "../save/save_screens.h"
#include "../field/field_menu_draw.h"
#include "../text.h"
#include "../../game_text.inc"   // the game's text, GT_<address>_<length>, made from the ELF

namespace game {

namespace {

// Menu text: the original tables' rows for English, French and German (Title_Language), accents as the font's
// escapes ("/e" e acute, "~a" a diaeresis). Their fourth rows are empty.
const char* const kMainItems[3][5] = {   // D_0029AD20
    {GT_002EA350_9, GT_002EA340_10, GT_002EA330_8, GT_00333350_7, GT_00333348_7},
    {GT_002EA320_8, GT_002EA310_13, GT_002EA300_9, GT_00333350_7, GT_002EA2F0_10},
    {GT_002EA2E0_9, GT_002EA2C8_16, GT_002EA2B8_9, GT_002EA2A8_8, GT_002EA298_10},
};
const char* const kAdventureItems[3][2] = {   // D_0029ACE0
    {GT_002EA1D8_8, GT_002EA1C8_8}, {GT_002EA1B8_15, GT_002EA1A8_9}, {GT_002EA198_11, GT_002EA188_14}};
// D_0029AC90: Vibration, Speaker, Sound Volume, Language, Exit. (French has "Volume" for Speaker.) The US game's
// window leaves out Language (+0xC); the PC adds Controls and Extras before Exit, and Language with its extra.
const char* const kOptionNames[3][5] = {
    {GT_002EA128_9, GT_00333340_7, GT_002EA178_12, GT_002EA168_8, GT_00333338_4},
    {GT_002EA128_9, GT_00333330_6, GT_002EA158_10, GT_00333328_6, GT_00333320_7},
    {GT_002EA128_9, GT_002EA148_12, GT_002EA138_11, GT_00333318_7, GT_00333310_7},
};
const int kLanguageItem = 3;   // where the table has it; Controls and Extras come after it then
const char* const kVersusItems[3][3] = {   // D_0029ACF8
    {GT_002EA288_15, GT_002EA278_11, GT_002EA268_11},
    {GT_002EA250_17, GT_002EA238_17, GT_002EA220_18},
    {GT_002EA208_17, GT_002EA1F8_14, GT_002EA1E8_13},
};
// The Language window's items: the PC's names (the game has none), each in its own language.
const char* const kLanguageNames[3] = {"English", "Fran~cais", "Deutsch"};

int g_language = 0;   // D_00333284, as the Language window last set it

enum MainItem { kAdventureItem, kQuickRaceItem, kTwoPlayerItem, kOptionsItem, kResultsItem };

}  // namespace

int Title_Language() { return Extras_On(kExtraLanguage) ? g_language : 0; }

MainMenu::MainMenu(gfx::Draw2D& draw, const std::string& dataDir)
    : d_(draw), adventureOpen_(false), loading_(false), load_(draw), records_(draw), versusOpen_(false), versusChild_(kNoVersusChild),
      versusMode_(0), child_(kNone), optionsChild_(kNoOption), frame_(0) {
    controls_.SetDataDir(dataDir);
    Reset();
}

// The menu window: one line per item, at the lower right (Place 0xA). The PS2 shows 2 Player only while a
// second controller is connected; the PC version always shows it.
void MainMenu::Reset() {
    main_ = Window();
    for (const char* item : kMainItems[Title_Language()]) main_.AddLine(item, kLineSelectable);
    main_.Place(0xA);
    main_.pad = platform::kEitherPad;
    Save_SetTestRecord(false);   // the test menu's record is gone from here on (New Game and the loads replace it)
    adventureOpen_ = false;
    loading_ = false;
    versusOpen_ = false;
    versusChild_ = kNoVersusChild;
    child_ = kNone;
    optionsChild_ = kNoOption;
}

MainMenu::Result MainMenu::Update(const platform::Input& input) {
    d_.BeginFrame(0x80000000);
    d_.Pillarbox();   // the title's picture is 4:3 (a wide screen's sides black)
    d_.Sprite(d_.Get("SYS/TITLE.GSL", 0), 0, 0, 640, 448, 0, 0, 640, 448);
    d_.Sprite(d_.Get("SYS/TITLE.GSL", 1), 16, 24, 440, 220, 0, 0, 440, 220);
    // The Adventure and Results fade (Fade_SetOverlay): black, up to half over 32 frames, under the windows.
    if (adventureOpen_ || versusOpen_ || child_ == kResults)
        d_.Fill(0, 0, 640, 448, (uint32_t)(frame_ < 32 ? frame_ * 2 : 0x40) << 24);
    Result result = kStay;
    if (adventureOpen_) {
        Help_Draw(d_, 1, 0x80808080, Title_Language());
        result = Title_AdventureMenuTask(input);
    } else if (versusOpen_) {
        Help_Draw(d_, 1, 0x80808080, Title_Language());
        result = Title_VersusMenuTask(input);
    } else {
        // Each menu draws its own hints; the deepest one ends up on top.
        Help_Draw(d_, child_ != kOptions ? 0 : optionsChild_ != kNoOption ? 3 : 2, 0x80808080, Title_Language());
        result = Title_MainMenuTask(input);
    }
    d_.EndFrame(0x20);
    return result;
}

// New Game or Continue, at the lower middle. In the original the root task (func_00205088) runs this in place of
// the title when the main menu returns Adventure, and acts on its result: New Game starts NewGame_Setup, Continue
// loads a save through the memory card screens (Mc_LoadSlotWindowTask 0x00228D10), Triangle brings the main menu
// back. When the load ends the menu ends with it: a loaded save goes on to NewGame_Continue (0x0026D370), anything
// else back to the main menu. Item 2 ends it with result 2 (race flags 0x400000, as New Game), on which the root starts
// the developers' test menu, Debug_TestMenuTask (0x0026B848); the retail window has no third line for it (D_0029ACE0
// has two strings a language). With the Test Menu extra the PC adds one: "Test Menu" (its text is a GUESS).
MainMenu::Result MainMenu::Title_AdventureMenuTask(const platform::Input& input) {
    ++frame_;
    int r = adventure_.Update(d_, input);
    if (loading_) {
        int loaded = load_.Update(input);
        if (loaded == 1) return kContinue;
        if (loaded == 0) Reset();
        return kStay;
    }
    if (r == 1) {
        audio::Sound_PlaySe(0x1A);
        if (adventure_.sel == 0) return kNewGame;
        if (adventure_.sel == 2) return kTestMenu;
        adventure_.state |= Window::kInactive;
        load_.Start();
        loading_ = true;
    } else if (r == -1) {
        audio::Sound_PlaySe(0x1B);
        Reset();
    }
    return kStay;
}

// Race Right-Away, Random Race or Custom Race, at the lower middle. Race Right-Away (mode 2) gives both players the
// preset records. Random Race (mode 3) and Custom Race (mode 6) ask for both memory cards and load player 1's from
// slot 1 and player 2's from slot 2; any failure says why and comes back here. Either way the original then ends with
// result 3, and the root starts Versus_Task (0x00211320) and its course select. Triangle goes back to the main menu.
MainMenu::Result MainMenu::Title_VersusMenuTask(const platform::Input& input) {
    ++frame_;
    int r = versus_.Update(d_, input);
    if (versusChild_ == kNoVersusChild) {
        if (r == 1) {
            audio::Sound_PlaySe(0x1A);
            versus_.state |= Window::kInactive;
            if (versus_.sel == 0) {
                versusMode_ = 2;
                Save_InitBoth();
                Save_InitVersusPreset();
                return kVersus;
            } else {
                versusMode_ = versus_.sel == 1 ? 3 : 6;
                OpenVersusMessage(kMcCards);
            }
        } else if (r == -1) {
            audio::Sound_PlaySe(0x1B);
            Reset();
        }
        return kStay;
    }
    if (versusChild_ == kVersusLoaded) {   // Versus_LoadedTask: both summaries, then the window; Cross only
        Save_DrawSummary(d_, Save_Record(0), 2, 1);
        Save_DrawSummary(d_, Save_Record(1), 0xA, 2);
    }
    r = message_.Update(d_, input);
    switch (versusChild_) {
    case kVersusCards:   // Mc_MessageTask: either item (Triangle is No) ends it at once, with no closing fade
        if (r == 1 || r == -1) {
            audio::Sound_PlaySe(0x1A);
            if (r == -1 || message_.sel != 0) {
                versusChild_ = kNoVersusChild;
                versus_.state &= ~Window::kInactive;
                break;
            }
            int failed = Mc_LoadPlayers();
            if (failed >= 0) {
                OpenVersusMessage(failed);
            } else {
                audio::Sound_PlaySe(0x303);
                message_ = Window();
                message_.AddLine(GT_002EB518_21, 0);   // D_002EB518
                message_.pad = platform::kEitherPad;
                versusChild_ = kVersusLoaded;
            }
        }
        break;
    case kVersusMessage:
        if (r == 1 || r == -1) {
            audio::Sound_PlaySe(0x1A);
            versusChild_ = kNoVersusChild;
            versus_.state &= ~Window::kInactive;
        }
        break;
    case kVersusLoaded:
        if (r == 1) return kVersus;
        break;
    case kNoVersusChild: break;
    }
    return kStay;
}

int MainMenu::Mc_LoadPlayers() {
    // A PC "memory card" is always there; a slot without its file has no data. Player 1 first: with no save1.json
    // the original also tries slot 2 (into player 2's record), to tell "no data in slot 1" from "in slot 1 and 2".
    if (!Save_SlotExists(1))
        return Save_SlotExists(2) && Save_ReadFile(Save_SlotPath(2), Save_Record(1)) ? kMcNoData1 : kMcNoData12;
    if (!Save_ReadFile(Save_SlotPath(1), Save_Record(0))) return kMcLoadFailed1;
    if (!Save_SlotExists(2)) return kMcNoData1 + 1;
    if (!Save_ReadFile(Save_SlotPath(2), Save_Record(1))) return kMcLoadFailed1 + 1;
    // (Versus_LoadedTask then clamps the license, +0x651, to Super A. The PS2 clamps player 2's and a byte past both
    // records instead of player 1's; the port clamps both players'.)
    for (int p = 0; p < 2; ++p)
        if (Save_Record(p).license > 3) Save_Record(p).license = 3;
    return -1;
}

// Mc_MessageTask's window; the one child the menu shows at a time.
void MainMenu::OpenVersusMessage(int message) {
    Mc_MessageWindow(message_, message);
    versusChild_ = message == kMcCards ? kVersusCards : kVersusMessage;
}

// Cross picks an item. Quick Race leaves the title (the original hands the choice to TitleSeq_Task, which ends and
// lets the root task start func_00211170); everything else opens a child and makes the menu inactive until it's
// done. The window reads either pad, but Adventure, Quick Race and Results are player 1's: from pad 2 they buzz
// (SE 0x1C) and the menu stays.
MainMenu::Result MainMenu::Title_MainMenuTask(const platform::Input& input) {
    int r = main_.Update(d_, input);
    bool padOne = (input.Pressed(0) & (platform::kCross | platform::kStart)) != 0;
    int sel = main_.sel;
    if (r == 1 && !padOne && (sel == kAdventureItem || sel == kQuickRaceItem || sel == kResultsItem)) {
        audio::Sound_PlaySe(0x1C);
        r = 0;
    }
    if (r == 1) {
        audio::Sound_PlaySe(0x1A);
        main_.state |= Window::kInactive;
        switch (main_.sel) {
        case kAdventureItem:
            adventure_ = Window();
            adventure_.AddLine(kAdventureItems[Title_Language()][0], kLineSelectable);
            adventure_.AddLine(kAdventureItems[Title_Language()][1], kLineSelectable);
            if (Extras_On(kExtraTestMenu)) adventure_.AddLine("Test Menu", kLineSelectable);   // (GUESS: its text)
            adventure_.Place(8);
            adventure_.x = 0x190;
            adventureOpen_ = true;
            frame_ = 0;
            break;
        case kQuickRaceItem:
            return kQuickRace;
        case kTwoPlayerItem:
            // The root runs Title_VersusMenuTask in place of the title, as it does the Adventure menu. Its window
            // reads both pads (+0x0C = 2).
            versus_ = Window();
            versus_.pad = platform::kEitherPad;
            for (const char* item : kVersusItems[Title_Language()]) versus_.AddLine(item, kLineSelectable);
            versus_.Place(8);
            versus_.x = 0x17C;
            versusOpen_ = true;
            versusChild_ = kNoVersusChild;
            frame_ = 0;
            break;
        case kOptionsItem:
            Options_Build(0);
            options_.state = Window::kOpening;
            child_ = kOptions;
            break;
        case kResultsItem:
            // Title_ResultsTask: the memory card's slot window and load (Mc_LoadSlotWindowTask) into record 0.
            load_.Start();
            recordsOpen_ = false;
            child_ = kResults;
            frame_ = 0;
            break;
        }
    }
    if (child_ == kOptions) Options_MenuTask(input);
    else if (child_ == kResults) Title_ResultsTask(input);
    if (child_ == kNone) main_.state &= ~Window::kInactive;
    return kStay;
}

// Results: the memory card screens (Mc_LoadSlotWindowTask 0x00228D10) load a save into record 0, then
// VersusRecords_Task (func_00269F48) shows its 2 Player records; the picture stays dimmed under both. A load that
// fails or is left, or the records left, brings the main menu back.
void MainMenu::Title_ResultsTask(const platform::Input& input) {
    ++frame_;
    if (!recordsOpen_) {
        int loaded = load_.Update(input);
        if (loaded == 1) {
            records_.Start();
            recordsOpen_ = true;
        } else if (loaded == 0) {
            child_ = kNone;
        }
        return;
    }
    if (!records_.Update(input)) child_ = kNone;
}

// Options_MenuTask's lines (func_002092D8: items 0, 1, 2 and 4 of the language's row), with Language (item 3) where
// the table has it when the extra is on, and the PC's Controls, Display, Extras and (with extras.json's "cheats")
// Cheats before Exit. The window is 0xB4 wide.
void MainMenu::Options_Build(int sel) {
    const char* const* names = kOptionNames[Title_Language()];
    optionsLanguage_ = Extras_On(kExtraLanguage);
    options_ = Window();
    options_.pad = platform::kEitherPad;
    for (int i = 0; i < 3; ++i) options_.AddLine(names[i], kLineSelectable);
    if (optionsLanguage_) options_.AddLine(names[kLanguageItem], kLineSelectable);
    options_.AddLine("Controls", kLineSelectable);
    options_.AddLine("Display", kLineSelectable);
    options_.AddLine("Extras", kLineSelectable);
    if (Cheats_Enabled()) options_.AddLine("Cheats", kLineSelectable);
    options_.AddLine(names[4], kLineSelectable);
    options_.w = 0xB4;   // set after the lines, so it doesn't stay centred
    options_.sel = (signed char)sel;
}

// The language or the Language extra changed: the main menu (under Options, so inactive, on Options) and the
// Options window (open at once, the cursor on the item it had: Language or Extras, whose place moves with Language)
// are made again in the language. The PS2 never changes the language while its menus are up.
void MainMenu::Options_Relabel() {
    const int had = optionsLanguage_ ? 1 : 0, has = Extras_On(kExtraLanguage) ? 1 : 0;
    int sel = options_.sel;
    if (sel > kLanguageItem || (sel == kLanguageItem && !had)) sel += has - had;   // Controls ... Exit move
    Reset();
    main_.sel = kOptionsItem;
    main_.state |= Window::kInactive;
    child_ = kOptions;
    Options_Build(sel);
}

// Options: Vibration, Speaker, Sound Volume, (with the extra) Language, (the PC's) Controls, Display, Extras and
// Cheats, Exit.
// Triangle is Exit.
void MainMenu::Options_MenuTask(const platform::Input& input) {
    const int lang = optionsLanguage_ ? 1 : 0;
    const int kControlsItem = 3 + lang, kDisplayItem = 4 + lang, kExtrasItem = 5 + lang;
    const int kCheatsItem = Cheats_Enabled() ? 6 + lang : -1;
    int r = options_.Update(d_, input);
    if (r == -1) options_.sel = (signed char)(options_.items - 1);
    if (r == 1 || r == -1) {
        audio::Sound_PlaySe(options_.sel == options_.items - 1 ? 0x1B : 0x1A);
        if (options_.sel == options_.items - 1) {
            options_.state = Window::kClosing;
        } else if (lang && options_.sel == kLanguageItem) {
            // The Language window (the PC's: the US game has no task for the item): the item's name, then the
            // languages, the one in use green; Cross sets it, Triangle leaves.
            options_.state |= Window::kInactive;
            language_ = Window();
            language_.pad = platform::kEitherPad;
            language_.state = Window::kOpening;
            language_.AddLine(kOptionNames[Title_Language()][kLanguageItem], 0);
            for (const char* name : kLanguageNames) language_.AddLine(name, kLineSelectable);
            language_.sel = (signed char)Title_Language();
            language_.SetLineFlags(1 + Title_Language(), kLineSelectable | kLineCurrent);
            optionsChild_ = kLanguage;
        } else if (options_.sel == kControlsItem) {
            options_.state |= Window::kInactive;
            controls_.Start();
            optionsChild_ = kControls;
        } else if (options_.sel == kDisplayItem) {
            options_.state |= Window::kInactive;
            display_.Start();
            optionsChild_ = kDisplay;
        } else if (options_.sel == kExtrasItem) {
            options_.state |= Window::kInactive;
            extras_.Start();
            optionsChild_ = kExtras;
        } else if (options_.sel == kCheatsItem) {
            options_.state |= Window::kInactive;
            extras_.Start(true);
            optionsChild_ = kCheats;
        } else {
            options_.state |= Window::kInactive;
            sub_.Open((OptionWindows::Kind)options_.sel, Title_Language());
            optionsChild_ = kSetting;
        }
    } else if (r == -2) {
        child_ = kNone;
        return;
    }
    switch (optionsChild_) {
    case kNoOption: break;
    case kSetting:
        if (!sub_.Update(d_, input)) optionsChild_ = kNoOption;
        break;
    case kControls: Options_ControlsTask(input); break;
    case kDisplay: Options_DisplayTask(input); break;
    case kExtras:
        Options_ExtrasTask(input);
        // The Language item comes and goes with its extra, and the language with it.
        if (optionsChild_ == kNoOption && optionsLanguage_ != Extras_On(kExtraLanguage)) Options_Relabel();
        break;
    case kCheats: Options_ExtrasTask(input); break;
    case kLanguage: Options_LanguageTask(input); break;
    }
    if (optionsChild_ == kNoOption) options_.state &= ~Window::kInactive;
}

// The Language window: Cross on a language (SE 0x1A) sets it and closes the window, and the title's menus are made
// again in it; Triangle (SE 0x1B) closes it as it was.
void MainMenu::Options_LanguageTask(const platform::Input& input) {
    int r = language_.Update(d_, input);
    if (r == -2) {
        optionsChild_ = kNoOption;
    } else if (r == -1) {
        audio::Sound_PlaySe(0x1B);
        language_.state = Window::kClosing;
    } else if (r == 1) {
        audio::Sound_PlaySe(0x1A);
        language_.state = Window::kClosing;
        if (language_.sel != g_language) {
            g_language = language_.sel;
            Options_Relabel();
            options_.state |= Window::kInactive;   // until the Language window has gone
        }
    }
}

// Controls: the field menu's Settings screen over the whole frame, on its background (OPTION2's tiles), the
// panel's title in place of its title car.
void MainMenu::Options_ControlsTask(const platform::Input& input) {
    menu::Menu_DrawRoad(d_, 0, 0, 5);
    menu::Menu_DrawPanel(d_);
    Text_PrintAt(d_, 40, 24, "Controls", 0);
    if (controls_.Update(d_, input) == menu::kListLeft) optionsChild_ = kNoOption;
}

// Display: the PC's display settings, as Controls.
void MainMenu::Options_DisplayTask(const platform::Input& input) {
    menu::Menu_DrawRoad(d_, 0, 0, 5);
    menu::Menu_DrawPanel(d_);
    Text_PrintAt(d_, 40, 24, "Display", 0);
    if (display_.Update(d_, input) == menu::kListLeft) optionsChild_ = kNoOption;
}

// Extras: the PC's switches for the unused content and the debug features, as Controls; Cheats the same screen.
void MainMenu::Options_ExtrasTask(const platform::Input& input) {
    menu::Menu_DrawRoad(d_, 0, 0, 5);
    menu::Menu_DrawPanel(d_);
    Text_PrintAt(d_, 40, 24, optionsChild_ == kCheats ? "Cheats" : "Extras", 0);
    if (extras_.Update(d_, input) == menu::kListLeft) optionsChild_ = kNoOption;
}

}  // namespace game
