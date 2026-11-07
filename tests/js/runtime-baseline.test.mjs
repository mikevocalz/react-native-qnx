import assert from 'node:assert/strict';
import { spawn, spawnSync } from 'node:child_process';
import { existsSync, mkdtempSync, readFileSync, writeFileSync, rmSync } from 'node:fs';
import { tmpdir } from 'node:os';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../..');
const temporary = mkdtempSync(path.join(tmpdir(), 'rnqnx-runtime-baseline-'));
try {
  const candidate = path.join(temporary, 'candidate.json');
  writeFileSync(candidate, JSON.stringify({ lane: 'host simulated child; no RN/device proof' }));
  const fixture = path.join(temporary, 'child.py');
  writeFileSync(
    fixture,
    `import os,signal,struct,sys,time,zlib
mode, destination = sys.argv[1:]
if mode == 'interrupt':
    signal.signal(signal.SIGTERM, signal.SIG_IGN)
    signal.signal(signal.SIGHUP, signal.SIG_IGN)
    open(destination+'.pid','w').write(str(os.getpid()))
    time.sleep(60)
if mode == 'hang':
    signal.signal(signal.SIGTERM, signal.SIG_IGN)
    time.sleep(60)
if mode == 'crash': os.kill(os.getpid(), signal.SIGABRT)
def chunk(kind, data):
    return struct.pack('>I', len(data))+kind+data+struct.pack('>I', zlib.crc32(kind+data))
image = b'\\x89PNG\\r\\n\\x1a\\n' + chunk(b'IHDR', struct.pack('>IIBBBBB',2,1,8,6,0,0,0)) + chunk(b'IDAT',zlib.compress(b'\\0'+b'\\xff'*8))+chunk(b'IEND',b'')
open(destination,'wb').write(image[:33] if mode == 'truncated' else image)
if mode != 'missing': print('RNQNX_HELLO_NITRO_V1_OK platform=qnx architecture=arm64 add=42')
print('RNQNX_PLAYGROUND_SCREENSHOT '+destination+' 2x1')
`
  );
  /** @param {string} mode @param {string} timeout @param {string[]} options */
  function run(mode, timeout = '2', options = []) {
    const output = path.join(temporary, mode);
    const result = spawnSync(
      'python3',
      [
        path.join(root, 'scripts/runtime-baseline.py'),
        '--candidate',
        candidate,
        '--output',
        output,
        '--trials',
        '2',
        '--timeout',
        timeout,
        ...options,
        '--width',
        '2',
        '--height',
        '1',
        '--',
        'python3',
        fixture,
        mode,
        '{screenshot}',
      ],
      { encoding: 'utf8', timeout: 10000 }
    );
    assert.equal(result.error, undefined);
    return { result, receipt: JSON.parse(readFileSync(path.join(output, 'receipt.json'), 'utf8')) };
  }
  const success = run('success');
  assert.equal(success.result.status, 0);
  assert.equal(success.receipt.completedTrials, 2);
  assert.equal(success.receipt.trials[0].frame.contentVerification, 'not performed');
  assert.equal(success.receipt.trials[0].frame.width, 2);
  const missing = run('missing');
  assert.equal(missing.result.status, 1);
  assert.equal(missing.receipt.completedTrials, 1);
  assert.ok(missing.receipt.trials[0].failures.includes('missing-nitro-marker'));
  const truncated = run('truncated');
  assert.equal(truncated.receipt.trials[0].frame.status, 'invalid-png');
  const crash = run('crash');
  assert.equal(crash.result.status, 1);
  assert.ok(crash.receipt.trials[0].exitCode < 0);
  const hang = run('hang', '0.1');
  assert.equal(hang.result.status, 1);
  assert.equal(hang.receipt.trials[0].timedOut, true);
  assert.ok(hang.receipt.trials[0].elapsedSeconds < 4);
  const health = run('health', '2', ['--health-command', '/usr/bin/false']);
  assert.equal(health.result.status, 1);
  assert.equal(health.receipt.completedTrials, 1);
  assert.ok(health.receipt.trials[0].failures.includes('health-failed'));
  /** @type {Array<[NodeJS.Signals, number]>} */
  const signals = [
    ['SIGTERM', 15],
    ['SIGHUP', 1],
  ];
  for (const [name, number] of signals) {
    const output = path.join(temporary, name);
    const supervisor = spawn(
      'python3',
      [
        path.join(root, 'scripts/runtime-baseline.py'),
        '--candidate',
        candidate,
        '--output',
        output,
        '--timeout',
        '20',
        '--trials',
        '2',
        '--',
        'python3',
        fixture,
        'interrupt',
        '{screenshot}',
      ],
      { stdio: 'ignore' }
    );
    const exited = new Promise((resolve, reject) => {
      const timer = setTimeout(() => reject(new Error('signal cleanup did not finish')), 5000);
      supervisor.once('exit', (code, signal) => {
        clearTimeout(timer);
        resolve({ code, signal });
      });
      supervisor.once('error', reject);
    });
    let childPid = 0;
    try {
      const pidFile = path.join(output, 'trial-001', 'frame.png.pid');
      const deadline = Date.now() + 3000;
      while (!existsSync(pidFile) && Date.now() < deadline) {
        await new Promise((resolve) => setTimeout(resolve, 10));
      }
      assert.ok(existsSync(pidFile), 'child must be running before interrupt');
      childPid = Number(readFileSync(pidFile, 'utf8'));
      assert.ok(childPid > 0);
      process.kill(childPid, 0);
      supervisor.kill(name);
      const outcome = await exited;
      assert.equal(outcome.signal, null);
      assert.equal(outcome.code, 128 + number);
      const interrupted = JSON.parse(readFileSync(path.join(output, 'receipt.json'), 'utf8'));
      assert.equal(interrupted.status, 'failed');
      assert.equal(interrupted.interruptedSignal, number);
      assert.equal(interrupted.completedTrials, 1);
      assert.ok(interrupted.trials[0].failures.includes('interrupted'));
      assert.equal(interrupted.trials[0].interruptedSignal, number);
      assert.throws(() => process.kill(childPid, 0), { code: 'ESRCH' });
    } finally {
      if (supervisor.exitCode === null) supervisor.kill('SIGKILL');
      if (childPid) {
        try {
          process.kill(childPid, 'SIGKILL');
        } catch {}
      }
    }
  }
  const reused = spawnSync('python3', [
    path.join(root, 'scripts/runtime-baseline.py'),
    '--candidate',
    candidate,
    '--output',
    path.join(temporary, 'success'),
    '--',
    'python3',
    fixture,
    'success',
    '{screenshot}',
  ]);
  assert.notEqual(reused.status, 0);
  console.log(
    'Runtime baseline supervisor: repeated capture, missing marker, truncated PNG, crash, watchdog, TERM/HUP cleanup and stale artifact checks passed.'
  );
} finally {
  rmSync(temporary, { recursive: true, force: true });
}
