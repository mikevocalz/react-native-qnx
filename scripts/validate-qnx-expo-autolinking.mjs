#!/usr/bin/env node

import { readFile } from 'node:fs/promises';
import { createRequire } from 'node:module';
import path from 'node:path';

const root = process.cwd();
const require = createRequire(import.meta.url);
const expoManifest = require.resolve('expo/package.json');
const expoRequire = createRequire(expoManifest);
const autolinkingManifest = expoRequire.resolve('expo-modules-autolinking/package.json');
const autolinkingRoot = path.dirname(autolinkingManifest);
const manifest = JSON.parse(await readFile(autolinkingManifest, 'utf8'));

if (manifest.version !== '58.0.6') {
  throw new Error(`Expected expo-modules-autolinking 58.0.6, got ${manifest.version}`);
}

const types = await readFile(path.join(autolinkingRoot, 'src/types.ts'), 'utf8');
if (!types.includes('| (string & {})')) {
  throw new Error('Expo SupportedPlatform no longer permits custom platform names.');
}

const config = await readFile(path.join(autolinkingRoot, 'src/ExpoModuleConfig.ts'), 'utf8');
if (!config.includes('return supportedPlatforms.includes(platform);')) {
  throw new Error('ExpoModuleConfig custom-platform resolution contract changed.');
}

const platforms = await readFile(path.join(autolinkingRoot, 'src/platforms/index.ts'), 'utf8');
if (!platforms.includes('No linking implementation is available for platform')) {
  throw new Error('Expo platform resolver behavior changed; re-audit QNX wrapper.');
}

const expoAutolinking = expoRequire('expo-modules-autolinking/exports');
const results = await expoAutolinking.findModulesAsync({
  projectRoot: root,
  platform: 'qnx',
});
if (results == null || typeof results !== 'object') {
  throw new Error('Expo QNX search phase did not return SearchResults.');
}

console.log('validate:qnx-expo-autolinking passed for expo-modules-autolinking 58.0.6');
