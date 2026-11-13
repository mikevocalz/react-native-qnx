// Shared by drawer-layout.js and JsDrawer.js. The progress value is an
// Animated.Value here; react-native-drawer-layout's own is a Reanimated
// SharedValue, so code that reads it as one does not work on QNX.
const React = require('react');

const DrawerProgressContext = React.createContext(undefined);
const DrawerGestureContext = React.createContext(undefined);

module.exports = { DrawerProgressContext, DrawerGestureContext };
