/**
 * React Native out-of-tree platform resolver for MarathonOS.
 *
 * It reuses the existing QNX Qt/Fabric React Native component overrides for
 * native views while giving Marathon a separate Platform identity. The
 * fallback is intentionally source-level: QNX and Linux do NOT share binaries.
 * This resolver is opt-in; it does not modify the qnx Metro configuration.
 */
'use strict';

const fs = require('node:fs');
const path = require('node:path');

const { getDefaultConfig, mergeConfig } = require('@react-native/metro-config');
const { OVERRIDES_ROOT: QNX_OVERRIDES_ROOT } = require('@rn-qnx/react-native-qnx');
const { OVERRIDES_ROOT: MARATHON_OVERRIDES_ROOT } = require('@rn-qnx/react-native-marathon');

const MARATHON_PLATFORM = 'marathon';

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

  return function resolveRequest(context, moduleName, platform) {
    // Honor upstream resolver; never replace a non-React Native resolution.
    // Fail if upstream resolution fails so missing modules are not fabricated.
    const resolution = context.resolveRequest(context, moduleName, platform);
    if (platform !== MARATHON_PLATFORM || resolution?.type !== 'sourceFile') {
      return resolution;
    }

    const marker = resolution.filePath.lastIndexOf(reactNativeLibraries);
    if (marker < 0) {
      return resolution;
    }

    const relative = resolution.filePath.slice(marker + reactNativeLibraries.length);
    // First prefer Marathon-specific JS, then source-compatible QNX React
    // overrides. No QNX Screen, ABI, native C++ or QNX binaries are reused.
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
