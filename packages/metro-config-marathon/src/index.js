/**
 * React Native out-of-tree platform resolver for MarathonOS.
 *
 * Marathon has its own Platform identity and its own JS overrides under
 * `@rn-qnx/react-native-marathon`. For every other React Native library file
 * it falls back, at the source level, to the QNX JS overrides, because the Qt
 * view components they implement are not QNX specific. No QNX binary, Screen
 * code or ABI is shared. Resolution order for a React Native `Libraries/` file:
 * Marathon override, then QNX override, then React Native's own file.
 *
 * The resolver is opt-in and never touches the `qnx` Metro configuration. It
 * only acts when Metro asks for the `marathon` platform.
 *
 * Marathon-Shell reference: https://github.com/patrickjquinn/Marathon-Shell
 * (the shell owns the Wayland compositor, system bars and keyboard).
 */
'use strict';

const fs = require('node:fs');
const path = require('node:path');

const { getDefaultConfig, mergeConfig } = require('@react-native/metro-config');
const { OVERRIDES_ROOT: QNX_OVERRIDES_ROOT } = require('@rn-qnx/react-native-qnx');
const { OVERRIDES_ROOT: MARATHON_OVERRIDES_ROOT } = require('@rn-qnx/react-native-marathon');

const MARATHON_PLATFORM = 'marathon';

// expo-modules-core's polyfill entry is a no-op outside the web. The shared
// polyfill under the QNX package defines globalThis.expo and its JS classes
// without a native host, which is what Marathon needs as well.
const EXPO_POLYFILL_ENTRY =
  /[\\/]expo-modules-core[\\/](?:build[\\/]polyfill[\\/]index\.js|src[\\/]polyfill[\\/]index\.ts)$/;

/**
 * A `src/private` module of React Native that exists only as `.ios.js` or
 * `.android.js` (for example `ReactDevToolsSettingsManager` in 0.88), found in
 * one of the override packages, or null.
 *
 * @param {string} origin Absolute path of the importing module.
 * @param {string} moduleName Relative import as written.
 * @param {string[]} privateRoots Override packages' `src/private` directories,
 *   in priority order.
 */
function privateOverride(origin, moduleName, privateRoots) {
  const reactNativeRoot = `${path.sep}react-native${path.sep}`;
  const target = path.resolve(path.dirname(origin), moduleName);
  const marker = target.lastIndexOf(reactNativeRoot);
  if (marker === -1) return null;
  const relative = target.slice(marker + reactNativeRoot.length);
  const prefix = `src${path.sep}private${path.sep}`;
  if (!relative.startsWith(prefix)) return null;
  for (const root of privateRoots) {
    const candidate = path.join(root, `${relative.slice(prefix.length)}.js`);
    if (fs.existsSync(candidate)) return candidate;
  }
  return null;
}

/**
 * @param {string} marathonOverridesRoot
 * @param {string} qnxOverridesRoot
 * @returns {(context: any, moduleName: string, platform: string | null) => any}
 */
function createMarathonResolveRequest(
  marathonOverridesRoot = MARATHON_OVERRIDES_ROOT,
  qnxOverridesRoot = QNX_OVERRIDES_ROOT
) {
  const reactNativeLibraries = `${path.sep}react-native${path.sep}Libraries${path.sep}`;
  const privateRoots = [marathonOverridesRoot, qnxOverridesRoot].map((root) =>
    path.join(path.dirname(root), 'src', 'private')
  );
  const expoPolyfill = path.join(path.dirname(qnxOverridesRoot), 'expo', 'polyfill.js');

  return function resolveRequest(context, moduleName, platform) {
    let resolution;
    try {
      resolution = context.resolveRequest(context, moduleName, platform);
    } catch (error) {
      // Only the relative-import case for platform-suffixed private modules is
      // recovered. Any other failure is a real missing module and is rethrown.
      const fallback =
        platform === MARATHON_PLATFORM && moduleName.startsWith('.')
          ? privateOverride(context.originModulePath, moduleName, privateRoots)
          : null;
      if (fallback) return { type: 'sourceFile', filePath: fallback };
      throw error;
    }
    if (platform !== MARATHON_PLATFORM || resolution?.type !== 'sourceFile') {
      return resolution;
    }
    if (EXPO_POLYFILL_ENTRY.test(resolution.filePath) && fs.existsSync(expoPolyfill)) {
      return { type: 'sourceFile', filePath: expoPolyfill };
    }

    const marker = resolution.filePath.lastIndexOf(reactNativeLibraries);
    if (marker < 0) {
      return resolution;
    }

    const relative = resolution.filePath.slice(marker + reactNativeLibraries.length);
    for (const root of [marathonOverridesRoot, qnxOverridesRoot]) {
      const override = path.join(root, relative);
      if (override !== resolution.filePath && fs.existsSync(override)) {
        return { type: 'sourceFile', filePath: override };
      }
    }
    return resolution;
  };
}

/**
 * @param {string} projectRoot
 * @param {import('metro-config').InputConfigT} [overrides] Project settings are
 *   merged before adding the required Marathon package export condition.
 * @returns {import('metro-config').ConfigT}
 */
function getMarathonMetroConfig(projectRoot, overrides) {
  const defaultConfig = getDefaultConfig(projectRoot);
  const platforms = defaultConfig.resolver?.platforms ?? [];
  const platformConfig = {
    resolver: {
      resolveRequest: createMarathonResolveRequest(),
      platforms: platforms.includes(MARATHON_PLATFORM)
        ? platforms
        : [...platforms, MARATHON_PLATFORM],
    },
  };
  const merged = mergeConfig(defaultConfig, platformConfig);
  const config = overrides ? mergeConfig(merged, overrides) : merged;
  const conditions = config.resolver.unstable_conditionsByPlatform ?? {};
  // An explicit .native.js package export bypasses suffix resolution. Allow
  // installed packages to select a Qt entry without changing mobile or web
  // resolution. The package controls condition priority through export order.
  return {
    ...config,
    resolver: {
      ...config.resolver,
      unstable_conditionsByPlatform: {
        ...conditions,
        [MARATHON_PLATFORM]: [
          ...new Set([...(conditions[MARATHON_PLATFORM] ?? []), MARATHON_PLATFORM]),
        ],
      },
    },
  };
}

module.exports = {
  MARATHON_PLATFORM,
  createMarathonResolveRequest,
  getMarathonMetroConfig,
};
