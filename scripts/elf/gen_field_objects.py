"""Writes src/game/field/field_objects.inc from the ELF: the field's object places, 16 bytes a place: f32 (x, y, z) in
their block's units, then a f32 1.
- D_002C4250: block 39's 22 windmill sails (func_0025D5F8).
- D_002C43C0: block 40's 107 palms, where their three fronds hang (func_0025CC18).
- D_002C4A70: block 42's 6 palms (func_0025CE20).

    python -I scripts/elf/gen_field_objects.py
"""
import os
import struct

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..')
# The extracted disc: OriginalGameFiles next to the repo, or the folder in RTAGLASS_GAME.
GAME = os.environ.get('RTAGLASS_GAME', os.path.join(ROOT, '..', 'OriginalGameFiles'))
ELF = os.path.join(GAME, 'SLUS_203.98')
OUT = os.path.join(ROOT, 'src', 'game', 'field', 'field_objects.inc')
PER_LINE = 4
TABLES = [
    ('// D_002C4250: block 39\'s windmill sails (func_0025D5F8)', 'kSailPlaces', 0x002C4250, 22),
    ('// D_002C43C0: block 40\'s palms, where their three fronds hang (func_0025CC18)', 'kPalmPlaces40', 0x002C43C0, 107),
    ('// D_002C4A70: block 42\'s palms (func_0025CE20)', 'kPalmPlaces42', 0x002C4A70, 6),
]

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


lines = ['// The field\'s object places, from the executable (positions in their block\'s units). Included by',
         '// field_objects.cpp.']
for comment, name, va, n in TABLES:
    lines += ['', comment, 'const float %s[%d][3] = {' % (name, n)]
    items = ['{%s},' % ', '.join(f(v) for v in struct.unpack('<3f', rd(va + 16 * i, 12))) for i in range(n)]
    for i in range(0, n, PER_LINE):
        lines.append('    ' + ' '.join(items[i:i + PER_LINE]))
    lines.append('};')
open(OUT, 'w', newline='\n').write('\n'.join(lines) + '\n')
print('wrote', OUT)
