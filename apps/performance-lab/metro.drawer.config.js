// drawer-drag renders the shipped JsDrawer, which imports the public
// `react-native` entry. The other two workloads keep the plain config so their
// bundles and baselines do not change.
const withPublicEntry = require('../../packages/metro-config-qnx/public-entry');
const base = require('./metro.config');

module.exports = withPublicEntry(base, 'qnx');
