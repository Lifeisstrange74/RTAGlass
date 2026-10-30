#include "car_effects.h"

#include <math.h>

#include "../../audio/sound.h"

namespace game {

namespace {

// D_0029CBA8: the skid loop by the ground's sound bits (4-6), CQ_MAIN entries (0: none); a player's number is added.
const int kSkidSounds[8] = {6, 8, 0xA, 0xC, 0, 0, 0, 0};
const int kEngineType = 2;   // the race's engine (CQ_MAIN waves 0x2A off the throttle, 0x29 on it)
const uint32_t kSeJet = 0x10;   // the jet's roar (CQ_MAIN 0x10 and 0x11, a loop), by the car's pad

// EFFECT.GSL's pictures (SYS/EFFECT.GSL, loaded at boot to TBP 0x3F29-0x3F9E): the four frames of tire smoke
// (TBP 0x3F4A + 3n), dust (0x3F7A + 3n), snow and water drops (0x3F68 + 3n), and the bubble ring (0x3F29).
const int kSmokeTexture = 7, kDustTexture = 23, kSnowTexture = 17, kRingTexture = 0;
// The jet's flame, six frames (TBP 0x3F2C + 5n).
const int kFlameTexture = 1, kFlameFrames = 6;

#include "car_jets.inc"

// The rear wheels' x (D_00332D08 and the like) and where the effects sit behind the car.
const float kWheelX = 0.74f;

// A point in the car's frame (x right, y up, z forward) in the world (func_00272728 with the car's matrix).
void CarPoint(const CarBody& c, float x, float y, float z, float* out) {
    for (int k = 0; k < 3; ++k) out[k] = c.m[0][k] * x + c.m[1][k] * y + c.m[2][k] * z + c.pos[k];
    out[3] = 1.0f;
}

// Whether a point is in front of the camera (the view transform's w > 0, func_00272758).
bool InView(const gfx::Camera3D& camera, const float* p) {
    return (p[0] - camera.eye[0]) * camera.forward[0] + (p[1] - camera.eye[1]) * camera.forward[1] +
               (p[2] - camera.eye[2]) * camera.forward[2] > 0;
}

// The car's speed (func_0021D358: the velocity's length / 32768 x 216, km/h) over 300, at most 1.
float SplashStrength(const CarBody& c) {
    float x = c.vel[0] / 32768.0f, y = c.vel[1] / 32768.0f, z = c.vel[2] / 32768.0f;
    float s = sqrtf(x * x + y * y + z * z) * 216.0f / 300.0f;
    return s > 1.0f ? 1.0f : s;
}

}  // namespace

void CarSounds::Start(int horn, int player) {
    Stop();
    player_ = player;
    horn_ = horn;
    audio::Sound_EnginePlay(player_);
    // Car_Init's last call is Car_Update, whose Car_Drive runs Car_Sound at rest: type 2, rpm 0 (so select 1). Both
    // requests land before SNDMOD's next vblank, so the engine keys on as type 2's wave 0x29.
    audio::Sound_EngineRpms(player_, kEngineType, 0, 0, 0);
}

void CarSounds::Stop() {
    if (skid_) audio::Sound_PlaySe(skid_ | 0x8000);
    audio::Sound_PlaySe((kSeJet + player_) | 0x8000);
    skid_ = 0;
    StopHorn();
    audio::Sound_EngineStop(player_);
}

void CarSounds::Car_Sound(const CarBody& c, uint32_t input, bool finished, bool replay) {
    if (finished) {
        if (skid_) audio::Sound_SetVolume(skid_, 0);
        audio::Sound_EngineRpms(player_, kEngineType, 0, 0, 0);
        return;
    }
    if (replay) return;
    int surface = c.surface[0];
    int skid = kSkidSounds[(surface >> 4) & 7], volume;
    if (skid) skid += player_;
    if (c.squeal < 8) {
        skid = 0;
        volume = 0;
    } else if (c.squeal < 0x20) {
        volume = 0x40;
    } else if (c.squeal < 0x40) {
        volume = 0x80;
    } else {
        volume = c.squeal < 0x60 ? 0xC0 : c.squeal;
    }
    if (volume > 0x80) volume = 0x100;
    if (skid_ != skid) {
        if (skid_) audio::Sound_PlaySe(skid_ | 0x8000);
        if (skid) audio::Sound_PlaySe(skid);
        skid_ = skid;
    }
    if (skid_) audio::Sound_SetVolume(skid_, volume);
    int select = c.water > 0 ? 0 : (input & kCarAccel);
    audio::Sound_EngineRpms(player_, kEngineType, select, c.rpm, (surface >> 16) & 1);
}

void CarSounds::Car_Horn(bool held) {
    if (held && !hornOn_) audio::Sound_PlaySe(horn_);
    if (!held && hornOn_) audio::Sound_PlaySe(horn_ | 0x8000);
    hornOn_ = held;
}

void CarSounds::StopHorn() { Car_Horn(false); }

void CarSounds::Car_Jet(uint8_t event, bool replay) {
    if (event == kJetStart && !replay) audio::Sound_PlaySe(kSeJet + player_);
    if (event == kJetStop) audio::Sound_PlaySe((kSeJet + player_) | 0x8000);
}

void CarSounds::Mute() {
    audio::Sound_EngineMute(0);
    audio::Sound_EngineMute(1);
    audio::Sound_StopEffects();
    skid_ = 0;
    hornOn_ = false;
}

void CarSounds::Loud() {
    audio::Sound_EngineLoud(0);
    audio::Sound_EngineLoud(1);
}

void CarEffects::Load(gfx::Draw2D& d) {
    for (int f = 0; f < 4; ++f) {
        textures_[0][f] = &d.Get("SYS/EFFECT.GSL", kSmokeTexture + f);
        textures_[1][f] = &d.Get("SYS/EFFECT.GSL", kDustTexture + f);
        textures_[2][f] = &d.Get("SYS/EFFECT.GSL", kSnowTexture + f);
        textures_[3][f] = &d.Get("SYS/EFFECT.GSL", kRingTexture);
    }
    for (int f = 0; f < kFlameFrames; ++f) flames_[f] = &d.Get("SYS/EFFECT.GSL", kFlameTexture + f);
    Clear();
}

void CarEffects::Clear() {
    for (Particle& p : particles_) p = Particle();
}

void CarEffects::Effect_DrawPuff(gfx::Draw2D& d, const gfx::Camera3D& camera, int anim, const float* a,
                                 const float* b, Type type, float size, float alpha) const {
    int set = type == kSmoke ? 0 : type == kDust ? 1 : type == kRing ? 3 : 2;
    const gfx::Texture* t = textures_[set][type == kRing ? 0 : anim & 3];
    if (!t || !t->id || alpha <= 0 || size <= 0) return;
    // The quad's x runs along up x (the eye - the first point), flat; its y is the world's up (func_00207F88).
    float ex = camera.eye[0] - a[0], ez = camera.eye[2] - a[2], len = sqrtf(ex * ex + ez * ez);
    if (len <= 0) return;
    float rx = ez / len * size, rz = -ex / len * size;
    static const float kCorner[4][4] = {{-0.5f, 1, 0, 0}, {0.5f, 1, 1, 0}, {-0.5f, 0, 0, 1}, {0.5f, 0, 1, 1}};
    static const int kOrder[6] = {0, 1, 2, 1, 3, 2};
    gfx::WorldVertex v[12];
    const float* at[2] = {a, b};
    for (int p = 0; p < 2; ++p)
        for (int i = 0; i < 6; ++i) {
            const float* k = kCorner[kOrder[i]];
            gfx::WorldVertex& w = v[p * 6 + i];
            w.pos[0] = at[p][0] + rx * k[0];
            w.pos[1] = at[p][1] + size * k[1];
            w.pos[2] = at[p][2] + rz * k[0];
            w.uv[0] = k[2];
            w.uv[1] = k[3];
            w.color[0] = w.color[1] = w.color[2] = 0.5f;   // vertex colour 0x80: the texture as it is
            w.color[3] = alpha;
        }
    gfx::DrawStyle style;
    style.clearDepth = false;
    style.state.depthWrite = false;
    gfx::DrawWorldTriangles(d, camera, gfx::Rect{0, 0, gfx::Draw2D::kWidth, gfx::Draw2D::kHeight}, v, 12, t, style);
}

void CarEffects::Car_DrawJet(gfx::Draw2D& d, const gfx::Camera3D& camera, const CarBody& c, int body) const {
    gfx::DrawSourceScope source(d.GetRenderer(), gfx::DrawSource::Effects);
    if (body < 0 || body >= (int)(sizeof kJetNozzle / sizeof kJetNozzle[0])) return;
    int n = c.jet;
    float alpha = 0.5f;   // D_0029D370's w
    if (n < 0) {
        alpha = n * -0.15f;
        n = 20 - n;
    }
    const gfx::Texture* t = flames_[n % kFlameFrames];
    if (!t || !t->id) return;
    // The quad (D_00290670): from the nozzle back along -z, a little below its axis at the tail; s runs along it.
    static const float kQuad[4][5] = {{-0.375f, 0.15f, 0, 0, 1},
                                      {0.375f, 0.15f, 0, 0, 0},
                                      {-0.375f, -0.05f, -1.5f, 1, 1},
                                      {0.375f, -0.05f, -1.5f, 1, 0}};
    static const int kOrder[6] = {0, 1, 2, 1, 3, 2};
    float sxy = 1, sz = 1;
    if (n < 8) {
        sxy = n * 0.0625f + 0.5f;
        sz = n * 0.03125f + 0.75f;
    }
    gfx::WorldVertex v[2 * 8 * 6];
    int count = 0;
    for (int side = 0; side < 2; ++side) {
        const float* nozzle = kJetNozzle[body];
        float nx = side ? -nozzle[0] : nozzle[0];
        // Eight turns about the axis (sceVu0RotMatrixZ), 3.14 / 4 apart, and 3.14 / 8 further on odd frames.
        for (int k = ((n % kFlameFrames) & 1) - 8; k < 8; k += 2) {
            float a = k * 3.14f * 0.125f, ca = cosf(a), sa = sinf(a);
            for (int i = 0; i < 6; ++i) {
                const float* q = kQuad[kOrder[i]];
                float x = q[0] * sxy, y = q[1] * sxy;
                float p[3] = {x * ca - y * sa + nx, x * sa + y * ca + nozzle[1], q[2] * sz + nozzle[2]};
                // The car's body on its suspension (car +0x40, +0x74), then the car.
                float b[3];
                for (int r = 0; r < 3; ++r) b[r] = c.body[0][r] * p[0] + c.body[1][r] * p[1] + c.body[2][r] * p[2];
                b[1] += c.bodyHeight;
                gfx::WorldVertex& w = v[count++];
                float world[4];
                CarPoint(c, b[0], b[1], b[2], world);
                for (int l = 0; l < 3; ++l) w.pos[l] = world[l];
                w.uv[0] = q[3];
                w.uv[1] = q[4];
                w.color[0] = w.color[1] = w.color[2] = 0.5f;   // vertex colour 0x80
                w.color[3] = alpha;
            }
        }
    }
    gfx::DrawStyle style;
    style.clearDepth = false;
    style.state.depthWrite = false;
    gfx::DrawWorldTriangles(d, camera, gfx::Rect{0, 0, gfx::Draw2D::kWidth, gfx::Draw2D::kHeight}, v, count, t,
                            style);
}

int CarEffects::FreeParticle() const {
    for (int i = 0; i < kParticles; ++i)
        if (particles_[i].a[3] <= 0) return i;
    return -1;
}

// Tire smoke: while the count runs (1-32, then round again from 16) a puff at each rear wheel grows and thickens,
// with a second, older one from 17 on; every 2 units the car moves, a puff is left behind that thins out in 16
// frames as it grows.
void CarEffects::Car_EmitSmoke(gfx::Draw2D& d, const gfx::Camera3D& camera, CarBody& c, int anim) {
    float a[4], b[4];
    int n = c.effect;
    if (n > 0x20) c.effect = n = 0;
    if (n > 0x10) {
        float k = (float)(n - 0x10);
        CarPoint(c, -kWheelX, 0, -1.2f - k * 0.02f, a);
        CarPoint(c, kWheelX, 0, -1.2f - k * 0.02f, b);
        Effect_DrawPuff(d, camera, anim, a, b, kSmoke, k * 0.03125f + 0.4f, k * 0.015625f);
    }
    float size, alpha, z;
    if (n <= 0) {
        c.effect = 0;
        alpha = n * -0.015625f;
        size = n * -0.03125f + 0.4f;
        z = n * 0.02f - 1.2f;
    } else {
        alpha = n * 0.015625f;
        size = n * 0.03125f + 0.4f;
        z = -1.2f - n * 0.02f;
        if (n == 0x20) c.effect = 0x10;
    }
    CarPoint(c, -kWheelX, 0, z, a);
    CarPoint(c, kWheelX, 0, z, b);
    if (c.effect) Effect_DrawPuff(d, camera, anim, a, b, kSmoke, size, alpha);
    const Particle& last = particles_[c.particle];
    float dx = a[0] - last.a[0], dz = a[2] - last.a[2];
    if (sqrtf(dx * dx + dz * dz) > 2.0f) {
        int i = FreeParticle();
        if (i < 0) return;
        c.particle = (int16_t)i;
        Particle& p = particles_[i];
        p = Particle();
        p.type = kSmoke;
        for (int k = 0; k < 3; ++k) {
            p.a[k] = a[k];
            p.b[k] = b[k];
        }
        p.a[3] = 0.25f;
        p.b[3] = 0.4f;
    }
}

// Dust: the same at the rear wheels on a 16-frame cycle, thicker (n / 32); a puff is left every unit the car moves
// where the camera can see it, and thins out to an eighth over 48 frames.
void CarEffects::Car_EmitDust(gfx::Draw2D& d, const gfx::Camera3D& camera, CarBody& c, int anim) {
    float a[4], b[4];
    int n = c.effect;
    if (n > 0x10) c.effect = n = 0;
    float size, alpha, z;
    if (n > 0) {
        alpha = n * 0.03125f;
        size = n * 0.03125f + 0.4f;
        z = -1.2f - n * 0.02f;
        if (n == 0x10) c.effect = 0;
    } else {
        c.effect = 0;
        alpha = n * -0.015625f;
        size = n * -0.03125f + 0.4f;
        z = n * 0.02f - 1.2f;
    }
    CarPoint(c, -kWheelX, 0, z, a);
    CarPoint(c, kWheelX, 0, z, b);
    if (c.effect) Effect_DrawPuff(d, camera, anim, a, b, kDust, size, alpha);
    const Particle& last = particles_[c.particle];
    float dx = a[0] - last.a[0], dz = a[2] - last.a[2];
    if (sqrtf(dx * dx + dz * dz) > 1.0f && InView(camera, a)) {
        int i = FreeParticle();
        if (i < 0) return;
        c.particle = (int16_t)i;
        Particle& p = particles_[i];
        p = Particle();
        p.type = kDust;
        for (int k = 0; k < 3; ++k) {
            p.a[k] = a[k];
            p.b[k] = b[k];
        }
        p.a[3] = 0.5f;
        p.b[3] = size;
    }
}

// Snow: as the dust, at half the thickness, a puff every 1.25 units.
void CarEffects::Car_EmitSnow(gfx::Draw2D& d, const gfx::Camera3D& camera, CarBody& c, int anim) {
    float a[4], b[4];
    int n = c.effect;
    if (n > 0x10) c.effect = n = 0;
    float size, alpha, z;
    if (n > 0) {
        alpha = n * 0.015625f;
        size = n * 0.03125f + 0.4f;
        z = -1.2f - n * 0.02f;
        if (n == 0x10) c.effect = 0;
    } else {
        c.effect = 0;
        alpha = n * -0.015625f;
        size = n * -0.03125f + 0.4f;
        z = n * 0.02f - 1.2f;
    }
    CarPoint(c, -kWheelX, 0, z, a);
    CarPoint(c, kWheelX, 0, z, b);
    if (c.effect) Effect_DrawPuff(d, camera, anim, a, b, kSnow, size, alpha);
    const Particle& last = particles_[c.particle];
    float dx = a[0] - last.a[0], dz = a[2] - last.a[2];
    if (sqrtf(dx * dx + dz * dz) > 1.25f && InView(camera, a)) {
        int i = FreeParticle();
        if (i < 0) return;
        c.particle = (int16_t)i;
        Particle& p = particles_[i];
        p = Particle();
        p.type = kSnow;
        for (int k = 0; k < 3; ++k) {
            p.a[k] = a[k];
            p.b[k] = b[k];
        }
        p.a[3] = 0.5f;
        p.b[3] = size;
    }
}

// Under water: a ring grows at each rear wheel over 16 frames, then (where the camera can see it) leaves the car and
// rises a sixteenth a frame, still growing, until it is half a unit under the surface.
void CarEffects::Car_EmitBubbles(gfx::Draw2D& d, const gfx::Camera3D& camera, CarBody& c, int anim) {
    float a[4], b[4];
    int n = c.effect;
    if (n > 0x10) c.effect = n = 0;
    float size = 0, z = -0.8f;
    if (n > 0) {
        size = n * 0.0078125f + 0.0078125f;
        z = -0.8f - n * 0.04f;
        if (n == 0x10) c.effect = 0;
    } else {
        c.effect = 0;
    }
    CarPoint(c, -kWheelX, 0, z, a);
    CarPoint(c, kWheelX, 0, z, b);
    if (c.effect) {
        Effect_DrawPuff(d, camera, anim, a, b, kRing, size, 0.5f);
        return;
    }
    if (!InView(camera, a)) return;
    int i = FreeParticle();
    if (i < 0) return;
    c.particle = (int16_t)i;
    Particle& p = particles_[i];
    p = Particle();
    p.type = kRing;
    for (int k = 0; k < 3; ++k) {
        p.a[k] = a[k];
        p.b[k] = b[k];
    }
    p.a[3] = size;
    p.b[3] = c.probe[0][3] - 0.5f;   // the water level at the front wheels (+0x11C)
}

// Wading, every fourth frame (four of these): a pair of drops thrown up beside the car, from 0.4 to 1.4 out and
// along its length by `seed`, thicker the faster it goes; they spread fast and are gone in 8-40 frames.
void CarEffects::Car_EmitSplash(const gfx::Camera3D& camera, CarBody& c, int seed) {
    int i = FreeParticle();
    if (i < 0) return;
    float strength = SplashStrength(c);
    int s = seed + 4;
    float a[4], b[4];
    CarPoint(c, -0.4f - (s & 15) * 0.0625f, 0.5f, 1.5f - (s & 7) * 0.375f, a);
    CarPoint(c, 0.4f + (seed & 15) * 0.0625f, 0.5f, 1.5f - (seed & 7) * 0.375f, b);
    if (!InView(camera, a)) return;
    c.particle = (int16_t)i;
    Particle& p = particles_[i];
    p = Particle();
    p.type = kDrop;
    p.kind = 1;
    for (int k = 0; k < 3; ++k) {
        p.a[k] = a[k];
        p.b[k] = b[k];
    }
    p.a[3] = strength + 0.25f;
    p.b[3] = 0.5f;
}

// Wading, every frame: the bow wave, a pair of drops at the car's nose that Drop_Update keeps moving with the car.
void CarEffects::Car_EmitBow(const gfx::Camera3D& camera, CarBody& c, int car) {
    int i = FreeParticle();
    if (i < 0) return;
    float strength = SplashStrength(c);
    float a[4];
    CarPoint(c, 0, 0.2f, 1.75f, a);
    if (!InView(camera, a)) return;
    c.particle = (int16_t)i;
    Particle& p = particles_[i];
    p = Particle();
    p.type = kDrop;
    p.kind = 2;
    p.car = (int8_t)car;
    for (int k = 0; k < 4; ++k) p.a[k] = p.b[k] = a[k];
    p.a[3] = strength + 0.5f;
    p.b[3] = 0.25f;
    cars_[car & 31] = &c;
}

// A drop's frame: a splash thins by a thirty-second and spreads by a quarter; a bow wave thins by a sixteenth and
// grows by a sixteenth, its two drops parting to the sides and back from the nose over a 16-frame cycle.
void CarEffects::Drop_Update(int i) {
    Particle& p = particles_[i];
    if (p.kind == 1) {
        p.a[3] -= 0.03125f;
        if (p.a[3] < 0) {
            p.type = kNone;
            p.a[3] = 0;
            p.age = 0;
            p.kind = 0;
        }
        p.b[3] += 0.25f;
    } else if (p.kind == 2) {
        p.a[3] -= 0.0625f;
        if (p.a[3] < 0) {
            p.type = kNone;
            p.a[3] = p.b[3] = 0;
            p.age = 0;
            p.kind = 0;
            return;
        }
        const CarBody* c = cars_[p.car & 31];
        if (c) {
            float n = (float)(p.age & 15);
            float y = n * 0.015625f + 0.2f, z = 1.75f - n * 0.075f;
            float alpha = p.a[3], size = p.b[3];   // (func_00273268 moves only the positions)
            CarPoint(*c, n * 0.09375f, y, z, p.a);
            CarPoint(*c, -n * 0.09375f, y, z, p.b);
            p.a[3] = alpha;
            p.b[3] = size;
        }
        p.b[3] += 0.0625f;
    }
}

void CarEffects::Car_Effects(gfx::Draw2D& d, const gfx::Camera3D& camera, CarBody& c, int car, int frame,
                             int phase, bool paused) {
    gfx::DrawSourceScope source(d.GetRenderer(), gfx::DrawSource::Effects);
    int anim = (phase >> 2) & 3;
    switch ((c.surface[0] >> 8) & 7) {
    case 0:
        if (c.effect) Car_EmitSmoke(d, camera, c, anim);
        break;
    case 1:
        if (c.effect) Car_EmitDust(d, camera, c, anim);
        break;
    case 2:
        if (c.effect) Car_EmitSnow(d, camera, c, anim);
        break;
    case 6:
        if (!c.effect) break;
        if (!(c.events & 0x40)) {
            Car_EmitBubbles(d, camera, c, anim);
        } else if (!paused) {
            if (!(frame & 3))
                for (int k = 0; k < 8; k += 2) Car_EmitSplash(camera, c, frame + k);
            Car_EmitBow(camera, c, car);
        }
        break;
    default:
        c.effect = 0;
        break;
    }
}

void CarEffects::Effects_Draw(gfx::Draw2D& d, const gfx::Camera3D& camera, int phase, bool paused) {
    gfx::DrawSourceScope source(d.GetRenderer(), gfx::DrawSource::Effects);
    int anim = (phase >> 2) & 3;
    for (int i = 0; i < kParticles; ++i) {
        Particle& p = particles_[i];
        if (p.a[3] <= 0) continue;
        // While paused (race flag 0x8000) every particle is drawn as it is, without ageing.
        switch (p.type) {
        case kSmoke:
            Effect_DrawPuff(d, camera, anim, p.a, p.b, kSmoke, p.b[3], p.a[3]);
            if (paused) break;
            p.a[3] -= 0.015625f;
            if (p.a[3] < 0) {
                p.a[3] = 0;
                p.type = kNone;
            }
            p.b[3] += 0.0625f;
            break;
        case kDust:
            Effect_DrawPuff(d, camera, anim, p.a, p.b, kDust, p.b[3], p.a[3]);
            if (paused) break;
            p.a[3] -= 0.0078125f;
            if (p.a[3] <= 0.125f) {
                p.a[3] = 0;
                p.type = kNone;
            }
            p.b[3] += 0.0625f;
            break;
        case kSnow:
            Effect_DrawPuff(d, camera, anim, p.a, p.b, kSnow, p.b[3], p.a[3]);
            if (paused) break;
            p.a[3] -= 0.0078125f;
            if (p.a[3] < 0) {
                p.a[3] = 0;
                p.type = kNone;
            }
            p.b[3] += 0.0625f;
            break;
        case kDrop:
            if (!paused) {
                Drop_Update(i);
                if (p.kind) ++p.age;
            }
            Effect_DrawPuff(d, camera, anim, p.a, p.b, kSnow, p.b[3], p.a[3]);
            break;
        case kRing:
            Effect_DrawPuff(d, camera, 0, p.a, p.b, kRing, p.a[3], 0.5f);
            if (paused) break;
            p.a[3] += 0.0078125f;
            if (p.a[3] >= 1.0f || p.b[3] <= p.a[1]) {
                p.a[3] = 0;
                p.type = kNone;
            }
            p.a[1] += 0.0625f;
            p.b[1] += 0.0625f;
            break;
        default:
            break;
        }
    }
}

}  // namespace game
