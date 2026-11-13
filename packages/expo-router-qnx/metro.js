// Node-side half: runs in metro.config.js. The app-side half is ./src/index.js.
const { createExpoRouterResolveRequest } = require('./src/resolver');

/**
 * Adds the expo-router resolver to a Metro config made by getQnxMetroConfig.
 *
 * @param {import('metro-config').ConfigT} config
 * @param {{ routerContext?: string }} [options]
 */
function withExpoRouterQnx(config, options) {
  const next = config.resolver.resolveRequest;
  return {
    ...config,
    resolver: {
      ...config.resolver,
      resolveRequest: createExpoRouterResolveRequest(next, options),
    },
  };
}

module.exports = { withExpoRouterQnx };
