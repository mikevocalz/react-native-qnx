const path = require('node:path');
const { createRequire } = require('node:module');
const withPublicEntry = require('../../packages/metro-config-qnx/public-entry');
const { getMarathonMetroConfig } = require('../../packages/metro-config-marathon/src');
const rnRequire = createRequire(require.resolve('react-native/package.json'));
const workspaceRoot = path.resolve(__dirname, '..', '..');

// This source fixture does not declare a separate Marathon package importer.
// Resolve the existing workspace factory without changing the locked graph.
module.exports = withPublicEntry(
  getMarathonMetroConfig(__dirname, {
    watchFolders: [workspaceRoot],
    resolver: {
      extraNodeModules: {
        '@babel/runtime': path.dirname(rnRequire.resolve('@babel/runtime/package.json')),
      },
      nodeModulesPaths: [
        path.join(__dirname, 'node_modules'),
        path.join(workspaceRoot, 'node_modules'),
      ],
      disableHierarchicalLookup: false,
    },
  }),
  'marathon'
);
