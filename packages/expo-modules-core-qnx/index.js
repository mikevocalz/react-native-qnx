'use strict';

const { requireOptionalNativeModule } = require('expo-modules-core');

function getQnxExpoPlatformModule() {
  return requireOptionalNativeModule('ExpoQnxPlatform');
}

module.exports = {
  getQnxExpoPlatformModule,
};
