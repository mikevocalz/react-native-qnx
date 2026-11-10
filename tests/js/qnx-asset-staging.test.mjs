import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { createHash } from 'node:crypto';
import { mkdir, mkdtemp, readFile, rm, symlink, writeFile, lstat } from 'node:fs/promises';
import { tmpdir } from 'node:os';
import path from 'node:path';
import {
  assetDestination,
  stageMetroAssets,
  verifyAssetInventory,
} from '../../packages/react-native-qnx-cli/src/asset-staging.mjs';
import { nativeConfiguration } from '../../packages/react-native-qnx-cli/src/prepare-native.mjs';

const root = process.cwd();
// 1x1 PNG.
const png = Buffer.from(
  'iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mP8z8DwHwAFBQIAX8jx0gAAAABJRU5ErkJggg==',
  'base64'
);
const sha = (/** @type {Buffer|string} */ data) => createHash('sha256').update(data).digest('hex');
const bundleSha = sha('bundle');

assert.equal(assetDestination('/assets/img', 'logo', 'png', 1), 'assets/img/logo.png');
assert.equal(assetDestination('/assets', 'logo', 'png', 2), 'assets/logo@2x.png');
assert.equal(assetDestination('/assets/a/b', 'logo', 'png', 1.5), 'assets/a/b/logo@1.5x.png');
for (const [location, name, type, scale] of [
  ['/assets/../x', 'logo', 'png', 1],
  ['/static/img', 'logo', 'png', 1],
  ['/assets/img', '../logo', 'png', 1],
  ['/assets/img', 'logo@2x', 'png', 1],
  ['/assets/img', 'logo', 'p/ng', 1],
  ['/assets/img', 'logo', 'png', 0],
  ['/assets/img', 'logo', 'png', Number.NaN],
]) {
  assert.throws(() => assetDestination(location, name, type, scale), Error, String(location));
}

const scratch = await mkdtemp(path.join(tmpdir(), 'rnqnx-assets-'));
try {
  const project = path.join(scratch, 'project');
  await mkdir(path.join(project, 'img'), { recursive: true });
  await writeFile(path.join(project, 'img/logo.png'), png);
  await writeFile(path.join(project, 'img/logo@2x.png'), Buffer.concat([png, Buffer.from([0])]));
  const logo = {
    httpServerLocation: '/assets/img',
    name: 'logo',
    type: 'png',
    scales: [1, 2],
    files: [path.join(project, 'img/logo.png'), path.join(project, 'img/logo@2x.png')],
  };
  const stage = async (/** @type {unknown[]} */ assets, name = 'out') => {
    const staging = path.join(scratch, name);
    await mkdir(staging);
    const inventory = await stageMetroAssets({
      project,
      staging,
      assets,
      platform: 'qnx',
      bundleSha256: bundleSha,
      metroVersion: '0.87.1',
    });
    const file = path.join(staging, 'app.bundle.assets.json');
    await writeFile(file, JSON.stringify(inventory));
    return { staging, inventory, file };
  };

  const good = await stage([logo]);
  assert.deepEqual(
    good.inventory.assets.map((/** @type {any} */ entry) => [entry.path, entry.scale]),
    [
      ['assets/img/logo.png', 1],
      ['assets/img/logo@2x.png', 2],
    ]
  );
  assert.equal(good.inventory.assets[0].sha256, sha(png));
  assert.equal(good.inventory.bundleSha256, bundleSha);
  assert.equal((await verifyAssetInventory(good.file)).assets.length, 2);

  const empty = await stage([], 'empty');
  assert.deepEqual(empty.inventory.assets, []);
  await verifyAssetInventory(empty.file);

  // Tampering with a staged file, removing it, or symlinking it is rejected.
  const staged = path.join(good.staging, 'assets/img/logo.png');
  await writeFile(staged, 'changed');
  await assert.rejects(verifyAssetInventory(good.file), /missing or changed/);
  await rm(staged);
  await assert.rejects(verifyAssetInventory(good.file));
  await symlink(path.join(project, 'img/logo.png'), staged);
  await assert.rejects(verifyAssetInventory(good.file), /symlink/);

  // Unexpected or unsafe inventory content is rejected.
  const unsafe = {
    ...empty.inventory,
    assets: [{ path: 'assets/../x.png', scale: 1, size: 1, sha256: sha('x') }],
  };
  await writeFile(empty.file, JSON.stringify(unsafe));
  await assert.rejects(verifyAssetInventory(empty.file), /Invalid asset inventory entry/);
  await writeFile(empty.file, JSON.stringify({ ...empty.inventory, extra: true }));
  await assert.rejects(verifyAssetInventory(empty.file), /unexpected fields/);

  // Source confinement, symlinks, collisions and malformed Metro records.
  const outside = path.join(scratch, 'outside.png');
  await writeFile(outside, png);
  await assert.rejects(
    stage([{ ...logo, scales: [1], files: [outside] }], 'outside'),
    /inside the project/
  );
  await symlink(path.join(project, 'img/logo.png'), path.join(project, 'img/link.png'));
  await assert.rejects(
    stage(
      [{ ...logo, name: 'link', scales: [1], files: [path.join(project, 'img/link.png')] }],
      'link'
    ),
    /regular file/
  );
  await writeFile(path.join(project, 'img/LOGO.png'), Buffer.from('other'));
  await assert.rejects(
    stage(
      [logo, { ...logo, name: 'LOGO', scales: [1], files: [path.join(project, 'img/LOGO.png')] }],
      'collide'
    ),
    /collide/
  );
  await assert.rejects(stage([{ ...logo, scales: [1] }], 'mismatch'), /mismatched/);
  await assert.rejects(stage([{ ...logo, files: ['img/logo.png', 'x'] }], 'relative'), /absolute/);
  await assert.rejects(stage(/** @type {any} */ ('x'), 'notarray'), /asset list/);
} finally {
  await rm(scratch, { recursive: true, force: true });
}

// Real installed Metro runBuild({ assets: true }) through the CLI, then the
// native descriptor binding. Bundle and staging only: no native run.
const project = await mkdtemp(path.join(tmpdir(), 'rnqnx-assets-metro-'));
try {
  await symlink(path.join(root, 'node_modules'), path.join(project, 'node_modules'), 'dir');
  await writeFile(path.join(project, 'package.json'), JSON.stringify({ name: 'asset-metro' }));
  await writeFile(
    path.join(project, 'app.json'),
    JSON.stringify({
      expo: {
        name: 'Asset Metro',
        extra: { rnqnx: { appId: 'dev.rnqnx.assetmetro', componentName: 'RNQnxPlayground' } },
      },
    })
  );
  await writeFile(
    path.join(project, 'babel.config.js'),
    "module.exports = { presets: ['babel-preset-expo'] };\n"
  );
  await mkdir(path.join(project, 'images/nested'), { recursive: true });
  await writeFile(path.join(project, 'images/nested/badge.png'), png);
  await writeFile(
    path.join(project, 'images/nested/badge@2x.png'),
    Buffer.concat([png, Buffer.from([1])])
  );
  await writeFile(
    path.join(project, 'index.js'),
    `const React = require('react');
const AppRegistry = require('react-native/Libraries/ReactNative/AppRegistry').AppRegistry;
const Image = require('react-native/Libraries/Image/Image').default;
const badge = require('./images/nested/badge.png');
AppRegistry.registerComponent('RNQnxPlayground', () => () => React.createElement(Image, { source: badge }));
`
  );
  const packageFile = path.join(root, 'packages/metro-config-qnx/src/index.js');
  await writeFile(
    path.join(project, 'metro.config.js'),
    `const path = require('node:path');
const { createRequire } = require('node:module');
const { getQnxMetroConfig } = require(${JSON.stringify(packageFile)});
const rnRequire = createRequire(require.resolve('react-native/package.json'));
module.exports = getQnxMetroConfig(__dirname, {
  watchFolders: [${JSON.stringify(root)}],
  resolver: {
    extraNodeModules: { '@babel/runtime': path.dirname(rnRequire.resolve('@babel/runtime/package.json')) },
    nodeModulesPaths: [${JSON.stringify(path.join(root, 'node_modules'))}],
  },
});
`
  );
  const cli = path.join(root, 'packages/react-native-qnx-cli/src/bin.mjs');
  const run = (/** @type {string[]} */ args) =>
    spawnSync(process.execPath, [cli, 'project', ...args], {
      cwd: project,
      encoding: 'utf8',
      timeout: 150000,
      maxBuffer: 4 * 1024 * 1024,
    });
  const bundled = run(['bundle', '--project', project, '--assets']);
  assert.equal(bundled.error, undefined);
  assert.equal(bundled.status, 0, bundled.stdout + bundled.stderr);
  const out = path.join(project, 'build/qnx');
  const inventoryFile = path.join(out, 'app.bundle.assets.json');
  const inventory = await verifyAssetInventory(inventoryFile);
  assert.equal(inventory.bundleSha256, sha(await readFile(path.join(out, 'app.bundle.js'))));
  assert.deepEqual(
    inventory.assets.map((/** @type {any} */ entry) => entry.path),
    ['assets/images/nested/badge.png', 'assets/images/nested/badge@2x.png']
  );
  assert.ok((await lstat(path.join(out, 'assets/images/nested/badge@2x.png'))).isFile());

  // Rerunning must not clobber the published set.
  const again = run([
    'bundle',
    '--project',
    project,
    '--assets',
    '--out',
    'build/qnx/other.bundle.js',
  ]);
  assert.notEqual(again.status, 0);
  assert.match(again.stderr + again.stdout, /Asset directory already exists/);

  const { configuration } = await nativeConfiguration(
    project,
    'qnx',
    'build/qnx/app.bundle.js',
    'build/qnx/app.bundle.assets.json'
  );
  assert.equal(configuration.assets, inventoryFile);
  assert.equal(configuration.assetsSha256, sha(await readFile(inventoryFile)));
  assert.equal(configuration.assetsBundleBinding, 'bundle-sha256');
  const { configuration: plain } = await nativeConfiguration(
    project,
    'qnx',
    'build/qnx/app.bundle.js'
  );
  assert.equal('assets' in plain, false, 'default descriptor shape is unchanged');

  // The consumer CMake stager copies exactly the inventory and rejects tampering.
  const cmake = process.env.CMAKE ?? 'cmake';
  const destination = path.join(project, 'native-out');
  const stageArgs = (/** @type {string} */ expected) => [
    `-DINVENTORY=${inventoryFile}`,
    `-DEXPECTED_SHA256=${expected}`,
    `-DBUNDLE_SHA256=${inventory.bundleSha256}`,
    '-DPLATFORM=qnx',
    '-DBINDING=bundle-sha256',
    `-DDESTINATION=${destination}`,
    '-P',
    path.join(root, 'cmake/StageConsumerAssets.cmake'),
  ];
  const staged = spawnSync(cmake, stageArgs(configuration.assetsSha256 ?? ''), {
    encoding: 'utf8',
  });
  assert.equal(staged.status, 0, `${staged.error ?? ''}${staged.stdout}${staged.stderr}`);
  assert.equal(
    sha(await readFile(path.join(destination, 'assets/images/nested/badge@2x.png'))),
    inventory.assets[1].sha256
  );
  const wrongInventory = spawnSync(cmake, stageArgs(sha('other')), { encoding: 'utf8' });
  assert.notEqual(wrongInventory.status, 0);
  assert.match(wrongInventory.stderr, /inventory changed/);
  await writeFile(path.join(out, 'assets/images/nested/badge.png'), 'tampered');
  const tampered = spawnSync(cmake, stageArgs(configuration.assetsSha256 ?? ''), {
    encoding: 'utf8',
  });
  assert.notEqual(tampered.status, 0);
  assert.match(tampered.stderr, /changed after bundling/);
  await writeFile(path.join(out, 'assets/images/nested/badge.png'), png);

  // Changing the bundle after asset staging breaks the binding.
  await writeFile(path.join(out, 'app.bundle.js'), 'changed');
  await assert.rejects(
    nativeConfiguration(
      project,
      'qnx',
      'build/qnx/app.bundle.js',
      'build/qnx/app.bundle.assets.json'
    ),
    /different bundle/
  );
  console.log(
    `Metro assets: ${inventory.assets.length} staged files bound to the bundle; no native run`
  );
} finally {
  await rm(project, { recursive: true, force: true });
}
