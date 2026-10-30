"""Writes the PC's own data for COURSE/C16.BIN, the course on the disc that no table of the game's names (the Test
Course extra, docs/extras.md). The game has no path, map or trackside cameras for it, so they're made here from the
road itself (modelconv's data/models/COURSE/C16 blocks):

- data/ai/C16.txt: the AI path (docs/ai.md's format). The road (a closed tube) is traced from the start box, driving
  -x, by cross-sections of its top view; a gate 6 units wide every 15 units, square to the road, and a zone between
  each two. Gate 0 is the line's (x 410, the start box's -x edge). -x because the tube's two ends meet over the box
  with a gap at x 427-431, the floor at 27.1 on the +x side and 24.0 on the -x: that way it's a step down. The speeds
  (5 km/h) come from the road's bend, eased so a car can brake for the next bend.
- data/models/COURSE/C16/map.obj: the HUD map's ribbon, the road's edges every 10 units.
- data/camera/C16.txt: only the courses' common last trackside camera (following behind the car).

Prints the path's length (the course select's "Track" length is that over ~1580 units a mile, as the Quick Race
courses' paths measure against their D_002A8F30 lengths).

    python -I scripts/course/make_c16.py [DATA]   (DATA: the data folder, default the repo's)
"""
import glob
import math
import os
import sys

import numpy as np

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..')
DATA = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, 'data')
BLOCKS = os.path.join(DATA, 'models', 'COURSE', 'C16')

START = (435.0, 836.0)   # the start box's middle (x 410-460, z 808-864; a flat quad at y 20.4, under the road)
HEADING = -1.0           # driving -x at the start
LINE_X = 410.0           # the box's far edge that way
GATE_STEP, GATE_WIDTH, MAP_STEP = 15.0, 6.0, 10.0
CELL = 0.5               # the top view's grid
SIZE = (1800, 1700)      # cells: z 0-900, x 0-850


def load_triangles():
    tris = []
    for path in sorted(glob.glob(os.path.join(BLOCKS, '[0-9][0-9].obj'))):
        vs = []
        for row in open(path):
            if row.startswith('v '):
                vs.append([float(x) for x in row.split()[1:4]])
            elif row.startswith('f '):
                idx = [int(t.split('/')[0]) for t in row.split()[1:]]
                tris.append([vs[i - 1] for i in idx[:3]])
    return tris


def top_view(tris):
    """The road's cells seen from above (the start box's quad is only 2 of 10k triangles; it widens the road there)."""
    h, w = SIZE
    road = np.zeros((h, w), bool)
    for p0, p1, p2 in tris:
        xs = [p[0] / CELL for p in (p0, p1, p2)]
        zs = [p[2] / CELL for p in (p0, p1, p2)]
        i0, i1 = max(0, int(min(xs))), min(w - 1, int(max(xs)) + 1)
        j0, j1 = max(0, int(min(zs))), min(h - 1, int(max(zs)) + 1)
        d = (zs[1] - zs[2]) * (xs[0] - xs[2]) + (xs[2] - xs[1]) * (zs[0] - zs[2])
        if abs(d) < 1e-9:
            continue
        ii, jj = np.meshgrid(np.arange(i0, i1 + 1) + 0.5, np.arange(j0, j1 + 1) + 0.5)
        u = ((zs[1] - zs[2]) * (ii - xs[2]) + (xs[2] - xs[1]) * (jj - zs[2])) / d
        v = ((zs[2] - zs[0]) * (ii - xs[2]) + (xs[0] - xs[2]) * (jj - zs[2])) / d
        road[j0:j1 + 1, i0:i1 + 1] |= (u >= -0.01) & (v >= -0.01) & (1 - u - v >= -0.01)
    # Fill the mesh's small holes: a cell is road if 13 of its 5x5 neighbours are.
    padded = np.pad(road, 2)
    count = sum(padded[dy:dy + h, dx:dx + w].astype(int) for dy in range(5) for dx in range(5))
    return road | (count >= 13)


def trace(road):
    """The road's middle from the start box round to it again: (x, z, left edge, right edge) along the way."""
    h, w = road.shape

    def on(x, z):
        i, j = int(x / CELL), int(z / CELL)
        return 0 <= i < w and 0 <= j < h and road[j, i]

    ts = np.arange(-40, 40.01, 0.25)
    p, d = np.array(START), np.array([HEADING, 0.0])
    out, total = [], 0.0
    for _ in range(5000):
        q = p + d * 2.0
        n = np.array([-d[1], d[0]])
        hit = np.array([on(*(q + t * n)) for t in ts])
        k = int(np.argmin(np.where(hit, np.abs(ts), 1e9)))
        a = b = k
        while a > 0 and hit[a - 1]:
            a -= 1
        while b < len(ts) - 1 and hit[b + 1]:
            b += 1
        lo, hi = ts[a], ts[b]
        if hi - lo > 34:   # the start box: keep the road's own 30
            lo, hi = -15.0, 15.0
        m = q + n * (lo + hi) / 2
        nd = (m - p) / np.linalg.norm(m - p)
        d = (d + nd) / np.linalg.norm(d + nd)
        total += float(np.linalg.norm(m - p))
        p = m
        out.append((m[0], m[1], (hi - lo) / 2))
        if total > 300 and np.linalg.norm(p - np.array(START)) < 3:
            break
    return out


def resample(points, step, start):
    """Points every `step` along the closed line, the first at x = start on the start straight."""
    pts = np.array([(x, z) for x, z, _ in points])
    half = np.array([r for _, _, r in points])
    seg = np.linalg.norm(np.diff(np.vstack([pts, pts[:1]]), axis=0), axis=1)
    cum = np.concatenate([[0], np.cumsum(seg)])
    length = cum[-1]
    # where the line crosses x = start near the box
    s0 = next(cum[i] + (start - pts[i][0]) / (pts[i + 1][0] - pts[i][0]) * seg[i]
              for i in range(len(pts) - 1)
              if (pts[i][0] - start) * (pts[i + 1][0] - start) <= 0 and pts[i][0] != pts[i + 1][0]
              and abs(pts[i][1] - START[1]) < 20)
    n = int(round(length / step))
    out = []
    for k in range(n):
        s = (s0 + k * length / n) % length
        i = int(np.searchsorted(cum, s, side='right') - 1) % len(pts)
        f = (s - cum[i]) / seg[i] if seg[i] else 0
        j = (i + 1) % len(pts)
        out.append((pts[i] * (1 - f) + pts[j] * f, half[i] * (1 - f) + half[j] * f))
    return out, length


def directions(samples):
    n = len(samples)
    out = []
    for k in range(n):
        a, b = samples[(k - 1) % n][0], samples[(k + 1) % n][0]
        d = b - a
        out.append(d / np.linalg.norm(d))
    return out


def speeds(samples, step):
    """Each zone's speed in 5 km/h (0 = none): the bend's radius over +-2 gates gives v = sqrt(6 R) m/s (a unit is
    about a metre), then eased backwards so the car can brake at 7 m/s/s, within 55-170 km/h."""
    n = len(samples)
    pts = [s[0] for s in samples]
    v = []
    for k in range(n):
        a, b, c = pts[(k - 2) % n], pts[k], pts[(k + 2) % n]
        ab, bc, ca = np.linalg.norm(b - a), np.linalg.norm(c - b), np.linalg.norm(a - c)
        cross = abs((b - a)[0] * (c - a)[1] - (b - a)[1] * (c - a)[0])
        r = ab * bc * ca / (2 * cross) if cross > 1e-6 else 1e9
        v.append(min(math.sqrt(6.0 * r), 170 / 3.6))
    for _ in range(3):
        for k in range(n - 1, -1, -1):
            v[k] = min(v[k], math.sqrt(v[(k + 1) % n] ** 2 + 2 * 7.0 * step))
    return [max(11, min(34, int(x * 3.6 / 5))) for x in v]


def main():
    road = top_view(load_triangles())
    centre = trace(road)
    gates, length = resample(centre, GATE_STEP, LINE_X)
    dirs = directions(gates)
    speed = speeds(gates, length / len(gates))
    n = len(gates)
    assert n < 256
    with open(os.path.join(DATA, 'ai', 'C16.txt'), 'w', newline='\n') as f:
        f.write('# (PC) A path for course 0x27 (COURSE/C16.BIN, the Test Course extra), which the game has none for:\n'
                '# made by scripts/course/make_c16.py from the road. See docs/ai.md.\n'
                '# line x0 z0 x1 z1 splitX splitZ: a gate across the racing line\n')
        for (m, _), d in zip(gates, dirs):
            # (x0 - x1, z0 - z1) is the direction turned a quarter: the AI's course heading is its angle - 0x4000.
            ex, ez = d[1] * GATE_WIDTH / 2, -d[0] * GATE_WIDTH / 2
            f.write('line %.1f %.1f %.1f %.1f 0 0\n' % (m[0] + ex, m[1] + ez, m[0] - ex, m[1] - ez))
        f.write('# zone in out prev prevAlt next nextAlt rank speed\n')
        for k in range(n):
            p, q = (k - 1) % n, (k + 1) % n
            f.write('zone %d %d %d %d %d %d %d %d\n' % (k, q, p, p, q, q, k, speed[k]))
    samples, _ = resample(centre, MAP_STEP, LINE_X)
    sdirs = directions(samples)
    with open(os.path.join(BLOCKS, 'map.obj'), 'w', newline='\n') as f:
        f.write('# (PC) COURSE/C16.BIN map (the game has none): the road\'s edges, by scripts/course/make_c16.py\n'
                'mtllib map.mtl\nusemtl color\n')
        for (m, r), d in zip(samples, sdirs):
            nx, nz = -d[1], d[0]
            for s in (-1, 1):
                f.write('v %.3f 0.000000 %.3f 1.0000 1.0000 1.0000\n' % (m[0] + s * r * nx, m[1] + s * r * nz))
        k = len(samples)
        for i in range(k):
            a, b, c, e = 2 * i + 1, 2 * i + 2, 2 * ((i + 1) % k) + 1, 2 * ((i + 1) % k) + 2
            f.write('f %d %d %d\nf %d %d %d\n' % (a, b, c, c, b, e))
    with open(os.path.join(BLOCKS, 'map.mtl'), 'w', newline='\n') as f:
        f.write('# (PC) COURSE/C16.BIN map by scripts/course/make_c16.py\n\nnewmtl color\nKd 1 1 1\n')
    with open(os.path.join(DATA, 'camera', 'C16.txt'), 'w', newline='\n') as f:
        f.write('# (PC) The trackside cameras on course 0x27 (COURSE/C16.BIN, the Test Course extra), which the game\n'
                '# has none for: only the last camera every course ends with, following 7 behind the car. By\n'
                '# scripts/course/make_c16.py. See docs/finish.md.\n'
                'camera 0 0 0 0 400 4096 0 0 7 0x801C\n')
    print('gates', n, 'length %.0f units, %.3f mile at 1580 a mile' % (length, length / 1580.0))
    print('speeds', min(speed), max(speed))


if __name__ == '__main__':
    main()
