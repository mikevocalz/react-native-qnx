/**
 * The base view config for QNX.
 *
 * React Native's `Libraries/NativeComponent/BaseViewConfig.js` resolves by
 * platform extension to `.ios.js` or `.android.js`. With neither present for an
 * unknown platform the import is undefined, and the first thing React touches
 * is `bubblingEventTypes`.
 *
 * The event tables below are the platform-independent ones React's event
 * system needs; the iOS file adds accessibility and force-touch events that
 * have no QNX equivalent yet, and those are deliberately absent rather than
 * declared and unimplemented. `validAttributes` comes from React Native's own
 * generic style attribute table, so the prop surface is React Native's rather
 * than a list invented here.
 *
 * This grows as the mounting layer learns props: today it applies layout, and
 * `docs/component-support.md` is where the gaps are recorded.
 */

'use strict';

const ReactNativeStyleAttributes =
  require('react-native/Libraries/Components/View/ReactNativeStyleAttributes').default;

const bubblingEventTypes = {
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
};

module.exports = {
  bubblingEventTypes,
  directEventTypes,
  validAttributes: {
    ...ReactNativeStyleAttributes,
    style: ReactNativeStyleAttributes,
    collapsable: true,
    nativeID: true,
    testID: true,
    pointerEvents: true,
    onLayout: true,
  },
};
module.exports.default = module.exports;
