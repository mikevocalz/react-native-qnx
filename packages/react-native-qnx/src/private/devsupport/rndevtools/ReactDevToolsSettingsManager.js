/**
 * React DevTools hook settings for QNX development builds.
 *
 * React Native 0.88 ships this module only as `.ios.js` (backed by Settings)
 * and `.android.js` (backed by a native module), and its development bundle
 * imports it unconditionally. QNX has neither backing store, so the settings
 * live for the life of the JavaScript runtime and a reload starts from React
 * DevTools' defaults.
 */

'use strict';

/** @type {string | null} */
let globalHookSettings = null;

/** @param {unknown} settings */
function setGlobalHookSettings(settings) {
  globalHookSettings = typeof settings === 'string' ? settings : null;
}

function getGlobalHookSettings() {
  return globalHookSettings;
}

module.exports = { setGlobalHookSettings, getGlobalHookSettings };
