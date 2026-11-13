// Replaces react-native-drawer-layout for the qnx platform. expo-router's
// DrawerView renders `Drawer` from that package, whose native build needs
// Reanimated and react-native-gesture-handler. This module keeps its public
// API (Drawer, DrawerProgressContext, DrawerGestureContext, useDrawerProgress)
// and draws the drawer with the native RNQnxDrawer when the host has it, or
// with JsDrawer otherwise.
const React = require('react');
const { hasNative } = require('./native');
const { JsDrawer } = require('./JsDrawer');

const { DrawerProgressContext, DrawerGestureContext } = require('./drawerContexts');

function useDrawerProgress() {
  const progress = React.useContext(DrawerProgressContext);
  if (progress === undefined) {
    throw new Error("Couldn't find a drawer. Is your component inside a drawer navigator?");
  }
  return progress;
}

function Drawer(props) {
  if (hasNative('RNQnxDrawer')) {
    const { QtDrawer } = require('./QtDrawer');
    return React.createElement(QtDrawer, props);
  }
  return React.createElement(JsDrawer, props);
}

module.exports = { Drawer, DrawerProgressContext, DrawerGestureContext, useDrawerProgress };
