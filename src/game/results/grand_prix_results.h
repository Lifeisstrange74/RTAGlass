// The World Grand Prix's results: a reimplementation of Results_GrandPrixTask (0x00237388), which Race_SetupTask
// runs after each of the seven stages (races 28-34; after the replay, a retire or a fall too, never Results_Task).
// Every car scores 30 - its place (an out car 0) into the save's points for the stage (+0x3314 + 0x18 x stage); the
// teams are cars 3t..3t+2, the player's team 0. Over the results' backdrop and "World GPX stage n": the stage's
// finishing order (Gpx_DrawStageOrder), the teams' places and then points (Gpx_DrawTeamStage) and the standings over
// the stages so far (Gpx_DrawStandings), with the prize on the final. See docs/finish.md, "World Grand Prix".
#pragma once

#include "../../gfx/draw2d.h"
#include "../../platform/input.h"
#include "../race/race.h"

namespace game {

class GrandPrixResults {
public:
    explicit GrandPrixResults(gfx::Draw2D& draw) : d_(draw), result_() {}

    // The task's first frame: the stage (save +0x3310) and its points, from `result` (Race::Outcome after a finish,
    // Race::RetireResult after a retire or fall).
    void Start(const RaceResult& result);
    // Draws one frame. kRunning while it's up; then what Race_Outcome becomes: 3 a stage run, 0 retired or fallen
    // (stages 1-6); on the final 7 the player's team won, 6 not.
    static const int kRunning = -1;
    int Update(const platform::Input& input);

private:
    void Gpx_DrawStageOrder();   // 0x00237080 (state 0)
    void Gpx_DrawTeamStage();    // 0x00236D28 (states 1 and 2)
    int Gpx_DrawStandings(const platform::Input& input);   // 0x00236858 (state 3)
    int Points(int stage, int car) const;
    bool Blink() const { return (timer_ & 0x20) != 0; }

    gfx::Draw2D& d_;
    RaceResult result_;
    int stage_ = 0;   // +0x26
    int state_ = 0;   // +0x25
    int timer_ = 0;   // +0x08, which Cross sets back to 0 between some states
    int prize_ = 0;   // +0x20
};

}  // namespace game
