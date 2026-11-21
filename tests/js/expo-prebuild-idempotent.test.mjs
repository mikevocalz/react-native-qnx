import assert from 'node:assert/strict';
import { createHash } from 'node:crypto';
import {
  copyFile,
  lstat,
  mkdir,
  mkdtemp,
  readFile,
  readdir,
  utimes,
  rm,
  symlink,
  writeFile,
} from 'node:fs/promises';
import { tmpdir } from 'node:os';
import path from 'node:path';
import { addQnx } from '../../packages/react-native-qnx-cli/src/add-qnx.mjs';
import { createProjectMain } from '../../packages/react-native-qnx-cli/src/create-project.mjs';
import { prepareNativeMain } from '../../packages/react-native-qnx-cli/src/prepare-native.mjs';
import { useIdentityToolInEnvironment } from './helpers/identity-tool.mjs';

// "Prebuild" for this platform is `project prepare-native`: it resolves the Expo
// config (config plugins included) and writes build/<platform>/native-app.json
// and the icon set. A second run must change nothing, which here means every
// file in the project keeps its bytes, its inode and its mtime. Comparing bytes
// alone would miss a run that rewrites identical content.
//
// Host-only. The identity tool is the stand-in unless RNQNX_IDENTITY_TOOL is set,
// and node_modules holds a symlink to the plugin package instead of a pnpm
// install; qnx-create-app.yml does the real install and the real tool.
await useIdentityToolInEnvironment();
process.env.npm_config_cache ||= path.join(tmpdir(), 'rnqnx-prebuild-npm-cache');
const temporary = await mkdtemp(path.join(tmpdir(), 'rnqnx-prebuild-idempotent-'));
const templateIcon = path.resolve('template/qnx/assets/icon.png');
let checks = 0;

/** @param {() => Promise<unknown>} run @returns {Promise<string>} */
async function captured(run) {
  const log = console.log;
  let out = '';
  console.log = (/** @type {unknown[]} */ ...parts) => {
    out += parts.join(' ') + '\n';
  };
  try {
    await run();
  } finally {
    console.log = log;
  }
  return out;
}

/** Every file and directory outside node_modules: relative path, mode, and for
 * files bytes, inode and mtime.
 * @param {string} root @returns {Promise<Record<string, string>>} */
async function snapshot(root) {
  /** @type {Record<string, string>} */
  const files = {};
  const walk = async (/** @type {string} */ dir) => {
    for (const entry of await readdir(dir, { withFileTypes: true })) {
      if (entry.name === 'node_modules') continue;
      const full = path.join(dir, entry.name);
      if (entry.isDirectory()) {
        files[path.relative(root, full) + '/'] = `mode=${(await lstat(full)).mode}`;
        await walk(full);
        continue;
      }
      const stat = await lstat(full);
      const digest = createHash('sha256')
        .update(entry.isSymbolicLink() ? '' : await readFile(full))
        .digest('hex');
      files[path.relative(root, full)] =
        `${digest} mode=${stat.mode} ino=${stat.ino} mtime=${stat.mtimeMs}`;
    }
  };
  await walk(root);
  return files;
}

/** @param {string} project */
async function linkPlugin(project) {
  await mkdir(path.join(project, 'node_modules/@rn-qnx'), { recursive: true });
  await symlink(
    path.resolve('packages/expo-qnx-config-plugins'),
    path.join(project, 'node_modules/@rn-qnx/expo-qnx-config-plugins')
  );
}

/** Prepare twice and assert the second run changed nothing.
 * @param {string} label @param {string} project @param {string[]} platforms */
async function assertSecondRunChangesNothing(label, project, platforms) {
  for (const platform of platforms) {
    await mkdir(path.join(project, 'build', platform), { recursive: true });
    await writeFile(path.join(project, 'build', platform, 'app.bundle.js'), 'bundle\n');
  }
  for (const platform of platforms) {
    const args = ['--project', project, '--platform', platform];
    const first = JSON.parse(await captured(() => prepareNativeMain(args)));
    assert.equal(first.action, 'created', `${label} ${platform} first run creates`);
    checks++;
  }
  // Pin every generated file to an old mtime. A second run that rewrites a file
  // with identical bytes then shows up as a new mtime however coarse the
  // filesystem's timestamp resolution is.
  const old = new Date('2001-01-01T00:00:00Z');
  const pin = async (/** @type {string} */ dir) => {
    for (const entry of await readdir(dir, { withFileTypes: true })) {
      const full = path.join(dir, entry.name);
      if (entry.isDirectory()) await pin(full);
      else await utimes(full, old, old);
    }
  };
  await pin(path.join(project, 'build'));
  const afterFirst = await snapshot(project);
  const generated = Object.keys(afterFirst).filter((file) => file.startsWith('build/'));
  assert.ok(
    platforms.every((p) => generated.includes(`build/${p}/native-app.json`)) &&
      generated.some((file) => file.includes('/icons/icon-')),
    `${label}: first run wrote the descriptor and the icon set`
  );
  checks++;
  for (const platform of platforms) {
    const second = JSON.parse(
      await captured(() => prepareNativeMain(['--project', project, '--platform', platform]))
    );
    assert.equal(second.action, 'unchanged', `${label} ${platform} second run reports unchanged`);
    checks++;
  }
  assert.deepEqual(await snapshot(project), afterFirst, `${label}: second run changed a file`);
  checks++;
}

try {
  // One-command create, qnx only and dual target.
  /** @type {Array<[string, string[]]>} */
  const creations = [
    ['created-qnx', []],
    ['created-dual', ['--with-marathon']],
  ];
  for (const [name, extra] of creations) {
    const project = path.join(temporary, name);
    await captured(() =>
      createProjectMain([
        '--project',
        project,
        '--app-id',
        `org.example.${name.replace('-', '')}`,
        '--name',
        'Notes',
        ...extra,
      ])
    );
    await linkPlugin(project);
    await assertSecondRunChangesNothing(
      name,
      project,
      extra.length ? ['qnx', 'marathon'] : ['qnx']
    );
  }

  // Add to an existing Expo app, then prepare twice.
  const existing = path.join(temporary, 'existing');
  await mkdir(path.join(existing, 'assets'), { recursive: true });
  await mkdir(path.join(existing, 'ios/ExistingApp'), { recursive: true });
  await writeFile(path.join(existing, 'ios/ExistingApp/AppDelegate.swift'), 'import Expo\n');
  await copyFile(templateIcon, path.join(existing, 'assets/icon.png'));
  await writeFile(
    path.join(existing, 'package.json'),
    JSON.stringify({
      name: 'existing-app',
      version: '1.0.0',
      main: 'expo-router/entry',
      dependencies: { expo: '58.0.22', react: '19.3.0', 'react-native': '0.88.1' },
    })
  );
  await writeFile(
    path.join(existing, 'app.json'),
    JSON.stringify({
      expo: { name: 'Existing App', slug: 'existing-app', icon: './assets/icon.png' },
    })
  );
  await addQnx({ project: existing, appId: 'org.example.existing' });
  await linkPlugin(existing);
  await assertSecondRunChangesNothing('add-to-existing', existing, ['qnx']);

  console.log(
    `expo-prebuild-idempotent: ${checks} checks passed (host; stand-in identity tool unless RNQNX_IDENTITY_TOOL is set).`
  );
} finally {
  await rm(temporary, { recursive: true, force: true });
}
