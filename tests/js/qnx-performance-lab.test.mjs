import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { createHash } from 'node:crypto';
import { mkdir, mkdtemp, readFile, rm } from 'node:fs/promises';
import { createRequire } from 'node:module';
import path from 'node:path';

const root = process.cwd();
const app = path.join(root, 'apps/performance-lab');
const require = createRequire(import.meta.url);
const fixture = require(path.join(app, 'fixtures.js'));
/** @type {Array<{ id: string, entry: string | null, status: string }>} */
const workloads = fixture.WORKLOADS;
assert.deepEqual(
  workloads.map((workload) => workload.id),
  ['hello-world', 'list-1000', 'drawer-drag']
);
assert.equal(workloads[2].status, 'blocked');
assert.equal(workloads[2].entry, null);
const rows = fixture.createRows();
assert.equal(rows.length, 1000);
assert.equal(rows[0].id, 'row-0001');
assert.equal(rows[999].id, 'row-1000');
assert.deepEqual(fixture.createRows(3), rows.slice(0, 3));
for (const invalid of [0, -1, 1001, 1.5, NaN, '1000']) {
  assert.throws(() => fixture.createRows(invalid), RangeError);
}
const baseline = await readFile(path.join(app, 'hello-world.js'), 'utf8');
assert.match(baseline, /require\('\.\.\/rn-playground\/hello-world\.js'\)/);
const source = await readFile(path.join(app, 'index.js'), 'utf8');
assert.match(source, /FlatList/);
assert.match(source, /registerComponent\('RNQnxPlayground'/);
assert.doesNotMatch(source, /require\('react-native'\)|useState|useReducer|nitro-forge/);

// Execute actual Metro, not a mock resolver or source-string-only assertion.
const metroPackage = require.resolve('metro/package.json');
const metro = JSON.parse(await readFile(metroPackage, 'utf8'));
const entry = typeof metro.bin === 'string' ? metro.bin : metro.bin?.metro;
assert.equal(typeof entry, 'string', 'Installed Metro must expose its CLI.');
const executable = path.resolve(path.dirname(metroPackage), entry);
await mkdir(path.join(root, 'build'), { recursive: true });
const output = await mkdtemp(path.join(root, 'build/performance-lab-test-'));
try {
  for (const workload of workloads) {
    if (workload.entry === null) continue;
    const bundlePath = path.join(output, `${workload.id}.bundle.js`);
    const result = spawnSync(
      process.execPath,
      [
        executable,
        'build',
        workload.entry,
        '--config',
        path.join(app, 'metro.config.js'),
        '--platform',
        'qnx',
        '--dev',
        'false',
        '--max-workers',
        '2',
        '--out',
        bundlePath,
      ],
      {
        cwd: app,
        encoding: 'utf8',
        timeout: 90000,
        killSignal: 'SIGKILL',
        maxBuffer: 8 * 1024 * 1024,
      }
    );
    assert.equal(result.error, undefined, `${workload.id}: ${result.error?.message}`);
    assert.equal(result.status, 0, `${workload.id}: ${result.stderr}\n${result.stdout}`);
    const bundle = await readFile(bundlePath);
    assert.ok(bundle.length > 0);
    assert.match(bundle.toString('utf8'), /RNQnxPlayground/);
    const sha256 = createHash('sha256').update(bundle).digest('hex');
    console.log(`performance-lab bundle ${workload.id}: ${bundle.length} bytes ${sha256}`);
  }
  console.log('Performance workload bundles passed; no device, FPS, RSS or startup claim.');
} finally {
  await rm(output, { recursive: true, force: true });
}
