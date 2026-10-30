"""Translate Road Trip's VU1 microcode image into C++ (stage A of docs/graphics-plan.md).

Each instruction pair becomes one `case` of a switch over the VU program counter, built from the helpers
in tools/common/vu1/vu1_ops.h, which mirror the reference interpreter vusim.py bit for bit. Pair semantics
match vusim: the upper half runs first (ACC/CLIP/MAC/Q update at once), the lower half reads registers
before either half's VF writes land, then the upper writes and the lower writes are committed in that
order, and every pair (nops included) advances the 4-deep MAC flag pipeline that FMAND reads. Branch
delay slots are inlined after the branch; an E-bit pair runs its delay slot and returns.

Usage: python -I scripts/vu/vu2cpp.py   -> tools/common/vu1/vu1_microcode.gen.cpp
"""
import os
import sys

sys.path.insert(0, os.path.dirname(__file__))
from vudis import UPPER, UPPER_FD, LOWER, LOWEROP, T3, LOWER_NOP, sext, disasm_pair  # noqa: E402
from vuflow import load_image  # noqa: E402
from vusim import parse_upper  # noqa: E402

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
OUT = os.path.join(ROOT, 'tools', 'common', 'vu1', 'vu1_microcode.gen.cpp')

OPS = {'add': 'ADD', 'sub': 'SUB', 'mul': 'MUL', 'madd': 'MADD', 'msub': 'MSUB', 'max': 'MAX', 'mini': 'MINI'}
KIND = {'x': 'BC_X', 'y': 'BC_Y', 'z': 'BC_Z', 'w': 'BC_W', 'v': 'VEC', 'q': 'USE_Q', 'i': 'USE_I'}


def mask_of(c):
    """dest bits 24..21 (x..w) -> helper mask with bit k = field k."""
    return sum(1 << k for k in range(4) if c & (1 << (24 - k)))


def fields_of(c):
    return [k for k in range(4) if c & (1 << (24 - k))]


def upper_cpp(c):
    op = c & 0x3F
    ft, fs, fd = (c >> 16) & 31, (c >> 11) & 31, (c >> 6) & 31
    m = mask_of(c)
    name = UPPER[op]
    acc = False
    if name.startswith('FD'):
        name = UPPER_FD[op & 3][(c >> 6) & 31]
        if name == 'nop':
            return ''
        if name == 'clip':
            return 'Clip(s, %d, %d);' % (fs, ft)
        if name.startswith('ftoi'):
            return 'Ftoi(s, pu, %d, %d, %d, %d);' % (ft, fs, int(name[4:]), m)
        if name.startswith('itof'):
            return 'Itof(s, pu, %d, %d, %d, %d);' % (ft, fs, int(name[4:]), m)
        if name == 'abs':
            return 'Abs(s, pu, %d, %d, %d);' % (ft, fs, m)
        if name == 'opmula':
            return 'OpMula(s, %d, %d);' % (fs, ft)
        acc = True
    if name == 'opmsub':
        return 'OpMsub(s, pu, %d, %d, %d);' % (fd, fs, ft)
    base, operand = parse_upper(name)
    if acc:
        base = base[:-1]                       # 'madda' -> 'madd', result goes to ACC
    return 'Arith(s, pu, %s, %d, %d, %d, %s, %d);' % (OPS[base], -1 if acc else fd, fs, ft, KIND[operand], m)


def lower_cpp(c, pc):
    """Return (statements, branch) where branch is None or (kind, cond_expr, target_expr)."""
    if c == LOWER_NOP:
        return '', None
    name = LOWER[(c >> 25) & 0x7F]
    it, is_ = (c >> 16) & 31, (c >> 11) & 31
    it16, is16 = it & 15, is_ & 15
    imm11 = sext(c & 0x7FF, 11)
    m = mask_of(c)
    fl = fields_of(c)
    tgt = pc + 8 + imm11 * 8
    if name == 'lq':
        return 'LoadVf(s, pl, %d, s.vi[%d] + (%d), %d);' % (it, is16, imm11, m), None
    if name == 'sq':
        return 'StoreVf(s, %d, s.vi[%d] + (%d), %d);' % (is_, it16, imm11, m), None
    if name == 'ilw':
        return 'SetVi(s, %d, s.mem[(s.vi[%d] + (%d)) & 0x3FF][%d]);' % (it16, is16, imm11, fl[0]), None
    if name == 'isw':
        return ' '.join('s.mem[(s.vi[%d] + (%d)) & 0x3FF][%d] = s.vi[%d];' % (is16, imm11, k, it16) for k in fl), None
    if name in ('iaddiu', 'isubiu'):
        imm15 = ((c >> 10) & 0x7800) | (c & 0x7FF)
        return 'SetVi(s, %d, s.vi[%d] %s 0x%X);' % (it16, is16, '+' if name == 'iaddiu' else '-', imm15), None
    if name == 'fcand':
        return 'SetVi(s, 1, (s.clip & 0x%06Xu) ? 1 : 0);' % (c & 0xFFFFFF), None
    if name == 'fcor':
        return 'SetVi(s, 1, ((s.clip | 0x%06Xu) & 0xFFFFFFu) == 0xFFFFFFu ? 1 : 0);' % (c & 0xFFFFFF), None
    if name == 'fcget':
        return 'SetVi(s, %d, s.clip & 0xFFF);' % it16, None
    if name == 'fmand':
        return 'SetVi(s, %d, s.macRing[0] & s.vi[%d]);' % (it16, is16), None
    if name == 'b':
        return '', ('b', 'true', '0x%04X' % tgt)
    if name == 'bal':
        return 'SetVi(s, %d, 0x%X);' % (it16, (pc + 16) // 8), ('b', 'true', '0x%04X' % tgt)
    if name == 'jr':
        return '', ('jr', 'true', '(uint32_t)s.vi[%d] * 8' % is16)
    if name in ('ibeq', 'ibne'):
        return '', ('b', 's.vi[%d] %s s.vi[%d]' % (it16, '==' if name == 'ibeq' else '!=', is16), '0x%04X' % tgt)
    if name in ('ibltz', 'ibgtz', 'iblez', 'ibgez'):
        cmp = {'ibltz': '<', 'ibgtz': '>', 'iblez': '<=', 'ibgez': '>='}[name]
        return '', ('b', '(int16_t)s.vi[%d] %s 0' % (is16, cmp), '0x%04X' % tgt)
    if name != 'LOWEROP':
        raise NotImplementedError(name)
    sub = LOWEROP[c & 0x3F]
    id16 = (c >> 6) & 15
    if sub in ('iadd', 'isub', 'iand', 'ior'):
        sym = {'iadd': '+', 'isub': '-', 'iand': '&', 'ior': '|'}[sub]
        return 'SetVi(s, %d, s.vi[%d] %s s.vi[%d]);' % (id16, is16, sym, it16), None
    if sub == 'iaddi':
        return 'SetVi(s, %d, s.vi[%d] + (%d));' % (it16, is16, sext((c >> 6) & 31, 5)), None
    t3 = T3[c & 3][(c >> 6) & 31]
    fsf, ftf = (c >> 21) & 3, (c >> 23) & 3
    if t3 == 'move':
        return 'MoveVf(s, pl, %d, %d, %d, false);' % (it, is_, m), None
    if t3 == 'mr32':
        return 'MoveVf(s, pl, %d, %d, %d, true);' % (it, is_, m), None
    if t3 == 'lqi':
        return 'LoadVf(s, pl, %d, s.vi[%d], %d); SetVi(s, %d, s.vi[%d] + 1);' % (it, is16, m, is16, is16), None
    if t3 == 'sqi':
        return 'StoreVf(s, %d, s.vi[%d], %d); SetVi(s, %d, s.vi[%d] + 1);' % (is_, it16, m, it16, it16), None
    if t3 == 'sqd':
        return 'SetVi(s, %d, s.vi[%d] - 1); StoreVf(s, %d, s.vi[%d], %d);' % (it16, it16, is_, it16, m), None
    if t3 == 'div':
        return 'Div(s, %d, %d, %d, %d);' % (is_, fsf, it, ftf), None
    if t3 == 'waitq':
        return '', None
    if t3 == 'ilwr':
        return 'SetVi(s, %d, s.mem[s.vi[%d] & 0x3FF][%d]);' % (it16, is16, fl[0]), None
    if t3 == 'iswr':
        return ' '.join('s.mem[s.vi[%d] & 0x3FF][%d] = s.vi[%d];' % (is16, k, it16) for k in fl), None
    if t3 == 'xtop':
        return 'SetVi(s, %d, s.top);' % it16, None
    if t3 == 'xgkick':
        return 'Kick(s, s.vi[%d], sink);' % is16, None
    raise NotImplementedError(t3)


def pair_block(image, pc, indent):
    """C++ for one pair without control transfer; returns (code_lines, branch, is_end)."""
    lo, hi = image[pc]
    if hi & (1 << 31):
        raise NotImplementedError('I-bit pair at 0x%04X' % pc)
    up = upper_cpp(hi & 0x07FFFFFF)
    low, br = lower_cpp(lo, pc)
    u_txt, l_txt, flags, _t = disasm_pair(lo, hi, pc)
    pad = ' ' * indent
    lines = ['%s// %04X %-4s %s | %s' % (pad, pc, '[' + flags + ']' if flags else '', u_txt, l_txt)]
    body = []
    if up:
        body.append(up)
    if low:
        body.append(low)
    if body:
        lines.append('%s{ Pending pu, pl; %s Commit(s, pu); Commit(s, pl); } PushMac(s);' % (pad, ' '.join(body)))
    else:
        lines.append('%sPushMac(s);' % pad)   # even nop pairs advance the MAC flag pipeline
    return lines, br, bool(hi & (1 << 30))


def generate():
    image, _vif = load_image()
    pcs = sorted(image)
    out = ['// GENERATED by scripts/vu/vu2cpp.py from the VU1 microcode in SLUS_203.98 .vutext. Do not edit.',
           '// One case per instruction pair; see vu1_ops.h for the helpers and their semantics.',
           '#include "vu1_ops.h"', '', 'namespace vu1 {', '',
           'bool RunGenerated(State& s, uint32_t entry, GifSink& sink) {',
           '    using namespace ops;',
           '    uint32_t pc = entry;',
           '    for (int guard = 0; guard < 2000000; ++guard) {',
           '        switch (pc) {']
    for pc in pcs:
        lines, br, end = pair_block(image, pc, 12)
        out.append('        case 0x%04X: {' % pc)
        if br is None and not end:
            out.extend(lines)
            out.append('        } [[fallthrough]];')
            continue
        # Control transfer: evaluate the condition/target before the pair's writes land (as vusim does,
        # the lower half reads registers first), run the delay slot, then jump or stop.
        if br is not None:
            kind, cond, target = br
            out.append('            bool tk = %s; uint32_t tg = %s;' % (cond, target))
        out.extend(lines)
        nxt = pc + 8
        if nxt not in image:
            raise RuntimeError('delay slot outside the image at 0x%04X' % pc)
        dl, dbr, dend = pair_block(image, nxt, 12)
        if dbr is not None or dend:
            raise NotImplementedError('control transfer in a delay slot at 0x%04X' % nxt)
        out.append('            // delay slot')
        out.extend(dl)
        if end:
            out.append('            return true;')
        else:
            out.append('            pc = tk ? tg : 0x%04X; continue;' % (pc + 16))
        out.append('        }')
    out += ['        default:', '            return false;', '        }',
            '        return false;   // ran off the end of the image', '    }', '    return false;', '}', '',
            '}  // namespace vu1', '']
    return '\n'.join(out)


def main():
    text = generate()
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, 'w', encoding='utf-8', newline='\n') as fp:
        fp.write(text)
    print('wrote %s (%d lines)' % (os.path.relpath(OUT, ROOT), text.count('\n')))


if __name__ == '__main__':
    sys.exit(main())
