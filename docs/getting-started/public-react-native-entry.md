# Opt in to the partial public React Native entry

This Phase3 prerequisite lets supported application code use named imports from react-native instead of importing its internal Libraries paths. It reuses the existing Qt/Fabric component implementations. It is explicitly opt-in and does not change the default QNX or Marathon resolver.

Wrap your existing target Metro config with the public subpath helper:

```js
const withPublicEntry = require('@rn-qnx/metro-config-qnx/public-entry');
const { getQnxMetroConfig } = require('@rn-qnx/metro-config-qnx');
module.exports = withPublicEntry(getQnxMetroConfig(__dirname), 'qnx');
```

For Marathon, wrap the existing getMarathonMetroConfig result and select marathon. Other platform requests and non-root requests retain the original resolver. This subpath is included in the package file list, but no new package publication is claimed.

The initial exports are AppRegistry, View, Text, Image, ScrollView, FlatList, SectionList, TextInput, Pressable, ActivityIndicator, Switch, Modal, StyleSheet, Platform and Animated. They retain the actual component-support limits; an export existing is not full native parity. Unimplemented exports such as BackHandler or Alert throw an explicit error rather than yielding a fake implementation. CommonJS interop metadata and promise introspection are exempt from those errors.

This is not Expo Router, native screen-stack, Worklets, Reanimated or Gesture Handler support. Those Phase3 interfaces remain unimplemented. The included public-api-probe exercises public View/Text/StyleSheet/AppRegistry/Platform imports and validates selected platform identity; it still uses the existing runner surface name. No new renderer, native bridge or C++ API is introduced.

Tests separately cover lazy export wiring with synthetic module objects, resolver preservation, real Metro bundles and an actual shared Linux Qt/Hermes host capture. Linux rendering of both platform bundles is not physical QNX or Marathon shell qualification. No support rows or release gates are promoted by this experiment. Keep default configs unchanged until broader native/device and consumer-package compatibility is reviewed.
