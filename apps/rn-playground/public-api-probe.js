import React from 'react';
import {
  AppRegistry,
  Button,
  Dimensions,
  Easing,
  findNodeHandle,
  I18nManager,
  ImageBackground,
  PanResponder,
  PixelRatio,
  Platform,
  processColor,
  StyleSheet,
  Text,
  TouchableOpacity,
  useWindowDimensions,
  View,
  VirtualizedList,
} from 'react-native';

// Assert the real public JS implementations when this bundle runs in the host.
// Metro bundling alone does not establish these runtime results.
if (Easing.linear(0.25) !== 0.25 || Easing.inOut(Easing.linear)(0.75) !== 0.75)
  throw new Error('Public Easing semantics changed');
if (processColor('#123456') !== 0xff123456)
  throw new Error('Public processColor semantics changed');
if (findNodeHandle(null) !== null) throw new Error('Public renderer handle semantics changed');
const window = Dimensions.get('window');
if (!(window.width > 0 && window.height > 0 && PixelRatio.get() === window.scale))
  throw new Error('Public dimensions/pixel ratio disagree with DeviceInfo');
if (I18nManager.isRTL !== false || I18nManager.doLeftAndRightSwapInRTL !== true)
  throw new Error('Expected the upstream LTR fallback without a native I18nManager');
const pan = PanResponder.create({ onStartShouldSetPanResponder: () => true });
if (typeof pan.panHandlers.onResponderMove !== 'function')
  throw new Error('Public PanResponder handlers are unavailable');

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
  const { width, height } = useWindowDimensions();
  return React.createElement(
    View,
    { style: styles.root, collapsable: false },
    React.createElement(Text, { style: styles.title }, 'React Native QNX'),
    React.createElement(Text, { style: styles.detail }, `Public imports / ${Platform.OS}`),
    React.createElement(Text, { style: styles.detail }, `Window / ${width} x ${height}`),
    React.createElement(Button, { title: 'Public Button', onPress: () => {} }),
    React.createElement(
      TouchableOpacity,
      { onPress: () => {} },
      React.createElement(Text, { style: styles.detail }, 'Public TouchableOpacity')
    ),
    React.createElement(
      ImageBackground,
      { style: { width: 240, height: 40 } },
      React.createElement(Text, { style: styles.detail }, 'Public ImageBackground')
    ),
    React.createElement(VirtualizedList, {
      style: { width: 240, maxHeight: 80 },
      data: ['Public VirtualizedList'],
      getItem: (data, index) => data[index],
      getItemCount: (data) => data.length,
      keyExtractor: (item) => item,
      renderItem: ({ item }) => React.createElement(Text, { style: styles.detail }, item),
    })
  );
}
if (!['qnx', 'marathon'].includes(Platform.OS))
  throw new Error('Wrong public-entry platform identity');
AppRegistry.registerComponent('RNQnxPlayground', () => PublicApp);
console.log(`RNQNX_PUBLIC_API_READY platform=${Platform.OS}`);
