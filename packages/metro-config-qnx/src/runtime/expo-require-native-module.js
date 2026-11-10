'use strict';

// The QNX Metro resolver leaves this import on Expo's original implementation.
// Keep Expo's actual installer, optional lookup and bridge/TurboModule fallback.
const upstream = require('@rn-qnx/internal-expo-native-module');

/**
 * Same acquisition contract as Expo SDK 58; no synthetic native providers.
 * Optional imports remain null when absent. Required imports fail explicitly
 * before an application can mistake an absent backend for denied permission.
 */
function requireNativeModule(moduleName) {
  const nativeModule = upstream.requireOptionalNativeModule(moduleName);
  if (!nativeModule) {
    const error = new Error(
      `Expo native module '${moduleName}' is unavailable in this QNX runtime. ` +
        'Installing the JavaScript package does not provide a QNX backend. ' +
        'Link and register a QNX implementation or disable this optional feature. ' +
        'This is not a permission-denied result.'
    );
    error.name = 'ExpoQnxModuleUnavailableError';
    error.code = 'ERR_QNX_EXPO_MODULE_UNAVAILABLE';
    error.moduleName = moduleName;
    throw error;
  }
  return nativeModule;
}

module.exports = {
  requireNativeModule,
  requireOptionalNativeModule: upstream.requireOptionalNativeModule,
};
