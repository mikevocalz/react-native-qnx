'use strict';

const Registry = require('react-native/Libraries/TurboModule/TurboModuleRegistry');
const Emitter = require('react-native/Libraries/EventEmitter/RCTDeviceEventEmitter').default;
const { findNodeHandle } = require('react-native/Libraries/ReactNative/RendererProxy');

function capabilityError(message, code = 'ERR_QT_ACCESSIBILITY_UNSUPPORTED') {
  const error = new Error(message);
  error.code = code;
  return error;
}

function native() {
  const provider = Registry.get('QtAccessibilityInfo');
  if (!provider) {
    throw capabilityError(
      'AccessibilityInfo requires RNQNX_ENABLE_PLATFORM_SERVICES and the Qt accessibility bridge.',
      'ERR_QT_ACCESSIBILITY_UNAVAILABLE'
    );
  }
  return provider;
}

function query(method) {
  return new Promise((resolve, reject) => {
    try {
      resolve(native()[method]());
    } catch (error) {
      reject(error);
    }
  });
}

function unavailable(method) {
  return Promise.reject(capabilityError(`AccessibilityInfo.${method} has no Qt system provider.`));
}

const AccessibilityInfo = {
  isScreenReaderEnabled: () => query('isScreenReaderEnabled'),
  isAccessibilityServiceEnabled: () => query('isScreenReaderEnabled'),
  isHighTextContrastEnabled: () => query('isHighTextContrastEnabled'),
  isDarkerSystemColorsEnabled: () => query('isHighTextContrastEnabled'),
  isReduceMotionEnabled: () => unavailable('isReduceMotionEnabled'),
  isBoldTextEnabled: () => unavailable('isBoldTextEnabled'),
  isGrayscaleEnabled: () => unavailable('isGrayscaleEnabled'),
  isInvertColorsEnabled: () => unavailable('isInvertColorsEnabled'),
  isReduceTransparencyEnabled: () => unavailable('isReduceTransparencyEnabled'),
  prefersCrossFadeTransitions: () => unavailable('prefersCrossFadeTransitions'),
  getRecommendedTimeoutMillis: () => unavailable('getRecommendedTimeoutMillis'),
  addEventListener(eventName, handler) {
    if (typeof handler !== 'function')
      throw new TypeError('AccessibilityInfo listener must be a function.');
    const names = {
      change: 'qtScreenReaderChanged',
      screenReaderChanged: 'qtScreenReaderChanged',
      accessibilityServiceChanged: 'qtScreenReaderChanged',
      highTextContrastChanged: 'qtHighTextContrastChanged',
      darkerSystemColorsChanged: 'qtHighTextContrastChanged',
    };
    if (!Object.prototype.hasOwnProperty.call(names, eventName)) {
      throw capabilityError(`AccessibilityInfo event "${eventName}" has no Qt system provider.`);
    }
    native();
    return Emitter.addListener(names[eventName], handler);
  },
  setAccessibilityFocus(reactTag) {
    native().setAccessibilityFocus(reactTag);
  },
  sendAccessibilityEvent(handle, eventType) {
    if (eventType !== 'focus') {
      throw capabilityError(
        `AccessibilityInfo event "${eventType}" is unsupported by the Qt host.`
      );
    }
    const tag = findNodeHandle(handle);
    if (tag == null)
      throw new Error('AccessibilityInfo requires a mounted React Native host instance.');
    native().setAccessibilityFocus(tag);
  },
  announceForAccessibility(announcement) {
    native().announceForAccessibility(announcement, false);
  },
  announceForAccessibilityWithOptions(announcement, options) {
    if (options == null || typeof options !== 'object') {
      throw new TypeError('AccessibilityInfo announcement options must be an object.');
    }
    if (options.queue === true)
      throw capabilityError('Qt does not provide queued accessibility announcements.');
    if (options.queue !== undefined && typeof options.queue !== 'boolean') {
      throw new TypeError('AccessibilityInfo queue must be a boolean.');
    }
    if (options.priority !== undefined && !['low', 'default', 'high'].includes(options.priority)) {
      throw new TypeError('AccessibilityInfo priority must be low, default, or high.');
    }
    native().announceForAccessibility(announcement, options.priority === 'high');
  },
};

module.exports = AccessibilityInfo;
module.exports.default = AccessibilityInfo;
