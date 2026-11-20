import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { mkdtemp, rm, symlink } from 'node:fs/promises';
import { tmpdir } from 'node:os';
import path from 'node:path';

// A CLI module must run as the entry point when it is reached through a
// symlinked directory. macOS tmpdir() is /var/folders, a symlink to
// /private/var/folders; Node resolves import.meta.url to the real path while
// process.argv[1] keeps the symlink, so a path comparison silently skips main().
const entries = [
  'packages/qnx-devtools/src/marathon.mjs',
  'packages/qnx-devtools/src/cli.mjs',
  'packages/react-native-qnx-cli/src/cli.mjs',
  'packages/react-native-qnx-cli/src/doctor.mjs',
  'packages/expo-qnx/src/cli.mjs',
  'scripts/qnx-release-dossier.mjs',
  'scripts/expo-sdk58-support.mjs',
  'scripts/nitro-platform-codegen.mjs',
  'scripts/stage-gallery-assets.mjs',
];
const temporary = await mkdtemp(path.join(tmpdir(), 'rnqnx-entry-symlink-'));
try {
  const link = path.join(temporary, 'checkout');
  await symlink(process.cwd(), link);
  /** @param {string} base @param {string} entry */
  const run = (base, entry) =>
    spawnSync(process.execPath, [path.join(base, entry), '--help'], {
      encoding: 'utf8',
      timeout: 30000,
    });
  for (const entry of entries) {
    const direct = run(process.cwd(), entry);
    assert.equal(direct.error, undefined, entry);
    assert.ok(direct.stdout + direct.stderr, `${entry} prints something when run directly`);
    const linked = run(link, entry);
    assert.equal(linked.error, undefined, entry);
    assert.equal(linked.status, direct.status, `${entry} exit status through a symlink`);
    assert.equal(linked.stdout, direct.stdout, `${entry} stdout through a symlink`);
    assert.equal(linked.stderr, direct.stderr, `${entry} stderr through a symlink`);
  }
  console.log(
    `cli-entry-symlink: ${entries.length} entry points behave the same through a symlink.`
  );
} finally {
  await rm(temporary, { recursive: true, force: true });
}
