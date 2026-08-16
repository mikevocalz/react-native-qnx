/**
 * QNX `Switch`.
 *
 * React Native's `Libraries/Components/Switch/Switch.js` uses `useState` and
 * the iOS `SwitchCommands.setValue` command, both of which lower to
 * `@babel/runtime` helpers that are not installed in this workspace. This
 * override is a thin forwardRef wrapper around `SwitchNativeComponent` that
 * translates `trackColor`/`thumbColor` to the iOS native prop names and
 * dispatches `onValueChange` from the native `onChange` event.
 */

'use strict';

const React = require('react');
const SwitchNativeComponent =
  require('react-native/Libraries/Components/Switch/SwitchNativeComponent').default;

const forwardRef = React.forwardRef;

const Switch = forwardRef(function Switch(props, ref) {
  const trackColorForFalse = props.trackColor != null ? props.trackColor.false : null;
  const trackColorForTrue = props.trackColor != null ? props.trackColor.true : null;

  const handleChange = (event) => {
    if (props.onChange) {
      props.onChange(event);
    }
    if (props.onValueChange) {
      props.onValueChange(event.nativeEvent.value);
    }
  };

  return React.createElement(
    SwitchNativeComponent,
    Object.assign({}, props, {
      ref,
      disabled: props.disabled,
      onTintColor: trackColorForTrue,
      style: Object.assign({ alignSelf: 'flex-start' }, props.style),
      thumbTintColor: props.thumbColor,
      tintColor: trackColorForFalse,
      value: props.value === true,
      onChange: handleChange,
      onResponderTerminationRequest: () => false,
      onStartShouldSetResponder: () => true,
    })
  );
});

Switch.displayName = 'Switch';

module.exports = Switch;
module.exports.default = Switch;
