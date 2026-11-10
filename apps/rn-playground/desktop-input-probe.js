// Real public React Native imports and callbacks, exercised by Qt input events.
const React = require('react');
const { AppRegistry, View, Text, Pressable, Platform } = require('react-native');
// Reflect avoids Metro replacing a direct Platform.OS read with a literal.
globalThis.__qtProbePlatform = Reflect.get(Platform, 'OS');

const state = {
  mount: 0,
  enter: 0,
  leave: 0,
  hoverIn: 0,
  hoverOut: 0,
  focus: '',
  first: 0,
  second: 0,
  disabled: 0,
  keys: [],
};
let firstDisabled = false;
const listeners = new Set();
const subscribe = (listener) => {
  listeners.add(listener);
  return () => listeners.delete(listener);
};
const first = React.createRef();
const second = React.createRef();
const steps = [
  [{ action: 'assert', label: 'public React component mounted' }, () => state.mount === 1],
  [
    {
      action: 'hover',
      target: 'first',
      label: 'hover reaches containing View and Pressable onHoverIn',
    },
    () => state.enter === 1 && state.hoverIn === 1,
  ],
  [
    { action: 'hover', target: 'first', label: 'hover does not duplicate enter' },
    () => state.enter === 1,
  ],
  [
    { action: 'leave', label: 'window exit sends leave and onHoverOut' },
    () => state.leave === 1 && state.hoverOut === 1,
  ],
  [
    { action: 'call', label: 'public ref.focus reaches native View' },
    () => state.focus === 'first',
  ],
  [
    {
      action: 'key',
      key: 'Enter',
      label: 'ancestor Escape listener preserves ordinary Enter activation',
    },
    () => state.first === 1,
  ],
  [
    { action: 'key', key: 'Space', label: 'Space activates ordinary Pressable once' },
    () => state.first === 2,
  ],
  [{ action: 'key', key: 'Tab', label: 'Tab skips disabled View' }, () => state.focus === 'second'],
  [
    { action: 'key', key: 'ArrowDown', label: 'native arrow key reaches JS' },
    () => state.keys.join(',') === 'ArrowDown',
  ],
  [
    { action: 'key', key: 'Enter', label: 'custom activation avoids duplicate Pressable press' },
    () => state.second === 1,
  ],
  [
    { action: 'key', key: 'Backtab', label: 'Shift Tab restores previous focus' },
    () => state.focus === 'first',
  ],
  [
    { action: 'click', target: 'first', label: 'pointerEvents none overlay does not steal tap' },
    () => state.first === 3,
  ],
  [
    { action: 'click', target: 'disabled', label: 'disabled control does not activate' },
    () => state.disabled === 0,
  ],
  [{ action: 'call', label: 'public ref.blur clears focus' }, () => state.focus === ''],
  [{ action: 'call', label: 'refocus before disabling' }, () => state.focus === 'first'],
  [{ action: 'call', label: 'disabling focused control delivers blur' }, () => state.focus === ''],
  [
    { action: 'key', key: 'Enter', label: 'disabled focused control cannot activate' },
    () => state.first === 3,
  ],
  [{ action: 'call', label: 'reenable control' }, () => !firstDisabled],
  [{ action: 'call', label: 'reenabled control regains focus' }, () => state.focus === 'first'],
  [
    { action: 'key', key: 'Enter', label: 'reenabled control activates once' },
    () => state.first === 4,
  ],
];
globalThis.__qtProbeSteps = steps.map((entry) => entry[0]);
globalThis.__qtProbeCheck = (index) => steps[index][1]();
globalThis.__qtProbeSnapshot = () => JSON.stringify({ ...state, firstDisabled });
globalThis.__qtProbeAction = (index) => {
  if (index === 4) first.current.focus();
  if (index === 13) first.current.blur();
  if (index === 14 || index === 18) first.current.focus();
  if (index === 15 || index === 17) {
    firstDisabled = index === 15;
    for (const listener of listeners) listener();
  }
};
function Probe() {
  const disabled = React.useSyncExternalStore(subscribe, () => firstDisabled);
  React.useLayoutEffect(() => {
    state.mount += 1;
  }, []);
  return React.createElement(
    View,
    { collapsable: false, style: { width: 960, height: 720, padding: 20 } },
    React.createElement(
      View,
      {
        collapsable: false,
        style: { width: 220, height: 64 },
        onPointerEnter: () => {
          state.enter += 1;
        },
        onPointerLeave: () => {
          state.leave += 1;
        },
        onKeyDown: (event) => {
          if (event.nativeEvent.key === 'Escape') event.stopPropagation();
        },
      },
      React.createElement(
        Pressable,
        {
          ref: first,
          testID: 'first',
          collapsable: false,
          disabled,
          style: { width: 220, height: 64, backgroundColor: '#dcecff' },
          onHoverIn: () => {
            state.hoverIn += 1;
          },
          onHoverOut: () => {
            state.hoverOut += 1;
          },
          onFocus: () => {
            state.focus = 'first';
          },
          onBlur: () => {
            state.focus = '';
          },
          onPress: () => {
            state.first += 1;
          },
        },
        React.createElement(Text, null, 'First control')
      )
    ),
    React.createElement(
      Pressable,
      {
        testID: 'disabled',
        collapsable: false,
        disabled: true,
        style: { width: 220, height: 64 },
        onPress: () => {
          state.disabled += 1;
        },
      },
      React.createElement(Text, null, 'Disabled control')
    ),
    React.createElement(
      Pressable,
      {
        ref: second,
        testID: 'second',
        collapsable: false,
        style: { width: 220, height: 64, backgroundColor: '#e8e8e8' },
        onFocus: () => {
          state.focus = 'second';
        },
        onBlur: () => {
          state.focus = '';
        },
        onKeyDown: (event) => {
          const key = event.nativeEvent.key;
          if (key === 'ArrowDown') state.keys.push(key);
          if (key === 'Enter') {
            event.preventDefault();
            state.second += 1;
          }
        },
        onPress: () => {
          state.second += 100;
        },
      },
      React.createElement(Text, null, 'Keyboard-owned control')
    ),
    React.createElement(
      View,
      {
        pointerEvents: 'none',
        collapsable: false,
        style: { position: 'absolute', left: 20, top: 20, width: 220, height: 64 },
      },
      React.createElement(Text, null, 'Non-intercepting overlay')
    )
  );
}
AppRegistry.registerComponent('RNQnxDesktopInputProbe', () => Probe);
