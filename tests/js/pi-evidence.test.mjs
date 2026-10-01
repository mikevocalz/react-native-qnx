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
  'playground-output.txt':
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
assert.equal(manifest.checks.aarch64le, true);
assert.equal(manifest.checks.screenReady, true);
assert.equal(manifest.checks.cleanShutdown, true);
assert.equal(manifest.checks.screenshotCaptured, false);
assert.match(manifest.files['playground-output.txt'].sha256, /^[a-f0-9]{64}$/);

await writeFile(
  path.join(fixture, 'playground-output.txt'),
  'rnqnx_playground: Screen surface initialized (1280x720)\n'
);

const bad = spawnSync(
  process.execPath,
  [path.join(root, 'scripts', 'verify-pi-evidence.mjs'), fixture],
  { cwd: root, encoding: 'utf8' }
);
assert.notEqual(bad.status, 0);
assert.match(bad.stderr, /clean shutdown proof line/);

console.log('pi-evidence.test.mjs passed');
