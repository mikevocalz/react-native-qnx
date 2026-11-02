/**
 * MarathonOS React Native Platform identity.
 *
 * This file must only resolve in a Marathon Metro build. Real native platform
 * constants are installed by the shared QtReactHost before evaluating JS.
 * When a host has not installed them, getters fail instead of returning fake
 * version or device information.
 */
'use strict';

function hostConstants() {
  const constants = globalThis.__RNQNX_PLATFORM_CONSTANTS;
  if (constants == null) {
    throw new Error(
      'Marathon native host must install __RNQNX_PLATFORM_CONSTANTS before JS evaluation.'
    );
  }
  return constants;
}

const Platform = {
  __constants: null,
  OS: 'marathon',
  get Version() {
    return this.constants.osVersion;
  },
  get constants() {
    if (this.__constants == null) this.__constants = hostConstants();
    return this.__constants;
  },
  get isPad() { return false; },
  get isTV() { return false; },
  get isVision() { return false; },
  get isVisionOS() { return false; },
  get isTesting() { return __DEV__ ? this.constants.isTesting : false; },
  get isDisableAnimations() {
    return this.constants.isDisableAnimations ?? this.isTesting;
  },
  select: (spec) =>
    'marathon' in spec ? spec.marathon : 'native' in spec ? spec.native : spec.default,
};

module.exports = Platform;
module.exports.default = Platform;
