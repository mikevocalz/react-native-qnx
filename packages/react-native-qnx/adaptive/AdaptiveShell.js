'use strict';

const React = require('react');
const { View, Text, Pressable, ScrollView, Modal, useWindowDimensions } = require('react-native');
const { length, getWindowSizeClass } = require('./layout');
const { useElementSize, useLocalStore } = require('./state');
const h = React.createElement;

const defaultAdaptiveColors = Object.freeze({
  background: '#0b1725',
  surface: '#142538',
  text: '#edf4fa',
  muted: '#adbdcc',
  border: '#2d455b',
  selected: '#19464a',
  selectedText: '#96f3e8',
  backdrop: 'rgba(0,0,0,0.65)',
});

function AdaptiveShell({
  title,
  subtitle,
  navigation,
  selectedId,
  onSelect,
  children,
  navigationLabel = 'Sections',
  sidebarBreakpoint = 840,
  sidebarWidth = 208,
  colors: overrides,
  style,
  contentStyle,
  onContentLayout,
  testID,
}) {
  length(sidebarBreakpoint, 'sidebarBreakpoint', false);
  length(sidebarWidth, 'sidebarWidth', false);
  if (
    !navigation.length ||
    new Set(navigation.map((item) => item.id)).size !== navigation.length ||
    navigation.some((item) => !item.id || !item.label) ||
    !navigation.some((item) => item.id === selectedId)
  ) {
    throw new RangeError(
      'navigation requires unique nonempty IDs and labels, and must contain selectedId.'
    );
  }
  const window = useWindowDimensions();
  const size = useElementSize();
  const width = size.width || window.width;
  const height = size.height || window.height;
  const wide = width >= sidebarBreakpoint && width > sidebarWidth;
  const short = getWindowSizeClass({ width, height }).height === 'compact';
  const [{ menuOpen }, menuStore] = useLocalStore({ menuOpen: false });
  const menuButtonRef = React.useRef(null);
  const navigationRefs = React.useRef({});
  const dialogNavigationRefs = React.useRef({});
  const focusOwner = React.useRef(null);
  const previousWide = React.useRef(wide);
  const menuWasVisible = React.useRef(false);
  // Capture before the native display:none commit can blur the old control.
  const focusBeforeLayout = focusOwner.current;
  const colors = { ...defaultAdaptiveColors, ...overrides };
  const closeMenu = () => menuStore.setState({ menuOpen: false });
  const restoreNavigationFocus = () => {
    const target = wide ? navigationRefs.current[selectedId] : menuButtonRef.current;
    target?.focus();
  };
  React.useEffect(() => {
    if (wide) menuStore.setState({ menuOpen: false });
  }, [wide, menuStore]);
  React.useEffect(() => {
    if (previousWide.current !== wide && focusBeforeLayout !== null && !menuWasVisible.current)
      restoreNavigationFocus();
    previousWide.current = wide;
  }, [wide]);
  React.useEffect(() => {
    const visible = menuOpen && !wide;
    if (visible && !menuWasVisible.current) dialogNavigationRefs.current[selectedId]?.focus();
    if (!visible && menuWasVisible.current) restoreNavigationFocus();
    menuWasVisible.current = visible;
  }, [menuOpen, wide, selectedId]);
  const navigationButtons = (inDialog = false) =>
    navigation.map((item) =>
      h(
        Pressable,
        {
          key: item.id,
          ref: (value) => {
            (inDialog ? dialogNavigationRefs : navigationRefs).current[item.id] = value;
          },
          onFocus: () => {
            focusOwner.current = 'navigation';
          },
          onBlur: () => {
            focusOwner.current = null;
          },
          onPress: () => {
            onSelect(item.id);
            closeMenu();
          },
          accessibilityRole: 'button',
          accessibilityLabel: item.label,
          accessibilityState: { selected: selectedId === item.id },
          style: ({ pressed }) => ({
            minHeight: 44,
            paddingHorizontal: 14,
            paddingVertical: 12,
            borderRadius: 5,
            marginBottom: 6,
            backgroundColor: selectedId === item.id || pressed ? colors.selected : colors.surface,
          }),
        },
        h(
          Text,
          {
            style: {
              color: selectedId === item.id ? colors.selectedText : colors.text,
              fontSize: 16,
              lineHeight: 23,
              fontWeight: selectedId === item.id ? '600' : '400',
            },
          },
          item.label
        )
      )
    );
  return h(
    View,
    {
      testID,
      collapsable: false,
      onLayout: size.onLayout,
      style: [
        {
          flex: 1,
          width: '100%',
          height: '100%',
          minWidth: 0,
          minHeight: 0,
          backgroundColor: colors.background,
        },
        style,
      ],
    },
    h(
      View,
      {
        style: {
          flexDirection: 'row',
          alignItems: 'center',
          paddingHorizontal: short ? 16 : 24,
          paddingVertical: short ? 8 : 16,
          borderBottomWidth: 1,
          borderBottomColor: colors.border,
        },
      },
      h(
        View,
        { style: { flex: 1, minWidth: 0, marginRight: 12 } },
        h(
          Text,
          {
            accessibilityRole: 'header',
            style: {
              color: colors.text,
              fontSize: short ? 20 : 25,
              lineHeight: short ? 27 : 32,
              fontWeight: '600',
            },
          },
          title
        ),
        subtitle && !short
          ? h(
              Text,
              { style: { color: colors.muted, fontSize: 14, lineHeight: 21, marginTop: 3 } },
              subtitle
            )
          : null
      ),
      h(
        Pressable,
        {
          ref: menuButtonRef,
          onPress: () => menuStore.setState({ menuOpen: true }),
          onFocus: () => {
            focusOwner.current = 'menu';
          },
          onBlur: () => {
            focusOwner.current = null;
          },
          accessibilityRole: 'button',
          accessibilityLabel: navigationLabel,
          accessibilityState: { expanded: menuOpen },
          style: {
            display: wide ? 'none' : 'flex',
            minHeight: 44,
            paddingHorizontal: 14,
            paddingVertical: 10,
            backgroundColor: colors.surface,
            borderColor: colors.border,
            borderWidth: 1,
            borderRadius: 5,
            justifyContent: 'center',
          },
        },
        h(Text, { style: { color: colors.text, fontSize: 16 } }, navigationLabel)
      )
    ),
    h(
      View,
      { style: { flex: 1, minHeight: 0, minWidth: 0, flexDirection: 'row' } },
      h(
        View,
        {
          style: {
            display: wide ? 'flex' : 'none',
            width: sidebarWidth,
            borderRightWidth: 1,
            borderRightColor: colors.border,
            padding: 12,
            backgroundColor: colors.background,
          },
        },
        h(
          ScrollView,
          { style: { flex: 1 }, contentContainerStyle: { paddingBottom: 8 } },
          ...navigationButtons()
        )
      ),
      h(
        View,
        {
          collapsable: false,
          onLayout: onContentLayout,
          style: [{ flex: 1, minWidth: 0, minHeight: 0, padding: short ? 12 : 20 }, contentStyle],
        },
        children
      )
    ),
    h(
      Modal,
      { visible: menuOpen && !wide, transparent: true, onRequestClose: closeMenu },
      h(
        View,
        {
          style: {
            flex: 1,
            padding: 16,
            justifyContent: 'center',
            alignItems: 'center',
            backgroundColor: colors.backdrop,
          },
        },
        h(
          View,
          {
            accessibilityViewIsModal: true,
            style: {
              width: '100%',
              maxWidth: 360,
              maxHeight: Math.max(0, height - 32),
              backgroundColor: colors.surface,
              borderRadius: 6,
              padding: 16,
            },
          },
          h(
            ScrollView,
            { style: { flexShrink: 1 }, contentContainerStyle: { paddingBottom: 8 } },
            h(
              Text,
              {
                accessibilityRole: 'header',
                style: { color: colors.text, fontSize: 20, lineHeight: 28, marginBottom: 16 },
              },
              navigationLabel
            ),
            ...navigationButtons(true),
            h(
              Pressable,
              {
                onPress: closeMenu,
                accessibilityRole: 'button',
                accessibilityLabel: 'Close navigation',
                style: {
                  minHeight: 44,
                  padding: 12,
                  borderWidth: 1,
                  borderColor: colors.border,
                  borderRadius: 5,
                  marginTop: 8,
                },
              },
              h(Text, { style: { color: colors.text, fontSize: 16 } }, 'Close navigation')
            )
          )
        )
      )
    )
  );
}

module.exports = { AdaptiveShell, defaultAdaptiveColors };
