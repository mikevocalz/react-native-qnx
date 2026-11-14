"""Host fixtures for QNX runtime staging; no Qt or SDP binaries are used."""
import hashlib
import json
import os
from pathlib import Path
import runpy
import subprocess
import sys
import tempfile
import unittest
from unittest import mock


SCRIPT = Path(__file__).resolve().parents[2] / 'scripts/qt-public/stage-runtime.py'
LIBS = ('Core', 'Gui', 'Network', 'OpenGL', 'Qml', 'QmlMeta', 'QmlModels',
        'QmlWorkerScript', 'Quick', 'QuickShapes')
QML = {
    'QML': ('qmldir', 'plugins.qmltypes', 'jsroot.qmltypes'),
    'QtQml': ('qmldir', 'plugins.qmltypes', 'libqmlplugin.so'),
    'QtQml/Models': ('qmldir', 'plugins.qmltypes', 'libmodelsplugin.so'),
    'QtQml/WorkerScript': ('qmldir', 'plugins.qmltypes', 'libworkerscriptplugin.so'),
    'QtQuick': ('qmldir', 'plugins.qmltypes', 'libqtquick2plugin.so'),
    'QtQuick/Shapes': ('qmldir', 'plugins.qmltypes', 'libqmlshapesplugin.so'),
}


class StagingTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='rnqnx-stage-test-')
        self.addCleanup(self.temp.cleanup)
        self.base = Path(self.temp.name)
        self.root = self.base / 'public'
        self.build = self.base / 'build'
        self.stage = self.build / 'qnx-build/qtbase'
        self.src = self.root / 'src/qt-everywhere-src-6.12.0'
        self.out = self.base / 'runtime'
        self.out.mkdir()
        (self.out / 'known-good').write_text('previous runtime')
        (self.out / 'link').symlink_to('known-good')
        for lib in LIBS:
            self.write(self.stage / f'lib/libQt6{lib}.so.6.12.0', lib)
        for rel in ('platforms/libqqnx.so', 'tls/libqopensslbackend.so',
                    'imageformats/libqjpeg.so', 'imageformats/libqgif.so'):
            self.write(self.stage / 'plugins' / rel, rel)
        for folder, names in QML.items():
            for name in names:
                self.write(self.stage / 'qml' / folder / name, name)
        for mod in ('qtbase', 'qtdeclarative', 'qtshadertools'):
            self.write(self.src / mod / 'LICENSES/LGPL-3.0-only.txt', 'license fixture')
        self.audit = self.base / 'audit.json'
        self.audit.write_text(json.dumps({'third_party_components': {}, 'embedded_attributions': []}))
        tool = self.base / 'bin/ntoaarch64-readelf'
        self.write(tool, f'#!{sys.executable}\nimport os, sys\n'
                   'if os.environ.get("READELF_FAIL"): sys.exit(9)\n'
                   'dependency = os.environ.get("READELF_NEEDED", "libQt6Core.so.6")\n'
                   'print(f" (NEEDED) Shared library: [{dependency}]")\n')
        tool.chmod(0o755)
        self.env = {**os.environ, 'QT_PUBLIC_ROOT': str(self.root),
                    'QT_PUBLIC_BUILD': str(self.build),
                    'QT_EVAL_MANIFEST': str(self.base / 'absent-evaluation.json'),
                    'PATH': str(tool.parent) + os.pathsep + os.environ['PATH']}

    @staticmethod
    def write(path, content):
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(content)

    def snapshot(self, folder=None):
        folder = folder or self.out
        return {str(p.relative_to(folder)): ('link', os.readlink(p)) if p.is_symlink()
                else ('file', p.read_bytes())
                for p in folder.rglob('*') if p.is_file() or p.is_symlink()}

    def run_stage(self, output=None):
        return subprocess.run([sys.executable, '-I', str(SCRIPT), str(output or self.out),
                               str(self.audit)], env=self.env, capture_output=True,
                              text=True, timeout=15)

    def assert_preserved_failure(self, expected):
        before = self.snapshot()
        result = self.run_stage()
        self.assertNotEqual(result.returncode, 0, result.stdout)
        self.assertIn(expected, result.stderr)
        self.assertEqual(self.snapshot(), before)
        self.assertEqual(list(self.out.parent.glob('.runtime.stage-*')), [])

    def test_missing_library_preserves_previous_runtime(self):
        (self.stage / 'lib/libQt6Core.so.6.12.0').unlink()
        self.assert_preserved_failure('missing in public build')

    def test_missing_plugin_preserves_previous_runtime(self):
        (self.stage / 'plugins/tls/libqopensslbackend.so').unlink()
        self.assert_preserved_failure('missing in public build')

    def test_malformed_audit_preserves_previous_runtime(self):
        self.audit.write_text('{')
        self.assert_preserved_failure('JSONDecodeError')

    def test_readelf_failure_preserves_previous_runtime(self):
        self.env['READELF_FAIL'] = '1'
        self.assert_preserved_failure('returned non-zero exit status 9')

    def test_missing_qt_dependency_preserves_previous_runtime(self):
        self.env['READELF_NEEDED'] = 'libQt6NotStaged.so.6'
        self.assert_preserved_failure('not staged')

    def test_source_symlink_escape_preserves_previous_runtime(self):
        lib = self.stage / 'lib/libQt6Core.so.6.12.0'
        lib.unlink()
        external = self.base / 'outside'
        external.write_text('not a public build artifact')
        lib.symlink_to(external)
        self.assert_preserved_failure('source path escapes')

    def test_attribution_output_escape_preserves_previous_runtime(self):
        # A malicious audit may otherwise place this outside the candidate.
        self.audit.write_text(json.dumps({
            'third_party_components': {'qtbase/../../../../outside': {
                'attribution': [{'license_files': ['LICENSE']}]}},
            'embedded_attributions': []}))
        self.write(self.base / 'outside/LICENSE', 'outside')
        self.assert_preserved_failure('output path escapes')
        self.assertEqual((self.base / 'outside/LICENSE').read_text(), 'outside')

    def test_output_symlink_is_rejected(self):
        alias = self.base / 'alias'
        alias.symlink_to(self.out, target_is_directory=True)
        before = self.snapshot()
        result = self.run_stage(alias)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('output must not be a symlink', result.stderr)
        self.assertEqual(self.snapshot(), before)

    def test_protected_output_is_rejected(self):
        before = self.snapshot(self.root)
        for path in (self.root, self.src, self.build, self.stage / 'nested'):
            with self.subTest(path=path):
                result = self.run_stage(path)
                self.assertNotEqual(result.returncode, 0)
                self.assertIn('source/build tree', result.stderr)
        self.assertEqual(self.snapshot(self.root), before)

    def test_existing_lock_refuses_concurrent_staging(self):
        lock = self.out.parent / '.runtime.stage-lock'
        lock.mkdir()
        before = self.snapshot()
        result = self.run_stage()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('staging is locked', result.stderr)
        self.assertEqual(self.snapshot(), before)
        self.assertTrue(lock.is_dir())

    def test_failed_promotion_restores_previous_runtime(self):
        before = self.snapshot()
        rename = Path.rename

        def fail_promotion(path, target):
            if path.name == 'candidate':
                raise OSError('injected promotion failure')
            return rename(path, target)

        with mock.patch.dict(os.environ, self.env), mock.patch.object(
                sys, 'argv', [str(SCRIPT), str(self.out), str(self.audit)]), \
                mock.patch.object(Path, 'rename', fail_promotion):
            with self.assertRaisesRegex(OSError, 'injected promotion failure'):
                runpy.run_path(str(SCRIPT), run_name='__main__')
        self.assertEqual(self.snapshot(), before)
        self.assertEqual(list(self.out.parent.glob('.runtime.stage-*')), [])

    def test_terminated_publication_retains_recoverable_previous_runtime(self):
        before = self.snapshot()
        # Terminate after the first rename in a separate process. This tests
        # the real interruption window without adding a production test hook.
        launcher = '''import os, runpy, signal, sys
from pathlib import Path
rename = Path.rename
def interrupted(path, target):
    result = rename(path, target)
    if Path(target).name == 'previous':
        os.kill(os.getpid(), signal.SIGTERM)
    return result
Path.rename = interrupted
sys.argv = sys.argv[1:]
runpy.run_path(sys.argv[0], run_name='__main__')
'''
        result = subprocess.run([sys.executable, '-I', '-c', launcher, str(SCRIPT),
                                 str(self.out), str(self.audit)], env=self.env,
                                capture_output=True, text=True, timeout=15)
        self.assertLess(result.returncode, 0)
        self.assertEqual(result.stdout, '')
        lock = self.out.parent / '.runtime.stage-lock'
        journal = json.loads((lock / 'transaction.json').read_text())
        self.assertEqual(journal['schema'], 'rnqnx-qt-staging/v1')
        self.assertEqual(Path(journal['destination']), self.out.resolve())
        previous = Path(journal['previous'])
        self.assertFalse(self.out.exists())
        self.assertEqual(self.snapshot(previous), before)
        # Repeat the documented recovery after the child has exited. Verify
        # the recorded sibling paths before moving any data.
        work = Path(journal['work'])
        self.assertEqual(work.parent, self.out.parent.resolve())
        self.assertEqual(previous, work / 'previous')
        self.assertFalse(work.is_symlink())
        previous.rename(self.out)
        self.assertEqual(self.snapshot(), before)
        self.assertTrue(Path(journal['candidate']).is_dir())
        # A stale lock must refuse the next run until deliberately cleared.
        result = self.run_stage()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('staging is locked', result.stderr)

    def test_success_replaces_runtime_and_hashes_actual_files(self):
        result = self.run_stage()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertFalse((self.out / 'known-good').exists())
        entries = json.loads((self.out / 'manifest.json').read_text())
        for rel, meta in entries.items():
            file = self.out / rel
            if 'sha256' in meta:
                self.assertEqual(meta['sha256'], hashlib.sha256(file.read_bytes()).hexdigest())
            else:
                self.assertEqual(os.readlink(file), meta['symlink'])
        self.assertTrue((self.out / 'licenses/THIRD_PARTY_ATTRIBUTIONS.txt').is_file())
        self.assertGreater(json.loads((self.out / 'needed.json').read_text())['elf'], 0)
        self.assertEqual(list(self.out.parent.glob('.runtime.stage-*')), [])
        before = self.snapshot()
        result = self.run_stage()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(self.snapshot(), before)


if __name__ == '__main__':
    unittest.main(verbosity=2)
