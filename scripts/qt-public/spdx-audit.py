#!/usr/bin/env python3
"""License audit of everything the compiler read for the selected QNX targets.

Usage: spdx-audit.py <ninja-build-dir> <source-root> <out.json> <target>...

1. `ninja -t commands <target>...` gives every compile command; each `-o`
   object and `-c` translation unit is recorded.
2. `ninja -t deps <object>...` gives the files the compiler actually read for
   each object (translation unit plus every header and every `#include`d
   .c/.cpp), from the depfiles ninja stored during the build.
3. Each file under <source-root> is classified:
   - files below a `3rdparty` directory are grouped under the nearest
     `qt_attribution.json` and reported with every license that file names;
   - other files report the SPDX expression in their first 40 lines.
   Files whose expression offers no LGPL option are flagged, and any
   qt_attribution.json beside them that names a file read is reported.
4. Files under the build directory (moc, rcc, qmltyperegistrar and other
   generated output) are counted per generator directory, not read.
   Toolchain and QNX sysroot headers are counted only.
"""
import json, re, subprocess, sys
from collections import Counter, defaultdict
from pathlib import Path

build = Path(sys.argv[1]).resolve()
src = Path(sys.argv[2]).resolve()
out = Path(sys.argv[3])
targets = sys.argv[4:]


def ninja(*args):
    return subprocess.run(['ninja', '-C', str(build), *args], check=True,
                          capture_output=True, text=True).stdout


objects, units = [], set()
for c in ninja('-t', 'commands', *targets).splitlines():
    o = re.search(r'\s-o\s+(\S+\.o)\b', c)
    u = re.search(r'\s-c\s+(\S+)', c)
    if o and u:
        objects.append(o.group(1))
        p = Path(u.group(1))
        units.add((p if p.is_absolute() else build / p).resolve())

read = set()
for i in range(0, len(objects), 200):
    for line in ninja('-t', 'deps', *objects[i:i + 200]).splitlines():
        line = line.strip()
        if line and not line.endswith(')') and not line.startswith('#'):
            p = Path(line)
            read.add((p if p.is_absolute() else build / p).resolve())
missing_units = sorted(str(u) for u in units - read)
read |= units

spdx_re = re.compile(r'SPDX-License-Identifier:\s*(.+?)\s*(\*/|-->)?\s*$')


def spdx(p):
    try:
        with open(p, errors='replace') as f:
            for i, line in enumerate(f):
                m = spdx_re.search(line)
                if m:
                    return m.group(1).strip()
                if i > 40:
                    return None
    except OSError:
        return None


def attribution(p):
    for d in p.parents:
        if d == src:
            return None
        a = d / 'qt_attribution.json'
        if a.is_file():
            return a
    return None


def licenses_of(a):
    data = json.loads(a.read_text(errors='replace'), strict=False)
    data = data if isinstance(data, list) else [data]
    return [{'name': e.get('Name'), 'version': e.get('Version'),
             'license': e.get('LicenseId'), 'copyright': e.get('Copyright'),
             'license_files': e.get('LicenseFiles') or e.get('LicenseFile')}
            for e in data]


qt_files, third, flagged = Counter(), defaultdict(list), []
generated, toolchain = Counter(), 0
for p in sorted(read):
    if p.is_relative_to(build):
        rel = p.relative_to(build)
        generated['/'.join(rel.parts[:3])] += 1
        continue
    if not p.is_relative_to(src):
        toolchain += 1
        continue
    rel = str(p.relative_to(src))
    if '/3rdparty/' in '/' + rel:
        a = attribution(p)
        key = str(a.parent.relative_to(src)) if a else str(p.parent.relative_to(src))
        third[key].append({'file': rel, 'spdx': spdx(p)})
        continue
    expr = spdx(p)
    qt_files[expr or '(none in first 40 lines)'] += 1
    if expr is None or 'LGPL' not in expr:
        flagged.append({'file': rel, 'spdx': expr})

components = {}
for d, files in sorted(third.items()):
    a = src / d / 'qt_attribution.json'
    components[d] = {
        'attribution': licenses_of(a) if a.is_file() else None,
        'files_read': len(files),
        'spdx_in_files': sorted({f['spdx'] for f in files if f['spdx']}),
        'license_files_in_dir': sorted(
            str(x.relative_to(src)) for x in (src / d).rglob('*')
            if x.is_file() and re.search(r'(LICEN[CS]E|COPYING)|^FTL\.TXT$', x.name)),
    }

# qt_attribution.json files outside 3rdparty/ that name a file the compiler
# read (bundled code kept beside Qt sources, e.g. Unicode data, Khronos headers).
embedded = []
dirs = {p.parent for p in read if p.is_relative_to(src) and '/3rdparty/' not in '/' + str(p.relative_to(src))}
for d in sorted(dirs):
    a = d / 'qt_attribution.json'
    if not a.is_file():
        continue
    data = json.loads(a.read_text(errors='replace'), strict=False)
    for e in (data if isinstance(data, list) else [data]):
        files = e.get('Files') or []
        files = files.split() if isinstance(files, str) else files
        hit = [f for f in files if (d / f).resolve() in read]
        if hit:
            embedded.append({'attribution': str(a.relative_to(src)), 'name': e.get('Name'),
                             'license': e.get('LicenseId'), 'copyright': e.get('Copyright'),
                             'license_files': e.get('LicenseFiles') or e.get('LicenseFile'),
                             'files_read': hit})

report = {
    'targets': targets,
    'objects': len(objects),
    'translation_units': len(units),
    'translation_units_without_deps_entry': missing_units,
    'files_read_total': len(read),
    'qt_files_by_expression': dict(qt_files),
    'qt_files_without_lgpl_option': flagged,
    'third_party_components': components,
    'embedded_attributions': embedded,
    'generated_files_by_dir': dict(sorted(generated.items())),
    'toolchain_and_sysroot_files': toolchain,
}
out.write_text(json.dumps(report, indent=2) + '\n')
print(json.dumps({k: report[k] for k in ('objects', 'translation_units',
      'files_read_total', 'qt_files_by_expression', 'toolchain_and_sysroot_files')}, indent=2))
print('qt files without LGPL option:', len(flagged))
for d, c in components.items():
    print(f"{d}: {c['files_read']} files; "
          f"{[e['license'] for e in (c['attribution'] or [])]}; spdx {c['spdx_in_files']}")
