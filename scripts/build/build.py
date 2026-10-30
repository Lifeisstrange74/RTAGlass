"""Builds RTAGlass from your own copy of Road Trip (PS2, USA, SLUS-203.98). Run it through build.bat.

    build.bat [IMAGE] [--game DIR] [--dxsdk DIR] [--no-verify] [--arch SSE2|SSE|IA32]

Run with no arguments from a console (build.bat double-clicked, say), it asks for the choices instead, and for the
disc image when it doesn't find exactly one.

IMAGE is the disc image: a .cue (with its .bin), a .bin or an .iso. Without one, the build looks for a single .cue in
the repo's folder and the folder above it. --game uses an already extracted disc instead (checked the same way).

The steps, each logged to obj/build/<step>.log:

 1. check    Visual Studio 2022 with the v141 toolset and its Windows XP support, the DirectX SDK (June 2010),
             and the Python packages in requirements.txt (build.bat installs those into .venv).
 2. disc     extract the disc to game/ and check every file against disc_files.txt (skipped when game/ already
             checks out).
 3. decomp   the reference the port is read from: splat's disassembly of the ELF (reference/asm) and the VU1
             microprograms (reference/vu).
 4. generate the sources made from the ELF: the game's tables (src/**/*.inc) and the VU1 microcode C++ the
             converters use.
 5. tools    build the converters (MSBuild, Release|x86).
 6. data     convert the disc's textures, models, collision, AI paths, messages and sound to data/.
 7. game     the icon, then the whole solution; the game's Release build assembles the player folder dist/RTAGlass.
 8. verify   compare everything made from the disc, and the player folder, with expected.txt, the fingerprint of the
             reference build (scripts/build/fingerprint.py).

Nothing here is the game's: everything it makes from the disc lands in folders git ignores.
"""
import argparse
import glob
import os
import shutil
import struct
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(HERE, '..', '..'))
sys.path.insert(0, HERE)
import disc          # noqa: E402
import fingerprint   # noqa: E402

GAME = os.path.join(REPO, 'game')
LOGS = os.path.join(REPO, 'obj', 'build')
SPLAT_WORK = os.path.join(REPO, 'obj', 'splat')
BIN = os.path.join(REPO, 'bin', 'Release')
DXSDK_DEFAULT = r'C:\Program Files (x86)\Microsoft DirectX SDK (June 2010)'
VS_COMPONENTS = {
    'Microsoft.VisualStudio.Component.VC.v141.x86.x64': 'MSVC v141 - VS 2017 C++ x64/x86 build tools',
    'Microsoft.VisualStudio.Component.WinXP': 'C++ Windows XP Support for VS 2017 (v141) tools',
}
PYTHON_MODULES = {'splat': 'splat64', 'numpy': 'numpy', 'PIL': 'pillow'}
# The generators, in order (make_icon.py runs later: it reads the converted textures).
GENERATORS = sorted(glob.glob(os.path.join(REPO, 'scripts', 'elf', 'gen_*.py'))) + [
    os.path.join(REPO, 'scripts', 'gen_parts_tables.py'),
    os.path.join(REPO, 'scripts', 'vu', 'vu2cpp.py'),
]


class BuildError(Exception):
    pass


def step(title):
    print('\n== %s' % title, flush=True)


def run(name, args, cwd=REPO, env=None):
    """Runs a command with its output in obj/build/<name>.log; on failure shows the log's end."""
    os.makedirs(LOGS, exist_ok=True)
    log = os.path.join(LOGS, name + '.log')
    with open(log, 'a', encoding='utf-8', errors='replace') as fp:
        fp.write('> %s\n' % subprocess.list2cmdline(args))
        fp.flush()
        try:
            result = subprocess.run(args, cwd=cwd, env=env, stdout=fp, stderr=subprocess.STDOUT)
        except OSError as e:
            raise BuildError('couldn\'t run %s: %s' % (args[0], e))
    if result.returncode != 0:
        lines = open(log, encoding='utf-8', errors='replace').read().splitlines()
        print('\n'.join('    ' + l for l in lines[-25:]))
        raise BuildError('%s failed (exit code %d); the whole log is %s' % (os.path.basename(args[0]),
                                                                            result.returncode, log))


def python(name, script, *args):
    env = dict(os.environ, RTAGLASS_GAME=GAME)
    run(name, [sys.executable, '-I', script] + list(args), env=env)


# 1. check -----------------------------------------------------------------------------------------------------------

def find_vs(problems):
    """Visual Studio 2022's folder, if it has the components the build needs."""
    vswhere = os.path.join(os.environ.get('ProgramFiles(x86)', r'C:\Program Files (x86)'),
                           'Microsoft Visual Studio', 'Installer', 'vswhere.exe')
    if not os.path.isfile(vswhere):
        problems.append('Visual Studio 2022 isn\'t installed (Community is enough): '
                        'https://visualstudio.microsoft.com/vs/community/')
        return None
    found = subprocess.run([vswhere, '-latest', '-products', '*', '-version', '[17.0,18.0)', '-requires']
                           + list(VS_COMPONENTS) + ['-property', 'installationPath'],
                           capture_output=True, text=True).stdout.strip()
    if found and os.path.isfile(os.path.join(found, r'MSBuild\Current\Bin\MSBuild.exe')):
        return found
    any_vs = subprocess.run([vswhere, '-latest', '-products', '*', '-version', '[17.0,18.0)', '-property',
                             'installationPath'], capture_output=True, text=True).stdout.strip()
    if not any_vs:
        problems.append('Visual Studio 2022 isn\'t installed (Community is enough).')
    else:
        problems.append('Visual Studio 2022 (%s) lacks these components; add them in the Visual Studio Installer '
                        '(Modify > Individual components):\n%s'
                        % (any_vs, '\n'.join('        - ' + v for v in VS_COMPONENTS.values())))
    return None


def find_dxsdk(given, problems):
    candidates = [given] if given else [os.environ.get('DXSDK_DIR', ''), DXSDK_DEFAULT]
    for path in candidates:
        if path and all(os.path.isfile(os.path.join(path, f)) for f in
                        (r'Include\xaudio2.h', r'Lib\x86\dxguid.lib', r'Utilities\bin\x86\fxc.exe')):
            return os.path.join(os.path.abspath(path), '')
    problems.append('The DirectX SDK (June 2010) isn\'t at %s. Install it from '
                    'https://www.microsoft.com/en-us/download/details.aspx?id=6812 (on Windows 10/11 install .NET '
                    '3.5 first; an S1023 error at the end of setup is harmless), or give its folder with --dxsdk.'
                    % (given or DXSDK_DEFAULT))
    return None


def check(args):
    step('1/8 check: build dependencies')
    problems = []
    print('  Python %d.%d.%d' % sys.version_info[:3])
    for module, package in PYTHON_MODULES.items():
        try:
            __import__(module)
        except ImportError:
            problems.append('The Python package %s is missing: run build.bat, which installs requirements.txt '
                            'into .venv.' % package)
    vs = find_vs(problems)
    msbuild = None
    if vs:
        msbuild = os.path.join(vs, r'MSBuild\Current\Bin\MSBuild.exe')
        print('  Visual Studio %s' % vs)
        xp_sdk = os.path.join(os.environ.get('ProgramFiles(x86)', r'C:\Program Files (x86)'),
                              r'Microsoft SDKs\Windows\v7.1A\Include\d3d9.h')
        if not os.path.isfile(xp_sdk):
            problems.append('The Windows 7.1A SDK (it comes with the Windows XP support component) is missing: '
                            '%s' % xp_sdk)
        tools = glob.glob(os.path.join(vs, r'VC\Tools\MSVC\14.16.*'))
        print('  MSVC v141 toolset %s' % (', '.join(os.path.basename(t) for t in tools) or 'not found'))
    dxsdk = find_dxsdk(args.dxsdk, problems)
    if dxsdk:
        print('  DirectX SDK %s' % dxsdk)
    if problems:
        print('\nMissing:')
        for p in problems:
            print('  - ' + p)
        raise BuildError('install what\'s missing and run build.bat again')
    return msbuild, dxsdk


# 2. disc ------------------------------------------------------------------------------------------------------------

def find_image(args):
    if args.image:
        return args.image
    found = []
    for folder in (REPO, os.path.dirname(REPO)):
        found += glob.glob(os.path.join(folder, '*.cue'))
    if len(found) == 1:
        return found[0]
    if args.interactive:
        if found:
            return found[ask('Which disc image?', found, 0)]
        while True:
            image = input('\nThe disc image (.cue, .bin or .iso; you can drag it here): ').strip().strip('"')
            if os.path.isfile(image):
                return image
            print('  no such file: %s' % image)
    if found:
        raise BuildError('several .cue files found (%s): give the one to use, build.bat "PATH\\TO\\GAME.cue"'
                         % ', '.join(found))
    raise BuildError('no disc image given: build.bat "PATH\\TO\\Road Trip (USA).cue" (or a .bin or .iso)')


def extract_disc(args):
    step('2/8 disc: Road Trip (USA, SLUS-203.98)')
    if args.game:
        global GAME
        GAME = os.path.abspath(args.game)
        print('  using the extracted disc in %s' % GAME)
        if not disc.check(GAME):
            raise BuildError('%s isn\'t a complete, unmodified Road Trip (USA) disc' % GAME)
        return
    if os.path.isdir(GAME):
        print('  game/ is already extracted; checking it')
        if disc.check(GAME):
            return
        print('  extracting again')
    image = find_image(args)
    if os.path.isdir(GAME):
        shutil.rmtree(GAME)
    print('  %s -> game/' % image)
    disc.extract(image, GAME)
    if not disc.check(GAME):
        raise BuildError('this image isn\'t Road Trip (USA, SLUS-203.98), or the dump is damaged; the port is '
                         'made for that release only (redump.org has the reference hashes)')


# 3. decomp ----------------------------------------------------------------------------------------------------------

def elf_image(elf_path):
    """The ELF's load segments laid out from 0x00200000, as splat reads the program (its sha1 is in the yaml)."""
    elf = open(elf_path, 'rb').read()
    phoff, = struct.unpack_from('<I', elf, 0x1C)
    phnum, = struct.unpack_from('<H', elf, 0x2C)
    image = bytearray()
    for i in range(phnum):
        kind, offset, vaddr, _, filesz, _ = struct.unpack_from('<6I', elf, phoff + i * 32)
        if kind == 1:
            image += bytes(vaddr - 0x00200000 - len(image))
            image += elf[offset:offset + filesz]
    return bytes(image)


def decomp():
    step('3/8 decomp: the disassembly and the VU1 microprograms')
    if os.path.isdir(SPLAT_WORK):
        shutil.rmtree(SPLAT_WORK)
    os.makedirs(os.path.join(SPLAT_WORK, 'tools', 'splat_ext'))
    shutil.copy(os.path.join(REPO, 'reference', 'splat', 'game.elf.yaml'), SPLAT_WORK)
    shutil.copy(os.path.join(REPO, 'reference', 'symbol_addrs.txt'), SPLAT_WORK)
    open(os.path.join(SPLAT_WORK, 'reloc_addrs.txt'), 'w').close()
    with open(os.path.join(SPLAT_WORK, 'game.elf.rom'), 'wb') as fp:
        fp.write(elf_image(os.path.join(GAME, 'SLUS_203.98')))
    run('decomp', [sys.executable, '-I', '-m', 'splat', 'split', 'game.elf.yaml'], cwd=SPLAT_WORK)
    asm = os.path.join(REPO, 'reference', 'asm')
    if os.path.isdir(asm):
        shutil.rmtree(asm)
    shutil.copytree(os.path.join(SPLAT_WORK, 'asm'), asm)
    print('  reference/asm: %d functions' % len(os.listdir(os.path.join(asm, 'nonmatchings', 'cod'))))
    os.makedirs(os.path.join(REPO, 'reference', 'vu'), exist_ok=True)
    python('decomp', os.path.join(REPO, 'scripts', 'vu', 'vutext.py'))
    python('decomp', os.path.join(REPO, 'scripts', 'vu', 'vuflow.py'))
    print('  reference/vu: the VU1 microprograms')


# 4. generate --------------------------------------------------------------------------------------------------------

def generate():
    step('4/8 generate: the tables from the ELF')
    for script in GENERATORS:
        extra = [os.path.join(REPO, 'src', 'game', 'story', 'parts_tables.inc')] \
            if script.endswith('gen_parts_tables.py') else []
        python('generate', script, *extra)
    print('  %d generators run' % len(GENERATORS))


# 5-7. tools, data, game ---------------------------------------------------------------------------------------------

def msbuild(name, exe, dxsdk, targets=None, arch='SSE2'):
    # The arch is RTAGlass.vcxproj's alone (RTAGlassArch); the converters keep the default, so the data doesn't change.
    args = [exe, os.path.join(REPO, 'RTAGlass.sln'), '-p:Configuration=Release', '-p:Platform=x86',
            '-p:DXSDKPath=' + dxsdk, '-p:RTAGlassArch=' + arch, '-m', '-nologo', '-v:m']
    if targets:
        args.append('-t:' + ';'.join(targets))
    run(name, args)


def build_tools(exe, dxsdk):
    step('5/8 tools: the converters')
    msbuild('tools', exe, dxsdk, ['texconv', 'modelconv', 'sndconv', 'rpak', 'adpconv'])
    print('  bin/Release: texconv, modelconv, sndconv, rpak, adpconv')


def convert_data():
    step('6/8 data: converting the disc')
    data = os.path.join(REPO, 'data')
    for entry in os.listdir(data) if os.path.isdir(data) else []:
        if os.path.isdir(os.path.join(data, entry)):
            shutil.rmtree(os.path.join(data, entry))       # always from scratch: data/README.md stays
    for tool, what in (('texconv', 'textures'), ('modelconv', 'models, collision, AI paths, cameras, messages'),
                       ('sndconv', 'sound')):
        t = time.time()
        run('data', [os.path.join(BIN, tool + '.exe'), GAME, data])
        print('  %-9s %s (%.0f s)' % (tool, what, time.time() - t))
    python('data', os.path.join(REPO, 'scripts', 'course', 'make_c16.py'), data)
    print('  make_c16  the Test Course\'s path, map and camera')


def build_game(exe, dxsdk, arch):
    step('7/8 game: RTAGlass.exe and the player folder')
    python('game', os.path.join(REPO, 'scripts', 'make_icon.py'))
    packs = os.path.join(REPO, fingerprint.DIST, 'data')
    if os.path.isdir(packs):
        shutil.rmtree(packs)                                # repacked from the new data
    msbuild('game', exe, dxsdk, arch=arch)
    print('  %s (%s)' % (os.path.join(REPO, fingerprint.DIST), arch))


# 8. verify ----------------------------------------------------------------------------------------------------------

def verify():
    step('8/8 verify: against the reference build')
    if fingerprint.check(REPO):
        return
    print('\nThe build works, but isn\'t byte for byte the reference. If only tools\\*.exe differ, the cause is '
          'usually a different MSVC v141 toolset update (the reference used the one in expected.txt\'s header).')


# Interactive options -----------------------------------------------------------------------------------------------

ARCHES = (('SSE2', 'SSE2: Pentium 4, Athlon 64 and later (the reference build)'),
          ('SSE', 'SSE: also Pentium III and Athlon XP'),
          ('IA32', 'IA32: any Pentium (x87 floating point)'))


def ask(question, options, default):
    """Asks for one of `options` by number; Enter takes `default`. Returns its index."""
    print('\n' + question)
    for i, option in enumerate(options):
        print('  %d. %s%s' % (i + 1, option, ' (default)' if i == default else ''))
    while True:
        answer = input('Choice [%d]: ' % (default + 1)).strip()
        if not answer:
            return default
        if answer.isdigit() and 1 <= int(answer) <= len(options):
            return int(answer) - 1
        print('  type a number from 1 to %d' % len(options))


def ask_options(args):
    """Run with no arguments from a console (build.bat double-clicked): the choices the options would give."""
    print('RTAGlass build. Press Enter to take the defaults.')
    args.arch = ARCHES[ask('Which CPUs should the game run on?', [text for _, text in ARCHES], 0)][0]
    args.no_verify = ask('Compare the result with the reference build at the end?',
                         ['Yes', 'No (a little faster)'], 0) == 1
    print('\nThe same build without the questions: build.bat --arch %s%s'
          % (args.arch, ' --no-verify' if args.no_verify else ''))


def main():
    parser = argparse.ArgumentParser(description='Build RTAGlass from your copy of Road Trip.')
    parser.add_argument('image', nargs='?', help='the disc image: .cue (with its .bin), .bin or .iso')
    parser.add_argument('--game', help='an extracted disc instead of an image')
    parser.add_argument('--dxsdk', help='the DirectX SDK (June 2010) folder, if not at its default path')
    parser.add_argument('--no-verify', action='store_true', help='skip the comparison with the reference build')
    parser.add_argument('--arch', choices=('SSE2', 'SSE', 'IA32'), default='SSE2',
                        help='the CPU the game needs: SSE2 (default), SSE (Pentium III, Athlon XP) or IA32 (x87, any '
                             'Pentium); the converters always use the default')
    args = parser.parse_args()
    # No arguments and a console to answer in: ask instead of taking every default silently.
    args.interactive = len(sys.argv) == 1 and sys.stdin.isatty()
    if args.interactive:
        try:
            ask_options(args)
        except (KeyboardInterrupt, EOFError):
            print('\nBuild cancelled.')
            return 1
    start = time.time()
    if os.path.isdir(LOGS):
        shutil.rmtree(LOGS)
    try:
        exe, dxsdk = check(args)
        extract_disc(args)
        decomp()
        generate()
        build_tools(exe, dxsdk)
        convert_data()
        build_game(exe, dxsdk, args.arch)
        if not args.no_verify:
            verify()
    except BuildError as e:
        print('\nBuild failed: %s' % e)
        return 1
    except (KeyboardInterrupt, EOFError):
        print('\nBuild cancelled.')
        return 1
    print('\nDone in %d:%02d. The game is in %s: copy that folder anywhere to play.'
          % (int(time.time() - start) // 60, int(time.time() - start) % 60, os.path.join(REPO, fingerprint.DIST)))
    return 0


if __name__ == '__main__':
    sys.exit(main())
