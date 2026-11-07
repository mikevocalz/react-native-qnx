/**
 * QNX `TextInput`.
 *
 * React Native's `Libraries/Components/TextInput/TextInput.js` branches on
 * `Platform.OS` and has no `qnx` branch, so an app writing `<TextInput>`
 * renders nothing. This override uses React Native's portable iOS single-line
 * textinput descriptor; it is the same C++ shadow node that the QNX platform
 * already mounts and applies props to.
 *
 * Deliberately minimal: `value`/`defaultValue` become the native `text` prop,
 * `onChange` is translated to `onChangeText`, and `mostRecentEventCount` is
 * tracked so a controlled field does not fight native edits.
 *
 * Gaps recorded in `docs/component-support.md` rather than stubbed here:
 * placeholder, multiline, focus/blur/submit/selection events and commands,
 * `focus`/`blur` imperative commands, and IME.
 */

'use strict';

const React = require('react');
const SinglelineTextInputNativeComponent =
  require('react-native/Libraries/Components/TextInput/RCTSingelineTextInputNativeComponent').default;

const forwardRef = React.forwardRef;
const { createStore } = require('zustand/vanilla');
const { useStore } = require('zustand');

const TextInput = forwardRef(function TextInput(props, ref) {
  const initialText = String(
    props.value !== undefined
      ? props.value
      : props.defaultValue !== undefined
        ? props.defaultValue
        : ''
  );
  if (
    globalThis.__RNQNX_TEXT_INPUT_CONTRACT__ === true &&
    props.secureTextEntry &&
    props.multiline
  ) {
    throw new Error('RNQNX_TEXT_INPUT_SECURE_MULTILINE_UNSUPPORTED');
  }
  const storeRef = React.useRef(null);
  if (storeRef.current === null) {
    storeRef.current = createStore(() => ({
      text: initialText,
      mostRecentEventCount: 0,
    }));
  }
  const store = storeRef.current;
  const text = useStore(store, (state) => state.text);
  const mostRecentEventCount = useStore(store, (state) => state.mostRecentEventCount);
  const currentText = props.value !== undefined ? String(props.value) : text;

  const handleChange = (event) => {
    const text = event.nativeEvent.text;
    const eventCount = event.nativeEvent.eventCount;
    if (props.value === undefined) {
      store.setState({ text, mostRecentEventCount: eventCount });
    } else {
      store.setState(function (prev) {
        return { text: prev.text, mostRecentEventCount: eventCount };
      });
    }
    if (props.onChange) {
      props.onChange(event);
    }
    if (props.onChangeText) {
      props.onChangeText(text);
    }
  };

  const passThrough = (handler) => (event) => {
    if (handler) {
      handler(event);
    }
  };

  return React.createElement(
    SinglelineTextInputNativeComponent,
    Object.assign({}, props, {
      ref,
      text: currentText,
      mostRecentEventCount,
      onChange: handleChange,
      onFocus: passThrough(props.onFocus),
      onBlur: passThrough(props.onBlur),
      onSelectionChange: passThrough(props.onSelectionChange),
      onContentSizeChange: passThrough(props.onContentSizeChange),
      onScroll: passThrough(props.onScroll),
      onKeyPress: passThrough(props.onKeyPress),
      onEndEditing: passThrough(props.onEndEditing),
      onSubmitEditing: passThrough(props.onSubmitEditing),
    })
  );
});

TextInput.displayName = 'TextInput';

module.exports = TextInput;
module.exports.default = TextInput;
