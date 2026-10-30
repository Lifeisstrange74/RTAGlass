// The pads' vibration, as the game runs it: a call asks for one of eight short patterns of motor commands on a pad
// (Pad_Vibrate, func_0020ACB8) and a task plays them a step a frame (Pad_VibrationTask, func_0020A660). The
// DualShock 2's small motor is only on or off; the large one has a strength 0-255. On PC they drive an XInput
// controller's right (high-frequency) and left (low-frequency) motors. See docs/effects.md.
#pragma once

#include <stdint.h>

namespace game {

// The Options' vibration setting (the pad state's +0x0E): it is ANDed with the pad's mode (+0x0C, which looks like
// libpad's current extended id: 7 analog, 4 digital), so On shakes in either mode and "ANALOG mode button" only in
// analog mode. An XInput controller is taken as analog.
enum VibrationMode { kVibrationOff = 0, kVibrationAnalog = 2, kVibrationOn = 4 };
int Pad_VibrationMode();
void Pad_SetVibrationMode(int mode);

// The patterns (D_0029AD60).
enum VibrationPattern {
    kShakeNone,      // nothing
    kShakeLanding,   // both motors (the large at the strength) 2 frames, then the large alone 6: a hard landing
    kShakeShort,     // both motors (the large at the strength) 4 frames: no caller found
    kShakeRight,     // the small motor 1 frame, then the large at full 2: a wall on the car's right
    kShakeLeft,      // the large at full 1 frame, then both 1: a wall on the car's left
    kShakeHit,       // both at full 2 frames: a wall ahead, behind, or more than one side
    kShakeBump,      // the large at the strength 4 frames, then the small on and off 2 each: another car
    kShakeTest,      // the large at half 30 frames: the Options' TEST
};

// Pad_Vibrate (func_0020ACB8): starts `pattern` on `pad` (0 or 1) at `strength` (1..255; more is 255, 0 does
// nothing), replacing what was playing, if the setting allows it.
void Pad_Vibrate(int pad, int pattern, int strength);
// Pad_VibrationTask (func_0020A660): once a frame, steps both pads' patterns. A pad whose motors have run for 3
// seconds is held still until 6 seconds have passed.
void Pad_VibrationTask();
// The motors' state after the task: the small motor (0 or 1) and the large one (0-255).
void Pad_Motors(int pad, uint8_t& smallMotor, uint8_t& largeMotor);

}  // namespace game
