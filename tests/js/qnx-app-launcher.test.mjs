import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import {
  copyFile,
  chmod,
  lstat,
  mkdir,
  mkdtemp,
  readFile,
  readdir,
  rm,
  symlink,
  writeFile,
} from 'node:fs/promises';
import path from 'node:path';
import { tmpdir } from 'node:os';
import { crc32 } from 'node:zlib';
import {
  launcherConfiguration,
  launcherFiles,
  prepareLauncher,
  validateLauncherIcon,
} from '../../packages/react-native-qnx-cli/src/launcher.mjs';
import {
  installLauncherScript,
  launcherMain,
  launcherTarget,
  removeLauncherScript,
  runLauncherTool,
} from '../../packages/react-native-qnx-cli/src/launcher-device.mjs';
import { prepareNativeMain } from '../../packages/react-native-qnx-cli/src/prepare-native.mjs';
import { planNativeBuild } from '../../packages/react-native-qnx-cli/src/build-native.mjs';

const temporary = await mkdtemp(path.join(tmpdir(), 'rnqnx-launcher-'));
const project = path.join(temporary, 'consumer');
const home = path.join(temporary, 'home/developer');
const deployed = home + '/application';
const stock = path.join(temporary, 'stock');
const registry = home + '/.local/share/rnqnx/launcher';
const app = {
  expo: {
    name: 'Field Notes',
    icon: './assets/icon.png',
    extra: { rnqnx: { appId: 'org.example.notes', componentName: 'main', platforms: ['qnx'] } },
  },
};
let checks = 0;
/** @param {unknown} value */
function check(value) {
  assert.ok(value);
  checks++;
}
/** @param {() => Promise<unknown>} operation */
async function rejects(operation) {
  await assert.rejects(operation);
  checks++;
}
/** @param {string} script @param {boolean} [success] @param {NodeJS.ProcessEnv} [env] */
function shell(script, success = true, env = {}) {
  const result = spawnSync('/bin/sh', [], {
    input: script,
    encoding: 'utf8',
    env: { ...process.env, HOME: home, ...env },
    timeout: 10000,
  });
  assert.equal(result.status === 0, success, result.stdout + result.stderr);
  checks++;
  return result;
}
try {
  await mkdir(project + '/assets', { recursive: true });
  await mkdir(deployed, { recursive: true });
  await mkdir(stock);
  await writeFile(deployed + '/app', '#!/bin/sh\nexit 0\n');
  await chmod(deployed + '/app', 0o700);
  await writeFile(deployed + '/keep-user-data', 'not owned by launcher');
  await copyFile('packages/react-native-qnx-cli/assets/icon.png', project + '/assets/icon.png');
  await writeFile(project + '/app.json', JSON.stringify(app));
  await writeFile(
    stock + '/desktop.conf',
    'LogoText=Original QNX\nRowTwoHeader=QNX Sample Applications\n[Launcher]\nDesktopFileName=stock.desktop\nRow=2\nOrder=6\n'
  );
  await writeFile(
    stock + '/stock.desktop',
    '[Desktop Entry]\nName=Stock\nExec=/system/bin/stock\n'
  );
  const { configuration, icon } = await launcherConfiguration(project);
  const recoveryHome = temporary + '/home/recovery';
  const recoveryApp = recoveryHome + '/application';
  const recoveryRegistry = recoveryHome + '/.local/share/rnqnx/launcher';
  const incompleteStock = temporary + '/incomplete-stock';
  await mkdir(recoveryApp, { recursive: true });
  await copyFile(deployed + '/app', recoveryApp + '/app');
  await chmod(recoveryApp + '/app', 0o700);
  await mkdir(incompleteStock);
  await copyFile(stock + '/stock.desktop', incompleteStock + '/stock.desktop');
  shell(
    installLauncherScript(configuration, recoveryHome, recoveryApp, 'app', false, incompleteStock),
    false,
    { HOME: recoveryHome }
  );
  check(
    (await readFile(recoveryRegistry + '/owner', 'utf8')).includes('react-native-qnx/launcher-v1')
  );
  await copyFile(stock + '/desktop.conf', incompleteStock + '/desktop.conf');
  await writeFile(recoveryRegistry + '/incoming-' + configuration.appId + '.png', icon);
  shell(
    installLauncherScript(configuration, recoveryHome, recoveryApp, 'app', false, incompleteStock),
    true,
    { HOME: recoveryHome }
  );
  check(
    (await readFile(recoveryRegistry + '/base.conf', 'utf8')) ===
      (await readFile(stock + '/desktop.conf', 'utf8'))
  );
  check(configuration.appId === app.expo.extra.rnqnx.appId && configuration.icon.width === 1024);
  const dry = await prepareLauncher(project, true);
  check(dry.configuration.icon.sha256 === configuration.icon.sha256);
  await rejects(() => lstat(dry.output));
  await mkdir(project + '/build/qnx', { recursive: true });
  await writeFile(
    project + '/build/qnx/app.bundle.js',
    '/* host metadata fixture, not executed */'
  );
  await prepareNativeMain(['--project', project]);
  check((await readFile(dry.output + '/icon.png')).equals(icon));
  await prepareLauncher(project);
  check((await readFile(dry.output + '/icon.png')).equals(icon));
  await writeFile(dry.output + '/launcher.json.new', 'preserve old metadata staging');
  await writeFile(dry.output + '/icon.png.new', 'preserve old icon staging');
  await prepareLauncher(project);
  check((await readFile(dry.output + '/icon.png')).equals(icon));
  check((await readFile(dry.output + '/icon.png.new', 'utf8')) === 'preserve old icon staging');
  await rm(dry.output + '/icon.png');
  await mkdir(dry.output + '/icon.png');
  await rejects(() => prepareLauncher(project));
  check(!(await readdir(dry.output)).some((name) => name.startsWith('.rnqnx-stage-')));
  check((await lstat(dry.output + '/icon.png')).isDirectory());
  await rm(dry.output + '/icon.png', { recursive: true });
  await prepareLauncher(project);
  check((await readFile(dry.output + '/icon.png')).equals(icon));
  await prepareLauncher(project);
  const files = launcherFiles(configuration, deployed, registry, 'app');
  check(
    files['app.desktop'].includes('Name=Field Notes\n') &&
      files['app.desktop'].includes('/apps/org.example.notes/icon.png')
  );
  check(
    files['launch.sh'].includes('QT_QPA_PLATFORM=qnx') &&
      files['launch.sh'].includes('must not run as root')
  );
  check(!files['launch.sh'].includes('wayland'));
  check(files['launch.sh'].endsWith("/app' --fullscreen\n"));
  check(
    launcherFiles(
      { ...configuration, runtime: { ...configuration.runtime, launchArguments: [] } },
      deployed,
      registry,
      'app'
    )['launch.sh'] === files['launch.sh']
  );
  const literalArguments = [
    '--desktop',
    '--invert-wheel',
    'a value with spaces',
    "an apostrophe's value",
    '$(touch injected)',
    '; touch injected',
    '',
    'QNX — fullscreen',
  ];
  const configured = {
    ...configuration,
    runtime: { ...configuration.runtime, launchArguments: literalArguments },
  };
  const argumentFiles = launcherFiles(configured, deployed, registry, 'app');
  await writeFile(deployed + '/app', '#!/bin/sh\nprintf "%s\\n" "$@" > received-arguments\n');
  shell(argumentFiles['launch.sh']);
  assert.deepEqual((await readFile(deployed + '/received-arguments', 'utf8')).split('\n'), [
    '--fullscreen',
    ...literalArguments,
    '',
  ]);
  checks++;
  await rejects(() => lstat(deployed + '/injected'));
  await writeFile(deployed + '/app', '#!/bin/sh\nexit 0\n');
  await launcherMain([
    'install',
    '--project',
    project,
    '--app-dir',
    deployed,
    '--executable',
    'app',
    '--dry-run',
  ]);
  await rejects(() => lstat(registry));
  shell(installLauncherScript(configuration, home, deployed, 'app', false, stock), false);
  await writeFile(registry + '/incoming-' + configuration.appId + '.png', icon);
  shell(installLauncherScript(configuration, home, deployed, 'app', false, stock));
  const desktop = registry + '/' + configuration.appId + '.desktop';
  check((await readFile(desktop, 'utf8')).includes('Name=Field Notes\n'));
  check((await readFile(registry + '/desktop.conf', 'utf8')).includes('Order=7\n'));
  check(
    (await readFile(registry + '/stock.desktop', 'utf8')) ===
      (await readFile(stock + '/stock.desktop', 'utf8'))
  );
  const unchanged = await readFile(desktop);
  await writeFile(registry + '/incoming-' + configuration.appId + '.png', 'corrupt icon');
  shell(installLauncherScript(configuration, home, deployed, 'app', false, stock), false);
  check((await readFile(desktop)).equals(unchanged));
  const updated = {
    ...configuration,
    displayName: 'Renamed Notes',
    runtime: { ...configuration.runtime, launchArguments: ['--desktop'] },
  };
  const hashBin = temporary + '/openssl-only';
  await mkdir(hashBin);
  for (const command of [
    'id',
    'mkdir',
    'rmdir',
    'ls',
    'cat',
    'cp',
    'mv',
    'chmod',
    'grep',
    'awk',
    'openssl',
  ]) {
    const location = spawnSync('which', [command], { encoding: 'utf8' });
    assert.equal(location.status, 0);
    await symlink(location.stdout.trim(), hashBin + '/' + command);
  }
  await writeFile(registry + '/incoming-' + configuration.appId + '.png', icon);
  shell(installLauncherScript(updated, home, deployed, 'app', false, stock), true, {
    PATH: hashBin,
  });
  check((await readFile(desktop, 'utf8')).includes('Name=Renamed Notes\n'));
  check(
    (await readFile(registry + '/apps/' + configuration.appId + '/launch.sh', 'utf8')).endsWith(
      "/app' --fullscreen '--desktop'\n"
    )
  );
  check(
    (await readFile(registry + '/desktop.conf', 'utf8')).split(
      'DesktopFileName=org.example.notes.desktop'
    ).length === 2
  );
  const second = { ...configuration, appId: 'org.example.tasks' };
  await writeFile(registry + '/incoming-' + second.appId + '.png', icon);
  shell(installLauncherScript(second, home, deployed, 'app', false, stock));
  const combined = await readFile(registry + '/desktop.conf', 'utf8');
  check(combined.includes('Order=7\n') && combined.includes('Order=8\n'));
  shell(removeLauncherScript(configuration.appId, home, false, [], stock));
  await rejects(() => lstat(desktop));
  check(
    (await readFile(registry + '/org.example.tasks.desktop', 'utf8')).includes('Name=Field Notes')
  );
  check((await readFile(deployed + '/keep-user-data', 'utf8')) === 'not owned by launcher');
  shell(removeLauncherScript(configuration.appId, home, false, [], stock));
  const failureBin = temporary + '/interrupted-remove';
  await mkdir(failureBin);
  const realMove = spawnSync('which', ['mv'], { encoding: 'utf8' }).stdout.trim();
  await writeFile(
    failureBin + '/mv',
    `#!/bin/sh\ncase "$1" in */desktop.conf.new) exit 73;; esac\nexec '${realMove}' "$@"\n`
  );
  await chmod(failureBin + '/mv', 0o700);
  shell(removeLauncherScript(second.appId, home, false, [], stock), false, {
    PATH: failureBin + ':' + process.env.PATH,
  });
  await rejects(() => lstat(registry + '/apps/' + second.appId));
  check((await readFile(registry + '/desktop.conf', 'utf8')).includes(second.appId + '.desktop'));
  shell(removeLauncherScript(second.appId, home, false, [], stock));
  check(!(await readFile(registry + '/desktop.conf', 'utf8')).includes(second.appId + '.desktop'));
  const bin = temporary + '/bin';
  await mkdir(bin);
  await writeFile(bin + '/id', '#!/bin/sh\necho 0\n');
  await chmod(bin + '/id', 0o700);
  shell(installLauncherScript(configuration, home, deployed, 'app', false, stock), false, {
    PATH: bin + ':' + process.env.PATH,
  });
  await mkdir(registry + '/apps/org.example.foreign');
  await writeFile(registry + '/apps/org.example.foreign/user-file', 'preserve');
  shell(removeLauncherScript('org.example.foreign', home, false, [], stock), false);
  check((await readFile(registry + '/apps/org.example.foreign/user-file', 'utf8')) === 'preserve');
  await symlink(deployed + '/keep-user-data', registry + '/desktop.conf.new');
  shell(removeLauncherScript(second.appId, home, false, [], stock), false);
  check((await readFile(deployed + '/keep-user-data', 'utf8')) === 'not owned by launcher');
  await rm(registry + '/desktop.conf.new');
  for (const bad of [
    '/etc/passwd',
    '../outside.png',
    'assets/../../icon.png',
    'https://example.test/icon.png',
  ]) {
    app.expo.icon = bad;
    await writeFile(project + '/app.json', JSON.stringify(app));
    await rejects(() => launcherConfiguration(project));
  }
  app.expo.icon = './assets/icon.png';
  await writeFile(project + '/app.json', JSON.stringify(app));
  await rm(project + '/assets/icon.png');
  await symlink(temporary + '/secret.png', project + '/assets/icon.png');
  await rejects(() => launcherConfiguration(project));
  await rm(project + '/assets/icon.png');
  await writeFile(project + '/assets/icon.png', icon);
  const malformed = Buffer.from(icon);
  malformed[30] ^= 1;
  assert.throws(() => validateLauncherIcon(malformed));
  checks++;
  assert.throws(() => validateLauncherIcon(icon.subarray(0, 40)));
  checks++;
  for (const [width, height] of [
    [255, 255],
    [2049, 2049],
    [512, 1024],
  ]) {
    const altered = Buffer.from(icon);
    altered.writeUInt32BE(width, 16);
    altered.writeUInt32BE(height, 20);
    altered.writeUInt32BE(crc32(altered.subarray(12, 29)), 29);
    assert.throws(() => validateLauncherIcon(altered));
    checks++;
  }
  assert.throws(() => validateLauncherIcon(Buffer.alloc(8 * 1024 * 1024 + 1)));
  checks++;
  await writeFile(project + '/assets/override.png', icon);
  await writeFile(
    project + '/app.json',
    JSON.stringify({
      ...app,
      expo: {
        ...app.expo,
        icon: './missing.png',
        extra: {
          rnqnx: {
            ...app.expo.extra.rnqnx,
            qnx: {
              icon: './assets/override.png',
              runtime: {
                libraryPaths: ['/usr/lib'],
                qtPluginDirectory: '/opt/qt/plugins',
                qtFontDirectory: '/usr/share/fonts',
                launchArguments: ['--desktop'],
              },
            },
          },
        },
      },
    })
  );
  const override = await launcherConfiguration(project);
  check(
    override.configuration.icon.source === 'assets/override.png' &&
      override.configuration.runtime.qtPluginDirectory === '/opt/qt/plugins'
  );
  assert.deepEqual(override.configuration.runtime.launchArguments, ['--desktop']);
  checks++;
  for (const runtime of [
    { libraryPaths: '/usr/lib' },
    { libraryPaths: ['/usr/lib;bad'] },
    { libraryPaths: Array(17).fill('/usr/lib') },
    { launchArguments: '--desktop' },
    { launchArguments: [1] },
    { launchArguments: [null] },
    { launchArguments: ['line\nbreak'] },
    { launchArguments: ['nul\x00value'] },
    { launchArguments: ['\x7f'] },
    { launchArguments: ['a'.repeat(1025)] },
    { launchArguments: ['é'.repeat(513)] },
    { launchArguments: Array(17).fill('--desktop') },
    { unrecognized: true },
  ]) {
    await writeFile(
      project + '/app.json',
      JSON.stringify({
        ...app,
        expo: { ...app.expo, extra: { rnqnx: { ...app.expo.extra.rnqnx, qnx: { runtime } } } },
      })
    );
    await rejects(() => launcherConfiguration(project));
    const invalidConfiguration = JSON.parse(JSON.stringify({ ...configuration, runtime }));
    assert.throws(() => launcherFiles(invalidConfiguration, deployed, registry, 'app'));
    checks++;
  }
  await writeFile(project + '/app.json', JSON.stringify(app));
  const target = launcherTarget({
    QNX_TARGET_IP: 'device.example',
    QNX_TARGET_USER: 'developer',
    QNX_SSH_KEY: '/keys/device',
  });
  for (const required of [
    'IdentitiesOnly=yes',
    'BatchMode=yes',
    'StrictHostKeyChecking=yes',
    'MACs=hmac-sha2-256',
    'ProxyCommand=none',
  ])
    check(target.options.includes(required));
  for (const invalid of [
    { QNX_TARGET_USER: 'root' },
    { QNX_TARGET_IP: 'host;touch bad' },
    { QNX_SSH_PORT: '0' },
    { QNX_SSH_KEY: 'relative' },
  ]) {
    assert.throws(() =>
      launcherTarget({ QNX_TARGET_IP: 'device.example', QNX_SSH_KEY: '/keys/device', ...invalid })
    );
    checks++;
  }
  for (const args of [
    ['install', '--project', project],
    ['install', '--project', project, '--app-dir', deployed, '--executable', '../app', '--dry-run'],
    ['prepare', '--project', project, '--no-refresh'],
    ['prepare', '--project', project, '--project', project],
  ])
    await rejects(() => launcherMain(args));
  assert.throws(() => removeLauncherScript('../foreign', home, false));
  checks++;
  assert.throws(() => installLauncherScript(configuration, home, '/system/bin', 'app', false));
  checks++;
  await rejects(() => runLauncherTool(process.execPath, ['-e', 'process.exit(7)']));
  check(
    String(await runLauncherTool(process.execPath, ['-e', 'process.stdout.write("ok")'])) === 'ok'
  );
  const started = Date.now();
  await rejects(() =>
    runLauncherTool(
      process.execPath,
      ['-e', 'process.on("SIGTERM",()=>{});setInterval(()=>{},1000)'],
      '',
      150
    )
  );
  check(Date.now() - started < 2000);
  // Planning must validate the launcher icon before any cross-build starts.
  const planProject = temporary + '/plan-consumer';
  const planEnv = { QNX_HOST: process.env.QNX_HOST, QNX_TARGET: process.env.QNX_TARGET };
  process.env.QNX_HOST = '/fake/qnx/host';
  process.env.QNX_TARGET = '/fake/qnx/target';
  try {
    await mkdir(planProject + '/assets', { recursive: true });
    await mkdir(planProject + '/build/qnx', { recursive: true });
    await writeFile(planProject + '/build/qnx/app.hbc', 'hbc fixture, not executed');
    /** @param {Record<string, unknown>} extra */
    const writeApp = (extra) =>
      writeFile(planProject + '/app.json', JSON.stringify({ expo: { ...app.expo, ...extra } }));
    const prepare = () =>
      prepareNativeMain(['--project', planProject, '--bundle', 'build/qnx/app.hbc']);
    // Iconless consumers still plan successfully.
    await writeApp({ icon: undefined });
    await prepare();
    check((await planNativeBuild(planProject, 'qnx')).appId === 'org.example.notes');
    await rm(planProject + '/build/qnx/native-app.json');
    // A good icon plans successfully.
    await writeApp({});
    await copyFile(
      'packages/react-native-qnx-cli/assets/icon.png',
      planProject + '/assets/icon.png'
    );
    await prepare();
    check((await planNativeBuild(planProject, 'qnx')).appId === 'org.example.notes');
    // Each bad icon is rejected by planning with a launcher error, not a later build error.
    const good = await readFile(planProject + '/assets/icon.png');
    const withSize = (/** @type {number} */ width, /** @type {number} */ height) => {
      const altered = Buffer.from(good);
      altered.writeUInt32BE(width, 16);
      altered.writeUInt32BE(height, 20);
      altered.writeUInt32BE(crc32(altered.subarray(12, 29)), 29);
      return altered;
    };
    const crcBroken = Buffer.from(good);
    crcBroken[30] ^= 1;
    const iconCases = {
      'non-square': withSize(512, 1024),
      oversize: withSize(2049, 2049),
      'trailing bytes': Buffer.concat([good, Buffer.from('trailing')]),
      'bad crc': crcBroken,
    };
    for (const [label, bytes] of Object.entries(iconCases)) {
      await writeFile(planProject + '/assets/icon.png', bytes);
      await assert.rejects(
        () => planNativeBuild(planProject, 'qnx'),
        (/** @type {Error} */ error) => !/QNX_HOST|\.hbc/.test(error.message),
        label
      );
      checks++;
    }
    await rm(planProject + '/assets/icon.png');
    await assert.rejects(
      () => planNativeBuild(planProject, 'qnx'),
      (/** @type {NodeJS.ErrnoException} */ error) => error.code === 'ENOENT',
      'missing icon'
    );
    checks++;
    await symlink(temporary + '/secret.png', planProject + '/assets/icon.png');
    await assert.rejects(() => planNativeBuild(planProject, 'qnx'), /symlink/i, 'symlinked icon');
    checks++;
    // prepare-native must reject a bad icon before writing the descriptor.
    await rm(planProject + '/assets/icon.png');
    await rm(planProject + '/build/qnx/native-app.json');
    await writeFile(planProject + '/assets/icon.png', iconCases['bad crc']);
    await rejects(() => prepare());
    await rejects(() => lstat(planProject + '/build/qnx/native-app.json'));
  } finally {
    for (const [name, value] of Object.entries(planEnv))
      if (value === undefined) delete process.env[name];
      else process.env[name] = value;
  }
  console.log(
    `QNX launcher: ${checks} checks passed (host configuration/filesystem/SSH planning only; physical UI separate).`
  );
} finally {
  await rm(temporary, { recursive: true, force: true });
}
