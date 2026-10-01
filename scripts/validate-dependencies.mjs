#!/usr/bin/env node

import { readFile } from 'node:fs/promises';
import { createRequire } from 'node:module';
import path from 'node:path';

const root = process.cwd();
const EXPECTED_PNPM = '12.8.1';
const EXPECTED_EXPO_SDK_MAJOR = 58;

/** @param {string} file */
async function readJson(file) {
  return JSON.parse(await readFile(file, 'utf8'));
}

/**
 * @param {string} name
 * @param {string | URL} [from]
 */
function resolvePackageJson(name, from = import.meta.url) {
  return createRequire(from).resolve(`${name}/package.json`);
}

/** @param {string} value */
function exactVersion(value) {
  return /^\d+\.\d+\.\d+(?:-[0-9A-Za-z.-]+)?$/.test(value);
}

/** @param {string} version */
function majorVersion(version) {
  const match = /^(\d+)\./.exec(version);
  return match == null ? null : Number(match[1]);
}

/** @param {string} version */
function versionParts(version) {
  const match = /^(\d+)\.(\d+)\.(\d+)(?:-[0-9A-Za-z.-]+)?$/.exec(version);
  if (match == null) return null;
  return match.slice(1, 4).map(Number);
}

/** @param {number[]} a @param {number[]} b */
function compareParts(a, b) {
  for (let i = 0; i < 3; i += 1) {
    if (a[i] !== b[i]) return a[i] < b[i] ? -1 : 1;
  }
  return 0;
}

/** @param {string | undefined} range @param {string} version */
function simpleRangeAllowsVersion(range, version) {
  if (range == null) return false;
  if (range === version || range === '*') return true;

  const actual = versionParts(version);
  const prefix = range[0];
  const base = versionParts(prefix === '^' || prefix === '~' ? range.slice(1) : range);
  if (actual == null || base == null) return false;

  if (prefix === '~') {
    return actual[0] === base[0] && actual[1] === base[1] && compareParts(actual, base) >= 0;
  }

  if (prefix === '^') {
    if (compareParts(actual, base) < 0) return false;
    if (base[0] > 0) return actual[0] === base[0];
    if (base[1] > 0) return actual[0] === 0 && actual[1] === base[1];
    return actual[0] === 0 && actual[1] === 0 && actual[2] === base[2];
  }

  return false;
}

/**
 * @param {unknown} actual
 * @param {unknown} expected
 * @param {string} label
 */
function assertEqual(actual, expected, label) {
  if (actual !== expected) {
    throw new Error(`${label}: expected ${expected}, got ${actual ?? '<missing>'}`);
  }
}

/**
 * @param {boolean} condition
 * @param {string} label
 */
function assertTrue(condition, label) {
  if (!condition) {
    throw new Error(label);
  }
}

/**
 * @param {string} packageRoot
 * @param {string} relativePath
 * @param {string} symbol
 */
async function assertSourceContract(packageRoot, relativePath, symbol) {
  const file = path.join(packageRoot, relativePath);
  const source = await readFile(file, 'utf8');
  if (!source.includes(symbol)) {
    throw new Error(`${relativePath} no longer contains authoritative symbol ${symbol}`);
  }
  console.log(`SOURCE: ${relativePath} -> ${symbol}`);
}

async function main() {
  const workspace = await readJson(path.join(root, 'package.json'));
  const dependencies = workspace.devDependencies ?? {};

  assertEqual(workspace.packageManager, `pnpm@${EXPECTED_PNPM}`, 'Workspace package manager');

  for (const [name, version] of Object.entries(dependencies)) {
    if (!exactVersion(version)) {
      throw new Error(`${name} must use an exact version, got ${version}`);
    }
  }

  const directPackages = [
    '@react-native/metro-config',
    '@types/node',
    '@types/react',
    'babel-preset-expo',
    'expo',
    'metro',
    'nitrogen',
    'prettier',
    'react',
    'react-native',
    'react-native-nitro-modules',
    'typescript',
  ];

  const manifests = new Map();
  for (const name of directPackages) {
    const manifestPath = resolvePackageJson(name);
    const manifest = await readJson(manifestPath);
    assertEqual(manifest.version, dependencies[name], `${name} installed version`);
    manifests.set(name, { manifest, manifestPath });
    console.log(`PIN: ${name}@${manifest.version}`);
  }

  const react = manifests.get('react').manifest;
  const reactNative = manifests.get('react-native').manifest;
  const expo = manifests.get('expo').manifest;
  const metro = manifests.get('metro').manifest;
  const rnMetroConfig = manifests.get('@react-native/metro-config').manifest;
  const babelPresetExpo = manifests.get('babel-preset-expo').manifest;
  const nitrogen = manifests.get('nitrogen').manifest;
  const nitro = manifests.get('react-native-nitro-modules').manifest;

  assertEqual(majorVersion(expo.version), EXPECTED_EXPO_SDK_MAJOR, 'Expo SDK major');
  assertEqual(expo.devDependencies?.react, react.version, 'Expo tested React version');
  assertEqual(
    expo.devDependencies?.['react-native'],
    reactNative.version,
    'Expo tested React Native version'
  );
  assertTrue(
    simpleRangeAllowsVersion(reactNative.peerDependencies?.react, react.version),
    `React Native peer range ${reactNative.peerDependencies?.react ?? '<missing>'} does not name React ${react.version}`
  );
  assertEqual(nitrogen.version, nitro.version, 'Nitrogen/Nitro release line');
  assertTrue(
    simpleRangeAllowsVersion(nitrogen.dependencies?.['react-native-nitro-modules'], nitro.version),
    `Nitrogen does not depend on the selected Nitro release ${nitro.version}`
  );
  assertTrue(
    majorVersion(babelPresetExpo.version) === EXPECTED_EXPO_SDK_MAJOR,
    `babel-preset-expo must be on SDK ${EXPECTED_EXPO_SDK_MAJOR}, got ${babelPresetExpo.version}`
  );

  const rnMetroRequire = createRequire(manifests.get('@react-native/metro-config').manifestPath);
  const metroConfig = await readJson(rnMetroRequire.resolve('metro-config/package.json'));
  assertEqual(metroConfig.version, metro.version, 'React Native Metro config / Metro release line');

  const expoRequire = createRequire(manifests.get('expo').manifestPath);
  const expoModules = await readJson(expoRequire.resolve('expo-modules-core/package.json'));
  const expoMetro = await readJson(expoRequire.resolve('@expo/metro/package.json'));
  const expoMetroConfig = await readJson(expoRequire.resolve('@expo/metro-config/package.json'));

  for (const [label, manifest] of [
    ['expo-modules-core', expoModules],
    ['@expo/metro', expoMetro],
    ['@expo/metro-config', expoMetroConfig],
  ]) {
    assertEqual(
      majorVersion(manifest.version),
      EXPECTED_EXPO_SDK_MAJOR,
      `${label} SDK major`
    );
    console.log(`RESOLVED: ${label}@${manifest.version}`);
  }

  if (expoMetro.dependencies?.metro != null) {
    assertTrue(
      simpleRangeAllowsVersion(expoMetro.dependencies.metro, metro.version),
      `@expo/metro dependency ${expoMetro.dependencies.metro} does not name Metro ${metro.version}`
    );
  }

  if (rnMetroConfig.dependencies?.['metro-config'] != null) {
    assertTrue(
      simpleRangeAllowsVersion(rnMetroConfig.dependencies['metro-config'], metroConfig.version),
      `@react-native/metro-config dependency ${rnMetroConfig.dependencies['metro-config']} does not name metro-config ${metroConfig.version}`
    );
  }

  const reactNativeRoot = path.dirname(manifests.get('react-native').manifestPath);
  const nitroRoot = path.dirname(manifests.get('react-native-nitro-modules').manifestPath);
  const expoModulesRoot = path.dirname(expoRequire.resolve('expo-modules-core/package.json'));
  const expoMetroConfigRoot = path.dirname(expoRequire.resolve('@expo/metro-config/package.json'));
  const xcodeManifestPath = expoRequire.resolve('xcode/package.json');
  const xcodeRoot = path.dirname(xcodeManifestPath);
  const xcodeRequire = createRequire(xcodeManifestPath);
  const xcodeUuid = await readJson(xcodeRequire.resolve('uuid/package.json'));
  const nitrogenRoot = path.dirname(manifests.get('nitrogen').manifestPath);

  assertEqual(xcodeUuid.version, '11.1.1', 'xcode uuid security override');
  assertEqual(typeof xcodeRequire('uuid').v4, 'function', 'xcode uuid.v4 CommonJS contract');

  const hermesCompiler = reactNative.dependencies?.['hermes-compiler'];
  assertTrue(typeof hermesCompiler === 'string', 'React Native must declare hermes-compiler');
  const hermesV1 = (
    await readFile(path.join(reactNativeRoot, 'sdks/.hermesv1version'), 'utf8')
  ).trim();
  assertTrue(
    /^hermes-v\d+\.\d+\.\d+(?:-[0-9A-Za-z.-]+)?$/.test(hermesV1),
    `Unexpected React Native Hermes source tag: ${hermesV1}`
  );
  const hermesSourceVersion = hermesV1.slice('hermes-v'.length);
  const compilerFamily = hermesCompiler.split('.').slice(0, 2).join('.');
  const sourceFamily = hermesSourceVersion.split('.').slice(0, 2).join('.');
  assertEqual(sourceFamily, compilerFamily, 'Hermes compiler/source family');
  console.log(`BUNDLED: Hermes source ${hermesV1}`);
  console.log(`RESOLVED: hermes-compiler@${hermesCompiler}`);

  await assertSourceContract(
    reactNativeRoot,
    'ReactCommon/react/renderer/scheduler/Scheduler.h',
    'class Scheduler final'
  );
  await assertSourceContract(
    reactNativeRoot,
    'Libraries/Renderer/implementations/ReactFabric-dev.js',
    `reconcilerVersion: "${react.version}"`
  );
  await assertSourceContract(
    reactNativeRoot,
    'ReactCommon/react/renderer/scheduler/SchedulerToolbox.h',
    'struct SchedulerToolbox final'
  );
  await assertSourceContract(
    reactNativeRoot,
    'ReactCommon/react/renderer/scheduler/SchedulerDelegate.h',
    'schedulerDidFinishTransaction'
  );
  await assertSourceContract(
    reactNativeRoot,
    'ReactCommon/react/renderer/mounting/MountingCoordinator.h',
    'pullTransaction('
  );
  await assertSourceContract(
    reactNativeRoot,
    'ReactCommon/react/renderer/mounting/MountingTransaction.h',
    'class MountingTransaction final'
  );
  await assertSourceContract(
    reactNativeRoot,
    'ReactCommon/react/renderer/mounting/ShadowViewMutation.h',
    'struct ShadowViewMutation final'
  );
  await assertSourceContract(
    reactNativeRoot,
    'ReactCommon/react/renderer/uimanager/UIManagerBinding.h',
    'createAndInstallIfNeeded'
  );
  await assertSourceContract(
    reactNativeRoot,
    'ReactCommon/yoga/yoga/YGNode.h',
    'YGNodeCalculateLayout'
  );
  await assertSourceContract(
    reactNativeRoot,
    'ReactCommon/react/runtime/hermes/HermesInstance.h',
    'createJSRuntime('
  );
  await assertSourceContract(
    nitroRoot,
    'cpp/registry/HybridObjectRegistry.hpp',
    'registerHybridObjectConstructor'
  );
  await assertSourceContract(nitroRoot, 'cpp/entrypoint/InstallNitro.hpp', 'void install(');
  await assertSourceContract(nitroRoot, 'src/HybridObject.ts', "android?: 'c++' | 'kotlin'");
  await assertSourceContract(nitrogenRoot, 'src/getPlatformSpecs.ts', "ios: ['swift', 'c++']");
  await assertSourceContract(
    expoModulesRoot,
    'android/src/main/cpp/ExpoModulesHostObject.h',
    'class ExpoModulesHostObject'
  );
  await assertSourceContract(
    expoModulesRoot,
    'src/ensureNativeModulesAreInstalled.native.ts',
    "TurboModuleRegistry.get('ExpoModulesCore')"
  );
  await assertSourceContract(
    expoModulesRoot,
    'src/requireNativeModule.ts',
    'export function requireNativeModule'
  );
  await assertSourceContract(
    expoMetroConfigRoot,
    'build/ExpoMetroConfig.js',
    "platforms: ['ios', 'android', 'tvos', 'macos']"
  );
  await assertSourceContract(xcodeRoot, 'lib/pbxProject.js', 'uuid.v4()');

  console.log('validate:dependencies passed.');
}

main().catch((error) => {
  console.error(error);
  process.exitCode = 1;
});
