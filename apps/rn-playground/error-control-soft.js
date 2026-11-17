require('./error-control-app');

globalThis.ErrorUtils.reportError(new Error('RNQNX_TEST_REPORTED_SOFT_ERROR'));
try {
  throw new Error('RNQNX_TEST_CAUGHT_ERROR');
} catch {
  console.log('RNQNX_TEST_CAUGHT_ERROR_CONTINUED');
}
console.warn('RNQNX_TEST_BENIGN_WARNING');
