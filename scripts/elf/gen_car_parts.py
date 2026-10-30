"""Writes src/game/race/car_parts.inc from the ELF: where each body carries its parts, at +0x08 of the body table
D_002ED720 (0x44 bytes a body, 151 bodies): six f32 (y, z) places in the body's frame (x 0), for parts 0/1, 2/3, 4,
5/9, 6 and 8 (Car_Draw, 0x00221788).

    python -I scripts/elf/gen_car_parts.py
"""
import os
import struct

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..')
# The extracted disc: OriginalGameFiles next to the repo, or the folder in RTAGLASS_GAME.
GAME = os.environ.get('RTAGLASS_GAME', os.path.join(ROOT, '..', 'OriginalGameFiles'))
ELF = os.path.join(GAME, 'SLUS_203.98')
OUT = os.path.join(ROOT, 'src', 'game', 'race', 'car_parts.inc')
BODIES = 0x002ED720
STRIDE = 0x44
COUNT = 151

elf = open(ELF, 'rb').read()
phoff, = struct.unpack_from('<I', elf, 0x1C)
count, = struct.unpack_from('<H', elf, 0x2C)
segs = [struct.unpack_from('<6I', elf, phoff + 32 * i)[1:5] for i in range(count)]


def rd(va, n):
    for off, vaddr, _pa, size in segs:
        if vaddr <= va < vaddr + size:
            return elf[off + va - vaddr:off + va - vaddr + n]
    raise ValueError(hex(va))


def f(v):
    # Four decimals, trailing zeros dropped (but always one digit after the point).
    s = ('%.4f' % v).rstrip('0')
    return (s + '0' if s.endswith('.') else s) + 'f'


lines = ['// Generated from the ELF (SLUS_203.98): where each body carries its parts, D_002ED720 + 0x44 x body, +0x08:',
         '// six (y, z) places in the body\'s frame (x 0), for parts 0/1, 2/3, 4, 5/9, 6 and 8 (Car_Draw, 0x00221788).',
         'const float kPartPlace[%d][6][2] = {' % COUNT]
for b in range(COUNT):
    v = struct.unpack('<12f', rd(BODIES + STRIDE * b + 0x08, 48))
    lines.append('    {%s},' % ', '.join('{%s, %s}' % (f(v[2 * k]), f(v[2 * k + 1])) for k in range(6)))
lines.append('};')
open(OUT, 'w', newline='\n').write('\n'.join(lines) + '\n')
print('wrote', OUT)
