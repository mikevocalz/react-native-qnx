// react-native-safe-area-context for QNX. The package's native half
// (RNCSafeAreaProvider, NativeSafeAreaContext) does not exist on this
// platform. The experimental single-window host supplies native title-bar
// insets; hosts without it keep zero insets. Metro points
// `react-native-safe-area-context` here for the qnx platform only.
const React = require('react');
const {
  View,
  useWindowDimensions,
  __unstableQnxWindowMetrics: nativeMetrics,
} = require('react-native');

const ZERO_INSETS = Object.freeze({ top: 0, right: 0, bottom: 0, left: 0 });

const initialWindowMetrics = {
  frame: { x: 0, y: 0, width: 1280, height: 800 },
  insets: ZERO_INSETS,
};

const SafeAreaInsetsContext = React.createContext(null);
const SafeAreaFrameContext = React.createContext(null);

function SafeAreaProvider({ children, style, initialMetrics, ...rest }) {
  const { width, height } = useWindowDimensions();
  const fallback = React.useMemo(
    () => ({
      frame: { x: 0, y: 0, width, height },
      insets: initialMetrics?.insets ?? ZERO_INSETS,
    }),
    [width, height, initialMetrics]
  );
  const native = nativeMetrics ? nativeMetrics.useWindowMetrics(fallback) : fallback;
  const frame = globalThis.__rnqnxWindowHost ? native.frame : fallback.frame;
  const insets = native.insets;
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

const SafeAreaView = React.forwardRef(function SafeAreaView({ style, edges, mode, ...props }, ref) {
  const insets = useSafeAreaInsets();
  const resolved = nativeMetrics ? nativeMetrics.insetStyle(style, insets, edges, mode) : {};
  return React.createElement(View, { ...props, ref, style: [style, resolved] });
});

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
  initialWindowSafeAreaInsets: ZERO_INSETS,
  NativeSafeAreaProvider: SafeAreaProvider,
  NativeSafeAreaView: SafeAreaView,
};
