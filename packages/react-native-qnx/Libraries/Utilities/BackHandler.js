// The standard RN dispatcher provides reverse subscription order, event objects,
// consumption and removal semantics. Reuse it without pretending QNX is Android.
const NativeDeviceEventManager =
  require('react-native/Libraries/NativeModules/specs/NativeDeviceEventManager').default;
const BackHandler = require('react-native/Libraries/Utilities/BackHandler.android').default;

if (!NativeDeviceEventManager) {
  throw new Error(
    'React Native QNX BackHandler requires RNQNX_ENABLE_BACK_HANDLER=ON and an attached host window.'
  );
}
module.exports = BackHandler;
module.exports.default = BackHandler;
