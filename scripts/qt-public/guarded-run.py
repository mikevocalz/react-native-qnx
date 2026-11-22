#!/usr/bin/env python3
"""Run a build in its own session and stop only that group on low disk space."""
import datetime
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
    path = os.environ.get('GUARD_PATH', '/work')
    if minimum < 0 or interval <= 0:
        raise SystemExit('MIN_KB must be nonnegative and GUARD_INTERVAL_S positive')

    def available():
        info = os.statvfs(path)
        return info.f_bavail * info.f_frsize // 1024

    with open(sys.argv[2], 'a', buffering=1) as log:
        # Refuse before starting work as well as during the build.
        free = available()
        if free < minimum:
            log.write(f'GUARD: host free {free}KiB < {minimum}KiB; build not started\n')
            return 99
        child = subprocess.Popen([sys.argv[1]], stdout=log, stderr=log,
                                 start_new_session=True)

        def stop():
            # start_new_session completes before Popen returns. The group ID
            # belongs to this child and its descendants, not another ninja.
            try:
                os.killpg(child.pid, signal.SIGTERM)
            except ProcessLookupError:
                pass
            try:
                child.wait(timeout=10)
            except subprocess.TimeoutExpired:
                os.killpg(child.pid, signal.SIGKILL)
                child.wait()

        def interrupted(signum, _frame):
            stop()
            raise SystemExit(128 + signum)

        signal.signal(signal.SIGTERM, interrupted)
        signal.signal(signal.SIGINT, interrupted)
        try:
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
            if child.poll() is None:
                stop()
            raise


if __name__ == '__main__':
    sys.exit(main())
