import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import {
  copyFile,
  chmod,
  lstat,
  mkdir,
  mkdtemp,
  readFile,
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
  const updated = { ...configuration, displayName: 'Renamed Notes' };
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
  for (const runtime of [
    { libraryPaths: '/usr/lib' },
    { libraryPaths: ['/usr/lib;bad'] },
    { libraryPaths: Array(17).fill('/usr/lib') },
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
  console.log(
    `QNX launcher: ${checks} checks passed (host configuration/filesystem/SSH planning only; physical UI separate).`
  );
} finally {
  await rm(temporary, { recursive: true, force: true });
}
