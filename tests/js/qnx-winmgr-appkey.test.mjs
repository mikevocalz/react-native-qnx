// rnqnx-winmgr groups windows of one app (build variants included) by the key
// in tools/qnx-winmgr/appkey.h; this compiles and runs its host test.
import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { mkdtemp, rm } from 'node:fs/promises';
import { tmpdir } from 'node:os';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const source = fileURLToPath(new URL('../../tools/qnx-winmgr/appkey-test.c', import.meta.url));
const compiler = ['cc', 'clang', 'gcc'].find(
  (name) => spawnSync(name, ['--version'], { stdio: 'ignore' }).status === 0
);
assert.ok(compiler, 'a C compiler is needed for the rnqnx-winmgr app key test');
const dir = await mkdtemp(path.join(tmpdir(), 'rnqnx-appkey-'));
try {
  const exe = path.join(dir, 'appkey-test');
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
  assert.match(run.stdout, /appkey: ok/);
} finally {
  await rm(dir, { recursive: true, force: true });
}
console.log('qnx-winmgr-appkey: ok');
