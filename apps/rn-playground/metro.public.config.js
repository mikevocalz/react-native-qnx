const withPublicEntry = require('../../packages/metro-config-qnx/public-entry');
module.exports = withPublicEntry(require('./metro.config'), 'qnx');
