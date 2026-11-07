'use strict';

const React = require('react');
const { createStore } = require('zustand/vanilla');
const { useStore } = require('zustand');
const AppRegistry = require('react-native/Libraries/ReactNative/AppRegistry').AppRegistry;
const View = require('react-native/Libraries/Components/View/View').default;
const TextInput = require('react-native/Libraries/Components/TextInput/TextInput').default;
const {
  Commands,
} = require('react-native/Libraries/Components/TextInput/RCTSingelineTextInputNativeComponent');

const initialText = 'A😀é';

function TextInputProbe() {
  const nativeRef = React.useRef(null);
  const secondNativeRef = React.useRef(null);
  const storeRef = React.useRef(null);
  if (storeRef.current === null) {
    storeRef.current = createStore(() => ({ value: undefined, visible: true }));
    globalThis.__rnqnxTextInputSnapshot = {
      text: initialText,
      eventCount: 0,
      changes: 0,
      changeTexts: 0,
      focus: 0,
      blur: 0,
      selection: { start: 0, end: 0 },
      selectionEvents: 0,
      keys: [],
    };
    globalThis.__rnqnxTextInputSecondSnapshot = {
      text: 'second',
      eventCount: 0,
      changes: 0,
      changeTexts: 0,
      focus: 0,
      blur: 0,
      selection: { start: 0, end: 0 },
      selectionEvents: 0,
      keys: [],
    };
  }
  const store = storeRef.current;
  const value = useStore(store, (state) => state.value);
  const visible = useStore(store, (state) => state.visible);
  const snapshot = globalThis.__rnqnxTextInputSnapshot;
  const secondSnapshot = globalThis.__rnqnxTextInputSecondSnapshot;
  const liveRef = () => {
    if (nativeRef.current === null) throw new Error('RNQNX_TEXT_INPUT_PROBE_UNMOUNTED');
    return nativeRef.current;
  };
  const liveSecondRef = () => {
    if (secondNativeRef.current === null) throw new Error('RNQNX_TEXT_INPUT_PROBE_UNMOUNTED');
    return secondNativeRef.current;
  };
  globalThis.__rnqnxTextInputFocusSecond = () => Commands.focus(liveSecondRef());
  globalThis.__rnqnxTextInputCommandSecond = (count, textOrNull, start, end) =>
    Commands.setTextAndSelection(liveSecondRef(), count, textOrNull, start, end);
  globalThis.__rnqnxTextInputCommand = (count, textOrNull, start, end) =>
    Commands.setTextAndSelection(liveRef(), count, textOrNull, start, end);
  globalThis.__rnqnxTextInputSetValue = (text) => store.setState({ value: text });
  globalThis.__rnqnxTextInputFocus = () => Commands.focus(liveRef());
  globalThis.__rnqnxTextInputBlur = () => Commands.blur(liveRef());
  globalThis.__rnqnxTextInputUnmount = () => store.setState({ visible: false });

  return React.createElement(
    View,
    { style: { width: 240, height: 120 }, collapsable: false },
    visible
      ? React.createElement(TextInput, {
          ref: nativeRef,
          style: { width: 240, height: 40, fontSize: 18 },
          defaultValue: initialText,
          value,
          onChange: (event) => {
            snapshot.text = event.nativeEvent.text;
            snapshot.eventCount = event.nativeEvent.eventCount;
            snapshot.changes += 1;
            if (store.getState().value !== undefined) {
              store.setState({ value: event.nativeEvent.text });
            }
          },
          onChangeText: (text) => {
            snapshot.textFromChangeText = text;
            snapshot.changeTexts += 1;
          },
          onKeyPress: (event) => snapshot.keys.push(event.nativeEvent.key),
          onFocus: (event) => {
            snapshot.focus += 1;
            snapshot.focusEventCount = event.nativeEvent.eventCount;
          },
          onBlur: (event) => {
            snapshot.blur += 1;
            snapshot.blurEventCount = event.nativeEvent.eventCount;
          },
          onSelectionChange: (event) => {
            snapshot.selection = event.nativeEvent.selection;
            snapshot.selectionEvents += 1;
            snapshot.selectionEventCount = event.nativeEvent.eventCount;
          },
        })
      : null,
    visible
      ? React.createElement(TextInput, {
          ref: secondNativeRef,
          style: { width: 240, height: 40, fontSize: 18 },
          defaultValue: 'second',
          onChange: (event) => {
            secondSnapshot.text = event.nativeEvent.text;
            secondSnapshot.eventCount = event.nativeEvent.eventCount;
            secondSnapshot.changes += 1;
          },
          onChangeText: (text) => {
            secondSnapshot.textFromChangeText = text;
            secondSnapshot.changeTexts += 1;
          },
          onKeyPress: (event) => secondSnapshot.keys.push(event.nativeEvent.key),
          onFocus: (event) => {
            secondSnapshot.focus += 1;
            secondSnapshot.focusEventCount = event.nativeEvent.eventCount;
          },
          onBlur: (event) => {
            secondSnapshot.blur += 1;
            secondSnapshot.blurEventCount = event.nativeEvent.eventCount;
          },
          onSelectionChange: (event) => {
            secondSnapshot.selection = event.nativeEvent.selection;
            secondSnapshot.selectionEvents += 1;
            secondSnapshot.selectionEventCount = event.nativeEvent.eventCount;
          },
        })
      : null
  );
}

AppRegistry.registerComponent('RNQnxTextInputProbe', () => TextInputProbe);
