import assert from 'node:assert/strict';
import { createHash } from 'node:crypto';
import { spawnSync } from 'node:child_process';
import { cp, mkdir, mkdtemp, readFile, realpath, rm, writeFile } from 'node:fs/promises';
import { tmpdir } from 'node:os';
import path from 'node:path';

const root = process.cwd();
const cli = path.join(root, 'packages/react-native-qnx-cli/src/cli.mjs');
const temporary = await realpath(await mkdtemp(path.join(tmpdir(), 'rnqnx-evidence-')));

/** @param {string[]} args */
function run(args) {
  const result = spawnSync(process.execPath, [cli, ...args], {
    cwd: temporary,
    encoding: 'utf8',
    timeout: 10000,
  });
  assert.equal(result.error, undefined);
  return result;
}

try {
  // This is a synthetic Screen-probe fixture. It never connects to a Pi or
  // represents React Native/Fabric, device parity, or a release gate pass.
  await cp(path.join(root, 'package.json'), path.join(temporary, 'package.json'));
  await cp(path.join(root, 'CMakePresets.json'), path.join(temporary, 'CMakePresets.json'));
  await mkdir(path.join(temporary, 'scripts'));
  await cp(
    path.join(root, 'scripts/verify-pi-evidence.mjs'),
    path.join(temporary, 'scripts/verify-pi-evidence.mjs')
  );
  const evidence = path.join(temporary, 'artifacts/pi/2026-11-07-fixture');
  await mkdir(path.join(evidence, 'preflight'), { recursive: true });
  const files = {
    'completed-at.txt': '2026-11-07T17:00:00Z\n',
    'config.txt': 'Debug\n',
    'playground-output.txt':
      'rnqnx_playground: Screen surface initialized (640x480)\n' +
      'rnqnx_playground: exiting cleanly after 3 frames\n',
    'slog2-tail.txt': 'synthetic logs, no device\n',
    'target.txt': 'qnxuser@qnxpi.local\n',
    'board.txt': 'pi400\n',
    'preflight/qcc-V.txt': 'gcc_ntoaarch64le\n',
    'preflight/qxx-V.txt': 'q++ compiler fixture\n',
    'preflight/target-preflight.txt': 'machine=aarch64le\nscreen=ready\ntarget_dir=writable\n',
  };
  for (const [file, value] of Object.entries(files)) {
    await writeFile(path.join(evidence, file), value);
  }

  const dryRun = run([
    'evidence',
    'verify',
    '--dir',
    'artifacts/pi/2026-11-07-fixture',
    '--dry-run',
  ]);
  assert.equal(dryRun.status, 0, dryRun.stderr);
  const plan = JSON.parse(dryRun.stdout);
  assert.equal(plan.executable, process.execPath);
  assert.deepEqual(plan.args, [path.join(temporary, 'scripts/verify-pi-evidence.mjs'), evidence]);
  assert.equal(plan.cwd, temporary);
  await assert.rejects(readFile(path.join(evidence, 'manifest.json')), /ENOENT/);

  const checked = run(['evidence', 'verify', '--dir', evidence]);
  assert.equal(checked.status, 0, checked.stderr);
  assert.match(checked.stdout, /Pi evidence verified/);
  const manifest = JSON.parse(await readFile(path.join(evidence, 'manifest.json'), 'utf8'));
  assert.equal(manifest.board, 'pi400');
  assert.equal(manifest.runCount, 1);
  assert.equal(manifest.checks.screenshotCaptured, false);
  assert.equal(manifest.checks.repeatedRuns, false);
  assert.equal(
    manifest.files['config.txt'].sha256,
    createHash('sha256').update(files['config.txt']).digest('hex')
  );

  const latest = run(['evidence', 'verify']);
  assert.equal(latest.status, 0, latest.stderr);
  assert.match(latest.stdout, /2026-11-07-fixture/);

  const invalidCases = [
    ['evidence'],
    ['evidence', 'list'],
    ['evidence', 'verify', '--target', 'rnqnx_playground'],
    ['evidence', 'verify', '--dir', ''],
    ['evidence', 'verify', 'extra'],
  ];
  for (const args of invalidCases) {
    const result = run(args);
    assert.notEqual(result.status, 0, args.join(' '));
  }
  const missingDir = run(['evidence', 'verify', '--dir', 'artifacts/pi/not-a-run']);
  assert.notEqual(missingDir.status, 0);
  assert.match(missingDir.stderr, /Missing required evidence file/);

  console.log(
    'qnx-cli-evidence.test.mjs passed (dry-run, real verifier, SHA-256 manifest, newest run, errors; synthetic host fixture only)'
  );
} finally {
  await rm(temporary, { recursive: true, force: true });
}
