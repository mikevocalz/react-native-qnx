/**
 * `Platform` for QNX.
 *
 * React Native's `Libraries/Utilities/Platform.js` re-exports `./Platform`,
 * which Metro resolves by platform extension. With no `Platform.qnx.js` in
 * React Native's own tree that import resolves back to the same file, leaving
 * `Platform` undefined and every `Platform.select` call throwing. This file is
 * what Metro resolves to instead.
 *
 * Shape follows `Libraries/Utilities/Platform.ios.js`. Constants come from the
 * host rather than a native module: `globalThis.__RNQNX_PLATFORM_CONSTANTS` is
 * installed by QtReactHost before the bundle runs, so `Platform.constants` is
 * real data and not a placeholder. If the host has not installed it, the
 * getters throw rather than inventing values.
 */

'use strict';

function hostConstants() {
  const constants = globalThis.__RNQNX_PLATFORM_CONSTANTS;
  if (constants == null) {
    throw new Error(
      'Platform constants are unavailable: the QNX host did not install ' +
        'globalThis.__RNQNX_PLATFORM_CONSTANTS before evaluating the bundle.'
    );
  }
  return constants;
}

const Platform = {
  __constants: null,
  OS: 'qnx',
  get Version() {
    return this.constants.osVersion;
  },
  get constants() {
    if (this.__constants == null) {
      this.__constants = hostConstants();
    }
    return this.__constants;
  },
  get isPad() {
    return false;
  },
  get isTV() {
    return false;
  },
  get isVision() {
    return false;
  },
  get isTesting() {
    if (__DEV__) {
      return this.constants.isTesting;
    }
    return false;
  },
  get isDisableAnimations() {
    return this.constants.isDisableAnimations ?? this.isTesting;
  },
  // React Native calls this to decide whether the new architecture is on. QNX
  // is Fabric-only, so it never varies.
  get isVisionOS() {
    return false;
  },
  select: (spec) => ('qnx' in spec ? spec.qnx : 'native' in spec ? spec.native : spec.default),
};

module.exports = Platform;
module.exports.default = Platform;
