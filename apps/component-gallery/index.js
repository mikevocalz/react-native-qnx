// The gallery is an expo-router app: ./app holds the routes (a drawer around a
// tabs group). This file only registers the root; the route modules live in
// app-ctx.js so Fast Refresh can swap a route without reloading the app.
const { AppRegistry, LogBox } = require('react-native');
const { App } = require('expo-router/build/qualified-entry');

// A host built before app.json gained `scheme` embeds the older config, and
// expo-linking warns about it; the warning goes away with the next native build.
LogBox.ignoreLogs(['Linking requires a build-time setting']);

AppRegistry.registerComponent('RNQnxPlayground', () => App);
// The SVG cards alone, for the device screenshot check; see ./svg/standalone.js.
AppRegistry.registerComponent('RNQnxSvgGallery', () => require('./svg/standalone').SvgGalleryApp);
