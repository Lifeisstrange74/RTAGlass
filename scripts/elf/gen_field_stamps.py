"""Writes src/game/field/field_stamps.inc from the ELF: the stamps' texts D_002DBBA0, 100 string pointers (stamp n is
entry n - 1), each text's lines starting after a '#'. The .inc is the body of a string array, one text a line.

    python -I scripts/elf/gen_field_stamps.py
"""
import os
import struct

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..')
# The extracted disc: OriginalGameFiles next to the repo, or the folder in RTAGLASS_GAME.
GAME = os.environ.get('RTAGLASS_GAME', os.path.join(ROOT, '..', 'OriginalGameFiles'))
ELF = os.path.join(GAME, 'SLUS_203.98')
OUT = os.path.join(ROOT, 'src', 'game', 'field', 'field_stamps.inc')
TABLE = 0x002DBBA0
STAMPS = 100

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
    b = rd(va, 256).split(b'\0')[0]
    return ''.join(chr(c) if 32 <= c < 127 and c not in (34, 92) else '\\%03o' % c for c in b)


lines = ['// D_002DBBA0: the stamps\' texts (stamp n = entry n - 1), lines after each \'#\'. Dumped from the ELF.']
for p in struct.unpack('<%dI' % STAMPS, rd(TABLE, 4 * STAMPS)):
    lines.append('"%s",' % cstr(p))
open(OUT, 'w', newline='\n').write('\n'.join(lines) + '\n')
print('wrote', OUT)
