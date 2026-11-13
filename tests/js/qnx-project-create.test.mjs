import assert from 'node:assert/strict';
import { createHash } from 'node:crypto';
import { spawnSync } from 'node:child_process';
import { lstat, mkdtemp, readFile, readdir, rm, writeFile } from 'node:fs/promises';
import { tmpdir } from 'node:os';
import path from 'node:path';

const root = await mkdtemp(path.join(tmpdir(), 'rnqnx-project-create-'));
const cli = path.resolve('packages/react-native-qnx-cli/src/bin.mjs');
const cache = path.join(root, 'npm-cache');
/** @param {string[]} args */
const run = (...args) =>
  spawnSync(process.execPath, [cli, 'project', 'create', ...args], {
    cwd: process.cwd(),
    encoding: 'utf8',
    timeout: 120000,
    maxBuffer: 4 * 1024 * 1024,
    env: { ...process.env, npm_config_cache: cache },
  });

try {
  const qnx = path.join(root, 'qnx-app');
  const params = ['--project', qnx, '--app-id', 'org.example.notes', '--name', 'Notes'];
  const dry = run(...params, '--dry-run');
  assert.equal(dry.status, 0, dry.stderr);
  assert.equal(JSON.parse(dry.stdout).action, 'dry-run');
  await assert.rejects(lstat(qnx), { code: 'ENOENT' });
  const created = run(...params);
  assert.equal(created.status, 0, created.stderr);
  assert.equal(JSON.parse(created.stdout).action, 'created');
  const qnxManifest = JSON.parse(await readFile(path.join(qnx, 'package.json'), 'utf8'));
  assert.equal(qnxManifest.private, true);
  assert.equal((await lstat(path.join(qnx, 'build', 'qnx'))).isDirectory(), true);
  assert.equal(qnxManifest.dependencies.expo, '58.0.24');
  assert.ok(qnxManifest.dependencies['@rn-qnx/react-native-qnx'].startsWith('file:vendor/'));
  assert.equal(qnxManifest.dependencies['@rn-qnx/react-native-marathon'], undefined);
  assert.equal(
    JSON.parse(await readFile(path.join(qnx, 'app.json'), 'utf8')).expo.extra.rnqnx.componentName,
    'main'
  );
  assert.equal(
    JSON.parse(await readFile(path.join(qnx, 'app.json'), 'utf8')).expo.icon,
    './assets/icon.png'
  );
  assert.deepEqual(
    await readFile(path.join(qnx, 'assets', 'icon.png')),
    await readFile(path.join(process.cwd(), 'template', 'qnx', 'assets', 'icon.png'))
  );
  const qnxArtifacts = JSON.parse(await readFile(path.join(qnx, 'platform-packages.json'), 'utf8'));
  const qnxArtifacts0 = qnxArtifacts.artifacts;
  assert.equal(Object.keys(qnxArtifacts.artifacts).length, 3);
  assert.ok(qnxArtifacts.artifacts['@rn-qnx/expo-router-qnx']);
  for (const artifact of Object.values(qnxArtifacts.artifacts)) {
    const data = await readFile(path.join(qnx, artifact.file));
    assert.equal(createHash('sha256').update(data).digest('hex'), artifact.sha256);
  }
  // The qnx-only project is the expo-router drawer and tabs starter.
  assert.equal(qnxManifest.dependencies['expo-router'], '58.0.31');
  assert.equal(qnxManifest.dependencies['react-native-screens'], '4.30.1');
  const layout = await readFile(path.join(qnx, 'app', '_layout.js'), 'utf8');
  assert.match(layout, /expo-router\/drawer/);
  assert.match(layout, /header\("Notes"\)/);
  assert.match(await readFile(path.join(qnx, 'app', '(tabs)', '_layout.js'), 'utf8'), /Tabs/);
  assert.match(await readFile(path.join(qnx, 'index.js'), 'utf8'), /registerComponent\('main'/);
  assert.match(await readFile(path.join(qnx, 'metro.config.js'), 'utf8'), /withExpoRouterQnx/);
  assert.doesNotMatch(layout, /__APP_NAME__/);
  assert.equal(JSON.parse(await readFile(path.join(qnx, 'app.json'), 'utf8')).expo.scheme, 'notes');
  // The router archive carries its source, the Metro helper and the README, and
  // the overrides point every platform archive at vendor/.
  const routerArchive = qnxArtifacts0['@rn-qnx/expo-router-qnx'];
  assert.ok(routerArchive.file.startsWith('vendor/'));
  const listing = spawnSync('tar', ['-tzf', path.join(qnx, routerArchive.file)], {
    encoding: 'utf8',
  });
  for (const entry of ['package/metro.js', 'package/README.md', 'package/src/resolver.js']) {
    assert.ok(listing.stdout.includes(entry), entry);
  }
  const workspaceYaml = await readFile(path.join(qnx, 'pnpm-workspace.yaml'), 'utf8');
  assert.match(workspaceYaml, /'@rn-qnx\/expo-router-qnx': 'file:vendor\//);
  for (const name of [
    'react-native-drawer-layout',
    'react-dom',
    'expo-linking',
    '@expo/metro-runtime',
  ]) {
    assert.ok(qnxManifest.dependencies[name], name);
  }
  // A hostile display name stays exact inside the generated JavaScript.
  const hostile = path.join(root, 'hostile-app');
  const hostileName = 'O\'Brien "x" $& {y} \\';
  const hostileRun = run(
    '--project',
    hostile,
    '--app-id',
    'org.example.hostile',
    '--name',
    hostileName,
    '--component-name',
    'Probe'
  );
  assert.equal(hostileRun.status, 0, hostileRun.stderr);
  const hostileLayout = await readFile(path.join(hostile, 'app', '_layout.js'), 'utf8');
  assert.ok(hostileLayout.includes('header(' + JSON.stringify(hostileName) + ')'));
  assert.match(
    await readFile(path.join(hostile, 'index.js'), 'utf8'),
    /registerComponent\('Probe'/
  );
  const before = await readFile(path.join(qnx, 'index.js'));
  const duplicate = run(...params);
  assert.notEqual(duplicate.status, 0);
  assert.deepEqual(await readFile(path.join(qnx, 'index.js')), before);
  const invalid = run('--project', path.join(root, 'bad'), '--app-id', 'bad', '--name', 'Bad');
  assert.notEqual(invalid.status, 0);
  await assert.rejects(lstat(path.join(root, 'bad')), { code: 'ENOENT' });

  const marathon = path.join(root, 'dual app');
  const dual = run(
    '--project',
    marathon,
    '--app-id',
    'org.example.dual',
    '--name',
    'Dual App',
    '--with-marathon'
  );
  assert.equal(dual.status, 0, dual.stderr);
  const dualManifest = JSON.parse(await readFile(path.join(marathon, 'package.json'), 'utf8'));
  assert.equal((await lstat(path.join(marathon, 'build', 'qnx'))).isDirectory(), true);
  assert.equal((await lstat(path.join(marathon, 'build', 'marathon'))).isDirectory(), true);
  assert.ok(dualManifest.dependencies['@rn-qnx/react-native-marathon']);
  assert.ok(dualManifest.dependencies['@rn-qnx/metro-config-marathon']);
  assert.ok(dualManifest.scripts['bundle:marathon']);
  assert.equal(
    JSON.parse(await readFile(path.join(marathon, 'app.json'), 'utf8')).expo.extra.rnqnx.platforms
      .length,
    2
  );
  assert.ok(
    (await readFile(path.join(marathon, 'metro.marathon.config.js'), 'utf8')).includes(
      'getMarathonMetroConfig'
    )
  );
  assert.equal((await readdir(path.join(marathon, 'vendor'))).length, 4);
  assert.ok(
    (await readFile(path.join(marathon, 'index.js'), 'utf8')).includes("from 'react-native'")
  );
  const failure = run(
    '--project',
    path.join(root, 'empty'),
    '--app-id',
    'org.example.empty',
    '--name',
    'Empty',
    '--with-marathon',
    '--with-marathon'
  );
  assert.notEqual(failure.status, 0);
  await assert.rejects(lstat(path.join(root, 'empty')), { code: 'ENOENT' });
  await writeFile(path.join(qnx, 'user-owned.txt'), 'preserve');
  assert.notEqual(run(...params).status, 0);
  assert.equal(await readFile(path.join(qnx, 'user-owned.txt'), 'utf8'), 'preserve');

  console.log(
    'QNX project create: real local archives, independent QNX and dual-target source, identity, hash, refusal and no-overwrite checks passed; native runtime not tested.'
  );
} finally {
  await rm(root, { recursive: true, force: true });
}
