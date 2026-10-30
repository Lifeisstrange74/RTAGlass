#include "option_windows.h"

#include "../../audio/sound.h"
#include "../pad/vibration.h"
#include "../../game_text.inc"   // the game's text, GT_<address>_<length>, made from the ELF

namespace game {

namespace {

// D_0029AC40, five lines a language (English, French, German; the fourth row is empty).
const char* const kVibrationLines[3][5] = {
    {GT_002EA128_9, " On", GT_003332C0_4, GT_002EA110_19, GT_003332B8_4},
    {GT_002EA128_9, GT_003332B0_4, GT_003332A8_4, GT_002EA0F8_22, GT_003332B8_4},
    {GT_002EA128_9, GT_003332A0_4, GT_00333298_4, GT_002EA0E0_18, GT_003332B8_4},
};
const char* const kSpeakerItems[2] = {GT_003332D0_6, GT_003332D8_4};
const char* const kVolumeItems[6] = {GT_003332E0_6, "4", "3", "2", "1", GT_00333308_7};

// The volume step (D_00333290, 0 = full).
int g_volumeStep = 0;

}  // namespace

void OptionWindows::Open(Kind kind, int language) {
    kind_ = kind;
    w_ = Window();
    w_.pad = platform::kEitherPad;
    w_.state = Window::kOpening;
    if (kind == kVibration) {
        for (int i = 0; i < 5; ++i) w_.AddLine(kVibrationLines[language][i], i ? kLineSelectable : 0);
    } else if (kind == kSpeaker) {
        for (const char* item : kSpeakerItems) w_.AddLine(item, kLineSelectable);
        w_.Place(8);
        w_.sel = (signed char)audio::Sound_GetSpeaker();
        w_.SetLineFlags(w_.sel, kLineSelectable | kLineCurrent);
    } else {
        for (const char* item : kVolumeItems) w_.AddLine(item, kLineSelectable);
        w_.Place(8);
        w_.sel = (signed char)g_volumeStep;
        w_.SetLineFlags(w_.sel, kLineSelectable | kLineCurrent);
    }
}

bool OptionWindows::Update(gfx::Draw2D& d, const platform::Input& input) {
    if (kind_ == kVibration) {
        // The current mode is green.
        for (int line = 1; line <= 3; ++line) w_.SetLineFlags(line, kLineSelectable);
        int mode = Pad_VibrationMode();
        int current = mode == kVibrationOn ? 1 : mode == kVibrationOff ? 2 : mode == kVibrationAnalog ? 3 : 0;
        if (current) w_.SetLineFlags(current, kLineSelectable | kLineCurrent);
    }
    int r = w_.Update(d, input);
    if (r == -2) return false;
    if (r == -1) {
        audio::Sound_PlaySe(0x1B);
        w_.state = Window::kClosing;
    } else if (r == 1) {
        switch (kind_) {
        case kVibration: Options_VibrationTask(); break;
        case kSpeaker: Options_SpeakerTask(); break;
        case kVolume: Options_VolumeTask(); break;
        }
    }
    return true;
}

// Vibration: On, Off, ANALOG mode button (the pad's ANALOG button decides), and TEST. Choosing stays in the
// window; Triangle leaves.
void OptionWindows::Options_VibrationTask() {
    switch (w_.sel) {
    case 0: Pad_SetVibrationMode(kVibrationOn); break;
    case 1: Pad_SetVibrationMode(kVibrationOff); break;
    case 2: Pad_SetVibrationMode(kVibrationAnalog); break;
    case 3:   // TEST: both pads' large motors at half for half a second
        Pad_Vibrate(0, kShakeTest, 1);
        Pad_Vibrate(1, kShakeTest, 1);
        break;
    }
    audio::Sound_PlaySe(0x1A);
}

// Speaker: Stereo or Mono, applied straight away; the window closes either way.
void OptionWindows::Options_SpeakerTask() {
    w_.SetLineFlags(0, kLineSelectable);
    w_.SetLineFlags(1, kLineSelectable);
    audio::Sound_SetSpeaker(w_.sel);
    audio::Sound_PlaySe(0x1F);
    w_.SetLineFlags(w_.sel, kLineSelectable | kLineCurrent);
    w_.state = Window::kClosing;
}

// Sound Volume: six steps from full to mute; sets the master volume to (5 - step) / 5 and plays 0x2F at it.
void OptionWindows::Options_VolumeTask() {
    for (int i = 0; i < 6; ++i) w_.SetLineFlags(i, kLineSelectable);
    w_.SetLineFlags(w_.sel, kLineSelectable | kLineCurrent);
    g_volumeStep = w_.sel;
    int volume = (5 - g_volumeStep) * 0x3FFF / 5;
    audio::Sound_SetMasterVolume(volume, volume);
    audio::Sound_PlaySe(0x2F);
    w_.state = Window::kClosing;
}

}  // namespace game
