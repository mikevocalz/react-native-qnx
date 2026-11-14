// Replaces expo-router's react-navigation/bottom-tabs/views/BottomTabBar for
// the qnx platform (see resolver.js). BottomTabView hands this component the
// navigation state and an emitter; it draws the native RNQnxTabBar when the
// host has it and JsTabBar otherwise. Which side of the content it sits on is
// BottomTabView's decision, from the `tabBarPosition` screen option: the
// QnxTabs layout helper sets that from the window width.
const React = require('react');
const { Animated, I18nManager, Keyboard, useWindowDimensions } = require('react-native');
const {
  BottomTabBarHeightCallbackContext,
} = require('expo-router/build/react-navigation/bottom-tabs/utils/BottomTabBarHeightCallbackContext');
const { hasNative } = require('./native');
const { JsTabBar } = require('./JsTabBar');
const {
  buildTabs,
  needsJsTabBar,
  flatStyle,
  tintsFor,
  resolveTabBarLayout,
  resolveTabBarEdge,
  isRail,
} = require('./tabBarModel');
const { setSwipeExclusionLeft } = require('./swipeExclusion');

const BOTTOM_HEIGHT = 60;
const RAIL_WIDTH = 96;

function getTabBarHeight({ state, descriptors, insets }) {
  const options = descriptors[state.routes[state.index].key].options;
  const { tabBarPosition } = options;
  if (tabBarPosition === 'left' || tabBarPosition === 'right') return 0;
  const style = flatStyle(options.tabBarStyle);
  if (style.display === 'none' || options.qnxTabBarHidden) return 0;
  const height = typeof style.height === 'number' ? style.height : BOTTOM_HEIGHT;
  return height + (tabBarPosition === 'top' ? 0 : (insets?.bottom ?? 0));
}

function TabBarContent({ state, descriptors, emitter, navigateToTab, insets }) {
  const { width } = useWindowDimensions();
  const focusedKey = state.routes[state.index].key;
  const options = descriptors[focusedKey].options;
  const position = options.tabBarPosition;
  const layout =
    position === 'left' || position === 'right'
      ? 'side'
      : position === 'bottom' || position === 'top'
        ? 'bottom'
        : resolveTabBarLayout(options.qnxTabBarLayout ?? 'auto', width, options.qnxSideBreakpoint);
  const edge = resolveTabBarEdge(position, layout);
  const side = isRail(edge);
  const rtl = Boolean(I18nManager?.isRTL);
  const tabs = React.useMemo(() => buildTabs(state, descriptors), [state, descriptors]);
  const tint = tintsFor(options);
  const showLabel = options.tabBarShowLabel !== false;
  const barStyle = flatStyle(options.tabBarStyle);
  const barHeight = typeof barStyle.height === 'number' ? barStyle.height : BOTTOM_HEIGHT;

  const hidden = barStyle.display === 'none' || options.qnxTabBarHidden;
  // A rail on the physical left keeps the drawer's edge swipe from starting on it.
  const railOnLeft = side && (edge === 'right') === rtl;
  const exclusion = railOnLeft && !hidden ? RAIL_WIDTH : 0;
  // Set on change only; the unmount cleanup is a separate effect so a change
  // from one width to another does not pass through 0.
  React.useEffect(() => {
    setSwipeExclusionLeft(exclusion);
  }, [exclusion]);
  React.useEffect(() => () => setSwipeExclusionLeft(0), []);

  if (hidden) return null;

  const onTabPress = (key) => {
    const event = emitter.emit({ type: 'tabPress', target: key, canPreventDefault: true });
    if (key !== focusedKey && !event.defaultPrevented) navigateToTab(key);
  };
  const onTabLongPress = (key) => emitter.emit({ type: 'tabLongPress', target: key });

  const bottomInset = edge === 'bottom' ? (insets?.bottom ?? 0) : 0;
  const sizeStyle = side
    ? { width: RAIL_WIDTH }
    : { height: barHeight + bottomInset, paddingBottom: bottomInset };

  // React elements (tabBarIcon without qnxIcon, tabBarBackground, tabBarButton)
  // cannot cross to a native bar, and the native bar draws a fixed set of
  // styles. A bar that needs more is drawn by JsTabBar, which draws every tab
  // bar option, and `tabBarIcon` is called with { focused, color, size } like
  // on every other platform.
  const needsJs = needsJsTabBar(tabs, options);

  if (hasNative('RNQnxTabBar') && !needsJs) {
    const { QtTabBar } = require('./QtTabBar');
    return React.createElement(QtTabBar, {
      tabs,
      activeKey: focusedKey,
      tint,
      layout: side ? 'side' : 'bottom',
      position: edge,
      showLabel,
      rtl,
      railWidth: RAIL_WIDTH,
      barHeight,
      onTabPress,
      onTabLongPress,
      style: sizeStyle,
    });
  }
  return React.createElement(JsTabBar, {
    tabs,
    activeKey: focusedKey,
    tint,
    layout: side ? 'side' : 'bottom',
    edge,
    showLabel,
    rtl,
    onTabPress,
    onTabLongPress,
    style: sizeStyle,
    options,
  });
}

// Follows the software keyboard: it is only reported where the host has a
// Keyboard module that emits (QNX has no on-screen keyboard unless one is
// installed, so the bar then stays).
function useKeyboardShown(enabled) {
  const [shown, setShown] = React.useState(() => enabled && Keyboard?.isVisible?.() === true);
  React.useEffect(() => {
    if (!enabled || typeof Keyboard?.addListener !== 'function') {
      setShown(false);
      return undefined;
    }
    const subscriptions = [
      Keyboard.addListener('keyboardDidShow', () => setShown(true)),
      Keyboard.addListener('keyboardDidHide', () => setShown(false)),
    ];
    // The option can be enabled after the keyboard's show event has fired.
    setShown(Keyboard?.isVisible?.() === true);
    return () => subscriptions.forEach((subscription) => subscription?.remove?.());
  }, [enabled]);
  return enabled && shown;
}

// tabBarHideOnKeyboard with tabBarVisibilityAnimationConfig ({ show, hide }:
// { animation: 'timing' | 'spring', config }).
function VisibilityGate({ hideOnKeyboard, config, height, children }) {
  const hide = useKeyboardShown(hideOnKeyboard);
  const opacity = React.useRef(new Animated.Value(hide ? 0 : 1)).current;
  const [gone, setGone] = React.useState(hide);
  const onHeightChange = React.useContext(BottomTabBarHeightCallbackContext);
  const reservedHeight = gone && hide ? 0 : height;
  // BottomTabView owns the height context used by scene padding. Report the
  // settled occupied size even when the bar has no host item to emit onLayout.
  React.useLayoutEffect(() => {
    onHeightChange?.(reservedHeight);
  }, [onHeightChange, reservedHeight]);
  const first = React.useRef(true);
  React.useEffect(() => {
    if (first.current) {
      first.current = false;
      return undefined;
    }
    const spec = hide ? config?.hide : config?.show;
    const kind = spec?.animation === 'spring' && Animated.spring ? 'spring' : 'timing';
    if (!hide) setGone(false);
    const animation = Animated[kind](opacity, {
      toValue: hide ? 0 : 1,
      useNativeDriver: false,
      ...(kind === 'timing' ? { duration: hide ? 200 : 250 } : null),
      ...spec?.config,
    });
    animation.start(({ finished }) => {
      if (finished && hide) setGone(true);
    });
    return () => animation.stop();
  }, [hide, config, opacity]);
  if (!hideOnKeyboard) return children;
  if (gone && hide) return null;
  return React.createElement(Animated.View, { style: { opacity } }, children);
}

function BottomTabBar(props) {
  const options = props.descriptors[props.state.routes[props.state.index].key].options;
  return React.createElement(
    VisibilityGate,
    {
      hideOnKeyboard: options.tabBarHideOnKeyboard === true,
      config: options.tabBarVisibilityAnimationConfig,
      height: getTabBarHeight(props),
    },
    React.createElement(TabBarContent, props)
  );
}

module.exports = { BottomTabBar, getTabBarHeight };
