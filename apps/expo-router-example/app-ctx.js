// The route files of ./app, as a Metro require.context. expo-router normally
// builds this from `expo-router/_ctx` with an Expo Babel plugin; on QNX the
// resolver in @rn-qnx/expo-router-qnx points that import here instead. The
// pattern skips API routes and +html files, which have no meaning on a device.
export const ctx = require.context(
  './app',
  true,
  /^(?:\.\/)(?!(?:(?:(?:.*\+api)|(?:\+html)|(?:\+middleware)))\.[tj]sx?$).*\.[tj]sx?$/,
  'sync'
);
