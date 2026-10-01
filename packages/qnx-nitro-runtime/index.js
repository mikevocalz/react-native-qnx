'use strict';

const { NitroModules } = require('react-native-nitro-modules');

function createQnxPlatformInfo() {
  return NitroModules.createHybridObject('QnxPlatformInfo');
}

module.exports = {
  createQnxPlatformInfo,
};
