import assert from 'node:assert/strict';
import { spawn, spawnSync } from 'node:child_process';
import { existsSync, mkdtempSync, readFileSync, rmSync } from 'node:fs';
import os from 'node:os';
import path from 'node:path';

const root = mkdtempSync(path.join(os.tmpdir(), 'rnqnx-target-watchdog-'));
const script = path.join(root, 'supervisor');
const compile = spawnSync(
  'c++',
  [
    '-std=c++17',
    '-Wall',
    '-Wextra',
    '-Werror',
    path.resolve('native/platform/qnx/tools/runtime-baseline-supervisor.cpp'),
    '-o',
    script,
  ],
  { encoding: 'utf8' }
);
assert.equal(compile.status, 0, compile.stderr);
/** @param {string} name @param {string} seconds @param {string[]} command */
function run(name, seconds, command) {
  return spawnSync(script, [seconds, path.join(root, name), '--', ...command], {
    encoding: 'utf8',
    timeout: 8000,
  });
}
/** @param {'SIGTERM' | 'SIGHUP'} signal */
async function checkInterruptedSupervisor(signal) {
  const pidFile = path.join(root, `${signal}.pid`);
  const child = spawn(
    script,
    [
      '20',
      path.join(root, `${signal}.log`),
      '--',
      'python3',
      '-c',
      'import os, signal, sys, time\nsignal.signal(signal.SIGTERM, signal.SIG_IGN)\nwith open(sys.argv[1] + ".tmp", "w") as output: output.write(str(os.getpid()))\nos.replace(sys.argv[1] + ".tmp", sys.argv[1])\nwhile True: time.sleep(1)',
      pidFile,
    ],
    { stdio: ['ignore', 'pipe', 'pipe'] }
  );
  let output = '';
  let errors = '';
  child.stdout.on('data', (chunk) => {
    output += String(chunk);
  });
  child.stderr.on('data', (chunk) => {
    errors += String(chunk);
  });
  let directPid = 0;
  let completed = false;
  /** @type {NodeJS.Timeout | undefined} */
  let deadline;
  /** @type {Promise<number | null>} */
  const exited = new Promise((resolve, reject) => {
    deadline = setTimeout(() => reject(new Error(`${signal}: supervisor did not exit`)), 8000);
    child.once('error', reject);
    child.once('close', (code) => {
      clearTimeout(deadline);
      resolve(code);
    });
  });
  // Observe rejection while polling readiness, before awaiting the exit itself.
  exited.catch(() => {});
  try {
    const readyDeadline = Date.now() + 4000;
    while (!existsSync(pidFile) && Date.now() < readyDeadline) {
      assert.equal(
        child.exitCode,
        null,
        `${signal}: supervisor exited before readiness: ${errors}`
      );
      await new Promise((resolve) => setTimeout(resolve, 10));
    }
    assert.ok(existsSync(pidFile), `${signal}: child did not publish its PID: ${errors}`);
    directPid = Number(readFileSync(pidFile, 'utf8'));
    assert.ok(Number.isSafeInteger(directPid) && directPid > 1);
    assert.equal(child.kill(signal), true);
    assert.equal(await exited, 125, `${signal}: ${errors}`);
    assert.match(output, /RNQNX_BASELINE_CHILD_EXIT=125/);
    assert.throws(
      () => process.kill(directPid, 0),
      (/** @type {NodeJS.ErrnoException} */ error) => error.code === 'ESRCH',
      `${signal}: direct child survived supervisor interruption`
    );
    completed = true;
  } finally {
    clearTimeout(deadline);
    if (!completed) {
      // Failure cleanup is bounded too; never leave this test's child running.
      if (directPid > 1) {
        try {
          process.kill(directPid, 'SIGKILL');
        } catch (error) {
          if (/** @type {NodeJS.ErrnoException} */ (error).code !== 'ESRCH') throw error;
        }
      }
      child.kill('SIGKILL');
    }
  }
}
try {
  const success = run('success', '3', ['/bin/sh', '-c', 'echo rendered; exit 0']);
  assert.equal(success.status, 0, success.stderr);
  assert.match(success.stdout, /RNQNX_BASELINE_CHILD_EXIT=0/);
  assert.equal(readFileSync(path.join(root, 'success'), 'utf8'), 'rendered\n');
  const failure = run('failure', '3', ['/bin/sh', '-c', 'echo failed >&2; exit 7']);
  assert.equal(failure.status, 7);
  assert.match(failure.stdout, /RNQNX_BASELINE_CHILD_EXIT=7/);
  const crash = run('crash', '3', ['/bin/sh', '-c', 'kill -ABRT $$']);
  assert.notEqual(crash.status, 0);
  assert.notEqual(crash.status, 124);
  const hung = run('hung', '1', ['/bin/sh', '-c', 'trap "" TERM; while :; do :; done']);
  assert.equal(hung.status, 124, hung.stderr);
  assert.match(hung.stdout, /RNQNX_BASELINE_CHILD_EXIT=124/);
  const recovery = run('recovery', '3', ['/bin/sh', '-c', 'echo recovered']);
  assert.equal(recovery.status, 0);
  const stale = run('success', '3', ['/bin/sh', '-c', 'echo must-not-run']);
  assert.equal(stale.status, 2);
  assert.doesNotMatch(stale.stdout, /must-not-run/);
  const invalid = run('invalid', '0', ['/bin/true']);
  assert.equal(invalid.status, 2);
  await checkInterruptedSupervisor('SIGTERM');
  await checkInterruptedSupervisor('SIGHUP');
  console.log(
    'QNX target watchdog: success, failure, crash, timeout, recovery, stale-state and interrupted-child cleanup checks passed'
  );
} finally {
  rmSync(root, { recursive: true, force: true });
}
