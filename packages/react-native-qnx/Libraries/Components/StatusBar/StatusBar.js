/**
 * `StatusBar` for QNX.
 *
 * QNX targets typically have no status bar to control, so this is a deliberate
 * no-op. It exports the same static API as React Native's StatusBar so code
 * that calls `StatusBar.setHidden(...)` does not crash, but the calls do
 * nothing.
 */

'use strict';

const React = require('react');

function StatusBar() {
  return null;
}

StatusBar.setHidden = function () {};
StatusBar.setBarStyle = function () {};
StatusBar.setNetworkActivityIndicatorVisible = function () {};
StatusBar.setBackgroundColor = function () {};
StatusBar.setTranslucent = function () {};

StatusBar.pushStackEntry = function () {
  return 0;
};
StatusBar.popStackEntry = function () {};
StatusBar.replaceStackEntry = function () {
  return 0;
};

StatusBar.currentHeight = 0;

module.exports = StatusBar;
module.exports.default = StatusBar;
