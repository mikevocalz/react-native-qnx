// Adapter for the native RNQnxTabBar (Qt Quick item). Everything it draws
// comes from the `tabs` array; presses come back as topTabPress events.
const React = require('react');
const { nativeComponent } = require('./native');

const NativeTabBar = nativeComponent('RNQnxTabBar', {
  validAttributes: {
    tabs: true,
    activeKey: true,
    layout: true,
    position: true,
    showLabel: true,
    rtl: true,
    badgeColor: true,
    badgeTextColor: true,
    sideBreakpoint: true,
    availableWidth: true,
    railWidth: true,
    barHeight: true,
    tintActive: true,
    tintInactive: true,
    hidden: true,
  },
  directEventTypes: {
    topTabPress: { registrationName: 'onTabPress' },
    topTabLongPress: { registrationName: 'onTabLongPress' },
    topLayoutMode: { registrationName: 'onLayoutMode' },
  },
});

function QtTabBar({
  tabs,
  activeKey,
  tint,
  layout,
  position,
  showLabel,
  rtl,
  railWidth,
  barHeight,
  onTabPress,
  onTabLongPress,
  style,
}) {
  const data = React.useMemo(
    () =>
      tabs.map(({ key, title, icon, badge, accessibilityLabel }) => ({
        key,
        title,
        icon,
        badge,
        accessibilityLabel,
      })),
    [tabs]
  );
  return React.createElement(NativeTabBar, {
    style: [style, { backgroundColor: tint.background }],
    tabs: data,
    activeKey,
    // JavaScript decides bottom or side from the window width (BottomTabView
    // needs the answer to place the bar), so the native bar is never 'auto' here.
    layout,
    position,
    showLabel,
    rtl,
    badgeColor: tint.badge,
    badgeTextColor: tint.badgeText,
    railWidth,
    barHeight,
    tintActive: tint.active,
    tintInactive: tint.inactive,
    onTabPress: (event) => onTabPress(event.nativeEvent.key),
    onTabLongPress: (event) => onTabLongPress?.(event.nativeEvent.key),
  });
}

module.exports = { QtTabBar };
