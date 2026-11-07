/**
 * QNX `TextInput`.
 *
 * React Native's `Libraries/Components/TextInput/TextInput.js` branches on
 * `Platform.OS` and has no `qnx` branch, so an app writing `<TextInput>`
 * renders nothing. This override uses React Native's portable iOS single-line
 * textinput descriptor; it is the same C++ shadow node that the QNX platform
 * already mounts and applies props to.
 *
 * The override forwards standard text, focus, submit and selection events and
 * commands. Each component retains a vanilla Zustand store for text/edit count;
 * native handles stay in refs.
 *
 * The default native path remains available. With the experimental default-OFF
 * TextInput contract enabled, the Qt bridge owns edit tokens, rejects stale
 * text/selection commands and suppresses JS-write feedback. Secure multiline
 * and secure input-method provider routes are unsupported and rejected.
 * Hardware IME and accessibility qualification remain separate validation work.
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
