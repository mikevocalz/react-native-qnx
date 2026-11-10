// The standard RN dispatcher provides reverse subscription order, event objects,
// consumption and removal semantics. Reuse it without pretending QNX is Android.
const NativeDeviceEventManager =
  require('react-native/Libraries/NativeModules/specs/NativeDeviceEventManager').default;
// Importing RN components can read BackHandler transitively even when no
// navigation is used. Reject operations, not that ordinary module import.
const unavailable = () => {
  throw new Error(
    'React Native QNX BackHandler requires RNQNX_ENABLE_BACK_HANDLER=ON and an attached host window.'
  );
};
const BackHandler = NativeDeviceEventManager
  ? require('react-native/Libraries/Utilities/BackHandler.android').default
  : { addEventListener: unavailable, exitApp: unavailable };
module.exports = BackHandler;
module.exports.default = BackHandler;
