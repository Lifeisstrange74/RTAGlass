// The cars' sounds and ground effects. A reimplementation of the player's car sounds (Car_Sound, func_00217C68: the
// engine, the skid loops), the race's other car sounds (the splash, the bumps, the horn) and the effects the race
// draws at the cars' wheels (Cars_DrawEffects, func_00225120): tire smoke on the road, dust on dirt, snow spray,
// and in water the splashes, the bow wave and the bubble rings. See docs/effects.md.
#pragma once

#include <stdint.h>

#include "../../gfx/draw2d.h"
#include "../../gfx/model.h"
#include "car_physics.h"

namespace game {

// A player's car sounds: SNDMOD's engine `player` and the skid slot D_00335728[player] (2 Player's player 2 has
// engine 1, and its skid loops are the CQ_MAIN entries one after player 1's).
class CarSounds {
public:
    // Car_Init's part: the engine starts at rest (Sound_EnginePlay), no skid loop.
    void Start(int horn, int player = 0);
    // The race is left or over: the engine, the skid loop and the horn stop.
    void Stop();

    // Car_Sound's second half (func_00217C68), from Car_Drive each frame for a player's car: the skid loop for the
    // ground under the front wheels (D_0029CBA8 by surface bits 4-6: CQ_MAIN 6, 8, 0xA, 0xC), at a volume by the
    // squeal, and the engine (type 2, the throttle sample while Cross is held and the car isn't under water, the
    // rpm, reverb on surfaces with bit 16). A finished car's skid loop goes quiet and its engine idles (rpm 0); in
    // the replay (race flag 0x40) nothing changes.
    void Car_Sound(const CarBody& c, uint32_t input, bool finished, bool replay);
    // Car_ReadPad's horn (key 6, R1): the car's horn (+0x245) while the button is held.
    void Car_Horn(bool held);
    // Car_Jet's roar (`event`, CarBody::jetSound): CQ_MAIN 0x10 + the pad starts (not in the replay) or 0x8010 + the
    // pad stops. A loop.
    void Car_Jet(uint8_t event, bool replay);
    // Race_CompleteLap at the player's finish: the horn stops.
    void StopHorn();
    // Sound_MuteCars (func_00258FF0), as the pause opens: the engines go quiet, every sound effect stops (the skid
    // loop and the horn too) and the skid loop is forgotten, so Car_Sound starts it again after. Sound_LoudCars
    // (func_00259050) brings the engines back.
    void Mute();
    void Loud();

private:
    int player_ = 0;    // car flags & 1
    int skid_ = 0;      // D_00335728: the skid loop playing, 0 none
    int horn_ = 0x3C;   // +0x245: 0x3C + 2 x the parts' sound (D_002F00AC)
    bool hornOn_ = false;
};

// The race's particles (D_0172FF70: 64 pairs of puffs, one at each rear wheel, or a pair of drops) and the effects
// that make them, drawn after the course (Race_DrawView step 6).
class CarEffects {
public:
    void Load(gfx::Draw2D& d);
    void Clear();

    // Cars_DrawEffects (func_00225120) for one car, by the class of the ground under its front wheels (surface bits
    // 8-10) while its effect runs (+0x208): 0 road, tire smoke (Car_EmitSmoke); 1 dirt, dust (Car_EmitDust); 2 snow
    // (Car_EmitSnow); 6 a river bed: bubble rings under water (Car_EmitBubbles), and while wading spray every fourth
    // frame (Car_EmitSplash x4) and a bow wave (Car_EmitBow). Other classes stop the effect. `frame` is the race's
    // frame (the puffs' animation and the splashes' timing), `phase` the car's animation offset (6 a car). While
    // `paused` (race flag 0x8000) no spray or bow wave is thrown.
    void Car_Effects(gfx::Draw2D& d, const gfx::Camera3D& camera, CarBody& c, int car, int frame, int phase,
                     bool paused);
    // Car_DrawJet (func_002238A0), from Cars_DrawEffects for cars 0-2 while their jet fires (+0x20E not 0): a flame
    // out of each of the body's two nozzles, eight quads 0.75 wide and 1.5 long fanned round its axis, at half alpha
    // with EFFECT.GSL's flame n % 6. Over the first 8 frames it grows from half size; let go, it fades out in two.
    void Car_DrawJet(gfx::Draw2D& d, const gfx::Camera3D& camera, const CarBody& c, int body) const;
    // Effects_Draw (func_00223D58): every live particle, drawn and aged by its type (not aged while `paused`).
    void Effects_Draw(gfx::Draw2D& d, const gfx::Camera3D& camera, int phase, bool paused);

private:
    enum Type : uint8_t { kNone = 0, kSmoke = 1, kDust = 2, kSnow = 3, kRing = 7, kDrop = 8 };
    struct Particle {
        float a[4], b[4];   // the two puffs' positions; a[3] the alpha (a ring: its size), b[3] the size (a ring:
                            // the water level it rises to)
        Type type;
        int age;            // D_017307F0 +0: frames (drops)
        int8_t kind;        // +4: a drop's kind: 1 splash, 2 bow wave
        int8_t car;         // +5: the bow wave's car
    };
    static const int kParticles = 64;

    // func_00223B30: one puff of picture `type` at each of the two points: a quad 1 wide and 1 high times `size`,
    // standing on the point and facing the camera, at `alpha` (1 = the texture's own).
    void Effect_DrawPuff(gfx::Draw2D& d, const gfx::Camera3D& camera, int anim, const float* a, const float* b,
                         Type type, float size, float alpha) const;
    int FreeParticle() const;   // the first with no alpha left, -1 if all are live

    void Car_EmitSmoke(gfx::Draw2D& d, const gfx::Camera3D& camera, CarBody& c, int anim);     // func_00224068
    void Car_EmitDust(gfx::Draw2D& d, const gfx::Camera3D& camera, CarBody& c, int anim);      // func_002243E0
    void Car_EmitSnow(gfx::Draw2D& d, const gfx::Camera3D& camera, CarBody& c, int anim);      // func_002246C0
    void Car_EmitBubbles(gfx::Draw2D& d, const gfx::Camera3D& camera, CarBody& c, int anim);   // func_00224EC0
    void Car_EmitSplash(const gfx::Camera3D& camera, CarBody& c, int seed);                    // func_002249B8
    void Car_EmitBow(const gfx::Camera3D& camera, CarBody& c, int car);                       // func_00224CD0
    void Drop_Update(int i);                                                                  // func_00225390

    const gfx::Texture* textures_[4][4] = {};   // smoke, dust, snow (four frames each), the ring
    const gfx::Texture* flames_[6] = {};        // the jet's flame (EFFECT.GSL 1-6)
    Particle particles_[kParticles] = {};
    const CarBody* cars_[32] = {};               // the bow waves' cars, by number
};

}  // namespace game
