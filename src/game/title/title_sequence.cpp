#include "title_sequence.h"

#include <math.h>

#include <vector>

#include "../../audio/sound.h"
#include "../extras.h"
#include "../race/race.h"
#include "../text.h"
#include "../../game_text.inc"   // the game's text, GT_<address>_<length>, made from the ELF

namespace game {

namespace {

// The previous-frame weight the game uses on these screens (Frame_BlendPrevious, FIX 0x20).
const uint8_t kBlend = 0x20;
const float kPi = 3.14159265f;

}  // namespace

TitleSequence::TitleSequence(gfx::Draw2D& draw, const std::string& dataDir)
    : d_(draw), menu_(draw, dataDir), newGame_(draw, dataDir), quickRace_(draw, dataDir), versus_(draw, dataDir),
      state_(kConspiracy), frame_(0) {
    gfx::LoadObj(dataDir + "\\models\\SYS\\TAKARA.obj", takara_);
    Enter(kConspiracy);
}

const char* TitleSequence::StateName() const {
    switch (state_) {
    case kConspiracy: return "Conspiracy logo";
    case kTakara: return "TAKARA logo";
    case kPressStart: return "title";
    case kDemo: return "attract demo";
    case kMenu: return "main menu";
    case kNewGame: return GT_002EA1D8_8;
    case kQuickRace: return GT_002EA340_10;
    case kVersus: return GT_002EA330_8;
    }
    return "";
}

void TitleSequence::StartRace(int courseId) {
    quickRace_.StartRace(courseId);
    Enter(kQuickRace);
}

void TitleSequence::StartFactory(int race, int fieldTown, int roomTown, int roomPlace) {
    newGame_.StartFactory(race, fieldTown, roomTown, roomPlace);
    Enter(kNewGame);
}

void TitleSequence::Enter(State s) {
    state_ = s;
    frame_ = 0;
}

void TitleSequence::Update(const platform::Input& input) {
    Info_Tick();
    State before = state_;
    int age = frame_;
    switch (state_) {
    case kConspiracy: Logo_ConspiracyTask(); break;
    case kTakara: Logo_TakaraTask(); break;
    case kPressStart: Title_PressStartTask(input); break;
    case kDemo: Title_AttractDemoTask(input); break;
    case kMenu:
        switch (menu_.Update(input)) {
        case MainMenu::kStay: break;
        case MainMenu::kNewGame:
            newGame_.Start();
            Enter(kNewGame);
            break;
        case MainMenu::kContinue:
            newGame_.Continue();
            Enter(kNewGame);
            break;
        case MainMenu::kQuickRace:
            quickRace_.Start();
            Enter(kQuickRace);
            break;
        case MainMenu::kVersus:
            versus_.Start(menu_.VersusMode());
            Enter(kVersus);
            break;
        case MainMenu::kTestMenu:
            // The root's result 2: Debug_TestMenuTask (0x0026B848), which runs the New Game flow's parts.
            newGame_.StartTestMenu();
            Enter(kNewGame);
            break;
        }
        break;
    case kNewGame:
        if (newGame_.Update(input)) Enter(kPressStart);
        break;
    case kQuickRace:
        if (quickRace_.Update(input)) Enter(kPressStart);
        break;
    case kVersus:
        if (versus_.Update(input)) Enter(kPressStart);
        break;
    }
    // The logos can be skipped (a PC courtesy; the PS2 waits them out).
    if ((state_ == kConspiracy || state_ == kTakara) && (input.Pressed() & platform::kStart)) {
        audio::Sound_StopSong();   // TitleSeq_Task stops the song and plays the start sound when it moves on
        audio::Sound_PlaySe(0x1F);
        Enter(kPressStart);
    }
    if (state_ == before && frame_ == age) ++frame_;   // a screen entered this frame starts at 0
}

// A full-screen colour over everything drawn so far: the game's fades (Fade_InTask 0x00208270,
// Fade_OutTask 0x002082D8) set its alpha each frame.
void TitleSequence::Fade_SetOverlay(uint32_t rgba) {
    if (rgba >> 24) d_.Fill(0, 0, gfx::Draw2D::kWidth, gfx::Draw2D::kHeight, rgba);
}

// Conspiracy Entertainment logo (SYS/LOGO.GSL), 384x384 at (128, 32) on black. Fades in over 32 frames, starts
// fading out at frame 212 and ends at 244.
void TitleSequence::Logo_ConspiracyTask() {
    d_.BeginFrame(0x80000000);
    if (frame_ < 244) d_.Sprite(d_.Get("SYS/LOGO.GSL", 0), 128, 32, 384, 384, 0, 0, 384, 384);
    if (frame_ < 32) Fade_SetOverlay((uint32_t)(32 - frame_) << 26);
    if (frame_ >= 212) Fade_SetOverlay((uint32_t)(frame_ - 212 < 32 ? frame_ - 212 : 32) << 26);
    d_.EndFrame(kBlend);
    if (frame_ + 1 >= 244) Enter(kTakara);
}

// TAKARA logo (SYS/TAKARA.E3D, 3D) on near-white. Over the first 64 frames the camera's projection scale drops
// from 60000 to 10000 (the text shrinks from filling the screen); over the next 64 two rotation angles unwind
// from 0x1C00 and 0x1100 to 0. It holds until frame 165. The TAKARA jingle (ACTION.TSQ entry 0x28) starts with it.
void TitleSequence::Logo_TakaraTask() {
    if (frame_ == 0) audio::Sound_PlaySe(0x328);
    int t = frame_ + 1;
    float focal = t < 65 ? (64 - t) * 781.25f + 10000.0f : 10000.0f;
    int yaw = 0x1C00, roll = 0x1100;
    if (t >= 65) {
        yaw = t < 129 ? (128 - t) * 0x70 : 0;
        roll = t < 129 ? (128 - t) * 0x44 : 0;
    }
    d_.BeginFrame(0x80FCFCFC);
    DrawTakara(focal, yaw, roll);
    d_.EndFrame(kBlend);
    if (t >= 0xA5) Enter(kPressStart);
}

// The camera sits 50 units in front of the model (which is about 2.2 x 0.4 units), looking at it; `focal` is
// pixels per unit at distance 1. Angles are PS2 16-bit units (0x10000 = a full turn).
void TitleSequence::DrawTakara(float focal, int yaw, int roll) {
    if (takara_.indices.empty()) {
        Text_Print(d_, 13, 18, "TAKARA", 0, 0x800000FF);   // model missing: run modelconv
        return;
    }
    float a = yaw * (2 * kPi / 65536.0f), b = roll * (2 * kPi / 65536.0f);
    float ca = cosf(a), sa = sinf(a), cb = cosf(b), sb = sinf(b);
    std::vector<gfx::Vertex> out;
    out.reserve(takara_.indices.size());
    for (uint32_t i : takara_.indices) {
        const gfx::MeshVertex& v = takara_.vertices[i];
        // Roll about the view axis, then yaw about the vertical axis.
        float x = v.pos[0] * cb - v.pos[1] * sb, y = v.pos[0] * sb + v.pos[1] * cb, z = v.pos[2];
        float px = x * ca + z * sa, pz = -x * sa + z * ca;
        float zc = pz + 50.0f;
        // The game's light is fixed to the model (the vertex colours VU1 sends don't change as it turns): fronts
        // come out at about 0.77 of the base colour, tops at 1.0, sides and bottoms at 0.4.
        float nl = v.normal[1] * 0.851f - v.normal[2] * 0.525f;
        float light = 0.40f + 0.705f * (nl > 0 ? nl : 0.0f);
        if (light > 1.0f) light = 1.0f;
        uint32_t r = (uint32_t)(v.color[0] * 255 * light), g = (uint32_t)(v.color[1] * 255 * light),
                 bl = (uint32_t)(v.color[2] * 255 * light);
        gfx::Vertex o;
        o.x = gfx::Draw2D::kWidth / 2 + px * focal / zc;
        o.y = gfx::Draw2D::kHeight / 2 - y * focal / zc;
        o.z = 10.0f / zc;
        o.color = gfx::Draw2D::Color(0x80000000u | (bl << 16) | (g << 8) | r);
        o.s = o.t = 0;
        o.q = 1;
        o.fog = 1;
        out.push_back(o);
    }
    d_.Triangles(out.data(), (uint32_t)out.size());
}

void TitleSequence::DrawTitle() {
    d_.Pillarbox();   // the picture is 4:3 (a wide screen's sides black)
    d_.Sprite(d_.Get("SYS/TITLE.GSL", 0), 0, 0, 640, 448, 0, 0, 640, 448);
    d_.Sprite(d_.Get("SYS/TITLE.GSL", 1), 16, 24, 440, 220, 0, 0, 440, 220);
}

// Title screen (SYS/TITLE.GSL): background 640x448 and licence plate 440x220 at (16, 24), fading in over
// 16 frames. "PRESS START BUTTON" (palette 4) blinks every 32 frames at text cell (10, 11). After 1816 frames the
// game runs its attract demo. Start opens the main menu.
void TitleSequence::Title_PressStartTask(const platform::Input& input) {
    d_.BeginFrame(0x80FFFFFF);
    DrawTitle();
    // In the title's language (func_00208A88's switch on D_00333284: D_002EA090, D_002EA0A8, D_002EA0C8).
    static const char* const kPressStart[3] = {GT_002EA090_18, GT_002EA0A8_27,
                                               GT_002EA0C8_20};
    if (frame_ < 0x708 && (frame_ & 0x20)) Text_Print(d_, 10, 11, kPressStart[Title_Language()], 4);
    if (frame_ < 16) Fade_SetOverlay((uint32_t)(15 - frame_) << 27);
    d_.EndFrame(kBlend);
    if (frame_ >= 16 && (input.Pressed() & platform::kStart)) {
        audio::Sound_StopSong();
        audio::Sound_PlaySe(0x1F);
        menu_.Reset();
        Enter(kMenu);
        return;
    }
    if (frame_ + 1 >= 0x718) Enter(kDemo);
}

// The attract demo (Title_AttractDemoTask, 0x0020A150): race mode 8 on Endurance Run (Race::StartDemo) to
// REPLAY.TSQ. While the loader works the held title darkens by a black 8/128 a frame under "NOW LOADING..." (text
// cell (12, 10)); the song starts when it's done. At frame 3600 the song fades, then the screen fades to black,
// and at 3664 (loading included) the title comes back, to count to the demo again. Any button newly pressed on
// either pad once loaded (TitleSeq_Task's state 3: info+0x30) stops the song and goes back to the title with SE
// 0x1F, before the race sees it.
void TitleSequence::Title_AttractDemoTask(const platform::Input& input) {
    // GUESS: the PS2's load time (unmeasured); the PC loads the course in one go on the loading screen's 2nd frame.
    const int kLoadFrames = 0x40, kSongFade = 3600, kEnd = 3664;
    if (frame_ < kLoadFrames) {
        // REPLAY.TSQ's song; with the Demo Song extra DEMO.TSQ's (file 0x1B, entry 1), which nothing in the game
        // loads: docs/extras.md.
        if (frame_ == 0) audio::Sound_LoadSong(Extras_On(kExtraDemoSong) ? 0x0A1B0201 : 0x0A180201);
        if (frame_ == 1) quickRace_.DemoRace().StartDemo();
        // The held picture under an 8/128 black each frame: (120/128)^n of it left.
        d_.BeginFrame(0x80000000);
        DrawTitle();
        Fade_SetOverlay((uint32_t)(128.0f * (1.0f - powf(120.0f / 128.0f, (float)(frame_ + 1)))) << 24);
        Text_Print(d_, 12, 10, GT_002EA398_14, 0);
        d_.EndFrame(0);
        return;
    }
    if ((input.Pressed(0) | input.Pressed(1)) != 0) {
        audio::Sound_StopSong();
        Enter(kPressStart);
        audio::Sound_PlaySe(0x1F);
        Title_PressStartTask(input);
        return;
    }
    if (frame_ == kLoadFrames) audio::Sound_StartSong();
    if (frame_ == kSongFade) audio::Sound_FadeOutSong();
    Race& race = quickRace_.DemoRace();
    d_.BeginFrame(race.ClearColor());
    race.Update(input);
    if (frame_ > kSongFade) Fade_SetOverlay((uint32_t)(frame_ - kSongFade - 1) << 25);
    d_.EndFrame(Race::kFrameBlend);
    if (frame_ + 1 >= kEnd) {
        audio::Sound_StopSong();
        Enter(kPressStart);
    }
}

}  // namespace game
