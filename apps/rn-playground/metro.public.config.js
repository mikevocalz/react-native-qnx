const path = require('node:path');
const { createRequire } = require('node:module');
const withPublicEntry = require('../../packages/metro-config-qnx/public-entry');
const base = require('./metro.config');
const rnRequire = createRequire(require.resolve('react-native/package.json'));

module.exports = withPublicEntry(
  {
    ...base,
    resolver: {
      ...base.resolver,
      extraNodeModules: {
        ...base.resolver?.extraNodeModules,
        '@babel/runtime': path.dirname(rnRequire.resolve('@babel/runtime/package.json')),
      },
    },
  },
  'qnx'
);
