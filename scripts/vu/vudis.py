"""VU micro-mode disassembler (VU0/VU1 64-bit instruction pairs).

Opcode tables follow PCSX2 pcsx2/VUops.cpp (_vuTablesMess): the lower instruction is dispatched on
bits 31..25, the upper on bits 5..0, and both have sub-tables. Operand layouts follow the VU User's Manual.
Output syntax is close to Sony's dvp-as / openvcl, e.g.  "mulax.xyzw ACC, vf04, vf08x".

disasm_pair(lower, upper, pc) -> (upper_text, lower_text, flags, branch_target_or_None)
"""
import struct

UPPER = [
    'addx', 'addy', 'addz', 'addw', 'subx', 'suby', 'subz', 'subw',
    'maddx', 'maddy', 'maddz', 'maddw', 'msubx', 'msuby', 'msubz', 'msubw',
    'maxx', 'maxy', 'maxz', 'maxw', 'minix', 'miniy', 'miniz', 'miniw',
    'mulx', 'muly', 'mulz', 'mulw', 'mulq', 'maxi', 'muli', 'minii',
    'addq', 'maddq', 'addi', 'maddi', 'subq', 'msubq', 'subi', 'msubi',
    'add', 'madd', 'mul', 'max', 'sub', 'msub', 'opmsub', 'mini',
] + [None] * 12 + ['FD00', 'FD01', 'FD10', 'FD11']
UPPER_FD = {
    0: ['addax', 'subax', 'maddax', 'msubax', 'itof0', 'ftoi0', 'mulax', 'mulaq', 'addaq', 'subaq', 'adda', 'suba'],
    1: ['adday', 'subay', 'madday', 'msubay', 'itof4', 'ftoi4', 'mulay', 'abs', 'maddaq', 'msubaq', 'madda', 'msuba'],
    2: ['addaz', 'subaz', 'maddaz', 'msubaz', 'itof12', 'ftoi12', 'mulaz', 'mulai', 'addai', 'subai', 'mula', 'opmula'],
    3: ['addaw', 'subaw', 'maddaw', 'msubaw', 'itof15', 'ftoi15', 'mulaw', 'clip', 'maddai', 'msubai', None, 'nop'],
}
LOWER = {0x00: 'lq', 0x01: 'sq', 0x04: 'ilw', 0x05: 'isw', 0x08: 'iaddiu', 0x09: 'isubiu',
         0x10: 'fceq', 0x11: 'fcset', 0x12: 'fcand', 0x13: 'fcor', 0x14: 'fseq', 0x15: 'fsset', 0x16: 'fsand',
         0x17: 'fsor', 0x18: 'fmeq', 0x1A: 'fmand', 0x1B: 'fmor', 0x1C: 'fcget',
         0x20: 'b', 0x21: 'bal', 0x24: 'jr', 0x25: 'jalr', 0x28: 'ibeq', 0x29: 'ibne',
         0x2C: 'ibltz', 0x2D: 'ibgtz', 0x2E: 'iblez', 0x2F: 'ibgez', 0x40: 'LOWEROP'}
LOWEROP = {0x30: 'iadd', 0x31: 'isub', 0x32: 'iaddi', 0x34: 'iand', 0x35: 'ior',
           0x3C: 'T3_00', 0x3D: 'T3_01', 0x3E: 'T3_10', 0x3F: 'T3_11'}
T3 = {
    0: {0x0C: 'move', 0x0D: 'lqi', 0x0E: 'div', 0x0F: 'mtir', 0x10: 'rnext', 0x19: 'mfp', 0x1A: 'xtop',
        0x1B: 'xgkick', 0x1C: 'esadd', 0x1D: 'eatanxy', 0x1E: 'esqrt', 0x1F: 'esin'},
    1: {0x0C: 'mr32', 0x0D: 'sqi', 0x0E: 'sqrt', 0x0F: 'mfir', 0x10: 'rget', 0x1A: 'xitop',
        0x1C: 'ersadd', 0x1D: 'eatanxz', 0x1E: 'ersqrt', 0x1F: 'eatan'},
    2: {0x0D: 'lqd', 0x0E: 'rsqrt', 0x0F: 'ilwr', 0x10: 'rinit', 0x1C: 'eleng', 0x1D: 'esum', 0x1E: 'ercpr',
        0x1F: 'eexp'},
    3: {0x0D: 'sqd', 0x0E: 'waitq', 0x0F: 'iswr', 0x10: 'rxor', 0x1C: 'erleng', 0x1E: 'waitp'},
}
LOWER_NOP = 0x8000033C
BRANCHES = {'b', 'bal', 'ibeq', 'ibne', 'ibltz', 'ibgtz', 'iblez', 'ibgez'}
JUMPS = {'jr', 'jalr'}
FIELD = 'xyzw'


def dest(c):
    d = ''.join(FIELD[i] for i in range(4) if c & (1 << (24 - i)))
    return '.' + d if d else ''


def fsf(c):
    return FIELD[(c >> 21) & 3]


def ftf(c):
    return FIELD[(c >> 23) & 3]


def sext(v, bits):
    return v - (1 << bits) if v & (1 << (bits - 1)) else v


def vf(n):
    return 'vf%02d' % n


def vi(n):
    return 'vi%02d' % n


def disasm_upper(c):
    op = c & 0x3F
    ft, fs, fd = (c >> 16) & 31, (c >> 11) & 31, (c >> 6) & 31
    bc = FIELD[c & 3]
    name = UPPER[op]
    if name is None:
        return '???upper %08X' % c
    if name.startswith('FD'):
        sub = UPPER_FD[op & 3]
        idx = (c >> 6) & 31
        name = sub[idx] if idx < len(sub) else None
        if name is None:
            return '???upperFD %08X' % c
        if name == 'nop':
            return 'nop'
        d = dest(c)
        if name.startswith(('itof', 'ftoi')):
            return '%s%s %s, %s' % (name, d, vf(ft), vf(fs))
        if name == 'abs':
            return 'abs%s %s, %s' % (d, vf(ft), vf(fs))
        if name == 'clip':
            return 'clipw.xyz %s, %sw' % (vf(fs), vf(ft))
        if name[-1] in 'xyzw' and name not in ('mula', 'adda', 'suba', 'madda', 'msuba'):
            return '%s%s ACC, %s, %s%s' % (name, d, vf(fs), vf(ft), name[-1])
        if name[-1] in 'qi':
            return '%s%s ACC, %s, %s' % (name, d, vf(fs), 'Q' if name[-1] == 'q' else 'I')
        if name == 'opmula':
            return 'opmula.xyz ACC, %s, %s' % (vf(fs), vf(ft))
        return '%s%s ACC, %s, %s' % (name, d, vf(fs), vf(ft))
    d = dest(c)
    if op < 0x1C:   # broadcast forms
        return '%s%s %s, %s, %s%s' % (name, d, vf(fd), vf(fs), vf(ft), bc)
    if name[-1] in 'qi' and name not in ('mini', 'maxi'):
        return '%s%s %s, %s, %s' % (name, d, vf(fd), vf(fs), 'Q' if name[-1] == 'q' else 'I')
    if name in ('maxi', 'minii'):
        return '%s%s %s, %s, I' % (name, d, vf(fd), vf(fs))
    if name == 'opmsub':
        return 'opmsub.xyz %s, %s, %s' % (vf(fd), vf(fs), vf(ft))
    return '%s%s %s, %s, %s' % (name, d, vf(fd), vf(fs), vf(ft))


def disasm_lower(c, pc):
    """pc is the byte address (in VU micro memory) of this instruction pair."""
    if c == LOWER_NOP:
        return 'nop', None
    op = (c >> 25) & 0x7F
    name = LOWER.get(op)
    it, is_ = (c >> 16) & 31, (c >> 11) & 31
    imm11 = sext(c & 0x7FF, 11)
    d = dest(c)
    if name is None:
        return '???lower %08X' % c, None
    if name in BRANCHES:
        target = pc + 8 + imm11 * 8
        if name == 'b':
            return 'b 0x%04X' % target, target
        if name == 'bal':
            return 'bal %s, 0x%04X' % (vi(it), target), target
        if name in ('ibeq', 'ibne'):
            return '%s %s, %s, 0x%04X' % (name, vi(it), vi(is_), target), target
        return '%s %s, 0x%04X' % (name, vi(is_), target), target
    if name == 'jr':
        return 'jr %s' % vi(is_), None
    if name == 'jalr':
        return 'jalr %s, %s' % (vi(it), vi(is_)), None
    if name == 'lq':
        return 'lq%s %s, %d(%s)' % (d, vf(it), imm11, vi(is_)), None
    if name == 'sq':
        return 'sq%s %s, %d(%s)' % (d, vf(is_), imm11, vi(it)), None
    if name in ('ilw', 'isw'):
        return '%s%s %s, %d(%s)' % (name, d, vi(it), imm11, vi(is_)), None
    if name in ('iaddiu', 'isubiu'):
        imm15 = ((c >> 10) & 0x7800) | (c & 0x7FF)
        return '%s %s, %s, 0x%X' % (name, vi(it), vi(is_), imm15), None
    if name in ('fceq', 'fcset', 'fcand', 'fcor'):
        return '%s vi01, 0x%06X' % (name, c & 0xFFFFFF) if name != 'fcset' else 'fcset 0x%06X' % (c & 0xFFFFFF), None
    if name in ('fseq', 'fsset', 'fsand', 'fsor'):
        imm12 = ((c >> 10) & 0x800) | (c & 0x7FF)
        return ('%s %s, 0x%03X' % (name, vi(it), imm12)) if name != 'fsset' else 'fsset 0x%03X' % imm12, None
    if name in ('fmeq', 'fmand', 'fmor'):
        return '%s %s, %s' % (name, vi(it), vi(is_)), None
    if name == 'fcget':
        return 'fcget %s' % vi(it), None
    # LowerOP class
    sub = LOWEROP.get(c & 0x3F)
    if sub is None:
        return '???lowerop %08X' % c, None
    if sub in ('iadd', 'isub', 'iand', 'ior'):
        return '%s %s, %s, %s' % (sub, vi((c >> 6) & 31), vi(is_), vi(it)), None
    if sub == 'iaddi':
        return 'iaddi %s, %s, %d' % (vi(it), vi(is_), sext((c >> 6) & 31, 5)), None
    t3 = T3[c & 3].get((c >> 6) & 31)
    if t3 is None:
        return '???t3 %08X' % c, None
    ft, fs = it, is_
    return {
        'move': lambda: 'move%s %s, %s' % (d, vf(ft), vf(fs)),
        'mr32': lambda: 'mr32%s %s, %s' % (d, vf(ft), vf(fs)),
        'lqi': lambda: 'lqi%s %s, (%s++)' % (d, vf(ft), vi(is_)),
        'sqi': lambda: 'sqi%s %s, (%s++)' % (d, vf(fs), vi(it)),
        'lqd': lambda: 'lqd%s %s, (--%s)' % (d, vf(ft), vi(is_)),
        'sqd': lambda: 'sqd%s %s, (--%s)' % (d, vf(fs), vi(it)),
        'div': lambda: 'div Q, %s%s, %s%s' % (vf(fs), fsf(c), vf(ft), ftf(c)),
        'sqrt': lambda: 'sqrt Q, %s%s' % (vf(ft), ftf(c)),
        'rsqrt': lambda: 'rsqrt Q, %s%s, %s%s' % (vf(fs), fsf(c), vf(ft), ftf(c)),
        'waitq': lambda: 'waitq',
        'waitp': lambda: 'waitp',
        'mtir': lambda: 'mtir %s, %s%s' % (vi(it), vf(fs), fsf(c)),
        'mfir': lambda: 'mfir%s %s, %s' % (d, vf(ft), vi(is_)),
        'ilwr': lambda: 'ilwr%s %s, (%s)' % (d, vi(it), vi(is_)),
        'iswr': lambda: 'iswr%s %s, (%s)' % (d, vi(it), vi(is_)),
        'mfp': lambda: 'mfp%s %s, P' % (d, vf(ft)),
        'xtop': lambda: 'xtop %s' % vi(it),
        'xitop': lambda: 'xitop %s' % vi(it),
        'xgkick': lambda: 'xgkick %s' % vi(is_),
        'rnext': lambda: 'rnext%s %s, R' % (d, vf(ft)),
        'rget': lambda: 'rget%s %s, R' % (d, vf(ft)),
        'rinit': lambda: 'rinit R, %s%s' % (vf(fs), fsf(c)),
        'rxor': lambda: 'rxor R, %s%s' % (vf(fs), fsf(c)),
    }.get(t3, lambda: efu(t3, fs, c))(), None


def efu(name, fs, c):
    # EFU ops: the vector forms take a whole register, the scalar forms take one field.
    if name in ('esadd', 'ersadd', 'eleng', 'erleng', 'esum'):
        return '%s P, %s' % (name, vf(fs))
    return '%s P, %s%s' % (name, vf(fs), fsf(c))


def disasm_pair(lower, upper, pc):
    flags = ''.join(f for f, bit in (('I', 31), ('E', 30), ('M', 29), ('D', 28), ('T', 27)) if upper & (1 << bit))
    up = disasm_upper(upper & 0x07FFFFFF)
    if 'I' in flags:
        fval = struct.unpack('<f', struct.pack('<I', lower))[0]
        return up, 'loi %g' % fval, flags, None
    lo, target = disasm_lower(lower, pc)
    return up, lo, flags, target
