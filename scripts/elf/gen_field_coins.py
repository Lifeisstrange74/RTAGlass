"""Writes src/game/field/field_coins.inc from the ELF: the 100 ChoroQ coins D_002A3970, 16 bytes a coin: f32 (x, y, z)
in their world block, then s32 the block (FieldWorld ids). Read by Field_CheckCoins 0x00241BA8, func_00241D38 and
func_00241E90.

    python -I scripts/elf/gen_field_coins.py
"""
import os
import struct

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..')
# The extracted disc: OriginalGameFiles next to the repo, or the folder in RTAGLASS_GAME.
GAME = os.environ.get('RTAGLASS_GAME', os.path.join(ROOT, '..', 'OriginalGameFiles'))
ELF = os.path.join(GAME, 'SLUS_203.98')
OUT = os.path.join(ROOT, 'src', 'game', 'field', 'field_coins.inc')
TABLE = 0x002A3970
COINS = 100
PER_LINE = 3

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
    # Nine significant digits (enough to round-trip any float32), always with a decimal point.
    s = '%.9g' % v
    return (s if '.' in s else s + '.0') + 'f'


lines = ['// D_002A3970: the 100 ChoroQ coins, {x, y, z} in their world block and the block (FieldWorld ids). Read from',
         '// SLUS_203.98 (Field_CheckCoins 0x00241BA8, func_00241D38, func_00241E90).',
         'const FieldCoin kCoins[%d] = {' % COINS]
items = []
for i in range(COINS):
    x, y, z, block = struct.unpack('<3fi', rd(TABLE + 16 * i, 16))
    items.append('{{%s, %s, %s}, %d},' % (f(x), f(y), f(z), block))
for i in range(0, COINS, PER_LINE):
    lines.append('    ' + ' '.join(items[i:i + PER_LINE]))
lines.append('};')
open(OUT, 'w', newline='\n').write('\n'.join(lines) + '\n')
print('wrote', OUT)
