// Replaces expo-router's bottom-tabs/views/ScreenFallback for the qnx
// platform. Without react-native-screens that module drew every tab as a plain
// View and dropped its `active` state, so the four inactive tabs stayed laid
// out, stacked under the focused one at full container size. They measured
// themselves, reported layouts, and took part in hit testing for nothing. An
// inactive tab (activity state 0) is now `display: none`: Yoga skips it, it
// reports no layout, and it receives no touches. It stays mounted, so its
// state survives a switch, and it lays out again when it is focused.
const React = require('react');
const { View } = require('react-native');

const STATE_INACTIVE = 0;

// The tab container is a plain View: `enabled` and `hasTwoStates` only mean
// something to react-native-screens.
function MaybeScreenContainer({ enabled: _enabled, hasTwoStates: _hasTwoStates, ...rest }) {
  return React.createElement(View, rest);
}

function MaybeScreen({ enabled: _enabled, active, style, ...rest }) {
  const inactive = active === STATE_INACTIVE;
  return React.createElement(View, {
    ...rest,
    style: inactive ? [style, { display: 'none' }] : style,
    pointerEvents: inactive ? 'none' : rest.pointerEvents,
  });
}

module.exports = { MaybeScreen, MaybeScreenContainer };
