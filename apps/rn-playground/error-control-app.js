const React = require('react');
const { AppRegistry, View } = require('react-native');

function ErrorControlApp() {
  React.useEffect(() => {
    console.log('RNQNX_TEST_ERROR_CONTROL_MOUNTED');
  }, []);
  return React.createElement(View, {
    collapsable: false,
    style: { width: 80, height: 80, backgroundColor: '#204080' },
  });
}

// Register before the test error: a later import failure must not become a
// passing screenshot merely because React can still start this component.
AppRegistry.registerComponent('RNQnxPlayground', () => ErrorControlApp);
