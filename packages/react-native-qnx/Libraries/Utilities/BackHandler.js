// The standard RN dispatcher provides reverse subscription order, event objects,
// consumption and removal semantics. Reuse it without pretending QNX is Android.
import NativeDeviceEventManager from 'react-native/Libraries/NativeModules/specs/NativeDeviceEventManager';
import BackHandler from 'react-native/Libraries/Utilities/BackHandler.android';

if (!NativeDeviceEventManager) {
  throw new Error(
    'React Native QNX BackHandler requires RNQNX_ENABLE_BACK_HANDLER=ON and an attached host window.'
  );
}
export default BackHandler;
