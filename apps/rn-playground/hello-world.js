/** Minimal static app for the existing Qt Quick/Fabric playground runner. */
const React = require('react');
const AppRegistry = require('react-native/Libraries/ReactNative/AppRegistry').AppRegistry;
const View = require('react-native/Libraries/Components/View/View').default;
const Text = require('react-native/Libraries/Text/Text').default;

function HelloWorld() {
  return React.createElement(
    View,
    {
      collapsable: false,
      style: {
        flex: 1,
        width: '100%',
        height: '100%',
        justifyContent: 'center',
        alignItems: 'center',
        backgroundColor: '#102033',
      },
    },
    React.createElement(
      Text,
      { style: { color: '#ffffff', fontSize: 32 } },
      'React Native QNX: Hello World'
    )
  );
}

// This name matches the existing native playground surface entry point.
AppRegistry.registerComponent('RNQnxPlayground', () => HelloWorld);
