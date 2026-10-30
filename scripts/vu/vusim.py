"""Minimal VU1 interpreter for Road Trip's microcode image: the reference oracle for the C++ versions.

Scope: exactly the opcodes the 959-instruction image uses (see vudis.py), executed one pair at a time.
Register results are visible to the next instruction pair (the microcode never reads a VF result early,
so stalls never change values). The one timing effect that matters is the MAC flag delay: FMAND sees the
MAC flags as they were 4 pairs earlier, because FMAC results land 4 cycles after issue and slot 0x60's
backface test relies on it (validated against PCSX2). Within a pair, both halves read their operands
before either writes. Branch delay slots and the E-bit delay slot are modelled.

Registers hold raw 32-bit patterns (ftoi results are integers living in float registers). Every FMAC
result is rounded toward zero to float32, like the VU (and PCSX2 with VU1.Roundmode = chop): each
operation is computed exactly (TwoSum for add/sub, exact double products, exact rational division) and
then truncated. There's no Inf/NaN (clamped to +-FLT_MAX), and denormals read as zero.

    sim = VU1(code)                           # code: {byte_addr: (lower_word, upper_word)}
    sim.mem[q][k] = uint32                    # 1024 qwords x 4 words of data memory
    kicks = sim.run(entry_addr, top)          # [(qword_addr, [qwords...]), ...] from XGKICK
"""
import math
import struct
from fractions import Fraction

from vudis import UPPER, UPPER_FD, LOWER, LOWEROP, T3, LOWER_NOP, sext

FLT_MAX = 3.4028234663852886e38
X, Y, Z, W = 0, 1, 2, 3
_pack_f, _unpack_f = struct.Struct('<f').pack, struct.Struct('<f').unpack
_pack_i, _unpack_i = struct.Struct('<I').pack, struct.Struct('<I').unpack


def bits_to_f(u):
    """uint32 -> float, treating Inf/NaN patterns as +-FLT_MAX like the VU."""
    if (u >> 23) & 0xFF == 0xFF:
        return -FLT_MAX if u & 0x80000000 else FLT_MAX
    if (u >> 23) & 0xFF == 0:                 # VU flushes denormals to zero
        return -0.0 if u & 0x80000000 else 0.0
    return _unpack_f(_pack_i(u))[0]


def f_to_bits(f):
    """float -> uint32 after rounding to float32 and clamping."""
    if math.isnan(f):
        f = FLT_MAX
    if f > FLT_MAX:
        f = FLT_MAX
    elif f < -FLT_MAX:
        f = -FLT_MAX
    try:
        return _unpack_i(_pack_f(f))[0]
    except OverflowError:                     # rounds past FLT_MAX
        return _unpack_i(_pack_f(math.copysign(FLT_MAX, f)))[0]


def _f32_step_toward_zero(f):
    u = _unpack_i(_pack_f(f))[0]
    return _unpack_f(_pack_i(u - 1))[0] if u & 0x7FFFFFFF else f


def f32_rz(s, e=0.0):
    """Round the exact value s + e (two doubles, |e| << |s|) toward zero to float32, clamped to +-FLT_MAX."""
    if math.isnan(s):
        return FLT_MAX
    if abs(s) > FLT_MAX:                      # rounding is monotone, so the exact value is beyond too
        return math.copysign(FLT_MAX, s)
    f = _unpack_f(_pack_f(s))[0]              # round to nearest float32
    d = (s - f) + e                           # sign of (exact - f)
    if (f > 0 and d < 0) or (f < 0 and d > 0):
        f = _f32_step_toward_zero(f)          # nearest was away from zero: step back
    return f


def rz_add(a, b):
    s = a + b
    bb = s - a
    e = (a - (s - bb)) + (b - bb)             # TwoSum: s + e == a + b exactly
    return f32_rz(s, e)


def rz_mul(a, b):
    return f32_rz(a * b)                      # float32 x float32 is exact in double


def rz_div(a, b):
    exact = Fraction(a) / Fraction(b)
    s = float(exact)
    return f32_rz(s, float(exact - Fraction(s)))


def dest_fields(c):
    return [k for k in range(4) if c & (1 << (24 - k))]


def parse_upper(name):
    """Return (base, operand) for an upper mnemonic; operand is 'x'/'y'/'z'/'w' (broadcast), 'q', 'i' or 'v'."""
    vector_names = {'add', 'sub', 'mul', 'madd', 'msub', 'max', 'mini', 'opmsub',
                    'adda', 'suba', 'mula', 'madda', 'msuba', 'opmula'}
    if name in vector_names:
        return name, 'v'
    if name == 'minii':
        return 'mini', 'i'
    base, last = name[:-1], name[-1]
    if base in vector_names and last in 'xyzwqi':
        return base, last
    raise NotImplementedError(name)


class VU1:
    def __init__(self, code):
        self.code = code
        self.mem = [[0, 0, 0, 0] for _ in range(1024)]
        self.vf = [[0, 0, 0, 0] for _ in range(32)]
        self.vf[0] = [0, 0, 0, f_to_bits(1.0)]
        self.vi = [0] * 16
        self.acc = [0, 0, 0, 0]
        self.q = f_to_bits(0.0)
        self.i = f_to_bits(0.0)
        self.clip = 0
        self.mac = 0
        self.mac_ring = [0, 0, 0, 0]          # MAC after each of the last 4 pairs; [0] is what FMAND sees
        self.top = 0
        self.kicks = []

    # ---- helpers ----
    def fv(self, r):
        return [bits_to_f(u) for u in self.vf[r]]

    def write_vf(self, writes, r, vals_bits, fields):
        if r != 0:
            for k in fields:
                writes.append((r, k, vals_bits[k]))

    def set_vi(self, r, v):
        if r:
            self.vi[r] = v & 0xFFFF

    def set_mac(self, vals, fields):
        z = s = 0
        for k in fields:
            bit = 3 - k
            if vals[k] == 0.0:
                z |= 1 << bit
            if math.copysign(1.0, vals[k]) < 0 and vals[k] != 0.0:
                s |= 1 << bit
        self.mac = (s << 4) | z

    def gif_packet(self, addr):
        out, a = [], addr
        for _ in range(64):
            tag = self.mem[a & 0x3FF]
            out.append(list(tag))
            lo = tag[0] | (tag[1] << 32)
            nloop, eop = lo & 0x7FFF, (lo >> 15) & 1
            flg, nreg = (lo >> 58) & 3, ((lo >> 60) & 15) or 16
            n = nloop * nreg if flg == 0 else (nloop * nreg + 1) // 2 if flg == 1 else nloop
            for k in range(n):
                out.append(list(self.mem[(a + 1 + k) & 0x3FF]))
            a += 1 + n
            if eop:
                break
        return out

    # ---- upper half: returns list of (reg, field, bits) writes ----
    def upper(self, c):
        writes = []
        op = c & 0x3F
        ft, fs, fd = (c >> 16) & 31, (c >> 11) & 31, (c >> 6) & 31
        fields = dest_fields(c)
        S, T = self.fv(fs), self.fv(ft)
        name = UPPER[op]
        acc_dest = False
        if name.startswith('FD'):
            name = UPPER_FD[op & 3][(c >> 6) & 31]
            if name == 'nop':
                return writes
            if name == 'clip':
                w = abs(T[W])
                f = (S[X] > w) | (S[X] < -w) << 1 | (S[Y] > w) << 2 | (S[Y] < -w) << 3 \
                    | (S[Z] > w) << 4 | (S[Z] < -w) << 5
                self.clip = ((self.clip << 6) | f) & 0xFFFFFF
                return writes
            if name.startswith('itof'):
                sh = int(name[4:])
                vals = [0] * 4
                for k in fields:
                    iv = self.vf[fs][k]
                    iv = iv - (1 << 32) if iv & 0x80000000 else iv
                    vals[k] = f_to_bits(f32_rz(iv / float(1 << sh)))
                self.write_vf(writes, ft, vals, fields)
                return writes
            if name.startswith('ftoi'):
                sh = int(name[4:])
                vals = [0] * 4
                for k in fields:
                    v = math.trunc(S[k] * (1 << sh))
                    vals[k] = max(-0x80000000, min(0x7FFFFFFF, v)) & 0xFFFFFFFF
                self.write_vf(writes, ft, vals, fields)
                return writes
            if name == 'abs':
                self.write_vf(writes, ft, [f_to_bits(abs(v)) for v in S], fields)
                return writes
            acc_dest = True
        base, operand = parse_upper(name)
        if operand in 'xyzw':
            t = [T['xyzw'.index(operand)]] * 4
        elif operand == 'q':
            t = [bits_to_f(self.q)] * 4
        elif operand == 'i':
            t = [bits_to_f(self.i)] * 4
        else:
            t = T
        A = [bits_to_f(u) for u in self.acc]

        if base in ('opmula', 'opmsub'):
            fields = [X, Y, Z]
            if base == 'opmula':
                vals = [rz_mul(S[Y], T[Z]), rz_mul(S[Z], T[X]), rz_mul(S[X], T[Y]), 0.0]
            else:
                vals = [rz_add(A[0], -rz_mul(S[Y], T[Z])), rz_add(A[1], -rz_mul(S[Z], T[X])),
                        rz_add(A[2], -rz_mul(S[X], T[Y])), 0.0]
        else:
            vals = [0.0] * 4
            for k in fields:
                b = base[:-1] if acc_dest else base       # 'madda' -> 'madd'
                if b == 'add':
                    vals[k] = rz_add(S[k], t[k])
                elif b == 'sub':
                    vals[k] = rz_add(S[k], -t[k])
                elif b == 'mul':
                    vals[k] = rz_mul(S[k], t[k])
                elif b == 'madd':
                    vals[k] = rz_add(A[k], rz_mul(S[k], t[k]))
                elif b == 'msub':
                    vals[k] = rz_add(A[k], -rz_mul(S[k], t[k]))
                elif b == 'max':
                    vals[k] = max(S[k], t[k])
                elif b == 'mini':
                    vals[k] = min(S[k], t[k])
                else:
                    raise NotImplementedError(name)
        vbits = [f_to_bits(v) for v in vals]
        if acc_dest or base == 'opmula':
            for k in fields:
                self.acc[k] = vbits[k]
        else:
            self.write_vf(writes, fd, vbits, fields)
        if base not in ('max', 'mini'):
            self.set_mac(vals, fields)
        return writes

    # ---- lower half: returns (writes, branch_target_or_None) ----
    def lower(self, c, pc):
        writes = []
        if c == LOWER_NOP:
            return writes, None
        name = LOWER[(c >> 25) & 0x7F]
        it, is_ = (c >> 16) & 31, (c >> 11) & 31
        it16, is16 = it & 15, is_ & 15
        imm11 = sext(c & 0x7FF, 11)
        fields = dest_fields(c)
        vi = self.vi
        target = None
        if name == 'lq':
            q = self.mem[(vi[is16] + imm11) & 0x3FF]
            self.write_vf(writes, it, q, fields)
        elif name == 'sq':
            q = self.mem[(vi[it16] + imm11) & 0x3FF]
            for k in fields:
                q[k] = self.vf[is_][k]
        elif name == 'ilw':
            self.set_vi(it16, self.mem[(vi[is16] + imm11) & 0x3FF][fields[0]])
        elif name == 'isw':
            for k in fields:
                self.mem[(vi[is16] + imm11) & 0x3FF][k] = vi[it16]
        elif name in ('iaddiu', 'isubiu'):
            imm15 = ((c >> 10) & 0x7800) | (c & 0x7FF)
            self.set_vi(it16, vi[is16] + imm15 if name == 'iaddiu' else vi[is16] - imm15)
        elif name == 'fcand':
            self.set_vi(1, int(bool(self.clip & c & 0xFFFFFF)))
        elif name == 'fcor':
            self.set_vi(1, int(((self.clip | c) & 0xFFFFFF) == 0xFFFFFF))
        elif name == 'fcget':
            self.set_vi(it16, self.clip & 0xFFF)
        elif name == 'fmand':
            self.set_vi(it16, self.mac_ring[0] & vi[is16])
        elif name == 'b':
            target = pc + 8 + imm11 * 8
        elif name == 'bal':
            self.set_vi(it16, (pc + 16) // 8)
            target = pc + 8 + imm11 * 8
        elif name == 'jr':
            target = vi[is16] * 8
        elif name in ('ibeq', 'ibne'):
            if (vi[it16] == vi[is16]) == (name == 'ibeq'):
                target = pc + 8 + imm11 * 8
        elif name in ('ibltz', 'ibgtz', 'iblez', 'ibgez'):
            a = sext(vi[is16], 16)
            if {'ibltz': a < 0, 'ibgtz': a > 0, 'iblez': a <= 0, 'ibgez': a >= 0}[name]:
                target = pc + 8 + imm11 * 8
        elif name == 'LOWEROP':
            sub = LOWEROP[c & 0x3F]
            id16 = (c >> 6) & 15
            if sub == 'iadd':
                self.set_vi(id16, vi[is16] + vi[it16])
            elif sub == 'isub':
                self.set_vi(id16, vi[is16] - vi[it16])
            elif sub == 'iand':
                self.set_vi(id16, vi[is16] & vi[it16])
            elif sub == 'ior':
                self.set_vi(id16, vi[is16] | vi[it16])
            elif sub == 'iaddi':
                self.set_vi(it16, vi[is16] + sext((c >> 6) & 31, 5))
            else:
                self.lower_t3(T3[c & 3][(c >> 6) & 31], c, it, is_, it16, is16, fields, writes)
        else:
            raise NotImplementedError(name)
        return writes, target

    def lower_t3(self, t3, c, it, is_, it16, is16, fields, writes):
        vi = self.vi
        fsf, ftf = (c >> 21) & 3, (c >> 23) & 3
        if t3 == 'move':
            self.write_vf(writes, it, self.vf[is_], fields)
        elif t3 == 'mr32':
            s = self.vf[is_]
            self.write_vf(writes, it, [s[Y], s[Z], s[W], s[X]], fields)
        elif t3 == 'lqi':
            self.write_vf(writes, it, self.mem[vi[is16] & 0x3FF], fields)
            self.set_vi(is16, vi[is16] + 1)
        elif t3 == 'sqi':
            q = self.mem[vi[it16] & 0x3FF]
            for k in fields:
                q[k] = self.vf[is_][k]
            self.set_vi(it16, vi[it16] + 1)
        elif t3 == 'sqd':
            self.set_vi(it16, vi[it16] - 1)
            q = self.mem[vi[it16] & 0x3FF]
            for k in fields:
                q[k] = self.vf[is_][k]
        elif t3 == 'div':
            a, b = bits_to_f(self.vf[is_][fsf]), bits_to_f(self.vf[it][ftf])
            if b == 0.0:
                self.q = f_to_bits(math.copysign(FLT_MAX, a if a else 1.0) * math.copysign(1.0, b))
            else:
                self.q = f_to_bits(rz_div(a, b))
        elif t3 == 'waitq':
            pass
        elif t3 == 'ilwr':
            self.set_vi(it16, self.mem[vi[is16] & 0x3FF][fields[0]])
        elif t3 == 'iswr':
            for k in fields:
                self.mem[vi[is16] & 0x3FF][k] = vi[it16]
        elif t3 == 'xtop':
            self.set_vi(it16, self.top)
        elif t3 == 'xgkick':
            self.kicks.append((vi[is16], self.gif_packet(vi[is16])))
        else:
            raise NotImplementedError(t3)

    def run(self, entry, top, max_steps=2_000_000):
        """Execute from byte address `entry` until the E-bit delay slot completes."""
        self.top = top
        self.kicks = []
        pc, pending, ending = entry, None, False
        for _ in range(max_steps):
            lo, hi = self.code[pc]
            if hi & (1 << 31):
                raise NotImplementedError('I-bit pairs are not used by this microcode')
            uw = self.upper(hi & 0x07FFFFFF)
            lw, target = self.lower(lo, pc)
            for r, k, v in uw + lw:            # both halves read before either writes
                self.vf[r][k] = v
            self.mac_ring = self.mac_ring[1:] + [self.mac]
            nxt = pc + 8
            if pending is not None:
                nxt, pending = pending, None
            if target is not None:
                pending = target
            if ending:
                return self.kicks
            if hi & (1 << 30):
                ending = True
            pc = nxt
        raise RuntimeError('VU1 program did not end within %d steps' % max_steps)

