#include "input.h"

#include <string.h>

#include "controls.h"

namespace platform {

namespace {

// The parts of XINPUT_STATE used here (xinput.h isn't in the XP-era SDK the project builds against).
struct XGamepad {
    WORD buttons;
    BYTE leftTrigger, rightTrigger;
    SHORT lx, ly, rx, ry;
};
struct XState {
    DWORD packet;
    XGamepad pad;
};
typedef DWORD(WINAPI* XInputGetStateFn)(DWORD, XState*);
struct XVibration {
    WORD left, right;
};
typedef DWORD(WINAPI* XInputSetStateFn)(DWORD, XVibration*);

// D-pad repeat, as the game's pad reader (func_0020AB10) does it: 21 frames after the press, then every 3 frames.
const int kRepeatDelay = 21, kRepeatPeriod = 3;
const uint32_t kDpad = kUp | kDown | kLeft | kRight;

// XINPUT_GAMEPAD's buttons for each PadInput (the triggers aren't buttons).
const WORD kXButtons[kPadInputCount] = {
    0,      0x1000, 0x2000, 0x4000, 0x8000, 0x0100, 0x0200, 0,      0,   // none, A, B, X, Y, LB, RB, LT, RT
    0x0020, 0x0010, 0x0040, 0x0080, 0x0001, 0x0002, 0x0004, 0x0008,      // Back, Start, LS, RS, the d-pad
};
const BYTE kTriggerDown = 30;      // XINPUT_GAMEPAD_TRIGGER_THRESHOLD
const SHORT kStickDown = 0x4000;   // how far the stick counts as the d-pad

// Each action's button: the car's own, and the menus' PS2 buttons.
const uint32_t kActionButtons[kActionCount] = {
    kSteerLeft, kSteerRight, kGas, kBrake, kReverse, kJet, kView, kHorn, kWingUp, kWingDown, kNavigator,
    kUp, kDown, kLeft, kRight, kCross, kTriangle, kStart,
};

// The actions a scripted PS2 button (--press) holds: the PS2's default key setup (Save_ResetButtons) and the d-pad.
uint32_t ScriptActions(uint32_t ps2) {
    static const struct {
        uint32_t ps2, action;
    } kScript[] = {
        {kCross, kGas},     {kSquare, kBrake}, {kCircle, kReverse},  {kR2, kJet},          {kTriangle, kView},
        {kR1, kHorn},       {kL1, kWingUp},    {kL2, kWingDown},     {kSelect, kNavigator}, {kLeft, kSteerLeft},
        {kRight, kSteerRight},
    };
    uint32_t b = 0;
    for (const auto& k : kScript)
        if (ps2 & k.ps2) b |= k.action;
    return b;
}

}  // namespace

Input::Input()
    : packetDown_(false), keyPressed_(0), keyPressedNext_(0), xinput_(nullptr), getState_(nullptr), setState_(nullptr) {
    memset(pads_, 0, sizeof(pads_));
    ReleaseKeys();
    const char* dlls[] = {"xinput1_4.dll", "xinput1_3.dll", "xinput9_1_0.dll"};
    for (const char* dll : dlls) {
        if ((xinput_ = LoadLibraryA(dll)) != nullptr) break;
    }
    if (xinput_) getState_ = (void*)GetProcAddress(xinput_, "XInputGetState");
    if (xinput_) setState_ = (void*)GetProcAddress(xinput_, "XInputSetState");
}

Input::~Input() {
    SetMotors(0, 0, 0);
    SetMotors(1, 0, 0);
    if (xinput_) FreeLibrary(xinput_);
}

void Input::ReleaseKeys() {
    memset(keys_, 0, sizeof keys_);
    memset(latched_, 0, sizeof latched_);
    memset(packet_, 0, sizeof packet_);
}

void Input::KeyDown(int vk) {
    if (!keys_[vk] && !keyPressedNext_) keyPressedNext_ = vk;   // not the key's auto-repeat
    keys_[vk] = latched_[vk] = true;
}

void Input::OnKey(WPARAM vk, bool down) {
    if (vk == VK_PACKET) {   // the key comes with WM_CHAR; the key-up releases it
        packetDown_ = down;
        if (!down)
            for (int k = 0; k < 256; ++k)
                if (packet_[k]) keys_[k] = packet_[k] = false;
        return;
    }
    if (vk > 0xFF) return;
    if (down) KeyDown((int)vk);
    else keys_[vk] = false;
}

void Input::OnChar(WPARAM ch) {
    if (!packetDown_) return;   // ordinary keys already arrived as key codes
    packetDown_ = false;
    // Letters come as either case; Enter and Space as their characters.
    WPARAM vk = ch >= 'a' && ch <= 'z' ? ch - 'a' + 'A' : ch == '\r' ? VK_RETURN : ch == ' ' ? VK_SPACE : ch;
    if ((vk >= 'A' && vk <= 'Z') || (vk >= '0' && vk <= '9') || vk == VK_RETURN || vk == VK_SPACE) {
        KeyDown((int)vk);
        packet_[vk] = true;
    }
}

void Input::Script(int pad, uint32_t buttons, bool down) {
    Pad& p = pads_[pad & 1];
    p.script = down ? p.script | buttons : p.script & ~buttons;
}

// The buttons held on XInput controller `pad` (the user index: 0 is pad 1's, 1 pad 2's).
uint32_t Input::PollController(int pad) {
    Pad& p = pads_[pad];
    p.analog = false;
    p.raw = 0;
    memset(p.stick, 0x80, sizeof p.stick);
    if (!getState_) return 0;
    if (p.retry > 0) {
        --p.retry;
        return 0;
    }
    XState s;
    if (((XInputGetStateFn)getState_)((DWORD)pad, &s) != ERROR_SUCCESS) {
        p.retry = 60;
        p.motors[0] = p.motors[1] = 0;   // a controller plugged in later starts still
        return 0;
    }
    // The sticks as the PS2 pad's bytes: XInput's y is up, the PS2's down.
    p.analog = true;
    p.stick[0] = (uint8_t)((s.pad.rx + 32768) >> 8);
    p.stick[1] = (uint8_t)(255 - ((s.pad.ry + 32768) >> 8));
    p.stick[2] = (uint8_t)((s.pad.lx + 32768) >> 8);
    p.stick[3] = (uint8_t)(255 - ((s.pad.ly + 32768) >> 8));
    WORD w = s.pad.buttons;
    for (int i = 1; i < kPadInputCount; ++i)
        if (w & kXButtons[i]) p.raw |= 1u << (i - 1);
    if (s.pad.leftTrigger > kTriggerDown) p.raw |= 1u << (kPadLT - 1);
    if (s.pad.rightTrigger > kTriggerDown) p.raw |= 1u << (kPadRT - 1);
    uint32_t stick = 0;
    if (s.pad.lx < -kStickDown) stick |= kLeft | kSteerLeft;
    if (s.pad.lx > kStickDown) stick |= kRight | kSteerRight;
    return stick;
}

void Input::Update() {
    const Controls& c = Controls_Get();
    for (int i = 0; i < 2; ++i) {
        Pad& p = pads_[i];
        uint32_t was = p.raw;
        uint32_t now = PollController(i) | p.script | ScriptActions(p.script);
        p.rawPressed = p.raw & ~was;
        const Bindings& b = c.player[i];
        for (int a = 0; a < kActionCount; ++a) {
            int key = b.key[a], in = b.pad[a];
            if ((key && (keys_[key] || latched_[key])) || (in && (p.raw >> (in - 1) & 1))) now |= kActionButtons[a];
        }
        p.pressed = now & ~p.held;
        p.held = now;
        p.repeated = p.pressed;
        if (p.pressed & kDpad || !(p.held & kDpad)) {
            p.heldFrames = 0;
        } else if (++p.heldFrames >= kRepeatDelay && (p.heldFrames - kRepeatDelay) % kRepeatPeriod == 0) {
            p.repeated |= p.held & kDpad;
        }
    }
    memset(latched_, 0, sizeof latched_);
    keyPressed_ = keyPressedNext_;
    keyPressedNext_ = 0;
}

void Input::SetMotors(int pad, uint8_t smallMotor, uint8_t largeMotor) {
    Pad& p = pads_[pad & 1];
    uint16_t left = (uint16_t)(largeMotor * 0x101), right = smallMotor ? 0xFFFF : 0;
    if (!setState_ || p.retry > 0 || (left == p.motors[0] && right == p.motors[1])) return;
    XVibration v = {left, right};
    if (((XInputSetStateFn)setState_)((DWORD)(pad & 1), &v) == ERROR_SUCCESS) p.motors[0] = left, p.motors[1] = right;
}

}  // namespace platform
