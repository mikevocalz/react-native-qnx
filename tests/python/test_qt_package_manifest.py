"""Synthetic host fixtures executing the real closure verifier; no Qt build."""
import datetime
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

SCRIPT = Path(__file__).resolve().parents[2] / 'scripts/qt-public/generate-package-manifest.py'
spec = importlib.util.spec_from_file_location('package_manifest', SCRIPT)
tool = importlib.util.module_from_spec(spec)
spec.loader.exec_module(tool)


class PackageManifestTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='qt-package-fixture-')
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.runtime = self.root / 'runtime'
        self.bundle = self.root / 'source'
        self.manifest = self.bundle / 'SOURCE-MANIFEST.json'
        self.stamp = self.root / 'stamp'
        self.stages = self.root / 'stages.tsv'
        self.build_plugin = self.root / 'built-libqqnx.so'
        self.output = self.root / 'package.json'
        self.plugin = self.runtime / 'plugins/platforms/libqqnx.so'
        # Minimal synthetic ELF64 aarch64 header, no program headers/dependencies.
        elf = bytearray(64)
        elf[:6] = b'\x7fELF\x02\x01'
        struct.pack_into('<H', elf, 18, 183)
        self.write(self.plugin, elf)
        (self.plugin.parent / 'libqqnx-alias.so').symlink_to('libqqnx.so')
        self.build_plugin.write_bytes(elf)
        for name in ('licenses/THIRD_PARTY_ATTRIBUTIONS.txt',
                     'licenses/qtbase/LGPL-3.0-only.txt',
                     'licenses/qtdeclarative/LGPL-3.0-only.txt'):
            self.write(self.runtime / name, b'license fixture')
        self.source = {'schema': 1, 'qt': {'version': '6.12.0', 'sha256': tool.ARCHIVE_SHA},
                       'rnQnxPatches': {}, 'recipe': {'commit': 'a' * 40, 'files': {}},
                       'appliedQtbaseDiffs': {'files': {}}}
        for name in tool.PATCHES + [tool.SOFTWARE]:
            relative = f'qt-public/patches/{name}'
            path = self.bundle / relative
            self.write(path, name.encode())
            self.source['recipe']['files'][relative] = tool.digest(path)
        path = self.bundle / 'cve-patches/fixture.diff'
        self.write(path, b'synthetic CVE fixture, not an upstream patch')
        self.source['appliedQtbaseDiffs']['files']['cve-patches/fixture.diff'] = tool.digest(path)
        self.select(False)
        start = datetime.datetime(2026, 11, 22)
        rows = ['\t'.join(tool.HEADER)]
        for index, name in enumerate(tool.STAGES):
            a = start + datetime.timedelta(seconds=index)
            b = a + datetime.timedelta(seconds=1)
            rows.append('\t'.join([name, a.strftime('%Y-%m-%dT%H:%M:%SZ'), b.strftime('%Y-%m-%dT%H:%M:%SZ'), '1', '0', '100', '200']))
        self.stages.write_text('\n'.join(rows) + '\n')
        self.refresh_runtime()

    @staticmethod
    def write(path, value):
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(value)

    def select(self, software):
        applied = tool.PATCHES + ([tool.SOFTWARE] if software else [])
        self.source['rnQnxPatches'] = {'softwareGrab': software, 'appliedByScript': applied,
                                      'notAppliedByScript': [] if software else [tool.SOFTWARE]}
        self.manifest.write_text(json.dumps(self.source))
        self.stamp.write_text('\n'.join(applied) + '\n')

    def refresh_runtime(self):
        entries = {}
        for path in self.runtime.rglob('*'):
            name = path.relative_to(self.runtime).as_posix()
            if name in ('manifest.json', 'needed.json', 'SHA256SUMS'):
                continue
            if path.is_symlink():
                entries[name] = {'symlink': os.readlink(path)}
            elif path.is_file():
                entries[name] = {'sha256': tool.digest(path), 'bytes': path.stat().st_size}
        (self.runtime / 'manifest.json').write_text(json.dumps(entries))
        (self.runtime / 'needed.json').write_text(json.dumps({'needed': {'plugins/platforms/libqqnx.so': []}}))
        sums = [f'{tool.digest(path)}  {path.relative_to(self.runtime).as_posix()}'
                for path in sorted(self.runtime.rglob('*')) if path.is_file() and not path.is_symlink() and path.name != 'SHA256SUMS']
        (self.runtime / 'SHA256SUMS').write_text('\n'.join(sums) + '\n')

    def args(self):
        return ['--runtime', str(self.runtime), '--source-manifest', str(self.manifest),
                '--patch-stamp', str(self.stamp), '--stages', str(self.stages),
                '--build-plugin', str(self.build_plugin), '--output', str(self.output)]

    def run_tool(self):
        return subprocess.run([sys.executable, '-I', str(SCRIPT), *self.args()],
                              capture_output=True, text=True, timeout=15)

    def refuses(self):
        result = self.run_tool()
        self.assertNotEqual(result.returncode, 0, result.stdout)
        self.assertFalse(self.output.exists())
        return result.stderr

    def test_both_variants_emit_only_provisional_verified_inventory(self):
        for software in (False, True):
            with self.subTest(software=software):
                self.select(software)
                result = self.run_tool()
                self.assertEqual(result.returncode, 0, result.stderr)
                output = json.loads(self.output.read_text())
                self.assertIs(output['releaseEligible'], False)
                self.assertEqual(output['status'], 'provisional')
                self.assertEqual(output['softwareGrab'], software)
                self.assertEqual(output['patchesApplied'], self.source['rnQnxPatches']['appliedByScript'])
                self.assertEqual(output['pluginSha256'], tool.digest(self.plugin))
                self.assertEqual(output['counts']['hashedFiles'], len(output['files']))
                self.assertEqual(output['symlinks']['plugins/platforms/libqqnx-alias.so'], 'libqqnx.so')
                self.assertEqual(set(output['closureCheck']['checks']), tool.CHECKS)
                self.assertFalse(output['buildStages']['cleanStartAttested'])
                self.output.unlink()

    def test_bad_stage_tables_are_refused(self):
        baseline = self.stages.read_text().splitlines()
        variants = [baseline[:-1], baseline + [baseline[-1]],
                    baseline[:1] + list(reversed(baseline[1:])),
                    [baseline[0]] + [row.replace('\t0\t100\t', '\t1\t100\t') for row in baseline[1:]],
                    ['bad header'] + baseline[1:], baseline[:2] + [baseline[1]] + baseline[3:]]
        for rows in variants:
            with self.subTest(rows=rows):
                self.stages.write_text('\n'.join(rows) + '\n')
                self.refuses()

    def test_manifest_variant_stamp_and_source_hash_disagreement_refused(self):
        self.stamp.write_text('\n'.join(tool.PATCHES + [tool.SOFTWARE]) + '\n')
        self.refuses()
        self.select(False)
        self.source['rnQnxPatches']['softwareGrab'] = 0
        self.manifest.write_text(json.dumps(self.source))
        self.refuses()
        self.select(False)
        path = self.bundle / 'qt-public/patches' / tool.PATCHES[0]
        path.write_text('changed patch')
        self.refuses()

    def test_changed_plugin_refused_even_with_refreshed_runtime_hashes(self):
        self.plugin.write_bytes(self.plugin.read_bytes() + b'changed')
        self.refresh_runtime()
        self.assertIn('differs from build plugin', self.refuses())

    def test_fresh_closure_cannot_be_replaced_by_saved_success(self):
        self.write(self.runtime / 'libc.so.6', b'SDP fixture')
        self.refresh_runtime()
        self.assertIn('fresh closure verification failed', self.refuses())

    def test_unsafe_and_symlink_inputs_refused(self):
        original = self.stamp.read_bytes()
        self.stamp.unlink()
        target = self.root / 'other-stamp'
        target.write_bytes(original)
        self.stamp.symlink_to(target)
        self.refuses()
        self.stamp.unlink()
        self.stamp.write_bytes(original)
        self.source['recipe']['files']['../escape'] = 'b' * 64
        self.manifest.write_text(json.dumps(self.source))
        self.refuses()
        del self.source['recipe']['files']['../escape']
        self.manifest.write_text(json.dumps(self.source))
        (self.runtime / 'escape').symlink_to(self.build_plugin)
        self.refuses()

    def test_mutation_during_closure_refused(self):
        real_run = subprocess.run
        def mutate(*args, **kwargs):
            result = real_run(*args, **kwargs)
            self.build_plugin.write_bytes(b'changed after verification')
            return result
        with patch.object(tool.subprocess, 'run', side_effect=mutate):
            with self.assertRaisesRegex(ValueError, 'evidence changed'):
                tool.main(self.args())
        self.assertFalse(self.output.exists())

    def test_duplicate_sums_and_json_or_unsafe_runtime_metadata_refused(self):
        sums = self.runtime / 'SHA256SUMS'
        sums.write_text(sums.read_text() + sums.read_text().splitlines()[0] + '\n')
        self.assertIn('duplicate SHA256SUMS', self.refuses())
        self.refresh_runtime()
        self.manifest.write_text(self.manifest.read_text().replace('"schema": 1', '"schema": 1, "schema": 1'))
        self.assertIn('duplicate JSON', self.refuses())
        self.select(False)
        staged = self.runtime / 'manifest.json'
        value = json.loads(staged.read_text())
        value['../escape'] = {'sha256': 'b' * 64, 'bytes': 1}
        staged.write_text(json.dumps(value))
        self.assertIn('unsafe relative path', self.refuses())

    def test_missing_manifest_inventory_and_wrong_architecture_refused(self):
        staged = self.runtime / 'manifest.json'
        value = json.loads(staged.read_text())
        value.pop('plugins/platforms/libqqnx.so')
        staged.write_text(json.dumps(value))
        self.assertIn('does not cover', self.refuses())
        data = bytearray(self.plugin.read_bytes())
        struct.pack_into('<H', data, 18, 62)
        self.plugin.write_bytes(data)
        self.build_plugin.write_bytes(data)
        self.refresh_runtime()
        self.assertIn('must be ELF64', self.refuses())

    def test_existing_output_and_source_directory_alias_refused(self):
        self.output.write_text('preserve prior receipt')
        result = self.run_tool()
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(self.output.read_text(), 'preserve prior receipt')
        self.output.unlink()
        alias = self.root / 'source-alias'
        alias.symlink_to(self.bundle, target_is_directory=True)
        self.manifest = alias / 'SOURCE-MANIFEST.json'
        self.assertIn('symlink input/output path', self.refuses())

    def test_runpath_with_dot_segments_is_not_accepted_by_prefix_only(self):
        strings = b'\0/data/qt612-public/../escape\0'
        data = bytearray(224 + len(strings))
        data[:6] = b'\x7fELF\x02\x01'
        struct.pack_into('<H', data, 18, 183)
        struct.pack_into('<Q', data, 32, 64)
        struct.pack_into('<HH', data, 54, 56, 2)
        struct.pack_into('<IIQQQQQQ', data, 64, 1, 4, 0, 0, 0, len(data), len(data), 8)
        struct.pack_into('<IIQQQQQQ', data, 120, 2, 4, 176, 176, 176, 48, 48, 8)
        struct.pack_into('<qQqQqQ', data, 176, 5, 224, 29, 1, 0, 0)
        data[224:] = strings
        self.plugin.write_bytes(data)
        self.build_plugin.write_bytes(data)
        self.refresh_runtime()
        self.assertIn('fresh closure verification failed', self.refuses())

    def test_non_elf_or_wrong_architecture_library_refused(self):
        library = self.runtime / 'lib/libQt6Core.so.6.12.0'
        for data in (b'not ELF', bytes(self.plugin.read_bytes()[:18] + b'\x3e\0' + self.plugin.read_bytes()[20:])):
            with self.subTest(data=data):
                self.write(library, data)
                self.refresh_runtime()
                self.refuses()


if __name__ == '__main__':
    unittest.main()
