#!/usr/bin/env node

import { readFile } from 'node:fs/promises';
import { createRequire } from 'node:module';
import path from 'node:path';

const require = createRequire(import.meta.url);
const root = process.cwd();

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

/** @param {string} value */
function minimumCaretVersion(value) {
  const match = /^\^(\d+\.\d+\.\d+)$/.exec(value);
  return match?.[1] ?? null;
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

  assertEqual(workspace.packageManager, 'pnpm@11.21.0', 'Workspace package manager');

  for (const [name, version] of Object.entries(dependencies)) {
    if (!exactVersion(version)) {
      throw new Error(`${name} must use an exact version, got ${version}`);
    }
  }

  const directPackages = [
    '@react-native/jest-preset',
    '@types/node',
    '@types/react',
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
  const nitrogen = manifests.get('nitrogen').manifest;
  const nitro = manifests.get('react-native-nitro-modules').manifest;

  assertEqual(
    minimumCaretVersion(reactNative.peerDependencies?.react),
    react.version,
    'React Native peer baseline'
  );
  assertEqual(expo.devDependencies?.react, react.version, 'Expo tested React version');
  assertEqual(
    expo.devDependencies?.['react-native'],
    reactNative.version,
    'Expo tested React Native version'
  );
  assertEqual(nitrogen.version, nitro.version, 'Nitrogen/Nitro release line');
  assertEqual(
    minimumCaretVersion(nitrogen.dependencies?.['react-native-nitro-modules']),
    nitro.version,
    'Nitrogen Nitro dependency baseline'
  );

  const expoRequire = createRequire(manifests.get('expo').manifestPath);
  const expoModules = await readJson(expoRequire.resolve('expo-modules-core/package.json'));
  const expoMetro = await readJson(expoRequire.resolve('@expo/metro/package.json'));
  const expoMetroConfig = await readJson(expoRequire.resolve('@expo/metro-config/package.json'));

  assertEqual(expoModules.version, '57.0.10', 'Expo Modules Core resolved version');
  assertEqual(expoMetro.version, '56.0.0', 'Expo Metro fork resolved version');
  assertEqual(expoMetro.dependencies?.metro, metro.version, 'Expo Metro upstream version');
  assertEqual(expoMetroConfig.version, '57.0.8', 'Expo Metro config resolved version');

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

  const hermesV1 = (
    await readFile(path.join(reactNativeRoot, 'sdks/.hermesv1version'), 'utf8')
  ).trim();
  assertEqual(
    hermesV1,
    `hermes-v${reactNative.dependencies?.['hermes-compiler']}`,
    'React Native Hermes v1 source tag'
  );

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
