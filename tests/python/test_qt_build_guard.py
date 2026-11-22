"""Host process-ownership fixtures; no Qt or QNX compilation."""
import os
from pathlib import Path
import runpy
import subprocess
import sys
import tempfile
import time
from types import SimpleNamespace
import unittest
from unittest.mock import patch

SCRIPT = Path(__file__).resolve().parents[2] / 'scripts/qt-public/guarded-run.py'


class BuildGuardTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.script = self.root / 'build.sh'
        self.log = self.root / 'build.log'
        self.main = runpy.run_path(str(SCRIPT))['main']

    def run_guard(self, body, available):
        self.script.write_text('#!/bin/sh\n' + body)
        self.script.chmod(0o755)
        with patch.dict(os.environ, {'MIN_KB': '100', 'GUARD_INTERVAL_S': '0.01',
                                     'GUARD_PATH': str(self.root)}), \
             patch.object(sys, 'argv', [str(SCRIPT), str(self.script), str(self.log)]), \
             patch('os.statvfs', side_effect=available):
            return self.main()

    @staticmethod
    def free(kib):
        return SimpleNamespace(f_bavail=kib, f_frsize=1024)

    def test_refuses_before_start_and_preserves_exit(self):
        marker = self.root / 'started'
        self.assertEqual(self.run_guard(f'touch "{marker}"\n',
                                        lambda _: self.free(0)), 99)
        self.assertFalse(marker.exists())
        self.assertIn('build not started', self.log.read_text())
        self.assertEqual(self.run_guard('exit 7\n', lambda _: self.free(1000)), 7)

    def test_low_space_stops_only_owned_group(self):
        unrelated = subprocess.Popen(['sleep', '30'])
        def cleanup():
            if unrelated.poll() is None:
                unrelated.terminate()
            unrelated.wait(timeout=5)
        self.addCleanup(cleanup)
        marker = self.root / 'child'
        calls = 0

        def free(_):
            nonlocal calls
            calls += 1
            if calls == 1:
                return self.free(1000)
            deadline = time.monotonic() + 5
            while not marker.exists() and time.monotonic() < deadline:
                time.sleep(0.01)
            self.assertTrue(marker.exists(), 'fixture did not start')
            return self.free(0)

        result = self.run_guard(f'sleep 30 &\necho $! > "{marker}"\nwait\n', free)
        self.assertEqual(result, 99)
        self.assertIsNone(unrelated.poll(), 'guard signalled an unrelated process')
        unrelated.terminate()
        unrelated.wait(timeout=5)
        self.assertIn('EXIT=guard-stopped', self.log.read_text())


if __name__ == '__main__':
    unittest.main()
