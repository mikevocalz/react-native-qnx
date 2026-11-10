'use strict';

// Keep this dependency in the graph: Metro runs it before main, and this also
// enforces ordering when the entry is evaluated without serializer preloads.
require('../../packages/react-native-qnx/dev/initialize');
const React = require('react');
const AppRegistry = require('react-native/Libraries/ReactNative/AppRegistry').AppRegistry;
const Counter = require('./Counter').default;

AppRegistry.registerComponent('RNQnxRefreshFixture', () => Counter);
