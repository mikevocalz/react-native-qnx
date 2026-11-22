"""Host process-ownership fixtures; no Qt or QNX compilation."""
import os
import shlex
from pathlib import Path
import runpy
import signal
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
                                     'GUARD_GRACE_S': '0.1',
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

        heartbeat = self.root / 'heartbeat'
        body = (f"sh -c 'trap \"\" TERM; echo $$ > \"{marker}\"; "
                f"while :; do echo tick >> \"{heartbeat}\"; sleep 0.01; done' &\nwait\n")
        result = self.run_guard(body, free)
        self.assertEqual(result, 99)
        self.assertIsNone(unrelated.poll(), 'guard signalled an unrelated process')
        size = heartbeat.stat().st_size
        time.sleep(0.2)
        self.assertEqual(heartbeat.stat().st_size, size,
                         'TERM-ignoring owned descendant continued work')
        unrelated.terminate()
        unrelated.wait(timeout=5)
        self.assertIn('EXIT=guard-stopped', self.log.read_text())

    def test_interrupt_during_launch_cleans_up_started_child(self):
        real_popen = subprocess.Popen
        children = []
        handler = signal.getsignal(signal.SIGTERM)
        int_handler = signal.getsignal(signal.SIGINT)
        real_killpg = os.killpg
        signalled = []
        descriptors = []
        real_descriptor_signal = getattr(signal, 'pidfd_send_signal', None)

        def repeated_descriptor(descriptor, signum):
            self.assertIsNone(children[0].returncode, 'leader reaped before session cleanup')
            descriptors.append(descriptor)
            signal.raise_signal(signal.SIGINT)
            return real_descriptor_signal(descriptor, signum)

        def repeated_interrupt(pgid, signum):
            self.assertIsNone(children[0].returncode, 'leader reaped before group cleanup')
            signalled.append((pgid, signum))
            signal.raise_signal(signal.SIGINT)
            return real_killpg(pgid, signum)

        def interrupted_launch(*args, **kwargs):
            child = real_popen(*args, **kwargs)
            children.append(child)
            if len(children) == 1:
                signal.raise_signal(signal.SIGTERM)
            return child

        with patch('subprocess.Popen', side_effect=interrupted_launch), \
             patch('os.killpg', side_effect=repeated_interrupt), \
             patch.object(signal, 'pidfd_send_signal', side_effect=repeated_descriptor, create=True):
            with self.assertRaises(SystemExit) as raised:
                self.run_guard('sleep 30\n', lambda _: self.free(1000))
        self.assertEqual(raised.exception.code, 128 + signal.SIGTERM)
        self.assertIsNotNone(children[0].returncode)
        self.assertIs(signal.getsignal(signal.SIGTERM), handler)
        self.assertIs(signal.getsignal(signal.SIGINT), int_handler)
        if sys.platform.startswith('linux'):
            self.assertTrue(descriptors)
            self.assertEqual(signalled, [], 'Linux must use identity-safe session descriptors')
        else:
            self.assertEqual([pgid for pgid, _ in signalled], [children[0].pid] * 2)

    def test_permission_error_does_not_hide_live_group(self):
        fake = SimpleNamespace(pid=987654, returncode=None)
        def launch(*args, **kwargs):
            signal.raise_signal(signal.SIGTERM)
            return fake
        inspection = SimpleNamespace(stdout='987654 987654 S\n')
        with patch('sys.platform', 'darwin'), \
             patch('subprocess.Popen', side_effect=launch), \
             patch('os.killpg', side_effect=PermissionError('fixture denial')) as kill, \
             patch('subprocess.run', return_value=inspection):
            with self.assertRaisesRegex(PermissionError, 'fixture denial'):
                self.run_guard('exit 0\n', lambda _: self.free(1000))
        self.assertEqual(kill.call_args.args[0], fake.pid)

    @unittest.skipUnless(sys.platform.startswith('linux'), 'Linux pidfd/session contract')
    def test_linux_stops_term_ignoring_command_subgroup(self):
        unrelated = subprocess.Popen(['sleep', '30'], start_new_session=True)
        def clean_unrelated():
            if unrelated.poll() is None:
                unrelated.terminate()
            unrelated.wait(timeout=5)
        self.addCleanup(clean_unrelated)
        marker = self.root / 'subgroup'
        heartbeat = self.root / 'subgroup-heartbeat'
        def clean_subgroup():
            if not marker.exists():
                return
            pid, sid, _ = map(int, marker.read_text().split())
            try:
                descriptor = os.pidfd_open(pid)
            except ProcessLookupError:
                return
            try:
                try:
                    fields = Path(f'/proc/{pid}/stat').read_text().rsplit(')', 1)[1].split()
                    if int(fields[3]) == sid:
                        signal.pidfd_send_signal(descriptor, signal.SIGKILL)
                except (FileNotFoundError, ProcessLookupError):
                    pass
            finally:
                os.close(descriptor)
        self.addCleanup(clean_subgroup)
        code = f"""import os, signal, time
os.setpgid(0, 0)
signal.signal(signal.SIGTERM, signal.SIG_IGN)
with open({str(marker)!r}, 'w') as handle:
    handle.write(f'{{os.getpid()}} {{os.getsid(0)}} {{os.getpgrp()}}')
while True:
    with open({str(heartbeat)!r}, 'a') as handle:
        handle.write('tick\\n')
    time.sleep(0.01)
"""
        calls = 0
        def free(_):
            nonlocal calls
            calls += 1
            if calls == 1:
                return self.free(1000)
            deadline = time.monotonic() + 5
            while not (marker.exists() and heartbeat.exists()) and time.monotonic() < deadline:
                time.sleep(0.01)
            self.assertTrue(marker.exists() and heartbeat.exists(), 'command subgroup did not start')
            return self.free(0)
        body = f'{shlex.quote(sys.executable)} -c {shlex.quote(code)} &\nwait\n'
        self.assertEqual(self.run_guard(body, free), 99)
        pid, sid, pgid = map(int, marker.read_text().split())
        self.assertNotEqual(pgid, sid, 'fixture must create a Ninja-style command subgroup')
        self.assertEqual(pid, pgid)
        self.assertIsNone(unrelated.poll(), 'another session was signalled')
        size = heartbeat.stat().st_size
        time.sleep(0.2)
        self.assertEqual(heartbeat.stat().st_size, size, 'owned command subgroup survived cleanup')
        try:
            state = Path(f'/proc/{pid}/stat').read_text().rsplit(')', 1)[1].split()[0]
        except FileNotFoundError:
            state = None
        self.assertIn(state, (None, 'Z'), 'owned command is still live')


if __name__ == '__main__':
    unittest.main()
