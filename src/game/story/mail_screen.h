// My Garage's Check E-mail: a high-level reimplementation of func_00264498. The mailbox (save +0x5A8, newest first,
// an envelope by each: closed unread, open read) in a list window; Cross opens a mail (marked read at once) in a frame
// that grows from the middle, with its subject, sender and lines (data/story/mail.txt); Triangle leaves. It draws over
// the garage, which the caller draws first. See docs/buildings.md, "My Garage".
#pragma once

#include <string>
#include <vector>

#include "../../gfx/draw2d.h"
#include "../../platform/input.h"
#include "../ui/window.h"

namespace game {

class MailScreen {
public:
    MailScreen(gfx::Draw2D& draw, const std::string& dataDir) : d_(draw), dataDir_(dataDir) {}
    void Start();
    // One frame, drawn over the frame the caller began. True once the screen is left (Triangle on the list).
    bool Update(const platform::Input& input);

private:
    // func_00264498's states (+0x25).
    enum { kList = 0, kReading = 1, kOpening = 2, kClosing = 3 };

    void Mail_DrawList();
    void Mail_DrawReading() const;
    int Mail_Selected() const;   // the box index of the list's cursor

    gfx::Draw2D& d_;
    std::string dataDir_;
    Window win_;
    std::vector<std::string> items_;   // "   " + each mail's subject (the envelope goes in the spaces)
    std::vector<const char*> list_;    // the box, newest first
    int state_ = kList;
    int halfWidth_ = 0, height_ = 0;   // +2, +0x1C: the opening frame's size
};

}  // namespace game
