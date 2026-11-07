'use strict';

const React = require('react');
const { createStore } = require('zustand/vanilla');
const { useStore } = require('zustand');
const AppRegistry = require('react-native/Libraries/ReactNative/AppRegistry').AppRegistry;
const View = require('react-native/Libraries/Components/View/View').default;

const modes = new Set([
  'registered',
  'unregistered',
  'unsupported',
  'no-handler',
  'semantic-disabled',
  'inaccessible',
]);

globalThis.__rnqnxAccessibilitySnapshot = { count: 0, lastAction: null, events: 0 };

function ActionTarget({ mode }) {
  const storeRef = React.useRef(null);
  if (storeRef.current === null) {
    storeRef.current = createStore(() => ({ count: 0 }));
  }
  const store = storeRef.current;
  const count = useStore(store, (state) => state.count);
  const onAccessibilityAction = (event) => {
    const lastAction = event.nativeEvent.actionName;
    const nextCount = store.getState().count + 1;
    store.setState({ count: nextCount });
    const snapshot = globalThis.__rnqnxAccessibilitySnapshot;
    snapshot.count = nextCount;
    snapshot.lastAction = lastAction;
    snapshot.events += 1;
  };

  return React.createElement(View, {
    style: { width: 200, height: 80 },
    collapsable: false,
    accessible: mode !== 'inaccessible',
    accessibilityRole: 'button',
    accessibilityLabel: `Activation count ${count} mode ${mode}`,
    accessibilityState: { disabled: mode === 'semantic-disabled' },
    accessibilityActions:
      mode === 'unregistered'
        ? []
        : [{ name: mode === 'unsupported' ? 'unsupported-fixture-action' : 'activate' }],
    onAccessibilityAction: mode === 'no-handler' ? undefined : onAccessibilityAction,
  });
}

function AccessibilityActionProbe() {
  const storeRef = React.useRef(null);
  if (storeRef.current === null) {
    storeRef.current = createStore(() => ({ visible: true, mode: 'registered', generation: 0 }));
  }
  const store = storeRef.current;
  const visible = useStore(store, (state) => state.visible);
  const mode = useStore(store, (state) => state.mode);
  const generation = useStore(store, (state) => state.generation);

  globalThis.__rnqnxAccessibilityConfigure = (nextMode) => {
    if (!modes.has(nextMode)) throw new Error('RNQNX_ACCESSIBILITY_PROBE_INVALID_MODE');
    store.setState({ mode: nextMode });
  };
  globalThis.__rnqnxAccessibilityUnmount = () => store.setState({ visible: false });
  globalThis.__rnqnxAccessibilityRemount = () => {
    store.setState((state) => ({ visible: true, generation: state.generation + 1 }));
  };

  return React.createElement(
    View,
    { style: { width: 240, height: 120 }, collapsable: false },
    visible ? React.createElement(ActionTarget, { key: generation, mode }) : null
  );
}

AppRegistry.registerComponent('RNQnxAccessibilityActionProbe', () => AccessibilityActionProbe);
