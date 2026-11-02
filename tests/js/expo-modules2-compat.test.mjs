import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';

// Source-level contract: unit tests and real JSI target execution follow
// once the Expo SDK 58 lockfile and installed native module graph are green.
const js = await readFile('packages/expo-modules-core-qnx/index.js', 'utf8');
const types = await readFile('packages/expo-modules-core-qnx/index.d.ts', 'utf8');
const docs = await readFile('docs/expo/EXPO_MODULES_2_QNX.md', 'utf8');
assert.match(js, /requireNativeModule, requireOptionalNativeModule/);
assert.match(js, /function getQnxExpoPlatformModule/);
assert.match(js, /function requireQnxExpoPlatformModule/);
assert.match(js, /return requireNativeModule\('ExpoQnxPlatform'\)/);
assert.match(js, /return requireOptionalNativeModule\('ExpoQnxPlatform'\)/);
assert.match(types, /getQnxExpoPlatformModule\(\): ExpoQnxPlatformModule \| null/);
assert.match(types, /requireQnxExpoPlatformModule\(\): ExpoQnxPlatformModule/);
assert.match(docs, /Expo Modules API \*\*2\.0\*\* annotation binding generator/);
assert.match(docs, /unsupported/);
console.log('expo-modules2-compat.test.mjs passed (source only)');
