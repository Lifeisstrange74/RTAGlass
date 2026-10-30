#include "message_task.h"

#include "../../audio/sound.h"
#include "../extras.h"
#include "../race/car.h"
#include "../save/save_data.h"
#include "../text.h"
#include "parts_screen.h"
#include "race_list.h"
#include "../../game_text.inc"   // the game's text, GT_<address>_<length>, made from the ELF

namespace game {

namespace {

const char kFont[] = "SYS/FONTE.GSL";
const int kBoxY = 0x158;           // Message_DrawBox's Y (344): the box spans y 328-432
const int kTextX = 70;             // each line's x
const int kTextY = 0xAD * 2;       // line 1 (346), then 24 a line
const int kTextTop = 0xA9 * 2;     // the scrolling top line is cut above this (338)
const int kNameRight = 0x23E;      // the name tab's right edge (574)
const int kLineScroll = 12;        // +0x26 a line
const int kFullPage = 0x7FFF;

uint32_t Alpha(int a) { return ((uint32_t)a << 24) | 0x808080; }

bool Pressed(const platform::Input& input) { return (input.Pressed(0) & (platform::kCross | platform::kTriangle)) != 0; }

// The event flags, stamps and town tests of the control codes (save +0x538, +0x518).
bool EventFlag(int f) { return (Save_Current().eventFlags[f >> 6 & 1] >> (f & 63) & 1) != 0; }
void SetEventFlag(int f, bool on) {
    uint64_t bit = 1ull << (f & 63);
    uint64_t& w = Save_Current().eventFlags[f >> 6 & 1];
    w = on ? w | bit : w & ~bit;
}
bool StampEarned(int s) { return Stamp_Earned(s); }

bool Photo_Taken(int p) {   // Photo_Taken (0x0023D8E8): photo p (1-based) taken; 0 all of them
    const SaveRecord& r = Save_Current();
    if (p == 0) return r.photoTaken[0] == ~0ull && (r.photoTaken[1] & 0xFFFFFFFFFull) == 0xFFFFFFFFFull;
    --p;
    return p < 128 && ((r.photoTaken[p >> 6] >> (p & 63)) & 1) != 0;
}

// The Duck Quiz extra (the PC's; docs/extras.md). Cloud Hill's duck, (8, 5): questions 2 and 3 (#9, #14) begin with
// "if event flag 75, go to #20", "Quack, quack! You're gonna try again? I think it's too hard for you.", which
// clears 75 and goes back to #1 (and on to the question again), but nothing in the game sets 75. #20 reads as the
// welcome back after giving up on question 2 or 3, so with the extra flag 75 is set when #5 ("You're not gonna
// try?!") or #6 ("Wrong!") comes up while flag 73 (question 1 won) or 74 (question 2 won) is set: the next visit's
// question 2 or 3 opens with #20. (GUESS: what the developers meant. After question 1 the duck has #1's "Huh? You
// came again?".)
const int kDuckTown = 8, kDuckPlace = 5, kDuckFlag = 75;
void DuckQuiz_Entered(int town, int place, int n) {
    if (town != kDuckTown || place != kDuckPlace || (n != 5 && n != 6) || !Extras_On(kExtraDuckQuiz)) return;
    if (EventFlag(73) || EventFlag(74)) SetEventFlag(kDuckFlag, true);
}

// The pre-control codes' argument counts (-1: unknown, the game skips such bytes).
const int kArgs[0x1E] = {-1, 2, 2, 3, 1, 2, 2, 1, 2, -1, -1, 2, -1, 2, 6, 1, 1,
                         2, 0, 1, 2, 1, 1, 1, 1, 2, 2, 1, 2, 3};

}  // namespace

uint32_t MessageTask::Message_EndCodes(const MessageTable& table) {
    uint32_t codes = 0;
    for (size_t m = 1; m < table.size(); ++m) {
        const std::string& s = table[m];
        size_t p = 0;
        while (p < s.size() && (unsigned char)s[p] < 0x20 && s[p] != 0x0A) {
            unsigned char c = s[p];
            if (c >= 0x1E || kArgs[c] < 0) break;
            p += 1 + kArgs[c];
        }
        while (p < s.size() && ((unsigned char)s[p] >= 0x20 || s[p] == 0x0A)) ++p;
        unsigned char end = p < s.size() ? (unsigned char)s[p] : 0;
        if (end < 32) codes |= 1u << end;
    }
    return codes;
}

MessageTask::MessageTask(gfx::Draw2D& draw)
    : blip(0), d_(draw), table_(nullptr), town_(0), place_(0), state_(kDone), age_(0), chars_(0), scroll_(0),
      result_(0), text_(0), end_(0), windowOpen_(false), windowClosing_(false), items_(0), child_(kChildChoice),
      firstRace_(0), noticeOpen_(false), save_(draw), saveClosing_(false), race_(0), goTown_(0), goPlace_(0),
      clock_(kDefaultClock), pickType_(9), childAge_(0), value_(0), digit_(3), pay_(0), miles_(0), teamSlot_(1),
      greeting_(draw), carChanged_(false), teamChanged_(false), leave_(false) {}

void MessageTask::Start(const MessageTable& table, int town, int place) {
    table_ = &table;
    town_ = town;
    place_ = place;
    state_ = kOpen;
    age_ = 0;
    result_ = 0;
    leave_ = false;
    windowOpen_ = windowClosing_ = false;
    noticeOpen_ = saveClosing_ = false;
    Message_Go(1);
    audio::Sound_PlaySe(0x25);
}

void MessageTask::Message_ShopDone() {
    if (state_ != kChild || child_ != kChildShop) return;
    const unsigned char* a = (const unsigned char*)msg_.c_str() + end_ + 1;
    state_ = kRun;
    age_ = 0;
    Message_Go(a[0]);
}

void MessageTask::Message_Go(int n) {
    msg_ = n > 0 && n < (int)table_->size() ? (*table_)[n] : std::string();
    text_ = 0;
    chars_ = 0;
    scroll_ = 0;
    DuckQuiz_Entered(town_, place_, n);
    Message_RunPreCodes();
}

// The codes before the text (Message_RunPreCodes). A jump makes message n the current one and runs its codes in turn.
void MessageTask::Message_RunPreCodes() {
    for (int guard = 0; guard < 64; ++guard) {
        if (text_ >= msg_.size()) return;
        unsigned char c = msg_[text_];
        if (c >= 0x20 || c == 0x0A) return;
        if (c >= 0x1E || kArgs[c] < 0) return;   // "Unknown pre-control code": the game skips such bytes too
        const unsigned char* a = (const unsigned char*)msg_.c_str() + text_ + 1;
        if (text_ + 1 + kArgs[c] > msg_.size()) return;
        int jump = -1;
        switch (c) {
        case 0x01: if (EventFlag(a[0])) jump = a[1]; break;
        case 0x02: if (!EventFlag(a[0])) jump = a[1]; break;
        case 0x03: if (Save_HasItem(a[0], a[1])) jump = a[2]; break;
        case 0x04: if (Place_Hidden(town_, place_)) jump = a[0]; break;   // the place's first visit
        case 0x06: if (StampEarned(a[0])) jump = a[1]; break;
        case 0x05: if (Photo_Taken(a[0])) jump = a[1]; break;
        case 0x07: if (Team_State()) jump = a[0]; break;                   // room on the team
        case 0x08:   // a teammate here; else not beaten yet
            if (Team_Find(town_, place_)) jump = a[0];
            else if (!Save_Beaten(town_, place_)) jump = a[1];
            break;
        case 0x0D: if (Pickup_Taken(a[0])) jump = a[1]; break;            // ChoroQ coin a[0] (1-100) found
        case 0x11: Save_TakeItem(a[0], a[1]); break;
        case 0x16: if (Resident_MovedIn(Resident_Of(town_, place_))) jump = a[0]; break;
        case 0x17: Resident_MoveIn(a[0]); break;
        case 0x18:   // the clock outside 6:00-22:00: night
            if (clock_ < 54000 || clock_ > 198000) jump = a[0];
            break;
        case 0x19: if (Resident_MovedIn(a[0])) jump = a[1]; break;
        case 0x1A: if (100 - Pickups_Left() >= a[0]) jump = a[1]; break;  // at least a[0] coins found
        case 0x1B: {   // the Super A license, and the president's stamp or not flag 87
            const SaveRecord& r = Save_Current();
            if (r.license == 3 && (StampEarned(100) || !EventFlag(87))) jump = a[0];
            break;
        }
        case 0x1D: if (Save_Current().cars[0].parts[a[0] & 15] == a[1]) jump = a[2]; break;
        case 0x0B: if (Race_Outcome() == a[0]) jump = a[1]; break;
        case 0x0E: {   // Q's Rally on: the next checkpoint's message (the game doesn't check the index; 6 isn't one)
            int i = Rally_NextCheckpoint();
            if (EventFlag(80)) jump = a[i < 6 ? i : 5];
            break;
        }
        case 0x13: Rally_Checkpoint(a[0]); break;
        case 0x14:   // result 12 now: this place found, and the car comes out at (t, p) once the talk ends
            Place_MarkFound(town_, place_);
            goTown_ = a[0];
            goPlace_ = a[1];
            leave_ = true;
            break;
        case 0x0F: SetEventFlag(a[0], true); break;
        case 0x10: SetEventFlag(a[0], false); break;
        case 0x15: {   // Stamp_SetTextShown (0x0023D730)
            int s = a[0] - 1;
            if (s >= 0) Save_Current().stampShown[s >> 6 & 1] |= 1ull << (s & 63);
            break;
        }
        case 0x1C: if (town_ == a[0]) jump = a[1]; break;
        default: break;
        }
        if (jump >= 0) {
            msg_ = jump < (int)table_->size() ? (*table_)[jump] : std::string();
            text_ = 0;
            DuckQuiz_Entered(town_, place_, jump);
        } else {
            text_ += 1 + kArgs[c];
        }
    }
}

std::string MessageTask::Expand(char letter, char arg) const {
    const SaveRecord& r = Save_Current();
    switch (letter) {
    case 'N': return r.name;
    case 'U': return r.currency;
    case 'G': return r.greeting;
    case 'R': return Race_CourseName(AdventureRace_Get(race_).course);   // info+0x10's race
    case 'S': {
        // The bank's savings (D_01782A6C, save +0x330C), func_00204460: in the narrow digits with a comma every
        // three; here in the text's own font.
        char n[16], out[24];
        snprintf(n, sizeof n, "%d", r.bank);
        size_t len = strlen(n), o = 0;
        for (size_t i = 0; i < len; ++i) {
            if (i && (len - i) % 3 == 0 && n[i - 1] != '-') out[o++] = ',';
            out[o++] = n[i];
        }
        out[o] = 0;
        return out;
    }
    case 'T': {
        // A time, "%d:%02d" of v / 9000 and v % 9000 / 150: $T7 the rally's best (save +0x12B0, unsigned), else
        // D_01787008[c] with c the raw byte, so $T0-$T6 are the rally's splits and total.
        char out[24];
        if (arg == '7') {
            uint32_t v = (uint32_t)r.rallyBest;
            snprintf(out, sizeof out, "%u:%02u", v / 9000, v % 9000 / 150);
        } else {
            int32_t v = arg >= '0' && arg <= '6' ? Rally_Split(arg - '0') : 0;
            snprintf(out, sizeof out, "%d:%02d", v / 9000, v % 9000 / 150);
        }
        return out;
    }
    default: return std::string();   // $A: an amount (no message here uses it)
    }
}

// Message_DrawText: skips scroll_ / 12 lines, then draws up to three lines (palette 10) moved up by the rest of the
// scroll, the top one cut at kTextTop. Stops at character chars_ (a two-byte character or an escape counts one),
// at the end code, or after three lines.
MessageTask::TextStop MessageTask::Message_DrawText(uint32_t rgba, bool draw) {
    const std::string& s = msg_;
    size_t p = text_;
    int count = 0;
    auto isEnd = [&](size_t k) { return k >= s.size() || ((unsigned char)s[k] < 0x20 && s[k] != 0x0A); };
    auto step = [&](size_t k) -> size_t {   // the bytes of the character at k
        unsigned char c = s[k];
        if (c == '$' && k + 1 < s.size()) return (s[k + 1] == 'A' || s[k + 1] == 'T') && k + 2 < s.size() ? 3 : 2;
        if ((c >= 0x80 || c == '/' || c == '`' || c == '^' || c == '~') && k + 1 < s.size() && s[k + 1] >= 0x20)
            return 2;
        return 1;
    };
    for (int skip = scroll_ / kLineScroll; skip > 0; --skip) {
        while (!isEnd(p) && s[p] != 0x0A) {
            p += step(p);
            ++count;
        }
        if (isEnd(p)) break;
        ++p;
    }
    const int f = scroll_ % kLineScroll;
    for (int line = 0; line < 3; ++line) {
        std::string out;
        TextStop stop = kPageFull;
        bool lineDone = false;
        for (;;) {
            if (isEnd(p)) {
                stop = kEndCode;
                end_ = p;
                break;
            }
            if (s[p] == 0x0A) {
                ++p;
                lineDone = true;
                break;
            }
            if (count == chars_) {
                stop = kTyping;
                break;
            }
            size_t n = step(p);
            if (s[p] == '$') out += Expand(s[p + 1], n == 3 ? s[p + 2] : 0);
            else out.append(s, p, n);
            p += n;
            ++count;
        }
        if (draw && !out.empty()) {
            int y = kTextY + 24 * line - 2 * f;
            Text_PrintClipped(d_, kTextX, y, out.c_str(), 10, kTextTop, rgba);
        }
        if (!lineDone) return stop;
    }
    // A full page: at a page's boundary a skipped page's count becomes the count so far.
    if ((scroll_ / kLineScroll) % 3 == 0 && chars_ == kFullPage) chars_ = count;
    return kPageFull;
}

// Message_DrawBox: FONTE palette 7. Corners 40 wide (the top row 56 tall from y - 16, the bottom 48), the middle rows
// tiled from 8-texel strips at u 0x50 and 0xC8 (CLAMP's region repeat; the strips are the same in every column, so
// the PC stretches their middle texels: tiling them with filtering showed a seam every 8 pixels), and the knob at the
// bottom centre.
void MessageTask::Message_DrawBox(uint32_t rgba) {
    const gfx::Texture& t = d_.Get(kFont, 0, 7);
    const float top = (float)(kBoxY - 16), bottom = (float)(kBoxY + 40);
    d_.Sprite(t, 32, top, 40, 56, 0x00, 0x100, 40, 56, rgba);
    d_.Sprite(t, 32, bottom, 40, 48, 0x78, 0x100, 40, 48, rgba);
    d_.Sprite(t, 72, top, 496, 56, 0x53, 0x100, 2, 56, rgba);
    d_.Sprite(t, 72, bottom, 496, 48, 0xCB, 0x100, 2, 48, rgba);
    d_.Sprite(t, 568, top, 40, 56, 0x28, 0x100, 40, 56, rgba);
    d_.Sprite(t, 568, bottom, 40, 48, 0xA0, 0x100, 40, 48, rgba);
    d_.Sprite(t, 0x130, (float)(kBoxY + 72), 32, 24, 0x58, 0x100, 32, 24, rgba);
}

// Message_DrawNameTab: the tab right-aligned to x 574 at y Y - 0x1C, 28 tall: a left cap (u 0x58 v 0x118), the
// middle from the strip at u 0x60, `width` + 4 wide, and the right cap (u 0xD0); `width` is the name's.
void MessageTask::Message_DrawNameTab(int width, uint32_t rgba) {
    const gfx::Texture& t = d_.Get(kFont, 0, 7);
    const float y = (float)(kBoxY - 0x1C);
    d_.Sprite(t, (float)(kNameRight - 12 - width), y, 8, 28, 0x58, 0x118, 8, 28, rgba);
    d_.Sprite(t, (float)(kNameRight - 4 - width), y, (float)(width + 4), 28, 0x63, 0x118, 2, 28, rgba);
    d_.Sprite(t, (float)kNameRight, y, 24, 28, 0xD0, 0x100, 24, 28, rgba);
}

// End codes 9 (Message_ChoiceTask: one item per 0-terminated string up to a 0x09 byte, then a target per item),
// 1 and 2 (Message_YesNoTask: "Yes" and "No", 2 with the cursor on No): a style-2 window at the top left.
void MessageTask::Message_OpenChoice(bool yesNo, bool onNo) {
    window_ = Window();
    items_ = 0;
    size_t p = end_ + 1;
    if (yesNo) {
        window_.AddLine("Yes", kLineSelectable);
        window_.AddLine("No", kLineSelectable);
        for (int i = 0; i < 2; ++i) targets_[i] = p + i < msg_.size() ? (unsigned char)msg_[p + i] : 0;
        items_ = 2;
        if (onNo) window_.sel = 1;
    } else {
        static std::string items[16];
        while (p < msg_.size() && items_ < 16) {
            size_t z = msg_.find('\0', p);
            if (z == std::string::npos) break;
            items[items_++] = msg_.substr(p, z - p);
            p = z + 1;
            if (p < msg_.size() && msg_[p] == 0x09) break;
        }
        ++p;
        for (int i = 0; i < items_; ++i) {
            targets_[i] = p + i < msg_.size() ? (unsigned char)msg_[p + i] : 0;
            window_.AddLine(items[i].c_str(), kLineSelectable);
        }
    }
    Message_OpenWindow(kChildChoice);
}

// The end codes' windows: style 2, at the top left, opening over 8 frames, the box and its text staying under them.
void MessageTask::Message_OpenWindow(Child child) {
    window_.style = 2;
    window_.Place(5);
    window_.state = Window::kOpening;
    windowOpen_ = true;
    windowClosing_ = false;
    child_ = child;
    state_ = kChild;
}

// Message_PickCarTask's window: the player's name (Save_GetCarName), the teammates' that there are (+0x674, then
// +0x676; the second only with the first) and "Quit".
void MessageTask::Message_PickCarTask(int type) {
    const SaveRecord& r = Save_Current();
    pickType_ = type;
    window_ = Window();
    window_.AddLine(r.name, kLineSelectable);
    for (int t = 0; t < 2 && r.teammates[t][0]; ++t)
        window_.AddLine(Car_DriverRecordName(r.teammates[t][0], r.teammates[t][1]), kLineSelectable);
    window_.AddLine(GT_002FF078_4, kLineSelectable);
    Message_OpenWindow(kChildPickCar);
}

// Message_RaceListTask's window: a line per race of the town (D_002BE000): "*" once the player has a place in it
// below 6th (the save's +0xFF0 byte for the race; 0xFF none), the race's name and its rank on the right, in red
// (disabled) when the license is below it; then "Quit" (padded with spaces in the original, which widens the window).
void MessageTask::Message_RaceListTask() {
    const SaveRecord& r = Save_Current();
    int count;
    AdventureRace_Town(town_, &firstRace_, &count);
    window_ = Window();
    for (int i = 0; i < count; ++i) {
        const int n = firstRace_ + i;
        const AdventureRace& race = AdventureRace_Get(n);
        // Indexed past the player's 26 bytes for races 26 and 27, as the original does.
        const uint8_t place = (&r.adventurePlace[0][0])[n];
        window_.AddLine(place < 6 ? "*" : " ", kLineSelectable);
        window_.AddLine(Race_CourseName(race.course), kLineSelectable | kLineSameLine);
        window_.AddLine(AdventureRace_RankName(race.rank),
                        kLineSelectable | kLineSameLine | kLineRight | (r.license < race.rank ? kLineDisabled : 0));
    }
    window_.AddLine(GT_002EC5D0_21, kLineSelectable);
    Message_OpenWindow(kChildRaceList);
}

// Cross on a race: a rank above the license says so in a notice (func_0023BB00: a centred one-line window, SE 0x1E,
// gone at the next press) and the list waits under it; so does race 25 (Tin Raceway) until stamp 100 (being the
// president): "Under construction.". Otherwise SE 0x1A, info+0x20 = the race (25 becomes 27 outside the clock's
// 54000-162000) and the message goes to the code's first target. Returns that message, or -1 while the list stays.
int MessageTask::Message_RaceListPick() {
    const unsigned char* a = (const unsigned char*)msg_.c_str() + end_ + 1;
    const int n = firstRace_ + window_.sel;
    const AdventureRace& race = AdventureRace_Get(n);
    const char* notice = nullptr;
    if (race.rank < 4 && Save_Current().license < race.rank)
        notice = race.rank == 1 ? GT_002EC5E8_22 : race.rank == 2 ? GT_002EC600_22
                                                                            : GT_002EC618_28;
    else if (n == 0x19 && !StampEarned(100))
        notice = GT_002EC638_19;
    if (notice) {
        window_.state = Window::kOpen | Window::kInactive;
        notice_ = Window();
        notice_.AddLine(notice, 0);
        noticeOpen_ = true;
        audio::Sound_PlaySe(0x1E);
        return -1;
    }
    audio::Sound_PlaySe(0x1A);
    race_ = n;
    if (n == 0x19 && (unsigned)(clock_ - 54000) > 108000) race_ = 0x1B;
    return a[0];
}

// Message_TeammateTask (func_00239748), end code 0x10 (a, q): with room on the team (Team_State: the first empty
// slot) the person joins at once (func_00239378). A full team gets a menu at the top left: teammate 1's and 2's
// names and "Quit" (Triangle: Quit).
void MessageTask::Message_TeamTask() {
    int slot = Team_State();
    if (slot) {
        Message_TeamJoin(slot);
        return;
    }
    const SaveRecord& r = Save_Current();
    window_ = Window();
    for (int t = 0; t < 2; ++t) {
        const char* name = Car_DriverRecordName(r.teammates[t][0], r.teammates[t][1]);
        lines_[t] = name ? name : "";
        window_.AddLine(lines_[t].c_str(), kLineSelectable);
    }
    window_.AddLine(GT_002FF078_4, kLineSelectable);
    Message_OpenWindow(kChildTeamMenu);
}

// func_00239378: the person here (the driver record of this town and place) joins as teammate `slot`
// (func_0023DA58: the car takes their paint and body, the speed tables start over; the car's parts 1-7 go into the
// items). Then a centred notice, open at once: "X is now in your team.", the song muted and SE 0x303.
void MessageTask::Message_TeamJoin(int slot) {
    const CarRecord* rec = Car_DriverRecord(town_, place_);
    if (rec) Team_Join(slot, town_, place_, rec->paint, rec->body);
    teamChanged_ = true;
    window_ = Window();
    window_.style = 2;
    lines_[0] = rec && rec->name ? rec->name : "";
    window_.AddLine(lines_[0].c_str(), 0);
    window_.AddLine(GT_002EC580_21, kLineSameLine);
    windowOpen_ = true;
    windowClosing_ = false;
    child_ = kChildTeamJoined;
    state_ = kChild;
    audio::Sound_MuteSong();
    audio::Sound_PlaySe(0x303);
}

// The children with frames of their own. Returns false for the window children, which Message_UpdateChild runs.
// Each ends by going to its next message (Message_Go) in the box, which is drawn under it but for the photo's.
bool MessageTask::Message_UpdateTask(const platform::Input& input) {
    const unsigned char* a = (const unsigned char*)msg_.c_str() + end_ + 1;
    const int age = childAge_++;
    auto next = [&](int n) {
        state_ = kRun;
        age_ = 0;
        Message_Go(n);
    };
    switch (child_) {
    case kChildNumber: {
        // func_0023A320 (a, y, x): a number 0-99, starting at 1, in a small style-2 frame at the top left with the
        // up and down arrows blinking beside it; Up takes one off, Down adds one (repeating, stopping at the ends,
        // silent). Cross only: SE 0x1A and message y if it's a, else x.
        Message_DrawBox(Alpha(0x80));
        Message_DrawText(Alpha(0x80), true);
        if (age == 0) return true;
        Window_DrawFrame(d_, 0x40, 0x20, 0x3C, 0x0C, 2);
        if (age & 0x10) {
            Icon_Draw(d_, 0x58, 0x28, 2);
            Icon_Draw(d_, 0x58, 0x58, 3);
        }
        uint32_t rep = input.Repeated(0);
        if ((rep & platform::kUp) && value_ > 0) --value_;
        if ((rep & platform::kDown) && value_ < 99) ++value_;
        Number_PrintRight(d_, 0x70, 0x40, value_, 10);
        if (input.Pressed(0) & platform::kCross) {
            audio::Sound_PlaySe(0x1A);
            next(value_ == a[0] ? a[1] : a[2]);
        }
        return true;
    }
    case kChildHorn:
        // func_0023BE88: the horn (started with the child) sounds; 64 frames on, message n.
        Message_DrawBox(Alpha(0x80));
        Message_DrawText(Alpha(0x80), true);
        if (age + 1 >= 0x40) next(a[1]);
        return true;
    case kChildShop:
        return true;
    case kChildPhoto:
        // Photo_ViewTask in its take mode: the photo just taken (SYS/PUTI.BIN's picture, twice its size), the box
        // hidden, until Cross or Triangle (no SE). The scene draws the car over it (PhotoShown).
        if (age > 0 && (input.Pressed(0) & (platform::kCross | platform::kTriangle))) {
            next(a[1]);
            Message_DrawBox(Alpha(0x80));   // the box at once, as it starts typing
            return true;
        }
        if (age > 0) d_.Sprite(d_.Get("SYS/PUTI.BIN", value_), 64, 40, 512, 368, 0, 0, 256, 184);
        return true;
    case kChildGreeting:
        // func_0023BE18: NameEntry mode 2, the greeting (save +0x646), over the box; then message n.
        Message_DrawBox(Alpha(0x80));
        Message_DrawText(Alpha(0x80), true);
        if (greeting_.Update(input)) next(a[0]);
        return true;
    case kChildMileage: {
        // func_0023A140's window (cells 8-22, rows 6-10, style 1): "Earned <pay> <unit>" / "by driving <miles>
        // miles.", the digits right-aligned before the words. Cross only: SE 0x1A, message n, the song back in.
        Message_DrawBox(Alpha(0x80));
        Message_DrawText(Alpha(0x80), true);
        if (age == 0) return true;
        const SaveRecord& r = Save_Current();
        Window_DrawCellFrame(d_, 8, 6, 0x16, 0x0A, 1);
        Text_Print(d_, 9, 7, GT_00333908_7, 0);
        Text_Print(d_, 0x10, 7, r.currency, 0);
        Number_PrintRight(d_, 0x14A, 0x59 * 2, pay_, 0);
        Text_Print(d_, 9, 8, GT_002EC6B0_11, 0);
        Number_PrintRight(d_, 0x172, 0x65 * 2, miles_, 0);
        Text_Print(d_, 0x12, 8, GT_00333910_6, 0);
        if (input.Pressed(0) & platform::kCross) {
            audio::Sound_PlaySe(0x1A);
            audio::Sound_FadeInSong();
            next(a[1]);
        }
        return true;
    }
    case kChildBank: {
        // func_0023A460 (w, z, ok, short, x): an amount in thousands (1000-999000; Left and Right pick the digit
        // of the thousands, ten or hundred thousands, SE 0x1D; Up and Down change it, wrapping 9 to 0 only on a
        // fresh press), in cells 10-19, rows 7-9; below the scene's money box, the money it would leave
        // (func_0022E8C0) with a bobbing arrow. Cross deposits (w 0) or withdraws it: too much for the wallet (x),
        // too little cash or savings (short), done (ok; z for 0). Triangle: z, SE 0x1B.
        Message_DrawBox(Alpha(0x80));
        Message_DrawText(Alpha(0x80), true);
        if (age == 0) return true;
        SaveRecord& r = Save_Current();
        Window_DrawCellFrame(d_, 0x0A, 7, 0x13, 9, 2);
        uint32_t rep = input.Repeated(0), press = input.Pressed(0);
        if ((rep & platform::kLeft) && digit_ < 5) {
            audio::Sound_PlaySe(0x1D);
            ++digit_;
        }
        if ((rep & platform::kRight) && digit_ >= 4) {
            audio::Sound_PlaySe(0x1D);
            --digit_;
        }
        if (age & 0x10) {
            Icon_Draw(d_, 0x138 - digit_ * 16, 0xB2, 2);
            Icon_Draw(d_, 0x138 - digit_ * 16, 0xE2, 3);
        }
        static const int kPow[6] = {1, 10, 100, 1000, 10000, 100000};
        const int step = kPow[digit_];
        const int d = value_ / step % 10;
        if (rep & platform::kUp) {
            if (d < 9) {
                audio::Sound_PlaySe(0x1D);
                value_ += step;
            } else if (press & platform::kUp) {
                audio::Sound_PlaySe(0x1D);
                value_ -= step * 9;
            }
        } else if (rep & platform::kDown) {
            if (d != 0) {
                audio::Sound_PlaySe(0x1D);
                value_ -= step;
            } else if (press & platform::kDown) {
                audio::Sound_PlaySe(0x1D);
                value_ += step * 9;
            }
        }
        Number_PrintRight(d_, 0x14A, 0x65 * 2, value_, 10);
        Text_Print(d_, 0x10, 8, r.currency, 10);
        const bool withdraw = a[0] != 0;
        int after = r.money - (withdraw ? -value_ : value_), over = after < 0 ? -1 : after > 999999 ? 1 : 0;
        Money_Draw(d_, 0x1B8, 0x4A, over > 0 ? 999999 : after, r.currency);
        Icon_Draw(d_, 0x204, ((age >> 1) & 7) + 0x39, 7);
        if (press & platform::kCross) {
            int n;
            if (withdraw) {
                if (over > 0) {
                    audio::Sound_PlaySe(0x1A);
                    n = a[4];
                } else if (r.bank < value_) {
                    audio::Sound_PlaySe(0x1A);
                    n = a[3];
                } else {
                    n = value_ == 0 ? a[1] : a[2];
                    Money_Pay(-value_);
                    r.bank -= value_;
                }
            } else if (over < 0) {
                audio::Sound_PlaySe(0x1A);
                n = a[3];
            } else {
                n = value_ == 0 ? a[1] : a[2];
                Money_Pay(value_);
                r.bank += value_;
            }
            next(n);
        } else if (press & platform::kTriangle) {
            audio::Sound_PlaySe(0x1B);
            next(a[1]);
        }
        return true;
    }
    default:
        return false;
    }
}

// The windows' frames. The choice and Yes/No windows go to the item's message (Triangle: the last item). The car pick
// hands 9 | car << 8 to the scene and closes the box (Quit or Triangle: the code's message). The race list goes to
// the code's first message with a race, the second on Quit or Triangle. The save point hides the box and its text
// until it's over, then goes to the code's message.
int MessageTask::Message_UpdateChild(const platform::Input& input) {
    const unsigned char* a = (const unsigned char*)msg_.c_str() + end_ + 1;
    if (child_ == kChildSave) {
        if (save_.Update(input) < 0) return 0;
        saveClosing_ = save_.Closing();
        state_ = kRun;
        age_ = 0;
        Message_Go(a[0]);
        return 0;
    }
    if (Message_UpdateTask(input)) return 0;
    Message_DrawBox(Alpha(0x80));
    Message_DrawText(Alpha(0x80), true);
    int r = window_.Update(d_, input);
    if (noticeOpen_) {
        if (notice_.Update(d_, input) != 0) {
            noticeOpen_ = false;
            window_.state = (unsigned char)(window_.state & ~Window::kInactive);
        }
        return 0;
    }
    if (r != 1 && r != -1) return 0;
    int next = 0, result = 0;
    bool fade = true;   // the window fades out (the notices of 0x07 and 0x10 go at once)
    switch (child_) {
    case kChildTeamMenu: {
        // A teammate: func_00239508's question, centred and open at once. Quit (or Triangle): message q, the
        // window fading out.
        audio::Sound_PlaySe(0x1A);
        int sel = r == 1 ? window_.sel : 2;
        if (sel == 2) {
            next = a[1];
            break;
        }
        teamSlot_ = sel + 1;
        const SaveRecord& s = Save_Current();
        const char* name = Car_DriverRecordName(s.teammates[sel][0], s.teammates[sel][1]);
        lines_[0] = name ? name : "";
        window_ = Window();
        window_.style = 2;
        window_.AddLine(GT_002EC598_22, 0);
        window_.AddLine(lines_[0].c_str(), kLineSameLine);
        window_.AddLine(GT_002EC5B0_15, 0);
        window_.AddLine("Yes", kLineSelectable);
        window_.AddLine("No", kLineSelectable);
        child_ = kChildTeamLeave;
        return 0;
    }
    case kChildTeamLeave:
        if (r == 1 && window_.sel == 0) {
            // Yes: the teammate leaves (their parts 1-7 out of the items, the slot empty) and message a, whose
            // 0x10 then finds room and lets the person join.
            Team_Leave(teamSlot_);
            teamChanged_ = true;
            audio::Sound_PlaySe(0x1A);
            next = a[0];
            fade = false;
            break;
        }
        // No (or Triangle): the menu again, a new window.
        audio::Sound_PlaySe(0x1B);
        Message_TeamTask();
        return 0;
    case kChildTeamJoined:
    case kChildGetItem:
        // Any press: SE 0x1A, the window gone at once, the next message, and the song fades back in.
        audio::Sound_PlaySe(0x1A);
        audio::Sound_FadeInSong();
        next = child_ == kChildTeamJoined ? a[0] : a[2];
        fade = false;
        break;
    case kChildPickCar:
        audio::Sound_PlaySe(0x1A);
        if (r == 1 && window_.sel != window_.items - 1) {
            result = pickType_ | window_.sel << 8;
            next = -1;
        } else {
            next = a[0];
        }
        break;
    case kChildRaceList:
        if (r == 1 && window_.sel != window_.items - 1) {
            next = Message_RaceListPick();
            if (next < 0) return 0;
        } else {
            audio::Sound_PlaySe(0x1A);
            next = a[1];
        }
        break;
    default: {
        // Triangle takes the last item (No).
        int item = r == 1 ? window_.sel : items_ - 1;
        audio::Sound_PlaySe(0x1A);
        next = item >= 0 && item < items_ ? targets_[item] : 0;
        break;
    }
    }
    window_.state = Window::kClosing;
    windowClosing_ = fade;
    if (next < 0) {
        // The box closes (SE 0x26) with nothing more for the scene.
        audio::Sound_PlaySe(0x26);
        result_ = -1;
        state_ = kClose;
        age_ = -1;
        return result;
    }
    state_ = kRun;
    age_ = 0;
    Message_Go(next);
    return result;
}

// The end code once the text before it is typed (Message_Task's state 1):
// - at once: a window or task, the box staying under it (state 4): the car pick 4 (parts) and 6 (paint), the race
//   list 8, the choices 9, 1 and 2, the number 5, the horn 0x0B, the greeting 0x12, the team 0x10, the bank 0x17;
//   the save point 0x0E hides the box;
// - after the arrow and a press (SE 0x1A): the next message in the same box (0x0C), an item (0x07), a photo (0x11,
//   the box hidden), a shop (0x13), a part fitted (0x15: SE 0x303, the next message typed in the same box) and the
//   mileage's pay (0x16);
// - after a press without the arrow, the box closing (SE 0x26): a plain end (0), a race (3), quitting (0x0F), going
//   somewhere (0x14), the ending (0x18) and the stamps (0x0D, queued for the book).
void MessageTask::Message_EndCode(const platform::Input& input) {
    unsigned char code = (unsigned char)msg_[end_];
    const unsigned char* a = (const unsigned char*)msg_.c_str() + end_ + 1;
    size_t left = msg_.size() - end_ - 1;
    // The codes' arguments. A last argument of 0 is the string's own end (e.g. 0x10's q when there's room on the
    // team); a message shorter than that is broken, and goes nowhere.
    static const unsigned char kArgCount[0x19] = {0, 2, 2, 1, 1, 3, 1, 3, 2, 0, 0, 2, 1, 0, 1, 0,
                                                  2, 2, 1, 1, 2, 3, 2, 5, 0};
    if (code < 0x19 && left + 1 < kArgCount[code]) {
        Message_Go(0);
        return;
    }
    auto child = [&](Child c) {
        child_ = c;
        childAge_ = 0;
        state_ = kChild;
    };
    switch (code) {
    case 0x04:
        Message_PickCarTask(9);
        return;
    case 0x06:
        // The paint shop's car pick: the scene gets 8 | car << 8.
        Message_PickCarTask(8);
        return;
    case 0x08:
        Message_RaceListTask();
        return;
    case 0x0E:
        // D_01780A3C (the save's +0x12DC) = the town, then Save_AdventureSaveTask; the box hides meanwhile.
        Save_Current().saveTown = (int8_t)town_;
        save_.Start();
        child_ = kChildSave;
        state_ = kChild;
        return;
    case 0x09:
        Message_OpenChoice(false, false);
        return;
    case 0x01:
    case 0x02:
        Message_OpenChoice(true, code == 0x02);
        return;
    case 0x05:
        value_ = 1;
        child(kChildNumber);
        return;
    case 0x0B: {
        // The horn (D_002F00AC: SE 0x3C + 2 x horn; 0x10 the player's own, cars[0].parts[13]), by func_002083C0.
        int horn = a[0] == 0x10 ? Save_Current().cars[0].parts[13] : a[0];
        audio::Sound_PlaySe(0x3C + 2 * horn);
        child(kChildHorn);
        return;
    }
    case 0x10:
        Message_TeamTask();
        return;
    case 0x12:
        greeting_.Start(NameEntry::kGreeting);
        child(kChildGreeting);
        return;
    case 0x17:
        value_ = 1000;
        digit_ = 3;
        child(kChildBank);
        return;
    case 0x07:
    case 0x0C:
    case 0x11:
    case 0x13:
    case 0x15:
    case 0x16:
        if (age_ & 0x10) d_.Sprite(d_.Get(kFont, 0, 0), 0x136, 0x1A4, 20, 20, 7 * 20 + 120, 0xB4, 20, 20);
        if (!Pressed(input)) return;
        audio::Sound_PlaySe(0x1A);
        break;
    default:
        if (!Pressed(input)) return;
        break;
    }
    switch (code) {
    case 0x0C:
        Message_Go(a[0]);
        return;
    case 0x07: {
        // func_00239F68: the item given (Item_Give), and a centred notice open at once (style 0): Get Body "name",
        // Get "name Tires" ... by the kind; the song muted, the fanfare SE 0x302.
        static const char* const kSuffix[16] = {"\"", GT_003338F8_7, GT_002EC650_8, GT_002EC660_9, GT_00333900_6,
                                                GT_002EC670_10, "\"", GT_002EC680_8, "\"", "\"", "\"", "\"",
                                                "\"", "\"", "\"", "\""};
        const int kind = a[0] & 15, n = a[1];
        Item_Give(kind, n);
        lines_[0] = std::string(Part_Name(kind, n)) + kSuffix[kind];
        window_ = Window();
        window_.AddLine(GT_003338E0_4, 0);
        window_.AddLine(kind == 0 ? GT_003338E8_6 : "\"", kLineSameLine);
        window_.AddLine(lines_[0].c_str(), kLineSameLine);
        windowOpen_ = true;
        windowClosing_ = false;
        child(kChildGetItem);
        audio::Sound_MuteSong();
        audio::Sound_PlaySe(0x302);
        return;
    }
    case 0x11:
        // Photo_Take, then the photo shown (Photo_ViewTask's take mode), the box hidden; the shutter's SE 0x306.
        Photo_Take(a[0]);
        value_ = a[0] >= 1 ? a[0] - 1 : 0;
        audio::Sound_PlaySe(0x306);
        child(kChildPhoto);
        return;
    case 0x13:
        // func_00269E88, the shop's screen by the place (Room_SceneKind: the Parts Shop, the Body Shop), which the
        // scene runs (it has the room picture the cards are drawn from); message n once it's left. (The box is
        // hidden meanwhile: Message_Task's state 6.)
        child(kChildShop);
        shopPending_ = true;
        return;
    case 0x15:
        // Part_Fit on the player's car, SE 0x303, and message n in the same box.
        Part_Fit(Save_Current().cars[0], a[0], a[1]);
        carChanged_ = true;
        audio::Sound_PlaySe(0x303);
        Message_Go(a[2]);
        return;
    case 0x16: {
        // func_0023A140's first frame: sponsor k's miles (whole ones; the rest of the metres stays) paid at
        // 10 (k + 1) a mile; none: on to message n at once.
        SaveRecord& r = Save_Current();
        int k = a[0] < 5 ? a[0] : 0;
        miles_ = r.sponsorDistance[k] / 1609;
        r.sponsorDistance[k] -= miles_ * 1609;
        if (miles_ == 0) {
            Message_Go(a[1]);
            return;
        }
        pay_ = miles_ * (k * 10 + 10);
        audio::Sound_MuteSong();
        Money_Pay(-pay_);
        child(kChildMileage);
        return;
    }
    case 0x03:
        result_ = 4;
        if (a[0]) race_ = a[0];
        break;
    case 0x0D:
        // The stamps after the code, up to a 0, queued for the book (Stamp_Queue); the box closes as a plain end.
        for (const unsigned char* p = a; p < (const unsigned char*)msg_.c_str() + msg_.size() && *p; ++p)
            Stamp_Queue(*p);
        result_ = 5;
        break;
    case 0x0F: result_ = 6; break;
    case 0x14:
        // On to (t, p): this place found first (Place_MarkFound at 0x0023B5E4), then result 11.
        Place_MarkFound(town_, place_);
        result_ = 11;
        goTown_ = a[0];
        goPlace_ = a[1];
        break;
    case 0x18: result_ = 15; break;
    default: result_ = 5; break;
    }
    audio::Sound_PlaySe(0x26);
    state_ = kClose;
    age_ = -1;   // 0 on the first closing frame
}

int MessageTask::Update(const platform::Input& input) {
    if (state_ == kDone || !table_) return 0;
    const char* name = table_->empty() ? "" : (*table_)[0].c_str();
    const int nameWidth = Text_Width(name);
    int result = 0;
    switch (state_) {
    case kOpen: {
        // 16 frames: the box fades in and the name tab grows; no text yet.
        uint32_t rgba = Alpha(age_ * 8);
        Message_DrawBox(rgba);
        Message_DrawNameTab(nameWidth * age_ / 16, rgba);
        if (++age_ >= 16) {
            state_ = kRun;
            age_ = 0;
        }
        return 0;
    }
    case kClose: {
        // 16 frames: the box and the tab fade out, without the text.
        Message_DrawBox(Alpha(0x80 - age_ * 8));
        if (age_ + 1 >= 16) {
            state_ = kDone;
            result = result_ < 0 ? 0 : result_ ? result_ : 5;
        }
        break;
    }
    case kScroll:
        // Two a frame: a line in 6 frames.
        scroll_ += 2;
        Message_DrawBox(Alpha(0x80));
        Message_DrawText(Alpha(0x80), true);
        if (scroll_ % kLineScroll == 0) state_ = kRun;
        break;
    case kChild:
        result = Message_UpdateChild(input);
        break;
    default: {   // kRun
        Message_DrawBox(Alpha(0x80));
        TextStop stop = Message_DrawText(Alpha(0x80), true);
        if (stop == kTyping) {
            // One character every 2 frames, the blip every 4; Cross or Triangle shows the rest of the page.
            if (!(age_ & 1)) ++chars_;
            if (!(age_ & 3)) {
                audio::Sound_PlaySe(0x23);
                blip = 2;
            }
            if (Pressed(input)) chars_ = kFullPage;
        } else if (stop == kPageFull) {
            if ((scroll_ / kLineScroll) % 3 == 0) {
                if (age_ & 0x10) d_.Sprite(d_.Get(kFont, 0, 0), 0x136, 0x1A4, 20, 20, 7 * 20 + 120, 0xB4, 20, 20);
                if (Pressed(input)) {
                    audio::Sound_PlaySe(0x1A);
                    state_ = kScroll;
                }
            } else {
                state_ = kScroll;
            }
        } else {
            Message_EndCode(input);
        }
        break;
    }
    }
    if (shopPending_) {
        shopPending_ = false;
        result = 0x13;
    }
    const bool hidden = state_ == kChild && (child_ == kChildSave || child_ == kChildPhoto || child_ == kChildShop);
    if (state_ != kOpen && state_ != kDone && !hidden) {
        Message_DrawNameTab(nameWidth, Alpha(state_ == kClose ? 0x80 - age_ * 8 : 0x80));
        if (state_ != kClose) Text_PrintAt(d_, kNameRight - nameWidth, 0xA0 * 2, name, 0);
    }
    // A chosen window closes over its 16 frames while the messages go on.
    if (windowClosing_ && state_ != kChild && window_.Update(d_, input) == -2) windowClosing_ = false;
    if (saveClosing_ && state_ != kChild) {
        save_.Update(input);
        saveClosing_ = save_.Closing();
    }
    ++age_;
    return result;
}

}  // namespace game
