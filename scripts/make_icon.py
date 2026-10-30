"""Make src/rtaglass.ico, the game's icon, from the title's ROAD TRIP licence plate (SYS/TITLE.GSL #1, the 440x220
plate Title_PressStartTask draws; docs/title.md), as texconv wrote it to data/textures.

The plate sits in a square, its glow and all, from 48 pixels up; the smaller sizes take the plate without the glow, so
it fills more of them. Every size but 256 is a 32-bit DIB with an AND mask, which Windows XP reads (it doesn't read
PNG frames); 256 is a PNG, which Vista on uses and XP skips.

Needs Pillow (to read the DDS and resize). Usage, from the repo root: python -I scripts/make_icon.py
"""
import io
import os
import struct
import sys

from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SOURCE = os.path.join(ROOT, 'data', 'textures', 'SYS', 'TITLE.GSL', '001.dds')
OUT = os.path.join(ROOT, 'src', 'rtaglass.ico')
SIZES = [16, 24, 32, 48, 64, 128, 256]
PLATE = (40, 14, 400, 204)   # the plate inside its glow


def square(image, box):
    part = image.crop(box)
    side = max(part.size)
    out = Image.new('RGBA', (side, side), (0, 0, 0, 0))
    out.paste(part, ((side - part.size[0]) // 2, (side - part.size[1]) // 2))
    return out


def dib(image):
    """A BITMAPINFOHEADER, the BGRA rows bottom up, then the AND mask (1 = transparent), rows padded to 4 bytes."""
    w, h = image.size
    header = struct.pack('<IiiHHIIiiII', 40, w, h * 2, 1, 32, 0, 0, 0, 0, 0, 0)
    px = image.load()
    colour = bytearray()
    for y in range(h - 1, -1, -1):
        for x in range(w):
            r, g, b, a = px[x, y]
            colour += bytes((b, g, r, a))
    stride = ((w + 31) // 32) * 4
    mask = bytearray()
    for y in range(h - 1, -1, -1):
        row = bytearray(stride)
        for x in range(w):
            if px[x, y][3] == 0:
                row[x // 8] |= 0x80 >> (x % 8)
        mask += row
    return header + bytes(colour) + bytes(mask)


def main():
    image = Image.open(SOURCE).convert('RGBA')
    full, plate = square(image, image.getbbox()), square(image, PLATE)
    frames = []
    for size in SIZES:
        icon = (full if size >= 48 else plate).resize((size, size), Image.LANCZOS)
        if size == 256:
            png = io.BytesIO()
            icon.save(png, 'PNG')
            frames.append((size, png.getvalue()))
        else:
            frames.append((size, dib(icon)))
    out = bytearray(struct.pack('<HHH', 0, 1, len(frames)))
    offset = 6 + 16 * len(frames)
    for size, data in frames:
        out += struct.pack('<BBBBHHII', size % 256, size % 256, 0, 0, 1, 32, len(data), offset)
        offset += len(data)
    for _, data in frames:
        out += data
    with open(OUT, 'wb') as f:
        f.write(out)
    print('%s: %d bytes, sizes %s' % (OUT, len(out), SIZES))


if __name__ == '__main__':
    sys.exit(main())
