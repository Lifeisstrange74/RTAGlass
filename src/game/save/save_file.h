// The save file: a player record (save_data.h) on disk. The PS2 card file is the record's 0x3448 bytes; the PC
// writes the same record as JSON, an object per part of it and a key per field (see docs/save.md), so every
// value the PS2 saves is there and readable. Bytes the game never uses are written only when they aren't zero, so
// a record survives the trip through the JSON unchanged.
//
// A memory card slot is a file next to the executable: save1.json, save2.json.
#pragma once

#include <string>

#include "save_data.h"

namespace game {

// Writes `r` as JSON. Written to a temporary file first and moved over the old one, so a failed write leaves the
// old save.
bool Save_WriteFile(const std::string& path, const SaveRecord& r);
// Reads a JSON save, or a PS2 card's BASLUS-20398 file (exactly 0x3448 bytes, as copied off a card). A JSON save
// starts from a new record (Save_Init), so a field missing from the file keeps its default.
bool Save_ReadFile(const std::string& path, SaveRecord& r);
// The record's bytes as the PS2 card keeps them.
bool Save_WriteRaw(const std::string& path, const SaveRecord& r);

// saveN.json next to the executable (slot 1 or 2, the PS2's memory card slots).
std::string Save_SlotPath(int slot);
bool Save_SlotExists(int slot);

// Development: checks that the field table covers every byte of the record once. Returns the first byte that
// isn't (or is twice), or -1.
int Save_CheckFields();

}  // namespace game
