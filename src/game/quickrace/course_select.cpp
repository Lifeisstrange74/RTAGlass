#include "course_select.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>

#include <vector>

#include "../../audio/sound.h"
#include "../extras.h"
#include "../flow/loading.h"
#include "../race/course.h"
#include "../save/save_data.h"
#include "../story/room_scene.h"
#include "../text.h"
#include "../../game_text.inc"   // the game's text, GT_<address>_<length>, made from the ELF

namespace game {

namespace {

const float kPi = 3.14159265f;

// The course tables: course id, cars, laps, flags (D_002BE158, D_002BE1D8; each row also has an opponent list, the
// race task and the AI routine). The Quick Race's eight courses each have 24 cars and 3 laps. 2 Player's rows 0-4
// are its courses (6 cars: both players and their teammates), 5-9 its events.
const CourseRow kQuickRaceRows[8] = {
    {0, 24, 3, 3}, {2, 24, 3, 3}, {5, 24, 3, 3}, {6, 24, 3, 3}, {9, 24, 3, 3}, {14, 24, 3, 3}, {16, 24, 3, 3},
    {17, 24, 3, 3},
};
const CourseRow kVersusRows[10] = {
    {0x11, 6, 2, 0xF}, {0x04, 6, 2, 0xF}, {0x05, 6, 2, 0xF}, {0x0A, 6, 2, 0xF}, {0x0D, 6, 2, 0xF},
    {0x14, 2, 1, 3},   {0x1C, 2, 2, 7},   {0x13, 2, 1, 0},   {0x1D, 2, 1, 1},   {0x1A, 6, 1, 0},
};
// (PC) The Test Course's row: the player alone, the Quick Race's three laps, the length and laps shown.
const CourseRow kTestCourseRow = {Course::kTestCourse, 1, 3, 3};
// (PC) Its length for the card, in thousandths of a mile: its AI path (scripts/course/make_c16.py) is 2554 units, and
// the Quick Race courses' paths measure about 1580 units to D_002A8F30's mile.
const int kTestCourseLength = 1616;
// (PC) Its card's name strip, "Test Course", put together from the strip's own letters (SELECT.GSL picture 1): {row,
// first column, last column} for T (Tunnel Race), e, st (Obstacle Cource), a gap, Cour, s and e.
const int kTestCourseLetters[7][3] = {{28, 19, 36}, {29, 173, 187}, {29, 35, 57}, {-1, 0, 7},
                                      {29, 105, 160}, {29, 35, 46}, {29, 173, 187}};
// (PC) Its picture: the course's own blocks (data/models/COURSE/C16) seen as a miniature is, by a camera 9 times as
// far off (2340 above), over the middle of the road's 780 x 845 (x 20-800, z 20-865): the whole course, 147 x 149
// pixels, in the 220 x 156 window of a card at the front.
const float kTestCourseScale = 9.0f;
const float kTestCourseCentre[3] = {410.0f, 0.0f, 442.0f};

// Ids from 0x12 up are not courses: their card shows a picture from SYS/S02.GSL, number D_002A8BF0[id - 0x12].
const int kFirstSpecial = 0x12;
const int kSpecialPicture[16] = {0, 0, 1, 1, 1, 1, 1, 5, 2, 6, 3, 4, 7, 8, 0, 0};

// Track lengths in thousandths of a mile, by course id (D_002A8F30).
const short kLength[32] = {745,  776,  1118, 1646, 994,  1522, 1367, 1646, 1118, 1118, 1553,
                           1367, 1429, 1056, 1864, 2113, 2113, 1864, 0,    0,    1149, 0,
                           0,    0,    0,    0,    0,    0,    1180, 2392, 0,    0};

// The events' explanations in their course box, up to five lines (D_002A8F70, D_002A8F84, D_002A8F98); the
// Obstacle Course's is one empty line. The game's spelling.
const char* const kSlidingDoorText[5] = {GT_00327670_18, GT_00327658_17, GT_00327640_20,
                                         GT_00327628_16, GT_00327610_22};
const char* const kSoccerText[5] = {GT_003275F8_20, GT_003275E8_14, GT_003275D0_20};
const char* const kObstacleText[5] = {""};

const char kSelect[] = "SYS/SELECT.GSL";   // 0 card frame 256x216, 1 name strip 192x(32x28), 2 cloud 200x150

uint32_t Grey(int c) { return 0x80000000u | (uint32_t)c << 16 | (uint32_t)c << 8 | (uint32_t)c; }
uint32_t Alpha(int a) { return (uint32_t)a << 24 | 0x808080u; }

// Both players have a second teammate (record 1's +0x676 and record 2's). 2 Player's Soccer card and the TEAM
// BATTLE line need it.
bool BothHaveTeams() { return Save_Record(0).teammates[1][0] != 0 && Save_Record(1).teammates[1][0] != 0; }

}  // namespace

const CourseRow& Course_Row(int mode, int row) {
    if (mode == 1 && row == kTestCourseCard) return kTestCourseRow;
    return mode == 1 ? kQuickRaceRows[row & 7] : kVersusRows[row % 10];
}

CourseSelect::CourseSelect(gfx::Draw2D& draw, const std::string& dataDir)
    : d_(draw), dataDir_(dataDir), modelsLoaded_(), testLoaded_(false), mode_(1), count_(0), save_(draw),
      shownCar_(0), partsOpen_(false), parts_(draw, dataDir, models_) {
    rivals_[0] = rivals_[1] = 0;
    ClearTestRecord();
    Reset();
}

// As Save_InitBoth does the eight courses' cards.
void CourseSelect::ClearTestRecord() {
    testRecord_ = SaveQuickRace();
    testRecord_.bestLap = testRecord_.bestTotal = kNoRecordTime;
    testRecord_.bestPlace = kNoRecordPlace;
}

// CourseSelect_Task's first frame. The card mask (+0x20) has a bit per slot: slots 0-9 are the rows of the mode's
// table, 12 the parts card (id 0x19), 13 Sell parts (0x1B), 14 Exit (0x1E) and 15 Save&Exit (0x1F). The Quick Race
// has 0x40FF; 2 Player 0x43FF (Race Right-Away), 0xA3FF (Random) or 0xB3FF (Custom), without row 9 (Soccer) unless
// both players have a second teammate. The cards are the mask's bits in order.
void CourseSelect::Start(int mode, int slot, const int* rivals) {
    mode_ = mode;
    Reset();
    unsigned mask = mode == 1 ? 0x40FF : mode == 6 ? 0xB3FF : mode == 3 ? 0xA3FF : 0x43FF;
    if (mode != 1 && !BothHaveTeams()) mask &= ~0x200u;
    // (PC) The Test Course extra adds its card after the courses, before Exit.
    if (mode == 1 && Extras_On(kExtraTestCourse)) mask |= 1u << kTestCourseCard;
    count_ = 0;
    for (int i = 0; i < 16; ++i)
        if (mask >> i & 1) {
            if (i == slot) sel_ = count_;   // Bits_Rank (0x002081D0): the start card (else the first)
            slots_[count_++] = i;
        }
    if (rivals) rivals_[0] = rivals[0], rivals_[1] = rivals[1];
    // The loader reads S00.E3D's miniatures for the Quick Race and S01.E3D's for 2 Player (func_0020B5E8, requests
    // 4 and 5).
    int set = Versus() ? 1 : 0;
    if (!modelsLoaded_[set]) {
        for (int i = 0; i < count_; ++i) {
            int id = CardId(slots_[i]);
            if (id >= kFirstSpecial) continue;
            char path[64];
            snprintf(path, sizeof path, "\\models\\SYS\\S0%d\\%02d.obj", set, id);
            courses_[set][id].Load(dataDir_ + path, d_);
        }
        modelsLoaded_[set] = true;
    }
    if ((mask >> kTestCourseCard & 1) && mode == 1 && !testLoaded_) {
        const std::string dir = dataDir_ + "\\models\\" + Course::ModelDir(Course::kTestCourse) + "\\";
        for (int b = 0; b < 64; ++b) {
            char name[16];
            snprintf(name, sizeof name, "%02d.obj", b);
            testBlocks_[b].Load(dir + name, d_);
        }
        testLoaded_ = true;
    }
    audio::Sound_LoadSong(0x0A190201);   // SELECT.TSQ into slot 2, entry 1
    audio::Sound_StartSong();
}

void CourseSelect::Reset() {
    sel_ = 0;
    scroll_ = 0;
    state_ = 0;
    timer_ = 0;
    clouds_ = 0;
    quit_ = false;
    child_ = kNoChild;
    childState_ = 0;
    childTimer_ = 0;
    hasResult_ = false;
    result_ = 0;
    dialogOpen_ = false;
    saveChild_ = kNoSaveChild;
    hasSaveResult_ = false;
    saveResult_ = 0;
}

// A slot's card id, which also picks its name in the name strip: a row's course id, else 0x19, 0x1B, 0x1E, 0x1F.
int CourseSelect::CardId(int slot) const {
    if (slot < 10) return Course_Row(mode_, slot).id;
    return slot == 12 ? 0x19 : slot == 13 ? 0x1B : slot + 0x10;
}

// The task's frame: act on a child's result, turn the carousel, draw it, take a choice, then run the child. In 2
// Player either pad turns it and picks a card.
CourseSelect::Result CourseSelect::Update(const platform::Input& input) {
    Result result = kStay;
    if (state_ != 0 && hasResult_) {
        hasResult_ = false;
        if (result_ == -1) {   // back from the card
            timer_ = 0;
            state_ = 2;
        } else if (result_ == 2) {   // Exit or End Game, Yes
            audio::Sound_StopSong();   // func_00258FB0
            quit_ = true;
            result = kQuit;
        } else if (result_ == 1) {   // a course: the parent loads its race
            audio::Sound_StopSong();
            quit_ = true;
            result = kRace;
        }
    }
    // Left and right (with the pad's repeat) turn the carousel one card; a turn plays out over 8 frames.
    if (state_ == 0 && !quit_) {
        if (scroll_ == 0) {
            uint32_t r = input.Repeated(Pad());
            if (r & platform::kRight) {
                sel_ = (sel_ + 1) % count_;
                scroll_ = 8;
                audio::Sound_PlaySe(0x24);
            } else if (r & platform::kLeft) {
                sel_ = (sel_ + count_ - 1) % count_;
                scroll_ = -8;
                audio::Sound_PlaySe(0x24);
            }
        }
        if (scroll_ != 0) {
            timer_ = 0;
            scroll_ += scroll_ < 0 ? 1 : -1;
        }
    }

    DrawClouds();
    DrawCards();

    // Cross on a card that has stopped opens its child: a course's information, Exit's question, Save&Exit's
    // menu, or Change Parts' and Trade Items' screens. The card moves to the top left and the others go. The
    // child task starts on the next frame.
    if (state_ == 0 && !quit_ && timer_ != 0 && scroll_ == 0 &&
        (input.Pressed(Pad()) & (platform::kCross | platform::kStart))) {
        int slot = slots_[sel_];
        child_ = slot == 14 ? kQuitDialog : slot == 15 ? kSaveExit : slot == 12 ? kVersusParts
               : slot == 13 ? kSellParts : kInfo;
        childState_ = 0;
        childTimer_ = 0;
        timer_ = 0;
        state_ = 1;
        audio::Sound_PlaySe(0x1A);
    } else if (child_ == kInfo) {
        CourseInfo_Task(input);
    } else if (child_ == kQuitDialog) {
        CourseSelect_QuitTask(input);
    } else if (child_ == kSaveExit) {
        CourseSelect_SaveExitTask(input);
    } else if (child_ == kVersusParts) {
        CourseSelect_VersusPartsTask(input);
    } else if (child_ == kSellParts) {
        CourseSelect_SellPartsTask(input);
    }
    ++timer_;
    ++clouds_;
    return result;
}

// Three rows of 200x150 cloud tiles, each row 64 pixels further along, all moving left a pixel a frame. (On a wide
// screen the rows go on to its edges.)
void CourseSelect::DrawClouds() {
    const gfx::Texture& t = d_.Get(kSelect, 2);
    const int first = (int)floorf(d_.Left() / 200.0f), last = (int)ceilf(d_.Right() / 200.0f) + 1;
    for (int row = 0; row < 3; ++row) {
        int offset = (clouds_ + row * 64) % 200;
        for (int i = first; i < last; ++i)
            d_.Sprite(t, (float)(i * 200 - offset), (float)(row * 150), 200, 150, 0, 0, 200, 150);
    }
}

// The cards stand on a ring seen from above and in front: card k of n sits at angle 2 pi (k - sel) / n, x =
// 320 + 192 sin, top y = 128 + 64 cos, size 192 + 64 cos (256 = full size, at the front). Cards away from the
// front are darker. They're drawn from the back, left before right. While a card is chosen only it is drawn,
// moving 10 pixels a frame between (320, 192) and (160, 32); its course view animates with the frames since it was
// chosen, and is back at the still view while it returns.
void CourseSelect::DrawCards() {
    struct Place {
        bool shown;
        int x, y, size, viewTime;
        uint32_t rgba;
    } place[16];
    for (int i = 0; i < count_; ++i) {
        Place& p = place[i];
        int pos = (i + count_ - sel_) % count_;
        p.viewTime = -1;
        float a = (int16_t)((pos * 8 + scroll_) * 0x2000 / count_) * kPi / 32768.0f;
        p.shown = true;
        p.rgba = Grey(a != 0.0f ? (int)(cos((double)a) * 16) + 0x60 : 0x80);
        if (state_ == 0) {
            p.x = 320 + (int)(sinf(a) * 192);
            p.y = 128 + (int)(cosf(a) * 64);
            p.size = 192 + (int)(cosf(a) * 64);
        } else if (a == 0.0f) {
            int t = timer_ < 16 ? timer_ : 16;
            p.x = state_ == 1 ? 320 - t * 10 : 160 + t * 10;
            p.y = state_ == 1 ? 192 - t * 10 : 32 + t * 10;
            p.size = 256;
            p.viewTime = state_ == 1 ? timer_ : 0;
            if (state_ == 2 && timer_ >= 16) state_ = 0;
        } else {
            p.shown = false;
        }
    }
    for (int k = count_ / 2; k >= 0; --k) {
        int left = (sel_ + count_ - k) % count_, right = (sel_ + k) % count_;
        const Place& l = place[left];
        const Place& r = place[right];
        if (l.shown) CourseSelect_DrawCard(left, l.x, l.y, l.size, l.rgba, l.viewTime);
        if (right != left && r.shown) CourseSelect_DrawCard(right, r.x, r.y, r.size, r.rgba, r.viewTime);
    }
}

// A card at (x, top y) and size (256 = full): the frame 256x216 (CourseSelect_PlaceCard 0x002443D0), the name
// 192x28 on its plate (CourseSelect_PlaceName 0x00244800), and the picture in its window: a course's miniature in
// 3D, other cards a 220x156 picture scaled with the card (CourseSelect_PlacePicture 0x00244538). viewTime is -1
// for the carousel's still view, else the frames the chosen card's view has been animating.
void CourseSelect::CourseSelect_DrawCard(int card, int x, int y, int size, uint32_t rgba, int viewTime) {
    auto scale = [size](int k) { return size * k / 256; };
    int id = CardId(slots_[card]);
    d_.Sprite(d_.Get(kSelect, 0), (float)(x - scale(128)), (float)y, (float)(scale(128) * 2), (float)scale(216), 0, 0,
              256, 216, rgba);
    const bool test = id == Course::kTestCourse;
    d_.Sprite(test ? TestCourse_Name() : d_.Get(kSelect, 1), (float)(x - scale(96)), (float)(y + scale(12)),
              (float)(scale(96) * 2), (float)(scale(40) - scale(12)), 0, test ? 0.0f : (float)(id * 28), 192, 28, rgba);
    if (test) {
        CourseSelect_DrawCourseView(id, x, y, size, viewTime);
    } else if (id >= kFirstSpecial) {
        d_.Sprite(d_.Get("SYS/S02.GSL", kSpecialPicture[id - kFirstSpecial]), (float)(x - scale(110)),
                  (float)(y + scale(50)), (float)(scale(110) * 2), (float)(scale(206) - scale(50)), 0, 0, 220, 156, rgba);
    } else {
        CourseSelect_DrawCourseView(id, x, y, size, viewTime);
    }
}

// A course's miniature (SYS/S00.E3D or S01.E3D, data/models/SYS/S00 or S01) in the card's window, set up as by
// CourseSelect_PlaceCourseView (0x00244988). The camera (D_002A8B50) sits at (0, h, -260) from the centre of the
// course's 200x200 cell, (100, 0, 100), turned by the pitch about x and then the yaw about y (16-bit angles), and
// looks along its z. The still view looks straight down (pitch 0x4000) with a focal length of 440 pixels. Once a
// card is chosen, over its frames t: focal 440 + 13t (1024 from t = 44), pitch 0x4000 - 256t down to 0x1300, yaw
// 32t, h = t up to 20. The view is centred on the card's centre (x, y + size / 2) and keeps its scale, so a
// smaller card shows less of it. The vertical focal is 0.94 of the horizontal (the game's 0.47 on its 224-line
// field). The card's grey doesn't apply to the model.
void CourseSelect::CourseSelect_DrawCourseView(int id, int x, int y, int size, int t) {
    auto scale = [size](int k) { return size * k / 256; };
    int pitch = 0x4000, yaw = 0;
    float focal = 440.0f, h = 0.0f;
    if (t >= 0) {
        focal = t < 0x2C ? 440.0f + t * 13 : 1024.0f;
        pitch = t < 0x2D ? 0x4000 - (t << 8) : 0x1300;
        yaw = (int16_t)(t << 5);
        h = (float)(t < 0x14 ? t : 0x14);
    }
    float a = pitch * (2 * kPi / 65536.0f), b = yaw * (2 * kPi / 65536.0f);
    float ca = cosf(a), sa = sinf(a), cb = cosf(b), sb = sinf(b);
    // The columns of Ry(yaw) Rx(pitch) are the camera's axes.
    gfx::Camera3D c;
    const float right[3] = {cb, 0, -sb}, up[3] = {sb * sa, ca, cb * sa}, fwd[3] = {sb * ca, -sa, cb * ca};
    // (PC) The Test Course has no miniature: its own blocks, the camera as far again as they are bigger.
    const bool test = id == Course::kTestCourse;
    const float k = test ? kTestCourseScale : 1.0f;
    const float center[3] = {100.0f, 0.0f, 100.0f};
    for (int i = 0; i < 3; ++i) {
        c.right[i] = right[i];
        c.up[i] = up[i];
        c.forward[i] = fwd[i];
        c.eye[i] = (test ? kTestCourseCentre[i] : center[i]) + (up[i] * h - fwd[i] * 260.0f) * k;
    }
    c.focalX = focal;
    c.focalY = focal * 0.94f;
    c.centerX = (float)x;
    c.centerY = (float)(y + size / 2);
    gfx::Rect window = {x - scale(110), y + scale(50), x + scale(111), y + scale(208)};
    if (!test) {
        courses_[Versus() ? 1 : 0][id].Draw(d_, c, window);
        return;
    }
    for (const gfx::Model& b : testBlocks_) b.Draw(d_, c, window);
}

// The Test Course card's name: the letters of kTestCourseLetters copied out of the name strip onto its orange, centred
// in a 192 x 28 strip. Made once and kept.
const gfx::Texture& CourseSelect::TestCourse_Name() {
    if (testName_) return *testName_;
    std::vector<uint32_t> src;
    int w = 0, h = 0;
    if (!d_.ReadPixels(kSelect, 1, 0, src, w, h) || w != 192 || h < 30 * 28) return d_.Get(kSelect, 1);
    int width = 0;
    for (const int* l : kTestCourseLetters) width += l[2] - l[1] + 1;
    std::vector<uint32_t> out(192 * 28, src[0]);   // the strip's orange (its top left pixel)
    int x = (192 - width) / 2;
    for (const int* l : kTestCourseLetters) {
        for (int c = l[1]; c <= l[2]; ++c, ++x)
            for (int r = 0; l[0] >= 0 && r < 28; ++r) out[r * 192 + x] = src[(l[0] * 28 + r) * 192 + c];
    }
    testName_ = &d_.GetFile("SYS/SELECT.GSL#1#test-course", out, 192, 28);
    return *testName_;
}

void CourseSelect::ChildDone(int result) {
    result_ = result;
    hasResult_ = true;
    childState_ = 2;
    childTimer_ = 0;
}

// The chosen course's information, fading in over 16 frames: the course box, then the records (the player's in
// the Quick Race, both players' in 2 Player). Cross (after the fade) starts the race; Triangle goes back, fading
// out over 16 frames. The BACK and OK plates show while it's open. In 2 Player either pad's buttons count.
void CourseSelect::CourseInfo_Task(const platform::Input& input) {
    int alpha;
    if (childState_ == 0) {
        alpha = childTimer_ < 16 ? childTimer_ * 8 : 0x80;
    } else if (childTimer_ < 16) {
        alpha = 0x80 - childTimer_ * 8;
    } else {
        child_ = kNoChild;
        return;
    }
    CourseInfo_DrawCourse(Alpha(alpha));
    if (Versus()) CourseInfo_DrawVersusRecords(Alpha(alpha));
    else CourseInfo_DrawRecords(Alpha(alpha));
    if (childState_ == 0) {
        uint32_t pressed = input.Pressed(Pad());
        if (childTimer_ > 0x10 && (pressed & (platform::kCross | platform::kStart))) {
            // Result 1: the carousel hands the course to its parent, which loads the race.
            audio::Sound_PlaySe(0x1A);
            ChildDone(1);
        } else if (pressed & platform::kTriangle) {
            ChildDone(-1);
            audio::Sound_PlaySe(0x1B);
        }
        Help_DrawButton(d_, 0x1FE, 0x20, kButtonOk, Alpha(alpha));
        Help_DrawButton(d_, 0x1A4, 0x20, kButtonBack, Alpha(alpha));
    }
    ++childTimer_;
}

// The course box: "Track 1.118 mile" and the laps (by the row's flags), the number of cars (the Quick Race only),
// TEAM BATTLE (flag 8, when both players have a second teammate; it flashes, palette 0 and 6 by the frame counter's
// bit 4), and the events' explanations in green. The length's fraction is three fixed digits ending at x 0x212 with
// the point just before them.
void CourseSelect::CourseInfo_DrawCourse(uint32_t rgba) {
    const CourseRow& c = Course_Row(mode_, slots_[sel_]);
    Window_DrawCellFrame(d_, 15, 2, 29, 9, 0, rgba);
    if (c.flags & 1) {
        int length = c.id == Course::kTestCourse ? kTestCourseLength : kLength[c.id & 31];
        Text_Print(d_, 16, 3, GT_00334460_5, 0, rgba);
        int x = Number_PrintDigits(d_, 0x212, 0x52, length % 1000, 3, 0, rgba) - 10;
        Text_PrintAt(d_, x, 0x52, ".", 0, rgba);
        Number_PrintRight(d_, x, 0x52, length / 1000, 0, rgba);
        Text_Print(d_, 26, 3, GT_00334470_5, 0, rgba);
    }
    if (c.flags & 2) {
        Number_PrintRight(d_, 0x212, 0x6A, c.laps, 0, rgba);
        Text_Print(d_, 26, 4, GT_00334478_5, 0, rgba);
    }
    if (mode_ == 1) {
        Text_Print(d_, 16, 5, GT_00334480_5, 0, rgba);
        Number_PrintRight(d_, 0x212, 0x82, c.cars, 0, rgba);
        Text_Print(d_, 26, 5, c.cars == 1 ? GT_00334488_4 : GT_00334488_5, 0, rgba);   // (" car": the PC's, the Test Course)
    }
    if ((c.flags & 8) && BothHaveTeams()) Text_Print(d_, 0x13, 6, GT_00327688_11, clouds_ & 0x10 ? 0 : 6, rgba);
    const char* const* text = c.id == 0x13 ? kSlidingDoorText : c.id == 0x1A ? kSoccerText
                            : c.id == 0x1D ? kObstacleText : nullptr;
    for (int i = 0; text && i < 5; ++i)
        if (text[i]) Text_Print(d_, 0x10, i + 4, text[i], 5, rgba);
}

// The records box: player 1's name (green), then the card's records in player 1's save (+0xB78 + 0xC card):
// challenge attempts (CourseInfo_DrawAttempts 0x00247AC8), best total and lap times, and best finish (" 1ST" ..
// "24TH", " --" for none). QuickRace_Task starts a new save, so they're this session's.
void CourseSelect::CourseInfo_DrawRecords(uint32_t rgba) {
    const SaveRecord& r = Save_Record(0);
    const SaveQuickRace& q = slots_[sel_] == kTestCourseCard ? testRecord_ : r.quickRace[slots_[sel_] & 7];
    Window_DrawCellFrame(d_, 10, 10, 29, 16, 0, rgba);
    Text_Print(d_, 11, 11, r.name, 5, rgba);
    Text_Print(d_, 11, 12, GT_00327698_19, 0, rgba);
    Number_PrintRight(d_, 0x226, 0x12A, q.attempts, 0, rgba);
    Text_Print(d_, 11, 13, GT_003276B0_16, 0, rgba);
    Time_Print(d_, 0x1D6, 0x142, q.bestTotal, 0, rgba);
    Text_Print(d_, 11, 14, GT_003276C8_14, 0, rgba);
    Time_Print(d_, 0x1D6, 0x15A, q.bestLap, 0, rgba);
    Text_Print(d_, 11, 15, GT_003276D8_17, 0, rgba);
    Text_Print(d_, 24, 15, Save_PlaceName(q.bestPlace == kNoRecordPlace ? -1 : q.bestPlace), 0, rgba);
}

// 2 Player's records box, wider, with player 1's column on the left and player 2's on the right under their names
// (green): the attempts (each player's races on the row against anyone, summed over their 20 rivals; this is
// CourseInfo_DrawAttempts' 2 Player branch), the best total time (flag 4) and best lap (2 laps or more) from each
// save's row (+0xA38 + 0x20 row), then "Course Score", the wins on this row against the current rival, and "Total
// Score", the wins against them on all ten rows.
void CourseSelect::CourseInfo_DrawVersusRecords(uint32_t rgba) {
    int row = slots_[sel_];
    const CourseRow& c = Course_Row(mode_, row);
    const SaveRecord* r[2] = {&Save_Record(0), &Save_Record(1)};
    const int x[2] = {0x96, 0x1FE}, timeX[2] = {0x5A, 0x1C2};
    Window_DrawCellFrame(d_, 1, 10, 29, 17, 0, rgba);
    Text_Print(d_, 2, 11, r[0]->name, 5, rgba);
    Text_Print(d_, 20, 11, r[1]->name, 5, rgba);
    Text_Print(d_, 10, 12, GT_00327698_19, 0, rgba);
    if (c.flags & 4) Text_Print(d_, 11, 13, GT_003276B0_16, 0, rgba);
    if (c.laps >= 2) Text_Print(d_, 11, 14, GT_003276C8_14, 0, rgba);
    Text_Print(d_, 11, 15, GT_003276F0_12, 0, rgba);
    Text_Print(d_, 11, 16, GT_00327700_11, 0, rgba);
    for (int p = 0; p < 2; ++p) {
        int races = 0, total = 0;
        for (int i = 0; i < 20; ++i) races += r[p]->rivals[i].races[row];
        const SaveVersusRival& rival = r[p]->rivals[rivals_[p]];
        for (int i = 0; i < 10; ++i) total += rival.wins[i];
        Number_PrintRight(d_, x[p], 0x12A, races, 0, rgba);
        if (c.flags & 4) Time_Print(d_, timeX[p], 0x142, r[p]->versus[row].bestTotal, 0, rgba);
        if (c.laps >= 2) Time_Print(d_, timeX[p], 0x15A, r[p]->versus[row].bestLap, 0, rgba);
        Number_PrintRight(d_, x[p], 0x172, rival.wins[row], 0, rgba);
        Number_PrintRight(d_, x[p], 0x18A, total, 0, rgba);
    }
}

// Dialog_YesNoTask (0x00239150) with "Quit Games": Yes / No, No first, centred. It reads either pad in 2 Player.
void CourseSelect::OpenQuitDialog() {
    dialog_ = Window();
    dialog_.AddLine(GT_00327740_10, 0);
    dialog_.AddLine("Yes", kLineSelectable);
    dialog_.AddLine("No", kLineSelectable);
    dialog_.sel = 1;
    dialog_.pad = (unsigned char)Pad();
    dialogOpen_ = true;
}

// Exit: "Quit Games / Yes / No". Yes quits (the carousel stops the song and its parent fades out); No or Triangle
// goes back to the carousel.
void CourseSelect::CourseSelect_QuitTask(const platform::Input& input) {
    if (childState_ == 2) {
        if (childTimer_ >= 16) child_ = kNoChild;
        ++childTimer_;
        return;
    }
    if (childTimer_ == 0) {
        OpenQuitDialog();
    } else if (dialogOpen_) {
        // The dialog is a task of its own, started on this task's first frame, so it begins a frame later.
        int r = dialog_.Update(d_, input);
        if (r == 1) {
            dialogOpen_ = false;
            audio::Sound_PlaySe(0x1A);
            ChildDone(dialog_.sel == 0 ? 2 : -1);
        } else if (r == -1) {
            dialogOpen_ = false;
            audio::Sound_PlaySe(0x1B);
            ChildDone(-1);
        }
    }
    ++childTimer_;
}

// Save&Exit (2 Player's Random and Custom Race): a window at the top right with Save Player 1, Save Player 2 and End
// Game, on either pad. Each save is its player's: it needs that player's own pad's Cross, and the other pad buzzes
// (0x1C). A save runs Save_VersusSaveTask (save_screens.h); afterwards the cursor moves down one. End Game asks
// "Quit Games"; Yes quits. Whatever ends, the window plays 0x1B as it takes the input back (End Game, Yes plays
// 0x1A). Triangle goes back to the carousel. A child's result is acted on the frame after it ends.
void CourseSelect::CourseSelect_SaveExitTask(const platform::Input& input) {
    if (childState_ == 2) {
        if (childTimer_ >= 16) {
            child_ = kNoChild;
            return;
        }
        saveExit_.Update(d_, input);
        ++childTimer_;
        return;
    }
    if (childTimer_ == 0) {
        saveExit_ = Window();
        saveExit_.AddLine(GT_00327750_33, kLineSelectable);
        saveExit_.AddLine(GT_00327778_33, kLineSelectable);
        saveExit_.AddLine(GT_003277A0_8, kLineSelectable);
        saveExit_.Place(6);
        saveExit_.pad = platform::kEitherPad;
        saveExit_.state = Window::kOpening;
    }
    if (hasSaveResult_) {
        hasSaveResult_ = false;
        if (saveChild_ == kEndGame && saveResult_ == 0) {
            audio::Sound_PlaySe(0x1A);
            saveExit_.state = Window::kClosing;
            ChildDone(2);
        } else {
            audio::Sound_PlaySe(0x1B);
            saveExit_.state &= ~Window::kInactive;
            if (saveChild_ == kSaving) ++saveExit_.sel;
        }
        saveChild_ = kNoSaveChild;
    }
    int r = saveExit_.Update(d_, input);
    if (r == 1) {
        int sel = saveExit_.sel;
        if (sel == 2) {
            audio::Sound_PlaySe(0x1A);
            OpenQuitDialog();
            saveExit_.state |= Window::kInactive;
            saveChild_ = kEndGame;
        } else if (input.Pressed(sel) & (platform::kCross | platform::kStart)) {
            audio::Sound_PlaySe(0x1A);
            save_.Start(sel);
            saveExit_.state |= Window::kInactive;
            saveChild_ = kSaving;
        } else {
            audio::Sound_PlaySe(0x1C);
        }
    } else if (r == -1) {
        saveExit_.state = Window::kClosing;
    }
    if (childState_ == 0 && saveChild_ == kNoSaveChild && (input.Pressed(platform::kEitherPad) & platform::kTriangle)) {
        audio::Sound_PlaySe(0x1B);
        saveExit_.state = Window::kClosing;
        ChildDone(-1);
    }
    if (saveChild_ == kSaving) {
        int s = save_.Update(input);
        if (s >= 0) hasSaveResult_ = true, saveResult_ = s;
    } else if (saveChild_ == kEndGame && dialogOpen_) {
        int d = dialog_.Update(d_, input);
        if (d == 1 || d == -1) {
            dialogOpen_ = false;
            hasSaveResult_ = true;
            saveResult_ = d == 1 ? dialog_.sel : 1;
        }
    }
    ++childTimer_;
}

// Car i of 2 Player's six (D_0177EA50: player 1's car and teammates, then player 2's): the player's own car is always
// there, a teammate's only when the save has that teammate.
bool CourseSelect::Versus_CarThere(int i) const {
    const int c = i % 3;
    return c == 0 || Save_Record(i / 3).teammates[c - 1][0] != 0;
}

void CourseSelect::Versus_SetCar(int i) {
    const SaveRecord& r = Save_Record(i / 3);
    const SaveCar& car = r.cars[i % 3];
    records_[i].body = car.body;
    records_[i].paint = car.paint & 0xFFFFFF;
    records_[i].wheelColor = car.wheelColor;
    records_[i].wheel = car.wheel;
    records_[i].billboard = car.billboard;
    records_[i].name = "";
    shown_[i] = RaceCar();
    shown_[i].record = &records_[i];
    shown_[i].partFlags = car.partFlags;
    // func_0022C280 lifts the body of a car on Big tires by 0.85 (+0x74, D_00332E24): Cars::Car_DrawCard does.
    models_.Load(d_, dataDir_, &shown_[i], 1);
}

// func_0022C280: car i standing at the origin facing +z (D_0029F320's fifth place), seen by the camera D_0029F3D0:
// the eye at (-1.9, -0.2, -20.7) in the frame turned by pitch 0x0F80 and then yaw 0x5C00, focal length 1588,
// centred on the screen; course 1's light set.
void CourseSelect::Versus_DrawCar(int i) {
    const float a = 0x0F80 * kPi / 32768.0f, b = 0x5C00 * kPi / 32768.0f;
    const float ca = cosf(a), sa = sinf(a), cb = cosf(b), sb = sinf(b);
    const float right[3] = {cb, 0, -sb}, up[3] = {sb * sa, ca, cb * sa}, fwd[3] = {sb * ca, -sa, cb * ca};
    const float eye[3] = {-1.9f, -0.2f, -20.7f};
    gfx::Camera3D cam;
    for (int k = 0; k < 3; ++k) {
        cam.right[k] = right[k];
        cam.up[k] = up[k];
        cam.forward[k] = fwd[k];
        cam.eye[k] = eye[0] * right[k] + eye[1] * up[k] + eye[2] * fwd[k];
    }
    cam.focalX = 1588.0f;
    cam.focalY = 1588.0f * 0.94f;
    cam.centerX = gfx::Draw2D::kWidth / 2.0f;
    cam.centerY = gfx::Draw2D::kHeight / 2.0f;
    Light_Setup(light_, 1);
    Light_SetHighlight(light_, cam.forward);
    d_.ClearDepth();
    models_.Car_DrawCard(d_, cam, light_, shown_[i], false, true);   // Car_Draw at distance 0 (func_002236D8)
}

// func_0022EDB8: both players' money, player 1's at (0x140, 0x2A) and player 2's at (0x1B8, 0x4A).
void CourseSelect::Versus_DrawMoney() {
    const SaveRecord& p1 = Save_Record(0);
    const SaveRecord& p2 = Save_Record(1);
    Money_Draw(d_, 0x140, 0x2A, p1.money, p1.currency);
    Money_Draw(d_, 0x1B8, 0x4A, p2.money, p2.currency);
}

// The card's art for the parts screens: Q's Factory's room picture of Peach Town (the town and place the task sets,
// info+0x23 = 1, +0x21 = 0). (The PS2 draws whatever its loader left at TBP 0x1F2E; on screen it's the same art.)
namespace {
const gfx::Texture* CardArt(gfx::Draw2D& d) { return RoomScene::Room_Picture(d, 1, 0); }
}

// CourseSelect_VersusPartsTask (0x00248580), Custom Race's Change Parts: the six cars' names (an empty line for a
// missing teammate) in a window at (0x40, 0x84) on either pad, from the task's 17th frame, and the car the cursor
// was last on that's there, standing on the right. Cross with player 1's pad on one of their cars (rows 0-2), or
// player 2's on theirs (3-5), opens the factory's parts screen for it (Parts_WindowTask, func_0022BB88) on that
// player's pad and record; the car on the right shows what it fits. Triangle (either pad) goes back (SE 0x1B).
void CourseSelect::CourseSelect_VersusPartsTask(const platform::Input& input) {
    if (childState_ == 2) {
        if (childTimer_ >= 16) child_ = kNoChild;
        ++childTimer_;
        return;
    }
    if (childTimer_ == 0) {
        menu_ = Window();
        for (int i = 0; i < 6; ++i) {
            const SaveRecord& r = Save_Record(i / 3);
            const int c = i % 3;
            const char* name = c == 0 ? r.name : Versus_CarThere(i) ? Car_DriverRecordName(r.teammates[c - 1][0],
                                                                                             r.teammates[c - 1][1])
                                                                     : "";
            menu_.AddLine(name, kLineSelectable);
            if (Versus_CarThere(i)) Versus_SetCar(i);
        }
        menu_.pad = platform::kEitherPad;
        menu_.x = 0x40;
        menu_.y = 0x84;
        shownCar_ = 0;
        partsOpen_ = false;
    }
    const bool open = partsOpen_;
    if (!open && childTimer_ >= 0x11) {
        if (menu_.Update(d_, input) == 1) {
            const int sel = menu_.sel;
            if (Versus_CarThere(sel) && (input.Pressed(sel / 3) & platform::kCross)) {
                parts_.Start(sel % 3, CardArt(d_), sel / 3, true);
                partsOpen_ = true;
            }
        }
        if (Versus_CarThere(menu_.sel)) shownCar_ = menu_.sel;
    }
    if (childTimer_ >= 0x11) Versus_DrawCar(shownCar_);
    if (partsOpen_) {
        const int r = parts_.Update(input);
        if (parts_.Changed()) Versus_SetCar(shownCar_);
        if (r < 0) partsOpen_ = false;
    }
    if (!open && childTimer_ != 0 && (input.Pressed(platform::kEitherPad) & platform::kTriangle)) {
        audio::Sound_PlaySe(0x1B);
        ChildDone(-1);
    }
    ++childTimer_;
}

// CourseSelect_SellPartsTask (0x00248398), Trade Items: both players' money at the top, and "Sell Player 1 parts." /
// "Sell Player 2 parts." centred, on either pad. Cross (SE 0x1A) opens that player's selling to the other
// (func_00269E50: the Recycle Shop trading), the window gone until it's left; Triangle (either pad) goes back
// (SE 0x1B).
void CourseSelect::CourseSelect_SellPartsTask(const platform::Input& input) {
    if (childState_ == 2) {
        if (childTimer_ >= 16) child_ = kNoChild;
        ++childTimer_;
        return;
    }
    Versus_DrawMoney();
    const bool open = partsOpen_;
    if (childTimer_ == 0) {
        menu_ = Window();
        menu_.AddLine(GT_00327710_20, kLineSelectable);
        menu_.AddLine(GT_00327728_20, kLineSelectable);
        menu_.pad = platform::kEitherPad;
        partsOpen_ = false;
    } else if (!open) {
        if (menu_.Update(d_, input) == 1) {
            audio::Sound_PlaySe(0x1A);
            parts_.StartShop(4, 9, CardArt(d_), menu_.sel, true);
            partsOpen_ = true;
        }
    } else if (parts_.Update(input) < 0) {
        partsOpen_ = false;
    }
    if (!open && !partsOpen_ && childTimer_ != 0 && (input.Pressed(platform::kEitherPad) & platform::kTriangle)) {
        audio::Sound_PlaySe(0x1B);
        ChildDone(-1);
    }
    ++childTimer_;
}

QuickRace::QuickRace(gfx::Draw2D& draw, const std::string& dataDir)
    : d_(draw), select_(draw, dataDir), race_(draw, dataDir), phase_(kLoading), frame_(0), card_(-1) {}

// QuickRace_Task's first frame: a new save for both players (func_00229FD8), player 1's current. So the Quick
// Race's records last until it's left.
void QuickRace::Start() {
    Save_InitBoth();
    Save_SetCurrent(0);
    select_.ClearTestRecord();
    phase_ = kLoading;
    frame_ = 0;
}

// QuickRace_Task (0x00211170) loads the course select (the loading screen holds the last picture for 64 frames;
// the PC then shows the black screen briefly instead of for the disc's loading time) and runs it. When the course
// select quits, QuickRace_ExitFadeTask (0x00211948) fades to black over 64 frames; the title then loads. A course
// confirmed loads its race the same way; leaving the race loads the course select again, on the same card.
bool QuickRace::Update(const platform::Input& input) {
    switch (phase_) {
    case kLoading:
    case kRaceExit:
        Loading_Draw(d_, frame_);
        if (++frame_ >= 0x40 + 16) {
            select_.Start(1, phase_ == kRaceExit ? card_ : 0);   // the race's card (--race too)
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
    case kExitLoading:
        Loading_Draw(d_, 0x40 + frame_);
        return ++frame_ >= 15;
    case kRaceLoading:
        Loading_Draw(d_, frame_);
        if (++frame_ >= 0x40 + 16) {
            card_ = select_.SelectedCard();
            QuickRace_SetPlayerCar(card_);
            QuickRace_StartRace(select_.CourseId());
            phase_ = kRace;
        }
        return false;
    case kRace:
        d_.BeginFrame(race_.ClearColor());
        // The race ends black, after its fade out (and replay); a Quick Race then has no results screen
        // (Results_Task 0x00237508 just keeps the screen black): back to the course select. Retire loads it at once:
        // the loading screen holds the race's last picture, the pause menu included, as it darkens.
        switch (race_.Update(input)) {
        case Race::kRunning:
            break;
        case Race::kRetired:
            phase_ = kRaceExit;
            frame_ = 0;
            break;
        case Race::kOver: {
            // Race_SetupTask, the player having finished: the card's records (func_00237C48).
            const RaceResult& o = race_.Outcome();
            if (card_ == kTestCourseCard)
                Results_UpdateQuickRace(select_.TestRecord(), o.raceBestLap, o.totalTime, o.place);
            else if (card_ >= 0)
                Results_UpdateQuickRace(Save_Current(), card_, o.raceBestLap, o.totalTime, o.place);
            phase_ = kRaceExit;
            frame_ = 0x40;
            break;
        }
        }
        d_.EndFrame(race_.FrameBlend());
        return false;
    }
    return true;
}

// QuickRace_Task, a course confirmed: the player's car is the card's (D_0029C028) in that body's paint from the
// save's colour table.
// (PC) The Test Course's card has Peach Raceway's car (card 0), as its race does (Race::Start).
void QuickRace::QuickRace_SetPlayerCar(int card) {
    SaveRecord& r = Save_Current();
    r.cars[0].body = (uint8_t)QuickRace_PlayerCar(card == kTestCourseCard ? 0 : card).body;
    r.cars[0].paint = r.bodyPaint[r.cars[0].body];
}

// The race on course `id`; (PC) on the Test Course, whose records the save has no card for, the lap to beat for the
// brighter chime is its card's own.
void QuickRace::QuickRace_StartRace(int id) {
    race_.Start(id);
    if (id == Course::kTestCourse) race_.SetBestLap(select_.TestRecord().bestLap);
}

void QuickRace::StartRace(int courseId) {
    Save_InitBoth();
    Save_SetCurrent(0);
    select_.ClearTestRecord();
    card_ = courseId == Course::kTestCourse ? kTestCourseCard : QuickRace_Card(courseId);
    if (card_ >= 0) QuickRace_SetPlayerCar(card_);
    QuickRace_StartRace(courseId);
    phase_ = kRace;
    frame_ = 0;
}

}  // namespace game
