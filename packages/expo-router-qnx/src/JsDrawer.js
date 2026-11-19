// Drawer drawn from React Native views. Used when the host has no native
// RNQnxDrawer, and as the reference for what the native component does:
// scrim, slide animation, an edge strip that opens it by mouse or touch drag,
// Escape to close. Animated with the JavaScript driver (no Reanimated or
// gesture-handler).
const React = require('react');
const { useStore } = require('zustand');
const { createStore } = require('zustand/vanilla');
const { Animated, Easing, PanResponder, Pressable, StyleSheet, View } = require('react-native');
const { DrawerProgressContext } = require('./drawerContexts');
const { getSwipeExclusionLeft, subscribeSwipeExclusion } = require('./swipeExclusion');
const { dismissKeyboardOnDrag, warnUnsupportedDrawerProps } = require('./drawerOptions');

const DURATION = 220;

function useDrawerState(open) {
  const store = React.useMemo(
    () =>
      createStore(() => ({
        visible: open,
        exclusion: getSwipeExclusionLeft(),
        dragStart: 0,
        dragStartedOpen: open,
      })),
    []
  );
  React.useEffect(() => {
    const update = (exclusion) => store.setState({ exclusion });
    const unsubscribe = subscribeSwipeExclusion(update);
    update(getSwipeExclusionLeft());
    return unsubscribe;
  }, [store]);
  const visible = useStore(store, (state) => state.visible);
  const exclusion = useStore(store, (state) => state.exclusion);
  const setVisible = React.useCallback((visible) => store.setState({ visible }), [store]);
  return { store, visible, exclusion, setVisible };
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
  const progressRef = React.useRef(null);
  if (progressRef.current === null) progressRef.current = new Animated.Value(open ? 1 : 0);
  const progress = progressRef.current;
  const { store: drawerStore, visible, exclusion, setVisible } = useDrawerState(open);
  const pendingTimer = React.useRef(null);
  const settlement = React.useRef(null);
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
    clearTimeout(pendingTimer.current);
    pendingTimer.current = null;
    settlement.current?.stop();
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
  }, [open, progress, setVisible]);
  React.useEffect(
    () => () => {
      clearTimeout(pendingTimer.current);
      settlement.current?.stop();
    },
    []
  );

  // Animate to wherever the `open` prop says, after a drag the parent did not
  // confirm or a terminated gesture.
  const settle = (next) => {
    if (next) setVisible(true);
    settlement.current?.stop();
    const animation = Animated.timing(progress, {
      toValue: next ? 1 : 0,
      duration: DURATION,
      useNativeDriver: false,
    });
    settlement.current = animation;
    animation.start(({ finished }) => {
      if (finished && !next) setVisible(false);
    });
  };
  const snapBack = () => settle(openRef.current);
  const edgePan = React.useMemo(
    () =>
      PanResponder.create({
        onMoveShouldSetPanResponder: (_e, g) =>
          Math.abs(g.dx) > 6 && Math.abs(g.dx) > Math.abs(g.dy),
        onPanResponderGrant: () => {
          clearTimeout(pendingTimer.current);
          pendingTimer.current = null;
          settlement.current?.stop();
          drawerStore.setState({ dragStartedOpen: openRef.current });
          progress.stopAnimation((value) => {
            drawerStore.setState({ dragStart: value });
          });
          setVisible(true);
          dismissKeyboardOnDrag(callbacks.current.keyboardDismissMode);
          callbacks.current.onGestureStart?.();
        },
        onPanResponderMove: (_e, g) => {
          const delta = (right ? -g.dx : g.dx) / width;
          progress.setValue(Math.max(0, Math.min(1, drawerStore.getState().dragStart + delta)));
        },
        onPanResponderRelease: (_e, g) => {
          const delta = (right ? -g.dx : g.dx) / width;
          callbacks.current.onGestureEnd?.();
          // PanResponder reports px/ms; QtDrawer's 500 px/s fling threshold
          // takes precedence over swipeMinDistance on both drawer sides.
          const velocity = (right ? -1 : 1) * (g.vx ?? 0);
          const farEnough = Math.abs(delta) * width >= swipeMinDistance;
          const startedOpen = drawerStore.getState().dragStartedOpen;
          const wantOpen =
            Math.abs(velocity) > 0.5
              ? velocity > 0
              : !farEnough
                ? startedOpen
                : startedOpen
                  ? delta >= -0.4
                  : delta > 0.4;
          if (wantOpen === openRef.current) {
            snapBack();
            return;
          }
          // Settle toward the requested state while the controlled parent
          // confirms it. Revert only if it stays unconfirmed, like QtDrawer.
          settle(wantOpen);
          pendingTimer.current = setTimeout(() => {
            pendingTimer.current = null;
            snapBack();
          }, 400);
          if (wantOpen) callbacks.current.onOpen?.();
          else callbacks.current.onClose?.();
        },
        onPanResponderTerminate: () => {
          clearTimeout(pendingTimer.current);
          pendingTimer.current = null;
          callbacks.current.onGestureCancel?.();
          snapBack();
        },
      }),
    [progress, right, width, swipeMinDistance, setVisible, drawerStore]
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
  const scrim =
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
    });

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
        children,
        back && scrim
      ),
      !back && scrim,
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
