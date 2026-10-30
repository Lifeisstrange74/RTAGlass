#include "mail_screen.h"

#include <stdio.h>
#include <string.h>

#include "../../audio/sound.h"
#include "../save/save_data.h"
#include "../text.h"
#include "messages.h"
#include "../../game_text.inc"   // the game's text, GT_<address>_<length>, made from the ELF

namespace game {

namespace {

const int kRows = 13;          // the list's rows shown
const int kLines = 11;         // a mail's lines drawn
const int kFullHalfWidth = 0xF0, kFullHeight = 0x90, kGrow = 16;
// The font's kanji cells as envelopes, palette 13: 既 (read) an open one, 未 (unread) a closed one.
const char kRead[] = "\x8A\xF9", kUnread[] = "\x96\xA2";

// Window_DrawFrame with y and height in half-lines, as the original takes them.
void Frame(gfx::Draw2D& d, int x, int y, int w, int h) { Window_DrawFrame(d, x, y, w, h, 0); }

}  // namespace

void MailScreen::Start() {
    static bool loaded = false;
    if (!loaded) loaded = Mail_Load(dataDir_);
    // The first frame: every mail's item ("   " + its subject), and the box's newest first; a 13-row list, then its
    // place forced to (0x64, 0x30), 0x1B8 x 0x9C, frame style 0, fading in.
    items_.assign(128, std::string());
    for (int m = 0; m < 128; ++m) {
        const MessageTable* t = Mail_Text(m);
        items_[m] = std::string("   ") + (t && !t->empty() ? (*t)[0] : std::string());
    }
    const SaveRecord& r = Save_Current();
    int count = r.mailCount < 0 ? 0 : r.mailCount > 128 ? 128 : r.mailCount;
    list_.assign(count, nullptr);
    for (int i = 0; i < count; ++i) list_[count - 1 - i] = items_[r.mail[i] & 0x7F].c_str();
    win_ = Window();
    win_.SetList(list_.data(), count, kRows);
    win_.style = 0;
    win_.x = 0x64;
    win_.y = 0x30;
    win_.w = 0x1B8;
    win_.h = 0x9C;
    win_.state = Window::kOpening;
    state_ = kList;
    halfWidth_ = height_ = 0;
}

int MailScreen::Mail_Selected() const { return (int)list_.size() - 1 - win_.sel; }

void MailScreen::Mail_DrawList() {
    // The header strip, "mail box" and the unread and total counts right-aligned by strlen x 10.
    const SaveRecord& r = Save_Current();
    int count = (int)list_.size(), unread = 0;
    for (int i = 0; i < count; ++i)
        if (!(r.mail[i] & 0x80)) ++unread;
    Frame(d_, 0x64, 0x18, 0x1B8, 0x0C);
    Text_PrintAt(d_, 0x8C, 0x32, GT_0032E230_8, 0);
    char buf[32];
    snprintf(buf, sizeof buf, GT_0032E240_12, unread, count);
    Text_PrintAt(d_, 0x1E0 - (int)strlen(buf) * 10, 0x32, buf, 0);
}

void MailScreen::Mail_DrawReading() const {
    const SaveRecord& r = Save_Current();
    const MessageTable* t = Mail_Text(r.mail[Mail_Selected()] & 0x7F);
    Frame(d_, 0x50, 0x18, 0x1E0, 0x18);
    Text_PrintAt(d_, 0x78, 0x32, GT_003352F0_7, 0);
    Text_PrintAt(d_, 0x78, 0x4A, GT_003352F8_4, 0);
    Frame(d_, 0x50, 0x3C, 0x1E0, 0x90);
    if (!t) return;
    for (int k = 0; k < 2 && k < (int)t->size(); ++k)
        Text_PrintAt(d_, 0x208 - Text_Width((*t)[k].c_str()), k ? 0x4A : 0x32, (*t)[k].c_str(), 0);
    // The lines, the first $N of each the player's car's name (Save_GetCarName).
    for (int i = 0; i < kLines && 2 + i < (int)t->size(); ++i) {
        std::string line = (*t)[2 + i];
        size_t at = line.find("$N");
        if (at != std::string::npos) line.replace(at, 2, r.name);
        Text_PrintAt(d_, 0x78, 0x7A + 24 * i, line.c_str(), 0);
    }
}

bool MailScreen::Update(const platform::Input& input) {
    const uint32_t pressed = input.Repeated(0);
    switch (state_) {
    case kList: {
        Mail_DrawList();
        // Window_UpdateList: the frame, bar, cursor and items, scrolling a row at a time (what it returns isn't
        // used); then an envelope by each row shown.
        win_.Update(d_, input);
        const SaveRecord& r = Save_Current();
        int count = (int)list_.size();
        for (int row = 0; row < kRows; ++row) {
            int k = count - row - win_.Top() - 1;
            if (k < 0 || k >= count) continue;
            Text_PrintAt(d_, 0x78, 0x62 + 24 * row, (r.mail[k] & 0x80) ? kRead : kUnread, 13);
        }
        if ((pressed & platform::kCross) && count) {
            // Opened: SE 0x1A, marked read at once (bit 7), the frame grows.
            audio::Sound_PlaySe(0x1A);
            Save_Current().mail[Mail_Selected()] |= 0x80;
            state_ = kOpening;
            halfWidth_ = height_ = 0;
        }
        if (pressed & platform::kTriangle) return true;   // no SE
        break;
    }
    case kOpening:
    case kClosing: {
        int step = state_ == kOpening ? kGrow : -kGrow;
        halfWidth_ += step;
        height_ += step;
        halfWidth_ = halfWidth_ < 0 ? 0 : halfWidth_ > kFullHalfWidth ? kFullHalfWidth : halfWidth_;
        height_ = height_ < 0 ? 0 : height_ > kFullHeight ? kFullHeight : height_;
        if (halfWidth_ && height_) Frame(d_, 0x140 - halfWidth_, (0x108 - height_) / 2, 2 * halfWidth_, height_);
        if (state_ == kOpening && halfWidth_ == kFullHalfWidth && height_ == kFullHeight) state_ = kReading;
        if (state_ == kClosing && !halfWidth_ && !height_) state_ = kList;
        break;
    }
    case kReading:
        Mail_DrawReading();
        if (pressed & (platform::kCross | platform::kTriangle)) {
            audio::Sound_PlaySe(0x1B);
            state_ = kClosing;
        }
        break;
    }
    return false;
}

}  // namespace game
