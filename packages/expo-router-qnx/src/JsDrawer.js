// Drawer drawn from React Native views. Used when the host has no native
// RNQnxDrawer, and as the reference for what the native component does:
// scrim, slide animation, an edge strip that opens it by mouse or touch drag,
// Escape to close. Animated with the JavaScript driver (no Reanimated or
// gesture-handler).
const React = require('react');
const { Animated, Easing, PanResponder, Pressable, StyleSheet, View } = require('react-native');
const { DrawerProgressContext } = require('./drawerContexts');
const { getSwipeExclusionLeft, subscribeSwipeExclusion } = require('./swipeExclusion');
const { dismissKeyboardOnDrag, warnUnsupportedDrawerProps } = require('./drawerOptions');

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
  swipeMinDistance = 0,
  keyboardDismissMode,
  onGestureStart,
  onGestureEnd,
  onGestureCancel,
  style,
  children,
  ...rest
}) {
  warnUnsupportedDrawerProps(rest);
  const right = drawerPosition === 'right';
  const width =
    drawerWidth ??
    StyleSheet.flatten(drawerStyle)?.width ??
    Math.min(320, Math.max(240, Math.round((layout?.width ?? 800) * 0.8)));
  const progress = React.useRef(new Animated.Value(open ? 1 : 0)).current;
  const [visible, setVisible] = React.useState(open);
  const exclusion = useExclusionLeft();
  const openRef = React.useRef(open);
  const callbacks = React.useRef({});
  callbacks.current = {
    onOpen,
    onClose,
    onTransitionStart,
    onTransitionEnd,
    onGestureStart,
    onGestureEnd,
    onGestureCancel,
    keyboardDismissMode,
  };

  const mounted = React.useRef(false);
  React.useEffect(() => {
    openRef.current = open;
    if (open) setVisible(true);
    // The first run only sets the resting position; no transition happens.
    if (mounted.current) callbacks.current.onTransitionStart?.(!open);
    const animation = Animated.timing(progress, {
      toValue: open ? 1 : 0,
      duration: DURATION,
      easing: Easing.out(Easing.cubic),
      useNativeDriver: false,
    });
    animation.start(({ finished }) => {
      if (!finished) return;
      if (!open) setVisible(false);
      if (mounted.current) callbacks.current.onTransitionEnd?.(!open);
    });
    mounted.current = true;
    return () => animation.stop();
  }, [open, progress]);

  const dragStart = React.useRef(0);
  // Animate to wherever the `open` prop says, after a drag the parent did not
  // confirm or a terminated gesture.
  const snapBack = () => {
    Animated.timing(progress, {
      toValue: openRef.current ? 1 : 0,
      duration: DURATION,
      useNativeDriver: false,
    }).start(({ finished }) => {
      if (finished && !openRef.current) setVisible(false);
    });
  };
  const edgePan = React.useMemo(
    () =>
      PanResponder.create({
        onMoveShouldSetPanResponder: (_e, g) =>
          Math.abs(g.dx) > 6 && Math.abs(g.dx) > Math.abs(g.dy),
        onPanResponderGrant: () => {
          dragStart.current = openRef.current ? 1 : 0;
          setVisible(true);
          dismissKeyboardOnDrag(callbacks.current.keyboardDismissMode);
          callbacks.current.onGestureStart?.();
        },
        onPanResponderMove: (_e, g) => {
          const delta = (right ? -g.dx : g.dx) / width;
          progress.setValue(Math.max(0, Math.min(1, dragStart.current + delta)));
        },
        onPanResponderRelease: (_e, g) => {
          const delta = (right ? -g.dx : g.dx) / width;
          const target = dragStart.current + delta;
          callbacks.current.onGestureEnd?.();
          // Past 40% of the width in the direction of travel flips the state,
          // and so does a drag of at least swipeMinDistance px if it is not shorter.
          const farEnough = Math.abs(delta) * width >= swipeMinDistance;
          const wantOpen = !farEnough
            ? dragStart.current === 1
            : dragStart.current === 1
              ? target >= 0.6
              : target > 0.4;
          if (wantOpen && !openRef.current) callbacks.current.onOpen?.();
          else if (!wantOpen && openRef.current) callbacks.current.onClose?.();
          snapBack();
        },
        onPanResponderTerminate: () => {
          callbacks.current.onGestureCancel?.();
          snapBack();
        },
      }),
    [progress, right, width, swipeMinDistance]
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
  const slide = drawerType === 'slide' || drawerType === 'back';
  // `back` keeps the panel where it is, under the content that slides away.
  const back = drawerType === 'back';
  const panel = React.createElement(
    Animated.View,
    {
      ...(open && swipeEnabled ? edgePan.panHandlers : {}),
      pointerEvents: visible ? 'auto' : 'none',
      style: [
        styles.panel,
        right ? { right: 0 } : { left: 0 },
        { width, transform: [{ translateX: back ? 0 : translate }] },
        drawerStyle,
      ],
    },
    visible ? renderDrawerContent() : null
  );

  return React.createElement(
    DrawerProgressContext.Provider,
    { value: progress },
    React.createElement(
      View,
      { style: [styles.fill, style] },
      back && panel,
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
      !back && panel,
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
    )
  );
}

const styles = StyleSheet.create({
  fill: { flex: 1 },
  panel: { position: 'absolute', top: 0, bottom: 0 },
  edge: { position: 'absolute', top: 0, bottom: 0 },
});

module.exports = { JsDrawer };
