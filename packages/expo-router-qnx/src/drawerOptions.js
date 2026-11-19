// Drawer props react-native-drawer-layout takes that no QNX drawer can honor,
// and the helpers both drawers share for the ones they can.
const { Keyboard } = require('react-native');
const { warnUnsupportedOptions } = require('./devWarnings');

const UNSUPPORTED = {
  hideStatusBarOnOpen: {
    reason:
      'the QNX window has no status bar to hide (StatusBar is a no-op, see docs/component-support.md).',
    when: Boolean,
  },
  statusBarAnimation: {
    reason:
      'the QNX window has no status bar to animate (StatusBar is a no-op, see docs/component-support.md).',
  },
  configureGestureHandler: {
    reason:
      'react-native-gesture-handler is not available on QNX; the drawer drag is handled by the Qt item or PanResponder.',
  },
};

/** Warns once for each unsupported drawer prop that is set. */
function warnUnsupportedDrawerProps(props) {
  warnUnsupportedOptions(props, UNSUPPORTED);
}

/** `keyboardDismissMode` 'on-drag' (the default) blurs the focused input when a drag starts. */
function dismissKeyboardOnDrag(mode) {
  if (mode === 'none') return;
  Keyboard?.dismiss?.();
}

module.exports = { warnUnsupportedDrawerProps, dismissKeyboardOnDrag };
