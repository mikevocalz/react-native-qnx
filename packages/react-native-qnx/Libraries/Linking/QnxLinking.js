const NativeLinkingManager = require('react-native/Libraries/Linking/NativeLinkingManager').default;
// Public RN imports must remain safe when this experimental feature is OFF.
// Every attempted operation still fails explicitly; no fake capability/result.
const unavailable = () => {
  throw new Error('React Native QNX Linking requires RNQNX_ENABLE_LINKING=ON.');
};
const Linking = NativeLinkingManager
  ? require('react-native/Libraries/Linking/Linking').default
  : {
      addEventListener: unavailable,
      getInitialURL: unavailable,
      openURL: unavailable,
      canOpenURL: unavailable,
      openSettings: unavailable,
      sendIntent: unavailable,
      addListener: unavailable,
      removeAllListeners: unavailable,
      listenerCount: unavailable,
      emit: unavailable,
    };
// Preserve RN's Promise, validation and additive subscription behavior when ON.
module.exports = Linking;
module.exports.default = Linking;
