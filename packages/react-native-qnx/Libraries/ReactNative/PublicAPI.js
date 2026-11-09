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
  get Dimensions() {
    return require('react-native/Libraries/Utilities/Dimensions').default;
  },
  get useWindowDimensions() {
    return require('react-native/Libraries/Utilities/useWindowDimensions').default;
  },
  get PixelRatio() {
    return require('react-native/Libraries/Utilities/PixelRatio').default;
  },
  get I18nManager() {
    // VirtualizedList reads this through the public entry when a list mounts.
    // Reuse RN's actual optional-native-module fallback; do not invent RTL state.
    return require('react-native/Libraries/ReactNative/I18nManager').default;
  },
  get Animated() {
    return require('react-native/Libraries/Animated/Animated').default;
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
