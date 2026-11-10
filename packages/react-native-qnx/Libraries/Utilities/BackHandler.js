'use strict';

// Reuse the pinned RN implementation for event objects, LIFO handling,
// duplicate subscriptions and removal. Its native contract is supplied by Qt;
// this does not change Platform.OS or use an Android runtime.
const BackHandler = require('react-native/Libraries/Utilities/BackHandler.android').default;
const Registry = require('react-native/Libraries/TurboModule/TurboModuleRegistry');

function requireProvider() {
  if (!Registry.get('DeviceEventManager')) {
    throw new Error('BackHandler requires RNQNX_ENABLE_PLATFORM_SERVICES in the Qt host.');
  }
}

const QtBackHandler = {
  exitApp() {
    requireProvider();
    BackHandler.exitApp();
  },
  addEventListener(eventName, handler) {
    requireProvider();
    if (!['hardwareBackPress', 'backPress'].includes(eventName)) {
      throw new Error(`Unsupported BackHandler event "${eventName}".`);
    }
    if (typeof handler !== 'function')
      throw new TypeError('BackHandler listener must be a function.');
    return BackHandler.addEventListener(eventName, handler);
  },
};

module.exports = QtBackHandler;
module.exports.default = QtBackHandler;
