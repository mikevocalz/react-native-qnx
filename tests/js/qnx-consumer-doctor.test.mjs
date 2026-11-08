import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { createHash } from 'node:crypto';
import { mkdir, mkdtemp, readFile, readdir, rm, symlink, writeFile } from 'node:fs/promises';
import { tmpdir } from 'node:os';
import path from 'node:path';
import { inspectConsumer } from '../../packages/react-native-qnx-cli/src/doctor.mjs';
import { CONSUMER_CONTRACT } from '../../packages/react-native-qnx-cli/src/consumer-contract.mjs';

const temporary = await mkdtemp(path.join(tmpdir(), 'rnqnx-doctor-'));
const cli = path.resolve('packages/react-native-qnx-cli/src/doctor.mjs');
const runtimeOk = Number(process.versions.node.split('.')[0]) >= CONSUMER_CONTRACT.minimumNodeMajor;
let checks = 0;
/** @param {string} name @param {unknown} contents */
async function installed(name, contents) {
  const directory = path.join(temporary, 'node_modules', name);
  await mkdir(directory, { recursive: true });
  await writeFile(path.join(directory, 'package.json'), JSON.stringify(contents));
}
/** @param {string[]} args */
function run(args) {
  const result = spawnSync(process.execPath, [cli, '--project', temporary, ...args], {
    cwd: temporary,
    encoding: 'utf8',
    timeout: 10000,
    killSignal: 'SIGKILL',
  });
  assert.equal(result.error, undefined);
  return result;
}
/** @param {string} directory @returns {Promise<Record<string, string>>} */
async function hashes(directory) {
  /** @type {Record<string, string>} */
  const result = {};
  for (const entry of await readdir(directory, { withFileTypes: true })) {
    const file = path.join(directory, entry.name);
    if (entry.isDirectory()) Object.assign(result, await hashes(file));
    else if (entry.isFile())
      result[file] = createHash('sha256')
        .update(await readFile(file))
        .digest('hex');
    else result[file] = entry.isSymbolicLink() ? 'symlink' : 'other';
  }
  return result;
}
/** @param {Awaited<ReturnType<typeof inspectConsumer>>} report @param {string} id */
function check(report, id) {
  const found = report.checks.find((item) => item.id === id);
  assert.ok(found, `Missing ${id}`);
  checks += 1;
  return found;
}
try {
  const manifest = {
    name: 'external-doctor-app',
    private: true,
    packageManager: CONSUMER_CONTRACT.packageManager,
    dependencies: {},
  };
  const packageFile = path.join(temporary, 'package.json');
  await writeFile(packageFile, JSON.stringify(manifest));
  await writeFile(path.join(temporary, 'index.js'), 'throw new Error("entry must not execute");');
  await writeFile(
    path.join(temporary, 'metro.config.js'),
    `require('fs').writeFileSync(${JSON.stringify(path.join(temporary, 'EXECUTED'))},'bad');throw new Error('config executed');`
  );
  for (const [name, version] of Object.entries(CONSUMER_CONTRACT.packages)) {
    await installed(name, { name, version, main: 'index.js' });
    await writeFile(
      path.join(temporary, 'node_modules', name, 'index.js'),
      'throw new Error("package executed");'
    );
  }
  const before = await hashes(temporary);
  let report = await inspectConsumer(temporary);
  assert.equal(report.platform, 'qnx');
  assert.equal(report.metadataCompatible, runtimeOk);
  assert.equal(report.runtimeVerified, false);
  assert.equal(report.nativeBuildVerified, false);
  assert.equal(check(report, 'node').status, runtimeOk ? 'passed' : 'failed');
  assert.equal(check(report, 'project-manifest').status, 'passed');
  for (const name of Object.keys(CONSUMER_CONTRACT.packages)) {
    assert.equal(check(report, `package:${name}`).status, 'passed');
  }
  for (const id of ['native-toolchain', 'native-modules', 'runtime', 'debugger']) {
    assert.equal(check(report, id).status, 'not-checked');
  }
  const result = run(['--json']);
  assert.equal(result.status, runtimeOk ? 0 : 1, result.stderr);
  assert.equal(JSON.parse(result.stdout).metadataCompatible, runtimeOk);
  checks += 1;
  assert.deepEqual(await hashes(temporary), before);
  checks += 1;

  report = await inspectConsumer(temporary, { platform: 'marathon' });
  assert.equal(check(report, 'metro-config').status, 'failed');
  assert.equal(check(report, 'package:@rn-qnx/react-native-marathon').status, 'failed');
  await writeFile(
    path.join(temporary, 'metro.marathon.config.js'),
    'throw new Error("never import config");'
  );
  for (const [name, version] of Object.entries(CONSUMER_CONTRACT.marathonPackages))
    await installed(name, { name, version });
  report = await inspectConsumer(temporary, { platform: 'marathon' });
  assert.equal(report.metadataCompatible, runtimeOk);
  checks += 1;
  assert.equal(check(report, 'metro-config').status, 'passed');
  assert.equal(check(report, 'package:@rn-qnx/react-native-marathon').status, 'passed');

  await installed('react', { name: 'react', version: '0.0.0' });
  report = await inspectConsumer(temporary);
  assert.equal(check(report, 'package:react').status, 'failed');
  assert.equal(report.metadataCompatible, false);
  checks += 1;
  await installed('react', { name: 'impostor', version: CONSUMER_CONTRACT.packages.react });
  assert.equal(check(await inspectConsumer(temporary), 'package:react').status, 'failed');
  await installed('react', { name: 'react', version: CONSUMER_CONTRACT.packages.react });
  await rm(path.join(temporary, 'node_modules/expo/package.json'));
  assert.equal(check(await inspectConsumer(temporary), 'package:expo').status, 'failed');
  await installed('expo', { name: 'expo', version: CONSUMER_CONTRACT.packages.expo });
  await writeFile(path.join(temporary, 'node_modules/expo/package.json'), 'invalid');
  assert.equal(check(await inspectConsumer(temporary), 'package:expo').status, 'failed');
  await installed('expo', { name: 'expo', version: CONSUMER_CONTRACT.packages.expo });

  await writeFile(packageFile, JSON.stringify({ ...manifest, packageManager: 'pnpm@0.0.0' }));
  assert.equal(check(await inspectConsumer(temporary), 'package-manager').status, 'failed');
  await writeFile(packageFile, JSON.stringify({ name: 'app' }));
  assert.equal(check(await inspectConsumer(temporary), 'package-manager').status, 'not-checked');
  for (const invalid of [
    [],
    null,
    {},
    { name: 'app', dependencies: [] },
    { name: 'app', dependencies: { react: 42 } },
  ]) {
    await writeFile(packageFile, JSON.stringify(invalid));
    report = await inspectConsumer(temporary);
    assert.equal(check(report, 'project-manifest').status, 'failed');
    assert.equal(check(report, 'package-graph').status, 'not-checked');
  }
  await writeFile(packageFile, ' '.repeat(1024 * 1024 + 1));
  assert.equal(check(await inspectConsumer(temporary), 'project-manifest').status, 'failed');
  await writeFile(packageFile, JSON.stringify(manifest));

  for (const entry of [
    '../outside.js',
    '/tmp/outside.js',
    'C:\\outside.js',
    'sub/../index.js',
    'sub//index.js',
    '',
  ]) {
    assert.equal(check(await inspectConsumer(temporary, { entry }), 'entry').status, 'failed');
  }
  await symlink('index.js', path.join(temporary, 'linked.js'));
  assert.equal(
    check(await inspectConsumer(temporary, { entry: 'linked.js' }), 'entry').status,
    'failed'
  );
  await mkdir(path.join(temporary, 'sub'));
  await writeFile(path.join(temporary, 'sub/main.js'), 'valid source path');
  assert.equal(
    check(await inspectConsumer(temporary, { entry: 'sub/main.js' }), 'entry').status,
    'passed'
  );
  await rm(path.join(temporary, 'metro.config.js'));
  await symlink('index.js', path.join(temporary, 'metro.config.js'));
  assert.equal(check(await inspectConsumer(temporary), 'metro-config').status, 'failed');

  for (const args of [
    ['--platform', 'android'],
    ['--project', temporary],
    ['--json', '--json'],
    ['--unknown'],
    ['extra'],
    ['--entry'],
  ]) {
    const invalid = run(args);
    assert.equal(invalid.status, 2, invalid.stdout + invalid.stderr);
    checks += 1;
  }
  const help = run(['--help']);
  assert.equal(help.status, 0);
  assert.match(help.stdout, /Read-only metadata checks/);
  checks += 1;
  console.log(
    `Consumer doctor: ${checks} checks passed (metadata fixtures; actual Node minimum enforced, no config execution or device proof).`
  );
} finally {
  await rm(temporary, { recursive: true, force: true });
}
