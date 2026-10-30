// The Adventure's race results: a reimplementation of Results_Task (0x00237508) in mode 0 and Results_DrawRace
// (0x002364B0), which Race_SetupTask runs once the finish (and the replay) is black. Over the results' backdrop: the
// race's name, a row for the player and each teammate (func_00235F10: the place, total and best lap, under each its
// record in brackets, a new record blinking), with prize money the Congratulations box (Results_DrawPrizes: each of
// the team's places in the first six and its PRIZE) and a license won, and the wallet (func_0022E7F0: the money now
// and after the prize). Cross pays the prize, raises the license, keeps the records (Results_UpdateRecords) and
// ends it. Race 26 (against the president) has no screen. See docs/finish.md.
#pragma once

#include "../../gfx/draw2d.h"
#include "../../platform/input.h"
#include "../race/race.h"

namespace game {

class AdventureResults {
public:
    explicit AdventureResults(gfx::Draw2D& draw) : d_(draw), result_(), race_(0), prize_(0), license_(0), frame_(0) {}

    // Results_Task's first frame for Adventure race `race`: the prize (Results_PrizeTotal 0x00236238) and the
    // license it wins (+0x27).
    void Start(const RaceResult& result, int race);
    // Whether the race has the screen (not race 26).
    bool HasScreen() const { return race_ != 0x1A; }
    // Draws one frame. 0 while it's up; once Cross has been pressed (the money, license and records saved), what the
    // parent is told: 5 a license won, 4 first, 3 second to sixth, 2 lower with a teammate's prize, 1 lower.
    int Update(const platform::Input& input);
    // Race 26's outcome without the screen: 2 won, 1 lost.
    int Outcome() const { return Place(0) == 0 ? 2 : 1; }

private:
    void Results_DrawRow(int member);   // func_00235F10
    void Results_DrawPrizes();          // 0x00236328
    void Results_UpdateRecords();       // 0x00237C48 in mode 0
    // Team member `member`'s place (0 = first), 0xFF out of the race or not in it.
    int Place(int member) const;
    int Car(int member) const { return result_.versusCar[0][member]; }
    int Record() const { return race_ >= 0x1A ? race_ - 2 : race_; }

    gfx::Draw2D& d_;
    RaceResult result_;
    int race_;
    int prize_;     // +0x20
    int license_;   // +0x27: the license won, 0 none
    int frame_;     // +0x08
};

}  // namespace game
