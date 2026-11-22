#!/usr/bin/env python3
"""Check a staged Qt runtime directory against the distribution gate's closure rules.

Usage: python3 -I verify-closure.py <runtime-dir> [--json out.json] [--sums SHA256SUMS]
                                    [--runpath-prefix /install/dir] [--allow-system name ...]

Reads ELF64 little-endian files directly (no readelf, no SDK), so it runs on the
host as well as in the build container. Every check prints PASS or FAIL and the
exit code is 1 when any check fails.

Checks:
  sums      every file listed in SHA256SUMS exists with that SHA-256, and every
            regular file in the directory (except SHA256SUMS) is listed.
  manifest  manifest.json (stage-runtime.py) matches the files' hashes and symlinks.
  needed    every DT_NEEDED entry of every ELF file is either a file in the
            runtime (matched by file name or DT_SONAME) or on the system list.
  qt-closed every DT_NEEDED entry starting with libQt6 resolves inside the runtime.
  rpath     no ELF file carries a DT_RPATH (QNX's loader searches it before
            LD_LIBRARY_PATH), and no DT_RUNPATH with an empty entry or an absolute path
            unless the path is at or below the normalized absolute --runpath-prefix
            on a directory boundary. Absolute paths with dot segments or repeated
            slashes are rejected. Relative-path behavior is unchanged.
  no-sdp    no file in the runtime has the name of a QNX system library.
  notices   licenses/ holds the LGPL-3.0 text and THIRD_PARTY_ATTRIBUTIONS.txt, and
            a licenses/<module>/ directory exists for each Qt module a staged
            library or plugin comes from.

The system list defaults to the libraries the 2026-11-12 Pi 400 receipt names as
QNX base-image libraries; --allow-system adds more.
"""
import hashlib, json, os, struct, sys
from pathlib import Path

SYSTEM = {
    'libEGL.so.1', 'libGLESv2.so.1', 'libc++.so.2', 'libc.so.6', 'libeventfd.so.1',
    'libfsnotify.so.1', 'libgcc_s.so.1', 'libm.so.3', 'libscreen.so.1', 'libslog2.so.1',
    'libsocket.so.4', 'libzstd.so.1', 'libz.so.2', 'libcatalog.so.1', 'libregex.so.1',
}
# Names that must never be staged: they are QNX SDP or board-image payloads.
SDP_NAMES = SYSTEM - {'libzstd.so.1'} | {'libEGL-mesa.so', 'libGLESv2-mesa.so', 'libglapi-mesa.so',
                                         'v3d_dri.so', 'libdrm.so.1', 'libWFDdummy.so'}
# Qt module that each staged Qt library or plugin comes from (for the notices check).
MODULE_OF = {'libQt6Svg': 'qtsvg', 'libqsvg': 'qtsvg', 'libqwebp': 'qtimageformats',
             'libqtiff': 'qtimageformats', 'libqtga': 'qtimageformats',
             'libqicns': 'qtimageformats', 'libqwbmp': 'qtimageformats',
             'libQt6QuickShapes': 'qtdeclarative', 'libQt6Qml': 'qtdeclarative',
             'libQt6QmlMeta': 'qtdeclarative', 'libQt6QmlModels': 'qtdeclarative',
             'libQt6QmlWorkerScript': 'qtdeclarative', 'libQt6Quick': 'qtdeclarative'}

PT_LOAD, PT_DYNAMIC = 1, 2
DT = {1: 'NEEDED', 5: 'STRTAB', 14: 'SONAME', 15: 'RPATH', 29: 'RUNPATH'}


def sha256(path):
    h = hashlib.sha256()
    with open(path, 'rb') as f:
        for chunk in iter(lambda: f.read(1 << 20), b''):
            h.update(chunk)
    return h.hexdigest()


def read_elf(path):
    """Return {'needed': [...], 'soname': str|None, 'rpath': [...], 'runpath': [...]} or None."""
    with open(path, 'rb') as f:
        data = f.read()
    if data[:4] != b'\x7fELF' or data[4] != 2 or data[5] != 1:
        return None
    phoff, = struct.unpack_from('<Q', data, 0x20)
    phentsize, phnum = struct.unpack_from('<HH', data, 0x36)
    loads, dyn = [], None
    for i in range(phnum):
        p_type, _, p_off, p_vaddr, _, p_filesz, _, _ = struct.unpack_from('<IIQQQQQQ', data, phoff + i * phentsize)
        if p_type == PT_LOAD:
            loads.append((p_vaddr, p_off, p_filesz))
        elif p_type == PT_DYNAMIC:
            dyn = (p_off, p_filesz)
    if dyn is None:
        return {'needed': [], 'soname': None, 'rpath': [], 'runpath': []}
    entries = [struct.unpack_from('<qQ', data, dyn[0] + o) for o in range(0, dyn[1], 16)]
    strtab = next((v for t, v in entries if t == 5), None)
    if strtab is None:
        raise ValueError(f'{path}: no DT_STRTAB')
    off = next((o + (strtab - va) for va, o, sz in loads if va <= strtab < va + sz), None)
    if off is None:
        raise ValueError(f'{path}: DT_STRTAB outside every PT_LOAD')

    def s(idx):
        end = data.index(b'\0', off + idx)
        return data[off + idx:end].decode()

    out = {'needed': [], 'soname': None, 'rpath': [], 'runpath': []}
    for t, v in entries:
        if t == 1:
            out['needed'].append(s(v))
        elif t == 14:
            out['soname'] = s(v)
        elif t == 15:
            out['rpath'] = s(v).split(':')
        elif t == 29:
            out['runpath'] = s(v).split(':')
    return out


def normalized_absolute(path):
    """Lexical QNX path check; do not resolve target paths on the host filesystem."""
    return (path.startswith('/') and '//' not in path and
            all(part not in ('.', '..') for part in path.split('/')))


def within_install_prefix(entry, prefix):
    if not prefix or not normalized_absolute(entry):
        return False
    entry = entry.rstrip('/') or '/'
    return prefix == '/' or entry == prefix or entry.startswith(prefix + '/')


def main(argv):
    args = list(argv)
    if not args or args[0].startswith('-'):
        print(__doc__)
        return 2
    root = Path(args.pop(0)).resolve()
    out_json = sums_name = None
    system = set(SYSTEM)
    runpath_prefix = None
    while args:
        a = args.pop(0)
        if a == '--json':
            out_json = args.pop(0)
        elif a == '--sums':
            sums_name = args.pop(0)
        elif a == '--runpath-prefix':
            if not args or not normalized_absolute(args[0]):
                print('--runpath-prefix must be a normalized absolute install path')
                return 2
            runpath_prefix = args.pop(0).rstrip('/') or '/'
        elif a == '--allow-system':
            while args and not args[0].startswith('--'):
                system.add(args.pop(0))
        else:
            print(f'unknown argument {a}')
            return 2
    sums_file = root / (sums_name or 'SHA256SUMS')
    results, report = [], {'root': str(root), 'files': {}, 'checks': {}}

    def check(name, failures, detail=''):
        results.append((name, not failures))
        report['checks'][name] = {'pass': not failures, 'failures': failures}
        print(f"{'PASS' if not failures else 'FAIL'} {name} {detail}".rstrip())
        for f in failures[:40]:
            print(f'     {f}')
        if len(failures) > 40:
            print(f'     ... {len(failures) - 40} more')

    files = sorted(p for p in root.rglob('*') if p.is_file() or p.is_symlink())
    rel = {p: str(p.relative_to(root)) for p in files}
    regular = [p for p in files if p.is_file() and not p.is_symlink()]

    # sums
    fails, listed = [], {}
    if not sums_file.exists():
        fails.append(f'{sums_file.name} missing')
    else:
        for line in sums_file.read_text().splitlines():
            if not line.strip():
                continue
            digest, name = line.split(None, 1)
            listed[name.strip().lstrip('*').removeprefix('./')] = digest
        for name, digest in listed.items():
            p = root / name
            if not p.is_file():
                fails.append(f'listed but missing: {name}')
            elif sha256(p) != digest:
                fails.append(f'hash mismatch: {name}')
        for p in regular:
            if rel[p] != sums_file.name and rel[p] not in listed:
                fails.append(f'not listed: {rel[p]}')
    check('sums', fails, f'({len(listed)} listed, {len(regular) - (1 if sums_file.is_file() else 0)} files)')
    for p in regular:
        report['files'][rel[p]] = {'sha256': sha256(p), 'size': p.stat().st_size}

    # manifest.json (written by stage-runtime.py) must describe the files as they are.
    fails = []
    manifest = root / 'manifest.json'
    if manifest.is_file():
        for name, meta in json.loads(manifest.read_text()).items():
            if 'sha256' in meta:
                p = root / name
                if not p.is_file():
                    fails.append(f'manifest.json lists missing file {name}')
                elif sha256(p) != meta['sha256']:
                    fails.append(f'manifest.json hash differs: {name}')
            elif 'symlink' in meta and os.readlink(root / name) != meta['symlink']:
                fails.append(f'manifest.json symlink differs: {name}')
    else:
        fails.append('manifest.json missing')
    check('manifest', fails)

    # ELF scan
    by_name = {p.name for p in files}
    sonames = {}
    elf = {}
    for p in regular:
        info = read_elf(p)
        if info is None:
            continue
        elf[rel[p]] = info
        report['files'][rel[p]].update(info)
        if info['soname']:
            sonames[info['soname']] = rel[p]
    fails, qt_fails, sys_used = [], [], set()
    for name, info in elf.items():
        for n in info['needed']:
            if n in by_name or n in sonames:
                continue
            if n in system:
                sys_used.add(n)
                continue
            (qt_fails if n.startswith('libQt6') else fails).append(f'{name} needs {n}')
    check('needed', fails, f'({len(elf)} ELF files, system libraries used: {len(sys_used)})')
    check('qt-closed', qt_fails)
    report['system_libraries_used'] = sorted(sys_used)
    fails = []
    for name, info in elf.items():
        if info['rpath']:
            fails.append(f"{name} DT_RPATH {':'.join(info['rpath'])!r}")
        for entry in info['runpath']:
            if not entry:
                fails.append(f'{name} DT_RUNPATH has an empty entry (current directory)')
            elif entry.startswith('/') and not within_install_prefix(entry, runpath_prefix):
                fails.append(f'{name} DT_RUNPATH {entry}')
    check('rpath', fails)
    check('no-sdp', [rel[p] for p in files if p.name in SDP_NAMES])

    # notices
    fails = []
    lic = root / 'licenses'
    if not lic.is_dir():
        fails.append('licenses/ missing')
    else:
        if not (lic / 'THIRD_PARTY_ATTRIBUTIONS.txt').is_file():
            fails.append('licenses/THIRD_PARTY_ATTRIBUTIONS.txt missing')
        modules = {'qtbase', 'qtdeclarative'}
        for name in elf:
            base = Path(name).name.split('.so')[0]
            if base in MODULE_OF:
                modules.add(MODULE_OF[base])
        for m in sorted(modules):
            if not (lic / m / 'LGPL-3.0-only.txt').is_file():
                fails.append(f'licenses/{m}/LGPL-3.0-only.txt missing')
    check('notices', fails)

    report['pass'] = all(ok for _, ok in results)
    if out_json:
        Path(out_json).write_text(json.dumps(report, indent=1, sort_keys=True) + '\n')
    print('RESULT', 'PASS' if report['pass'] else 'FAIL')
    return 0 if report['pass'] else 1


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
