// A deliberately small real React fixture, independent of the playground UI.
// Both host and QNX run the production Metro output from this entry point.
// Like index.js, import only the RN modules this platform implements.
const React = require('react');
const AppRegistry = require('react-native/Libraries/ReactNative/AppRegistry').AppRegistry;
const View = require('react-native/Libraries/Components/View/View').default;

function ReactRenderProbe() {
  const state = React.useState(false);
  const updated = state[0];
  const setUpdated = state[1];
  globalThis.__rnqnxUpdateProbe = () => setUpdated(true);
  return React.createElement(
    View,
    { collapsable: false, style: { width: 160, height: 100, backgroundColor: '#204080' } },
    React.createElement(View, {
      collapsable: false,
      style: {
        position: 'absolute',
        left: updated ? 40 : 12,
        top: 16,
        width: updated ? 72 : 48,
        height: 24,
        backgroundColor: updated ? '#00cc66' : '#ff8800',
      },
    })
  );
}

AppRegistry.registerComponent('RNQnxReactRenderProbe', () => ReactRenderProbe);
