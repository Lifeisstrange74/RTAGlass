#include "versus.h"

#include "../../audio/sound.h"
#include "../flow/loading.h"
#include "../save/save_data.h"

namespace game {

Versus::Versus(gfx::Draw2D& draw, const std::string& dataDir)
    : d_(draw), select_(draw, dataDir), race_(draw, dataDir), results_(draw), phase_(kLoading), frame_(0), mode_(2),
      card_(0) {
    rivals_[0] = rivals_[1] = 0;
}

// Versus_Task's first frame: Versus_FindRivals, then the loader reads the six cars (D_0177EA50: player 1, their
// teammates, player 2, theirs) and the course select (request 5). It starts on the first card.
void Versus::Start(int mode) {
    mode_ = mode;
    Versus_FindRivals(rivals_);
    card_ = 0;
    phase_ = kLoading;
    frame_ = 0;
}

// As QuickRace_Task does: the loading screen holds the last picture for 64 frames, then the course select. Exit or
// End Game plays QuickRace_ExitFadeTask (0x00211948), 64 frames to black, and the title loads. A course confirmed
// goes to Race_Setup with the card (+0x26); after the race the loader runs again (request 5) and the course select
// starts on that card.
bool Versus::Update(const platform::Input& input) {
    switch (phase_) {
    case kLoading:
    case kRaceExit:
        Loading_Draw(d_, frame_);
        if (++frame_ >= 0x40 + 16) {
            select_.Start(mode_, card_, rivals_);
            phase_ = kSelect;
        }
        return false;
    case kSelect:
    case kExitFade:
        d_.BeginFrame(0x80000000);
        switch (select_.Update(input)) {
        case CourseSelect::kStay: break;
        case CourseSelect::kQuit:
            phase_ = kExitFade;
            frame_ = 0;
            break;
        case CourseSelect::kRace:
            card_ = select_.SelectedCard();
            phase_ = kRaceLoading;
            frame_ = 0;
            break;
        }
        if (phase_ == kExitFade) {
            d_.Fill(0, 0, gfx::Draw2D::kWidth, gfx::Draw2D::kHeight, (uint32_t)(frame_ * 2) << 24);
            if (frame_++ == 0x40) {
                phase_ = kExitLoading;
                frame_ = 0;
            }
        }
        d_.EndFrame(phase_ == kRaceLoading ? 0 : 0x20);
        return false;
    case kRaceLoading:
        // Race_Setup with the row (D_002BE1D8): its course, cars and laps, in this race mode.
        Loading_Draw(d_, frame_);
        if (++frame_ >= 0x40 + 16) {
            const CourseRow& row = Course_Row(mode_, card_);
            race_.StartVersus(row.id, card_, row.cars, row.laps, mode_);
            phase_ = kRace;
        }
        return false;
    case kRace:
        // Retire: the loader (request 5) and the course select again, on the same card. Once both players have
        // finished and the fade is black, Race_SetupTask runs Versus_ResultsTask.
        d_.BeginFrame(race_.ClearColor());
        switch (race_.Update(input)) {
        case Race::kRunning: break;
        case Race::kRetired:
            phase_ = kRaceExit;
            frame_ = 0;
            break;
        case Race::kOver:
            results_.Start(race_.Outcome(), card_, Course_Row(mode_, card_).flags);
            phase_ = kResults;
            if (!results_.HasScreen()) {
                // Versus_ResultsTask on rows 5-9: the records at once (Race_Outcome as the event left it; the Tunnel
                // Race goes by the places), the screen kept black, then the loader.
                results_.Versus_SaveRecords(rivals_, race_.Outcome().outcome);
                phase_ = kRaceExit;
                frame_ = 0;
            }
            break;
        }
        d_.EndFrame(race_.FrameBlend());
        return false;
    case kResults:
        // Cross saves the records; the loader then darkens the results as it does any last picture.
        d_.BeginFrame(0x80000000);
        if (results_.Update(input, rivals_)) {
            phase_ = kRaceExit;
            frame_ = 0;
        }
        d_.EndFrame(Race::kFrameBlend);
        return false;
    case kExitLoading:
        Loading_Draw(d_, 0x40 + frame_);
        return ++frame_ >= 15;
    }
    return true;
}

}  // namespace game
