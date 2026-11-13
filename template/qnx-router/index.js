// Registers the expo-router app under the name the QNX host starts. The route
// files live in ./app and are found by app-ctx.js; keep this file free of
// components so Fast Refresh can swap a route without reloading the app.
const { AppRegistry } = require('react-native');
const { App } = require('expo-router/build/qualified-entry');

AppRegistry.registerComponent('__COMPONENT_NAME__', () => App);
