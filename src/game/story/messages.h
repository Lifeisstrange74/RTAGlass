// The Adventure's message tables (D_002A7ED0[town][place]): what the people in the buildings say. Each table is a
// list of byte strings, [0] the speaker's name and [n] message n, read from data/story/messages.txt (made by
// tools/modelconv from the executable). See docs/messages.md for the byte code.
#pragma once

#include <string>
#include <vector>

namespace game {

typedef std::vector<std::string> MessageTable;

// Reads dataDir/story/messages.txt. False if it's missing.
bool Messages_Load(const std::string& dataDir);
// The table of `place` in `town`, or null.
const MessageTable* Messages_Table(int town, int place);

// The E-mails (D_002DB948, data/story/mail.txt): mail m's strings, [0] the subject, [1] the sender, then its lines
// (`$N` the player's car's name). Mail_Load reads them (false if the file's missing); Mail_Text is null for a mail
// that isn't there.
bool Mail_Load(const std::string& dataDir);
const MessageTable* Mail_Text(int m);

}  // namespace game
