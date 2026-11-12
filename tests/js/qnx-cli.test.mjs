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
  const runFailure = run(['run-qnx', '--arch', 'x86_64', '--target', 'demo_app'], env);
  assert.equal(runFailure.status, 37, 'run must preserve SSH failure');
  assert.match(
    runFailure.stderr,
    /rnqnx: run-qnx exited with code 37\. Deploy first with pnpm qnx deploy-qnx --arch x86_64 --target demo_app, and check QNX_TARGET_IP and QNX_TARGET_USER\./
  );
  const logFailure = run(['log-qnx'], env);
  assert.equal(logFailure.status, 37, 'log must preserve SSH failure');
  assert.match(
    logFailure.stderr,
    /rnqnx: log-qnx exited with code 37\. Check QNX_TARGET_IP and QNX_TARGET_USER; the board must accept SSH\./
  );

  // Usage errors name the form to use instead.
  /** @type {Array<[string[], RegExp]>} */
  const usage = [
    [['--dry-run'], /^rnqnx: Missing command\. Run pnpm qnx --help to list commands\.$/m],
    [['frob'], /^rnqnx: Unknown command: frob\. Run pnpm qnx --help to list commands\.$/m],
    [['init'], /^rnqnx: init needs exactly one app name, for example: pnpm qnx init dashboard$/m],
    [
      ['init', 'one', 'two'],
      /^rnqnx: init needs exactly one app name, for example: pnpm qnx init dashboard$/m,
    ],
    [['evidence'], /^rnqnx: Use pnpm qnx evidence verify \[--dir <path>\]\.$/m],
    [['evidence', 'verify', 'extra'], /^rnqnx: Use pnpm qnx evidence verify \[--dir <path>\]\.$/m],
    [
      ['doctor', 'extra', 'words'],
      /^rnqnx: doctor takes options only; remove extra words\. Run pnpm qnx --help for its options\.$/m,
    ],
    [
      ['bundle-qnx', '--app', 'nope', '--dry-run'],
      /^rnqnx: apps\/nope\/index\.js not found\. Create the app with pnpm qnx init nope and run pnpm install --no-frozen-lockfile, or pass an existing --app name\.$/m,
    ],
  ];
  for (const [args, message] of usage) {
    const result = run(args);
    assert.equal(result.status, 1, args.join(' '));
    assert.match(result.stderr, message, args.join(' '));
  }

  // A failing configure/build/deploy child adds a next step with the selected
  // arch, config and target interpolated. The stub scripts only exit nonzero.
  for (const name of ['configure-qnx', 'build-qnx', 'deploy-qnx']) {
    await writeFile(path.join(temporary, 'scripts', `${name}.sh`), 'exit 3\n', { mode: 0o755 });
  }
  /** @type {Array<[string[], RegExp]>} */
  const children = [
    [
      ['configure-qnx', '--arch', 'x86_64', '--config', 'Release'],
      /^rnqnx: configure-qnx exited with code 3\. Run pnpm qnx doctor; it names the missing QNX SDP or build tool\.$/m,
    ],
    [
      ['build-qnx', '--arch', 'x86_64', '--config', 'Release'],
      /^rnqnx: build-qnx exited with code 3\. Run pnpm qnx configure-qnx --arch x86_64 --config Release first; if it fails too, run pnpm qnx doctor\.$/m,
    ],
    [
      ['deploy-qnx', '--arch', 'x86_64', '--config', 'Release', '--target', 'demo_app'],
      /^rnqnx: deploy-qnx exited with code 3\. Check that pnpm qnx build-qnx --arch x86_64 --config Release succeeded and that QNX_TARGET_IP and QNX_TARGET_USER reach the board over SSH\.$/m,
    ],
  ];
  for (const [args, message] of children) {
    const result = run(args);
    assert.equal(result.status, 3, args.join(' '));
    assert.match(result.stderr, message, args.join(' '));
  }

  // A failing Metro build adds the bundle-qnx next step.
  await mkdir(path.join(temporary, 'apps/broken'));
  await cp(
    path.join(temporary, 'apps/dashboard/metro.config.js'),
    path.join(temporary, 'apps/broken/metro.config.js')
  );
  await writeFile(path.join(temporary, 'apps/broken/index.js'), "require('./missing-module');\n");
  const metroFailure = spawnSync(process.execPath, [cli, 'bundle-qnx', '--app', 'broken'], {
    cwd: temporary,
    encoding: 'utf8',
    timeout: 120000,
  });
  assert.notEqual(metroFailure.status, 0);
  assert.match(
    metroFailure.stderr,
    /rnqnx: bundle-qnx exited with code \d+\. Read the Metro error above; a missing module usually means pnpm install --no-frozen-lockfile was not run after init\./
  );

  // bundle-qnx without an installed Metro names the install command.
  const bare = await realpath(await mkdtemp(path.join(tmpdir(), 'rnqnx-cli-bare-')));
  try {
    await cp(path.join(root, 'package.json'), path.join(bare, 'package.json'));
    await cp(path.join(root, 'CMakePresets.json'), path.join(bare, 'CMakePresets.json'));
    await mkdir(path.join(bare, 'apps/demo'), { recursive: true });
    await writeFile(path.join(bare, 'apps/demo/index.js'), '');
    const result = spawnSync(process.execPath, [cli, 'bundle-qnx', '--app', 'demo', '--dry-run'], {
      cwd: bare,
      encoding: 'utf8',
      timeout: 10000,
    });
    assert.equal(result.status, 1, result.stderr);
    assert.match(
      result.stderr,
      /^rnqnx: Metro is not installed in this checkout\. Run pnpm install --frozen-lockfile from the repository root\.$/m
    );
  } finally {
    await rm(bare, { recursive: true, force: true });
  }
  console.log(
    'qnx-cli.test.mjs passed (init safety, output symlink confinement, option rejection, dry-run, real shell failure propagation; no device connection)'
  );
} finally {
  await rm(temporary, { recursive: true, force: true });
}
