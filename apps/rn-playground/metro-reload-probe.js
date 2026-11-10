import React from 'react';
import { AppRegistry, View } from 'react-native';

globalThis.__metroReloadProbe = { evaluations: 1 };
function MetroReloadProbe() {
  return React.createElement(View, {
    testID: 'metro-reload-view',
    style: { width: 48, height: 32 },
  });
}
AppRegistry.registerComponent('MetroReloadProbe', () => MetroReloadProbe);
