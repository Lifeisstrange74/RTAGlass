"""Writes src/game/race/car_jets.inc from the ELF: each body's right jet nozzle in the car's frame (x right, y up, z
forward), f32 (x, y, z) at +0x38 of the body table D_002ED720 (0x44 bytes a body, 151 bodies). Car_DrawJet
(func_002238A0) puts the left one at -x.

    python -I scripts/elf/gen_car_jets.py
"""
import os
import struct

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..')
# The extracted disc: OriginalGameFiles next to the repo, or the folder in RTAGLASS_GAME.
GAME = os.environ.get('RTAGLASS_GAME', os.path.join(ROOT, '..', 'OriginalGameFiles'))
ELF = os.path.join(GAME, 'SLUS_203.98')
OUT = os.path.join(ROOT, 'src', 'game', 'race', 'car_jets.inc')
BODIES = 0x002ED720
STRIDE = 0x44
COUNT = 151
WIDTH = 120

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


lines = ['// Generated from the ELF (SLUS_203.98): each body\'s right jet nozzle in the car\'s frame (x right, y up, z',
         '// forward), D_002ED720 + 0x44 x body, +0x38 (Car_DrawJet, func_002238A0). The left one is at -x.',
         'const float kJetNozzle[%d][3] = {' % COUNT]
line = '   '
for b in range(COUNT):
    item = '{%s},' % ', '.join(f(v) for v in struct.unpack('<3f', rd(BODIES + STRIDE * b + 0x38, 12)))
    # Fill each line up to WIDTH columns.
    if len(line) + 1 + len(item) > WIDTH:
        lines.append(line)
        line = '   '
    line += ' ' + item
lines.append(line)
lines.append('};')
open(OUT, 'w', newline='\n').write('\n'.join(lines) + '\n')
print('wrote', OUT)
