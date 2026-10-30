#include "name_entry.h"

#include <string.h>

#include "../../audio/sound.h"
#include "../save/save_data.h"
#include "../text.h"
#include "../ui/window.h"
#include "../../game_text.inc"   // the game's text, GT_<address>_<length>, made from the ELF

namespace game {

namespace {

// The modes (D_002C40D0): prompt and length.
struct ModeInfo {
    const char* prompt;
    int max;
};
const ModeInfo kModes[3] = {{GT_003290F8_16, 9}, {GT_003290E0_20, 4}, {GT_003290D0_15, 5}};

// The keyboard (D_002C3B10, 16 bytes a key): label, moves, position. Keys 0-89 are single characters, four rows
// of 26 (the last row has 12); Cancel and Apply sit in column 28 of the first two rows.
const int kKeyCount = 92, kCancel = 90, kApply = 91;
const char kChars[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789!\"#$%&'()*+,-.:;<=>?@[\\]_{|}";

// A move with this bit only happens on a press, not on the pad's repeat: it wraps around.
const int kWrap = 0x8000;

struct Key {
    char label[8];
    int x, y;
    int up, down, left, right;
};

// The moves the table spells out; every other key moves in its grid and wraps at the edges.
struct NavOverride {
    int key, up, down, left, right;
};
const NavOverride kNav[] = {
    {0, 78 | kWrap, 26, 90 | kWrap, 1},
    {25, 77 | kWrap, 51, 24, 90},
    {26, 0, 52, 91 | kWrap, 27},
    {51, 25, 77, 50, 91},
    {77, 51, 25 | kWrap, 76, 52 | kWrap},
    {78, 52, 0 | kWrap, 89 | kWrap, 79},
    {89, 63, 11 | kWrap, 88, 78 | kWrap},
    {90, 91 | kWrap, 91, 25, 0 | kWrap},
    {91, 90, 90 | kWrap, 51, 26 | kWrap},
};

struct Keyboard {
    Key keys[kKeyCount];
    Keyboard() {
        for (int i = 0; i < kKeyCount; ++i) {
            Key& k = keys[i];
            if (i < kCancel) {
                k.label[0] = kChars[i];
                k.label[1] = 0;
            } else {
                strcpy_s(k.label, i == kCancel ? GT_00334BA8_6 : GT_00333508_5);
            }
            k.x = i < kCancel ? i % 26 : 28;
            k.y = i < kCancel ? i / 26 : i - kCancel;
            // The default moves (func_0025B908 when a key's entry is empty).
            int col = i % 26, row = i / 26;
            k.up = row == 0 ? (i + 78) | kWrap : i - 26;
            k.down = row == 3 ? col | kWrap : i + 26;
            k.left = col == 0 ? (i + 25) | kWrap : i - 1;
            k.right = col == 25 ? (i - 25) | kWrap : i + 1;
        }
        // M-Y and #-; have their own up and down moves (between the letters and the symbol rows).
        for (int i = 12; i <= 24; ++i) keys[i].up = (i + 52) | kWrap;
        for (int i = 64; i <= 76; ++i) keys[i].down = (i - 52) | kWrap;
        for (const NavOverride& n : kNav) {
            keys[n.key].up = n.up;
            keys[n.key].down = n.down;
            keys[n.key].left = n.left;
            keys[n.key].right = n.right;
        }
    }
};

const Keyboard& Keys() {
    static Keyboard k;
    return k;
}

}  // namespace

NameEntry::NameEntry(gfx::Draw2D& draw) : d_(draw) { Start(kName); }

void NameEntry::Start(Mode mode) {
    mode_ = mode;
    len_ = 0;
    cursor_ = 0;
    age_ = 0;
    closing_ = false;
    text_.clear();
}

bool NameEntry::Update(const platform::Input& input) {
    bool done = false;
    uint32_t alpha = 0x80;
    if (!closing_) {
        if (age_ < 16) {
            alpha = age_ * 8;
        } else {
            NameEntry_MoveCursor(input);
            if (NameEntry_Input(input)) {
                text_.clear();
                for (int i = 0; i < len_; ++i) text_ += Keys().keys[keys_[i]].label;
                // Into player 1's record: the name (+0x62C), the currency unit (+0x63F) or the greeting (+0x646).
                SaveRecord& r = Save_Record(0);
                char* field = mode_ == kName ? r.name : mode_ == kCurrency ? r.currency : r.greeting;
                size_t size = mode_ == kName ? sizeof(r.name) : mode_ == kCurrency ? sizeof(r.currency)
                                                                                    : sizeof(r.greeting);
                memset(field, 0, size);
                memcpy(field, text_.c_str(), text_.size() < size ? text_.size() : size - 1);
                closing_ = true;
                age_ = 0;
            }
        }
    } else {
        // func_0025BE30: fades out over 16 frames, without the hints.
        alpha = 0x80 - age_ * 8;
        done = age_ == 16;
    }
    uint32_t rgba = alpha << 24 | 0x808080;
    NameEntry_DrawKeyboard(rgba);
    NameEntry_DrawPrompt(rgba);
    if (!closing_) Help_Draw(d_, 1, rgba);
    ++age_;
    return done;
}

// A press always takes a key's move; the pad's repeat only takes moves that don't wrap. Plays 0x1D on a move.
void NameEntry::NameEntry_MoveCursor(const platform::Input& input) {
    const Key& k = Keys().keys[cursor_];
    uint32_t pressed = input.Pressed(), held = input.Repeated() & ~pressed;
    int to = -1;
    const platform::Button dirs[4] = {platform::kUp, platform::kDown, platform::kLeft, platform::kRight};
    const int moves[4] = {k.up, k.down, k.left, k.right};
    for (int i = 0; i < 4 && to < 0; ++i) {
        if ((pressed & dirs[i]) || ((held & dirs[i]) && !(moves[i] & kWrap))) to = moves[i] & 0x7FFF;
    }
    if (to >= 0 && to != cursor_) {
        cursor_ = to;
        audio::Sound_PlaySe(0x1D);
    }
}

// Triangle deletes the last character (0x1B), or buzzes (0x1C) when there is none. Cross types the key (0x22) and
// jumps to Apply when the text is full; on Cancel it deletes, on Apply it finishes if there is any text (0x1A).
// On PC, Start (Enter) also finishes, as if on Apply; the original ignores Start here.
bool NameEntry::NameEntry_Input(const platform::Input& input) {
    uint32_t p = input.Pressed();
    bool erase = (p & platform::kTriangle) != 0, finish = !erase && (p & platform::kStart) != 0;
    if (!erase && !finish && !(p & platform::kCross)) return false;
    if (finish || (!erase && cursor_ == kApply)) {
        audio::Sound_PlaySe(len_ > 0 ? 0x1A : 0x1C);
        return len_ > 0;
    }
    if (erase || cursor_ == kCancel) {
        if (len_ > 0) {
            audio::Sound_PlaySe(0x1B);
            --len_;
        } else {
            audio::Sound_PlaySe(0x1C);
        }
        return false;
    }
    if (len_ < kModes[mode_].max) {
        audio::Sound_PlaySe(0x22);
        keys_[len_++] = cursor_;
        if (len_ == kModes[mode_].max) cursor_ = kApply;
    } else {
        audio::Sound_PlaySe(0x1C);
    }
    return false;
}

// The keyboard: a style-2 window across the lower part (higher up for the greeting), keys 18 pixels apart and
// centred on their column, in palette 10, with the highlight bar on the current key.
void NameEntry::NameEntry_DrawKeyboard(uint32_t rgba) {
    int base = mode_ == kGreeting ? 0xA8 : 0x138;
    Window_DrawFrame(d_, 0x29, base / 2, 0x22E, 0x30, 2, rgba);
    const Key* keys = Keys().keys;
    const Key& cur = keys[cursor_];
    int w = Text_Width(cur.label);
    Window_DrawBar(d_, cur.x * 18 - w / 2 + 50, base + cur.y * 24, w, rgba);
    for (int i = 0; i < kKeyCount; ++i) {
        const Key& k = keys[i];
        Text_PrintAt(d_, k.x * 18 - Text_Width(k.label) / 2 + 50, base + k.y * 24, k.label, 10, rgba);
    }
}

// The prompt: a box at the top with the prompt and one 18-pixel slot per character, `*` where none is typed yet.
void NameEntry::NameEntry_DrawPrompt(uint32_t rgba) {
    const ModeInfo& m = kModes[mode_];
    int w = Text_Width(m.prompt), slots = m.max * 18;
    int bw = (slots < w ? w : slots) + 18;
    Window_DrawFrame(d_, (640 - bw) / 2, 0x24, bw, 0x18, 0, rgba);
    Text_PrintAt(d_, (640 - w) / 2, 0x48, m.prompt, 0, rgba);
    int x = (640 - slots) / 2;
    for (int i = 0; i < m.max; ++i, x += 18) {
        const char* s = i < len_ ? Keys().keys[keys_[i]].label : "*";
        Text_PrintAt(d_, x - Text_Width(s) / 2 + 9, 0x60, s, i < len_ ? 10 : 0, rgba);
    }
}

}  // namespace game
