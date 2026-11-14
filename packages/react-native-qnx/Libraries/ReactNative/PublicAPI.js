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
  // react-native-svg imports these at module load or in element render paths.
  get processColor() {
    return require('react-native/Libraries/StyleSheet/processColor').default;
  },
  get PanResponder() {
    return require('react-native/Libraries/Interaction/PanResponder').default;
  },
  get Easing() {
    return require('react-native/Libraries/Animated/Easing').default;
  },
  get TurboModuleRegistry() {
    // Named exports only; the fallback keeps the lazy-load contract uniform.
    const module = require('react-native/Libraries/TurboModule/TurboModuleRegistry');
    return module.default ?? module;
  },
  get codegenNativeComponent() {
    return require('react-native/Libraries/Utilities/codegenNativeComponent').default;
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
  // expo-modules-core and expo-constants import these (and
  // TurboModuleRegistry, above) from 'react-native'. Expo modules themselves
  // are found through globalThis.expo first.
  get NativeModules() {
    return require('react-native/Libraries/BatchedBridge/NativeModules').default;
  },
  get NativeEventEmitter() {
    return require('react-native/Libraries/EventEmitter/NativeEventEmitter').default;
  },
  // expo-router, its bundled react-navigation and @expo/log-box import these
  // from 'react-native'. Each is the real React Native module; a module whose
  // native half QNX lacks (Linking, Keyboard, AccessibilityInfo) degrades the
  // way React Native itself does when its TurboModule is absent.
  get LogBox() {
    return require('react-native/Libraries/LogBox/LogBox').default;
  },
  get Linking() {
    return require('react-native/Libraries/Linking/Linking').default;
  },
  get Keyboard() {
    return require('react-native/Libraries/Components/Keyboard/Keyboard').default;
  },
  get KeyboardAvoidingView() {
    return require('react-native/Libraries/Components/Keyboard/KeyboardAvoidingView').default;
  },
  get Appearance() {
    // Named exports only; the fallback keeps the lazy-load contract uniform.
    const module = require('react-native/Libraries/Utilities/Appearance');
    return module.default ?? module;
  },
  get useColorScheme() {
    return require('react-native/Libraries/Utilities/useColorScheme').default;
  },
  get AccessibilityInfo() {
    return require('react-native/Libraries/Components/AccessibilityInfo/AccessibilityInfo').default;
  },
  get UIManager() {
    return require('react-native/Libraries/ReactNative/UIManager').default;
  },
  get BackHandler() {
    return require('react-native/Libraries/Utilities/BackHandler').default;
  },
  get StatusBar() {
    return require('react-native/Libraries/Components/StatusBar/StatusBar').default;
  },
  get Alert() {
    return require('react-native/Libraries/Alert/Alert').default;
  },
  get LayoutAnimation() {
    return require('react-native/Libraries/LayoutAnimation/LayoutAnimation').default;
  },
  // expo-modules-core builds the view config of an Expo view with it.
  get NativeComponentRegistry() {
    const module = require('react-native/Libraries/NativeComponent/NativeComponentRegistry');
    return module.default ?? module;
  },
  // react-native-gesture-handler and Legend List read these when their module
  // loads. Gesture Handler's Android-only DrawerLayoutAndroid stays
  // unimplemented: it loads that export on first render of its own wrapper.
  get DeviceEventEmitter() {
    return require('react-native/Libraries/EventEmitter/RCTDeviceEventEmitter').default;
  },
  get RefreshControl() {
    return require('react-native/Libraries/Components/RefreshControl/RefreshControl').default;
  },
  get TouchableNativeFeedback() {
    return require('react-native/Libraries/Components/Touchable/TouchableNativeFeedback').default;
  },
  get TouchableOpacity() {
    return require('react-native/Libraries/Components/Touchable/TouchableOpacity').default;
  },
  get TouchableHighlight() {
    return require('react-native/Libraries/Components/Touchable/TouchableHighlight').default;
  },
  get TouchableWithoutFeedback() {
    return require('react-native/Libraries/Components/Touchable/TouchableWithoutFeedback').default;
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
