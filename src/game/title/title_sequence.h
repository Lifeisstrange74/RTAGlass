// The boot sequence: Conspiracy logo, TAKARA logo, title screen, main menu. A high-level reimplementation of
// TitleSeq_Task (0x0020A2B0) and the screen tasks it runs; timings and placements come from the original code
// (see docs/title.md). Runs one 60 Hz frame per Update.
#pragma once

#include <string>

#include "../../gfx/draw2d.h"
#include "../../gfx/mesh.h"
#include "../../platform/input.h"
#include "../flow/new_game.h"
#include "../quickrace/course_select.h"
#include "../versus/versus.h"
#include "title_menu.h"

namespace game {

class TitleSequence {
public:
    // dataDir holds models/ (TAKARA.obj); the textures come through `draw`.
    TitleSequence(gfx::Draw2D& draw, const std::string& dataDir);

    void Update(const platform::Input& input);
    // Development (--race): skips to a Quick Race on course `courseId`.
    void StartRace(int courseId);
    // --factory: New Game straight into Q's Factory (the loading screen after the opening).
    // --advrace: straight into that Adventure race instead; --field: out onto the field at that town's Q's Factory;
    // --room: into that building.
    void StartFactory(int race = -1, int fieldTown = -1, int roomTown = -1, int roomPlace = 0);

    // What's on screen, for the window title.
    const char* StateName() const;

private:
    enum State { kConspiracy, kTakara, kPressStart, kDemo, kMenu, kNewGame, kQuickRace, kVersus };

    void Enter(State s);
    void Logo_ConspiracyTask();                        // 0x002086B0
    void Logo_TakaraTask();                            // 0x00208848
    void Title_PressStartTask(const platform::Input& input);   // 0x00208A88
    void Title_AttractDemoTask(const platform::Input& input);  // 0x0020A150, and TitleSeq_Task's state 3
    void DrawTitle();
    void DrawTakara(float focal, int yaw, int roll);
    void Fade_SetOverlay(uint32_t rgba);               // 0x002080F8

    gfx::Draw2D& d_;
    gfx::Mesh takara_;
    MainMenu menu_;   // Title_MainMenuTask (0x00209CB0)
    NewGame newGame_;   // NewGame_Task (0x0026D550), which the root runs after the title
    QuickRace quickRace_;   // QuickRace_Task (0x00211170), likewise
    Versus versus_;   // Versus_Task (0x00211320), likewise
    State state_;
    int frame_;   // frames since the current screen started (the task's age)
};

}  // namespace game
