#!/usr/bin/env python3
"""Stage the public-source Qt 6.12.0 QNX runtime closure, verify its DT_NEEDED
closure, and write sha256 manifests.

Usage: stage-runtime.py [<out-dir> [<spdx-audit.json>...]]
With audits for the base and imageformats build trees, also writes licenses/THIRD_PARTY_ATTRIBUTIONS.txt and
copies the license files named by each qt_attribution.json the audit found.
Paths come from layout.sh's QT_PUBLIC_ROOT and QT_PUBLIC_BUILD.
The qtimageformats plugins and notices come from IMAGEFORMATS_BUILD and
IMAGEFORMATS_SRC respectively (build-imageformats-qnx.sh).
Run inside qnx805-build after sourcing qnxsdp-env.sh (needs ntoaarch64-readelf).
The staged layout matches the private evaluation runtime minus its smoke binary.
When the evaluation manifest is present, the file lists are compared; it is
never read for content and nothing from it is copied.
"""
import hashlib, json, os, re, shutil, subprocess, sys, tempfile
from contextlib import contextmanager
from pathlib import Path

root = Path(os.environ.get('QT_PUBLIC_ROOT', '/work/qt-6.12.0-public'))
stage = Path(os.environ.get('QT_PUBLIC_BUILD', '/home/qnxbuild/qt-6.12.0-public')) / 'qnx-build/qtbase'
src = root / 'src/qt-everywhere-src-6.12.0'
svg_build = Path(os.environ.get('SVG_BUILD', stage.parent.parent / 'svg-qnx'))
imageformats = Path(os.environ.get('IMAGEFORMATS_BUILD', stage.parent.parent / 'imageformats-qnx'))
imageformats_src = (Path(os.environ.get('IMAGEFORMATS_SRC', root / 'src')) /
                    'qt-everywhere-src-6.12.0/qtimageformats')
out = Path(sys.argv[1] if len(sys.argv) > 1 else root / 'runtime')
eval_manifest = Path(os.environ.get('QT_EVAL_MANIFEST', '/work/qt612-runtime/manifest.json'))

LIBS = ['Core', 'Gui', 'Network', 'OpenGL', 'Qml', 'QmlMeta', 'QmlModels',
        'QmlWorkerScript', 'Quick', 'QuickShapes']
# TLS backend: without it Qt Network has no HTTPS (remote <Image> URLs, Metro
# over https). JPEG/GIF: without them Qt Quick decodes only PNG, so most remote
# photos fail. Built by configure-qnx.sh.
PLUGINS = ['plugins/platforms/libqqnx.so',
           'plugins/tls/libqopensslbackend.so',
           'plugins/imageformats/libqjpeg.so',
           'plugins/imageformats/libqgif.so']
# Additional formats are built separately by build-imageformats-qnx.sh.
IMAGEFORMATS_PLUGINS = ['plugins/imageformats/' + name for name in
                        ('libqwebp.so', 'libqtiff.so', 'libqtga.so', 'libqicns.so', 'libqwbmp.so')]
QML = {
    'qml/QML': ['qmldir', 'plugins.qmltypes', 'jsroot.qmltypes'],
    'qml/QtQml': ['qmldir', 'plugins.qmltypes', 'libqmlplugin.so'],
    'qml/QtQml/Models': ['qmldir', 'plugins.qmltypes', 'libmodelsplugin.so'],
    'qml/QtQml/WorkerScript': ['qmldir', 'plugins.qmltypes', 'libworkerscriptplugin.so'],
    'qml/QtQuick': ['qmldir', 'plugins.qmltypes', 'libqtquick2plugin.so'],
    'qml/QtQuick/Shapes': ['qmldir', 'plugins.qmltypes', 'libqmlshapesplugin.so'],
}
MODULES = ('qtbase', 'qtdeclarative', 'qtshadertools', 'qtimageformats')

@contextmanager
def staging_destination(destination):
    # Never remove an input tree or follow an output symlink during replacement.
    destination = destination.absolute()
    if destination.is_symlink():
        raise ValueError(f'output must not be a symlink: {destination}')
    destination = destination.resolve()
    input_roots = (stage.resolve(), src.resolve(), svg_build.resolve(), imageformats.resolve(),
                   imageformats_src.resolve())
    protected = [root.resolve(), *input_roots, Path(__file__).resolve().parents[2]]
    if any(destination == p or destination in p.parents for p in protected):
        raise ValueError(f'output contains a source/build tree: {destination}')
    if any(destination.is_relative_to(p) for p in input_roots):
        raise ValueError(f'output is inside a source/build tree: {destination}')
    if destination.exists() and not destination.is_dir():
        raise ValueError(f'output must be a directory: {destination}')
    destination.parent.mkdir(parents=True, exist_ok=True)
    lock = destination.parent / f'.{destination.name}.stage-lock'
    try:
        lock.mkdir()
    except FileExistsError:
        raise ValueError(f'staging is locked: {lock}; check the owning process before removing it')
    work = None
    try:
        work = Path(tempfile.mkdtemp(prefix=f'.{destination.name}.stage-', dir=destination.parent))
        candidate = work / 'candidate'
        candidate.mkdir()
        backup = work / 'previous'
        # A process/power interruption cannot run finally. Leave an exact,
        # durable location for manual recovery instead of guessing by glob.
        journal = lock / 'transaction.json'
        with journal.open('w') as f:
            json.dump({'schema': 'rnqnx-qt-staging/v1', 'pid': os.getpid(),
                       'destination': str(destination), 'work': str(work),
                       'candidate': str(candidate), 'previous': str(backup)}, f)
            f.write('\n')
            f.flush()
            os.fsync(f.fileno())
        yield candidate
        # All copies, notices, readelf calls and manifests succeeded.
        try:
            if destination.exists():
                destination.rename(backup)
            candidate.rename(destination)
        except BaseException:
            if backup.exists() and not destination.exists():
                backup.rename(destination)
            raise
        if backup.exists():
            shutil.rmtree(backup)
    finally:
        # If rollback itself failed, retain the previous runtime for recovery.
        if work is not None:
            if (work / 'previous').exists():
                print(f'previous runtime retained for recovery: {work / "previous"}', file=sys.stderr)
            else:
                shutil.rmtree(work)
        if work is None or not (work / 'previous').exists():
            (lock / 'transaction.json').unlink(missing_ok=True)
            lock.rmdir()


with staging_destination(out) as out:
    entries = {}

    def copy(rel, source, *, source_roots=(stage, src)):
        destination = (out / rel).resolve()
        if not destination.is_relative_to(out) or destination == out:
            raise ValueError(f'output path escapes staging directory: {rel}')
        resolved = source.resolve()
        if not any(resolved.is_relative_to(p.resolve()) for p in source_roots):
            raise ValueError(f'source path escapes public source/build trees: {source}')
        if not source.is_file():
            sys.exit(f'missing in public build: {rel} ({source})')
        dst = out / rel
        dst.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, dst)
        data = dst.read_bytes()
        entries[rel] = {'sha256': hashlib.sha256(data).hexdigest(), 'bytes': len(data)}

    def module_source(mod):
        # The add-on builder can extract imageformats outside the base source
        # tree. Only that module's notice copies may read the selected root.
        if mod == 'qtimageformats':
            return imageformats_src, (imageformats_src,)
        return src / mod, (stage, src)

    for lib in LIBS:
        real = f'lib/libQt6{lib}.so.6.12.0'
        copy(real, stage / real)
        for link, target in ((f'lib/libQt6{lib}.so.6', f'libQt6{lib}.so.6.12.0'),
                             (f'lib/libQt6{lib}.so', f'libQt6{lib}.so.6')):
            os.symlink(target, out / link)
            entries[link] = {'symlink': target}
    for rel in PLUGINS:
        copy(rel, stage / rel)
    for rel in IMAGEFORMATS_PLUGINS:
        copy(rel, imageformats / rel, source_roots=(imageformats,))
    # QtSvg is built separately by build-svg-qnx.sh. Keep its artifacts inside
    # this transaction and constrain only these copies to the selected tree.
    if (svg_build / 'lib/libQt6Svg.so.6.12.0').is_file():
        copy('lib/libQt6Svg.so.6.12.0', svg_build / 'lib/libQt6Svg.so.6.12.0',
             source_roots=(svg_build,))
        for link, target in (('lib/libQt6Svg.so.6', 'libQt6Svg.so.6.12.0'),
                             ('lib/libQt6Svg.so', 'libQt6Svg.so.6')):
            os.symlink(target, out / link)
            entries[link] = {'symlink': target}
        copy('plugins/imageformats/libqsvg.so', svg_build / 'plugins/imageformats/libqsvg.so',
             source_roots=(svg_build,))
        MODULES = MODULES + ('qtsvg',)
    else:
        print(f'note: no QtSvg build at {svg_build}; the runtime will not decode SVG images', file=sys.stderr)
    for d, names in QML.items():
        for n in names:
            copy(f'{d}/{n}', stage / d / n)
    # License texts of the selected submodules, from the public source tree.
    for mod in MODULES:
        base, source_roots = module_source(mod)
        for f in sorted((base / 'LICENSES').glob('*')):
            copy(f'licenses/{mod}/{f.name}', f, source_roots=source_roots)
        for f in sorted(base.glob('src/3rdparty/**/*')):
            # LICENSE/COPYING anywhere in the name (BDF-LICENSE.txt, PSL-LICENSE.txt)
            # plus FreeType's FTL text, which the FTL option requires to accompany it.
            if f.is_file() and (re.search(r'(LICEN[CS]E|COPYING)', f.name) or f.name == 'FTL.TXT'):
                copy(f'licenses/{mod}/third-party/{f.relative_to(base / "src/3rdparty")}', f,
                     source_roots=source_roots)

    # Notices for bundled code the compiler read, from the SPDX audit's
    # qt_attribution.json entries. Some components ship no license file of their
    # own (emoji-segmenter, md4, md5, siphash, sha1, Unicode data); their
    # copyright line and SPDX id are written here, and the license text is the
    # module's LICENSES/<id>.txt copied above.
    audits = [Path(arg) for arg in sys.argv[2:]]
    if audits:
        items = []
        for audit in audits:
            a = json.loads(audit.read_text())
            items += [(d, e) for d, c in a['third_party_components'].items()
                      for e in (c['attribution'] or [])]
            items += [(str(Path(e['attribution']).parent), e) for e in a['embedded_attributions']]
        lines = ['Bundled third-party code compiled into this Qt 6.12.0 runtime.',
                 'Generated from qt_attribution.json entries listed by the SPDX audit.', '']
        for d, e in items:
            mod, sub = d.split('/', 1)
            base, source_roots = module_source(mod)
            files = e.get('license_files') or []
            files = files.split() if isinstance(files, str) else files
            texts = []
            for f in files:
                if (base / sub / f).is_file():
                    rel = os.path.normpath(f'licenses/{mod}/attributed/{sub}/{f}')
                    if rel not in entries:
                        copy(rel, base / sub / f, source_roots=source_roots)
                    texts.append(rel)
            ids = re.findall(r'[A-Za-z0-9.+-]+', e.get('license') or '')
            texts = [f'licenses/{mod}/{i}.txt' for i in ids
                     if f'licenses/{mod}/{i}.txt' in entries and i not in ('AND', 'OR')] + texts
            lines += [f"{e.get('name')} {e.get('version') or ''}".rstrip(),
                      f'  Source: {d}',
                      f"  License: {e.get('license')}",
                      '  Copyright: ' + ('\n    '.join(e['copyright']) if isinstance(e.get('copyright'), list)
                                        else (e.get('copyright') or '(see license text)')),
                      f"  License texts: {', '.join(texts) or 'none found'}", '']
        rel = 'licenses/THIRD_PARTY_ATTRIBUTIONS.txt'
        (out / rel).write_text('\n'.join(lines))
        data = (out / rel).read_bytes()
        entries[rel] = {'sha256': hashlib.sha256(data).hexdigest(), 'bytes': len(data)}


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
