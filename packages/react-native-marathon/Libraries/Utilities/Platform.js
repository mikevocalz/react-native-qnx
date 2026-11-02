/**
 * MarathonOS React Native Platform identity.
 * This source is resolved only when Metro is building --platform marathon.
 * Native runtime constants must be installed by the Qt/Hermes host first.
 */
'use strict';

/**
 * @typedef {{
 *   osVersion: string | number,
 *   isTesting: boolean,
 *   isDisableAnimations?: boolean
 * }} PlatformConstants
 */

/** @type {typeof globalThis & { __RNQNX_PLATFORM_CONSTANTS?: PlatformConstants, __DEV__?: boolean }} */
const hostGlobals = globalThis;

/** @returns {PlatformConstants} */
function hostConstants() {
  const constants = hostGlobals.__RNQNX_PLATFORM_CONSTANTS;
  if (constants == null) {
    throw new Error(
      'Marathon native host must install __RNQNX_PLATFORM_CONSTANTS before JS evaluation.'
    );
  }
  return constants;
}

const Platform = {
  /** @type {PlatformConstants | null} */
  __constants: null,
  OS: 'marathon',
  get Version() {
    return this.constants.osVersion;
  },
  get constants() {
    if (this.__constants === null) {
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
  get isVisionOS() {
    return false;
  },
  get isTesting() {
    return hostGlobals.__DEV__ === true ? this.constants.isTesting : false;
  },
  get isDisableAnimations() {
    return this.constants.isDisableAnimations ?? this.isTesting;
  },
  /** @param {Record<string, unknown>} spec */
  select: (spec) =>
    'marathon' in spec ? spec.marathon : 'native' in spec ? spec.native : spec.default,
};

// React Native's platform import can be consumed through CommonJS or .default.
// Do not emit a second module-level export assignment that TypeScript 7
// mistakenly interprets as mixing ES exports with CommonJS.
Object.assign(Platform, { default: Platform });
module.exports = Platform;
