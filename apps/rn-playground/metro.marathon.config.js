const path = require('node:path');
const { getMarathonMetroConfig } = require('@rn-qnx/metro-config-marathon');

const workspaceRoot = path.resolve(__dirname, '..', '..');

// Kept separate from metro.config.js so QNX builds keep exactly the original
// resolver, bundle identity, module overrides and dependency graph.
module.exports = getMarathonMetroConfig(__dirname, {
  watchFolders: [workspaceRoot],
  resolver: {
    nodeModulesPaths: [
      path.join(__dirname, 'node_modules'),
      path.join(workspaceRoot, 'node_modules'),
    ],
    disableHierarchicalLookup: false,
  },
});
