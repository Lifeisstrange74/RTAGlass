# Writes src/game/story/parts_tables.inc from the ELF: python -I gen_parts_tables.py OUT.inc
import struct, sys
import os
GAME = os.environ.get('RTAGLASS_GAME', os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..',
                                                  'OriginalGameFiles'))
b = open(os.path.join(GAME, 'SLUS_203.98'), 'rb').read()
phoff, = struct.unpack_from('<I', b, 0x1C)
phnum, = struct.unpack_from('<H', b, 0x2C)
segs = []
for i in range(phnum):
    t, off, va, pa, fs, ms = struct.unpack_from('<6I', b, phoff + i * 32)
    if t == 1: segs.append((va, off, fs))
def at(addr):
    for va, off, fs in segs:
        if va <= addr < va + fs: return off + addr - va
    return None
def u32(a): return struct.unpack_from('<I', b, at(a))[0]
def s32(a): return struct.unpack_from('<i', b, at(a))[0]
def u16(a): return struct.unpack_from('<H', b, at(a))[0]
def s16(a): return struct.unpack_from('<h', b, at(a))[0]
def u8(a): return b[at(a)]
def s8(a): return struct.unpack_from('<b', b, at(a))[0]
def f32(a): return struct.unpack_from('<f', b, at(a))[0]
def cstr(a):
    o = at(a)
    if o is None: return None
    e = b.index(b'\0', o)
    return b[o:e].decode('latin-1')
T = {1: (0x2ECA70, 0x1C), 2: (0x2ECED0, 0x18), 3: (0x2ED1F8, 0x10), 4: (0x2ED328, 0x1C), 5: (0x2ED4E8, 0x10),
     6: (0x2ED5D0, 0x2C), 7: (0x2EFFD8, 0xC), 8: (0x2EFF40, 0x10), 9: (0x2F0280, 0xC), 10: (0x2F02F8, 0x10),
     11: (0x2F03E0, 0x14), 12: (0x2F07F8, 0xC), 13: (0x2F00A0, 0x10), 14: (0x2F06C0, 0xC)}
COUNT = {1: 13, 2: 12, 3: 5, 4: 6, 5: 4, 6: 4, 7: 15, 8: 3, 9: 2, 10: 3, 11: 9, 12: 2, 13: 15, 14: 11}
NODESC = (7, 12, 13, 14)


def lit(s):
    out = '"'
    for ch in s:
        c = ord(ch)
        if ch in '"\\':
            out += '\\' + ch
        elif 0x20 <= c < 0x7F:
            out += ch
        else:
            out += '\\x%02X""' % c
    return out + '"'


def lines4(p):
    out = []
    for _ in range(4):
        s = cstr(p)
        out.append(s)
        p += len(s.encode('latin-1')) + 1
    return out


o = []
o.append('// Generated from the ELF (SLUS_203.98) by scripts/gen_parts_tables.py: each category\'s items (the tables of')
o.append('// func_00243468 / func_00243848, see docs/parts.md): the name, the four description lines func_0022E960')
o.append('// prints (read on from the item\'s description pointer, as the game does), and the value func_0023C418 uses:')
o.append('// tires +0x1A (1 Big), engines +0x14 (type), Special Parts +0xC (part flags), Options +0xC (part flags) with')
o.append('// +0x10 (the billboard\'s sponsor) in bits 16-23, horns +0xC (the SE); and the price')
o.append('// (+8, func_00243670).')
o.append('struct PartItem {')
o.append('    const char* name;')
o.append('    const char* desc[4];')
o.append('    int value;')
o.append('    int price;')
o.append('};')
o.append('')
for k in range(1, 15):
    base, st = T[k]
    o.append('const PartItem kParts%d[%d] = {' % (k, COUNT[k]))
    for i in range(COUNT[k]):
        a = base + i * st
        name = cstr(u32(a))
        if k in NODESC:
            desc = ['', '', '', '']
        else:
            desc = lines4(u32(a + 4))
        v = 0
        if k == 1: v = u8(a + 0x1A)
        elif k == 2: v = s32(a + 0x14)
        elif k == 10: v = u32(a + 0xC)
        elif k == 11: v = (u32(a + 0xC) & 0xFFFF) | (u32(a + 0x10) & 0xFF) << 16   # the asm: lhu +0xC, lbu +0x10
        elif k == 13: v = u32(a + 0xC)
        o.append('    {%s, {%s}, 0x%X, %d},' % (lit(name), ', '.join(lit(x) for x in desc), v, s32(a + 8)))
    o.append('};')
o.append('const PartItem* const kPartTables[15] = {nullptr, ' + ', '.join('kParts%d' % k for k in range(1, 8)) + ',')
o.append(' ' * 41 + ', '.join('kParts%d' % k for k in range(8, 15)) + '};')
o.append('const int kPartCounts[15] = {151, ' + ', '.join(str(COUNT[k]) for k in range(1, 15)) + '};')
o.append('')
o.append('// The bodies\' names (D_002ED720 + 0x44 x body, +0).')
o.append('const char* const kBodyNames[151] = {')
row = []
for i in range(151):
    row.append(lit(cstr(u32(0x2ED720 + i * 0x44))))
line = '   '
for r in row:
    if len(line) + len(r) + 2 > 118:
        o.append(line)
        line = '   '
    line += ' ' + r + ','
o.append(line)
o.append('};')
o.append('')
o.append('// The shops\' stock by town 0-9 (func_00267F48 / func_002681A8): the Body Shops\' bodies, a 160-bit mask each')
o.append('// (D_002DC460 + 0x14 x town), and the Parts Shops\' parts, a mask of items for each category 1-14 (D_002DC528 +')
o.append('// 0x40 x town + 4 x category). My City\'s, town 9, also has the stock of the shops found in other towns and what')
o.append('// the story unlocks (MyCity_ShopStockAdd, func_00267C30).')
o.append('const uint32_t kShopBodies[10][5] = {')
for t in range(10):
    o.append('    {' + ', '.join('0x%08X' % u32(0x2DC460 + t * 20 + 4 * j) for j in range(5)) + '},')
o.append('};')
o.append('const uint32_t kShopParts[10][15] = {')
for t in range(10):
    o.append('    {' + ', '.join('0x%X' % u32(0x2DC528 + (t * 16 + c) * 4) for c in range(15)) + '},')
o.append('};')
open(sys.argv[1], 'w', newline='\n').write('\n'.join(o) + '\n')
