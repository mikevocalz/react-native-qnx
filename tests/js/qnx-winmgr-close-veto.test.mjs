// Host-execute the manager deadline cancellation policy. Screen property-event
// delivery and permissions remain target acceptance, not simulated by this test.
import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { mkdtemp, rm } from 'node:fs/promises';
import { tmpdir } from 'node:os';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const source = fileURLToPath(new URL('../../tools/qnx-winmgr/close-veto-test.c', import.meta.url));
const compiler = ['cc', 'clang', 'gcc'].find(
  (name) => spawnSync(name, ['--version'], { stdio: 'ignore' }).status === 0
);
assert.ok(compiler, 'a C compiler is needed for the rnqnx-winmgr close-veto policy test');
const dir = await mkdtemp(path.join(tmpdir(), 'rnqnx-close-veto-'));
try {
  const exe = path.join(dir, 'close-veto-test');
  const build = spawnSync(
    compiler,
    ['-std=c11', '-Wall', '-Wextra', '-Werror', '-o', exe, source],
    {
      encoding: 'utf8',
    }
  );
  assert.equal(build.status, 0, build.stderr);
  const run = spawnSync(exe, { encoding: 'utf8' });
  assert.equal(run.status, 0, run.stderr);
  assert.match(run.stdout, /close-veto: ok/);
} finally {
  await rm(dir, { recursive: true, force: true });
}
console.log('qnx-winmgr-close-veto: ok');
