// Drawer drawn from React Native views. Used when the host has no native
// RNQnxDrawer, and as the reference for what the native component does:
// scrim, slide animation, an edge strip that opens it by mouse or touch drag,
// Escape to close. Animated with the JavaScript driver (no Reanimated or
// gesture-handler).
const React = require('react');
const { Animated, Easing, PanResponder, Pressable, StyleSheet, View } = require('react-native');
const { getSwipeExclusionLeft, subscribeSwipeExclusion } = require('./swipeExclusion');

const DURATION = 220;

function useExclusionLeft() {
  const [value, setValue] = React.useState(getSwipeExclusionLeft);
  React.useEffect(() => subscribeSwipeExclusion(setValue), []);
  return value;
}

function JsDrawer({
  open,
  onOpen,
  onClose,
  onTransitionStart,
  onTransitionEnd,
  renderDrawerContent,
  layout,
  drawerPosition = 'left',
  drawerType = 'front',
  drawerWidth,
  drawerStyle,
  overlayStyle,
  overlayAccessibilityLabel,
  swipeEnabled = true,
  swipeEdgeWidth = 24,
  style,
  children,
}) {
  const right = drawerPosition === 'right';
  const width =
    drawerWidth ?? Math.min(320, Math.max(240, Math.round((layout?.width ?? 800) * 0.8)));
  const progress = React.useRef(new Animated.Value(open ? 1 : 0)).current;
  const [visible, setVisible] = React.useState(open);
  const exclusion = useExclusionLeft();
  const openRef = React.useRef(open);
  const callbacks = React.useRef({});
  callbacks.current = { onOpen, onClose, onTransitionStart, onTransitionEnd };

  React.useEffect(() => {
    openRef.current = open;
    if (open) setVisible(true);
    callbacks.current.onTransitionStart?.(!open);
    const animation = Animated.timing(progress, {
      toValue: open ? 1 : 0,
      duration: DURATION,
      easing: Easing.out(Easing.cubic),
      useNativeDriver: false,
    });
    animation.start(({ finished }) => {
      if (!finished) return;
      if (!open) setVisible(false);
      callbacks.current.onTransitionEnd?.(!open);
    });
    return () => animation.stop();
  }, [open, progress]);

  const dragStart = React.useRef(0);
  const edgePan = React.useMemo(
    () =>
      PanResponder.create({
        onMoveShouldSetPanResponder: (_e, g) =>
          Math.abs(g.dx) > 6 && Math.abs(g.dx) > Math.abs(g.dy),
        onPanResponderGrant: () => {
          dragStart.current = openRef.current ? 1 : 0;
        },
        onPanResponderMove: (_e, g) => {
          const delta = (right ? -g.dx : g.dx) / width;
          progress.setValue(Math.max(0, Math.min(1, dragStart.current + delta)));
        },
        onPanResponderRelease: (_e, g) => {
          const delta = (right ? -g.dx : g.dx) / width;
          const target = dragStart.current + delta;
          if (target > 0.4) callbacks.current.onOpen?.();
          else callbacks.current.onClose?.();
          // Snap back when the request does not change `open`.
          Animated.timing(progress, {
            toValue: openRef.current ? 1 : 0,
            duration: DURATION,
            useNativeDriver: false,
          }).start();
        },
      }),
    [progress, right, width]
  );

  if (drawerType === 'permanent') {
    return React.createElement(
      View,
      { style: [styles.fill, { flexDirection: right ? 'row-reverse' : 'row' }, style] },
      React.createElement(View, { style: [{ width }, drawerStyle] }, renderDrawerContent()),
      React.createElement(View, { style: styles.fill }, children)
    );
  }

  const offscreen = right ? width : -width;
  const translate = progress.interpolate({ inputRange: [0, 1], outputRange: [offscreen, 0] });
  const pushed = progress.interpolate({
    inputRange: [0, 1],
    outputRange: [0, right ? -width : width],
  });
  const slide = drawerType === 'slide';

  return React.createElement(
    View,
    { style: [styles.fill, style] },
    React.createElement(
      Animated.View,
      { style: [styles.fill, slide && { transform: [{ translateX: pushed }] }] },
      children
    ),
    visible &&
      React.createElement(Animated.View, {
        pointerEvents: 'box-none',
        style: [StyleSheet.absoluteFill, { opacity: progress }],
        children: React.createElement(Pressable, {
          accessibilityLabel: overlayAccessibilityLabel ?? 'Close drawer',
          accessibilityRole: 'button',
          onPress: () => onClose?.(),
          style: [StyleSheet.absoluteFill, { backgroundColor: 'rgba(0,0,0,0.5)' }, overlayStyle],
        }),
      }),
    React.createElement(
      Animated.View,
      {
        ...(open ? edgePan.panHandlers : {}),
        pointerEvents: visible ? 'auto' : 'none',
        style: [
          styles.panel,
          right ? { right: 0 } : { left: 0 },
          { width, transform: [{ translateX: translate }] },
          drawerStyle,
        ],
      },
      visible ? renderDrawerContent() : null
    ),
    swipeEnabled &&
      !open &&
      React.createElement(View, {
        ...edgePan.panHandlers,
        style: [
          styles.edge,
          right ? { right: 0 } : { left: right ? 0 : exclusion },
          { width: swipeEdgeWidth },
        ],
      })
  );
}

const styles = StyleSheet.create({
  fill: { flex: 1 },
  panel: { position: 'absolute', top: 0, bottom: 0 },
  edge: { position: 'absolute', top: 0, bottom: 0 },
});

module.exports = { JsDrawer };
