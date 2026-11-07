#!/usr/bin/env python3
"""Bounded host supervisor for the existing RN visual runner.

Command arguments are passed directly, never through a shell. A target adapter
must copy its PNG to {screenshot} and bound/clean up its own remote process;
terminating a local SSH client alone does not terminate a remote application.
A captured PNG and log markers do not prove its title, pixels or presentation.
"""

import argparse
import hashlib
import json
import os
from pathlib import Path
import signal
import struct
import subprocess
import sys
import time
import zlib


_interrupted_signal = None


def interrupt(signum, _frame):
    # Defer unwinding until the launched process has an owner. Raising from a
    # handler during Popen could interrupt assignment of that process handle.
    global _interrupted_signal
    if _interrupted_signal is None:
        _interrupted_signal = signum


def bounded(command, log, timeout):
    started = time.monotonic()
    with log.open('wb') as stream:
        try:
            child = subprocess.Popen(command, stdout=stream, stderr=subprocess.STDOUT,
                                     start_new_session=True)
        except OSError as error:
            stream.write(str(error).encode())
            return {'exitCode': None, 'timedOut': False, 'launchError': str(error),
                    'interruptedSignal': _interrupted_signal,
                    'elapsedSeconds': time.monotonic() - started}
        timed_out = False
        try:
            while child.poll() is None:
                if _interrupted_signal is not None:
                    break
                remaining = timeout - (time.monotonic() - started)
                if remaining <= 0:
                    timed_out = True
                    break
                try:
                    child.wait(timeout=min(remaining, 0.1))
                except subprocess.TimeoutExpired:
                    pass
            if timed_out or _interrupted_signal is not None:
                try:
                    os.killpg(child.pid, signal.SIGTERM)
                except ProcessLookupError:
                    pass
                try:
                    child.wait(timeout=1)
                except subprocess.TimeoutExpired:
                    pass
        finally:
            # Kill the owned local process group even if its leader already
            # exited. Remote process cleanup remains the adapter's obligation.
            try:
                os.killpg(child.pid, signal.SIGKILL)
            except ProcessLookupError:
                pass
            child.wait()
    return {'exitCode': child.returncode, 'timedOut': timed_out,
            'interruptedSignal': _interrupted_signal,
            'elapsedSeconds': time.monotonic() - started}


def png_receipt(path, width, height):
    if not path.is_file():
        return {'status': 'missing'}
    data = path.read_bytes()
    if len(data) < 33 or data[:8] != b'\x89PNG\r\n\x1a\n' or data[12:16] != b'IHDR':
        return {'status': 'invalid-header'}
    actual = struct.unpack('>II', data[16:24])
    # Decode the complete bounded 8-bit, non-interlaced Qt PNG stream. A stale
    # header or truncated file is not captured-frame evidence.
    try:
        depth, color, compression, filtering, interlace = data[24:29]
        channels = {0: 1, 2: 3, 4: 2, 6: 4}[color]
        if depth != 8 or compression or filtering or interlace or not all(0 < n <= 8192 for n in actual):
            raise ValueError('unsupported PNG encoding')
        offset, compressed, ended = 8, bytearray(), False
        while offset < len(data):
            size = struct.unpack('>I', data[offset:offset + 4])[0]
            kind = data[offset + 4:offset + 8]
            payload = data[offset + 8:offset + 8 + size]
            crc = struct.unpack('>I', data[offset + 8 + size:offset + 12 + size])[0]
            if zlib.crc32(kind + payload) != crc:
                raise ValueError('PNG checksum mismatch')
            if kind == b'IDAT': compressed.extend(payload)
            offset += 12 + size
            if kind == b'IEND':
                ended = size == 0 and offset == len(data)
                break
        expected = (actual[0] * channels + 1) * actual[1]
        decoder = zlib.decompressobj()
        pixels = decoder.decompress(compressed, expected + 1)
        if not ended or not decoder.eof or decoder.unused_data or len(pixels) != expected:
            raise ValueError('incomplete PNG stream')
        stride = actual[0] * channels + 1
        if any(pixels[row] > 4 for row in range(0, expected, stride)):
            raise ValueError('invalid PNG row filter')
    except (ValueError, KeyError, struct.error, zlib.error):
        return {'status': 'invalid-png'}
    return {'status': 'captured' if actual == (width, height) else 'wrong-dimensions',
            'width': actual[0], 'height': actual[1], 'bytes': len(data),
            'sha256': hashlib.sha256(data).hexdigest(),
            'contentVerification': 'not performed',
            'presentationVerification': 'not performed'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--candidate', type=Path, required=True,
                        help='JSON object of declared source/toolchain/artifact provenance')
    parser.add_argument('--trials', type=int, default=10)
    parser.add_argument('--timeout', type=float, default=35)
    parser.add_argument('--width', type=int, default=1920)
    parser.add_argument('--height', type=int, default=1080)
    parser.add_argument('--health-command', help='optional executable, no arguments; bounded after each trial')
    parser.add_argument('command', nargs=argparse.REMAINDER)
    args = parser.parse_args()
    command = args.command[1:] if args.command[:1] == ['--'] else args.command
    if not command or not any('{screenshot}' in arg for arg in command):
        parser.error('command must include a {screenshot} argument placeholder')
    if not 1 <= args.trials <= 100 or not 0 < args.timeout <= 300:
        parser.error('trials must be 1..100 and timeout must be >0..300 seconds')
    if args.width <= 0 or args.height <= 0:
        parser.error('dimensions must be positive')
    candidate = json.loads(args.candidate.read_text())
    if not isinstance(candidate, dict) or not candidate:
        parser.error('candidate must be a nonempty JSON object')
    # Refuse reuse: stale logs/frames must never satisfy a new trial.
    args.output.mkdir(parents=True, exist_ok=False)
    output = args.output.resolve()
    receipt = {'schemaVersion': 1, 'candidateDeclaration': candidate,
               'candidateVerification': 'operator supplied; not independently verified',
               'scope': 'launch, Nitro marker, captured PNG header/dimensions, process exit',
               'limits': ['No expected-content or display-presentation verification.',
                          'Local watchdog cannot prove remote process cleanup.',
                          'Elapsed time is supervisor wall duration, not startup latency.'],
               'trials': []}
    for signum in (signal.SIGTERM, signal.SIGHUP, signal.SIGINT):
        signal.signal(signum, interrupt)
    for index in range(1, args.trials + 1):
        if _interrupted_signal is not None:
            break
        trial_dir = output / f'trial-{index:03d}'
        trial_dir.mkdir()
        frame = trial_dir / 'frame.png'
        run_command = [arg.replace('{screenshot}', str(frame)) for arg in command]
        result = bounded(run_command, trial_dir / 'application.log', args.timeout)
        log = (trial_dir / 'application.log').read_text(errors='replace')
        result['nitroMarker'] = 'RNQNX_HELLO_NITRO_V1_OK' in log
        result['captureMarker'] = any(
            line.startswith('RNQNX_PLAYGROUND_SCREENSHOT ') and
            line.endswith(f' {args.width}x{args.height}') for line in log.splitlines())
        result['frame'] = png_receipt(frame, args.width, args.height)
        if args.health_command and _interrupted_signal is None:
            result['health'] = bounded([args.health_command], trial_dir / 'health.log', args.timeout)
        result['interruptedSignal'] = _interrupted_signal
        failures = []
        if _interrupted_signal is not None: failures.append('interrupted')
        if result.get('launchError'): failures.append('launch-error')
        if result['timedOut']: failures.append('timeout')
        if result['exitCode'] != 0: failures.append('nonzero-exit')
        if not result['nitroMarker']: failures.append('missing-nitro-marker')
        if not result['captureMarker']: failures.append('missing-capture-marker')
        if result['frame']['status'] != 'captured': failures.append('missing-or-invalid-frame')
        health = result.get('health')
        if health and (health['exitCode'] != 0 or health['timedOut']): failures.append('health-failed')
        result.update({'trial': index, 'status': 'failed' if failures else 'passed', 'failures': failures})
        receipt['trials'].append(result)
        receipt['status'] = 'failed' if any(t['failures'] for t in receipt['trials']) else 'passed'
        (output / 'receipt.json').write_text(json.dumps(receipt, indent=2) + '\n')
        print(f"trial {index}: {result['status']} {','.join(failures)}", flush=True)
        # A failed/unknown cleanup state must not launch another application.
        if failures:
            break
    if _interrupted_signal is not None:
        receipt['status'] = 'failed'
        receipt['interruptedSignal'] = _interrupted_signal
    receipt['completedTrials'] = len(receipt['trials'])
    receipt['requestedTrials'] = args.trials
    (output / 'receipt.json').write_text(json.dumps(receipt, indent=2) + '\n')
    if _interrupted_signal is not None:
        return 128 + _interrupted_signal
    return 0 if receipt['status'] == 'passed' else 1


if __name__ == '__main__':
    sys.exit(main())
