/**
 * `KeyboardAvoidingView` for QNX.
 *
 * QNX targets do not have a software keyboard or `Keyboard` native module, so
 * this is a minimal pass-through `View`. `behavior`, `keyboardVerticalOffset`,
 * `enabled`, and `contentContainerStyle` are accepted and ignored.
 */

'use strict';

const React = require('react');
const View = require('react-native/Libraries/Components/View/View').default;

function KeyboardAvoidingView(props) {
  const { children, style } = props;
  return React.createElement(View, { style }, children);
}

module.exports = KeyboardAvoidingView;
module.exports.default = KeyboardAvoidingView;
