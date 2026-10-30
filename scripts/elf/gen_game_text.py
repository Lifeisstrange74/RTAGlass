"""Writes src/game_text.inc from the ELF: the game's own text the port shows (names, menus, descriptions, messages),
which the sources name by where it is in the ELF instead of spelling it out: GT_<address>_<length>, for example
GT_002E1234_13, is the 13 bytes at 0x002E1234. This collects every such name in src and defines each as a string
literal of those bytes, so a name works wherever a literal does (tables, concatenation, sizeof).

    python -I scripts/elf/gen_game_text.py
"""
import os
import re
import struct

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..'))
# The extracted disc: game/ in the repo, or the folder in RTAGLASS_GAME.
GAME = os.environ.get('RTAGLASS_GAME', os.path.join(ROOT, 'game'))
ELF = os.path.join(GAME, 'SLUS_203.98')
SRC = os.path.join(ROOT, 'src')
OUT = os.path.join(SRC, 'game_text.inc')
NAME = re.compile(r'\bGT_([0-9A-F]{8})_(\d+)\b')

elf = open(ELF, 'rb').read()
phoff, = struct.unpack_from('<I', elf, 0x1C)
count, = struct.unpack_from('<H', elf, 0x2C)
segs = [struct.unpack_from('<6I', elf, phoff + 32 * i)[1:5] for i in range(count)]


def rd(va, n):
    for off, vaddr, _pa, size in segs:
        if vaddr <= va and va + n <= vaddr + size:
            return elf[off + va - vaddr:off + va - vaddr + n]
    raise ValueError('GT_%08X_%d is outside the ELF\'s segments' % (va, n))


def literal(b):
    # Printable ASCII as is; anything else, and the quote and backslash, as 3-digit octal (never run into the next).
    return '"%s"' % ''.join(chr(c) if 32 <= c < 127 and c not in (34, 92) else '\\%03o' % c for c in b)


names = set()
for dirpath, dirnames, filenames in os.walk(SRC):
    dirnames.sort()
    for f in filenames:
        if f.endswith(('.cpp', '.h')):
            for m in NAME.finditer(open(os.path.join(dirpath, f), encoding='utf-8').read()):
                names.add((int(m.group(1), 16), int(m.group(2))))

lines = ['// The game\'s text the sources name as GT_<address>_<length>. Dumped from the ELF by',
         '// scripts/elf/gen_game_text.py; never commit it.', '#pragma once']
for va, n in sorted(names):
    lines.append('#define GT_%08X_%d %s' % (va, n, literal(rd(va, n))))
open(OUT, 'w', newline='\n').write('\n'.join(lines) + '\n')
print('wrote %s (%d strings)' % (OUT, len(names)))
