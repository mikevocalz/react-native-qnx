#!/usr/bin/env node

import { readFile } from 'node:fs/promises';
import { createRequire } from 'node:module';
import path from 'node:path';

const root = process.cwd();
const require = createRequire(import.meta.url);
const expoManifest = require.resolve('expo/package.json');
const expoRequire = createRequire(expoManifest);
const coreManifest = expoRequire.resolve('expo-modules-core/package.json');
const coreRoot = path.dirname(coreManifest);
const core = JSON.parse(await readFile(coreManifest, 'utf8'));

const qnxPeerManifest = JSON.parse(
  await readFile(path.join(root, 'packages/expo-modules-core-qnx/package.json'), 'utf8')
);
const requiredVersion = qnxPeerManifest.peerDependencies?.['expo-modules-core'];
if (core.version !== requiredVersion || !core.version.startsWith('58.')) {
  throw new Error(
    `QNX Expo Modules peer requires ${requiredVersion}; installed expo-modules-core is ${core.version}`
  );
}

const ensure = await readFile(
  path.join(coreRoot, 'src/ensureNativeModulesAreInstalled.native.ts'),
  'utf8'
);
if (
  !ensure.includes('globalThis.expo') ||
  !ensure.includes("TurboModuleRegistry.get('ExpoModulesCore')")
) {
  throw new Error('Expo Modules Core native bootstrap contract changed.');
}

const requireNative = await readFile(path.join(coreRoot, 'src/requireNativeModule.ts'), 'utf8');
if (!requireNative.includes('globalThis.expo?.modules?.[moduleName]')) {
  throw new Error('Expo requireNativeModule no longer prioritizes globalThis.expo.modules.');
}

const installer = await readFile(
  path.join(root, 'packages/expo-modules-core-qnx/cpp/ExpoQnxInstaller.cpp'),
  'utf8'
);
if (!installer.includes('runtime.global().setProperty(runtime, "expo"')) {
  throw new Error('QNX Expo installer must install globalThis.expo at native startup.');
}
if (!installer.includes('expo.setProperty(runtime, "modules"')) {
  throw new Error('QNX Expo installer must expose the modules registry.');
}

const integrationProbe = await readFile(
  path.join(root, 'vnext/probes/runtime-qnx/main.cpp'),
  'utf8'
);
if (
  !integrationProbe.includes('rnqnx::expo::installExpoModulesQnx(*runtime)') ||
  !integrationProbe.includes('expo.modules.ExpoQnxPlatform')
) {
  throw new Error('Combined QNX runtime probe no longer exercises Expo Modules through JSI.');
}

console.log(`validate:qnx-expo-modules passed for expo-modules-core ${core.version}`);
