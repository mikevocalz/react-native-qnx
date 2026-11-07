import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { realpath, mkdtemp, cp, mkdir, readFile, writeFile, rm, symlink } from 'node:fs/promises';
import { tmpdir } from 'node:os';
import path from 'node:path';

const root = process.cwd();
const cli = path.join(root, 'packages/react-native-qnx-cli/src/cli.mjs');
const temporary = await realpath(await mkdtemp(path.join(tmpdir(), 'rnqnx-cli-')));
/** @param {string[]} args @param {NodeJS.ProcessEnv} [env] */
function run(args, env = process.env) {
  const result = spawnSync(process.execPath, [cli, ...args], {
    cwd: temporary,
    env,
    encoding: 'utf8',
    timeout: 10000,
  });
  assert.equal(result.error, undefined);
  return result;
}
try {
  await cp(path.join(root, 'package.json'), path.join(temporary, 'package.json'));
  await cp(path.join(root, 'CMakePresets.json'), path.join(temporary, 'CMakePresets.json'));
  await cp(path.join(root, 'template'), path.join(temporary, 'template'), { recursive: true });
  await mkdir(path.join(temporary, 'apps'));
  await symlink(path.join(root, 'node_modules'), path.join(temporary, 'node_modules'), 'dir');
  await mkdir(path.join(temporary, 'scripts/lib'), { recursive: true });
  for (const file of ['run-qnx.sh', 'log-qnx.sh', 'lib/qnx-target.sh']) {
    await cp(path.join(root, 'scripts', file), path.join(temporary, 'scripts', file));
  }
  const created = run(['init', 'dashboard']);
  assert.equal(created.status, 0, created.stderr);
  const manifestPath = path.join(temporary, 'apps/dashboard/package.json');
  const manifest = JSON.parse(await readFile(manifestPath, 'utf8'));
  const workspace = JSON.parse(await readFile(path.join(root, 'package.json'), 'utf8'));
  assert.equal(manifest.dependencies.react, workspace.devDependencies.react);
  assert.equal(manifest.dependencies['react-native'], workspace.devDependencies['react-native']);
  const entry = await readFile(path.join(temporary, 'apps/dashboard/index.js'), 'utf8');
  assert.match(entry, /dashboard on React Native QNX/);
  assert.match(entry, /registerComponent\('RNQnxPlayground'/);
  assert.doesNotMatch(entry, /useState|useReducer|__APP_NAME__/);
  const bundle = run(['bundle-qnx', '--app', 'dashboard', '--dry-run']);
  assert.equal(bundle.status, 0, bundle.stderr);
  const bundlePlan = JSON.parse(bundle.stdout);
  assert.equal(bundlePlan.executable, process.execPath);
  assert.ok(bundlePlan.args[0].endsWith('/metro/src/cli.js'));
  assert.ok(bundlePlan.args.includes('qnx'));
  const build = path.join(temporary, 'build');
  await mkdir(build);
  const nested = run([
    'bundle-qnx',
    '--app',
    'dashboard',
    '--out',
    'build/new/nested/app.js',
    '--dry-run',
  ]);
  assert.equal(nested.status, 0, nested.stderr);
  assert.equal(JSON.parse(nested.stdout).args.at(-1), path.join(build, 'new/nested/app.js'));
  await mkdir(path.join(build, 'inside'));
  await writeFile(path.join(build, 'inside/existing.js'), 'previous bundle');
  await symlink(path.join(build, 'inside'), path.join(build, 'inside-link'), 'dir');
  for (const output of ['build/inside/existing.js', 'build/inside-link/new/app.js']) {
    const valid = run(['bundle-qnx', '--app', 'dashboard', '--out', output, '--dry-run']);
    assert.equal(valid.status, 0, valid.stderr);
  }
  const sentinel = path.join(temporary, 'apps/dashboard/index.js');
  await symlink(path.dirname(sentinel), path.join(build, 'escaped-directory'), 'dir');
  await symlink(sentinel, path.join(build, 'escaped-file.js'));
  await symlink(path.join(temporary, 'missing-outside.js'), path.join(build, 'dangling-file.js'));
  for (const output of [
    'build/escaped-directory/index.js',
    'build/escaped-file.js',
    'build/dangling-file.js',
  ]) {
    const rejected = run(['bundle-qnx', '--app', 'dashboard', '--out', output, '--dry-run']);
    assert.equal(rejected.status, 1, output);
    assert.match(rejected.stderr, /symlinks must stay inside/);
  }
  assert.equal(await readFile(sentinel, 'utf8'), entry);
  await rm(build, { recursive: true });
  await symlink(path.dirname(sentinel), build, 'dir');
  const redirectedBuild = run([
    'bundle-qnx',
    '--app',
    'dashboard',
    '--out',
    'build/index.js',
    '--dry-run',
  ]);
  assert.equal(redirectedBuild.status, 1);
  assert.match(redirectedBuild.stderr, /build\/ itself must not be a symlink/);
  assert.equal(await readFile(sentinel, 'utf8'), entry);
  await rm(build);
  const external = await mkdtemp(path.join(tmpdir(), 'rnqnx-cli-external-'));
  try {
    await symlink(external, build, 'dir');
    const rejected = run(['bundle-qnx', '--app', 'dashboard', '--dry-run']);
    assert.equal(rejected.status, 1);
    assert.match(rejected.stderr, /symlinks must stay inside/);
  } finally {
    await rm(build);
    await rm(external, { recursive: true });
  }
  assert.equal(run(['init', 'dashboard']).status, 1);
  assert.equal(await readFile(path.join(temporary, 'apps/dashboard/index.js'), 'utf8'), entry);
  for (const args of [
    ['init', '../escape'],
    ['init', 'dashboard', '--dry-run'],
    ['run-qnx', '--target', 'bad;touch-sentinel'],
    ['run-qnx', '--target', '--bad'],
    ['deploy-qnx', '--config', 'RelWithDebInfo'],
    ['build-qnx', '--arch', 'arm32'],
    ['doctor', '--target', 'rnqnx_playground'],
    ['run-qnx', 'extra'],
    ['unknown'],
    ['bundle-qnx', '--app', 'dashboard', '--out', 'apps/dashboard/index.js'],
  ])
    assert.equal(run(args).status, 1, args.join(' '));
  const plan = run([
    'deploy-qnx',
    '--arch',
    'x86_64',
    '--config',
    'Release',
    '--target',
    'rnqnx_playground',
    '--dry-run',
  ]);
  assert.equal(plan.status, 0, plan.stderr);
  const command = JSON.parse(plan.stdout);
  assert.equal(command.executable, 'bash');
  assert.deepEqual(command.args, [
    path.join(temporary, 'scripts/deploy-qnx.sh'),
    'x86_64',
    'rnqnx_playground',
    'Release',
  ]);
  // Real shell helpers run against a local SSH failure fixture; no network is used.
  const bin = path.join(temporary, 'bin');
  await mkdir(bin);
  await writeFile(path.join(bin, 'ssh'), '#!/bin/sh\nexit 37\n', { mode: 0o755 });
  const env = {
    ...process.env,
    PATH: `${bin}${path.delimiter}${process.env.PATH}`,
    QNX_TARGET_IP: 'fixture.invalid',
    QNX_TARGET_USER: 'qnxuser',
    QNX_SSH_KEY: '',
  };
  assert.equal(run(['run-qnx'], env).status, 37, 'run must preserve SSH failure');
  assert.equal(run(['log-qnx'], env).status, 37, 'log must preserve SSH failure');
  console.log(
    'qnx-cli.test.mjs passed (init safety, output symlink confinement, option rejection, dry-run, real shell failure propagation; no device connection)'
  );
} finally {
  await rm(temporary, { recursive: true, force: true });
}
