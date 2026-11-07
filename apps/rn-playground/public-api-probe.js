import React from 'react';
import { AppRegistry, View, Text, StyleSheet, Platform } from 'react-native';

const styles = StyleSheet.create({
  root: { flex: 1, width: '100%', height: '100%', justifyContent: 'center', alignItems: 'center', backgroundColor: '#102033' },
  title: { color: '#ffffff', fontSize: 28 },
  detail: { color: '#a5cce5', fontSize: 18, marginTop: 16 },
});
function PublicApp() {
  return React.createElement(View, { style: styles.root, collapsable: false },
    React.createElement(Text, { style: styles.title }, 'React Native QNX'),
    React.createElement(Text, { style: styles.detail }, `Public imports / ${Platform.OS}`));
}
if (!['qnx', 'marathon'].includes(Platform.OS)) throw new Error('Wrong public-entry platform identity');
AppRegistry.registerComponent('RNQnxPlayground', () => PublicApp);
console.log(`RNQNX_PUBLIC_API_READY platform=${Platform.OS}`);
