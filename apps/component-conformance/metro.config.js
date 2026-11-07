const path = require('node:path');

// This is a source-checkout fixture, not an npm-published package. Reuse the
// audited workspace config directly to avoid adding a lockfile importer.
const { getQnxMetroConfig } = require('../../packages/metro-config-qnx/src/index.js');
const workspaceRoot = path.resolve(__dirname, '..', '..');

module.exports = getQnxMetroConfig(__dirname, {
  watchFolders: [workspaceRoot],
  resolver: {
    nodeModulesPaths: [
      path.join(__dirname, 'node_modules'),
      path.join(workspaceRoot, 'node_modules'),
    ],
    disableHierarchicalLookup: false,
  },
});
