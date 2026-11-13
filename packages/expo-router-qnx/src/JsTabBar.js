// Tab bar drawn from React Native views: the fallback for a host without the
// native RNQnxTabBar, and the reference for what it draws. Icon glyph over a
// label, active tint, badge, hover and focus rings. `layout` 'bottom' is a
// row, 'side' a column (the rail).
const React = require('react');
const { Pressable, StyleSheet, Text, View } = require('react-native');

function JsTabItem({ tab, active, tint, side, showLabel, onPress, onLongPress }) {
  const [hover, setHover] = React.useState(false);
  const [focus, setFocus] = React.useState(false);
  const [pressed, setPressed] = React.useState(false);
  const color = active ? tint.active : tint.inactive;
  // tabBarIcon draws React elements, so it only works here (see BottomTabBar).
  const renderIcon = tab.options?.tabBarIcon;
  const icon =
    !tab.icon && typeof renderIcon === 'function'
      ? renderIcon({ focused: active, color, size: 24 })
      : React.createElement(Text, { style: [styles.icon, { color }] }, tab.icon || '●');
  return React.createElement(
    Pressable,
    {
      accessibilityRole: 'tab',
      accessibilityState: { selected: active },
      accessibilityLabel: tab.accessibilityLabel,
      onPress,
      onLongPress,
      onHoverIn: () => setHover(true),
      onHoverOut: () => setHover(false),
      onPressIn: () => setPressed(true),
      onPressOut: () => setPressed(false),
      onFocus: () => setFocus(true),
      onBlur: () => setFocus(false),
      style: [
        styles.item,
        side ? styles.itemSide : styles.itemBottom,
        hover && !active && styles.hover,
        active && styles.active,
        focus && { borderColor: tint.active, borderWidth: 2 },
        pressed && !active && styles.pressed,
      ],
    },
    React.createElement(
      View,
      null,
      icon,
      tab.badge !== '' &&
        React.createElement(
          View,
          { style: [styles.badge, { backgroundColor: tint.badge ?? '#f87171' }] },
          React.createElement(
            Text,
            { style: [styles.badgeText, { color: tint.badgeText ?? '#fff' }] },
            tab.badge
          )
        )
    ),
    showLabel &&
      React.createElement(
        Text,
        { numberOfLines: 1, ellipsizeMode: 'tail', style: [styles.label, { color }] },
        tab.title
      )
  );
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
      ],
    },
    tabs.map((tab) =>
      React.createElement(JsTabItem, {
        key: tab.key,
        tab,
        active: tab.key === activeKey,
        tint,
        side,
        showLabel,
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
