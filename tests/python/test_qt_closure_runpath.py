"""Real ELF dynamic-section fixtures for closure RUNPATH policy; no Qt build."""
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest

SCRIPT = Path(__file__).resolve().parents[2] / 'scripts/qt-public/verify-closure.py'


class RunpathClosureTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='qt-runpath-fixture-')
        self.addCleanup(self.temp.cleanup)
        self.base = Path(self.temp.name)
        self.runtime = self.base / 'runtime'
        self.plugin = self.runtime / 'plugins/platforms/libqqnx.so'
        self.plugin.parent.mkdir(parents=True)
        for name in ('THIRD_PARTY_ATTRIBUTIONS.txt', 'qtbase/LGPL-3.0-only.txt',
                     'qtdeclarative/LGPL-3.0-only.txt'):
            path = self.runtime / 'licenses' / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text('synthetic license fixture')
        self.report = self.base / 'report.json'

    def build(self, entry, tag=29):
        strings = b'\0' + entry.encode() + b'\0'
        data = bytearray(224 + len(strings))
        data[:6] = b'\x7fELF\x02\x01'
        struct.pack_into('<H', data, 18, 183)
        struct.pack_into('<Q', data, 32, 64)
        struct.pack_into('<HH', data, 54, 56, 2)
        struct.pack_into('<IIQQQQQQ', data, 64, 1, 4, 0, 0, 0, len(data), len(data), 8)
        struct.pack_into('<IIQQQQQQ', data, 120, 2, 4, 176, 176, 176, 48, 48, 8)
        struct.pack_into('<qQqQqQ', data, 176, 5, 224, tag, 1, 0, 0)
        data[224:] = strings
        self.plugin.write_bytes(data)
        files = {path.relative_to(self.runtime).as_posix(): {'sha256': hashlib.sha256(path.read_bytes()).hexdigest()}
                 for path in self.runtime.rglob('*') if path.is_file() and path.name not in ('manifest.json', 'SHA256SUMS')}
        (self.runtime / 'manifest.json').write_text(json.dumps(files))
        (self.runtime / 'SHA256SUMS').write_text(''.join(
            f'{hashlib.sha256(path.read_bytes()).hexdigest()}  {path.relative_to(self.runtime).as_posix()}\n'
            for path in sorted(self.runtime.rglob('*')) if path.is_file() and path.name != 'SHA256SUMS'))

    def run_closure(self, entry, prefix='/data/qt612-public/', tag=29):
        self.build(entry, tag)
        self.report.unlink(missing_ok=True)
        args = [sys.executable, '-I', str(SCRIPT), str(self.runtime), '--json', str(self.report)]
        if prefix is not None:
            args += ['--runpath-prefix', prefix]
        result = subprocess.run(args, capture_output=True, text=True, timeout=10)
        report = json.loads(self.report.read_text()) if self.report.exists() else None
        return result, report

    def test_accepts_install_directory_and_descendants_with_optional_trailing_slash(self):
        for entry in ('/data/qt612-public', '/data/qt612-public/', '/data/qt612-public/lib', '/data/qt612-public/lib/'):
            with self.subTest(entry=entry):
                result, report = self.run_closure(entry)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                self.assertTrue(report['checks']['rpath']['pass'])

    def test_rejects_dot_segments_prefix_collisions_empty_and_other_roots(self):
        entries = ('/data/qt612-public/../escape', '/data/qt612-public/lib/../lib',
                   '/data/qt612-public/.', '/data/qt612-public-evil/lib',
                   '/data/qt612-public//lib', '/other/lib', '',
                   '/data/qt612-public/lib:', ':/data/qt612-public/lib')
        for entry in entries:
            with self.subTest(entry=entry):
                result, report = self.run_closure(entry)
                self.assertEqual(result.returncode, 1, result.stderr)
                self.assertFalse(report['checks']['rpath']['pass'])
                self.assertTrue(all(value['pass'] for name, value in report['checks'].items() if name != 'rpath'))
        result, report = self.run_closure('/data/lib-evil', '/data/lib')
        self.assertEqual(result.returncode, 1)
        self.assertFalse(report['checks']['rpath']['pass'])

    def test_prefix_must_be_explicit_and_normalized(self):
        result, report = self.run_closure('/data/qt612-public/lib', None)
        self.assertEqual(result.returncode, 1)
        self.assertFalse(report['checks']['rpath']['pass'])
        for prefix in ('data/qt612-public', '/data/../data/qt612-public',
                       '/data/./qt612-public', '/data//qt612-public', '//data/qt612-public', ''):
            with self.subTest(prefix=prefix):
                result, report = self.run_closure('/data/qt612-public/lib', prefix)
                self.assertEqual(result.returncode, 2, result.stdout + result.stderr)
                self.assertIsNone(report)

    def test_relative_runpath_semantics_unchanged_and_rpath_still_forbidden(self):
        for entry in ('lib', './lib', '../lib', '$ORIGIN/lib', '$ORIGIN/../lib'):
            with self.subTest(entry=entry):
                result, _ = self.run_closure(entry, None)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        result, report = self.run_closure('lib', None, tag=15)
        self.assertEqual(result.returncode, 1)
        self.assertFalse(report['checks']['rpath']['pass'])


if __name__ == '__main__':
    unittest.main()
