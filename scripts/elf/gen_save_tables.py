"""Writes src/game/save/save_tables.inc from the ELF (see docs/save.md):
- the u32 at +4 of the body table D_002ED720 (0x44 bytes a body, 151 bodies): each body's own paint, the colour
  table's defaults (func_00229FD8).
- D_002A9B80 + 0x100 x D_002AC098[i], i 0-15: the default speed tables (0x100 bytes, one a zone) that
  Ai_CopySpeedTables (0x00250DD0) gives each teammate.
The file has CRLF line endings.

    python -I scripts/elf/gen_save_tables.py
"""
import os
import struct

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..')
# The extracted disc: OriginalGameFiles next to the repo, or the folder in RTAGLASS_GAME.
GAME = os.environ.get('RTAGLASS_GAME', os.path.join(ROOT, '..', 'OriginalGameFiles'))
ELF = os.path.join(GAME, 'SLUS_203.98')
OUT = os.path.join(ROOT, 'src', 'game', 'save', 'save_tables.inc')
BODIES = 0x002ED720
STRIDE = 0x44
COUNT = 151
SPEEDS = 0x002A9B80
SPEED_INDEX = 0x002AC098
TEAMMATES = 16

elf = open(ELF, 'rb').read()
phoff, = struct.unpack_from('<I', elf, 0x1C)
count, = struct.unpack_from('<H', elf, 0x2C)
segs = [struct.unpack_from('<6I', elf, phoff + 32 * i)[1:5] for i in range(count)]


def rd(va, n):
    for off, vaddr, _pa, size in segs:
        if vaddr <= va < vaddr + size:
            return elf[off + va - vaddr:off + va - vaddr + n]
    raise ValueError(hex(va))


lines = ['// Generated from the executable (SLUS_203.98) by the save survey; see docs/save.md.',
         '',
         '// D_002ED720 + 4 (0x44 a body): each body\'s own paint, the colour table\'s defaults (func_00229FD8).',
         'const uint32_t kBodyPaint[%d] = {' % COUNT]
paints = ['0x%06X,' % struct.unpack('<I', rd(BODIES + STRIDE * b + 4, 4))[0] for b in range(COUNT)]
for i in range(0, COUNT, 8):
    lines.append('    ' + ' '.join(paints[i:i + 8]))
lines.append('};')
lines.append('')
lines.append('// D_002A9B80[D_002AC098[i]]: the 16 default speed tables (a byte a zone) Ai_CopySpeedTables gives each '
             'teammate.')
lines.append('const uint8_t kTeammateSpeeds[%d][256] = {' % TEAMMATES)
for i in rd(SPEED_INDEX, TEAMMATES):
    table = rd(SPEEDS + 0x100 * i, 0x100)
    lines.append('    {')
    for j in range(0, 0x100, 32):
        lines.append('        ' + ' '.join('%d,' % v for v in table[j:j + 32]))
    lines.append('    },')
lines.append('};')
open(OUT, 'w', newline='\r\n').write('\n'.join(lines) + '\n')
print('wrote', OUT)
