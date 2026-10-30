// 2 Player: what the root runs after the 2 Player menu. A high-level reimplementation of Versus_Task (0x00211320):
// it finds each player's rival entry, loads, then runs the course select in its 2 Player form (game/quickrace) and,
// after each race, comes back to it on the same card. Exit or End Game fades out to the title. See
// docs/two_player.md.
//
// Rows 0-4 race split screen (game/race), then show the results (game/results), whose Cross saves the records in
// memory (Save&Exit writes them). The events, rows 5-9 (the Highway Race, the Tunnel Race, the Sliding Door Race,
// the Obstacle Course and Soccer), run the same way and save at once.
#pragma once

#include <string>

#include "../../gfx/draw2d.h"
#include "../../platform/input.h"
#include "../quickrace/course_select.h"
#include "../race/race.h"
#include "../results/versus_results.h"

namespace game {

class Versus {
public:
    Versus(gfx::Draw2D& draw, const std::string& dataDir);

    // From the 2 Player menu, both records ready: race mode `mode` (2 Race Right-Away, 3 Random, 6 Custom).
    void Start(int mode);
    // Runs and draws one frame. True once it's over and the title should come back.
    bool Update(const platform::Input& input);

private:
    enum Phase { kLoading, kSelect, kExitFade, kExitLoading, kRaceLoading, kRace, kResults, kRaceExit };

    gfx::Draw2D& d_;
    CourseSelect select_;
    Race race_;
    VersusResults results_;
    Phase phase_;
    int frame_;
    int mode_;       // pad +0x0A
    int card_;       // +0x26: the course select's card, kept over the race
    int rivals_[2];  // pad +0x1C, +0x1E
};

}  // namespace game
