// Start succeeds; AppRegistryBinding's real surface-stop call must be checked.
globalThis.RN$AppRegistry = { runApplication() {} };
globalThis.RN$stopSurface = () => {
  throw new Error('RNQNX_TEST_APPLICATION_STOP_FAILURE');
};
