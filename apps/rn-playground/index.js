/**
 * The smallest entry that exercises the real render path.
 *
 * It deliberately does not `import 'react-native'`: that entry pulls in the
 * TurboModule registry and a long tail of native modules this platform has not
 * implemented, which would fail for reasons that have nothing to do with
 * rendering. Importing AppRegistry and View directly keeps the bundle to what
 * the renderer actually needs.
 */

const React = require('react');
const AppRegistry = require('react-native/Libraries/ReactNative/AppRegistry').AppRegistry;
const View = require('react-native/Libraries/Components/View/View').default;

function PlaygroundApp() {
  return React.createElement(
    View,
    { style: { width: 320, height: 180 }, collapsable: false },
    React.createElement(View, {
      style: { width: 64, height: 32 },
      collapsable: false,
    })
  );
}

AppRegistry.registerComponent('RNQnxPlayground', () => PlaygroundApp);
