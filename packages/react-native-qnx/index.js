/**
 * The QNX platform's JavaScript overrides for React Native.
 *
 * Files under `Libraries/` mirror React Native's own layout. Metro is told to
 * prefer them for `platform: 'qnx'` by `@rn-qnx/metro-config-qnx`, because React
 * Native's package cannot be edited in place.
 */

const path = require('node:path');

/** Absolute path to the directory whose contents override react-native's. */
const OVERRIDES_ROOT = path.join(__dirname, 'Libraries');

module.exports = { OVERRIDES_ROOT };
