// The title Options' setting windows (docs/title.md): Vibration (Options_VibrationTask, 0x00208C98), Speaker
// (Options_SpeakerTask, 0x00208F50) and Sound Volume (Options_VolumeTask, 0x002090C0). The title's Options opens
// them, and on the PC so does the field menu's Settings > Other.
#pragma once

#include "../../gfx/draw2d.h"
#include "../../platform/input.h"
#include "window.h"

namespace game {

class OptionWindows {
public:
    enum Kind { kVibration, kSpeaker, kVolume };
    // Opens `kind`'s window (it fades in); Vibration's lines in `language` (Title_Language: 0 English, 1 French,
    // 2 German).
    void Open(Kind kind, int language = 0);
    // One frame of it; false once it has closed.
    bool Update(gfx::Draw2D& d, const platform::Input& input);

private:
    void Options_VibrationTask();
    void Options_SpeakerTask();
    void Options_VolumeTask();

    Window w_;
    Kind kind_ = kVibration;
};

}  // namespace game
