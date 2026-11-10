import assert from 'node:assert/strict';
import { cp, lstat, mkdir, mkdtemp, readFile, rm, symlink, writeFile } from 'node:fs/promises';
import { tmpdir } from 'node:os';
import path from 'node:path';
import { spawnSync } from 'node:child_process';

const scratch = await mkdtemp(path.join(tmpdir(), 'qnx-deploy-'));
try {
  await mkdir(path.join(scratch, 'scripts/lib'), { recursive: true });
  await mkdir(path.join(scratch, 'build/qnx-aarch64-debug/bin'), { recursive: true });
  await mkdir(path.join(scratch, 'fake-bin'));
  await cp('scripts/deploy-qnx.sh', path.join(scratch, 'scripts/deploy-qnx.sh'));
  await cp('scripts/lib/qnx-target.sh', path.join(scratch, 'scripts/lib/qnx-target.sh'));
  // Simulated SSH/SCP execute exclusively in the temporary directory, no device/network.
  await writeFile(
    path.join(scratch, 'fake-bin/ssh'),
    '#!/usr/bin/env bash\nexec sh -c "${@: -1}"\n',
    { mode: 0o755 }
  );
  await writeFile(
    path.join(scratch, 'fake-bin/scp'),
    `#!/usr/bin/env bash
set -eu
source_path="\${@: -2:1}"
destination="\${@: -1}"
if [[ "$source_path" == *@*:* ]]; then
  source_path="\${source_path#*:}"
  if [[ "\${FAIL_DOWNLOAD:-}" == 1 ]]; then printf partial > "$destination"; exit 73; fi
fi
if [[ "$destination" == *@*:* ]]; then
  destination="\${destination#*:}"
  if [[ "\${FAIL_UPLOAD:-}" == 1 ]]; then printf partial > "$destination"; exit 74; fi
  if [[ "\${CORRUPT_UPLOAD:-}" == 1 ]]; then printf corrupt > "$destination"; exit 0; fi
fi
cp "$source_path" "$destination"
`,
    { mode: 0o755 }
  );
  // Only the deployment's remote backup/candidate operations fail. SCP's
  // underlying copy and cleanup remain usable to exercise the actual script.
  await writeFile(
    path.join(scratch, 'fake-bin/cp'),
    `#!/usr/bin/env bash
set -eu
destination="\${@: -1}"
if [[ "\${FAIL_BACKUP_COPY:-}" == 1 && "$destination" == */lock/previous ]]; then
  printf partial > "$destination"
  exit 75
fi
exec /bin/cp "$@"
`,
    { mode: 0o755 }
  );
  await writeFile(
    path.join(scratch, 'fake-bin/mv'),
    `#!/usr/bin/env bash
set -eu
source_path="\${@: -2:1}"
if [[ "\${FAIL_ACTIVATE:-}" == 1 && "$source_path" == */lock/candidate ]]; then exit 76; fi
exec /bin/mv "$@"
`,
    { mode: 0o755 }
  );
  const remote = path.join(scratch, 'target');
  const binary = path.join(scratch, 'build/qnx-aarch64-debug/bin/demo');
  /** @param {string[]} [args] @param {Record<string,string>} [extra] */
  const deploy = (args = [], extra = {}) =>
    spawnSync('bash', ['scripts/deploy-qnx.sh', 'aarch64', 'demo', 'Debug', ...args], {
      cwd: scratch,
      encoding: 'utf8',
      env: {
        ...process.env,
        PATH: `${scratch}/fake-bin:${process.env.PATH}`,
        QNX_TARGET_DIR: remote,
        QNX_TARGET_IP: 'fixture',
        QNX_TARGET_USER: 'fixture',
        ...extra,
      },
    });
  await mkdir(remote);
  await writeFile(path.join(remote, 'demo.previous'), 'independent dotted app');
  await writeFile(path.join(remote, 'demo.deploy-lock'), 'another dotted app');
  await writeFile(binary, 'first executable');
  assert.equal(deploy().status, 0);
  assert.equal(await readFile(path.join(remote, 'demo'), 'utf8'), 'first executable');
  assert.notEqual(deploy(['--rollback']).status, 0, 'no previous deployment must fail');
  await writeFile(binary, 'second executable');
  assert.equal(deploy().status, 0);
  assert.equal(
    await readFile(path.join(remote, '.rnqnx-deploy/demo/previous'), 'utf8'),
    'first executable'
  );
  assert.equal(await readFile(path.join(remote, 'demo'), 'utf8'), 'second executable');
  await writeFile(binary, 'third executable');
  assert.notEqual(deploy([], { CORRUPT_UPLOAD: '1' }).status, 0);
  assert.equal(await readFile(path.join(remote, 'demo'), 'utf8'), 'second executable');
  assert.equal(
    await readFile(path.join(remote, '.rnqnx-deploy/demo/previous'), 'utf8'),
    'first executable'
  );
  const previous = path.join(remote, '.rnqnx-deploy/demo/previous');
  const lock = path.join(remote, '.rnqnx-deploy/demo/lock');
  for (const failure of ['FAIL_UPLOAD', 'FAIL_DOWNLOAD', 'FAIL_BACKUP_COPY', 'FAIL_ACTIVATE']) {
    await writeFile(previous, 'first executable');
    const result = deploy([], { [failure]: '1' });
    assert.notEqual(result.status, 0, `${failure} must fail deployment`);
    assert.equal(
      await readFile(path.join(remote, 'demo'), 'utf8'),
      'second executable',
      `${failure} must preserve the active executable`
    );
    assert.equal(
      await readFile(previous, 'utf8'),
      failure === 'FAIL_ACTIVATE' ? 'second executable' : 'first executable',
      `${failure} must leave a complete backup, never a partial copy`
    );
    await assert.rejects(lstat(lock), { code: 'ENOENT' }, `${failure} must clean its lock`);
  }
  // Activation failure occurs after backing up the still-active executable.
  // Restore the older fixture backup to independently exercise rollback below.
  await writeFile(previous, 'first executable');
  await rm(binary);
  assert.equal(deploy(['--rollback']).status, 0, 'rollback needs no local build');
  assert.equal(await readFile(path.join(remote, 'demo'), 'utf8'), 'first executable');
  await writeFile(binary, 'third executable');
  await mkdir(path.join(remote, '.rnqnx-deploy/demo/lock'));
  assert.notEqual(deploy().status, 0, 'concurrent operation refused');
  await rm(path.join(remote, '.rnqnx-deploy/demo/lock'), { recursive: true });
  await rm(path.join(remote, 'demo'));
  await symlink(binary, path.join(remote, 'demo'));
  assert.notEqual(deploy().status, 0, 'target symlink refused');
  assert.equal(await readFile(binary, 'utf8'), 'third executable');
  assert.equal(
    await readFile(path.join(remote, 'demo.previous'), 'utf8'),
    'independent dotted app'
  );
  assert.equal(await readFile(path.join(remote, 'demo.deploy-lock'), 'utf8'), 'another dotted app');
  assert.notEqual(deploy(['--unknown']).status, 0);
  console.log(
    'QNX deployment staging, integrity, rollback and failure tests passed (host simulation).'
  );
} finally {
  await rm(scratch, { recursive: true, force: true });
}
