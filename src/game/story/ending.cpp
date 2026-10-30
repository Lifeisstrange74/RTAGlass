#include "ending.h"

#include <string.h>

#include "../../audio/sound.h"
#include "../race/race.h"
#include "../text.h"
#include "../../game_text.inc"   // the game's text, GT_<address>_<length>, made from the ELF

namespace game {

namespace {

const uint32_t kSongFanfare = 0x0A1C0201;   // FANFARE.TSQ
const uint32_t kSongEnding = 0x0A1A0201;    // ENDING.TSQ
// Ending_Task's timer: the fanfare and scene 0 fade out from 0x30C and give way to scene 1 at 0x34C.
const int kSceneZeroFade = 0x30C, kSceneZeroEnd = 0x34C;
// D_002DCD50: the frame (from the roll's start) each scene ends at; Fade_OutTask 0x40 frames before.
const int kSceneEnd[Field::kEndingScenes] = {0, 1260, 2520, 3780, 5040, 6300, 7560, 8820, 10080, 12033};
const int kFade = 0x40;   // Fade_InTask and Fade_OutTask: overlay alpha 2 a frame
const int kLastScene = Field::kEndingScenes - 1;

// D_002DC938: {x, the frames after the last entry, text, palette}. The first entry whose text starts with a space
// ends the table; its palette 0xFFFF is the stop mark.
struct StaffLine {
    int16_t x;
    int16_t delay;
    const char* text;
    uint16_t pal;
};
const uint16_t kStopMark = 0xFFFF;
const StaffLine kStaffRoll[] = {
    { 40,  360, GT_0032F870_30},
    { 80,   60, ""},
    { 40,  360, GT_0032F860_9},
    { 60,   60, GT_0032F850_12},
    { 40,  360, GT_0032F830_28},
    { 60,   60, GT_0032F820_15},
    { 40,  360, GT_0032F808_18},
    { 60,   60, GT_0032F7F8_13},
    { 40,  360, GT_0032F7D8_26},
    { 60,   60, GT_0032F7C0_17},
    { 40,  360, GT_0032F7A0_27},
    { 60,   60, GT_0032F790_10},
    { 40,  360, GT_0032F778_21},
    { 60,   60, GT_0032F768_14},
    { 40,  240, GT_0032F750_23},
    { 60,   60, GT_0032F738_19},
    { 40,  240, GT_0032F720_17},
    { 60,   60, GT_0032F710_15},
    { 60,   60, GT_0032F700_14},
    { 60,   60, GT_0032F6F0_15},
    { 40,  240, GT_0032F6E0_14},
    { 60,   60, GT_0032F6D0_10},
    { 60,   60, GT_0032F6C0_12},
    { 60,   60, GT_0032F6B0_12},
    { 60,   60, GT_0032F6A0_10},
    { 40,  240, GT_0032F690_11},
    { 60,   60, GT_0032F678_16},
    { 40,  360, GT_0032F668_8},
    { 60,   60, GT_0032F658_15},
    { 40,  360, GT_0032F640_17},
    { 60,   60, GT_0032F630_15},
    { 60,   60, GT_0032F620_13},
    { 40,  360, GT_0032F610_12},
    { 60,   60, GT_0032F600_11},
    { 40,  360, GT_0032F5F0_8},
    { 40,   60, GT_0032F5E0_15},
    { 60,   60, GT_0032F5D0_13},
    { 40,  360, GT_0032F5B8_18},
    { 60,   60, GT_0032F5A8_11},
    { 40,  240, GT_0032F590_20},
    { 60,   60, GT_0032F578_17},
    { 60,   60, GT_0032F568_14},
    { 40,  240, GT_0032F550_16},
    { 60,   60, GT_0032F538_18},
    { 60,   60, GT_0032F520_16},
    { 40,  240, GT_0032F510_13},
    { 60,   60, GT_0032F500_13},
    { 60,   60, GT_0032F5A8_11},
    { 60,   60, GT_0032F520_16},
    { 40,  240, GT_0032F4F0_12},
    { 60,   60, GT_0032F4E0_11},
    { 60,   60, GT_0032F500_13},
    { 40,  240, GT_0032F4D0_9},
    { 60,   60, GT_0032F4C0_13},
    { 40,  240, GT_0032F4B0_13},
    { 60,   60, GT_0032F4A0_12},
    { 40,  240, GT_0032F490_11},
    { 60,   60, GT_00335458_5},
    { 60,   60, "Bun"},
    { 60,   60, GT_00335448_5},
    { 60,   60, "Ryu"},
    { 60,   60, GT_00335438_6},
    { 40,  360, GT_0032F480_14},
    { 60,   60, GT_0032F5A8_11},
    { 60,   60, GT_0032F4E0_11},
    { 40,  360, GT_0032F470_12},
    { 60,   60, GT_0032F460_15},
    {180, 1080, GT_0032F448_22},
    {  0,    0, "  ", 0xFFFF},
};
const float kRollStart = 448.0f;   // the lines start at the bottom
const float kRollStop = 200.0f;    // the stop mark rises above this: the roll stops
const float kRollGone = -32.0f;

}  // namespace

void StaffRoll::Start() {
    memset(lines_, 0, sizeof lines_);
    stopped_ = false;
}

void StaffRoll::Update(gfx::Draw2D& d, int age) {
    // A table entry is spawned in the first free slot on the frame its delays add up to.
    int at = 0;
    for (const StaffLine& e : kStaffRoll) {
        at += e.delay;
        if (age == at) {
            for (Line& l : lines_) {
                if (l.live) continue;
                l.live = true;
                l.x = e.x;
                l.y = kRollStart;
                l.text = e.text;
                l.pal = e.pal;
                break;
            }
        }
        if (e.text[0] == ' ') break;
    }
    // Each line rises half a line a frame, unless the roll has stopped; one past the top is gone after this frame.
    // Text_PrintAt's line is (int)y / 2, a half-line row in the PS2's frame. (A line at exactly 0 jumps to -1.5.)
    for (Line& l : lines_) {
        if (!l.live) continue;
        if (l.pal == kStopMark && l.y < kRollStop) stopped_ = true;
        else if (!stopped_) l.y -= 0.5f;
        if (l.y < kRollGone) l.live = false;
        if (l.pal != kStopMark) Text_PrintAt(d, l.x, 2 * ((int)l.y / 2), l.text, l.pal);
        if (l.y == 0.0f) l.y = -1.5f;
    }
}

void Ending::Start(Field& field, bool full) {
    field_ = &field;
    shown_ = fadeOut_ = -1;
    timer_ = k_ = 0;
    if (full) {
        // Ending_Task's first frame: scene 0 (Cloud Hill) and the fanfare loading.
        state_ = 0;
        scene_ = 0;
        audio::Sound_LoadSong(kSongFanfare);
    } else {
        // func_0026D2C0: straight to scene 1 and the ending song.
        state_ = 2;
        scene_ = 1;
        audio::Sound_LoadSong(kSongEnding);
    }
    field.Ending_Start(scene_);
}

void Ending::Fade(int t, bool in) {
    int a = (t < kFade ? t : kFade) * 2;
    if (in) a = kFade * 2 - a;
    if (a > 0) d_.Fill(0, 0, gfx::Draw2D::kWidth, gfx::Draw2D::kHeight, (uint32_t)a << 24);
}

bool Ending::Update() {
    // Between scenes the frame is cleared black (info+0x50 0x80000000) until the next has loaded.
    d_.BeginFrame(0x80000000);
    const bool shown = field_->Ending_Update();
    if (shown && shown_ < 0) shown_ = 0;   // func_0026CA48's release: Fade_InTask
    bool done = false;
    if (state_ == 0 && shown) {
        // The loader done: the fanfare starts and the timer runs.
        audio::Sound_StartSong();
        timer_ = 0;
        state_ = 1;
    } else if (state_ == 1 && timer_ == kSceneZeroFade) {
        fadeOut_ = 0;
        audio::Sound_FadeOutSong();
    } else if (state_ == 1 && timer_ == kSceneZeroEnd) {
        // Scene 0 gone, scene 1 and the ending song loading.
        state_ = 2;
        scene_ = 1;
        shown_ = fadeOut_ = -1;
        field_->Ending_Start(scene_);
        audio::Sound_LoadSong(kSongEnding);
    } else if (state_ == 2 && shown) {
        // Scene 1 loaded: the task becomes func_0026D1C0, the roll (func_0026A928) and the song start.
        state_ = 3;
        k_ = 0;
        roll_.Start();
        audio::Sound_StartSong();
    }
    if (state_ == 1) ++timer_;
    if (state_ == 3) {
        // func_0026D1C0: each scene fades out 0x40 frames before its end, and at the end the next is set up; after
        // scene 9 the parent is told.
        if (k_ == kSceneEnd[scene_] - kFade) fadeOut_ = 0;
        if (k_ == kSceneEnd[scene_]) {
            if (scene_ == kLastScene) {
                done = true;
            } else {
                ++scene_;
                shown_ = fadeOut_ = -1;
                field_->Ending_Start(scene_);
            }
        }
    }
    if (shown_ >= 0 && !done) {
        Fade(shown_, true);
        ++shown_;
    }
    if (fadeOut_ >= 0) {
        Fade(fadeOut_, false);
        ++fadeOut_;
    }
    if (done || shown_ < 0) d_.Fill(0, 0, gfx::Draw2D::kWidth, gfx::Draw2D::kHeight, 0x80000000);
    // The roll's own text, over the fades (Main_Loop sends Text_PrintAt's list after every task's drawing,
    // 0x0020053C).
    if (state_ == 3) {
        roll_.Update(d_, k_);
        ++k_;
    }
    d_.EndFrame(Race::kFrameBlend);
    return done;
}

}  // namespace game
