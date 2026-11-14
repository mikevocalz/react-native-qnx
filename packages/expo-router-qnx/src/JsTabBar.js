// Tab bar drawn from React Native views: the fallback for a host without the
// native RNQnxTabBar, and the reference for what it draws. Icon glyph over a
// label, active tint, badge, hover and focus rings. `layout` 'bottom' is a
// row, 'side' a column (the rail).
const React = require('react');
const { Pressable, StyleSheet, Text, View } = require('react-native');
const { flatStyle } = require('./tabBarModel');

// '#rrggbb' or 'rgb(a)' with the given alpha; any other color string is returned unchanged.
function withAlpha(color, alpha) {
  if (typeof color === 'string' && /^#[0-9a-f]{6}$/i.test(color)) {
    const n = parseInt(color.slice(1), 16);
    return `rgba(${n >> 16},${(n >> 8) & 255},${n & 255},${alpha})`;
  }
  return color;
}

function JsTabItem({
  tab,
  active,
  tint,
  side,
  showLabel,
  variant,
  labelBeside,
  onPress,
  onLongPress,
}) {
  const [hover, setHover] = React.useState(false);
  const [focus, setFocus] = React.useState(false);
  const [pressed, setPressed] = React.useState(false);
  const activeColor = tab.activeTint ?? tint.active;
  const color = active ? activeColor : (tab.inactiveTint ?? tint.inactive);
  const options = tab.options ?? {};
  // tabBarIcon draws React elements, so it only works here (see BottomTabBar).
  const renderIcon = options.tabBarIcon;
  const iconNode =
    !tab.icon && typeof renderIcon === 'function'
      ? renderIcon({ focused: active, color, size: 24 })
      : React.createElement(
          Text,
          { allowFontScaling: options.tabBarAllowFontScaling, style: [styles.icon, { color }] },
          tab.icon || '\u25CF'
        );
  const badgeStyle = flatStyle(options.tabBarBadgeStyle);
  const materialPill = variant === 'material';
  const icon = React.createElement(
    View,
    {
      style: [
        materialPill && styles.pill,
        materialPill && active && { backgroundColor: withAlpha(activeColor, 0.2) },
        options.tabBarIconStyle,
      ],
    },
    iconNode,
    tab.badge !== '' &&
      React.createElement(
        View,
        { style: [styles.badge, { backgroundColor: tint.badge ?? '#f87171' }, badgeStyle] },
        React.createElement(
          Text,
          {
            allowFontScaling: options.tabBarAllowFontScaling,
            style: [styles.badgeText, { color: badgeStyle.color ?? tint.badgeText ?? '#fff' }],
          },
          tab.badge
        )
      )
  );
  const children = [
    React.createElement(React.Fragment, { key: 'icon' }, icon),
    showLabel &&
      React.createElement(
        Text,
        {
          key: 'label',
          numberOfLines: 1,
          ellipsizeMode: 'tail',
          allowFontScaling: options.tabBarAllowFontScaling,
          style: [
            styles.label,
            labelBeside && styles.labelBeside,
            { color },
            options.tabBarLabelStyle,
          ],
        },
        tab.title
      ),
  ];
  const buttonProps = {
    accessibilityRole: 'tab',
    role: 'tab',
    accessibilityState: { selected: active },
    accessibilityLabel: tab.accessibilityLabel,
    testID: tab.testID,
    onPress,
    onLongPress,
    style: [
      styles.item,
      side ? styles.itemSide : styles.itemBottom,
      labelBeside && styles.itemBeside,
      hover && !active && styles.hover,
      active && !materialPill && styles.active,
      active && tab.activeBackground !== undefined && { backgroundColor: tab.activeBackground },
      !active &&
        tab.inactiveBackground !== undefined && { backgroundColor: tab.inactiveBackground },
      focus && { borderColor: activeColor, borderWidth: 2 },
      pressed && !active && styles.pressed,
      options.tabBarItemStyle,
    ],
    children,
  };
  // tabBarButton replaces the pressable (expo-router's Tabs wraps it in a Link for `href`).
  if (typeof options.tabBarButton === 'function') {
    return React.createElement(options.tabBarButton, { ...buttonProps, href: options.href });
  }
  return React.createElement(Pressable, {
    ...buttonProps,
    onHoverIn: () => setHover(true),
    onHoverOut: () => setHover(false),
    onPressIn: () => setPressed(true),
    onPressOut: () => setPressed(false),
    onFocus: () => setFocus(true),
    onBlur: () => setFocus(false),
  });
}

function JsTabBar({
  tabs,
  activeKey,
  tint,
  layout,
  edge,
  showLabel = true,
  rtl = false,
  onTabPress,
  onTabLongPress,
  style,
  options = {},
}) {
  const side = layout === 'side';
  // The divider faces the content: the edge opposite the bar's own.
  const divider = side
    ? (edge === 'right') !== rtl
      ? { borderLeftWidth: 1, borderRightWidth: 0 }
      : null
    : edge === 'top'
      ? { borderTopWidth: 0, borderBottomWidth: 1 }
      : null;
  return React.createElement(
    View,
    {
      accessibilityRole: 'tablist',
      style: [
        styles.bar,
        side ? styles.barSide : styles.barBottom,
        divider,
        { backgroundColor: tint.background },
        rtl && !side && { flexDirection: 'row-reverse' },
        style,
        options.tabBarStyle,
      ],
    },
    // tabBarBackground draws behind the items.
    typeof options.tabBarBackground === 'function' &&
      React.createElement(
        View,
        { key: 'background', style: StyleSheet.absoluteFill, pointerEvents: 'none' },
        options.tabBarBackground()
      ),
    tabs.map((tab) =>
      React.createElement(JsTabItem, {
        key: tab.key,
        tab,
        active: tab.key === activeKey,
        tint,
        side,
        showLabel,
        variant: options.tabBarVariant,
        // Material draws the label under the icon; UIKit puts it beside the icon in a rail.
        labelBeside: options.tabBarLabelPosition === 'beside-icon',
        onPress: () => onTabPress(tab.key),
        onLongPress: () => onTabLongPress?.(tab.key),
      })
    )
  );
}

const styles = StyleSheet.create({
  bar: { borderColor: 'rgba(255,255,255,0.08)' },
  barBottom: { flexDirection: 'row', borderTopWidth: 1, paddingHorizontal: 8 },
  barSide: {
    flexDirection: 'column',
    borderRightWidth: 1,
    paddingVertical: 12,
    paddingHorizontal: 6,
  },
  item: {
    alignItems: 'center',
    justifyContent: 'center',
    borderRadius: 10,
    borderWidth: 2,
    borderColor: 'transparent',
  },
  itemBottom: { flex: 1, marginVertical: 6, marginHorizontal: 2 },
  itemSide: { height: 68, marginBottom: 4 },
  hover: { backgroundColor: 'rgba(255,255,255,0.06)' },
  pressed: { backgroundColor: 'rgba(255,255,255,0.12)' },
  active: { backgroundColor: 'rgba(121,232,220,0.14)' },
  icon: { fontSize: 22, lineHeight: 26, textAlign: 'center' },
  itemBeside: { flexDirection: 'row' },
  pill: { paddingHorizontal: 16, paddingVertical: 2, borderRadius: 14 },
  labelBeside: { marginTop: 0, marginLeft: 8 },
  label: { fontSize: 12, lineHeight: 16, marginTop: 2, maxWidth: '100%', paddingHorizontal: 4 },
  badge: {
    position: 'absolute',
    top: -2,
    right: -12,
    minWidth: 16,
    height: 16,
    paddingHorizontal: 4,
    borderRadius: 8,
    backgroundColor: '#f87171',
    alignItems: 'center',
    justifyContent: 'center',
  },
  badgeText: { color: '#fff', fontSize: 10, lineHeight: 14 },
});

module.exports = { JsTabBar };
