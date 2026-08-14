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
const Pressable = require('react-native/Libraries/Components/Pressable/Pressable').default;

function PlaygroundApp() {
  return React.createElement(
    View,
    {
      style: {
        width: 320,
        height: 180,
        backgroundColor: '#204080',
        borderRadius: 12,
      },
      collapsable: false,
    },
    React.createElement(View, {
      style: {
        width: 64,
        height: 32,
        backgroundColor: '#ff8800',
        opacity: 0.5,
      },
      collapsable: false,
      // The host test presses here and reads these back, which is how a touch
      // reaching JavaScript is proven rather than assumed.
      onTouchStart: (event) => {
        globalThis.__touchStarts = (globalThis.__touchStarts ?? 0) + 1;
        globalThis.__lastTouch = event.nativeEvent.locationX;
      },
      onTouchEnd: () => {
        globalThis.__touchEnds = (globalThis.__touchEnds ?? 0) + 1;
      },
      onStartShouldSetResponder: () => {
        globalThis.__askedResponder = (globalThis.__askedResponder ?? 0) + 1;
        return true;
      },
      onResponderGrant: () => {
        globalThis.__granted = (globalThis.__granted ?? 0) + 1;
      },
      onResponderRelease: () => {
        globalThis.__released = (globalThis.__released ?? 0) + 1;
      },
    }),
    // Pressable sits on top of the responder system. It is the first component
    // this platform renders that is not a bare View.
    React.createElement(Pressable, {
      style: { width: 64, height: 32, backgroundColor: '#00aa66' },
      collapsable: false,
      onPress: () => {
        globalThis.__presses = (globalThis.__presses ?? 0) + 1;
      },
    })
  );
}

AppRegistry.registerComponent('RNQnxPlayground', () => PlaygroundApp);
