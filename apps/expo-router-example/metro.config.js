const path = require('node:path');
const { createRequire } = require('node:module');
const { getQnxMetroConfig } = require('@rn-qnx/metro-config-qnx');
const withPublicEntry = require('@rn-qnx/metro-config-qnx/public-entry');
const { withExpoRouterQnx } = require('@rn-qnx/expo-router-qnx/metro');
const workspaceRoot = path.resolve(__dirname, '../..');
const rnRequire = createRequire(require.resolve('react-native/package.json'));
const babelRuntimeRoot = path.dirname(rnRequire.resolve('@babel/runtime/package.json'));

module.exports = withExpoRouterQnx(
  withPublicEntry(
    getQnxMetroConfig(__dirname, {
      watchFolders: [workspaceRoot],
      transformer: { unstable_allowRequireContext: true },
      resolver: {
        extraNodeModules: { '@babel/runtime': babelRuntimeRoot },
        nodeModulesPaths: [
          path.join(__dirname, 'node_modules'),
          path.join(workspaceRoot, 'node_modules'),
        ],
        disableHierarchicalLookup: false,
      },
    }),
    'qnx'
  ),
  { routerContext: path.join(__dirname, 'app-ctx.js') }
);
