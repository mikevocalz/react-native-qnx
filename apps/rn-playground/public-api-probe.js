import React from 'react';
import { AppRegistry, View, Text, Image, StyleSheet, Platform } from 'react-native';

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
  return React.createElement(
    View,
    { style: styles.root, collapsable: false },
    React.createElement(Text, { style: styles.title }, 'React Native QNX'),
    React.createElement(Text, { style: styles.detail }, `Public imports / ${Platform.OS}`)
  );
}
if (!['qnx', 'marathon'].includes(Platform.OS))
  throw new Error('Wrong public-entry platform identity');
AppRegistry.registerComponent('RNQnxPlayground', () => PublicApp);

// Validate the public Image loader rather than importing an iOS-only component.
// The local PNG keeps this a decode/API probe with no networking dependency.
const imageUri =
  'data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR4nGMwTpv5HwAENAIyWy0K4AAAAABJRU5ErkJggg==';
Image.getSize(imageUri)
  .then((size) => {
    if (size.width !== 1 || size.height !== 1) {
      throw new Error('Unexpected public Image.getSize result');
    }
    globalThis.__publicImageGetSize = size;
    console.log('RNQNX_PUBLIC_IMAGE_API_READY width=1 height=1');
    console.log(`RNQNX_PUBLIC_API_READY platform=${Platform.OS}`);
  })
  .catch((error) => {
    globalThis.__publicImageGetSizeError = String(error);
    console.error('RNQNX_PUBLIC_IMAGE_API_FAILED', String(error));
  });
