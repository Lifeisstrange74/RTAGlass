// The dialogue box the Adventure's people talk in: a high-level reimplementation of Message_Task (0x0023B168) with
// Message_RunPreCodes (0x0023AD00), Message_DrawText (0x0023A8E8), Message_DrawBox (0x002028B0), Message_DrawNameTab
// (0x00202B70), the choice windows (Message_ChoiceTask 0x0023BCD0, Message_YesNoTask 0x00239260), the car pick
// (Message_PickCarTask 0x00239900), the race list (Message_RaceListTask 0x00239A80, with Notice_Task 0x0023BB00) and
// the save point (Save_AdventureSaveTask, save_screens.h). It runs one
// message table (messages.h): message 1 first, each message's control codes, its text typed out three lines to a
// page, and the end code that says what comes next. See docs/messages.md.
#pragma once

#include <string>

#include "../../gfx/draw2d.h"
#include "../../platform/input.h"
#include "../flow/name_entry.h"
#include "../save/save_screens.h"
#include "../ui/window.h"
#include "messages.h"

namespace game {

class MessageTask {
public:
    explicit MessageTask(gfx::Draw2D& draw);

    // Starts `table` for (town, place) (what some codes test): message 1, the box opening (SE 0x25).
    void Start(const MessageTable& table, int town, int place);
    // One frame, drawn. Returns 0 while it runs; once the box has closed, the result for the scene: 5 a plain end,
    // 4 a race (Race()), 6 quit the game, 11 go to Destination(), 15 the ending. A car picked for the parts (end
    // code 4) gives 9 | car << 8 at once, on the frame it's picked, and the box then closes without another result.
    // A shop (code 0x13) gives 0x13 once: the scene runs the shop's screen, the box hidden, until Message_ShopDone.
    int Update(const platform::Input& input);
    // The shop's screen is left: on to the code's next message.
    void Message_ShopDone();
    bool Running() const { return state_ != kDone; }
    // The end codes `table`'s messages use, a bit each (what a scene needs of the port).
    static uint32_t Message_EndCodes(const MessageTable& table);
    // The time of day (info+0x14, frames of the day) the codes test (0x18 night, the race list's night race).
    // The opening scene, which sets none, keeps 54000.
    void SetClock(int clock) { clock_ = clock; }
    static const int kDefaultClock = 54000;

    // info+0x18: each typing blip sets it to 2; the scene counts it down (the speaker's lights flash with it).
    int blip;
    // What the scene's info gets from the codes: the race (codes 3 and 8, info+0x20; an AdventureRace) and the place
    // to go (code 0x14).
    int Race() const { return race_; }
    void Destination(int* town, int* place) const {
        *town = goTown_;
        *place = goPlace_;
    }
    // Pre-code 0x14 (t, p) (Shop_SceneTask's result 12): the scene ends as the talk does and the car comes out on the
    // field at place p of town t (Destination()), not at this building's door.
    bool Leaving() const { return leave_; }
    // Set once a code has changed the player's car (0x15 fits a part) or the team (0x10): the scene redraws them.
    // Reading it clears it.
    bool TakeCarChanged() {
        bool c = carChanged_;
        carChanged_ = false;
        return c;
    }
    bool TakeTeamChanged() {
        bool c = teamChanged_;
        teamChanged_ = false;
        return c;
    }

private:
    enum State { kOpen, kRun, kScroll, kClose, kChild, kDone };
    enum TextStop { kTyping, kPageFull, kEndCode };
    // The window an end code runs (state kChild).
    enum Child {
        kChildChoice, kChildPickCar, kChildRaceList, kChildSave,
        kChildNumber,        // 0x05: func_0023A320
        kChildTeamMenu,      // 0x10: func_00239748
        kChildTeamLeave,     //       func_00239508
        kChildTeamJoined,    //       func_00239378
        kChildGetItem,       // 0x07: func_00239F68
        kChildHorn,          // 0x0B: func_0023BE88
        kChildPhoto,         // 0x11: Photo_ViewTask (the box hidden meanwhile)
        kChildGreeting,      // 0x12: func_0023BE18
        kChildMileage,       // 0x16: func_0023A140
        kChildBank,          // 0x17: func_0023A460
        kChildShop,          // 0x13: func_00269E88 (the scene's; the box hidden meanwhile)
    };

    void Message_Go(int n);                    // message n of the table, its control codes run
    void Message_RunPreCodes();                // 0x0023AD00
    // 0x0023A8E8: the visible lines up to character chars_; sets end_ to the end code's offset on kEndCode.
    TextStop Message_DrawText(uint32_t rgba, bool draw);
    void Message_DrawBox(uint32_t rgba);                  // 0x002028B0
    void Message_DrawNameTab(int width, uint32_t rgba);   // 0x00202B70, and the name
    void Message_OpenChoice(bool yesNo, bool onNo);       // end codes 9, 1 and 2
    void Message_OpenWindow(Child child);
    void Message_PickCarTask(int type);                   // end codes 4 and 6: 0x00239900 (its window)
    void Message_TeamTask();                              // end code 0x10: 0x00239748's first frame
    void Message_TeamJoin(int slot);                      // 0x00239378's first frame
    // The windows of the codes that have their own frames (number, horn, photo, greeting, mileage, bank), or false
    // for the window children.
    bool Message_UpdateTask(const platform::Input& input);
    void Message_RaceListTask();                          // end code 8: 0x00239A80 (its window)
    int Message_UpdateChild(const platform::Input& input);
    int Message_RaceListPick();                           // 0x00239A80's Cross
    void Message_EndCode(const platform::Input& input);
    std::string Expand(char letter, char arg) const;             // the $ escapes

    gfx::Draw2D& d_;
    const MessageTable* table_;
    int town_, place_;
    State state_;
    int age_;
    int chars_;          // +0x02: characters shown (0x7FFF: the rest of the page at once)
    int scroll_;         // +0x26: 12 a line
    int result_;         // +0x27
    std::string msg_;    // +0x28: the current message
    size_t text_;        // where its text starts, after the control codes
    size_t end_;         // its end code
    Window window_;      // the choice or Yes/No window (state kChild), then while it closes
    bool windowOpen_;
    bool windowClosing_;
    unsigned char targets_[16];   // the message each item goes to
    int items_;
    Child child_;
    int firstRace_;      // the race list's +0x02: the town's first race
    Window notice_;      // func_0023BB00's one-line notice over the race list
    bool noticeOpen_;
    AdventureSave save_;
    bool saveClosing_;   // the save's window fading out while the messages go on
    int race_, goTown_, goPlace_;
    bool leave_;   // pre-code 0x14
    int clock_;
    int pickType_;       // the car pick's result type: 9 parts (code 4), 8 paint (code 6)
    int childAge_;       // the frames of a child task
    int value_;          // the number (0x05), the amount (0x17), the photo (0x11)
public:
    // The photo Photo_ViewTask's take mode shows this frame (its caller draws the car over it), or -1.
    int PhotoShown() const { return state_ == kChild && child_ == kChildPhoto && childAge_ > 1 ? value_ : -1; }
private:
    int digit_;          // the bank's digit under the cursor (its power of ten, 3-5)
    int pay_, miles_;    // code 0x16's
    int teamSlot_;       // the teammate the menu picked (1, 2)
    std::string lines_[3];   // the windows' own text
    NameEntry greeting_;
    bool carChanged_, teamChanged_;
    bool shopPending_ = false;   // 0x13 for the scene on this frame
};

}  // namespace game
