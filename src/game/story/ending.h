// The ending, after the president's race and Forest's talk (end code 0x18 in Cloud Hill), and My City Cinemas'
// replay of it. A high-level reimplementation of Ending_Task (0x0026CB60), the cinema's func_0026D2C0, the scene
// sequencer func_0026D1C0 and the staff roll func_0026A928. The full ending is scene 0 (Cloud Hill) under the
// fanfare (song 0x0A1C0201), then scenes 1-9 under the ending song (0x0A1A0201) with the staff roll over them; the
// replay is scenes 1-9 alone. Each scene is the field in its ending mode (Field::Ending_Start). Nothing reads the
// pad: nothing skips it. See docs/finish.md, "The ending".
#pragma once

#include "../../gfx/draw2d.h"
#include "../field/field.h"

namespace game {

// func_0026A928: the names rise from the bottom half a line a frame, each from its frame in D_002DC938, until the
// blank last entry has risen above line 200: then the roll stops, "Thank You for Playing!" held mid-screen.
class StaffRoll {
public:
    void Start();
    // One frame at the roll's age `age` (its task's +0x08), drawn over whatever is there.
    void Update(gfx::Draw2D& d, int age);

private:
    static const int kSlots = 24;   // D_01772488's 20-byte slots ("ending_scroll:WORK_OVERFLOW!")
    struct Line {
        bool live;
        int16_t x;
        float y;   // in half-lines: 448 the bottom
        const char* text;
        uint16_t pal;
    };
    Line lines_[kSlots];
    bool stopped_ = false;   // +0x26
};

class Ending {
public:
    explicit Ending(gfx::Draw2D& draw) : d_(draw) {}

    // Ending_Task (`full`, from Cloud Hill) or the cinema's replay, on `field`.
    void Start(Field& field, bool full);
    // One frame (BeginFrame to EndFrame). True once scene 9 is over (func_0026D1C0's signal to Shop_SceneTask).
    bool Update();

private:
    void Fade(int t, bool in);

    gfx::Draw2D& d_;
    Field* field_ = nullptr;
    StaffRoll roll_;
    // Ending_Task's +0x25: 0 scene 0 loading, 1 scene 0 shown (its timer +0x02 running), 2 scene 1 loading; then 3,
    // the sequencer (func_0026D1C0), with the roll's frame count k.
    int state_ = 0;
    int timer_ = 0;
    int scene_ = 0;      // +0x27
    int k_ = 0;          // func_0026D1C0's age, the roll's too
    int shown_ = -1;     // frames since the scene faded in (Fade_InTask), -1 while it loads
    int fadeOut_ = -1;   // Fade_OutTask's frames, -1 none
};

}  // namespace game
