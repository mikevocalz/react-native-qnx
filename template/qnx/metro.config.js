const path = require('node:path');

const { getQnxMetroConfig } = require('@rn-qnx/metro-config-qnx');

const workspaceRoot = path.resolve(__dirname, '..', '..');

// pnpm keeps real package files under <root>/node_modules/.pnpm and links them
// into each workspace package, so Metro has to watch the workspace root and be
// told both module directories. Without this it cannot resolve `react` even
// though the symlink exists next to this app.
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
