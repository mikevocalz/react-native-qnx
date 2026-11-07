// Production Metro fixture: real React, Fabric responder negotiation and Pressability.
const React = require('react');
const AppRegistry = require('react-native/Libraries/ReactNative/AppRegistry').AppRegistry;
const View = require('react-native/Libraries/Components/View/View').default;
const Pressable = require('react-native/Libraries/Components/Pressable/Pressable').default;

globalThis.__rnqnxPressableEvents = [];
globalThis.__rnqnxPressableMounts = 0;
globalThis.__rnqnxPressableUnmounts = 0;
function record(type) {
  globalThis.__rnqnxPressableEvents.push(type);
}
function PressableProbe() {
  React.useLayoutEffect(() => {
    globalThis.__rnqnxPressableMounts += 1;
    return () => {
      globalThis.__rnqnxPressableUnmounts += 1;
    };
  }, []);
  return React.createElement(
    View,
    { collapsable: false, style: { width: 320, height: 240 } },
    React.createElement(Pressable, {
      collapsable: false,
      style: { position: 'absolute', left: 20, top: 20, width: 120, height: 60 },
      onPressIn: () => record('in'),
      onPressOut: () => record('out'),
      onPress: () => record('press'),
    }),
    React.createElement(Pressable, {
      disabled: true,
      collapsable: false,
      style: { position: 'absolute', left: 20, top: 100, width: 120, height: 60 },
      onPressIn: () => record('disabled-in'),
      onPressOut: () => record('disabled-out'),
      onPress: () => record('disabled-press'),
    })
  );
}
AppRegistry.registerComponent('RNQnxPressableProbe', () => PressableProbe);
