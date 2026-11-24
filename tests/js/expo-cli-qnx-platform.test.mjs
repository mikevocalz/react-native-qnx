import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import { createRequire } from 'node:module';
import path from 'node:path';

// Regression test for the `expo` gate: `expo start` answered GET /index.bundle?platform=qnx with
// HTTP 500 "No support package is known for platform "qnx"". Expo CLI asks
// expo-modules-autolinking for the React Native host package of every Metro platform on each
// resolve, and 58.1.0 hard-codes the platforms it knows in a switch with no registration hook.
// patches/expo-modules-autolinking@58.1.0.patch adds qnx and marathon; this test fails on the
// unpatched package.

const root = process.cwd();
const appRequire = createRequire(path.join(root, 'apps', 'expo-router-example', 'package.json'));
const expoRequire = createRequire(appRequire.resolve('expo/package.json'));
const autolinkingManifest = expoRequire.resolve('expo-modules-autolinking/package.json');
const { version } = JSON.parse(await readFile(autolinkingManifest, 'utf8'));
assert.equal(version, '58.1.0', 'the patch targets exactly expo-modules-autolinking@58.1.0');

// Same entry point Expo CLI uses (`expo/internal/unstable-autolinking-exports`).
const { getSupportPackageForPlatform } = appRequire('expo/internal/unstable-autolinking-exports');

assert.equal(getSupportPackageForPlatform('qnx'), 'react-native');
assert.equal(getSupportPackageForPlatform('marathon'), 'react-native');

// The patch must not change any platform that already worked, and must keep rejecting strangers.
assert.equal(getSupportPackageForPlatform('ios'), 'react-native');
assert.equal(getSupportPackageForPlatform('android'), 'react-native');
assert.equal(getSupportPackageForPlatform('web'), null);
assert.throws(
  () => getSupportPackageForPlatform('not-a-platform'),
  /No support package is known for platform "not-a-platform"/
);

// The workspace must keep the patch applied and the version pinned so it cannot go unused.
const workspace = await readFile(path.join(root, 'pnpm-workspace.yaml'), 'utf8');
assert.match(
  workspace,
  /^ {2}expo-modules-autolinking@58\.1\.0: patches\/expo-modules-autolinking@58\.1\.0\.patch$/m
);
assert.match(workspace, /^ {2}'expo-modules-autolinking': 58\.1\.0$/m);

console.log('expo-cli-qnx-platform: ok');
