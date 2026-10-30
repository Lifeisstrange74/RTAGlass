// The text entry New Game uses: "Enter your name.", "Enter currency unit." and (later in the game) "Enter
// greeting." A high-level reimplementation of NameEntry_Task (0x0025BCF0) and its helpers (0x0025B5A0-0x0025BE30);
// see docs/new_game.md.
//
// A prompt box at the top shows the text so far as `*` slots; a keyboard of 92 keys (A-Z, a-z, digits, symbols,
// Cancel and Apply) fills the lower part. Cross types the key (or deletes with Cancel, or finishes with Apply);
// Triangle deletes. The entry fades in over 16 frames and out over 16 once applied.
#pragma once

#include <string>

#include "../../gfx/draw2d.h"
#include "../../platform/input.h"

namespace game {

class NameEntry {
public:
    enum Mode { kName, kCurrency, kGreeting };

    explicit NameEntry(gfx::Draw2D& draw);

    // Starts an empty entry (the task's argument is the mode).
    void Start(Mode mode);
    // NameEntry_Task (0x0025BCF0): one frame; draws without starting or ending the frame. Returns true once the
    // entry has been applied and has faded out; Text() is then the result.
    bool Update(const platform::Input& input);

    const std::string& Text() const { return text_; }

private:
    void NameEntry_MoveCursor(const platform::Input& input);   // 0x0025B908
    bool NameEntry_Input(const platform::Input& input);        // 0x0025B798: true on Apply
    void NameEntry_DrawKeyboard(uint32_t rgba);                // 0x0025B5A0
    void NameEntry_DrawPrompt(uint32_t rgba);                  // 0x0025BB08

    gfx::Draw2D& d_;
    Mode mode_;
    int keys_[9];   // the keys typed so far
    int len_;
    int cursor_;    // the key under the cursor
    int age_;       // frames in the current phase
    bool closing_;  // applied: fading out (func_0025BE30)
    std::string text_;
};

}  // namespace game
