// Player input as PS2 pad buttons and the car's actions, for the game's two pads. Pad 1 is the keyboard (fed from the
// window procedure) and the first XInput controller together; pad 2 is the second XInput controller (and keys, if
// player 2 has any). XInput is loaded at run time, so machines without it (plain XP) still run with the keyboard.
//
// Which key or controller button does what is the control setup (controls.h, controls.json): each action of the
// car (kSteerLeft ... kNavigator) and of the menus (the d-pad, Cross, Triangle, Start) has its own. The left stick
// always steers and moves Left and Right as well. The sticks themselves (Analog, Stick) turn the rooms'
// turntable.
// Remote-desktop tools (AnyDesk, for one) may send letters as typed characters (a VK_PACKET key event plus
// WM_CHAR) instead of key codes; those count as the key for the letter typed.
#pragma once

#include <windows.h>
#include <stdint.h>

namespace platform {

enum Button : uint32_t {
    kStart = 1u << 0,
    kCross = 1u << 1,
    kCircle = 1u << 2,
    kUp = 1u << 3,
    kDown = 1u << 4,
    kLeft = 1u << 5,
    kRight = 1u << 6,
    kTriangle = 1u << 7,
    kSquare = 1u << 8,
    kL1 = 1u << 9,
    kR1 = 1u << 10,
    kL2 = 1u << 11,
    kR2 = 1u << 12,
    kSelect = 1u << 13,
    // The car's actions (controls.h's kActSteerLeft ... kActNavigator, in that order). The PS2 has the car read pad
    // buttons through the save's key setup; on the PC each action has its own keys.
    kSteerLeft = 1u << 16,
    kSteerRight = 1u << 17,
    kGas = 1u << 18,
    kBrake = 1u << 19,
    kReverse = 1u << 20,
    kJet = 1u << 21,
    kView = 1u << 22,
    kHorn = 1u << 23,
    kWingUp = 1u << 24,
    kWingDown = 1u << 25,
    kNavigator = 1u << 26,
};

// The pad argument below: 0 pad 1, 1 pad 2, kEitherPad both together (as the game's windows read them, +0x0C).
const int kEitherPad = 2;

class Input {
public:
    Input();
    ~Input();

    void OnKey(WPARAM vk, bool down);
    // WM_CHAR: only used for characters that arrive as VK_PACKET.
    void OnChar(WPARAM ch);
    void ReleaseKeys();
    // Scripted checks (--press): holds or lets go of PS2 buttons `buttons` on pad `pad` (0, 1), with the actions
    // the PS2's default key setup gives them.
    void Script(int pad, uint32_t buttons, bool down);
    // Once per frame: polls the controllers and works out which buttons were just pressed.
    void Update();

    uint32_t Held(int pad = 0) const { return Get(pad, &Pad::held); }
    uint32_t Pressed(int pad = 0) const { return Get(pad, &Pad::pressed); }
    // Pressed, plus the d-pad repeating while held (the game's menus move the cursor with this).
    uint32_t Repeated(int pad = 0) const { return Get(pad, &Pad::repeated); }
    // Whether pad `pad` (0, 1) has a controller: the PS2 pad's analog mode (libpad mode 7, the pad state's +0x0C),
    // with sticks. The keyboard has none.
    bool Analog(int pad) const { return pads_[pad & 1].analog; }
    // A stick axis of pad `pad` as the PS2's pad bytes 4-7 have them (the pad state's +0x3C + 4 pad, before the
    // game's mapping): `axis` 0 right X, 1 right Y, 2 left X, 3 left Y; 0-255 with 0x80 the centre, 0 left or up.
    uint8_t Stick(int pad, int axis) const { return pads_[pad & 1].stick[axis & 3]; }
    // Pad `pad`'s controller's motors: the PS2 pad's small motor (on or off) as the right, high-frequency one and
    // its large motor (0-255) as the left. Only sent when they change; turned off when the Input goes.
    void SetMotors(int pad, uint8_t smallMotor, uint8_t largeMotor);

    // For the Controls screen, whatever the setup: the key (a virtual-key code) that went down since the last
    // frame, 0 none; and controller `pad`'s inputs that went down (bit n - 1 for controls.h's PadInput n).
    int KeyPressed() const { return keyPressed_; }
    uint32_t ControllerPressed(int pad) const { return pads_[pad & 1].rawPressed; }

private:
    struct Pad {
        uint32_t held, pressed, repeated;
        uint32_t script;      // buttons held by Script
        int heldFrames;       // frames the current d-pad direction has been held
        int retry;            // frames until polling a missing controller again (polling an empty slot is slow)
        uint16_t motors[2];   // the speeds last sent: left, right
        bool analog;          // a controller answered the last poll
        uint8_t stick[4];     // right X, right Y, left X, left Y (see Stick)
        uint32_t raw, rawPressed;   // the controller's inputs (bit n - 1 for PadInput n), and those just down
    };
    uint32_t Get(int pad, uint32_t Pad::*field) const {
        return pad == kEitherPad ? pads_[0].*field | pads_[1].*field : pads_[pad & 1].*field;
    }
    // Polls XInput controller `pad`: its inputs into raw, and the left stick's Left and Right (as buttons).
    uint32_t PollController(int pad);
    void KeyDown(int vk);

    Pad pads_[2];
    bool keys_[256];      // keys held, by virtual-key code
    bool latched_[256];   // keys pressed since the last Update: a tap shorter than a frame still counts
    bool packet_[256];    // keys held through VK_PACKET characters
    bool packetDown_;     // a VK_PACKET key-down is waiting for its WM_CHAR
    int keyPressed_, keyPressedNext_;   // KeyPressed, and the first key down since the last Update
    HMODULE xinput_;
    void* getState_;
    void* setState_;
};

}  // namespace platform
