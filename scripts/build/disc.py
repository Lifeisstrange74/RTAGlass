"""Reads the game's disc image and extracts its files.

Takes a .cue sheet (with its .bin: one data track, MODE1/2352 or MODE2/2352), a raw .bin, or a 2048-byte .iso, and
writes every file of its ISO 9660 file system to a folder, names without the ';1' version. Then each extracted file is
checked against disc_files.txt, the SHA-1 of every file on the disc this port was made from (Road Trip, USA,
SLUS-203.98), so a different version of the game is caught before anything is built from it.

    python -I scripts/build/disc.py IMAGE OUTDIR         extract and check
    python -I scripts/build/disc.py --check OUTDIR       only check an already extracted folder
"""
import hashlib
import os
import re
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
MANIFEST = os.path.join(HERE, 'disc_files.txt')


class Image:
    """2048-byte logical sectors from a disc image, whatever its sector layout."""

    def __init__(self, path):
        self.fp = open(path, 'rb')
        size = os.fstat(self.fp.fileno()).st_size
        if size % 2352 == 0 and size % 2048 != 0 or self._looks_raw():
            self.raw = 2352
            mode = self._read_raw(16)[15]
            self.data = 24 if mode == 2 else 16    # Mode 2 Form 1: 8-byte subheader after the 16-byte header
        else:
            self.raw, self.data = 2048, 0

    def _looks_raw(self):
        self.fp.seek(0)
        return self.fp.read(12) == b'\x00' + b'\xff' * 10 + b'\x00'   # a raw sector's sync pattern

    def _read_raw(self, lba):
        self.fp.seek(lba * 2352)
        return self.fp.read(2352)

    def sector(self, lba):
        self.fp.seek(lba * self.raw + self.data)
        return self.fp.read(2048)

    def read(self, lba, length):
        out = bytearray()
        while len(out) < length:
            out += self.sector(lba)
            lba += 1
        return bytes(out[:length])


def image_from_cue(path):
    """The data track's file from a cue sheet (the first FILE; Road Trip has only the one track)."""
    text = open(path, 'r', encoding='latin-1').read()
    m = re.search(r'^\s*FILE\s+"([^"]+)"', text, re.M) or re.search(r'^\s*FILE\s+(\S+)', text, re.M)
    if not m:
        sys.exit('%s: no FILE line in the cue sheet' % path)
    track = re.search(r'TRACK\s+01\s+(\S+)', text)
    if track and track.group(1).upper() not in ('MODE1/2352', 'MODE2/2352', 'MODE1/2048'):
        sys.exit('%s: track 1 is %s; expected a data track' % (path, track.group(1)))
    return os.path.join(os.path.dirname(os.path.abspath(path)), m.group(1))


def walk(img, lba, size, prefix, out):
    """Every file under the directory at lba: (path, lba, size)."""
    data = img.read(lba, size)
    pos = 0
    while pos < len(data):
        length = data[pos]
        if length == 0:                              # records don't cross sectors: skip to the next one
            pos = (pos // 2048 + 1) * 2048
            continue
        rec = data[pos:pos + length]
        extent, = struct.unpack_from('<I', rec, 2)
        dsize, = struct.unpack_from('<I', rec, 10)
        flags = rec[25]
        name = rec[33:33 + rec[32]]
        pos += length
        if name in (b'\x00', b'\x01'):
            continue
        name = name.decode('ascii').split(';')[0]
        path = prefix + name
        if flags & 2:
            walk(img, extent, dsize, path + '/', out)
        else:
            out.append((path, extent, dsize))
    return out


def extract(image_path, outdir):
    if image_path.lower().endswith('.cue'):
        image_path = image_from_cue(image_path)
    if not os.path.isfile(image_path):
        sys.exit('%s: not found' % image_path)
    img = Image(image_path)
    pvd = img.sector(16)
    if pvd[1:6] != b'CD001':
        sys.exit('%s: no ISO 9660 file system (not a PS2 data disc?)' % image_path)
    root = pvd[156:156 + 34]
    files = walk(img, struct.unpack_from('<I', root, 2)[0], struct.unpack_from('<I', root, 10)[0], '', [])
    print('%s: %s, %d files' % (os.path.basename(image_path), pvd[40:72].decode('ascii').strip(), len(files)))
    for path, lba, size in files:
        dest = os.path.join(outdir, *path.split('/'))
        os.makedirs(os.path.dirname(dest), exist_ok=True)
        with open(dest, 'wb') as fp:
            left = size
            while left:
                chunk = min(left, 1 << 22)
                fp.write(img.read(lba, chunk))
                lba += chunk // 2048
                left -= chunk


def load_manifest():
    expected = {}
    for line in open(MANIFEST, encoding='utf-8'):
        if line.strip() and not line.startswith('#'):
            sha, path = line.rstrip('\n').split('  ', 1)
            expected[path] = sha
    return expected


def sha1_file(path):
    h = hashlib.sha1()
    with open(path, 'rb') as fp:
        for chunk in iter(lambda: fp.read(1 << 22), b''):
            h.update(chunk)
    return h.hexdigest()


def check(outdir):
    """True if outdir holds every file of the disc, each as it should be."""
    expected = load_manifest()
    bad = []
    for path, sha in expected.items():
        full = os.path.join(outdir, *path.split('/'))
        if not os.path.isfile(full):
            bad.append('missing  ' + path)
        elif sha1_file(full) != sha:
            bad.append('differs  ' + path)
    for line in bad[:20]:
        print('  ' + line)
    if len(bad) > 20:
        print('  ... and %d more' % (len(bad) - 20))
    if bad:
        print('%d of %d files don\'t match Road Trip (USA, SLUS-203.98).' % (len(bad), len(expected)))
        return False
    print('All %d files match Road Trip (USA, SLUS-203.98).' % len(expected))
    return True


def main():
    args = sys.argv[1:]
    if len(args) == 2 and args[0] == '--check':
        sys.exit(0 if check(args[1]) else 1)
    if len(args) != 2:
        sys.exit(__doc__)
    extract(args[0], args[1])
    sys.exit(0 if check(args[1]) else 1)


if __name__ == '__main__':
    main()
