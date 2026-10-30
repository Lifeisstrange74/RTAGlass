// The memory card screens (save/memcard): Mc_MessageTask's messages, the save summary, 2 Player's saves and the
// Adventure's save point and Continue. A memory card slot is a save file next to the executable (save_file.h): slot 1
// is save1.json, slot 2 save2.json. The PC's slots are always "inserted" and formatted, so the card checks' messages
// are never shown; the rest keep the game's wording. See docs/save.md.
#pragma once

#include "../../gfx/draw2d.h"
#include "../../platform/input.h"
#include "../ui/window.h"
#include "save_data.h"

namespace game {

// Mc_MessageTask (0x00227380)'s messages, by number. 0-9 are for both players (the window reads either pad); 12 on
// come in pairs, the even one player 1's and the odd one player 2's, each read on that player's pad.
enum McMessage {
    kMcNotInserted1 = 0,   // 1: slot 2
    kMcCards = 2,          // "Insert memory card ... for player 1, ... for player 2." OK / No
    kMcNoData1 = 4,        // 5: slot 2
    kMcNoCards = 6,
    kMcNoData12 = 7,
    kMcLoadFailed1 = 8,    // 9: player 2
    kMcUnformatted = 12,   // Format? Yes / No (No first)
    kMcFormatFailed = 14,
    kMcNotInserted = 16,
    kMcLoadFailed = 18,
    kMcSaveFailed = 20,
    kMcCreateNew = 22,     // "There is no data." Create new saved data? Yes / No
    kMcNoSpace = 24,
    kMcNoData = 26,
    kMcLoadFailedNew = 28, // "Load failed." Newly create saved data? Yes / No (No first)
    kMcSaved = 30,
};

// Mc_MessageTask's window for `message`: its lines, its items, the pad it reads and the item chosen at first.
// Centred (WindowAlloc). Cross picks the item (or closes a message without items); Triangle picks the last item.
void Mc_MessageWindow(Window& w, int message);

// Save_DrawSummary (0x00227F90): a save in a frame of text cells from `row`: (" Player n" for player 1 or 2,) the
// name, the town it was saved in, the distance in miles and the money. Player 0 leaves out the first line.
void Save_DrawSummary(gfx::Draw2D& d, const SaveRecord& r, int row, int player);

// 2 Player's save of one player's record to their slot: Save_VersusSaveTask (0x00229758) running the save
// (Save_SaveTask, 0x002291C8). It reads what the slot holds first: an existing save asks "Do you want to overwrite?"
// over its summary (saying "This is not your saved data." when it's another name entry's); no file asks to create one;
// an unreadable one says "Load failed." and asks the same. Then it writes the file and says "Saved." (or "Save
// failed."). The other player's record is untouched.
class VersusSave {
public:
    explicit VersusSave(gfx::Draw2D& draw);

    // `slot` 0 is the player's own (player + 1); the Adventure's save point saves record 0 to the slot it picks.
    void Start(int player, int slot = 0);
    // Runs and draws one frame. -1 while it runs; then 1 if the save was written, 0 if not.
    int Update(const platform::Input& input);

private:
    // What the open window is: a Yes / No question before writing, the overwrite question, or the outcome.
    enum Step { kQuestion, kOverwrite, kOutcome };

    void OpenMessage(int message, Step step);
    int Mc_OverwriteTask(const platform::Input& input);   // 0x002280D8: 0 Yes, 1 No, -1 not yet
    void Write();

    gfx::Draw2D& d_;
    int player_;
    int slot_;
    Step step_;
    bool written_;
    Window window_;
    SaveRecord found_;   // what the slot holds (the PS2 reads it over the other record, and Save_VersusSaveTask
                         // puts that back after)
};

// Save_AdventureSaveTask (0x00229538), the Adventure's save point (a message's end code 0x0E): a "slot 1" / "slot 2"
// window (style 2, top left), then Save_SaveTask (VersusSave) of record 0 to the slot picked. A save that isn't
// written (No, or it failed) comes back to the window; Triangle leaves it (SE 0x1B) without saving.
class AdventureSave {
public:
    explicit AdventureSave(gfx::Draw2D& draw);

    void Start();
    // One frame. -1 while it runs; then 1 once saved, 0 if left. After that the window may still be fading out:
    // keep calling it while Closing() (it returns -1 then).
    int Update(const platform::Input& input);
    bool Closing() const { return closing_; }

private:
    gfx::Draw2D& d_;
    Window window_;
    VersusSave save_;
    bool saving_;
    bool closing_;
};

// Continue's load: Mc_LoadSlotWindowTask (0x00228D10), a "slot 1" / "slot 2" window as the save point's, running
// Mc_LoadSlotTask (0x00228B10) on the slot picked. That reads the slot's save (Mc_ReadRecordTask): none says "There is
// no data.", an unreadable one "Load failed.", and either comes back to the window. A save asks "This saved data?"
// (Mc_OverwriteTask with 0x100: Yes first) over its summary; Yes loads it into player 1's record (SE 0x303), No comes
// back to the window. Triangle on the window leaves it.
class AdventureLoad {
public:
    explicit AdventureLoad(gfx::Draw2D& draw);

    void Start();
    // One frame. -1 while it runs; then 1 once a save is loaded (Save_Record(0)), 0 if left.
    int Update(const platform::Input& input);

private:
    enum Step { kSlot, kMessage, kQuestion };

    gfx::Draw2D& d_;
    Window window_;     // the slots
    Window message_;    // the message or the question
    Step step_;
    SaveRecord found_;  // what the slot holds (the PS2 reads it straight into player 1's record)
};

}  // namespace game
