const path = require('node:path');
const { createRequire } = require('node:module');

// This is a source-checkout fixture, not an npm-published package. Reuse the
// audited workspace config directly to avoid adding a lockfile importer.
const { getQnxMetroConfig } = require('../../packages/metro-config-qnx/src/index.js');
const workspaceRoot = path.resolve(__dirname, '..', '..');
// pnpm does not install dependencies directly inside scaffold-only apps.
// Babel helpers generated for this fixture must resolve to the exact copy
// already owned by the pinned React Native package, not a new version.
const rnRequire = createRequire(require.resolve('react-native/package.json'));
const babelRuntimeRoot = path.dirname(rnRequire.resolve('@babel/runtime/package.json'));

module.exports = getQnxMetroConfig(__dirname, {
  watchFolders: [workspaceRoot],
  resolver: {
    extraNodeModules: { '@babel/runtime': babelRuntimeRoot },
    nodeModulesPaths: [
      path.join(__dirname, 'node_modules'),
      path.join(workspaceRoot, 'node_modules'),
    ],
    disableHierarchicalLookup: false,
  },
});
