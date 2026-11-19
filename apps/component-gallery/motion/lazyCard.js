// Turns one ./motion card into an Advanced-tab card (components/advanced/).
// The card module is required on first render, not at import, so a build
// without the native Worklets module (RNQNX_ENABLE_WORKLETS=OFF) never
// evaluates react-native-worklets and shows a notice instead. The cards draw
// their own frame and title, hence `framed: false`.
const React = require('react');
const { Text, TurboModuleRegistry, View } = require('react-native');

function nativeWorkletsAvailable() {
  return TurboModuleRegistry.get('WorkletsModule') != null;
}

function motionCard({ id, title, detail, order, needsWorklets, load }) {
  function MotionAdvancedCard() {
    if (needsWorklets && !nativeWorkletsAvailable()) {
      return React.createElement(
        View,
        { style: { backgroundColor: '#142538', borderRadius: 12, padding: 16, marginTop: 16 } },
        React.createElement(Text, { style: { color: '#e9f0f7', fontSize: 16 } }, title),
        React.createElement(
          Text,
          { style: { color: '#9dafc2', fontSize: 13, marginTop: 6 } },
          'This build has no native Worklets module. Configure with RNQNX_ENABLE_WORKLETS=ON.'
        )
      );
    }
    return React.createElement(load());
  }
  return { id, title, detail, order, framed: false, Component: MotionAdvancedCard };
}

module.exports = { motionCard };
