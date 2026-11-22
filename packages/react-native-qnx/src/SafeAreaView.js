'use strict';
const React = require('react');
const windowMetrics = require('./window-metrics');
const SafeAreaView = React.forwardRef(function SafeAreaView({ style, children, ...props }, ref) {
  const { View, useWindowDimensions } = require('react-native');
  const { width, height } = useWindowDimensions();
  const metrics = windowMetrics.useWindowMetrics({
    frame: { x: 0, y: 0, width, height },
    insets: windowMetrics.ZERO_INSETS,
  });
  return React.createElement(
    View,
    {
      ...props,
      ref,
      style: [style, windowMetrics.insetStyle(style, metrics.insets)],
    },
    children
  );
});
module.exports = { default: SafeAreaView };
