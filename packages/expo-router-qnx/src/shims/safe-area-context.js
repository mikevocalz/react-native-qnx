// react-native-safe-area-context for QNX. The package's native half
// (RNCSafeAreaProvider, NativeSafeAreaContext) does not exist on this
// platform, and the window has no notch or system bars: the host's title bar
// sits outside the React surface, so the insets are zero. Metro points
// `react-native-safe-area-context` here for the qnx platform only.
const React = require('react');
const { View, useWindowDimensions } = require('react-native');

const ZERO_INSETS = Object.freeze({ top: 0, right: 0, bottom: 0, left: 0 });

const initialWindowMetrics = {
  frame: { x: 0, y: 0, width: 1280, height: 800 },
  insets: ZERO_INSETS,
};

const SafeAreaInsetsContext = React.createContext(null);
const SafeAreaFrameContext = React.createContext(null);

function SafeAreaProvider({ children, style, initialMetrics, ...rest }) {
  const { width, height } = useWindowDimensions();
  const frame = React.useMemo(() => ({ x: 0, y: 0, width, height }), [width, height]);
  const insets = initialMetrics?.insets ?? ZERO_INSETS;
  return React.createElement(
    SafeAreaFrameContext.Provider,
    { value: frame },
    React.createElement(
      SafeAreaInsetsContext.Provider,
      { value: insets },
      React.createElement(View, { style: [{ flex: 1 }, style], ...rest }, children)
    )
  );
}

function SafeAreaView(props) {
  return React.createElement(View, props);
}

function useSafeAreaInsets() {
  return React.useContext(SafeAreaInsetsContext) ?? ZERO_INSETS;
}

function useSafeAreaFrame() {
  const { width, height } = useWindowDimensions();
  return React.useContext(SafeAreaFrameContext) ?? { x: 0, y: 0, width, height };
}

function withSafeAreaInsets(Component) {
  return React.forwardRef((props, ref) =>
    React.createElement(Component, { ...props, ref, insets: useSafeAreaInsets() })
  );
}

function SafeAreaListener({ children }) {
  return children ?? null;
}

module.exports = {
  SafeAreaProvider,
  SafeAreaView,
  SafeAreaInsetsContext,
  SafeAreaFrameContext,
  SafeAreaConsumer: SafeAreaInsetsContext.Consumer,
  useSafeAreaInsets,
  useSafeAreaFrame,
  useSafeArea: useSafeAreaInsets,
  withSafeAreaInsets,
  SafeAreaListener,
  initialWindowMetrics,
};
