/**
 * The base view config for QNX.
 *
 * React Native's `Libraries/NativeComponent/BaseViewConfig.js` resolves by
 * platform extension to `.ios.js` or `.android.js`. With neither present for an
 * unknown platform the import is undefined, and the first thing React touches
 * is `bubblingEventTypes`.
 *
 * The tables describe RN's standard prop/event transport. Native semantic-tree
 * and action behavior remain controlled by the experimental accessibility flag;
 * admitting a prop does not establish an OS accessibility consumer.
 */

'use strict';

const ReactNativeStyleAttributes =
  require('react-native/Libraries/Components/View/ReactNativeStyleAttributes').default;

const bubblingEventTypes = {
  topKeyDown: {
    phasedRegistrationNames: { bubbled: 'onKeyDown', captured: 'onKeyDownCapture' },
  },
  topKeyUp: {
    phasedRegistrationNames: { bubbled: 'onKeyUp', captured: 'onKeyUpCapture' },
  },
  topClick: {
    phasedRegistrationNames: { bubbled: 'onClick', captured: 'onClickCapture' },
  },
  topPointerEnter: {
    phasedRegistrationNames: {
      bubbled: 'onPointerEnter',
      captured: 'onPointerEnterCapture',
      skipBubbling: true,
    },
  },
  topPointerLeave: {
    phasedRegistrationNames: {
      bubbled: 'onPointerLeave',
      captured: 'onPointerLeaveCapture',
      skipBubbling: true,
    },
  },
  topPointerOver: {
    phasedRegistrationNames: { bubbled: 'onPointerOver', captured: 'onPointerOverCapture' },
  },
  topPointerOut: {
    phasedRegistrationNames: { bubbled: 'onPointerOut', captured: 'onPointerOutCapture' },
  },
  topPointerMove: {
    phasedRegistrationNames: { bubbled: 'onPointerMove', captured: 'onPointerMoveCapture' },
  },
  topPress: {
    phasedRegistrationNames: { bubbled: 'onPress', captured: 'onPressCapture' },
  },
  topChange: {
    phasedRegistrationNames: {
      bubbled: 'onChange',
      captured: 'onChangeCapture',
    },
  },
  topFocus: {
    phasedRegistrationNames: { bubbled: 'onFocus', captured: 'onFocusCapture' },
  },
  topBlur: {
    phasedRegistrationNames: { bubbled: 'onBlur', captured: 'onBlurCapture' },
  },
  topTouchStart: {
    phasedRegistrationNames: {
      bubbled: 'onTouchStart',
      captured: 'onTouchStartCapture',
    },
  },
  topTouchMove: {
    phasedRegistrationNames: {
      bubbled: 'onTouchMove',
      captured: 'onTouchMoveCapture',
    },
  },
  topTouchEnd: {
    phasedRegistrationNames: {
      bubbled: 'onTouchEnd',
      captured: 'onTouchEndCapture',
    },
  },
  topTouchCancel: {
    phasedRegistrationNames: {
      bubbled: 'onTouchCancel',
      captured: 'onTouchCancelCapture',
    },
  },
};

const directEventTypes = {
  topLayout: { registrationName: 'onLayout' },
  topAccessibilityAction: { registrationName: 'onAccessibilityAction' },
};

// React's Fabric renderer runs the JavaScript responder system itself, driven by
// the touch events this platform emits. It only attaches these handlers if the
// view config declares them, so `Pressable` and every gesture built on
// `Pressability` depend on this list. Names follow
// `Libraries/NativeComponent/BaseViewConfig.ios.js:391-403`.
const responderEventProps = {
  onStartShouldSetResponder: true,
  onStartShouldSetResponderCapture: true,
  onMoveShouldSetResponder: true,
  onMoveShouldSetResponderCapture: true,
  onResponderGrant: true,
  onResponderReject: true,
  onResponderStart: true,
  onResponderMove: true,
  onResponderEnd: true,
  onResponderRelease: true,
  onResponderTerminate: true,
  onResponderTerminationRequest: true,
  onShouldBlockNativeResponder: true,
};

module.exports = {
  bubblingEventTypes,
  directEventTypes,
  validAttributes: {
    ...responderEventProps,
    ...ReactNativeStyleAttributes,
    style: ReactNativeStyleAttributes,
    collapsable: true,
    focusable: true,
    onKeyDown: true,
    onKeyDownCapture: true,
    onKeyUp: true,
    onKeyUpCapture: true,
    nativeID: true,
    testID: true,
    pointerEvents: true,
    onLayout: true,
    accessible: true,
    accessibilityLabel: true,
    accessibilityHint: true,
    accessibilityRole: true,
    role: true,
    accessibilityState: true,
    accessibilityValue: true,
    accessibilityViewIsModal: true,
    accessibilityElementsHidden: true,
    importantForAccessibility: true,
    accessibilityActions: true,
    onAccessibilityAction: true,
  },
};
module.exports.default = module.exports;
