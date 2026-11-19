// @ts-nocheck: React Native stand-in for tests.
// A small stand-in for `react-native` so component code can be rendered with
// react-test-renderer in plain Node (the real package is Flow source). Host
// components keep their props, so tests assert on what a view would receive.
const React = require('react');

function host(name) {
  const Component = React.forwardRef((props, ref) => React.createElement(name, { ...props, ref }));
  Component.displayName = name;
  return Component;
}

function flatten(style) {
  if (Array.isArray(style)) return Object.assign({}, ...style.map(flatten));
  return style || {};
}

class Value {
  constructor(value) {
    this.value = value;
  }
  setValue(next) {
    this.value = next;
  }
  stopAnimation(callback) {
    callback?.(this.value);
  }
  interpolate(config) {
    return { interpolatedFrom: this, config };
  }
}

const Animated = {
  Value,
  View: host('Animated.View'),
  // Finishes at once, so a render sees the settled state.
  timing: (value, config) => ({
    start: (done) => {
      value.setValue(config.toValue);
      if (done) done({ finished: true });
    },
    stop: () => {},
  }),
};

module.exports = {
  View: host('View'),
  Text: host('Text'),
  Pressable: host('Pressable'),
  ScrollView: host('ScrollView'),
  Animated,
  Easing: { out: (curve) => curve, cubic: 'cubic' },
  PanResponder: { create: () => ({ panHandlers: {} }) },
  StyleSheet: {
    create: (styles) => styles,
    flatten,
    absoluteFill: { position: 'absolute', left: 0, right: 0, top: 0, bottom: 0 },
  },
  Platform: { OS: 'qnx' },
  useWindowDimensions: () => ({ width: 800, height: 600 }),
};
