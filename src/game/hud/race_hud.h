// The race HUD for one player: TOTALTIME and its time in the top left, the position under it, the course map, LAPTIME
// with the last laps and the running one in the top right, the speedometer in the bottom right, the start lights
// in the top middle during the countdown, and the banners: FINAL LAP, WRONG WAY, FINISH! and the replay's REPLAY.
// A high-level
// reimplementation of the race's part of the HUD canvas (HudCanvas_Init 0x002321B8 lays it out, HudCanvas_Draw
// 0x00233860 draws it) and of RaceMap_Draw / RaceMap_DrawCars and Meter_Draw, which the race task (Race_Task
// 0x0022DDD0) runs each frame. The race fills a RaceHudState; this only draws. See docs/hud.md.
//
// The field's HUD is its own layout over the same pieces (field/field_map.h, the meter with its clock).
#pragma once

#include <string>

#include "../../gfx/draw2d.h"
#include "hud_parts.h"
#include "minimap.h"
#include "speedometer.h"

namespace game {

struct RaceHudState {
    int frame;                // frames since the race began, for blinking
    int totalTime;            // the race clock in frames (0: not running yet)
    struct LapRow {
        int lap;              // 1, 2, ...
        int time;             // frames (0: not running yet)
        HudTimeStyle style;
    } lapRows[3];             // the last laps, then the one being driven (Race_SetLapRows 0x0022D8C8)
    int lapRowCount;
    int order, cars;          // the player's place (0 = first) out of `cars`
    int teammateOrder[2];     // the player's teammates' places (Hud_SetPosition's slots 1, 2), -1 none
    MeterReadout meter;
    int meterPart = 0;                  // the meter routine (the save's car 0's meter part, D_0177F760 +0x1A)
    MeterState* meterState = nullptr;   // the player's meter count (buf+0x1000)
    const MapMarker* markers; // every car, the players' first
    int markerCount;
    StartLights* lights;      // the countdown's lights (null or step 0: none); drawing steps their glow
    bool finished;            // the player has finished (record flag 0x200): the meter goes
    int finish;               // frames since FINISH! went up (Hud_SetFinish 0x00234128), -1 none; it zooms in
    bool finalLap;            // FINAL LAP is lit (Hud_SetFinalLap 0x002341F8); the race blinks it
    bool wrongWay;            // WRONG WAY is lit (Hud_SetWrongWay 0x00232C18); the race blinks it
    bool replay;              // the replay: only REPLAY (HudCanvas_InitReplay 0x00234528), no map or meter
};

class RaceHud {
public:
    // Loads course `id`'s map (data/models/<dir>/map.obj, dir as Course::ModelDir).
    void Load(const std::string& dataDir, int id, const std::string& dir);
    void Draw(gfx::Draw2D& d, const RaceHudState& s) const;
    // The split screen's canvas (HudCanvas_Init's 2 Player templates), for players 1 and 2: each player's LAPTIME,
    // lap rows, places, banners and meter (routine 10) in their half; one TOTALTIME and clock in the top half, the
    // start lights on the seam and one map, from s[0].
    void DrawVersus(gfx::Draw2D& d, const RaceHudState s[2]) const;
    // An event's HUD (events.md: the Sliding Door Race, the Obstacle Course): no times, places or map, only each
    // player's meter (Meter_Draw's own task, as in a race), then the event's canvas elements in their order. The
    // Obstacle Course's half-screen fades go between the two.
    void DrawEventMeters(gfx::Draw2D& d, const RaceHudState* s, int players) const;
    // The course's map and markers alone (Soccer's task draws it, with the ball).
    void DrawMap(gfx::Draw2D& d, const MapMarker* markers, int count) const;
    // RaceMap_DrawStep (func_00242A88): through another view (Figure 8's steps) and cut to its SCISSOR_2 (D_002A4430:
    // x 48-208, field lines 68-200). The Map Tuner extra changes `view` (RaceMap_Tune).
    void DrawMap(gfx::Draw2D& d, MapView& view, const MapMarker* markers, int count) const {
        gfx::AnchorScope left(d, gfx::Anchor::Left);
        map_.Draw(d, RaceMap_Tune(view), markers, count, gfx::Rect{48, 136, 209, 402});
    }
    static void DrawEventElements(gfx::Draw2D& d, HudElement* elements, int count);

private:
    void DrawMeter(gfx::Draw2D& d, const RaceHudState& s) const;
    int id_ = 0;
    MiniMap map_;
};

}  // namespace game
