#!/usr/bin/env node
// Expo Modules SDK58 JSI bridge and API 2.0 readiness check.
// Requires a genuine frozen Expo SDK58 install. This verifies the npm package
// source/API contract, not a QNX hardware or Swift/Kotlin v2 runtime.
import assert from 'node:assert/strict';
import { createRequire } from 'node:module';
import { readFile } from 'node:fs/promises';
import path from 'node:path';

const require = createRequire(import.meta.url);
const expoPath = require.resolve('expo/package.json');
const expoRequire = createRequire(expoPath);
const modulesPath = expoRequire.resolve('expo-modules-core/package.json');
const modules = JSON.parse(await readFile(modulesPath, 'utf8'));

assert.equal(Number(modules.version.split('.')[0]), 58, 'SDK 58 Expo Modules Core required');

const root = path.dirname(modulesPath);
const contract = await readFile(path.join(root, 'src/requireNativeModule.ts'), 'utf8');
assert.match(contract, /export function requireNativeModule/);
assert.match(contract, /export function requireOptionalNativeModule/);
assert.match(contract, /globalThis\.expo\?\.modules\?\./);

const native = await readFile('packages/expo-modules-core-qnx/cpp/ExpoQnxInstaller.cpp', 'utf8');
assert.match(native, /installExpoModulesQnx/);
assert.match(native, /runtime\.global\(\)\.setProperty\(runtime, "expo"/);
const exported = await readFile('packages/expo-modules-core-qnx/index.js', 'utf8');
assert.match(exported, /requireOptionalNativeModule\('ExpoQnxPlatform'\)/);
assert.match(exported, /return requireNativeModule\('ExpoQnxPlatform'\)/);
const types = await readFile('packages/expo-modules-core-qnx/index.d.ts', 'utf8');
assert.match(types, /requireQnxExpoPlatformModule\(\): ExpoQnxPlatformModule;/);

console.log('Expo 58 native module API contract verified. Expo Modules API 2.0 native QNX bindings remain unsupported.');
