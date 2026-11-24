// react-native-safe-area-context for QNX. The package's native half
// (RNCSafeAreaProvider, NativeSafeAreaContext) does not exist on this
// platform, and the window has no notch or system bars. The only inset is the
// host's title bar: zero in the `default` style (the bar sits outside the React
// surface) and the strip the floating controls cover in the `hidden` style.
// SafeAreaProvider reads it from the windows API and follows changes. Metro
// points `react-native-safe-area-context` here for the qnx platform only.
const React = require('react');
const { View, useWindowDimensions } = require('react-native');

const ZERO_INSETS = Object.freeze({ top: 0, right: 0, bottom: 0, left: 0 });

// The windows API, or null in a host built without window chrome, where no bar
// is drawn and so nothing insets (WINDOWS_UNAVAILABLE is the one soft failure).
function titleBarWindows() {
  try {
    const windows = require('@rn-qnx/qnx-nitro-runtime').getQnxWindows();
    return typeof windows.getTitleBarInset === 'function' ? windows : null;
  } catch (error) {
    if (error != null && error.code === 'WINDOWS_UNAVAILABLE') return null;
    // The runtime package is not installed next to this app: nothing to ask.
    // (create-project does not vendor @rn-qnx/qnx-nitro-runtime yet.)
    // Node reports MODULE_NOT_FOUND; Metro's development runtime throws a plain
    // Error whose message starts with "Cannot find module".
    if (
      error != null &&
      (error.code === 'MODULE_NOT_FOUND' || /^Cannot find module/.test(String(error.message)))
    ) {
      return null;
    }
    throw error;
  }
}

function insetsOf(inset) {
  if (inset.top === 0 && inset.right === 0 && inset.bottom === 0 && inset.left === 0) {
    return ZERO_INSETS;
  }
  return Object.freeze({
    top: inset.top,
    right: inset.right,
    bottom: inset.bottom,
    left: inset.left,
  });
}

// null when there is no windows API to ask.
function readInsets() {
  const windows = titleBarWindows();
  return windows === null ? null : insetsOf(windows.getTitleBarInset());
}

const initialWindowMetrics = {
  frame: { x: 0, y: 0, width: 1280, height: 800 },
  insets: readInsets() ?? ZERO_INSETS,
};

const SafeAreaInsetsContext = React.createContext(null);
const SafeAreaFrameContext = React.createContext(null);

function SafeAreaProvider({ children, style, initialMetrics, ...rest }) {
  const { width, height } = useWindowDimensions();
  const frame = React.useMemo(() => ({ x: 0, y: 0, width, height }), [width, height]);
  const [nativeInsets, setNativeInsets] = React.useState(readInsets);
  React.useEffect(() => {
    const windows = titleBarWindows();
    if (windows === null) return undefined;
    // The bar can change between the first read and this subscription.
    setNativeInsets(insetsOf(windows.getTitleBarInset()));
    return windows.addOnInsetChangedListener((inset) => setNativeInsets(insetsOf(inset)));
  }, []);
  const insets = nativeInsets ?? initialMetrics?.insets ?? ZERO_INSETS;
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
  initialWindowSafeAreaInsets: initialWindowMetrics.insets,
  NativeSafeAreaProvider: SafeAreaProvider,
  NativeSafeAreaView: SafeAreaView,
};
