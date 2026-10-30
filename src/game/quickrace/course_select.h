// The course select, a carousel of course cards over scrolling clouds, for the Quick Race and 2 Player; and the
// Quick Race around it. A high-level reimplementation of QuickRace_Task (0x00211170), the carousel
// CourseSelect_Task (0x002455C0) and the tasks it starts: CourseInfo_Task (0x00248180) when a course is chosen,
// CourseSelect_QuitTask (0x00248A70) for Exit and, in 2 Player, CourseSelect_SaveExitTask (0x00248BF0) for
// Save&Exit, CourseSelect_VersusPartsTask (0x00248580) for Change Parts and CourseSelect_SellPartsTask (0x00248398)
// for Trade Items. See docs/course_select.md and two_player.md.
//
// Confirming a course starts its race (game/race). Exit, Yes fades out and goes back to the title.
#pragma once

#include <string>

#include "../../gfx/draw2d.h"
#include "../../gfx/model.h"
#include "../../platform/input.h"
#include "../race/car.h"
#include "../race/race.h"
#include "../race/race_light.h"
#include "../save/save_data.h"
#include "../save/save_screens.h"
#include "../story/parts_screen.h"
#include "../ui/window.h"

namespace game {

// A row of a course table: the Quick Race's D_002BE158 or 2 Player's D_002BE1D8.
struct CourseRow {
    int id, cars, laps;
    int flags;   // 1 the track length, 2 the laps, 4 the best total time (2 Player), 8 TEAM BATTLE
};
// Row `row` of race mode `mode`'s table (1 the Quick Race's, else 2 Player's). (PC) The Quick Race's row 8 is the Test
// Course's (Course::kTestCourse, the Test Course extra): 1 car, 3 laps.
const CourseRow& Course_Row(int mode, int row);
// (PC) The Test Course's card (its bit in the Quick Race's card mask, after the eight courses'). The save has records
// for the eight only (+0xB78), so this card's are CourseSelect's own, for the Quick Race's session as the others are.
const int kTestCourseCard = 8;

class CourseSelect {
public:
    // dataDir holds models/SYS/S00 and S01, the course miniatures (loaded on the first Start that needs them).
    CourseSelect(gfx::Draw2D& draw, const std::string& dataDir);

    enum Result { kStay, kQuit, kRace };

    // Starts the carousel for race mode `mode` (pad +0x0A: 1 the Quick Race; 2, 3 or 6 2 Player) with the course
    // select song, at card `slot` (its bit in the card mask: a course row, or 12-15 the other cards). In 2 Player,
    // rivals[p] is player p's rivals[] entry for the other player (Versus_FindRivals).
    void Start(int mode, int slot = 0, const int* rivals = nullptr);
    // Runs and draws one frame into a frame already begun. kQuit on the frame Exit, Yes (or End Game, Yes) is acted
    // on, kRace on the frame a course is confirmed (see SelectedCard() and CourseId()); after that it keeps drawing
    // the chosen card and takes no input.
    Result Update(const platform::Input& input);

    // The card at the front: its slot (a course row) and its course id.
    int SelectedCard() const { return slots_[sel_]; }
    int CourseId() const { return CardId(slots_[sel_]); }
    // (PC) The Test Course card's records (kTestCourseCard).
    SaveQuickRace& TestRecord() { return testRecord_; }
    void ClearTestRecord();

private:
    enum Child { kNoChild, kInfo, kQuitDialog, kSaveExit, kVersusParts, kSellParts };
    enum SaveExitChild { kNoSaveChild, kSaving, kEndGame };

    void Reset();
    int CardId(int slot) const;
    void DrawClouds();
    void DrawCards();
    void CourseSelect_DrawCard(int slot, int x, int y, int size, uint32_t rgba, int viewTime);   // 0x00244D60
    void CourseSelect_DrawCourseView(int id, int x, int y, int size, int viewTime);   // 0x00244988, then the draw
    const gfx::Texture& TestCourse_Name();   // (PC) the Test Course card's name strip
    void CourseInfo_Task(const platform::Input& input);                          // 0x00248180
    void CourseInfo_DrawCourse(uint32_t rgba);                                   // 0x002477A8
    void CourseInfo_DrawRecords(uint32_t rgba);                                  // 0x00247BF0
    void CourseInfo_DrawVersusRecords(uint32_t rgba);                            // 0x00247E38
    void CourseSelect_QuitTask(const platform::Input& input);                    // 0x00248A70
    void CourseSelect_SaveExitTask(const platform::Input& input);                // 0x00248BF0
    void CourseSelect_VersusPartsTask(const platform::Input& input);           // 0x00248580
    void CourseSelect_SellPartsTask(const platform::Input& input);             // 0x00248398
    bool Versus_CarThere(int i) const;          // car i of the six (3 x player + car) is in the team
    void Versus_SetCar(int i);                  // car i's record from its player's save
    void Versus_DrawCar(int i);                 // 0x0022C280
    void Versus_DrawMoney();                    // 0x0022EDB8
    void OpenQuitDialog();
    void ChildDone(int result);
    bool Versus() const { return (mode_ & 2) != 0; }
    int Pad() const { return Versus() ? platform::kEitherPad : 0; }   // which pads the course select reads

    gfx::Draw2D& d_;
    std::string dataDir_;
    bool modelsLoaded_[2];
    gfx::Model courses_[2][0x12];   // the miniatures by course id: SYS/S00.E3D (Quick Race), S01.E3D (2 Player)
    bool testLoaded_;                // (PC) the Test Course's card: its course's own blocks, seen from further off
    gfx::Model testBlocks_[64];
    const gfx::Texture* testName_ = nullptr;
    SaveQuickRace testRecord_;
    int mode_;       // the race mode, pad +0x0A
    int slots_[16];  // the cards: their slots, from the card mask (+0x20)
    int count_;      // +0x26: how many
    int rivals_[2];  // pad +0x1C, +0x1E
    int sel_;      // +0x27: the card at the front
    int scroll_;    // +0x02: rotation still to show, 8 per card, counting down to 0
    int state_;     // +0x25: 0 choosing, 1 a card chosen (it moves to the top left), 2 it moves back
    int timer_;     // +0x08: frames since the last change of state or rotation
    int clouds_;    // the frame counter the clouds scroll with
    bool quit_;     // Exit, Yes or a course was chosen

    Child child_;
    int childState_;    // the child's +0x25: 0 running, 2 closing
    int childTimer_;    // the child's +0x08
    bool hasResult_;    // a child's result (+0x24) waits for the carousel's next frame
    int result_;        // +0x04: -1 back, 1 a course, 2 quit
    Window dialog_;     // the Quit Games dialog (Dialog_YesNoTask, 0x00239150)
    bool dialogOpen_;
    Window saveExit_;   // Save&Exit's window
    SaveExitChild saveChild_;
    bool hasSaveResult_;   // a save or the End Game dialog has finished
    int saveResult_;
    VersusSave save_;
    // Change Parts and Trade Items: the factory's parts screen and the Recycle Shop's trading over the card.
    Window menu_;       // Change Parts' six cars / Trade Items' two players
    int shownCar_;      // +0x27: the car shown on the right
    bool partsOpen_;    // the parts screen or the trade is open (the task's +0x25 = 1)
    CarRecord records_[6];
    RaceCar shown_[6];
    RaceLight light_;
    Cars models_;       // the six cars' bodies, and the parts cards'
    PartsScreen parts_;
};

class QuickRace {
public:
    QuickRace(gfx::Draw2D& draw, const std::string& dataDir);

    // From the main menu: the loading screen over the last picture, then the course select.
    void Start();
    // Runs and draws one frame. True once it's over and the title should come back.
    bool Update(const platform::Input& input);
    // Development (--race): straight into course `courseId`'s race, without the course select.
    void StartRace(int courseId);
    // The title's attract demo races on the Quick Race's Race (one course loaded at a time).
    Race& DemoRace() { return race_; }

private:
    enum Phase { kLoading, kSelect, kExitFade, kExitLoading, kRaceLoading, kRace, kRaceExit };

    void QuickRace_SetPlayerCar(int card);
    void QuickRace_StartRace(int id);

    gfx::Draw2D& d_;
    CourseSelect select_;
    Race race_;
    Phase phase_;
    int frame_;
    int card_;   // the race's course card (its records)
};

}  // namespace game
