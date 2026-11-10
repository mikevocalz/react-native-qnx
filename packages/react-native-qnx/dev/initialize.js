'use strict';

if (
  !__DEV__ ||
  !global.__RNQNX_DEV_SESSION__ ||
  typeof global.globalEvalWithSourceUrl !== 'function' ||
  typeof global.__RNQNX_DEV_REPORT__ !== 'function'
) {
  throw new Error(
    'QNX development bootstrap requires an opt-in source-enabled host and dev=true bundle'
  );
}

if (global.__RNQNX_DEV_BOOTSTRAPPED__) {
  throw new Error('Install QNX development bootstrap only once per runtime');
}

// These are the pinned RN contracts, with inspector/LogBox deliberately not
// installed. Errors go to the native Qt Quick development overlay instead.
const report = (kind, message) => global.__RNQNX_DEV_REPORT__(kind, message);
global.RN$handleException = (error) => {
  report('error', String((error && (error.stack || error.message)) || error));
  return true;
};
require('react-native/Libraries/Core/setUpGlobals');
require('react-native/Libraries/Core/setUpReactRefresh');
require('react-native/src/private/setup/setUpDefaultReactNativeEnvironment').default(false);

const MetroHMRClient = require('metro-runtime/src/modules/HMRClient');
const DevSettings = require('react-native/Libraries/Utilities/DevSettings').default;
const { startMetroSession } = require('./MetroDevSession');
const session = startMetroSession({
  Client: MetroHMRClient,
  config: global.__RNQNX_DEV_SESSION__,
  report,
  reload: (reason) => DevSettings.reload(reason),
});
global.__RNQNX_DEV_DISPOSE__ = () => session.dispose();
global.__RNQNX_DEV_BOOTSTRAPPED__ = true;
