'use strict';

// Opt-in public entry for the existing Qt component backend. These getters
// expose actual implementations, not UI stubs or a second renderer.
const api = Object.freeze({
  get AppRegistry() {
    return require('react-native/Libraries/ReactNative/AppRegistry').AppRegistry;
  },
  get AppState() {
    return require('react-native/Libraries/AppState/AppState').default;
  },
  get View() {
    return require('react-native/Libraries/Components/View/View').default;
  },
  get Text() {
    return require('react-native/Libraries/Text/Text').default;
  },
  get Image() {
    return require('react-native/Libraries/Image/Image').default;
  },
  get ScrollView() {
    return require('react-native/Libraries/Components/ScrollView/ScrollView').default;
  },
  get FlatList() {
    return require('react-native/Libraries/Lists/FlatList').default;
  },
  get SectionList() {
    return require('react-native/Libraries/Lists/SectionList').default;
  },
  get TextInput() {
    return require('react-native/Libraries/Components/TextInput/TextInput').default;
  },
  get Pressable() {
    return require('react-native/Libraries/Components/Pressable/Pressable').default;
  },
  get ActivityIndicator() {
    return require('react-native/Libraries/Components/ActivityIndicator/ActivityIndicator').default;
  },
  get Switch() {
    return require('react-native/Libraries/Components/Switch/Switch').default;
  },
  get Modal() {
    return require('react-native/Libraries/Modal/Modal').default;
  },
  get StyleSheet() {
    return require('react-native/Libraries/StyleSheet/StyleSheet').default;
  },
  get Platform() {
    return require('react-native/Libraries/Utilities/Platform').default;
  },
  get Animated() {
    return require('react-native/Libraries/Animated/Animated').default;
  },
  // FlatList and SectionList (@react-native/virtualized-lists) import these
  // from 'react-native'; without them the Documents and Sections pages threw.
  get I18nManager() {
    return require('react-native/Libraries/ReactNative/I18nManager').default;
  },
  get findNodeHandle() {
    return require('react-native/Libraries/ReactNative/RendererProxy').findNodeHandle;
  },
  // Backed by the host's DeviceInfo module (QnxDeviceInfo).
  get Dimensions() {
    return require('react-native/Libraries/Utilities/Dimensions').default;
  },
  get PixelRatio() {
    return require('react-native/Libraries/Utilities/PixelRatio').default;
  },
  get useWindowDimensions() {
    return require('react-native/Libraries/Utilities/useWindowDimensions').default;
  },
  // Backed by the development client's DevSettings module; React Native makes
  // it a no-op in release bundles.
  get DevSettings() {
    return require('react-native/Libraries/Utilities/DevSettings').default;
  },
  // expo-modules-core and expo-constants import these from 'react-native'.
  // Expo modules themselves are found through globalThis.expo first.
  get NativeModules() {
    return require('react-native/Libraries/BatchedBridge/NativeModules').default;
  },
  get TurboModuleRegistry() {
    return require('react-native/Libraries/TurboModule/TurboModuleRegistry');
  },
  get NativeEventEmitter() {
    return require('react-native/Libraries/EventEmitter/NativeEventEmitter').default;
  },
});

module.exports = new Proxy(api, {
  get(target, name, receiver) {
    // CommonJS interop and promise introspection are metadata, not RN exports.
    if (name === '__esModule' || name === 'default' || name === 'then') return undefined;
    if (typeof name === 'string' && !Object.prototype.hasOwnProperty.call(target, name)) {
      throw new Error(
        `React Native QNX public export "${name}" is not implemented in this experimental entry.`
      );
    }
    return Reflect.get(target, name, receiver);
  },
});
