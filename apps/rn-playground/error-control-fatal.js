require('./error-control-app');

// The real registry throws and Metro reports the failure through ErrorUtils
// and ExceptionsManager. Bundle evaluation itself can still return normally.
require('react-native').TurboModuleRegistry.getEnforcing('RNQNX_TEST_MISSING_BOOTSTRAP');
