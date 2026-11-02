'use strict';

// Nitro v1 places the public API and the global proxy contract in nitro-core.
// Importing react-native-nitro-modules would also load its Android/iOS Turbo
// module path, which is not a valid QNX install route.
const { NitroModules } = require('nitro-core');

function createQnxPlatformInfo() {
  const QnxPlatformInfo = NitroModules.getHybridClass('QnxPlatformInfo');
  return new QnxPlatformInfo();
}

module.exports = { createQnxPlatformInfo };
