/**
 * The Marathon platform's JavaScript overrides for React Native.
 *
 * Files under `Libraries/` mirror React Native's own layout. Metro prefers them
 * for `platform: 'marathon'` through `@rn-qnx/metro-config-marathon`. Every
 * React Native file without a Marathon override falls back to the QNX override
 * of the same path (source reuse only), then to React Native's own file.
 */
'use strict';
const path = require('node:path');

/** Absolute path to the directory whose contents override react-native's. */
const OVERRIDES_ROOT = path.join(__dirname, 'Libraries');

module.exports = { OVERRIDES_ROOT };
