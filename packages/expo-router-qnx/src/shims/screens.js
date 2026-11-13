// react-native-screens for QNX. A desktop window needs no native screen
// containers, so screensEnabled() is false and expo-router's navigators fall
// back to plain Views (their ScreenFallback modules check this first). The
// other exports exist so the optional-library probes do not throw.
const React = require('react');
const { View } = require('react-native');

const featureFlags = { experiment: {}, stable: {} };
const compatibilityFlags = {};

function passthrough(props) {
  return React.createElement(View, props);
}

module.exports = {
  screensEnabled: () => false,
  enableScreens: () => {},
  enableFreeze: () => {},
  freezeEnabled: () => false,
  featureFlags,
  compatibilityFlags,
  isSearchBarAvailableForCurrentPlatform: false,
  Screen: passthrough,
  ScreenContainer: passthrough,
  ScreenStack: passthrough,
  ScreenStackItem: passthrough,
  ScreenFooter: passthrough,
  ScreenStackHeaderLeftView: passthrough,
  ScreenStackHeaderRightView: passthrough,
  ScreenStackHeaderCenterView: passthrough,
  ScreenStackHeaderBackButtonImage: passthrough,
  ScreenStackHeaderSearchBarView: passthrough,
  SearchBar: passthrough,
  Tabs: undefined,
  Stack: undefined,
  Split: undefined,
};
