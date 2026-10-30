// Instruction helpers used by the generated microcode (vu1_microcode.gen.cpp).
// Each one mirrors the matching operation in scripts/vu/vusim.py exactly; change both together.
//
// Float math is plain float32 SSE arithmetic; vu1::Run() switches MXCSR to round-toward-zero, which makes
// every +, -, *, / the correctly rounded truncated result, the same as vusim's exact-then-truncate.
#pragma once

#include <float.h>
#include <math.h>
#include <string.h>

#include "vu1.h"

namespace vu1 {
namespace ops {

// ---- float <-> register bits ----

inline float Rd(uint32_t u) {
    uint32_t e = (u >> 23) & 0xFF;
    if (e == 0xFF) return (u & 0x80000000u) ? -FLT_MAX : FLT_MAX;   // no Inf/NaN on the VU
    if (e == 0) return (u & 0x80000000u) ? -0.0f : 0.0f;            // denormals read as zero
    float f;
    memcpy(&f, &u, 4);
    return f;
}

inline uint32_t Wr(float v) {
    if (v != v) v = FLT_MAX;
    if (v > FLT_MAX) v = FLT_MAX;
    else if (v < -FLT_MAX) v = -FLT_MAX;
    uint32_t u;
    memcpy(&u, &v, 4);
    return u;
}

// Writes from one half of an instruction pair, applied after both halves have read their operands.
struct Pending {
    int reg = 0;          // 0 = nothing pending (vf00 is never written)
    int mask = 0;         // bit k set = field k (x=0 .. w=3)
    uint32_t v[4] = {0, 0, 0, 0};
};

inline void Commit(State& s, const Pending& p) {
    if (p.reg == 0) return;
    for (int k = 0; k < 4; ++k)
        if (p.mask & (1 << k)) s.vf[p.reg][k] = p.v[k];
}

// End of every instruction pair: FMAND reads the MAC flags as of 4 pairs earlier.
inline void PushMac(State& s) {
    s.macRing[0] = s.macRing[1];
    s.macRing[1] = s.macRing[2];
    s.macRing[2] = s.macRing[3];
    s.macRing[3] = s.mac;
}

inline void SetMac(State& s, const float* vals, int mask) {
    uint32_t z = 0, sg = 0;
    for (int k = 0; k < 4; ++k) {
        if (!(mask & (1 << k))) continue;
        int bit = 3 - k;
        if (vals[k] == 0.0f) z |= 1u << bit;
        if (vals[k] < 0.0f) sg |= 1u << bit;
    }
    s.mac = (sg << 4) | z;
}

inline void SetVi(State& s, int r, int v) {
    if (r) s.vi[r] = (uint16_t)v;
}

// ---- upper (FMAC) ----

enum Op { ADD, SUB, MUL, MADD, MSUB, MAX, MINI };
enum { BC_X = 0, BC_Y = 1, BC_Z = 2, BC_W = 3, VEC = 4, USE_Q = 5, USE_I = 6 };

// Each product/sum is a separate float operation so it is rounded (toward zero) on its own, like the VU.
inline float Mul(float a, float b) { volatile float r = a * b; return r; }
inline float Add(float a, float b) { volatile float r = a + b; return r; }
inline float Sub(float a, float b) { volatile float r = a - b; return r; }

// fd < 0 writes ACC.
inline void Arith(State& s, Pending& pu, Op op, int fd, int fs, int ft, int kind, int mask) {
    float S[4], T[4], A[4], t[4], vals[4] = {0, 0, 0, 0};
    for (int k = 0; k < 4; ++k) {
        S[k] = Rd(s.vf[fs][k]);
        T[k] = Rd(s.vf[ft][k]);
        A[k] = Rd(s.acc[k]);
    }
    for (int k = 0; k < 4; ++k)
        t[k] = kind <= BC_W ? T[kind] : kind == VEC ? T[k] : kind == USE_Q ? Rd(s.q) : Rd(s.i);
    for (int k = 0; k < 4; ++k) {
        if (!(mask & (1 << k))) continue;
        switch (op) {
        case ADD:  vals[k] = Add(S[k], t[k]); break;
        case SUB:  vals[k] = Sub(S[k], t[k]); break;
        case MUL:  vals[k] = Mul(S[k], t[k]); break;
        case MADD: vals[k] = Add(A[k], Mul(S[k], t[k])); break;
        case MSUB: vals[k] = Sub(A[k], Mul(S[k], t[k])); break;
        case MAX:  vals[k] = (t[k] > S[k]) ? t[k] : S[k]; break;   // vusim: Python max(), first wins ties
        case MINI: vals[k] = (t[k] < S[k]) ? t[k] : S[k]; break;
        }
    }
    if (fd < 0) {
        for (int k = 0; k < 4; ++k)
            if (mask & (1 << k)) s.acc[k] = Wr(vals[k]);
    } else if (fd != 0) {
        pu.reg = fd;
        pu.mask = mask;
        for (int k = 0; k < 4; ++k) pu.v[k] = Wr(vals[k]);
    }
    if (op != MAX && op != MINI) SetMac(s, vals, mask);
}

inline void OpMula(State& s, int fs, int ft) {
    float S[4], T[4];
    for (int k = 0; k < 4; ++k) { S[k] = Rd(s.vf[fs][k]); T[k] = Rd(s.vf[ft][k]); }
    float vals[4] = {Mul(S[1], T[2]), Mul(S[2], T[0]), Mul(S[0], T[1]), 0.0f};
    for (int k = 0; k < 3; ++k) s.acc[k] = Wr(vals[k]);
    SetMac(s, vals, 7);
}

inline void OpMsub(State& s, Pending& pu, int fd, int fs, int ft) {
    float S[4], T[4], A[4];
    for (int k = 0; k < 4; ++k) { S[k] = Rd(s.vf[fs][k]); T[k] = Rd(s.vf[ft][k]); A[k] = Rd(s.acc[k]); }
    float vals[4] = {Sub(A[0], Mul(S[1], T[2])), Sub(A[1], Mul(S[2], T[0])), Sub(A[2], Mul(S[0], T[1])), 0.0f};
    if (fd) {
        pu.reg = fd;
        pu.mask = 7;
        for (int k = 0; k < 4; ++k) pu.v[k] = Wr(vals[k]);
    }
    SetMac(s, vals, 7);
}

inline void Clip(State& s, int fs, int ft) {
    float x = Rd(s.vf[fs][0]), y = Rd(s.vf[fs][1]), z = Rd(s.vf[fs][2]);
    float w = fabsf(Rd(s.vf[ft][3]));
    uint32_t f = (x > w) | (x < -w) << 1 | (y > w) << 2 | (y < -w) << 3 | (z > w) << 4 | (z < -w) << 5;
    s.clip = ((s.clip << 6) | f) & 0xFFFFFF;
}

inline void Ftoi(State& s, Pending& pu, int ft, int fs, int shift, int mask) {
    if (!ft) return;
    pu.reg = ft;
    pu.mask = mask;
    for (int k = 0; k < 4; ++k) {
        double v = trunc((double)Rd(s.vf[fs][k]) * (double)(1 << shift));   // exact in double
        if (v > 2147483647.0) v = 2147483647.0;
        if (v < -2147483648.0) v = -2147483648.0;
        pu.v[k] = (uint32_t)(int32_t)(int64_t)v;
    }
}

inline void Itof(State& s, Pending& pu, int ft, int fs, int shift, int mask) {
    if (!ft) return;
    pu.reg = ft;
    pu.mask = mask;
    for (int k = 0; k < 4; ++k) {
        volatile float f = (float)((double)(int32_t)s.vf[fs][k] / (double)(1 << shift));   // exact, then truncated
        pu.v[k] = Wr(f);
    }
}

inline void Abs(State& s, Pending& pu, int ft, int fs, int mask) {
    if (!ft) return;
    pu.reg = ft;
    pu.mask = mask;
    for (int k = 0; k < 4; ++k) pu.v[k] = Wr(fabsf(Rd(s.vf[fs][k])));
}

// ---- lower ----

inline void LoadVf(State& s, Pending& pl, int ft, int addr, int mask) {
    if (!ft) return;
    pl.reg = ft;
    pl.mask = mask;
    memcpy(pl.v, s.mem[addr & 0x3FF], 16);
}

inline void StoreVf(State& s, int fs, int addr, int mask) {
    uint32_t* q = s.mem[addr & 0x3FF];
    for (int k = 0; k < 4; ++k)
        if (mask & (1 << k)) q[k] = s.vf[fs][k];
}

inline void MoveVf(State& s, Pending& pl, int ft, int fs, int mask, bool rotate) {
    if (!ft) return;
    pl.reg = ft;
    pl.mask = mask;
    for (int k = 0; k < 4; ++k) pl.v[k] = s.vf[fs][rotate ? (k + 1) & 3 : k];
}

inline void Div(State& s, int fs, int fsf, int ft, int ftf) {
    float a = Rd(s.vf[fs][fsf]), b = Rd(s.vf[ft][ftf]);
    if (b == 0.0f) {
        float m = copysignf(FLT_MAX, a != 0.0f ? a : 1.0f) * copysignf(1.0f, b);
        s.q = Wr(m);
    } else {
        volatile float q = a / b;
        s.q = Wr(q);
    }
}

void Kick(State& s, uint16_t addr, GifSink& sink);   // vu1.cpp

}  // namespace ops
}  // namespace vu1
