const path = require('node:path');
const { createRequire } = require('node:module');

const { getQnxMetroConfig } = require('@rn-qnx/metro-config-qnx');
const withPublicEntry = require('@rn-qnx/metro-config-qnx/public-entry');

// Resolve Babel's injected helpers from the pinned React Native dependency,
// not an accidentally hoisted package in an existing developer checkout.
const rnRequire = createRequire(require.resolve('react-native/package.json'));
const babelRuntimeRoot = path.dirname(rnRequire.resolve('@babel/runtime/package.json'));

const workspaceRoot = path.resolve(__dirname, '..', '..');

// pnpm keeps real package files under <root>/node_modules/.pnpm and links them
// into each workspace package, so Metro has to watch the workspace root and be
// told both module directories. Without this it cannot resolve `react` even
// though the symlink exists next to this app.
module.exports = withPublicEntry(
  getQnxMetroConfig(__dirname, {
    watchFolders: [workspaceRoot],
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
);
