/**
 * Metro configuration for the QNX platform.
 *
 * Metro resolves platform-specific files by extension, and its default platform
 * list does not include `qnx`: `@expo/metro-config` ships
 * `platforms: ['ios', 'android', 'tvos', 'macos']`, and React Native's own
 * default is the same shape. Without `qnx` in that list a `Foo.qnx.js` file is
 * never preferred over `Foo.js`, which is the whole mechanism an out-of-tree
 * platform relies on.
 */

const fs = require('node:fs');
const path = require('node:path');

const { getDefaultConfig, mergeConfig } = require('@react-native/metro-config');

const QNX_PLATFORM = 'qnx';

/**
 * Builds a Metro `resolveRequest` that prefers the QNX platform package's copy
 * of a React Native module.
 *
 * React Native's own files cannot be edited in `node_modules`, and its
 * `Libraries/Utilities/Platform.js` re-exports `./Platform` expecting a
 * platform-suffixed sibling. With no `Platform.qnx.js` in React Native's tree,
 * that import resolves back to the same file and `Platform` ends up undefined.
 * Rather than special-casing one module, any file under the platform package's
 * `Libraries/` overrides the same relative path under React Native's
 * `Libraries/`, which is the same shape react-native-windows and
 * react-native-macos use.
 *
 * The override applies only to `platform === 'qnx'`, so a shared codebase still
 * builds for other platforms unchanged.
 *
 * @param {string} overridesRoot Absolute path to the platform package's
 *   `Libraries` directory.
 * @param {(context: unknown, moduleName: string, platform: string | null) => unknown} [fallback]
 * @returns {(context: any, moduleName: string, platform: string | null) => any}
 */
function createQnxResolveRequest(overridesRoot) {
  const expoRequireOverride = require.resolve('./runtime/expo-require-native-module');
  let originalExpoRequireSource;
  const expoRequireSources = [
    `${path.sep}expo-modules-core${path.sep}src${path.sep}requireNativeModule.ts`,
    `${path.sep}expo-modules-core${path.sep}build${path.sep}requireNativeModule.js`,
  ];
  const reactNativeLibraries = `${path.sep}react-native${path.sep}Libraries${path.sep}`;

  return function resolveRequest(context, moduleName, platform) {
    // This private alias is admitted only from our wrapper, never app input.
    if (
      platform === QNX_PLATFORM &&
      context.originModulePath === expoRequireOverride &&
      moduleName === '@rn-qnx/internal-expo-native-module'
    ) {
      if (!originalExpoRequireSource)
        throw new Error('Expo native-module wrapper resolved before its upstream module.');
      return { type: 'sourceFile', filePath: originalExpoRequireSource };
    }
    // Metro's own resolver runs first; the override is decided by where that
    // landed, which is what makes relative imports such as `./Platform` work.
    const resolution = context.resolveRequest(context, moduleName, platform);

    if (platform !== QNX_PLATFORM || resolution?.type !== 'sourceFile') {
      return resolution;
    }

    // Redirect public Expo imports, but preserve the wrapper's own upstream
    // import. This avoids recursion and retains the SDK's installer and lookup.
    if (
      expoRequireSources.some((suffix) => resolution.filePath.endsWith(suffix)) &&
      context.originModulePath !== expoRequireOverride
    ) {
      if (originalExpoRequireSource && originalExpoRequireSource !== resolution.filePath) {
        throw new Error(
          'Multiple Expo Modules Core lookup implementations in one QNX bundle; use one package/export condition before building.'
        );
      }
      originalExpoRequireSource = resolution.filePath;
      return { type: 'sourceFile', filePath: expoRequireOverride };
    }

    const marker = resolution.filePath.lastIndexOf(reactNativeLibraries);
    if (marker === -1) {
      return resolution;
    }

    const relative = resolution.filePath.slice(marker + reactNativeLibraries.length);
    const override = path.join(overridesRoot, relative);
    if (override !== resolution.filePath && fs.existsSync(override)) {
      return { type: 'sourceFile', filePath: override };
    }

    return resolution;
  };
}

/**
 * Returns a Metro config for `projectRoot` with the QNX platform registered.
 *
 * @param {string} projectRoot Absolute path to the app.
 * @param {import('metro-config').InputConfigT} [overrides] Merged last, so a
 *   project can still change anything this sets.
 * @returns {import('metro-config').InputConfigT}
 */
function getQnxMetroConfig(projectRoot, overrides) {
  const defaultConfig = getDefaultConfig(projectRoot);

  const platforms = defaultConfig.resolver?.platforms ?? [];
  // Resolved from this package so a project does not have to know where the
  // platform package lives.
  const { OVERRIDES_ROOT } = require('@rn-qnx/react-native-qnx');

  const qnxConfig = {
    resolver: {
      resolveRequest: createQnxResolveRequest(OVERRIDES_ROOT),
      // Order matters to Metro only in that the requested platform is tried
      // first; `qnx` is appended rather than replacing the list so a shared
      // codebase can still resolve its other platforms.
      platforms: platforms.includes(QNX_PLATFORM) ? platforms : [...platforms, QNX_PLATFORM],
    },
  };

  const merged = mergeConfig(defaultConfig, qnxConfig);
  return overrides ? mergeConfig(merged, overrides) : merged;
}

module.exports = { getQnxMetroConfig, createQnxResolveRequest, QNX_PLATFORM };
