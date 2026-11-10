import assert from 'node:assert/strict';
import { spawn, spawnSync } from 'node:child_process';
import { once } from 'node:events';
import { mkdtemp, readFile, rm, symlink, writeFile } from 'node:fs/promises';
import { createServer } from 'node:net';
import { tmpdir } from 'node:os';
import path from 'node:path';
import { setTimeout as delay } from 'node:timers/promises';

const root = process.cwd();
const cli = path.join(root, 'packages/react-native-qnx-cli/src/bin.mjs');
const project = await mkdtemp(path.join(tmpdir(), 'rnqnx-project-start-metro-'));

async function availablePort() {
  const server = createServer();
  server.listen(0, '127.0.0.1');
  await once(server, 'listening');
  const address = server.address();
  assert.ok(address && typeof address !== 'string');
  const port = address.port;
  await new Promise((resolve, reject) =>
    server.close((error) => (error ? reject(error) : resolve(undefined)))
  );
  return port;
}

/** @param {string} url @param {(status: number, body: string) => boolean} accepts
 * @param {() => string} diagnostics */
async function waitForResponse(url, accepts, diagnostics) {
  const deadline = Date.now() + 90000;
  let last = '';
  while (Date.now() < deadline) {
    try {
      const response = await fetch(url, { signal: AbortSignal.timeout(30000) });
      const body = await response.text();
      if (accepts(response.status, body)) return body;
      last = `HTTP ${response.status}: ${body.slice(0, 500)}`;
    } catch (error) {
      last = String(error);
    }
    await delay(250);
  }
  throw new Error(`Metro did not reach the expected response: ${last}\n${diagnostics()}`);
}

try {
  // Same installed dependency graph, independent source/config directory. This
  // proves development HTTP transport, not published packages or native refresh.
  await symlink(path.join(root, 'node_modules'), path.join(project, 'node_modules'), 'dir');
  await writeFile(
    path.join(project, 'package.json'),
    JSON.stringify({ name: 'external-dev-loop' })
  );
  await writeFile(
    path.join(project, 'babel.config.js'),
    "module.exports = { presets: ['babel-preset-expo'] };\n"
  );
  const entry = path.join(project, 'index.js');
  const source = (/** @type {string} */ marker) => `import 'react-native/setup-env';
import { AppRegistry, Text } from 'react-native';
import React from 'react';
import message from './message';
function App() { return React.createElement(Text, null, '${marker} ' + message); }
AppRegistry.registerComponent('DevLoop', () => App);\n`;
  await writeFile(entry, source('FIRST_DEV_BUILD'));
  await writeFile(path.join(project, 'message.qnx.js'), "export default 'QNX_DEV_PLATFORM';\n");
  await writeFile(
    path.join(project, 'message.marathon.js'),
    "export default 'MARATHON_DEV_PLATFORM';\n"
  );
  for (const platform of ['qnx', 'marathon']) {
    const factory = platform === 'qnx' ? 'getQnxMetroConfig' : 'getMarathonMetroConfig';
    const config = platform === 'qnx' ? 'metro.config.js' : 'metro.marathon.config.js';
    await writeFile(
      path.join(project, config),
      `const path = require('node:path');
const { createRequire } = require('node:module');
const { ${factory} } = require(${JSON.stringify(path.join(root, `packages/metro-config-${platform}/src/index.js`))});
const withPublicEntry = require(${JSON.stringify(path.join(root, 'packages/metro-config-qnx/public-entry.js'))});
const rn = createRequire(require.resolve('react-native/package.json'));
module.exports = withPublicEntry(${factory}(__dirname, {
  watchFolders: [${JSON.stringify(root)}],
  resolver: {
    extraNodeModules: { '@babel/runtime': path.dirname(rn.resolve('@babel/runtime/package.json')) },
    nodeModulesPaths: [${JSON.stringify(path.join(root, 'node_modules'))}],
  },
}), '${platform}');\n`
    );
  }
  for (const platform of ['qnx', 'marathon']) {
    await writeFile(entry, source('FIRST_DEV_BUILD'));
    const port = await availablePort();
    const args = [
      cli,
      'project',
      'start',
      '--project',
      project,
      '--platform',
      platform,
      '--port',
      String(port),
    ];
    const child = spawn(process.execPath, args, { stdio: ['ignore', 'pipe', 'pipe'] });
    const closed = once(child, 'close');
    let logs = '';
    const log = (/** @type {Buffer} */ chunk) => {
      logs = (logs + chunk.toString()).slice(-100000);
    };
    child.stdout.on('data', log);
    child.stderr.on('data', log);
    // Finite watchdog for this test, not a development-server lifetime limit.
    const watchdog = setTimeout(() => child.kill('SIGTERM'), 240000);
    try {
      const base = `http://127.0.0.1:${port}/index.js`;
      const query = `?platform=${platform}&dev=true&minify=false`;
      const bundleUrl = base + '.bundle' + query;
      const bundle = await waitForResponse(
        bundleUrl,
        (status, body) => status === 200 && body.includes('FIRST_DEV_BUILD'),
        () => logs
      );
      assert.ok(bundle.includes(platform === 'qnx' ? 'QNX_DEV_PLATFORM' : 'MARATHON_DEV_PLATFORM'));
      const map = JSON.parse(
        await waitForResponse(
          base + '.map' + query,
          (status) => status === 200,
          () => logs
        )
      );
      assert.equal(map.version, 3);
      // Metro can return an indexed source map with per-module sections.
      const maps = map.sections
        ? map.sections.map((/** @type {any} */ section) => section.map)
        : [map];
      assert.ok(
        maps
          .flatMap((/** @type {any} */ part) => part.sources ?? [])
          .some((/** @type {string} */ source) => source.endsWith('index.js'))
      );
      assert.ok(
        maps
          .flatMap((/** @type {any} */ part) => part.sourcesContent ?? [])
          .some((/** @type {string | null} */ content) => content?.includes('FIRST_DEV_BUILD'))
      );

      await writeFile(entry, 'export const broken = ;\n');
      await waitForResponse(
        bundleUrl,
        (status, body) => status === 500 && /SyntaxError|TransformError/.test(body),
        () => logs
      );
      await writeFile(entry, source('RECOVERED_DEV_BUILD'));
      await waitForResponse(
        bundleUrl,
        (status, body) => status === 200 && body.includes('RECOVERED_DEV_BUILD'),
        () => logs
      );
      const occupied = spawnSync(process.execPath, args, {
        encoding: 'utf8',
        timeout: 15000,
        maxBuffer: 1024 * 1024,
      });
      assert.equal(occupied.error, undefined);
      assert.notEqual(
        occupied.status,
        0,
        'occupied Metro port must fail instead of reporting ready'
      );
      assert.match(occupied.stdout + occupied.stderr, /EADDRINUSE|already in use/i);
      child.kill('SIGTERM');
      const [code] = await closed;
      assert.equal(code, 143);
      await assert.rejects(fetch(bundleUrl, { signal: AbortSignal.timeout(1000) }));
      await assert.rejects(
        readFile(path.join(project, `build/${platform}/app.bundle.js`)),
        /ENOENT/
      );
      console.log(
        `External ${platform} Metro dev server: real bundle, source map, syntax-error recovery, port collision and shutdown passed; native Fast Refresh unverified.`
      );
    } finally {
      clearTimeout(watchdog);
      if (child.exitCode === null) child.kill('SIGTERM');
      await closed;
    }
  }
} finally {
  await rm(project, { recursive: true, force: true });
}
