#include "controls.h"

#include <windows.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <vector>

#include "../util/files.h"
#include "../util/json.h"

namespace platform {

namespace {

const char kFormat[] = "RTAGlass controls";
const int kVersion = 1;

// controls.json's names for the actions.
const char* const kActionNames[kActionCount] = {
    "steerLeft", "steerRight", "gas", "brake", "reverse", "jet", "view", "horn", "wingUp", "wingDown", "navigator",
    "up", "down", "left", "right", "confirm", "back", "start",
};

// The PS2's default key setup (Save_ResetButtons: Cross gas, Square brake, Circle reverse, R2 jet, Triangle view,
// R1 horn and lights, L1 wing up, L2 wing down, Select navigator) on the keyboard's and the controller's PS2
// layout.
const uint8_t kDefaultKeys[kActionCount] = {
    VK_LEFT, VK_RIGHT, 'Z', 'A', 'X', '3', 'C', 'E', 'Q', '1', VK_TAB,
    VK_UP, VK_DOWN, VK_LEFT, VK_RIGHT, 'Z', 'C', VK_RETURN,
};
const uint8_t kDefaultPad[kActionCount] = {
    kPadLeft, kPadRight, kPadA, kPadX, kPadB, kPadRT, kPadY, kPadRB, kPadLB, kPadLT, kPadBack,
    kPadUp, kPadDown, kPadLeft, kPadRight, kPadA, kPadY, kPadStart,
};

const char* const kPadNames[kPadInputCount] = {
    "", "A", "B", "X", "Y", "LB", "RB", "LT", "RT", "Back", "Start", "LS", "RS",
    "DpadUp", "DpadDown", "DpadLeft", "DpadRight",
};
const char* const kPadShortNames[kPadInputCount] = {
    "", "A", "B", "X", "Y", "LB", "RB", "LT", "RT", "Back", "Start", "LS", "RS", "Up", "Down", "Left", "Right",
};

// Keys with a name other than their character. At most 5 characters: the screen's column is narrow.
struct KeyName {
    uint8_t vk;
    const char* name;
};
const KeyName kKeyNames[] = {
    {VK_SPACE, "Space"},     {VK_RETURN, "Enter"},    {VK_TAB, "Tab"},        {VK_BACK, "BkSp"},
    {VK_SHIFT, "Shift"},     {VK_CONTROL, "Ctrl"},    {VK_MENU, "Alt"},       {VK_CAPITAL, "Caps"},
    {VK_INSERT, "Ins"},      {VK_DELETE, "Del"},      {VK_HOME, "Home"},      {VK_END, "End"},
    {VK_PRIOR, "PgUp"},      {VK_NEXT, "PgDn"},       {VK_UP, "Up"},          {VK_DOWN, "Down"},
    {VK_LEFT, "Left"},       {VK_RIGHT, "Right"},     {VK_MULTIPLY, "Num*"},  {VK_ADD, "Num+"},
    {VK_SUBTRACT, "Num-"},   {VK_DECIMAL, "Num."},    {VK_DIVIDE, "Num/"},    {VK_OEM_1, ";"},
    {VK_OEM_PLUS, "="},      {VK_OEM_COMMA, ","},     {VK_OEM_MINUS, "-"},    {VK_OEM_PERIOD, "."},
    {VK_OEM_2, "/"},         {VK_OEM_3, "`"},         {VK_OEM_4, "["},        {VK_OEM_5, "\\"},
    {VK_OEM_6, "]"},         {VK_OEM_7, "'"},
};

Controls g_controls;
bool g_loaded = false;
std::string g_path;
bool g_capturing = false;

void Ensure() {
    if (!g_loaded) Controls_Defaults(g_controls), g_loaded = true;
}

// Player 1 must be able to work the menus from the keyboard: a menu action left without a key gets its default.
void FixMenuKeys(Controls& c) {
    for (int a = kDrivingActions; a < kActionCount; ++a)
        if (!c.player[0].key[a]) c.player[0].key[a] = kDefaultKeys[a];
}

}  // namespace

void Controls_Defaults(Controls& c) {
    for (int p = 0; p < 2; ++p) {
        Controls_DefaultPage(c, p, true);
        Controls_DefaultPage(c, p, false);
    }
}

void Controls_DefaultPage(Controls& c, int player, bool driving) {
    int first = driving ? 0 : kDrivingActions, last = driving ? kDrivingActions : kActionCount;
    for (int a = first; a < last; ++a) {
        c.player[player].key[a] = player == 0 ? kDefaultKeys[a] : 0;
        c.player[player].pad[a] = kDefaultPad[a];
    }
}

const Controls& Controls_Get() {
    Ensure();
    return g_controls;
}

bool Controls_Apply(const Controls& c) {
    g_controls = c;
    g_loaded = true;
    if (g_path.empty()) return true;
    return util::WriteReplacing(g_path, [&](FILE* f) {
        fprintf(f, "{\n    \"format\": \"%s\",\n    \"version\": %d,\n", kFormat, kVersion);
        fprintf(f, "    \"notes\": \"keys and controller buttons for each action; \\\"\\\" is none. Keys: A-Z, 0-9, "
                   "F1-F12, Num0-Num9, Space, Enter, Tab, BkSp, Shift, Ctrl, Alt, Up, Down, Left, Right ... (#XX a "
                   "virtual-key code in hex). Controller: A B X Y LB RB LT RT Back Start LS RS DpadUp DpadDown "
                   "DpadLeft DpadRight\"");
        for (int p = 0; p < 2; ++p) {
            fprintf(f, ",\n    \"player%d\": {\n", p + 1);
            for (int column = 0; column < 2; ++column) {
                fprintf(f, "        \"%s\": {", column ? "controller" : "keyboard");
                for (int a = 0; a < kActionCount; ++a) {
                    const char* name = column ? PadInput_Name(c.player[p].pad[a]) : Key_Name(c.player[p].key[a]);
                    fprintf(f, "%s\n            \"%s\": \"%s\"", a ? "," : "", kActionNames[a],
                            strcmp(name, "\\") ? name : "\\\\");
                }
                fprintf(f, "\n        }%s\n", column ? "" : ",");
            }
            fprintf(f, "    }");
        }
        fprintf(f, "\n}\n");
    });
}

void Controls_Load(const std::string& path) {
    g_path = path;
    Controls_Defaults(g_controls);
    g_loaded = true;
    std::vector<uint8_t> data;
    if (path.empty() || !util::ReadAll(path, data) || data.empty()) return;
    util::JsonParser parser = {(const char*)&data[0], (const char*)&data[0] + data.size()};
    util::Json root = parser.Value(0);
    const util::Json* format = root.Get("format");
    if (!parser.ok || !format || format->text != kFormat) return;
    for (int p = 0; p < 2; ++p) {
        char name[16];
        snprintf(name, sizeof name, "player%d", p + 1);
        const util::Json* player = root.Get(name);
        if (!player) continue;
        for (int column = 0; column < 2; ++column) {
            const util::Json* o = player->Get(column ? "controller" : "keyboard");
            if (!o) continue;
            for (int a = 0; a < kActionCount; ++a) {
                const util::Json* v = o->Get(kActionNames[a]);
                if (!v || v->kind != util::Json::kString) continue;
                if (column) {
                    int input = PadInput_FromName(v->text);
                    if (input >= 0) g_controls.player[p].pad[a] = (uint8_t)input;
                } else {
                    int vk = Key_FromName(v->text);
                    if (vk >= 0 && (vk == 0 || Key_Bindable(vk))) g_controls.player[p].key[a] = (uint8_t)vk;
                }
            }
        }
    }
    FixMenuKeys(g_controls);
}

const char* Key_Name(int vk) {
    static char buf[8];
    if (vk <= 0 || vk > 0xFF) return "";
    if ((vk >= 'A' && vk <= 'Z') || (vk >= '0' && vk <= '9')) {
        buf[0] = (char)vk;
        buf[1] = 0;
        return buf;
    }
    if (vk >= VK_F1 && vk <= VK_F24) {
        snprintf(buf, sizeof buf, "F%d", vk - VK_F1 + 1);
        return buf;
    }
    if (vk >= VK_NUMPAD0 && vk <= VK_NUMPAD9) {
        snprintf(buf, sizeof buf, "Num%d", vk - VK_NUMPAD0);
        return buf;
    }
    for (const KeyName& k : kKeyNames)
        if (k.vk == vk) return k.name;
    snprintf(buf, sizeof buf, "#%02X", vk);
    return buf;
}

int Key_FromName(const std::string& name) {
    if (name.empty()) return 0;
    for (int vk = 1; vk <= 0xFF; ++vk)
        if (name == Key_Name(vk)) return vk;
    if (name[0] == '#' && name.size() == 3) {
        char* end;
        long vk = strtol(name.c_str() + 1, &end, 16);
        if (*end == 0 && vk > 0 && vk <= 0xFF) return (int)vk;
    }
    return -1;
}

const char* PadInput_Name(int input) { return input > 0 && input < kPadInputCount ? kPadNames[input] : ""; }
const char* PadInput_ShortName(int input) {
    return input > 0 && input < kPadInputCount ? kPadShortNames[input] : "";
}

int PadInput_FromName(const std::string& name) {
    for (int i = 0; i < kPadInputCount; ++i)
        if (name == kPadNames[i]) return i;
    return -1;
}

bool Key_Bindable(int vk) {
    switch (vk) {
    case VK_ESCAPE: case VK_LWIN: case VK_RWIN: case VK_APPS: case VK_SNAPSHOT: case VK_PACKET: case VK_PROCESSKEY:
    case VK_F10:   // F10 opens the window's menu
        return false;
    }
    return vk > 7 && vk <= 0xFE;   // not the mouse buttons
}

void Controls_SetCapturing(bool on) { g_capturing = on; }
bool Controls_Capturing() { return g_capturing; }

}  // namespace platform
