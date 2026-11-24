/**
 * QNX `Button`.
 *
 * React Native's `Libraries/Components/Button.js` styles itself with
 * `Platform.select({ios, android})` and decides where `color` goes with
 * `Platform.OS === 'ios'`. `Platform.OS` is `'qnx'`, so neither branch matched:
 * the title had no color (black, unreadable on a dark background), the disabled
 * look was missing, and `color` painted the button background instead of the
 * title.
 *
 * This copy keeps upstream's structure and props and fixes only the styling to
 * the iOS look (blue title, `color` sets the title color, dimmed when
 * disabled). It is plain CommonJS like the other QNX overrides, and it keeps
 * `TouchableOpacity` as the touchable, which is what upstream picks off Android.
 */

'use strict';

const React = require('react');
const StyleSheet = require('react-native/Libraries/StyleSheet/StyleSheet').default;
const Text = require('react-native/Libraries/Text/Text').default;
const TouchableOpacity =
  require('react-native/Libraries/Components/Touchable/TouchableOpacity').default;
const View = require('react-native/Libraries/Components/View/View').default;

const Button = React.forwardRef(function Button(props, ref) {
  const state = props.accessibilityState;
  const accessibilityState = {
    busy: props['aria-busy'] ?? (state && state.busy),
    checked: props['aria-checked'] ?? (state && state.checked),
    disabled: props['aria-disabled'] ?? (state && state.disabled),
    expanded: props['aria-expanded'] ?? (state && state.expanded),
    selected: props['aria-selected'] ?? (state && state.selected),
  };
  const disabled = props.disabled != null ? props.disabled : accessibilityState.disabled;
  if (disabled !== accessibilityState.disabled) accessibilityState.disabled = disabled;

  const textStyles = [styles.text];
  // Upstream's iOS rule: `color` is the title color.
  if (props.color) textStyles.push({ color: props.color });
  if (disabled === true) textStyles.push(styles.textDisabled);

  // Upstream's invariant, without the dependency: this package does not list it.
  if (typeof props.title !== 'string') {
    throw new Error('The title prop of a Button must be a string');
  }

  // `no` would hide the title text from accessibility, so upstream maps it.
  const importantForAccessibility =
    props.importantForAccessibility === 'no'
      ? 'no-hide-descendants'
      : props.importantForAccessibility;

  return React.createElement(
    TouchableOpacity,
    {
      accessible: props.accessible,
      accessibilityActions: props.accessibilityActions,
      onAccessibilityAction: props.onAccessibilityAction,
      accessibilityLabel: props['aria-label'] || props.accessibilityLabel,
      accessibilityHint: props.accessibilityHint,
      accessibilityLanguage: props.accessibilityLanguage,
      accessibilityRole: 'button',
      accessibilityState,
      importantForAccessibility,
      hasTVPreferredFocus: props.hasTVPreferredFocus,
      nextFocusDown: props.nextFocusDown,
      nextFocusForward: props.nextFocusForward,
      nextFocusLeft: props.nextFocusLeft,
      nextFocusRight: props.nextFocusRight,
      nextFocusUp: props.nextFocusUp,
      testID: props.testID,
      disabled,
      onPress: props.onPress,
      touchSoundDisabled: props.touchSoundDisabled,
      ref,
    },
    React.createElement(
      View,
      null,
      React.createElement(Text, { style: textStyles, disabled }, props.title)
    )
  );
});

Button.displayName = 'Button';

const styles = StyleSheet.create({
  text: {
    textAlign: 'center',
    margin: 8,
    // iOS blue from the Human Interface Guidelines, as upstream uses on iOS.
    color: '#007AFF',
    fontSize: 18,
  },
  textDisabled: {
    color: '#cdcdcd',
  },
});

module.exports = Button;
module.exports.default = Button;
