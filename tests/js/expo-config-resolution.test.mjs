import assert from 'node:assert/strict';
import { createHash } from 'node:crypto';
import { mkdir, mkdtemp, readFile, readdir, rm, symlink, writeFile } from 'node:fs/promises';
import { createRequire } from 'node:module';
import { tmpdir } from 'node:os';
import path from 'node:path';
import {
  nativeConfiguration,
  prepareNativeMain,
} from '../../packages/react-native-qnx-cli/src/prepare-native.mjs';
import { loadExpoConfig } from '../../packages/react-native-qnx-cli/src/expo-config.mjs';

const require = createRequire(import.meta.url);
const withQnxProject = require('../../packages/expo-qnx-config-plugins/index.js');
const { resolveQnxWindow } = withQnxProject;
const pluginRoot = path.resolve('packages/expo-qnx-config-plugins');
// Fixture apps live outside the checkout so nothing resolves through the workspace.
const temporary = await mkdtemp(path.join(tmpdir(), 'rnqnx-expo-config-'));
let checks = 0;
const ok = (/** @type {unknown} */ value, /** @type {string} */ message) => {
  assert.ok(value, message);
  checks++;
};
const quiet = async (/** @type {() => Promise<unknown>} */ run) => {
  const log = console.log;
  console.log = () => {};
  try {
    return await run();
  } finally {
    console.log = log;
  }
};

/** Hash every file under build/ so a rerun can be compared byte for byte. */
async function treeHash(/** @type {string} */ directory) {
  const hash = createHash('sha256');
  const walk = async (/** @type {string} */ dir) => {
    for (const entry of (await readdir(dir, { withFileTypes: true })).sort((a, b) =>
      a.name.localeCompare(b.name)
    )) {
      const full = path.join(dir, entry.name);
      if (entry.isDirectory()) await walk(full);
      else hash.update(path.relative(directory, full) + '\0' + (await readFile(full)) + '\0');
    }
  };
  await walk(directory);
  return hash.digest('hex');
}

const pluginEntry = [
  '@rn-qnx/expo-qnx-config-plugins',
  {
    appId: 'org.example.fieldnotes',
    componentName: 'FieldNotes',
    shortName: 'Notes',
    window: {
      controls: 'windows',
      titleBarStyle: 'hidden',
      initialSize: { width: 1024, height: 600 },
    },
  },
];
const fixtures = {
  'app.json': JSON.stringify({
    expo: { name: 'Field Notes', slug: 'field-notes', plugins: [pluginEntry] },
  }),
  'app.config.js': `module.exports = ({ config }) => ({
  ...config,
  name: 'Field Notes ' + 'JS',
  slug: 'field-notes',
  plugins: [${JSON.stringify(pluginEntry)}],
});
`,
  'app.config.ts': `import type { ExpoConfig, ConfigContext } from 'expo/config';
const plugin: [string, Record<string, unknown>] = ${JSON.stringify(pluginEntry)};
export default ({ config }: ConfigContext): ExpoConfig => ({
  ...config,
  name: 'Field Notes TS',
  slug: 'field-notes',
  plugins: [plugin],
});
`,
};

try {
  // Plugin unit behaviour: defaults, option precedence, validation.
  assert.deepEqual(resolveQnxWindow(undefined), { controls: 'macos', titleBarStyle: 'default' });
  checks++;
  const merged = withQnxProject(
    { name: 'A', extra: { rnqnx: { appId: 'org.a.b', window: { controls: 'windows' } } } },
    { componentName: 'A', window: { titleBarStyle: 'none' } }
  );
  assert.deepEqual(merged.extra.rnqnx, {
    appId: 'org.a.b',
    componentName: 'A',
    window: { controls: 'windows', titleBarStyle: 'none' },
  });
  checks++;
  ok(!('rnqnx' in withQnxProject({}, {}).extra), 'no rnqnx block without QNX options');
  for (const bad of [
    { controls: 'linux' },
    { titleBarStyle: 'transparent' },
    { initialSize: { width: 0, height: 10 } },
    { initialSize: { width: 10.5, height: 10 } },
    { initialSize: { width: 10, height: 16385 } },
    { initialSize: { width: 10, height: 10, depth: 1 } },
    { initialSize: [10, 10] },
    { position: 'center' },
    'macos',
  ]) {
    assert.throws(() => resolveQnxWindow(bad), /extra\.rnqnx\.window/);
    checks++;
  }
  assert.throws(() => withQnxProject({}, /** @type {any} */ ({ window: { controls: 'gnome' } })));
  checks++;

  for (const [file, contents] of Object.entries(fixtures)) {
    const project = path.join(temporary, file.replaceAll('.', '-'));
    await mkdir(path.join(project, 'node_modules', '@rn-qnx'), { recursive: true });
    await symlink(pluginRoot, path.join(project, 'node_modules/@rn-qnx/expo-qnx-config-plugins'));
    await writeFile(path.join(project, 'package.json'), '{"name":"field-notes","version":"1.0.0"}');
    await writeFile(path.join(project, file), contents);
    await mkdir(path.join(project, 'build/qnx'), { recursive: true });
    await writeFile(path.join(project, 'build/qnx/app.bundle.js'), 'bundle\n');

    const loaded = await loadExpoConfig(project);
    ok(loaded.files.includes(file), `${file} detected`);
    ok(
      (file === 'app.json' ? loaded.staticConfigPath : loaded.dynamicConfigPath)?.endsWith(file),
      `${file} resolved by @expo/config`
    );
    const { configuration } = await nativeConfiguration(project, 'qnx');
    ok(configuration.appId === 'org.example.fieldnotes', `${file} appId from plugin`);
    ok(configuration.componentName === 'FieldNotes', `${file} componentName from plugin`);
    ok(configuration.displayName.startsWith('Field Notes'), `${file} name`);
    assert.deepEqual(configuration.window, {
      controls: 'windows',
      titleBarStyle: 'hidden',
      initialSize: { width: 1024, height: 600 },
    });
    checks++;

    // Prebuild twice: the second run must leave build/ byte-identical.
    const args = ['--project', project, '--platform', 'qnx'];
    await quiet(() => prepareNativeMain(args));
    const first = await treeHash(path.join(project, 'build'));
    await quiet(() => prepareNativeMain(args));
    ok(
      (await treeHash(path.join(project, 'build'))) === first,
      `${file} second run changes nothing`
    );

    // A config change is never written over generated output silently.
    if (file === 'app.json') {
      const changed = JSON.parse(contents);
      changed.expo.plugins[0][1].window.controls = 'macos';
      await writeFile(path.join(project, file), JSON.stringify(changed));
      await assert.rejects(() => quiet(() => prepareNativeMain(args)), /differs/);
      checks++;
      ok((await treeHash(path.join(project, 'build'))) === first, 'rejected run wrote nothing');
    }
  }

  // Failure paths: no package.json, no config, symlinked config, invalid window.
  const bare = path.join(temporary, 'bare');
  await mkdir(bare);
  await assert.rejects(() => loadExpoConfig(bare), /No Expo config/);
  await writeFile(path.join(bare, 'app.json'), '{"expo":{"name":"x"}}');
  await assert.rejects(() => loadExpoConfig(bare), /package\.json/);
  await writeFile(path.join(bare, 'package.json'), '{"name":"bare"}');
  await rm(path.join(bare, 'app.json'));
  await writeFile(path.join(temporary, 'outside.json'), '{"expo":{"name":"x"}}');
  await symlink(path.join(temporary, 'outside.json'), path.join(bare, 'app.json'));
  await assert.rejects(() => loadExpoConfig(bare), /symlink/);
  await rm(path.join(bare, 'app.json'));
  await writeFile(
    path.join(bare, 'app.config.js'),
    "module.exports = { name: 'Bad', extra: { rnqnx: { appId: 'org.a.b', componentName: 'Bad', window: { controls: 'linux' } } } };\n"
  );
  await mkdir(path.join(bare, 'build/qnx'), { recursive: true });
  await writeFile(path.join(bare, 'build/qnx/app.bundle.js'), 'bundle\n');
  await assert.rejects(() => nativeConfiguration(bare, 'qnx'), /window\.controls/);
  checks += 4;

  // Plugin listed but not installed (for example right after add-qnx, before
  // install): the error names the plugin and the install step.
  const uninstalled = path.join(temporary, 'uninstalled');
  await mkdir(uninstalled);
  await writeFile(path.join(uninstalled, 'package.json'), '{"name":"uninstalled"}');
  await writeFile(
    path.join(uninstalled, 'app.json'),
    JSON.stringify({ expo: { name: 'Pending', plugins: ['@rn-qnx/expo-qnx-config-plugins'] } })
  );
  await assert.rejects(
    () => loadExpoConfig(uninstalled),
    /"@rn-qnx\/expo-qnx-config-plugins" .*not installed.*Install the app's dependencies/
  );
  const skipped = await loadExpoConfig(uninstalled, { skipPlugins: true });
  ok(skipped.exp.name === 'Pending', 'skipPlugins reads the config without the plugin');

  // Option types.
  for (const bad of [
    { shortName: 7 },
    { icon: { path: 'a.png' } },
    { appId: '' },
    { componentName: ['A'] },
  ]) {
    assert.throws(() => withQnxProject({}, /** @type {any} */ (bad)), /must be a nonempty string/);
    checks++;
  }
  ok(
    withQnxProject({}, { shortName: 'Notes', icon: './assets/icon.png' }).extra.rnqnx?.icon ===
      './assets/icon.png',
    'string icon accepted'
  );
  console.log(
    `Expo config resolution: ${checks} checks passed (app.json, app.config.js, app.config.ts).`
  );
} finally {
  await rm(temporary, { recursive: true, force: true });
}
