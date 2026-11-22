// Keep SDK58 entry details inside the platform adapter. Consumer source uses
// public ExpoRoot and this provider; it does not import Router build internals.
const React = require('react');
const { Head } = require('expo-router/head');
require('expo-router/build/fast-refresh');

/** @param {{children?: import('react').ReactNode}} props */
function QnxRouterProvider({ children }) {
  return React.createElement(Head.Provider, null, children);
}
module.exports = { QnxRouterProvider };
