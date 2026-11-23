/**
 * Device probe for the window-chrome receipts. It logs every AppState change as
 * a RNQNX_APPSTATE line, shows the state on screen, and blocks the JavaScript
 * thread for BLOCK_MS when the surface is tapped, so a receipt can show the
 * title bar controls still working while JavaScript is stuck.
 */
const React = require('react');
const AppRegistry = require('react-native/Libraries/ReactNative/AppRegistry').AppRegistry;
const AppState = require('react-native/Libraries/AppState/AppState').default;
const View = require('react-native/Libraries/Components/View/View').default;
const Text = require('react-native/Libraries/Text/Text').default;
const Pressable = require('react-native/Libraries/Components/Pressable/Pressable').default;

const BLOCK_MS = 20000;

function log(message) {
  console.log(`RNQNX_CHROME_PROBE ${Date.now()} ${message}`);
}

function Probe() {
  const [state, setState] = React.useState(AppState.currentState);
  const [count, setCount] = React.useState(0);
  React.useEffect(() => {
    log(`initial appstate=${AppState.currentState}`);
    const sub = AppState.addEventListener('change', (next) => {
      log(`appstate=${next}`);
      setState(next);
    });
    return () => sub.remove();
  }, []);
  const block = () => {
    log(`js-block-start ms=${BLOCK_MS}`);
    const end = Date.now() + BLOCK_MS;
    while (Date.now() < end) {
      // Busy wait: the JavaScript thread is deliberately blocked.
    }
    log('js-block-end');
    setCount((n) => n + 1);
  };
  return React.createElement(
    View,
    {
      style: {
        flex: 1,
        backgroundColor: '#102033',
        justifyContent: 'center',
        alignItems: 'center',
      },
    },
    React.createElement(Text, { style: { color: '#fff', fontSize: 28 } }, `AppState: ${state}`),
    React.createElement(
      Pressable,
      { onPress: block, style: { marginTop: 24, padding: 20, backgroundColor: '#2b6cb0' } },
      React.createElement(
        Text,
        { style: { color: '#fff', fontSize: 22 } },
        `Block JS for ${BLOCK_MS / 1000} s (blocked ${count} times)`
      )
    )
  );
}

AppRegistry.registerComponent('RNQnxPlayground', () => Probe);
