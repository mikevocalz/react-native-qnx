// Adapter for the native RNQnxDrawer (Qt Quick item). JavaScript owns the
// `open` state; the native item animates toward it, and reports user gestures
// (edge drag, scrim click, Escape) as requests that this adapter turns into
// onOpen and onClose, the callbacks expo-router's DrawerView supplies.
const React = require('react');
const { StyleSheet, View } = require('react-native');
const { nativeComponent } = require('./native');
const { getSwipeExclusionLeft, subscribeSwipeExclusion } = require('./swipeExclusion');

const NativeDrawer = nativeComponent('RNQnxDrawer', {
  validAttributes: {
    open: true,
    drawerWidth: true,
    side: true,
    drawerType: true,
    swipeEnabled: true,
    scrimColor: true,
    edgeWidth: true,
    swipeExclusionLeft: true,
    accessibilityLabel: true,
  },
  directEventTypes: {
    topRequestChange: { registrationName: 'onRequestChange' }, // { open, reason }
    topTransitionEnd: { registrationName: 'onTransitionEnd' },
  },
});

function scrimColorOf(overlayStyle) {
  const flat = StyleSheet.flatten(overlayStyle);
  return (flat && flat.backgroundColor) || 'rgba(0,0,0,0.5)';
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
  style,
  children,
}) {
  const width =
    drawerWidth ??
    StyleSheet.flatten(drawerStyle)?.width ??
    Math.min(320, Math.max(240, Math.round((layout?.width ?? 800) * 0.8)));
  const [exclusion, setExclusion] = React.useState(getSwipeExclusionLeft);
  React.useEffect(() => subscribeSwipeExclusion(setExclusion), []);
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
      View,
      {
        style: [
          { flex: 1, flexDirection: drawerPosition === 'right' ? 'row-reverse' : 'row' },
          style,
        ],
      },
      React.createElement(View, { style: [{ width }, drawerStyle] }, renderDrawerContent()),
      React.createElement(View, { style: { flex: 1 } }, children)
    );
  }

  return React.createElement(
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
      swipeExclusionLeft: exclusion,
      accessibilityLabel: overlayAccessibilityLabel ?? 'Close drawer',
      onRequestChange: (event) => (event.nativeEvent.open ? onOpen?.() : onClose?.()),
      onTransitionEnd: (event) => onTransitionEnd?.(!event.nativeEvent.open),
    },
    // Yoga lays both children out; the native item only moves them. Child 0 is
    // the panel, child 1 the content, both pinned to the left edge. The native
    // item moves the panel to the right edge for side 'right'.
    React.createElement(
      View,
      { style: { position: 'absolute', top: 0, bottom: 0, left: 0, width }, collapsable: false },
      renderDrawerContent()
    ),
    React.createElement(
      View,
      { style: { position: 'absolute', top: 0, bottom: 0, left: 0, right: 0 }, collapsable: false },
      children
    )
  );
}

module.exports = { QtDrawer };
