"""Writes src/game/race/figure8_tables.inc from the ELF: Figure 8's (course 0x19, race 45) tables.
- D_002C3A18: each step's start (s16 x, z); D_002C3A30 its cones' count, D_002C3A38 its gates' count, D_002C3A40
  the stamp its clean run gives, D_002C3AF8 its goal time; D_002A42E0 its map view (x, y, scale, angle).
- D_002C3A00: each step's cones' homes (vec4); D_002C3238: each step's gates (s8 a, sign a, b, sign b, c, sign c:
  lines of D_002C3250, 76 x {x0, z0, x1, z1}).
- D_002C3AE0: each step's description, a list of strings ending in a null pointer.

    python -I scripts/elf/gen_figure8.py
"""
import os
import struct

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..')
# The extracted disc: OriginalGameFiles next to the repo, or the folder in RTAGLASS_GAME.
GAME = os.environ.get('RTAGLASS_GAME', os.path.join(ROOT, '..', 'OriginalGameFiles'))
ELF = os.path.join(GAME, 'SLUS_203.98')
OUT = os.path.join(ROOT, 'src', 'game', 'race', 'figure8_tables.inc')
STEPS = 5
LINES = 76

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


starts = struct.unpack('<10h', rd(0x002C3A18, 20))
cones = list(rd(0x002C3A30, STEPS))
gates = list(rd(0x002C3A38, STEPS))
stamps = list(rd(0x002C3A40, STEPS))
goals = struct.unpack('<5i', rd(0x002C3AF8, 20))
views = struct.unpack('<20f', rd(0x002A42E0, 80))
cone_ptr = struct.unpack('<5I', rd(0x002C3A00, 20))
gate_ptr = struct.unpack('<5I', rd(0x002C3238, 20))
desc_ptr = struct.unpack('<5I', rd(0x002C3AE0, 20))
lines = struct.unpack('<%df' % (4 * LINES), rd(0x002C3250, 16 * LINES))

o = ['// Made by scripts/elf/gen_figure8.py from the ELF; see that script for what each table is.', '']
o.append('Figure8Step kSteps[%d] = {   // not const: the Map Tuner extra changes the views' % STEPS)
for s in range(STEPS):
    o.append('    {%d, %d, %d, %d, %d, %d, {%s}},' % (
        starts[2 * s], starts[2 * s + 1], cones[s], gates[s], stamps[s], goals[s],
        ', '.join(g(v) for v in views[4 * s:4 * s + 4])))
o.append('};')
o.append('// D_002C3A00: the cones\' homes (x, z; at height 1.5).')
o.append('const float kConeHomes[%d][17][2] = {' % STEPS)
for s in range(STEPS):
    v = struct.unpack('<%df' % (4 * cones[s]), rd(cone_ptr[s], 16 * cones[s]))
    homes = ['{%s, %s}' % (g(v[4 * j]), g(v[4 * j + 2])) for j in range(cones[s])]
    o.append('    {')
    for k in range(0, len(homes), 6):
        o.append('        ' + ', '.join(homes[k:k + 6]) + ',')
    o.append('    },')
o.append('};')
o.append('// D_002C3238: the gates, {a, sign, b, sign, c, sign}.')
o.append('const int8_t kGates[%d][44][6] = {' % STEPS)
for s in range(STEPS):
    b = struct.unpack('<%db' % (6 * gates[s]), rd(gate_ptr[s], 6 * gates[s]))
    rows = ['{%s}' % ', '.join(str(x) for x in b[6 * k:6 * k + 6]) for k in range(gates[s])]
    o.append('    {')
    for k in range(0, len(rows), 4):
        o.append('        ' + ', '.join(rows[k:k + 4]) + ',')
    o.append('    },')
o.append('};')
o.append('// D_002C3250: the lines, {x0, z0, x1, z1}.')
o.append('const float kLines[%d][4] = {' % LINES)
for k in range(0, LINES, 4):
    o.append('    ' + ' '.join('{%s},' % ', '.join(g(x) for x in lines[4 * i:4 * i + 4]) for i in range(k, min(k + 4, LINES))))
o.append('};')
o.append('// D_002C3AE0: the steps\' descriptions.')
o.append('const char* const kStepText[%d][5] = {' % STEPS)
for s in range(STEPS):
    ptrs = struct.unpack('<5I', rd(desc_ptr[s], 20))
    strs = []
    for p in ptrs:
        if not p:
            break
        strs.append('"%s"' % cstr(p).replace('\\', '\\\\').replace('"', '\\"'))
    o.append('    {%s},' % ', '.join(strs))
o.append('};')
open(OUT, 'w', newline='\n').write('\n'.join(o) + '\n')
print('wrote', OUT)
