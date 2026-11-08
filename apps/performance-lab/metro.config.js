const path = require('node:path');
const { createRequire } = require('node:module');
const { getQnxMetroConfig } = require('../../packages/metro-config-qnx/src/index.js');

const workspaceRoot = path.resolve(__dirname, '..', '..');
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
