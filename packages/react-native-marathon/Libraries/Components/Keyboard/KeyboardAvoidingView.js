/**
 * `KeyboardAvoidingView` for Marathon.
 *
 * Marathon-Shell owns the keyboard (https://github.com/patrickjquinn/Marathon-Shell)
 * and the host exposes no keyboard frame events to JavaScript, so this view
 * cannot compute an offset and does not pretend to. It renders a `View` and
 * forwards every view prop, including `ref`, `testID`, accessibility props and
 * `onLayout`. The avoidance props (`behavior`, `keyboardVerticalOffset`,
 * `enabled`, `contentContainerStyle`) are consumed and have no effect.
 *
 * This file sits at the same path as React Native's own
 * `Libraries/Components/Keyboard/KeyboardAvoidingView.js`, which is what makes
 * the Marathon Metro resolver select it. tests/js/override-paths-match-upstream
 * fails if the path drifts.
 */
'use strict';

const React = require('react');
const View = require('react-native/Libraries/Components/View/View').default;

function KeyboardAvoidingView(props) {
  const {
    behavior: _behavior,
    keyboardVerticalOffset: _keyboardVerticalOffset,
    enabled: _enabled,
    contentContainerStyle: _contentContainerStyle,
    children,
    ...viewProps
  } = props;
  return React.createElement(View, viewProps, children);
}

module.exports = KeyboardAvoidingView;
module.exports.default = KeyboardAvoidingView;
