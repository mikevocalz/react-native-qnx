import { lstat } from 'node:fs/promises';
import { createRequire } from 'node:module';
import path from 'node:path';

// Expo config files @expo/config can evaluate, in its own lookup order.
export const EXPO_CONFIG_FILES = Object.freeze([
  'app.config.ts',
  'app.config.mts',
  'app.config.cts',
  'app.config.js',
  'app.config.mjs',
  'app.config.cjs',
  'app.json',
]);
const MAX_CONFIG_BYTES = 65536;

/** @param {unknown} error */
function absent(error) {
  return error instanceof Error && 'code' in error && error.code === 'ENOENT';
}

/**
 * Find @expo/config the way the Expo CLI would: from the app's own expo
 * installation first, then from this checkout. The app's copy wins so the
 * resolved config matches what `expo start` and `expo prebuild` see.
 * @param {string} root
 */
export function resolveExpoConfigModule(root) {
  /** @type {string[]} */
  const tried = [];
  const anchors = [
    { from: path.join(root, 'package.json'), via: 'expo/package.json', source: 'project expo' },
    { from: path.join(root, 'package.json'), via: null, source: 'project @expo/config' },
    { from: import.meta.url, via: 'expo/package.json', source: 'react-native-qnx checkout expo' },
  ];
  for (const anchor of anchors) {
    try {
      let require = createRequire(anchor.from);
      if (anchor.via) require = createRequire(require.resolve(anchor.via));
      const modulePath = require.resolve('@expo/config');
      const version = require('@expo/config/package.json').version;
      return { module: require('@expo/config'), modulePath, version, source: anchor.source };
    } catch (error) {
      tried.push(
        `${anchor.source}: ${error instanceof Error ? error.message.split('\n')[0] : error}`
      );
    }
  }
  throw new Error(
    `Cannot load @expo/config. Install expo in the app (npx expo install expo).\n  ${tried.join('\n  ')}`
  );
}

/**
 * Resolve app.json and app.config.{js,ts,...} through @expo/config, including
 * config plugins listed in `plugins`. app.config.* is code from the app and runs
 * here, exactly as it does under `expo start` and `expo prebuild`.
 * @param {string} root absolute, realpath'd project directory
 */
export async function loadExpoConfig(root) {
  /** @type {string[]} */
  const present = [];
  for (const name of EXPO_CONFIG_FILES) {
    let stat;
    try {
      stat = await lstat(path.join(root, name));
    } catch (error) {
      if (absent(error)) continue;
      throw error;
    }
    if (stat.isSymbolicLink() || !stat.isFile())
      throw new Error(`${name} must be a regular file, not a symlink or directory.`);
    if (stat.size < 1 || stat.size > MAX_CONFIG_BYTES)
      throw new Error(`${name} must be 1-${MAX_CONFIG_BYTES} bytes.`);
    present.push(name);
  }
  if (!present.length)
    throw new Error('No Expo config found. Add app.json or app.config.js/ts to the project root.');
  try {
    const stat = await lstat(path.join(root, 'package.json'));
    if (!stat.isFile()) throw new Error('package.json must be a regular file.');
  } catch (error) {
    if (absent(error))
      throw new Error(
        `The project needs a package.json next to its Expo config (${root}); @expo/config resolves app.json and app.config.* from it, as Expo CLI does.`
      );
    throw error;
  }
  const loader = resolveExpoConfigModule(root);
  const { exp, dynamicConfigPath, staticConfigPath } = loader.module.getConfig(root, {
    skipSDKVersionRequirement: true,
    isPublicConfig: false,
  });
  return {
    exp,
    files: present,
    dynamicConfigPath: dynamicConfigPath ?? null,
    staticConfigPath: staticConfigPath ?? null,
    loader: { source: loader.source, version: loader.version },
  };
}
