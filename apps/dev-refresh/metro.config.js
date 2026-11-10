const path = require('node:path');
const { createRequire } = require('node:module');
const { getQnxMetroConfig } = require('../../packages/metro-config-qnx/src/index.js');
const root = path.resolve(__dirname, '../..');
const rnRequire = createRequire(require.resolve('react-native/package.json'));
const metroRuntime = path.dirname(rnRequire.resolve('metro-runtime/package.json'));
const babelRuntime = path.dirname(rnRequire.resolve('@babel/runtime/package.json'));
const bootstrap = path.join(root, 'packages/react-native-qnx/dev/initialize.js');

module.exports = getQnxMetroConfig(__dirname, {
  watchFolders: [root],
  resolver: {
    extraNodeModules: { 'metro-runtime': metroRuntime, '@babel/runtime': babelRuntime },
    nodeModulesPaths: [path.join(root, 'node_modules')],
  },
  serializer: {
    getModulesRunBeforeMainModule: () => [bootstrap],
  },
});
