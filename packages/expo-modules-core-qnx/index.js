'use strict';

const { requireNativeModule, requireOptionalNativeModule } = require('expo-modules-core');

function getQnxExpoPlatformModule() {
  return requireOptionalNativeModule('ExpoQnxPlatform');
}

/**
 * Strict native module acquisition for code that REQUIRES QNX platform APIs.
 * The Expo SDK 58 requireNativeModule API throws an informative error when the
 * JSI host did not register ExpoQnxPlatform. Do not conceal missing native
 * runtimes by resolving undefined or inventing a Web/Android fallback.
 */
function requireQnxExpoPlatformModule() {
  return requireNativeModule('ExpoQnxPlatform');
}

module.exports = {
  getQnxExpoPlatformModule,
  requireQnxExpoPlatformModule,
};
