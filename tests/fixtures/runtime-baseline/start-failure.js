// Evaluation succeeds; the real RN AppRegistryBinding call fails at startup.
globalThis.RN$AppRegistry = {
  runApplication() {
    throw new Error('RNQNX_TEST_APPLICATION_START_FAILURE');
  },
};
globalThis.RN$stopSurface = () => {};
