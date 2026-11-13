// Development-client tooling: the SSH launch command for `project dev`, the
// Metro /message channel of `project start`, and the resolver fallback for
// React Native modules that ship only .ios.js/.android.js files.
import assert from 'node:assert/strict';
import { mkdtemp, mkdir, writeFile, rm } from 'node:fs/promises';
import { createRequire } from 'node:module';
import os from 'node:os';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import {
  launchCommand,
  addressTowards,
} from '../../packages/react-native-qnx-cli/src/dev-server.mjs';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..', '..');
const require = createRequire(import.meta.url);

// --- launchCommand -----------------------------------------------------------
const args = launchCommand({
  device: 'qnxuser@192.168.2.2',
  launcher: '/data/home/qnxuser/run-chrome-app-dev.sh',
  url: 'http://192.168.2.1:8081',
  identity: '/keys/id_ed25519',
  knownHosts: '/keys/known_hosts',
});
assert.deepEqual(args, [
  '-o',
  'BatchMode=yes',
  '-o',
  'ConnectTimeout=10',
  '-i',
  '/keys/id_ed25519',
  '-o',
  'UserKnownHostsFile=/keys/known_hosts',
  '--',
  'qnxuser@192.168.2.2',
  'nohup /data/home/qnxuser/run-chrome-app-dev.sh --dev-server http://192.168.2.1:8081 </dev/null >/dev/null 2>&1 &',
]);
assert.ok(!args.some((arg) => /StrictHostKeyChecking/.test(arg)), 'host keys are never skipped');
for (const bad of [
  { device: 'board', launcher: '/a.sh', url: 'http://1.2.3.4:8081' },
  { device: 'u@h;rm -rf /', launcher: '/a.sh', url: 'http://1.2.3.4:8081' },
  { device: '-oProxyCommand@h', launcher: '/a.sh', url: 'http://1.2.3.4:8081' },
  { device: 'u@h', launcher: 'relative.sh', url: 'http://1.2.3.4:8081' },
  { device: 'u@h', launcher: '/a.sh; reboot', url: 'http://1.2.3.4:8081' },
  { device: 'u@h', launcher: '/a.sh', url: 'http://1.2.3.4:8081/$(id)' },
]) {
  assert.throws(() => launchCommand(bad), Error, JSON.stringify(bad));
}
assert.equal(await addressTowards('127.0.0.1'), '127.0.0.1');

// --- resolver fallback ---------------------------------------------------------
const { createQnxResolveRequest } = require('../../packages/metro-config-qnx/src/index.js');
const overrides = path.join(root, 'packages/react-native-qnx/Libraries');
const resolveRequest = createQnxResolveRequest(overrides);
const origin = path.join(root, 'node_modules/react-native/Libraries/Core/setUpReactDevTools.js');
const failing = {
  originModulePath: origin,
  resolveRequest() {
    throw new Error('Unable to resolve module');
  },
};
const fallback = resolveRequest(
  failing,
  '../../src/private/devsupport/rndevtools/ReactDevToolsSettingsManager',
  'qnx'
);
assert.deepEqual(fallback, {
  type: 'sourceFile',
  filePath: path.join(
    root,
    'packages/react-native-qnx/src/private/devsupport/rndevtools/ReactDevToolsSettingsManager.js'
  ),
});
assert.throws(
  () =>
    resolveRequest(
      failing,
      '../../src/private/devsupport/rndevtools/ReactDevToolsSettingsManager',
      'ios'
    ),
  /Unable to resolve/,
  'other platforms keep the original error'
);
assert.throws(
  () => resolveRequest(failing, '../../src/private/does/not/Exist', 'qnx'),
  /Unable to resolve/,
  'only files the platform package provides are substituted'
);
const settings = require('../../packages/react-native-qnx/src/private/devsupport/rndevtools/ReactDevToolsSettingsManager.js');
assert.equal(settings.getGlobalHookSettings(), null);
settings.setGlobalHookSettings('{"x":1}');
assert.equal(settings.getGlobalHookSettings(), '{"x":1}');

// --- project start: /message broadcasts and /status ---------------------------
// Needs the installed Metro; a checkout without dependencies skips this part.
let metroAvailable = true;
try {
  require.resolve('metro/package.json', { paths: [path.join(root, 'apps/component-gallery')] });
} catch {
  metroAvailable = false;
}
if (metroAvailable) {
  const { startDevServer } = await import('../../packages/react-native-qnx-cli/src/dev-server.mjs');
  const tmp = await mkdtemp(path.join(os.tmpdir(), 'rnqnx-dev-server-'));
  try {
    const app = path.join(tmp, 'app');
    await mkdir(app);
    await writeFile(path.join(app, 'package.json'), JSON.stringify({ name: 'dev-server-fixture' }));
    await writeFile(path.join(app, 'index.js'), 'globalThis.loaded = true;\n');
    const metroDir = path.dirname(
      require.resolve('metro/package.json', { paths: [path.join(root, 'apps/component-gallery')] })
    );
    await writeFile(
      path.join(app, 'metro.config.js'),
      `module.exports = { projectRoot: ${JSON.stringify(app)}, watchFolders: [${JSON.stringify(app)}], resolver: { useWatchman: false, nodeModulesPaths: [${JSON.stringify(path.dirname(path.dirname(metroDir)))}] } };\n`
    );
    await mkdir(path.join(app, 'node_modules'));
    const { symlink } = await import('node:fs/promises');
    await symlink(metroDir, path.join(app, 'node_modules', 'metro'));
    /** @type {string[]} */
    const lines = [];
    // A free port: let the OS pick one, then hand it to Metro.
    const net = await import('node:net');
    const port = await new Promise((resolve) => {
      const probe = net.createServer().listen(0, '127.0.0.1', () => {
        const { port: free } = /** @type {import('node:net').AddressInfo} */ (probe.address());
        probe.close(() => resolve(free));
      });
    });
    const server = await startDevServer({
      project: app,
      port,
      host: '127.0.0.1',
      log: (l) => lines.push(l),
    });
    try {
      const status = await (await fetch(`http://127.0.0.1:${port}/status`)).text();
      assert.equal(status, 'packager-status:running');
      const wsRequire = createRequire(path.join(metroDir, 'package.json'));
      const WebSocket = wsRequire('ws');
      const socket = new WebSocket(`ws://127.0.0.1:${port}/message`);
      await new Promise((resolve, reject) => {
        socket.once('open', resolve);
        socket.once('error', reject);
      });
      const message = new Promise((resolve) =>
        socket.once('message', (/** @type {unknown} */ data) => resolve(JSON.parse(String(data))))
      );
      for (let i = 0; i < 50 && server.clients.size === 0; ++i)
        await new Promise((r) => setTimeout(r, 20));
      server.reload();
      assert.deepEqual(await message, { version: 2, method: 'reload', params: null });
      const menu = new Promise((resolve) =>
        socket.once('message', (/** @type {unknown} */ data) => resolve(JSON.parse(String(data))))
      );
      server.devMenu();
      assert.equal((await menu).method, 'devMenu');
      socket.close();
      assert.ok(lines.some((l) => l.includes(`--dev-server http://127.0.0.1:${port}`)));
    } finally {
      await server.close();
    }
  } finally {
    await rm(tmp, { recursive: true, force: true });
  }
}

console.log(
  `dev-server launch command, resolver fallback${metroAvailable ? ', /message and /status' : ''} passed`
);
