// Adapter for the native RNQnxDrawer (Qt Quick item). JavaScript owns the
// `open` state; the native item animates toward it, and reports user gestures
// (edge drag, scrim click, Escape) as requests that this adapter turns into
// onOpen and onClose, the callbacks expo-router's DrawerView supplies.
const React = require('react');
const { Animated, Easing, StyleSheet, View } = require('react-native');
const { nativeComponent } = require('./native');
const { getSwipeExclusionLeft, subscribeSwipeExclusion } = require('./swipeExclusion');
const { DrawerProgressContext } = require('./drawerContexts');
const { dismissKeyboardOnDrag, warnUnsupportedDrawerProps } = require('./drawerOptions');

const NativeDrawer = nativeComponent('RNQnxDrawer', {
  validAttributes: {
    open: true,
    drawerWidth: true,
    side: true,
    drawerType: true,
    swipeEnabled: true,
    scrimColor: true,
    edgeWidth: true,
    swipeMinDistance: true,
    swipeExclusionLeft: true,
    accessibilityLabel: true,
  },
  directEventTypes: {
    topRequestChange: { registrationName: 'onRequestChange' }, // { open, reason }
    topTransitionEnd: { registrationName: 'onTransitionEnd' },
    topGestureStart: { registrationName: 'onGestureStart' },
    topGestureEnd: { registrationName: 'onGestureEnd' },
    topGestureCancel: { registrationName: 'onGestureCancel' },
  },
});

function scrimColorOf(overlayStyle) {
  const flat = StyleSheet.flatten(overlayStyle);
  return (flat && flat.backgroundColor) || 'rgba(0,0,0,0.5)';
}

// The drawer's progress for useDrawerProgress(): 0 closed, 1 open. The native
// item animates on its own, so this value follows `open` with the same easing
// and a matching duration instead of being read back from native.
function useProgress(open) {
  const progress = React.useRef(null);
  if (progress.current === null) progress.current = new Animated.Value(open ? 1 : 0);
  React.useEffect(() => {
    const animation = Animated.timing(progress.current, {
      toValue: open ? 1 : 0,
      duration: 220,
      easing: Easing.out(Easing.cubic),
      useNativeDriver: false,
    });
    animation.start();
    return () => animation.stop();
  }, [open]);
  return progress.current;
}

function QtDrawer({
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
  swipeMinDistance,
  keyboardDismissMode,
  onGestureStart,
  onGestureEnd,
  onGestureCancel,
  style,
  children,
  ...rest
}) {
  warnUnsupportedDrawerProps(rest);
  const progress = useProgress(open);
  const width =
    drawerWidth ??
    StyleSheet.flatten(drawerStyle)?.width ??
    Math.min(320, Math.max(240, Math.round((layout?.width ?? 800) * 0.8)));
  const [exclusion, setExclusion] = React.useState(getSwipeExclusionLeft);
  React.useEffect(() => {
    const unsubscribe = subscribeSwipeExclusion(setExclusion);
    // The tab bar is a child of the drawer, so its effect has already set the
    // rail width when this subscription starts. Read the current value, or the
    // edge strip stays at x = 0 on top of the rail.
    setExclusion(getSwipeExclusionLeft());
    return unsubscribe;
  }, []);
  const last = React.useRef(open);

  // The native item animates on its own; JS only learns the start of a
  // transition from the prop change.
  React.useEffect(() => {
    if (last.current !== open) onTransitionStart?.(!open);
    last.current = open;
  }, [open, onTransitionStart]);

  // A permanent drawer is always on screen beside the content: a row, with no
  // scrim or gestures, so there is nothing for a native item to do.
  if (drawerType === 'permanent') {
    return React.createElement(
      DrawerProgressContext.Provider,
      { value: progress },
      React.createElement(
        View,
        {
          style: [
            { flex: 1, flexDirection: drawerPosition === 'right' ? 'row-reverse' : 'row' },
            style,
          ],
        },
        React.createElement(View, { style: [{ width }, drawerStyle] }, renderDrawerContent()),
        React.createElement(View, { style: { flex: 1 } }, children)
      )
    );
  }

  return React.createElement(
    DrawerProgressContext.Provider,
    { value: progress },
    React.createElement(
      NativeDrawer,
      {
        style: [{ flex: 1 }, style],
        open,
        drawerWidth: width,
        side: drawerPosition,
        drawerType: drawerType === 'slide' || drawerType === 'back' ? drawerType : 'front',
        swipeEnabled,
        scrimColor: scrimColorOf(overlayStyle),
        edgeWidth: swipeEdgeWidth,
        // 0 keeps the native 40% rule alone; undefined means react-native-drawer-layout's own default.
        swipeMinDistance: swipeMinDistance ?? 0,
        swipeExclusionLeft: exclusion,
        accessibilityLabel: overlayAccessibilityLabel ?? 'Close drawer',
        onRequestChange: (event) => (event.nativeEvent.open ? onOpen?.() : onClose?.()),
        onTransitionEnd: (event) => onTransitionEnd?.(!event.nativeEvent.open),
        onGestureStart: () => {
          dismissKeyboardOnDrag(keyboardDismissMode);
          onGestureStart?.();
        },
        onGestureEnd: () => onGestureEnd?.(),
        onGestureCancel: () => onGestureCancel?.(),
      },
      // Yoga lays both children out; the native item only moves them. Child 0 is
      // the panel, child 1 the content, both pinned to the left edge. The native
      // item moves the panel to the right edge for side 'right'.
      React.createElement(
        View,
        // drawerStyle (background, border, radius, shadow) first; the placement
        // and width Yoga needs for the native item come last and always win.
        {
          style: [drawerStyle, { position: 'absolute', top: 0, bottom: 0, left: 0, width }],
          collapsable: false,
        },
        renderDrawerContent()
      ),
      React.createElement(
        View,
        {
          style: { position: 'absolute', top: 0, bottom: 0, left: 0, right: 0 },
          collapsable: false,
        },
        children
      )
    )
  );
}

module.exports = { QtDrawer };
