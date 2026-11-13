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
  const reactNativeLibraries = `${path.sep}react-native${path.sep}Libraries${path.sep}`;
  const reactNativeRoot = `${path.sep}react-native${path.sep}`;
  // `src/private` sits beside `Libraries` in both React Native and the platform
  // package.
  const privateRoot = path.join(path.dirname(overridesRoot), 'src', 'private');

  return function resolveRequest(context, moduleName, platform) {
    // Metro's own resolver runs first; the override is decided by where that
    // landed, which is what makes relative imports such as `./Platform` work.
    let resolution;
    try {
      resolution = context.resolveRequest(context, moduleName, platform);
    } catch (error) {
      // Some React Native development modules exist only as `.ios.js` and
      // `.android.js` (src/private/devsupport/rndevtools/ReactDevToolsSettingsManager
      // in 0.88). A relative import of one from inside React Native fails for
      // any other platform; the platform package supplies the QNX file.
      const fallback =
        platform === QNX_PLATFORM && moduleName.startsWith('.')
          ? privateOverride(context.originModulePath, moduleName, reactNativeRoot, privateRoot)
          : null;
      if (fallback) return { type: 'sourceFile', filePath: fallback };
      throw error;
    }

    if (platform !== QNX_PLATFORM || resolution?.type !== 'sourceFile') {
      return resolution;
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
 * The platform package's copy of a React Native `src/private` module that
 * Metro could not resolve, or null.
 *
 * @param {string} origin Absolute path of the importing module.
 * @param {string} moduleName Relative import as written.
 * @param {string} reactNativeRoot `/react-native/` with platform separators.
 * @param {string} privateRoot The platform package's `src/private` directory.
 */
function privateOverride(origin, moduleName, reactNativeRoot, privateRoot) {
  const target = path.resolve(path.dirname(origin), moduleName);
  const marker = target.lastIndexOf(reactNativeRoot);
  if (marker === -1) return null;
  const relative = target.slice(marker + reactNativeRoot.length);
  const prefix = `src${path.sep}private${path.sep}`;
  if (!relative.startsWith(prefix)) return null;
  const candidate = path.join(privateRoot, `${relative.slice(prefix.length)}.js`);
  return fs.existsSync(candidate) ? candidate : null;
}

/**
 * Returns a Metro config for `projectRoot` with the QNX platform registered.
 *
 * @param {string} projectRoot Absolute path to the app.
 * @param {import('metro-config').InputConfigT} [overrides] Project settings are
 *   merged before adding the required QNX package export condition.
 * @returns {import('metro-config').ConfigT}
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
  const config = overrides ? mergeConfig(merged, overrides) : merged;
  const conditions = config.resolver.unstable_conditionsByPlatform ?? {};
  // Explicit package exports take precedence over platform file extensions.
  // Assert only this target's condition, preserving React Native's conditions
  // and every other platform. Packages must list qnx before react-native in
  // their exports objects when they provide both implementations.
  return {
    ...config,
    resolver: {
      ...config.resolver,
      unstable_conditionsByPlatform: {
        ...conditions,
        [QNX_PLATFORM]: [...new Set([...(conditions[QNX_PLATFORM] ?? []), QNX_PLATFORM])],
      },
    },
  };
}

module.exports = { getQnxMetroConfig, createQnxResolveRequest, QNX_PLATFORM };
