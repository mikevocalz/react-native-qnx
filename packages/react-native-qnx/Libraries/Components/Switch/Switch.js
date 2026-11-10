/**
 * QNX `Switch`.
 *
 * Uses RN 0.88's Switch command to restore the controlled value after a
 * rejected native change. Plain array indexing avoids Babel's destructuring
 * helper in the QNX override.
 */

'use strict';

const React = require('react');
const NativeSwitch = require('react-native/Libraries/Components/Switch/SwitchNativeComponent');
const SwitchNativeComponent = NativeSwitch.default;
const useMergeRefs = require('react-native/Libraries/Utilities/useMergeRefs').default;

const forwardRef = React.forwardRef;

const Switch = forwardRef(function Switch(props, ref) {
  const nativeRef = React.useRef(null);
  const mergedRef = useMergeRefs(nativeRef, ref);
  // A fresh object also reconciles repeated proposals of the same value.
  const nativeState = React.useState({ value: null });
  const native = nativeState[0];
  const setNative = nativeState[1];
  const trackColorForFalse = props.trackColor != null ? props.trackColor.false : null;
  const trackColorForTrue = props.trackColor != null ? props.trackColor.true : null;

  const handleChange = (event) => {
    if (props.onChange) {
      props.onChange(event);
    }
    if (props.onValueChange) {
      props.onValueChange(event.nativeEvent.value);
    }
    setNative({ value: event.nativeEvent.value });
  };

  React.useLayoutEffect(() => {
    const jsValue = props.value === true;
    if (native.value !== null && native.value !== jsValue && nativeRef.current != null) {
      NativeSwitch.Commands.setValue(nativeRef.current, jsValue);
    }
  }, [props.value, native]);

  return React.createElement(
    SwitchNativeComponent,
    Object.assign({}, props, {
      ref: mergedRef,
      disabled: props.disabled,
      onTintColor: trackColorForTrue,
      style: [{ alignSelf: 'flex-start' }, props.style],
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
