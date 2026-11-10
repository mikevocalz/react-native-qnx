import React from 'react';
import {
  AppRegistry,
  View,
  Text,
  StyleSheet,
  Platform,
  Dimensions,
  PixelRatio,
  useWindowDimensions,
} from 'react-native';

const styles = StyleSheet.create({
  root: {
    flex: 1,
    width: '100%',
    height: '100%',
    justifyContent: 'center',
    alignItems: 'center',
    backgroundColor: '#102033',
  },
  title: { color: '#ffffff', fontSize: 28 },
  detail: { color: '#a5cce5', fontSize: 18, marginTop: 16 },
});
function PublicApp() {
  const viewport = useWindowDimensions();
  if (!(viewport.width > 0 && viewport.height > 0 && viewport.scale > 0)) {
    throw new Error('Public window metrics must come from the native DeviceInfo provider');
  }
  return React.createElement(
    View,
    { style: styles.root, collapsable: false },
    React.createElement(Text, { style: styles.title }, 'React Native QNX'),
    React.createElement(
      Text,
      { style: styles.detail },
      `Public imports / ${Platform.OS} / ${viewport.width}×${viewport.height}`
    )
  );
}
if (!['qnx', 'marathon'].includes(Platform.OS))
  throw new Error('Wrong public-entry platform identity');
const initialWindow = Dimensions.get('window');
if (!(initialWindow.width > 0 && initialWindow.height > 0 && initialWindow.scale > 0)) {
  throw new Error('Invalid initial native display metrics');
}
if (PixelRatio.get() !== initialWindow.scale)
  throw new Error('PixelRatio must use DeviceInfo scale');
AppRegistry.registerComponent('RNQnxPlayground', () => PublicApp);
console.log(`RNQNX_PUBLIC_API_READY platform=${Platform.OS}`);
