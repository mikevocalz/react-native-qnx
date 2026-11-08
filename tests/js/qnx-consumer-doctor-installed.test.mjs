import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { mkdir, mkdtemp, readFile, rm, symlink, writeFile } from 'node:fs/promises';
import { createRequire } from 'node:module';
import { tmpdir } from 'node:os';
import path from 'node:path';
import { CONSUMER_CONTRACT } from '../../packages/react-native-qnx-cli/src/consumer-contract.mjs';

// Real installed workspace metadata, not a published or independently installed
// consumer artifact. Missing dependencies fail instead of skipping this lane.
const root = process.cwd();
const require = createRequire(path.join(root, 'package.json'));
const workspace = JSON.parse(await readFile(path.join(root, 'package.json'), 'utf8'));
const project = await mkdtemp(path.join(tmpdir(), 'rnqnx-doctor-installed-'));
try {
  assert.equal(workspace.packageManager, CONSUMER_CONTRACT.packageManager);
  await writeFile(
    path.join(project, 'package.json'),
    JSON.stringify({
      name: 'installed-graph-consumer',
      packageManager: workspace.packageManager,
    })
  );
  await writeFile(path.join(project, 'index.js'), 'throw new Error("must not execute entry");');
  for (const config of ['metro.config.js', 'metro.marathon.config.js']) {
    await writeFile(path.join(project, config), 'throw new Error("must not execute config");');
  }
  for (const [name, expected] of Object.entries({
    ...CONSUMER_CONTRACT.packages,
    ...CONSUMER_CONTRACT.marathonPackages,
  })) {
    let packageRoot;
    if (name.startsWith('@rn-qnx/')) {
      packageRoot = path.join(root, 'packages', name.split('/')[1]);
      const metadata = JSON.parse(await readFile(path.join(packageRoot, 'package.json'), 'utf8'));
      assert.equal(metadata.version, expected, `Consumer contract drift for ${name}`);
    } else {
      assert.equal(workspace.devDependencies[name], expected, `Consumer contract drift for ${name}`);
      packageRoot = path.dirname(require.resolve(`${name}/package.json`));
    }
    const link = path.join(project, 'node_modules', name);
    await mkdir(path.dirname(link), { recursive: true });
    await symlink(packageRoot, link, 'dir');
  }
  for (const platform of ['qnx', 'marathon']) {
    const result = spawnSync(
      process.execPath,
      [
        path.join(root, 'packages/react-native-qnx-cli/src/bin.mjs'),
        'project',
        'doctor',
        '--project',
        project,
        '--platform',
        platform,
        '--json',
      ],
      {
        cwd: project,
        encoding: 'utf8',
        timeout: 15000,
        killSignal: 'SIGKILL',
        maxBuffer: 1024 * 1024,
      }
    );
    assert.equal(result.error, undefined);
    assert.equal(result.status, 0, result.stdout + result.stderr);
    const report = JSON.parse(result.stdout);
    assert.equal(report.metadataCompatible, true);
    assert.equal(report.runtimeVerified, false);
    assert.equal(report.platform, platform);
    assert.equal(
      report.checks.filter((/** @type {{status: string}} */ item) => item.status === 'failed')
        .length,
      0
    );
  }
  console.log(
    'Consumer doctor: both target CLI reports matched the actual installed workspace graph; native/runtime not verified.'
  );
} finally {
  await rm(project, { recursive: true, force: true });
}
