#include "save_screens.h"

#include "../../audio/sound.h"
#include "../text.h"
#include "save_file.h"
#include "../../game_text.inc"   // the game's text, GT_<address>_<length>, made from the ELF

namespace game {

namespace {

// Mc_MessageTask's texts (D_002EB030 on): the lines, then the items.
struct McText {
    const char* lines[3];
    const char* items[2];
};
const char kCard[] = GT_002EB030_36;
const McText kMcText[32] = {
    {{kCard, GT_002EB058_40}},
    {{kCard, GT_002EB088_40}},
    {{GT_002EB0B8_43, GT_002EB0E8_47,
      GT_002EB118_41},
     {"OK", "No"}},
    {},
    {{GT_002EB148_46, GT_002EB178_43}},
    {{GT_002EB148_46, GT_002EB1A8_43}},
    {{GT_002EB1D8_49, GT_002EB210_28}},
    {{GT_002EB148_46, GT_002EB230_49}},
    {{GT_002EB268_45}},
    {{GT_002EB298_45}},
    {},
    {},
    {{kCard, GT_002EB2C8_15, GT_002EB2D8_22}, {"Yes", "No"}},
    {{kCard, GT_002EB2C8_15, GT_002EB2D8_22}, {"Yes", "No"}},
    {{GT_002EB2F0_14}},
    {{GT_002EB2F0_14}},
    {{kCard, GT_002EB300_16}},
    {{kCard, GT_002EB300_16}},
    {{GT_002EB318_12}},
    {{GT_002EB318_12}},
    {{GT_002EB328_12}},
    {{GT_002EB328_12}},
    {{GT_002EB338_17, GT_002EB350_37}, {"Yes", "No"}},
    {{GT_002EB338_17, GT_002EB350_37}, {"Yes", "No"}},
    {{GT_002EB378_28, GT_002EB398_47}},
    {{GT_002EB378_28, GT_002EB398_47}},
    {{GT_002EB338_17}},
    {{GT_002EB338_17}},
    {{GT_002EB318_12, GT_002EB3C8_39}, {"Yes", "No"}},
    {{GT_002EB318_12, GT_002EB3C8_39}, {"Yes", "No"}},
    {{GT_003335F0_6}},
    {{GT_003335F0_6}},
};

}  // namespace

void Mc_MessageWindow(Window& w, int message) {
    w = Window();
    const McText& m = kMcText[message];
    for (const char* line : m.lines)
        if (line) w.AddLine(line, 0);
    for (const char* item : m.items)
        if (item) w.AddLine(item, kLineSelectable);
    w.pad = (unsigned char)(message < 10 ? platform::kEitherPad : message & 1);
    if ((message & ~1) == kMcUnformatted || (message & ~1) == kMcLoadFailedNew) w.sel = 1;
}

void Save_DrawSummary(gfx::Draw2D& d, const SaveRecord& r, int row, int player) {
    Window_DrawCellFrame(d, 8, row, 0x16, row + 4 + (player != 0), 1);
    if (player != 0) {
        Text_Print(d, 0xC, row + 1, player == 1 ? GT_002EB4A0_9 : GT_002EB4B0_9, 0xA);
        ++row;
    }
    Text_Print(d, 9, row + 1, r.name, 0);
    Text_Print(d, 0xA, row + 2, Save_TownName(r.saveTown), 0);
    Number_PrintSmallRight(d, 0xFA, row * 0x18 + 0x52, r.distance / 1609, 0);
    Text_Print(d, 0xC, row + 3, "mile", 0);
    Number_PrintRight(d, 0x186, (row * 0xC + 0x29) * 2, r.money, 0);
    Text_Print(d, 0x13, row + 3, r.currency, 0);
}

VersusSave::VersusSave(gfx::Draw2D& draw) : d_(draw), player_(0), slot_(1), step_(kOutcome), written_(false) {}

// Save_SaveTask (0x002291C8) checks the card (on the PC there's always one, formatted) and reads the slot's save: there
// being one leads to the overwrite question, none to "There is no data." and an unreadable one to "Load failed.", both
// asking whether to create it. (Which read result is which is from the messages' wording: G.)
void VersusSave::Start(int player, int slot) {
    player_ = player & 1;
    written_ = false;
    if (slot == 0) slot = player_ + 1;
    slot_ = slot;
    if (!Save_SlotExists(slot)) {
        OpenMessage(kMcCreateNew + player_, kQuestion);
    } else if (!Save_ReadFile(Save_SlotPath(slot), found_)) {
        OpenMessage(kMcLoadFailedNew + player_, kQuestion);
    } else {
        // Mc_OverwriteTask: another name entry's save says so and starts on No. It's a window of its own lower down
        // (y 0x78), on the player's pad, with the save's summary over the window's frame.
        window_ = Window();
        // (The PC's guard: the test menu's complete record counts as another's, Save_TestRecord.)
        bool other = found_.id != Save_Record(player_).id || (player_ == 0 && Save_TestRecord());
        if (other) window_.AddLine(GT_002EB4D8_28, 0);
        window_.AddLine(GT_002EB4F8_25, 0);
        window_.AddLine("Yes", kLineSelectable);
        window_.AddLine("No", kLineSelectable);
        window_.sel = other ? 1 : 0;
        window_.y = 0x78;
        window_.pad = (unsigned char)player_;
        if (found_.license > 3) found_.license = 3;
        step_ = kOverwrite;
    }
}

void VersusSave::OpenMessage(int message, Step step) {
    Mc_MessageWindow(window_, message);
    step_ = step;
}

int VersusSave::Mc_OverwriteTask(const platform::Input& input) {
    Save_DrawSummary(d_, found_, 4, 0);
    int r = window_.Update(d_, input);
    if (r == 0) return -1;
    return r == -1 ? 1 : window_.sel;
}

// func_00228708 (func_0022A168 after the overwrite question): the record to its slot's file. Once the file is
// written and closed (state 9) it plays SE 0x303, or 0x1E if that failed, before "Saved." or "Save failed.".
void VersusSave::Write() {
    written_ = Save_WriteFile(Save_SlotPath(slot_), Save_Record(player_));
    audio::Sound_PlaySe(written_ ? 0x303 : 0x1E);
    OpenMessage((written_ ? kMcSaved : kMcSaveFailed) + player_, kOutcome);
}

// Mc_MessageTask plays 0x1A whichever item is picked; the overwrite question's No plays 0x1B.
int VersusSave::Update(const platform::Input& input) {
    if (step_ == kOverwrite) {
        int r = Mc_OverwriteTask(input);
        if (r == 0) Write();
        else if (r == 1) audio::Sound_PlaySe(0x1B);
        return r == 1 ? 0 : -1;
    }
    int r = window_.Update(d_, input);
    if (r != 1 && r != -1) return -1;
    audio::Sound_PlaySe(0x1A);
    if (step_ == kOutcome) return written_ ? 1 : 0;
    int sel = r == -1 ? window_.items - 1 : window_.sel;
    if (sel != 0) return 0;
    Write();
    return -1;
}

AdventureSave::AdventureSave(gfx::Draw2D& draw) : d_(draw), save_(draw), saving_(false), closing_(false) {}

// The slot strings by language (D_00333638 on); the window starts on the slot in the info's +0x24, which is 0 here
// (what else sets it: not checked).
void AdventureSave::Start() {
    window_ = Window();
    window_.AddLine(GT_00333638_6, kLineSelectable);
    window_.AddLine(GT_00333640_6, kLineSelectable);
    window_.Place(5);
    window_.style = 2;
    window_.state = Window::kOpening;
    saving_ = closing_ = false;
}

int AdventureSave::Update(const platform::Input& input) {
    if (saving_) {
        // The window stays, inactive, under the save's messages.
        window_.Update(d_, input);
        int r = save_.Update(input);
        if (r < 0) return -1;
        saving_ = false;
        if (r == 1) return 1;   // saved: the task ends there, its window with it
        window_.state = Window::kOpen;
        return -1;
    }
    int r = window_.Update(d_, input);
    if (closing_) {
        if (r == -2) closing_ = false;
        return -1;
    }
    if (r == 1) {
        audio::Sound_PlaySe(0x1A);
        window_.state = Window::kOpen | Window::kInactive;
        save_.Start(0, window_.sel + 1);
        saving_ = true;
    } else if (r == -1) {
        audio::Sound_PlaySe(0x1B);
        window_.state = Window::kClosing;
        closing_ = true;
        return 0;
    }
    return -1;
}

AdventureLoad::AdventureLoad(gfx::Draw2D& draw) : d_(draw), step_(kSlot), found_() {}

void AdventureLoad::Start() {
    window_ = Window();
    window_.AddLine(GT_00333638_6, kLineSelectable);
    window_.AddLine(GT_00333640_6, kLineSelectable);
    window_.Place(5);
    window_.style = 2;
    window_.state = Window::kOpening;
    step_ = kSlot;
}

int AdventureLoad::Update(const platform::Input& input) {
    int pick = window_.Update(d_, input);
    if (step_ == kSlot) {
        int r = pick;
        if (r == 1) {
            audio::Sound_PlaySe(0x1A);
            window_.state = Window::kOpen | Window::kInactive;
            // Mc_LoadSlotTask: the PC's card is always there and formatted (func_002282C0's checks), so on to the read.
            int slot = window_.sel + 1;
            if (!Save_SlotExists(slot)) {
                Mc_MessageWindow(message_, kMcNoData);
                step_ = kMessage;
            } else if (!Save_ReadFile(Save_SlotPath(slot), found_)) {
                Mc_MessageWindow(message_, kMcLoadFailed);
                step_ = kMessage;
            } else {
                // Mc_OverwriteTask (0x100): one line, Yes / No from Yes, at y 0x78; the license clamped to Super A.
                message_ = Window();
                message_.AddLine(GT_002EB4C0_16, 0);   // D_002EB4C0
                message_.AddLine("Yes", kLineSelectable);
                message_.AddLine("No", kLineSelectable);
                message_.y = 0x78;
                if (found_.license > 3) found_.license = 3;
                step_ = kQuestion;
            }
        } else if (r == -1) {
            // The window goes with the task: the Adventure menu ends at once.
            audio::Sound_PlaySe(0x1B);
            return 0;
        }
        return -1;
    }
    if (step_ == kQuestion) Save_DrawSummary(d_, found_, 4, 0);
    int r = message_.Update(d_, input);
    if (r != 1 && r != -1) return -1;
    if (step_ == kQuestion && r == 1 && message_.sel == 0) {
        audio::Sound_PlaySe(0x303);
        Save_Record(0) = found_;
        return 1;
    }
    // Mc_MessageTask plays 0x1A for either button; the question's No plays 0x1B.
    audio::Sound_PlaySe(step_ == kQuestion ? 0x1B : 0x1A);
    window_.state = Window::kOpen;
    step_ = kSlot;
    return -1;
}

}  // namespace game
