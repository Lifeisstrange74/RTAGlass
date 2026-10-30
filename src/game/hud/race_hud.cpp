#include "race_hud.h"

namespace game {

namespace {

// The one-player layout (the templates HudCanvas_Init copies from D_002EC070-D_002EC1A8, and func_00233F10).
const int kTotalLabel[2] = {20, 20};
const int kTotalTime[2] = {60, 52};
const int kLapLabel[2] = {490, 20};
const int kLapRows[2] = {494, 52};   // one row every 24 lines
const int kPosition[2] = {40, 88};
const int kStartLights[2] = {320, 64};   // the panel's centre (Hud_SetStartLights)
// The banners' centres: FINISH! and FINAL LAP (Hud_SetFinish, Hud_SetFinalLap), WRONG WAY over its arrow
// (Hud_SetWrongWay), REPLAY (D_002EC1D8).
const int kBanner[2] = {320, 112};
const int kWrongWay[2] = {320, 135}, kWrongWayArrow[2] = {320, 92};
const int kReplay[2] = {100, 44};
const int kFinishZoom = 30;   // FINISH! grows to full size over 30 frames
// The teammates' places beside the player's (D_002EC0D0, D_002EC0E8).
const int kTeammatePlace[2][2] = {{128, 88}, {128, 104}};

// 2 Player (D_002EC160 and the rest): player 2's elements are lower than player 1's, which are where one player's
// are, by these; the start lights sit on the seam; the meter is routine 10's (D_002A9AF0).
const int kVersusLapDown = 214, kVersusPlaceDown = 312, kVersusBannerDown = 224;
const int kVersusStartLights[2] = {320, 224};
const int kVersusMeter[2][2] = {{492, 112}, {492, 336}};

// On a wide screen (gfx::Draw2D's anchors) the left column (the times, places and map) keeps to the left edge and
// the right one (LAPTIME and the meter) to the right; the start lights and banners stay in the middle.

void DrawBanners(gfx::Draw2D& d, const RaceHudState& s, int down) {
    // The canvas's banner elements, in their order: WRONG WAY (palette 1, the arrow 5), FINISH! (zooming in, scale t
    // / 30), FINAL LAP.
    if (s.wrongWay) {
        Hud_DrawBanner(d, kBannerWrongWay, 1, (float)kWrongWay[0], (float)(kWrongWay[1] + down));
        Hud_DrawBanner(d, kBannerWrongWayArrow, 5, (float)kWrongWayArrow[0], (float)(kWrongWayArrow[1] + down));
    }
    if (s.finish >= 0) {
        float scale = s.finish < kFinishZoom ? (float)s.finish / kFinishZoom : 1.0f;
        Hud_DrawBanner(d, kBannerFinish, 0, (float)kBanner[0], (float)(kBanner[1] + down), scale);
    }
    if (s.finalLap) Hud_DrawBanner(d, kBannerFinalLap, 0, (float)kBanner[0], (float)(kBanner[1] + down));
}

void DrawLaps(gfx::Draw2D& d, const RaceHudState& s, int down) {
    gfx::AnchorScope right(d, gfx::Anchor::Right);
    Hud_DrawLabel(d, kLabelLapTime, kLapLabel[0], kLapLabel[1] + down);
    for (int i = 0; i < s.lapRowCount; ++i) {
        const RaceHudState::LapRow& r = s.lapRows[i];
        Hud_DrawTimeRow(d, kLapRows[0], kLapRows[1] + down + 24 * i, r.time, r.style, r.lap);
    }
}

void DrawPlaces(gfx::Draw2D& d, const RaceHudState& s, int down) {
    gfx::AnchorScope left(d, gfx::Anchor::Left);
    Hud_DrawPosition(d, kPosition[0], kPosition[1] + down, s.order, s.cars);
    for (int k = 0; k < 2; ++k)
        Hud_DrawPosition(d, kTeammatePlace[k][0], kTeammatePlace[k][1] + down, s.teammateOrder[k], 0);
}

}  // namespace

void RaceHud::Load(const std::string& dataDir, int id, const std::string& dir) {
    id_ = id;
    map_.Load(dataDir, dir);
}

// Meter_Draw for one player: the routine of the meter part.
void RaceHud::DrawMeter(gfx::Draw2D& d, const RaceHudState& s) const {
    gfx::AnchorScope right(d, gfx::Anchor::Right);
    if (s.meterState) Meter_Draw(d, s.meterPart, s.meter, *s.meterState, s.frame);
    else Meter_Draw0(d, kMeterPosition[0][0], kMeterPosition[0][1], s.meter, s.frame);
}

void RaceHud::Draw(gfx::Draw2D& d, const RaceHudState& s) const {
    // The replay clears the canvas but for REPLAY (palette 4); the race task draws no map and Meter_Draw no meter.
    if (s.replay) {
        gfx::AnchorScope left(d, gfx::Anchor::Left);
        Hud_DrawBanner(d, kBannerReplay, 4, (float)kReplay[0], (float)kReplay[1]);
        return;
    }
    if (!s.finished) DrawMeter(d, s);
    DrawMap(d, s.markers, s.markerCount);

    if (s.lights) Hud_DrawStartLights(d, *s.lights, kStartLights[0], kStartLights[1]);

    {
        gfx::AnchorScope left(d, gfx::Anchor::Left);
        Hud_DrawLabel(d, kLabelTotalTime, kTotalLabel[0], kTotalLabel[1]);
        Hud_DrawTimeRow(d, kTotalTime[0], kTotalTime[1], s.totalTime, kTimeRunning);
    }
    DrawLaps(d, s, 0);
    DrawPlaces(d, s, 0);
    DrawBanners(d, s, 0);
}

void RaceHud::DrawVersus(gfx::Draw2D& d, const RaceHudState s[2]) const {
    // Meter_Draw runs routine 10 for each player still racing; then the race task's map, then the canvas.
    {
        gfx::AnchorScope right(d, gfx::Anchor::Right);
        for (int p = 0; p < 2; ++p)
            if (!s[p].finished) Meter_Draw10(d, kVersusMeter[p][0], kVersusMeter[p][1], s[p].meter, s[p].frame);
    }
    DrawMap(d, s[0].markers, s[0].markerCount);
    if (s[0].lights) Hud_DrawStartLights(d, *s[0].lights, kVersusStartLights[0], kVersusStartLights[1]);
    {
        gfx::AnchorScope left(d, gfx::Anchor::Left);
        Hud_DrawLabel(d, kLabelTotalTime, kTotalLabel[0], kTotalLabel[1]);
        Hud_DrawTimeRow(d, kTotalTime[0], kTotalTime[1], s[0].totalTime, kTimeRunning);
    }
    for (int p = 0; p < 2; ++p) {
        DrawLaps(d, s[p], p * kVersusLapDown);
        DrawPlaces(d, s[p], p * kVersusPlaceDown);
        DrawBanners(d, s[p], p * kVersusBannerDown);
    }
}

void RaceHud::DrawEventMeters(gfx::Draw2D& d, const RaceHudState* s, int players) const {
    if (players == 1) {
        if (!s[0].finished) DrawMeter(d, s[0]);
    } else {
        gfx::AnchorScope right(d, gfx::Anchor::Right);
        for (int p = 0; p < players; ++p)
            if (!s[p].finished) Meter_Draw10(d, kVersusMeter[p][0], kVersusMeter[p][1], s[p].meter, s[p].frame);
    }
}

void RaceHud::DrawMap(gfx::Draw2D& d, const MapMarker* markers, int count) const {
    gfx::AnchorScope left(d, gfx::Anchor::Left);
    map_.Draw(d, RaceMap_Tune(RaceMap_EditView(id_)), markers, count);
}

void RaceHud::DrawEventElements(gfx::Draw2D& d, HudElement* elements, int count) {
    for (int i = 0; i < count; ++i) {
        gfx::AnchorScope anchor(d, Hud_AnchorAt((float)elements[i].x));
        Hud_DrawElement(d, elements[i]);
    }
}

}  // namespace game
