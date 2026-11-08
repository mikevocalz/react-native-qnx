import assert from 'node:assert/strict';
import { createHash } from 'node:crypto';
import { spawnSync } from 'node:child_process';
import { lstat, mkdtemp, readFile, readdir, rm, writeFile } from 'node:fs/promises';
import { tmpdir } from 'node:os';
import path from 'node:path';

const root = await mkdtemp(path.join(tmpdir(), 'rnqnx-project-create-'));
const cli = path.resolve('packages/react-native-qnx-cli/src/bin.mjs');
const cache = path.join(root, 'npm-cache');
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
  assert.equal(qnxManifest.dependencies.expo, '58.0.22');
  assert.ok(qnxManifest.dependencies['@rn-qnx/react-native-qnx'].startsWith('file:vendor/'));
  assert.equal(qnxManifest.dependencies['@rn-qnx/react-native-marathon'], undefined);
  assert.equal(
    JSON.parse(await readFile(path.join(qnx, 'app.json'), 'utf8')).expo.extra.rnqnx.componentName,
    'main'
  );
  const qnxArtifacts = JSON.parse(await readFile(path.join(qnx, 'platform-packages.json'), 'utf8'));
  assert.equal(Object.keys(qnxArtifacts.artifacts).length, 2);
  for (const artifact of Object.values(qnxArtifacts.artifacts)) {
    const data = await readFile(path.join(qnx, artifact.file));
    assert.equal(createHash('sha256').update(data).digest('hex'), artifact.sha256);
  }
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
