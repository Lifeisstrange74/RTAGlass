"""Writes src/game/race/golf_tables.inc from the ELF: Golf's (course 0x17, race 43) tables.
- D_002AC0B0: 9 holes of 64 bytes: +0 the car's start (x, y, z), +0x10 the tee (the ball), +0x20 the cup, +0x30 s16 the
  car's yaw, +0x32 s16 par.
- D_002AC2F0: the ball's drag in the air, then (horizontal, vertical) bounce by surface type 0-6.
- D_002AC458: each hole's description for its card, a list of strings ending in a null pointer.

    python -I scripts/elf/gen_golf.py
"""
import os
import struct

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..')
# The extracted disc: OriginalGameFiles next to the repo, or the folder in RTAGLASS_GAME.
GAME = os.environ.get('RTAGLASS_GAME', os.path.join(ROOT, '..', 'OriginalGameFiles'))
ELF = os.path.join(GAME, 'SLUS_203.98')
OUT = os.path.join(ROOT, 'src', 'game', 'race', 'golf_tables.inc')
HOLES = 9

elf = open(ELF, 'rb').read()
phoff, = struct.unpack_from('<I', elf, 0x1C)
count, = struct.unpack_from('<H', elf, 0x2C)
segs = [struct.unpack_from('<6I', elf, phoff + 32 * i)[1:5] for i in range(count)]


def rd(va, n):
    for off, vaddr, _pa, size in segs:
        if vaddr <= va < vaddr + size:
            return elf[off + va - vaddr:off + va - vaddr + n]
    raise ValueError(hex(va))


def cstr(va):
    s = rd(va, 128)
    return s[:s.index(b'\0')].decode('latin-1')


def g(v):
    return '%gf' % v if v != int(v) else '%d' % v


o = ['// Made by scripts/elf/gen_golf.py from the ELF; see that script for what each table is.', '']
o.append('const GolfHole kHoles[%d] = {' % HOLES)
for h in range(HOLES):
    b = rd(0x002AC0B0 + 64 * h, 64)
    car = struct.unpack_from('<3f', b, 0)
    tee = struct.unpack_from('<3f', b, 0x10)
    cup = struct.unpack_from('<3f', b, 0x20)
    yaw, par = struct.unpack_from('<hh', b, 0x30)
    o.append('    {{%s}, {%s}, {%s}, %d, %d},' % (', '.join(g(v) for v in car), ', '.join(g(v) for v in tee),
                                                ', '.join(g(v) for v in cup), yaw, par))
o.append('};')
drag = struct.unpack('<16f', rd(0x002AC2F0, 64))
o.append('// D_002AC2F0: the air\'s drag, then (horizontal, vertical) by surface type.')
o.append('const float kAirDrag = %s;' % g(round(drag[0], 6)))
pairs = ['{%s, %s}' % (g(round(drag[2 + 2 * k], 6)), g(round(drag[3 + 2 * k], 6))) for k in range(7)]
o.append('const float kBounce[7][2] = {')
o.append('    ' + ', '.join(pairs[:4]) + ',')
o.append('    ' + ', '.join(pairs[4:]) + ',')
o.append('};')
o.append('// D_002AC458: the holes\' descriptions.')
o.append('const char* const kHoleText[%d][8] = {' % HOLES)
ptrs = struct.unpack('<%dI' % HOLES, rd(0x002AC458, 4 * HOLES))
for h in range(HOLES):
    strs = []
    p = ptrs[h]
    while True:
        s, = struct.unpack('<I', rd(p, 4))
        if not s:
            break
        strs.append('"%s"' % cstr(s).replace('\\', '\\\\').replace('"', '\\"'))
        p += 4
    o.append('    {%s},' % ', '.join(strs))
o.append('};')
open(OUT, 'w', newline='\n').write('\n'.join(o) + '\n')
print('wrote', OUT)
