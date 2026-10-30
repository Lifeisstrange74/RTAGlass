"""A build's fingerprint: SHA-256 of everything build.py makes from the disc and of the player folder.

One line per item: the generated sources and every file of the player folder (dist/RTAGlass) on their own, and each
generated folder (the disassembly, the VU reference, each folder of data) as one hash over all its files' names and
hashes, so a difference shows where it is without listing 20,000 files.

expected.txt is this repo's reference: the fingerprint of the build the port's author made and played, which build.py
checks a new build against.

    python -I scripts/build/fingerprint.py [ROOT]              print ROOT's fingerprint (default: this repo)
    python -I scripts/build/fingerprint.py --check [ROOT]      compare it with expected.txt
"""
import hashlib
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(HERE, '..', '..'))
EXPECTED = os.path.join(HERE, 'expected.txt')

# Sources made from the disc (build.py's generate step). The game includes them like any other source file.
GENERATED_SOURCES = [
    'src/game/field/field_coins.inc',
    'src/game/field/field_objects.inc',
    'src/game/field/field_stamps.inc',
    'src/game/field/field_towns.inc',
    'src/game/race/adventure_speeds.inc',
    'src/game/race/car_jets.inc',
    'src/game/race/car_parts.inc',
    'src/game/race/driver_records.inc',
    'src/game/race/figure8_tables.inc',
    'src/game/race/golf_tables.inc',
    'src/game/save/save_tables.inc',
    'src/game/story/adventure_races.inc',
    'src/game/story/parts_tables.inc',
    'src/game_text.inc',
    'tools/common/vu1/vu1_microcode.gen.cpp',
    'src/rtaglass.ico',
]
GENERATED_FOLDERS = ['reference/asm', 'reference/vu']
DIST = 'dist/RTAGlass'
# A player's own files, which the exe makes next to itself: never part of a build.
PLAYER_FILES = {'save1.json', 'save2.json', 'controls.json', 'display.json', 'extras.json'}
# Not compared: the game's exe depends on the folder it's built in (link-time code generation orders code by names
# that MSVC derives from each source file's path), so each checkout builds its own, from the same sources.
NOT_COMPARED = {DIST + '/RTAGlass.exe'}


def sha256_file(path):
    h = hashlib.sha256()
    with open(path, 'rb') as fp:
        for chunk in iter(lambda: fp.read(1 << 22), b''):
            h.update(chunk)
    return h.hexdigest()


def tree(path, skip=()):
    """(hash, count) over every file under path: its name relative to path, and its contents."""
    rows = []
    for dirpath, dirnames, filenames in os.walk(path):
        dirnames.sort()
        for name in filenames:
            full = os.path.join(dirpath, name)
            rel = os.path.relpath(full, path).replace(os.sep, '/')
            if rel not in skip:
                rows.append('%s\t%s\n' % (rel, sha256_file(full)))
    rows.sort()
    return hashlib.sha256(''.join(rows).encode('utf-8')).hexdigest(), len(rows)


def fingerprint(root):
    """{item: 'hash' or 'hash count'}; an item that's missing is 'missing'."""
    out = {}
    for rel in GENERATED_SOURCES:
        full = os.path.join(root, rel)
        out['file ' + rel] = sha256_file(full) if os.path.isfile(full) else 'missing'
    folders = list(GENERATED_FOLDERS)
    data = os.path.join(root, 'data')
    if os.path.isdir(data):
        folders += ['data/' + d for d in sorted(os.listdir(data)) if os.path.isdir(os.path.join(data, d))]
    dist = os.path.join(root, DIST)
    if os.path.isdir(dist):
        for dirpath, dirnames, filenames in os.walk(dist):
            rel_dir = os.path.relpath(dirpath, root).replace(os.sep, '/')
            if rel_dir.startswith(DIST + '/data/'):          # the data folders a level down: one hash each
                folders.append(rel_dir)
                dirnames[:] = []
                continue
            dirnames.sort()
            for name in sorted(filenames):
                if rel_dir == DIST and name in PLAYER_FILES or '%s/%s' % (rel_dir, name) in NOT_COMPARED:
                    continue
                out['file %s/%s' % (rel_dir, name)] = sha256_file(os.path.join(dirpath, name))
    for rel in folders:
        full = os.path.join(root, rel)
        if os.path.isdir(full):
            h, n = tree(full)
            out['tree ' + rel] = '%s %d' % (h, n)
        else:
            out['tree ' + rel] = 'missing'
    return out


def write(fp, items):
    for key in sorted(items):
        fp.write('%s  %s\n' % (key, items[key]))


def load_expected():
    items = {}
    for line in open(EXPECTED, encoding='utf-8'):
        if line.strip() and not line.startswith('#'):
            key, value = line.rstrip('\n').split('  ', 1)
            items[key] = value
    return items


def check(root):
    """Prints what differs from expected.txt; True if nothing does."""
    expected, actual = load_expected(), fingerprint(root)
    bad = []
    for key in sorted(set(expected) | set(actual)):
        want, got = expected.get(key, 'not expected'), actual.get(key, 'missing')
        if want != got:
            bad.append('  %-60s expected %s, got %s' % (key, want[:16], got[:16]))
    for line in bad:
        print(line)
    if bad:
        print('%d of %d items differ from the reference build.' % (len(bad), len(expected)))
        return False
    print('All %d items match the reference build.' % len(expected))
    return True


def main():
    args = sys.argv[1:]
    if args and args[0] == '--check':
        sys.exit(0 if check(os.path.abspath(args[1]) if len(args) > 1 else REPO) else 1)
    root = os.path.abspath(args[0]) if args else REPO
    write(sys.stdout, fingerprint(root))


if __name__ == '__main__':
    main()
