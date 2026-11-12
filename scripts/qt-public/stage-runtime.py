#!/usr/bin/env python3
"""Stage the public-source Qt 6.12.0 QNX runtime closure, verify its DT_NEEDED
closure, and write sha256 manifests.

Run inside qnx805-build after sourcing qnxsdp-env.sh (needs ntoaarch64-readelf).
The staged layout matches the private evaluation runtime minus its smoke binary.
When the evaluation manifest is present, the file lists are compared; it is
never read for content and nothing from it is copied.
"""
import hashlib, json, os, re, shutil, subprocess, sys
from pathlib import Path

stage = Path('/home/qnxbuild/qt-6.12.0-public/qnx-build/qtbase')
src = Path('/work/qt-6.12.0-public/src/qt-everywhere-src-6.12.0')
out = Path(sys.argv[1] if len(sys.argv) > 1 else '/work/qt-6.12.0-public/runtime')
eval_manifest = Path('/work/qt612-runtime/manifest.json')

LIBS = ['Core', 'Gui', 'Network', 'OpenGL', 'Qml', 'QmlMeta', 'QmlModels',
        'QmlWorkerScript', 'Quick', 'QuickShapes']
PLUGINS = ['plugins/platforms/libqqnx.so']
QML = {
    'qml/QML': ['qmldir', 'plugins.qmltypes', 'jsroot.qmltypes'],
    'qml/QtQml': ['qmldir', 'plugins.qmltypes', 'libqmlplugin.so'],
    'qml/QtQml/Models': ['qmldir', 'plugins.qmltypes', 'libmodelsplugin.so'],
    'qml/QtQml/WorkerScript': ['qmldir', 'plugins.qmltypes', 'libworkerscriptplugin.so'],
    'qml/QtQuick': ['qmldir', 'plugins.qmltypes', 'libqtquick2plugin.so'],
    'qml/QtQuick/Shapes': ['qmldir', 'plugins.qmltypes', 'libqmlshapesplugin.so'],
}
MODULES = ('qtbase', 'qtdeclarative', 'qtshadertools')

if out.exists():
    shutil.rmtree(out)
entries = {}

def copy(rel, source):
    if not source.is_file():
        sys.exit(f'missing in public build: {rel} ({source})')
    dst = out / rel
    dst.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source, dst)
    data = dst.read_bytes()
    entries[rel] = {'sha256': hashlib.sha256(data).hexdigest(), 'bytes': len(data)}

for lib in LIBS:
    real = f'lib/libQt6{lib}.so.6.12.0'
    copy(real, stage / real)
    for link, target in ((f'lib/libQt6{lib}.so.6', f'libQt6{lib}.so.6.12.0'),
                         (f'lib/libQt6{lib}.so', f'libQt6{lib}.so.6')):
        os.symlink(target, out / link)
        entries[link] = {'symlink': target}
for rel in PLUGINS:
    copy(rel, stage / rel)
for d, names in QML.items():
    for n in names:
        copy(f'{d}/{n}', stage / d / n)
# License texts of the selected submodules, from the public source tree.
for mod in MODULES:
    base = src / mod
    for f in sorted((base / 'LICENSES').glob('*')):
        copy(f'licenses/{mod}/{f.name}', f)
    for f in sorted(base.glob('src/3rdparty/**/*')):
        # LICENSE/COPYING anywhere in the name (BDF-LICENSE.txt, PSL-LICENSE.txt)
        # plus FreeType's FTL text, which the FTL option requires to accompany it.
        if f.is_file() and (re.search(r'(LICEN[CS]E|COPYING)', f.name) or f.name == 'FTL.TXT'):
            copy(f'licenses/{mod}/third-party/{f.relative_to(base / "src/3rdparty")}', f)

def needed(p):
    t = subprocess.run(['ntoaarch64-readelf', '-d', str(p)], capture_output=True,
                       text=True, check=True).stdout
    return re.findall(r'\(NEEDED\)\s+Shared library: \[(.+?)\]', t)

staged_sonames = {p.name for p in (out / 'lib').iterdir()}
closure, external = {}, set()
for rel, meta in sorted(entries.items()):
    if 'sha256' not in meta or not re.search(r'\.so(\.6\.12\.0)?$', rel):
        continue
    n = needed(out / rel)
    closure[rel] = n
    for lib in n:
        if lib.startswith('libQt6'):
            if lib not in staged_sonames:
                sys.exit(f'{rel} needs {lib}, not staged')
        else:
            external.add(lib)

report = {'entries': len(entries), 'elf': len(closure),
          'non_qt_system_libraries': sorted(external)}
if eval_manifest.exists():
    want = {k for k in json.loads(eval_manifest.read_text())
            if not k.startswith('bin/')}
    have = set(entries)
    report['vs_evaluation_layout'] = {
        'only_in_evaluation': sorted(want - have),
        'only_in_public': sorted(have - want)}
(out / 'manifest.json').write_text(json.dumps(entries, indent=2, sort_keys=True) + '\n')
(out / 'needed.json').write_text(json.dumps({'needed': closure, **report}, indent=2) + '\n')
print(json.dumps(report, indent=2))
