#!/usr/bin/env python3
"""Replace build-host DT_RPATH entries in staged ELF files with an install-path DT_RUNPATH.

Usage: python3 -I normalize-rpath.py <dir> --runpath PATH[:PATH...] [--check]

For every regular ELF64 little-endian file under <dir> that has a DT_RPATH, or a
DT_RUNPATH with an absolute or empty entry (an empty entry means the current
directory; the staged libQt6Core carries 23 of them), the dynamic tag becomes DT_RUNPATH and the
string becomes --runpath, which must be absolute (for example the Qt library
directory on the device, `/data/qt612-public/lib`). The new string is written over
the old one in .dynstr, so the file size and every other offset stay as they were.
A string that would not fit is an error. With --check nothing is written; the exit
code is 1 when a file would change.

Why DT_RUNPATH: QNX's loader searches DT_RPATH before LD_LIBRARY_PATH, so a user's
replacement Qt library on LD_LIBRARY_PATH is ignored whenever the build-host
directory exists. DT_RUNPATH is searched after LD_LIBRARY_PATH.

Why an absolute path and not `$ORIGIN`: QNX 8.0.5's ldqnx-64.so.2 does not expand
`$ORIGIN`. DL_DEBUG=libs on the QEMU VM shows it trying the literal
`$ORIGIN/lib/libfoo.so.1` (docs/evidence/distribution-2026-11-21/vm/rpath-probe.txt).

Run this after stage-runtime.py and before the SHA256SUMS are written: it changes
the files' hashes. When <dir>/manifest.json exists (stage-runtime.py writes it),
the entries of the changed files are updated to the new hashes.
"""
import hashlib, json, os, struct, sys
from pathlib import Path

DT_STRTAB, DT_RPATH, DT_RUNPATH = 5, 15, 29
PT_LOAD, PT_DYNAMIC = 1, 2


def patch(path, new_for, check):
    """Return (old, new) when the file's search path is rewritten (or would be), else None."""
    with open(path, 'rb') as f:
        data = bytearray(f.read())
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
        return None
    base = dyn[0]
    entries = [(base + o, *struct.unpack_from('<qQ', data, base + o)) for o in range(0, dyn[1], 16)]
    strtab = next((v for _, t, v in entries if t == DT_STRTAB), None)
    soff = next((o + (strtab - va) for va, o, sz in loads if strtab is not None and va <= strtab < va + sz), None)
    if soff is None:
        raise ValueError(f'{path}: cannot locate .dynstr')
    hits = [(at, t, v) for at, t, v in entries if t in (DT_RPATH, DT_RUNPATH)]
    if not hits:
        return None
    if len(hits) != 1:
        raise ValueError(f'{path}: {len(hits)} DT_RPATH/DT_RUNPATH entries')
    at, tag, idx = hits[0]
    end = data.index(b'\0', soff + idx)
    old = data[soff + idx:end].decode()
    entries_old = old.split(':')
    if tag == DT_RUNPATH and all(e and not e.startswith('/') for e in entries_old):
        return None
    new = new_for(path)
    if tag == DT_RUNPATH and old == new:
        return None
    if len(new) > len(old):
        raise ValueError(f'{path}: new search path {new!r} is longer than the old one {old!r}')
    if not check:
        data[soff + idx:end] = new.encode() + b'\0' * (len(old) - len(new))
        struct.pack_into('<q', data, at, DT_RUNPATH)
        tmp = Path(str(path) + '.rpath-tmp')
        tmp.write_bytes(data)
        os.chmod(tmp, os.stat(path).st_mode)
        os.replace(tmp, path)
    return old, new


def main(argv):
    args = list(argv)
    if not args or args[0].startswith('-'):
        print(__doc__)
        return 2
    root = Path(args.pop(0)).resolve()
    runpath, check = None, False
    while args:
        a = args.pop(0)
        if a == '--runpath':
            runpath = args.pop(0)
        elif a == '--check':
            check = True
        else:
            print(f'unknown argument {a}')
            return 2
    if not runpath or not all(e.startswith('/') for e in runpath.split(':')):
        print('--runpath is required and every entry must be an absolute path')
        return 2

    def new_for(path):
        return runpath

    changed, touched = 0, []
    for p in sorted(root.rglob('*')):
        if p.is_symlink() or not p.is_file():
            continue
        result = patch(p, new_for, check)
        if result:
            changed += 1
            touched.append(str(p.relative_to(root)))
            print(f'{"WOULD CHANGE" if check else "changed"} {p.relative_to(root)}: {result[0]} -> {result[1]}')
    manifest = root / 'manifest.json'
    if touched and not check and manifest.is_file():
        # stage-runtime.py records each file's hash in manifest.json; keep it true.
        entries = json.loads(manifest.read_text())
        for rel in touched:
            if rel in entries and 'sha256' in entries[rel]:
                data = (root / rel).read_bytes()
                entries[rel] = {'sha256': hashlib.sha256(data).hexdigest(), 'bytes': len(data)}
        manifest.write_text(json.dumps(entries, indent=2, sort_keys=True) + '\n')
        print(f'manifest.json updated for {len(touched)} file(s)')
    print(f'{changed} file(s) {"would change" if check else "changed"}')
    return 1 if (check and changed) else 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
