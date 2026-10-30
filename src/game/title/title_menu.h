// The main menu over the title picture, and its Options menus. A high-level reimplementation of
// Title_MainMenuTask (0x00209CB0) and the tasks it starts, on top of game::Window (see docs/title.md).
//
// Adventure opens New Game / Continue; New Game hands over to the New Game flow (game/flow). Quick Race hands over
// to the course select (game/quickrace). 2 Player opens Race Right-Away / Random Race / Custom Race and readies both
// players' records (the preset, or save1.json and save2.json), then hands over to the 2 Player flow (game/versus).
// Continue loads a save (save1.json or save2.json) and hands over to the Adventure (game/flow). Results loads a save
// the same way and shows its 2 Player records (title_results.h).
// Options works and sets the sound volume and speaker mode; on the PC it also has Controls (ui/controls_screen.h),
// Display (ui/display_screen.h)
// and Extras (ui/extras_screen.h), and with the Language extra the original's Language item (docs/extras.md).
#pragma once

#include "../../gfx/draw2d.h"
#include "../../platform/input.h"
#include "../save/save_screens.h"
#include "../ui/controls_screen.h"
#include "../ui/display_screen.h"
#include "../ui/extras_screen.h"
#include "../ui/option_windows.h"
#include "../ui/window.h"
#include "title_results.h"

namespace game {

// The title's language (D_00333284: 0 English, 1 French, 2 German), which picks the rows of the title's menu
// tables, PRESS START and the title's hints and Vibration window. The US game never changes it; the Language
// extra's window does (for this run only), and with the extra off it's English.
int Title_Language();

class MainMenu {
public:
    // dataDir holds models/ (the Controls screen's pad).
    MainMenu(gfx::Draw2D& draw, const std::string& dataDir);

    enum Result { kStay, kNewGame, kContinue, kQuickRace, kVersus, kTestMenu };

    // Opens the menu fresh, as Start on the title does.
    void Reset();
    // Runs and draws one frame. kNewGame, kContinue (a save loaded into player 1's record), kQuickRace or kVersus
    // when one was chosen; kTestMenu, the Adventure menu's third line (the Test Menu extra).
    Result Update(const platform::Input& input);
    // After kVersus: the race mode (pad +0x0A), 2 Race Right-Away, 3 Random Race, 6 Custom Race.
    int VersusMode() const { return versusMode_; }

private:
    // What has the input besides the main menu (child tasks in the original).
    enum Child { kNone, kOptions, kResults };
    enum OptionsChild { kNoOption, kSetting, kControls, kDisplay, kExtras, kCheats, kLanguage };

    Result Title_MainMenuTask(const platform::Input& input);    // 0x00209CB0
    Result Title_AdventureMenuTask(const platform::Input& input);   // 0x00209520
    Result Title_VersusMenuTask(const platform::Input& input);      // 0x00209848
    // Mc_LoadPlayerTask (0x00228F40) for player 1 then player 2: reads save1.json and save2.json into the records.
    // Returns -1 when both are loaded, else the Mc_MessageTask message to show.
    int Mc_LoadPlayers();
    void OpenVersusMessage(int message);   // Mc_MessageTask (0x00227380)'s window for `message`
    void Options_MenuTask(const platform::Input& input);        // 0x002092D8
    void Options_ControlsTask(const platform::Input& input);    // the PC's
    void Options_DisplayTask(const platform::Input& input);     // the PC's
    void Options_ExtrasTask(const platform::Input& input);      // the PC's (and its Cheats)
    void Options_LanguageTask(const platform::Input& input);    // the Language extra's (the PC's)
    // The Options window's lines in the language, with Language when the extra is on; the cursor on line `sel`.
    void Options_Build(int sel);
    // After the language (or the extra) changed: the main menu and the Options window again in it.
    void Options_Relabel();
    void Title_ResultsTask(const platform::Input& input);       // 0x0020A580

    gfx::Draw2D& d_;
    // The 2 Player menu's child (the original's child tasks): the memory card question (Mc_MessageTask 2), a
    // loading message, or "Data has been loaded." (Versus_LoadedTask 0x0022A050).
    enum VersusChild { kNoVersusChild, kVersusCards, kVersusMessage, kVersusLoaded };

    Window main_, adventure_, versus_, options_, message_, language_;
    bool optionsLanguage_ = false;   // the Options window has the Language item (the extra was on when it was made)
    OptionWindows sub_;   // Vibration, Speaker or Sound Volume
    ControlsScreen controls_;
    ExtrasScreen extras_;
    DisplayScreen display_;
    bool adventureOpen_;   // the root has replaced the main menu with the Adventure menu
    bool loading_;         // ... and Continue's load runs over it
    AdventureLoad load_;
    VersusRecords records_;
    bool recordsOpen_ = false;   // Title_ResultsTask's +0x25: the load done, VersusRecords_Task runs
    bool versusOpen_;      // the root has replaced the main menu with the 2 Player menu
    VersusChild versusChild_;
    int versusMode_;       // the race mode chosen (pad +0x0A): 2 Race Right-Away, 3 Random Race, 6 Custom Race
    Child child_;
    OptionsChild optionsChild_;
    int frame_;    // frames since the current message (or the Adventure menu) opened
};

}  // namespace game
