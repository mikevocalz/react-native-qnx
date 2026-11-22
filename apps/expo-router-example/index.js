import * as React from 'react';
import { AppRegistry } from 'react-native';
import { ExpoRoot } from 'expo-router';
import { ctx } from './app-ctx';
import { ExampleProvider } from './components/example-store';

function App() {
  return <ExpoRoot context={ctx} wrapper={ExampleProvider} />;
}

AppRegistry.registerComponent('RNQnxRouterExample', () => App);
