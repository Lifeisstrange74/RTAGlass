// 2 Player's race results: a reimplementation of Versus_ResultsTask (0x00235B58), which Race_SetupTask runs after
// every 2 Player race once the finish's fade is black. On rows 0-4 the screen cuts in (no fade): the course name in
// its banner, each car's row, the points and the winner. Either pad's Cross saves the records (Results_UpdateVersus)
// and ends it. Rows 5-9 (the events; the Tunnel Race is row 6) have no screen: the task saves the records at once,
// keeps the screen black and ends. See docs/two_player.md, "The finish and the results".
#pragma once

#include "../../gfx/draw2d.h"
#include "../../platform/input.h"
#include "../race/race.h"

namespace game {

// The results screens' backdrop (the packet func_00235880 builds): the Takara checker tiled over the screen and the
// title banner, both from SYS/SCORE.GSL.
void Results_DrawBackdrop(gfx::Draw2D& d);

class VersusResults {
public:
    explicit VersusResults(gfx::Draw2D& draw) : d_(draw), result_(), row_(0), teams_(false), points_(0), frame_(0) {}

    // The race just run on course row `row` (its flags: 8 TEAM BATTLE). The first frame's part: the teams (TEAM
    // BATTLE and both players with a second teammate), player 1's points and Race_Outcome.
    void Start(const RaceResult& result, int row, int flags);
    // Draws one frame. True once Cross has been pressed on either pad: the records are saved by then (rival[p] is
    // player p's rival index).
    bool Update(const platform::Input& input, const int rival[2]);
    // Race_Outcome (func_0023D8C8): 2 player 1 won, 1 player 2.
    int Outcome() const { return points_ >= 11 ? 2 : 1; }
    // Whether the row shows the screen (rows 0-4).
    bool HasScreen() const { return row_ < 5; }
    // The records of this race (Results_UpdateVersus with each player's place, total and best lap) and Race_Outcome
    // `outcome`; rows 5-9 run it at once with the outcome their event left (the Tunnel Race goes by the places).
    void Versus_SaveRecords(const int rival[2], int outcome) const;

private:
    // Versus_DrawRow (0x002359E8): car `member` (0 the player, 1-2 the teammates) of player `side`'s team.
    void Versus_DrawRow(int member, int side);
    int Place(int side, int member) const;

    gfx::Draw2D& d_;
    RaceResult result_;
    int row_;
    bool teams_;     // +0x27
    int points_;     // +0x26: player 1's (player 2's is 21 minus)
    int frame_;
};

}  // namespace game
