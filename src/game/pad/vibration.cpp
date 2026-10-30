#include "vibration.h"

namespace game {

namespace {

// The patterns (D_002EA3D0..D_002EA408): 0x80 small motor on, 0x81 off; 0x82 n the large motor at n, 0x83 off,
// 0x84 at the strength asked for; 0xFF both off, the end; any other byte waits that many frames.
const uint8_t kPatterns[8][8] = {
    {0xFF},
    {0x80, 0x84, 0x02, 0x81, 0x06, 0xFF},
    {0x80, 0x84, 0x04, 0xFF},
    {0x80, 0x01, 0x81, 0x82, 0xFF, 0x02, 0xFF},
    {0x82, 0xFF, 0x01, 0x80, 0x01, 0xFF},
    {0x80, 0x82, 0xFF, 0x02, 0xFF},
    {0x84, 0x04, 0x80, 0x02, 0x81, 0x02, 0xFF},
    {0x82, 0x7F, 0x1E, 0xFF},
};

const uint8_t kPadMode = 7;   // the pad's +0x0C: an XInput controller is always analog
const int kRunLimit = 180, kRunReset = 360;   // D_003356B8: 3 seconds running, then still until 6

int g_mode = kVibrationAnalog;   // the pad state's +0x0E

struct PadVibration {
    const uint8_t* at;   // D_00333358: the pattern's next command, null when none
    uint8_t wait;        // D_00335698
    uint8_t strength;    // D_003356A0
    uint8_t motor[2];    // D_003356A8: small, large (scePadSetActDirect's data)
    uint8_t running;     // D_003356B0: 1 the small motor, 2 the large
    int frames;          // D_003356B8
};
PadVibration g_pads[2];

}  // namespace

int Pad_VibrationMode() { return g_mode; }
void Pad_SetVibrationMode(int mode) { g_mode = mode; }

void Pad_Vibrate(int pad, int pattern, int strength) {
    if (!(kPadMode & g_mode) || strength == 0) return;
    PadVibration& p = g_pads[pad & 1];
    p.wait = 0;
    p.strength = (uint8_t)(strength >= 0x100 ? 0xFF : strength);
    p.at = kPatterns[pattern & 7];
}

void Pad_VibrationTask() {
    for (PadVibration& p : g_pads) {
        if (p.at && (p.wait == 0 || --p.wait == 0)) {
            do {
                switch (*p.at) {
                case 0x80: p.motor[0] = 1, p.running |= 1; break;
                case 0x81: p.motor[0] = 0, p.running &= ~1; break;
                case 0x82: p.motor[1] = *++p.at, p.running |= 2; break;
                case 0x83: p.motor[1] = 0, p.running &= ~2; break;
                case 0x84: p.motor[1] = p.strength, p.running |= 2; break;
                case 0xFF: p.motor[0] = p.motor[1] = 0, p.running = 0, p.at = nullptr; break;
                default: p.wait = *p.at; break;
                }
            } while (p.at && (++p.at, p.wait == 0));
        }
        // Running motors count up; at 3 seconds they're held off, and the count starts over at 6.
        if (p.running || p.frames >= kRunLimit) ++p.frames;
        else p.frames = 0;
        if (p.frames == kRunReset) p.frames = 0;
        else if (p.frames >= kRunLimit) p.motor[0] = p.motor[1] = 0;
    }
}

void Pad_Motors(int pad, uint8_t& smallMotor, uint8_t& largeMotor) {
    const PadVibration& p = g_pads[pad & 1];
    smallMotor = p.motor[0];
    largeMotor = p.motor[1];
}

}  // namespace game
