import assert from 'node:assert/strict';
import { mkdtemp, mkdir, readFile, writeFile } from 'node:fs/promises';
import os from 'node:os';
import path from 'node:path';
import { spawnSync } from 'node:child_process';

const root = process.cwd();
const fixture = await mkdtemp(path.join(os.tmpdir(), 'rnqnx-pi-evidence-'));
await mkdir(path.join(fixture, 'preflight'), { recursive: true });

const files = {
  'completed-at.txt': '2026-10-01T17:00:00Z\n',
  'config.txt': 'Release\n',
  'target.txt': 'qnxuser@qnxpi.local\n',
  'run-count.txt': '3\n',
  'playground-output.txt':
    'rnqnx_playground: Screen surface initialized (1280x720)\n' +
    'rnqnx_playground: exiting cleanly after 600 frames\n' +
    'rnqnx_playground: Screen surface initialized (1280x720)\n' +
    'rnqnx_playground: exiting cleanly after 600 frames\n' +
    'rnqnx_playground: Screen surface initialized (1280x720)\n' +
    'rnqnx_playground: exiting cleanly after 600 frames\n',
  'slog2-tail.txt': 'synthetic slog2 evidence\n',
  'preflight/qcc-V.txt': 'gcc_ntoaarch64le\n',
  'preflight/qxx-V.txt': 'gcc_ntoaarch64le\n',
  'preflight/target-preflight.txt':
    'RNQNX_PI_PREFLIGHT=1\nmachine=aarch64le\nscreen=ready\ntarget_dir=writable\n',
};

for (const [relative, contents] of Object.entries(files)) {
  await writeFile(path.join(fixture, relative), contents);
}

const result = spawnSync(
  process.execPath,
  [path.join(root, 'scripts', 'verify-pi-evidence.mjs'), fixture],
  { cwd: root, encoding: 'utf8' }
);
assert.equal(result.status, 0, result.stderr || result.stdout);

const manifest = JSON.parse(await readFile(path.join(fixture, 'manifest.json'), 'utf8'));
assert.equal(manifest.schemaVersion, 1);
assert.equal(manifest.target, 'qnxuser@qnxpi.local');
assert.equal(manifest.configuration, 'Release');
assert.equal(manifest.runCount, 3);
assert.equal(manifest.checks.aarch64le, true);
assert.equal(manifest.checks.screenReady, true);
assert.equal(manifest.checks.cleanShutdown, true);
assert.equal(manifest.checks.repeatedRuns, true);
assert.equal(manifest.checks.screenshotCaptured, false);
assert.match(manifest.files['playground-output.txt'].sha256, /^[a-f0-9]{64}$/);

await writeFile(
  path.join(fixture, 'playground-output.txt'),
  'rnqnx_playground: Screen surface initialized (1280x720)\n'
);

const badInitCount = spawnSync(
  process.execPath,
  [path.join(root, 'scripts', 'verify-pi-evidence.mjs'), fixture],
  { cwd: root, encoding: 'utf8' }
);
assert.notEqual(badInitCount.status, 0);
assert.match(badInitCount.stderr, /Screen initialization proof line/);

await writeFile(
  path.join(fixture, 'playground-output.txt'),
  'rnqnx_playground: Screen surface initialized (1280x720)\n' +
    'rnqnx_playground: exiting cleanly after 600 frames\n' +
    'rnqnx_playground: Screen surface initialized (1280x720)\n' +
    'rnqnx_playground: exiting cleanly after 600 frames\n' +
    'rnqnx_playground: Screen surface initialized (1280x720)\n'
);

const badCleanExitCount = spawnSync(
  process.execPath,
  [path.join(root, 'scripts', 'verify-pi-evidence.mjs'), fixture],
  { cwd: root, encoding: 'utf8' }
);
assert.notEqual(badCleanExitCount.status, 0);
assert.match(badCleanExitCount.stderr, /clean shutdown proof line/);

for (const malformedRunCount of ['3junk', '3.5']) {
  await writeFile(path.join(fixture, 'run-count.txt'), `${malformedRunCount}\n`);
  const malformed = spawnSync(
    process.execPath,
    [path.join(root, 'scripts', 'verify-pi-evidence.mjs'), fixture],
    { cwd: root, encoding: 'utf8' }
  );
  assert.notEqual(malformed.status, 0);
  assert.match(malformed.stderr, /run-count\.txt must contain an integer from 1 through 20/);
}

console.log('pi-evidence.test.mjs passed');
