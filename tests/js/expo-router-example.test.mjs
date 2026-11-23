import assert from 'node:assert/strict';
import { createHash } from 'node:crypto';
import {
  cp,
  lstat,
  mkdir,
  mkdtemp,
  readFile,
  readdir,
  rm,
  symlink,
  utimes,
  writeFile,
} from 'node:fs/promises';
import { tmpdir } from 'node:os';
import path from 'node:path';
import { prepareNativeMain } from '../../packages/react-native-qnx-cli/src/prepare-native.mjs';
import { useIdentityToolInEnvironment } from './helpers/identity-tool.mjs';

// apps/expo-router-example is the Router app of the `expo` gate. This test is
// host-only. It checks the app's shape (identity, entry, routes, workspace
// wiring) and that a second `project prepare-native` run on a copy of the app
// changes no file. It does not bundle (qnx-create-app.yml and host-renderer.yml
// do), and it shows nothing about a device. The identity tool is the JS
// stand-in unless RNQNX_IDENTITY_TOOL points at the real Qt tool.
await useIdentityToolInEnvironment();
const app = path.resolve('apps/expo-router-example');
let checks = 0;

/** @param {string} file */
async function json(file) {
  return JSON.parse(await readFile(path.join(app, file), 'utf8'));
}

// Identity and entry.
const config = (await json('app.json')).expo;
assert.equal(config.name, 'Router Example');
assert.equal(config.extra.rnqnx.appId, 'org.rnqnx.routerexample');
assert.equal(config.extra.rnqnx.componentName, 'RNQnxPlayground');
assert.deepEqual(config.extra.rnqnx.platforms, ['qnx']);
assert.equal(config.icon, './assets/icon.png');
checks += 5;
const index = await readFile(path.join(app, 'index.js'), 'utf8');
assert.match(index, /registerComponent\('RNQnxPlayground'/);
assert.match(index, /expo-router\/build\/qualified-entry/);
checks += 2;

// The routes, the context shim and the Metro wiring the host needs.
for (const route of [
  'app/_layout.js',
  'app/(tabs)/_layout.js',
  'app/(tabs)/index.js',
  'app/(tabs)/explore.js',
  'app/(tabs)/controls.js',
  'app/settings.js',
  'app/about.js',
  'app-ctx.js',
  'metro.config.js',
  'babel.config.js',
  '.gitkeep',
]) {
  assert.ok((await lstat(path.join(app, route))).isFile(), `${route} exists`);
  checks++;
}
assert.match(await readFile(path.join(app, 'app/_layout.js'), 'utf8'), /expo-router\/drawer/);
assert.match(await readFile(path.join(app, 'metro.config.js'), 'utf8'), /withExpoRouterQnx/);
checks += 2;

// Workspace wiring: a package of its own, linked to the platform router
// package, with the router dependency versions the project creator pins.
const manifest = await json('package.json');
assert.equal(manifest.name, '@rn-qnx/expo-router-example');
assert.equal(manifest.private, true);
assert.equal(manifest.dependencies['@rn-qnx/expo-router-qnx'], 'workspace:*');
const { CONSUMER_CONTRACT } =
  await import('../../packages/react-native-qnx-cli/src/consumer-contract.mjs');
for (const [name, version] of Object.entries(CONSUMER_CONTRACT.routerPackages)) {
  if (name in manifest.dependencies && name !== '@rn-qnx/expo-router-qnx') {
    assert.equal(manifest.dependencies[name], version, `${name} matches the contract`);
    checks++;
  }
}
const lock = await readFile('pnpm-lock.yaml', 'utf8');
assert.ok(lock.includes('\n  apps/expo-router-example:\n'), 'pnpm-lock.yaml has the importer');
checks += 4;

// Second prepare-native run changes nothing, on a copy outside the checkout.
const temporary = await mkdtemp(path.join(tmpdir(), 'rnqnx-router-example-'));
try {
  const project = path.join(temporary, 'router-example');
  await cp(app, project, {
    recursive: true,
    filter: (source) => path.basename(source) !== 'node_modules',
  });
  await mkdir(path.join(project, 'node_modules/@rn-qnx'), { recursive: true });
  await symlink(
    path.resolve('packages/expo-qnx-config-plugins'),
    path.join(project, 'node_modules/@rn-qnx/expo-qnx-config-plugins')
  );
  await mkdir(path.join(project, 'build/qnx'), { recursive: true });
  await writeFile(path.join(project, 'build/qnx/app.bundle.js'), 'bundle\n');

  /** @param {string} root @returns {Promise<Record<string, string>>} */
  async function snapshot(root) {
    /** @type {Record<string, string>} */
    const files = {};
    const walk = async (/** @type {string} */ dir) => {
      for (const entry of await readdir(dir, { withFileTypes: true })) {
        if (entry.name === 'node_modules') continue;
        const full = path.join(dir, entry.name);
        const stat = await lstat(full);
        if (entry.isDirectory()) {
          files[path.relative(root, full) + '/'] = `mode=${stat.mode}`;
          await walk(full);
          continue;
        }
        const digest = createHash('sha256')
          .update(await readFile(full))
          .digest('hex');
        files[path.relative(root, full)] =
          `${digest} mode=${stat.mode} ino=${stat.ino} mtime=${stat.mtimeMs}`;
      }
    };
    await walk(root);
    return files;
  }
  /** @param {string[]} args */
  async function prepare(args) {
    const log = console.log;
    let out = '';
    console.log = (/** @type {unknown[]} */ ...parts) => {
      out += parts.join(' ') + '\n';
    };
    try {
      await prepareNativeMain(args);
    } finally {
      console.log = log;
    }
    return JSON.parse(out);
  }
  const args = ['--project', project, '--platform', 'qnx'];
  assert.equal((await prepare(args)).action, 'created');
  const old = new Date('2001-01-01T00:00:00Z');
  const pin = async (/** @type {string} */ dir) => {
    for (const entry of await readdir(dir, { withFileTypes: true })) {
      const full = path.join(dir, entry.name);
      if (entry.isDirectory()) await pin(full);
      else await utimes(full, old, old);
    }
  };
  await pin(path.join(project, 'build'));
  const first = await snapshot(project);
  assert.ok('build/qnx/native-app.json' in first, 'descriptor written');
  assert.ok(
    Object.keys(first).some((file) => file.includes('/icons/icon-')),
    'icons written'
  );
  const descriptor = JSON.parse(
    await readFile(path.join(project, 'build/qnx/native-app.json'), 'utf8')
  );
  assert.equal(descriptor.displayName, 'Router Example');
  assert.equal(descriptor.appId, 'org.rnqnx.routerexample');
  assert.equal(descriptor.componentName, 'RNQnxPlayground');
  assert.equal((await prepare(args)).action, 'unchanged');
  assert.deepEqual(await snapshot(project), first, 'second run changed a file');
  checks += 8;
} finally {
  await rm(temporary, { recursive: true, force: true });
}

console.log(
  `expo-router-example: ${checks} checks passed (host; stand-in identity tool unless RNQNX_IDENTITY_TOOL is set).`
);
