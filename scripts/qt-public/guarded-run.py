#!/usr/bin/env python3
"""Run a build in its own session and stop only that group on low disk space."""
import datetime
import math
import os
import signal
import subprocess
import sys
import time


def main():
    if len(sys.argv) != 3:
        raise SystemExit('usage: guarded-run.sh <script> <log>')
    minimum = int(os.environ.get('MIN_KB', '1572864'))
    interval = float(os.environ.get('GUARD_INTERVAL_S', '15'))
    grace = float(os.environ.get('GUARD_GRACE_S', '10'))
    path = os.environ.get('GUARD_PATH', '/work')
    if minimum < 0 or not math.isfinite(interval) or interval <= 0 or not math.isfinite(grace) or grace <= 0:
        raise SystemExit('MIN_KB must be nonnegative and guard intervals finite and positive')

    def available():
        info = os.statvfs(path)
        return info.f_bavail * info.f_frsize // 1024

    with open(sys.argv[2], 'a', buffering=1) as log:
        # Refuse before starting work as well as during the build.
        free = available()
        if free < minimum:
            log.write(f'GUARD: host free {free}KiB < {minimum}KiB; build not started\n')
            return 99
        child = None
        stopping = False
        pending = []

        def signal_owned_group(signum):
            try:
                os.killpg(child.pid, signum)
            except ProcessLookupError:
                return
            except PermissionError:
                # Darwin can report EPERM for a group containing only zombies.
                # Do not treat EPERM as success while any live member remains.
                # The unreaped leader reserves this PID/PGID during inspection.
                members = subprocess.run(['ps', '-axo', 'pid=,pgid=,stat='],
                                         capture_output=True, text=True, check=True)
                for line in members.stdout.splitlines():
                    fields = line.split()
                    if len(fields) >= 3 and fields[1] == str(child.pid):
                        if not fields[2].startswith('Z'):
                            raise

        def stop():
            nonlocal stopping
            stopping = True
            # start_new_session completes before Popen returns. The group ID
            # belongs to this child and its descendants, not another ninja.
            signal_owned_group(signal.SIGTERM)
            # Do not reap the leader before the group cleanup: its unreaped
            # PID reserves the group ID while TERM-ignoring descendants exit.
            time.sleep(grace)
            signal_owned_group(signal.SIGKILL)
            child.wait()

        def interrupted(signum, _frame):
            if stopping:
                return
            if child is None:
                if not pending:
                    pending.append(signum)
                return
            stop()
            raise SystemExit(128 + signum)

        original = {sig: signal.getsignal(sig) for sig in (signal.SIGTERM, signal.SIGINT)}
        signal.signal(signal.SIGTERM, interrupted)
        signal.signal(signal.SIGINT, interrupted)
        try:
            child = subprocess.Popen([sys.argv[1]], stdout=log, stderr=log,
                                     start_new_session=True)
            if pending:
                interrupted(pending[0], None)
            while child.poll() is None:
                free = available()
                if free < minimum:
                    log.write(f'GUARD: host free {free}KiB < {minimum}KiB; stopping owned build\n')
                    stop()
                    log.write('EXIT=guard-stopped\n')
                    return 99
                time.sleep(interval)
            result = child.returncode
            result = result if result >= 0 else 128 - result
            now = datetime.datetime.now(datetime.timezone.utc).strftime('%Y-%m-%dT%H:%M:%SZ')
            log.write(f'EXIT={result} {now}\n')
            return result
        except BaseException:
            if child is not None and child.returncode is None and not stopping:
                stop()
            raise
        finally:
            for sig, handler in original.items():
                signal.signal(sig, handler)


if __name__ == '__main__':
    sys.exit(main())
