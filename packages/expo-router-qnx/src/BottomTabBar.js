// Replaces expo-router's react-navigation/bottom-tabs/views/BottomTabBar for
// the qnx platform (see resolver.js). BottomTabView hands this component the
// navigation state and an emitter; it draws the native RNQnxTabBar when the
// host has it and JsTabBar otherwise. Which side of the content it sits on is
// BottomTabView's decision, from the `tabBarPosition` screen option: the
// QnxTabs layout helper sets that from the window width.
const React = require('react');
const { I18nManager, useWindowDimensions } = require('react-native');
const { hasNative } = require('./native');
const { JsTabBar } = require('./JsTabBar');
const {
  buildTabs,
  tintsFor,
  resolveTabBarLayout,
  resolveTabBarEdge,
  isRail,
} = require('./tabBarModel');
const { setSwipeExclusionLeft } = require('./swipeExclusion');

const BOTTOM_HEIGHT = 60;
const RAIL_WIDTH = 96;

function getTabBarHeight({ state, descriptors, insets }) {
  const { tabBarPosition } = descriptors[state.routes[state.index].key].options;
  if (tabBarPosition === 'left' || tabBarPosition === 'right') return 0;
  return BOTTOM_HEIGHT + (tabBarPosition === 'top' ? 0 : (insets?.bottom ?? 0));
}

function BottomTabBar({ state, descriptors, emitter, navigateToTab, insets }) {
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
  const barHeight =
    typeof options.tabBarStyle?.height === 'number' ? options.tabBarStyle.height : BOTTOM_HEIGHT;

  const hidden = options.tabBarStyle?.display === 'none' || options.qnxTabBarHidden;
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

  // tabBarIcon is a React element factory and cannot cross to a native bar.
  // A tab that has one and no qnxIcon is drawn by the React tab bar, which
  // calls it with { focused, color, size } like every other platform.
  const needsReactIcons = tabs.some(
    (tab) => typeof tab.options.tabBarIcon === 'function' && !tab.icon
  );

  if (hasNative('RNQnxTabBar') && !needsReactIcons) {
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
  });
}

module.exports = { BottomTabBar, getTabBarHeight };
