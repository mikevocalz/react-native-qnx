'use strict';

const path = require('node:path');
const { OVERRIDES_ROOT } = require('@rn-qnx/react-native-qnx');

/** Explicit opt-in: preserve the supplied resolver for every other request.
 * @param {import('metro-config').InputConfigT} config
 * @param {'qnx'|'marathon'} [platform]
 * @returns {import('metro-config').InputConfigT} */
function withPublicEntry(config, platform = 'qnx') {
  if (platform !== 'qnx' && platform !== 'marathon') {
    throw new Error('Public React Native entry requires qnx or marathon.');
  }
  const resolver = config.resolver ?? {};
  const fallback = resolver.resolveRequest;
  const initializer = path.join(OVERRIDES_ROOT, 'Core/InitializeCore.js');
  const serializer = config.serializer ?? {};
  const preload = serializer.getModulesRunBeforeMainModule;
  return {
    ...config,
    serializer: {
      ...serializer,
      getModulesRunBeforeMainModule(entry) {
        // Metro executes only preloads already present in the current graph.
        // Keep the caller's preloads for iOS/Android; our target resolver makes
        // this initializer reachable only in the opted-in platform graph.
        return [...new Set([...(preload ? preload(entry) : []), initializer])];
      },
    },
    resolver: {
      ...resolver,
      resolveRequest(context, moduleName, requestedPlatform) {
        if (requestedPlatform === platform && moduleName === 'react-native/setup-env') {
          return {
            type: 'sourceFile',
            filePath: initializer,
          };
        }
        if (requestedPlatform === platform && moduleName === 'react-native') {
          return {
            type: 'sourceFile',
            filePath: path.join(OVERRIDES_ROOT, 'ReactNative/PublicAPI.js'),
          };
        }
        return fallback
          ? fallback(context, moduleName, requestedPlatform)
          : context.resolveRequest(context, moduleName, requestedPlatform);
      },
    },
  };
}

module.exports = withPublicEntry;
