"""Walk the .vutext section of SLUS_203.98 (a DMA chain of VIF packets) and disassemble its microcode.

.vutext (0x0028E730, 0xC100 bytes) is what Sony's dvp-as produced: DMA tags whose upper 64 bits carry VIF
codes, with MPG uploads (microcode) and UNPACK/DIRECT blocks (prebuilt VU data / GIF packets).

Outputs:
  reference/vu/vutext_chain.txt     every DMA tag and VIF code in order, with addresses
  reference/vu/mpg_<va>.vsm         disassembly of each MPG upload, at its VU micro-memory address
  stdout                           summary: MPG blocks, their VU load address and size

Usage: python -I scripts/vu/vutext.py
"""
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(__file__))
from vudis import disasm_pair  # noqa: E402

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
# The extracted disc: OriginalGameFiles next to the repo, or the folder in RTAGLASS_GAME.
GAME = os.environ.get('RTAGLASS_GAME', os.path.join(ROOT, '..', 'OriginalGameFiles'))
ELF = os.path.join(GAME, 'SLUS_203.98')
OUTDIR = os.path.join(ROOT, 'reference', 'vu')

VA, FILE_OFS, SIZE = 0x0028E730, 0x0008F730, 0xC100

DMA_IDS = ['refe', 'cnt', 'next', 'ref', 'refs', 'call', 'ret', 'end']
VIF_NAMES = {0x00: 'NOP', 0x01: 'STCYCL', 0x02: 'OFFSET', 0x03: 'BASE', 0x04: 'ITOP', 0x05: 'STMOD',
             0x06: 'MSKPATH3', 0x07: 'MARK', 0x10: 'FLUSHE', 0x11: 'FLUSH', 0x13: 'FLUSHA', 0x14: 'MSCAL',
             0x15: 'MSCALF', 0x17: 'MSCNT', 0x20: 'STMASK', 0x30: 'STROW', 0x31: 'STCOL', 0x4A: 'MPG',
             0x50: 'DIRECT', 0x51: 'DIRECTHL'}
UNPACK_FMT = ['S-32', 'S-16', 'S-8', '?', 'V2-32', 'V2-16', 'V2-8', '?', 'V3-32', 'V3-16', 'V3-8', '?',
              'V4-32', 'V4-16', 'V4-8', 'V4-5']


def unpack_words(cmd, num, wl, cl):
    vn, vl = (cmd >> 2) & 3, cmd & 3
    bits = 16 if (vn == 3 and vl == 3) else (vn + 1) * (32 >> vl)
    if wl <= cl:
        elems = num
    else:
        elems = cl * (num // wl) + min(num % wl, cl)
    return (elems * bits + 31) // 32


class VifStream:
    """Decodes VIF codes from a flat list of 32-bit words; carries state across DMA packets."""

    def __init__(self, log):
        self.log = log
        self.wl = self.cl = 1
        self.pending = None      # (kind, words_remaining, info)
        self.mpgs = []           # (va_of_first_insn, vu_addr_bytes, [(lo, hi), ...])
        self.mscals = []
        self.directs = []
        self.unpacks = []

    def feed(self, words, va):
        i = 0
        while i < len(words):
            if self.pending:
                kind, remaining, info = self.pending
                take = min(remaining, len(words) - i)
                info['data'].extend(words[i:i + take])
                if 'va' not in info:
                    info['va'] = va + i * 4
                i += take
                remaining -= take
                self.pending = (kind, remaining, info) if remaining else None
                if not remaining:
                    self.finish(kind, info)
                continue
            w = words[i]
            cmd = (w >> 24) & 0x7F
            imm = w & 0xFFFF
            num = (w >> 16) & 0xFF
            here = va + i * 4
            i += 1
            if cmd >= 0x60:
                n = num or 256
                nw = unpack_words(cmd, n, self.wl, self.cl)
                addr = imm & 0x3FF
                self.log('%08X   UNPACK %s num=%d addr=0x%03X%s%s usn=%d (%d words)' % (
                    here, UNPACK_FMT[cmd & 0xF], n, addr, ' +TOPS' if imm & 0x8000 else '',
                    ' MASK' if cmd & 0x10 else '', (imm >> 14) & 1, nw))
                self.unpacks.append((here, UNPACK_FMT[cmd & 0xF], n, addr, bool(imm & 0x8000)))
                if nw:
                    self.pending = ('unpack', nw, {'data': []})
                continue
            name = VIF_NAMES.get(cmd, '?%02X' % cmd)
            if name == 'STCYCL':
                self.cl, self.wl = imm & 0xFF, (imm >> 8) & 0xFF
                self.log('%08X   STCYCL cl=%d wl=%d' % (here, self.cl, self.wl))
            elif name == 'MPG':
                n = num or 256
                # MPG data is 64-bit aligned in the stream
                if i % 2:
                    i += 1
                self.log('%08X   MPG num=%d vuaddr=0x%04X' % (here, n, imm * 8))
                self.pending = ('mpg', n * 2, {'data': [], 'vuaddr': imm * 8})
            elif name in ('DIRECT', 'DIRECTHL'):
                n = imm or 65536
                while i % 4:
                    i += 1
                self.log('%08X   %s qwc=%d' % (here, name, n))
                self.pending = ('direct', n * 4, {'data': []})
            elif name in ('MSCAL', 'MSCALF'):
                self.log('%08X   %s 0x%04X' % (here, name, imm * 8))
                self.mscals.append((here, imm * 8))
            elif name == 'STMASK':
                self.log('%08X   STMASK' % here)
                self.pending = ('skip', 1, {'data': []})
            elif name in ('STROW', 'STCOL'):
                self.log('%08X   %s' % (here, name))
                self.pending = ('skip', 4, {'data': []})
            elif name == 'NOP':
                pass
            else:
                self.log('%08X   %s 0x%04X' % (here, name, imm))

    def finish(self, kind, info):
        d = info['data']
        if kind == 'mpg':
            pairs = [(d[k], d[k + 1]) for k in range(0, len(d), 2)]
            self.mpgs.append((info['va'], info['vuaddr'], pairs))
        elif kind == 'direct':
            self.directs.append((info['va'], len(d) // 4))


def walk(blob):
    lines = []
    vif = VifStream(lines.append)
    pos = 0
    while pos + 16 <= len(blob):
        lo, addr, v0, v1 = struct.unpack_from('<4I', blob, pos)
        qwc, tid = lo & 0xFFFF, (lo >> 28) & 7
        va = VA + pos
        lines.append('%08X DMA %-4s qwc=%-4d addr=0x%08X' % (va, DMA_IDS[tid], qwc, addr))
        vif.feed([v0, v1], va + 8)
        pos += 16
        if tid in (1, 2, 6, 7, 5):        # data follows the tag
            words = list(struct.unpack_from('<%dI' % (qwc * 4), blob, pos))
            vif.feed(words, VA + pos)
            pos += qwc * 16
    return lines, vif


def write_mpg(va, vuaddr, pairs, path):
    with open(path, 'w', encoding='utf-8', newline='\n') as fp:
        fp.write('; MPG upload from .vutext 0x%08X -> VU micro memory 0x%04X, %d instructions\n' % (
            va, vuaddr, len(pairs)))
        fp.write('; columns: vu_addr  [flags]  upper  |  lower\n')
        targets = set()
        for k, (lo, hi) in enumerate(pairs):
            _u, _l, _f, t = disasm_pair(lo, hi, vuaddr + k * 8)
            if t is not None:
                targets.add(t)
        for k, (lo, hi) in enumerate(pairs):
            pc = vuaddr + k * 8
            up, low, flags, _t = disasm_pair(lo, hi, pc)
            if pc in targets:
                fp.write('L%04X:\n' % pc)
            fp.write('  %04X %-3s %-36s | %s\n' % (pc, '[' + flags + ']' if flags else '', up, low))


def main():
    elf = open(ELF, 'rb').read()
    blob = elf[FILE_OFS:FILE_OFS + SIZE]
    lines, vif = walk(blob)
    os.makedirs(OUTDIR, exist_ok=True)
    with open(os.path.join(OUTDIR, 'vutext_chain.txt'), 'w', encoding='utf-8', newline='\n') as fp:
        fp.write('\n'.join(lines) + '\n')
    print('%d MPG uploads, %d DIRECT blocks, %d UNPACKs, %d MSCALs' % (
        len(vif.mpgs), len(vif.directs), len(vif.unpacks), len(vif.mscals)))
    for va, vuaddr, pairs in vif.mpgs:
        path = os.path.join(OUTDIR, 'mpg_%08X.vsm' % va)
        write_mpg(va, vuaddr, pairs, path)
        print('  MPG @%08X -> vu 0x%04X..0x%04X (%d insns)' % (va, vuaddr, vuaddr + len(pairs) * 8, len(pairs)))


if __name__ == '__main__':
    sys.exit(main())
