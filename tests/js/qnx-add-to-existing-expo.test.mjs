import assert from 'node:assert/strict';
import { createHash } from 'node:crypto';
import { mkdir, mkdtemp, readFile, readdir, rm, symlink, writeFile } from 'node:fs/promises';
import { tmpdir } from 'node:os';
import path from 'node:path';
import { addQnx } from '../../packages/react-native-qnx-cli/src/add-qnx.mjs';
import { NEEDS_DEVICE_EXIT, runProject } from '../../packages/react-native-qnx-cli/src/run.mjs';
import { planProject } from '../../packages/react-native-qnx-cli/src/project.mjs';

// An existing Expo app with iOS, Android and web set up, created outside the
// checkout. add-qnx must keep all of that byte-identical.
const temporary = await mkdtemp(path.join(tmpdir(), 'rnqnx-add-qnx-'));
const app = path.join(temporary, 'existing app');
let checks = 0;
const ok = (/** @type {unknown} */ v, /** @type {string} */ m) => {
  assert.ok(v, m);
  checks++;
};
const hashTree = async (/** @type {string[]} */ names) => {
  const hash = createHash('sha256');
  const walk = async (/** @type {string} */ p) => {
    const entries = await readdir(p, { withFileTypes: true }).catch(() => null);
    if (!entries) return hash.update(p + (await readFile(p)));
    for (const e of entries.sort((a, b) => a.name.localeCompare(b.name)))
      await walk(path.join(p, e.name));
  };
  for (const name of names) await walk(path.join(app, name));
  return hash.digest('hex');
};
const preserved = ['ios', 'android', 'metro.config.js', 'app', 'babel.config.js'];
try {
  await mkdir(path.join(app, 'ios/ExistingApp'), { recursive: true });
  await mkdir(path.join(app, 'android/app'), { recursive: true });
  await mkdir(path.join(app, 'app'), { recursive: true });
  await mkdir(path.join(app, 'node_modules/@rn-qnx'), { recursive: true });
  await symlink(
    path.resolve('packages/expo-qnx-config-plugins'),
    path.join(app, 'node_modules/@rn-qnx/expo-qnx-config-plugins')
  );
  await mkdir(path.join(app, 'plugins'));
  await writeFile(
    path.join(app, 'plugins/with-existing.js'),
    'module.exports = (config) => config;\n'
  );
  await writeFile(path.join(app, 'ios/ExistingApp/AppDelegate.swift'), 'import Expo\n');
  await writeFile(
    path.join(app, 'android/app/build.gradle'),
    'apply plugin: "com.android.application"\n'
  );
  await writeFile(
    path.join(app, 'app/index.tsx'),
    'export default function Home() { return null; }\n'
  );
  await writeFile(
    path.join(app, 'metro.config.js'),
    "module.exports = require('expo/metro-config').getDefaultConfig(__dirname);\n"
  );
  await writeFile(
    path.join(app, 'babel.config.js'),
    "module.exports = { presets: ['babel-preset-expo'] };\n"
  );
  const manifest = {
    name: 'existing-app',
    version: '1.0.0',
    main: 'expo-router/entry',
    scripts: {
      start: 'expo start',
      ios: 'expo run:ios',
      android: 'expo run:android',
      web: 'expo start --web',
    },
    dependencies: {
      expo: '58.0.22',
      'expo-router': '58.0.0',
      react: '19.3.0',
      'react-native': '0.88.1',
    },
  };
  await writeFile(path.join(app, 'package.json'), JSON.stringify(manifest, null, 2) + '\n');
  const appJson = {
    expo: {
      name: 'Existing App',
      slug: 'existing-app',
      platforms: ['ios', 'android', 'web'],
      ios: { bundleIdentifier: 'org.example.existing' },
      android: { package: 'org.example.existing' },
      web: { bundler: 'metro', output: 'static' },
      plugins: ['./plugins/with-existing.js'],
    },
  };
  await writeFile(path.join(app, 'app.json'), JSON.stringify(appJson, null, 2) + '\n');
  const before = await hashTree(preserved);

  const dry = await addQnx({ project: app, appId: 'org.example.existing', dryRun: true });
  ok(
    dry.changes.some((c) => c.action === 'created'),
    'dry run plans changes'
  );
  ok(
    (await hashTree(preserved)) === before && !(await readdir(app)).includes('metro.qnx.config.js'),
    'dry run writes nothing'
  );

  const first = await addQnx({ project: app, appId: 'org.example.existing' });
  ok(first.entry === 'expo-router/entry', 'entry point reported');
  ok((await hashTree(preserved)) === before, 'ios, android, metro.config.js, app/ untouched');
  const nextManifest = JSON.parse(await readFile(path.join(app, 'package.json'), 'utf8'));
  ok(nextManifest.main === 'expo-router/entry', 'main unchanged');
  ok(
    nextManifest.scripts.ios === 'expo run:ios' && nextManifest.scripts.web === 'expo start --web',
    'existing scripts kept'
  );
  ok(/project run --project \.$/.test(nextManifest.scripts.qnx), 'qnx script added');
  ok(
    nextManifest.dependencies['@rn-qnx/metro-config-qnx'].startsWith('file:'),
    'metro config linked'
  );
  const nextApp = JSON.parse(await readFile(path.join(app, 'app.json'), 'utf8'));
  assert.deepEqual(
    { ...nextApp.expo, plugins: undefined },
    { ...appJson.expo, plugins: undefined }
  );
  checks++;
  assert.deepEqual(nextApp.expo.plugins, [
    './plugins/with-existing.js',
    ['@rn-qnx/expo-qnx-config-plugins', { appId: 'org.example.existing', componentName: 'main' }],
  ]);
  checks++;
  const snapshot = await hashTree(['package.json', 'app.json', 'metro.qnx.config.js']);
  const second = await addQnx({ project: app, appId: 'org.example.existing' });
  ok(
    second.changes.every((c) => c.action === 'unchanged'),
    'second run reports no changes'
  );
  ok(
    (await hashTree(['package.json', 'app.json', 'metro.qnx.config.js'])) === snapshot,
    'second run is byte-identical'
  );

  await assert.rejects(() => addQnx({ project: app, appId: 'org.example.other' }), /not replacing/);
  await assert.rejects(() => addQnx({ project: app, appId: 'not-reverse-dns' }));
  checks += 2;

  // project bundle picks metro.qnx.config.js for qnx (plan only; Metro is not installed here).
  await writeFile(path.join(app, 'index.js'), '');
  await symlink(path.resolve('node_modules/metro'), path.join(app, 'node_modules/metro'));
  const plan = await planProject(['inspect', '--project', app]);
  ok(plan?.config.endsWith('metro.qnx.config.js'), 'bundle uses metro.qnx.config.js');

  // project run: config resolves, then it stops at the missing bundle.
  let run = await runProject({ project: app, env: {} });
  ok(run.exitCode === NEEDS_DEVICE_EXIT, 'run stops');
  ok(run.steps[0].status === 'done' && run.steps[1].status === 'needs bundle', 'stops at bundle');
  ok(
    run.steps.slice(2).every((s) => s.status === 'not run'),
    'later steps reported not run'
  );
  // With a bundle and no SDP it prepares the descriptor and stops at the build.
  await mkdir(path.join(app, 'build/qnx'), { recursive: true });
  await writeFile(path.join(app, 'build/qnx/app.bundle.js'), 'bundle\n');
  run = await runProject({ project: app, env: {} });
  ok(
    run.steps[2].status === 'done' && run.steps[3].status === 'needs SDP',
    'stops at native build without SDP'
  );
  ok(run.steps.find((s) => s.step === 'launch')?.status === 'not run', 'launch never claimed');
  const descriptor = JSON.parse(
    await readFile(path.join(app, 'build/qnx/native-app.json'), 'utf8')
  );
  ok(
    descriptor.appId === 'org.example.existing' && descriptor.componentName === 'main',
    'descriptor from plugin'
  );

  // Dynamic config: printed, not edited.
  const dynamic = path.join(temporary, 'dynamic');
  await mkdir(dynamic);
  await writeFile(
    path.join(dynamic, 'package.json'),
    JSON.stringify({ name: 'dyn', dependencies: { expo: '58.0.22' } })
  );
  const configSource =
    "module.exports = ({ config }) => ({ ...config, name: 'Dyn', slug: 'dyn' });\n";
  await writeFile(path.join(dynamic, 'app.config.js'), configSource);
  const dyn = await addQnx({ project: dynamic, appId: 'org.example.dyn' });
  ok(
    dyn.manualStep?.includes('@rn-qnx/expo-qnx-config-plugins'),
    'dynamic config gets a manual step'
  );
  ok(
    (await readFile(path.join(dynamic, 'app.config.js'), 'utf8')) === configSource,
    'app.config.js not edited'
  );
  console.log(
    `Add QNX to an existing Expo app: ${checks} checks passed (fixture outside the checkout).`
  );
} finally {
  await rm(temporary, { recursive: true, force: true });
}
