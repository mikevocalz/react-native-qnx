/**
 * `KeyboardAvoidingView` for QNX.
 *
 * QNX targets do not have a software keyboard or `Keyboard` native module, so
 * this is a minimal pass-through `View`. `behavior`, `keyboardVerticalOffset`,
 * `enabled`, and `contentContainerStyle` are accepted and ignored. Every other
 * `View` prop (`style`, `onLayout`, `testID`, accessibility props, `ref`) is
 * forwarded.
 *
 * This file sits at the same path as React Native's own
 * `Libraries/Components/Keyboard/KeyboardAvoidingView.js`, which is what makes
 * the QNX Metro resolver select it. tests/js/override-paths-match-upstream
 * fails if the path drifts.
 */

'use strict';

const React = require('react');
const View = require('react-native/Libraries/Components/View/View').default;

function KeyboardAvoidingView(props) {
  const {
    children,
    // Accepted for API compatibility; no software keyboard exists to avoid.
    behavior: _behavior,
    keyboardVerticalOffset: _keyboardVerticalOffset,
    enabled: _enabled,
    contentContainerStyle: _contentContainerStyle,
    ...viewProps
  } = props;
  return React.createElement(View, viewProps, children);
}

module.exports = KeyboardAvoidingView;
module.exports.default = KeyboardAvoidingView;
