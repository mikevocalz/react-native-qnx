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

const { getDefaultConfig, mergeConfig } = require('@react-native/metro-config');

const QNX_PLATFORM = 'qnx';

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
  const qnxConfig = {
    resolver: {
      // Order matters to Metro only in that the requested platform is tried
      // first; `qnx` is appended rather than replacing the list so a shared
      // codebase can still resolve its other platforms.
      platforms: platforms.includes(QNX_PLATFORM) ? platforms : [...platforms, QNX_PLATFORM],
    },
  };

  const merged = mergeConfig(defaultConfig, qnxConfig);
  return overrides ? mergeConfig(merged, overrides) : merged;
}

module.exports = { getQnxMetroConfig, QNX_PLATFORM };
